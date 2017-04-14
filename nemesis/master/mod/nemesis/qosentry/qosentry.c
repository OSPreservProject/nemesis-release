/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/qosentry
**
** FUNCTIONAL DESCRIPTION:
** 
**      Inplementation of QoSEntry: scheduling of IO channels according
**      to QoS parameters.  Each IO channel is allocted TIME according 
**      to Astropos style QoS (p,s).
**
**      NOTE: Since we are scheduling TIME, rather than number of bytes
**      or some suce, it only makes sense to have a single worker thread
**      (for the moment).  Higher level code can provide parallelism if
**      necessary, but then the accounting would be wierd!
** 
** ENVIRONMENT: 
**
**      For use in device drivers.
**      Half of this code executes in activation handler, half
**      as a normal thread.
** 
** FILE         : qosentry.c
** CREATED      : Fri Nov 21 1997
** AUTHOR       : Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ORGANISATION : CUCL
** ID           : $Id: qosentry.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <bstring.h>

#include <VPMacros.h>
#include <IOMacros.h>

#include <QoSEntryMod.ih>
#include <QoSEntry.ih>
#include <QoSCtl.ih>                            
#include <ChannelNotify.ih>
#include <ThreadF.ih>
#include <Time.ih>

#include <links.h>
#include <time.h>
#include <exceptions.h>
#include <ecs.h>
#include <ntsc.h>

// #define DEBUG
//#define CONFIG_QOSENTRY_LOGGING

#ifdef DEBUG
#define TRC(_x) _x
#define QTRC(format, args...) \
 eprintf(format , ## args)
#define QDUMP(_x) qdump(_x)
#else
#define TRC(_x)
#define QTRC(format, args...)
#define QDUMP(_x)
#endif
#define DB(x)  x

#define ETRC(_x)
#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

#define DOM(x) (VP$DomainID(st->vp))

/* XXX PRB This should be a runtime configurable parameter */
#define BESTEFFORT_QUANTUM MILLISECS(20)


/*
 * Create an QOSENTRY
 */
static QoSEntryMod_New_fn	New_m;
static QoSEntryMod_op m_op = { New_m };
const QoSEntryMod_cl debug_cl = { &m_op, NULL };

/*CL_EXPORT (QoSEntryMod, qed, debug_cl); */
CL_EXPORT (QoSEntryMod, QoSEntryMod, debug_cl);

/*
 * QoSEntry interface operations 
 */
static IOEntry_Bind_fn		Bind_m;
static IOEntry_Unbind_fn	Unbind_m;
static IOEntry_Rendezvous_fn	Rendezvous_m;
static IOEntry_Suspend_fn   	Suspend_m; 
static IOEntry_Close_fn	        Close_m;
static QoSEntry_SetQoS_fn       SetQoS_m;
static QoSEntry_Charge_fn       Charge_m;
static QoSEntry_dbg_fn          dbg_m;
static QoSEntry_op e_op = {
    Bind_m,
    Unbind_m,
    Rendezvous_m,
    Suspend_m, 
    Close_m,
    SetQoS_m,
    Charge_m,
    dbg_m,
};

/*
 * ChannelNotify interface operations
 */
static ChainedHandler_SetLink_fn SetLink_m;
static ChannelNotify_Notify_fn	Notify_m;
static ChannelNotify_op cn_op = { SetLink_m, Notify_m };

/*
 * TimeNotify for timeout kicks
 */
static TimeNotify_Notify_fn	TimeNotify_m;
static TimeNotify_op timenotify_ms = { TimeNotify_m };

/*
 * QoSCtl interface 
 */
static	QoSCtl_GetQoS_fn 		QC_GetQoS_m;
static	QoSCtl_SetQoS_fn 		QC_SetQoS_m;
static	QoSCtl_Query_fn 		QC_Query_m;
static	QoSCtl_ListClients_fn 		QC_ListClients_m;

static	QoSCtl_op	qosctl_ms = {
    QC_GetQoS_m,
    QC_SetQoS_m,
    QC_Query_m,
    QC_ListClients_m
};


/* 
 * Useful types/structs
 */
typedef struct _bindlink    bindlink_t;
typedef struct _QoSEntry_st QoSEntry_st;
typedef struct _binding_t   binding_t;

typedef struct {
    Time_ns p, s, l;
    bool_t  x;
} qos_t;

struct _bindlink {
    bindlink_t	     *next;
    bindlink_t	     *prev;
};

/*
 * Per-binding state 
 */
struct _binding_t {
    bindlink_t	  	link;	/* Must be first in structure 	*/
    Time_T              d;      /* Deadline (position in queue) */
    Time_T		r;      /* Time remaining in current p  */
    QoSEntry_State      state;	/* State of binding           */

    ChannelNotify_cl    rdy_cn_cl;	/*** ChannelNotify closure    ***/
    ChannelNotify_clp	next_rdy_cn;/* next rdy handler to call       	*/
    ChannelNotify_clp   prev_rdy_cn;/* rdy handler that calls us        */
    ChannelNotify_cl    ack_cn_cl;
    ChannelNotify_clp   next_ack_cn;
    ChannelNotify_clp   prev_ack_cn;

    QoSEntry_st        *entry;	     /* Entry state		*/

    /* Scheduling params */
    qos_t               qos;	
    
    /* Accounting information */
    Time_T              ac_g, ac_x;	/* accounting for this period */
    Time_T              ac_tg, ac_tx;	/* totals over all binding existence */
    Time_T		ac_lp;		/* time the last period ended */
    Time_T              lastrun;

    /* The actual IO channel */
    Channel_Endpoint    rdy;	/* Channel end-point [rx side]	*/
    Channel_Endpoint    ack;	/* Channel end-point [tx side]	*/
    IO_cl              *io;	/* IO channel			*/
    
    char               *peername;
    dcb_ro_t           *peerdcbro;
    Domain_ID           peerdomid;

    struct _binding_t  *allnext; /* List of EVERY binding */

};

#ifdef CONFIG_QOSENTRY_LOGGING

/* Debugging trace */

static void dumptrace(QoSEntry_st *st);
typedef struct { 
    uint32_t delta;		/* Time delta since last entry (in us) */
    uint32_t event:4;		/* What happened */
    uint32_t client:4;		/* Who */
    uint32_t arg:24;		/* How much of it! */
} log_t;

typedef enum { 
    ACTIVATED,
    BLOCK, 
    UNBLOCK, 
    NOTIFY, 
    RENDEZVOUS, 
    CHARGE,
    ALLOC,
    DEADLINE,
    LAXITY,
    AC_G,
    AC_X,
    IDLE,
    WOKEN,
    NUM_EVENTS
} log_event_t;

static const char * const eventnames[NUM_EVENTS] = {
    "ACTIVATED",
    "BLOCK", 
    "UNBLOCK",
    "NOTIFY", 
    "RENDEZVOUS", 
    "CHARGE",
    "ALLOC",
    "DEADLINE",
    "LAXITY",
    "AC_G",
    "AC_X",
    "IDLE",
    "WOKEN"
};

#define LOGSIZE 16384
// #define LOGSIZE 1024
#define LOG(_now,_event,_client,_arg)				\
({								\
    log_t *entry;						\
    if(st->lastlog) {						\
        if (_now < st->lastlog) eprintf("BAD\n");		\
	entry = st->log + st->logindex++;			\
	entry->delta = (_now - st->lastlog) / MICROSECS(1);	\
	entry->event = _event;					\
	entry->client = _client;				\
	entry->arg   = _arg;					\
	if(st->logindex == LOGSIZE)				\
	    st->lastlog = 0;					\
    }								\
})
#else 
#define LOG(_now,_event,_client,_arg);
#endif

/*
 * Per-QoSEntry state
 */
struct _QoSEntry_st {
    /*** QoSEntry ***/
    QoSEntry_cl         qosentry_cl; /* self */
    
    /* Useful closures */
    ActivationF_cl	*actf;	
    ThreadF_cl          *threadf; 
    Thread_cl           *thread;        /* Worker thread */
    VP_cl		*vp;
    Time_cl             *t;

    bool_t              blocked;        /* Is the thread blocked? */
    Time_T              tlast;		/* Time last entered rendezvous */
    binding_t	        *current;
    binding_t           *lastX;		/* last binding to get best effort*/
    Time_ns             besteffort;     /* How much BE time left */

    /* notify handler */
    TimeNotify_cl	kick_cl;

    /* queues of bindings */
    bindlink_t	     	wait;
    bindlink_t	     	run;
    bindlink_t          idle;

    uint32_t	    	num_eps;	/* Current no. of bindings */
    binding_t          *allbindings;    /* List of all bindings */

    /*** QoSCtl ***/
    QoSCtl_cl           qosctl_cl;      /* Closure */

    uint32_t            epoch;		/* current rev of the clients list */
    uint32_t            polled;		/* last rev polled */
    int 	        nclients;
    QoSCtl_Client      *clients;
    bool_t		needupdate;	/* client list needs checking */

#ifdef CONFIG_QOSENTRY_LOGGING
    /**** Debugging stuff ****/
    log_t              log[LOGSIZE];
    Time_ns            lastlog;	        /* Non-zero when logging */
    Time_ns            logstart;
    uint32_t           logindex;        /* Points into log */
#endif
};

/* 
** ENTRY locking functions 
** 
** Single threaded so we only need to protect ourselves
** against the activation handler.
*/ 
static INLINE void ENTRY_Lock (QoSEntry_st *st)
{
    VP$ActivationsOff (st->vp);
}

static INLINE void ENTRY_Unlock (QoSEntry_st *st)
{
    VP$ActivationsOn (st->vp);
}

/*
 * Local utility function
 */
static void QC_UpdateClients(QoSEntry_st *st);

#if defined(DEBUG) || defined(CONFIG_QOSENTRY_LOGGING)

static void qdump(bindlink_t *headp)
{
    bindlink_t *p = headp->next;

    while (p != headp)
    {
	eprintf(" %x", (int)(((binding_t *)p)->rdy));
	p = p->next;
    }
    eprintf("\n");
}
#endif

static binding_t *FindBinding(QoSEntry_st	*st,
			      Channel_Endpoint ep)
{
    binding_t *b;

    b = st->allbindings;
    while (b && b->rdy != ep) b= b->allnext;
    return b;
}


static binding_t *FindBindingFromIO(QoSEntry_st	*st,
				    IO_cl *io )
{
    binding_t *b;

    b = st->allbindings;
    while (b && b->io != io) b = b->allnext;
    return b;
}

/* Sorted link insertion. */
static INLINE void LINK_INSERT(bindlink_t *headp, bindlink_t *linkp)
{
    bindlink_t *p = headp->next;

    while (p != headp && ((binding_t *)linkp)->d > ((binding_t*)p)->d)
	p = p->next;
    
    /* Now simple case of LINK_ADD_BEFORE(): */
    linkp->next = p;
    linkp->prev = p->prev;
    linkp->next->prev = linkp;
    linkp->prev->next = linkp;
}

#ifdef CONFIG_QOSENTRY_LOGGING
static INLINE int pending(VP_clp vp, Channel_Endpoint ep)
{
    Event_Val val, ack;
    Channel_State  state;
    Channel_EPType type;
	
    state = VP$QueryChannel(vp, ep, &type, &val, &ack);
    return val - ack; 
}
#endif

/*
** QoSEntryMod$New 
**
** Creates a new QoSEntry.
*/
static QoSEntry_clp New_m(QoSEntryMod_clp self, ActivationF_clp actf, 
			  VP_clp vp, uint32_t nch)
{
    QoSEntry_st *st;
    word_t      size = sizeof(*st);

    /* Allocate structure */
    if ( !(st = Heap$Malloc(Pvs(heap), size )) ) {
	DB(printf("QoSEntryMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }
    bzero(st, sizeof(*st));

    CL_INIT (st->qosentry_cl,  &e_op,  st);
    
    st->actf    = actf;
    st->threadf = (ThreadF_clp)Pvs(thds);
    st->vp      = vp;
    st->t       = Pvs(time);

    st->allbindings = NULL;
    st->besteffort  = BESTEFFORT_QUANTUM;

    st->current = NULL;
    LINK_INIT( &st->wait );
    LINK_INIT( &st->run );
    LINK_INIT( &st->idle );

    st->clients = Heap$Malloc(Pvs(heap), nch * sizeof(QoSCtl_Client));
    st->nclients = 0;
    st->epoch   = 1;
    st->needupdate = False;
    {
	Type_Any any;

	/* QoSCtl interface */
	CL_INIT(st->qosctl_cl, &qosctl_ms, st);
	ANY_INIT(&any, QoSCtl_clp, &st->qosctl_cl); 
	Context$Add(Pvs(root), "qosctl", &any);
    }

    CL_INIT(st->kick_cl, &timenotify_ms, st);

    return &st->qosentry_cl;
}


/*
** QoSEntry$Bind
**
** Add a new binding to this entry
*/
static void Bind_m(IOEntry_clp		self, 
		   Channel_Endpoint 	rdy, 
		   Channel_Endpoint     ack, 
		   IO_clp		io)
{
    QoSEntry_st *st = self->st;
    binding_t  *b;

    b = Heap$Malloc(Pvs(heap), sizeof(*b));
    if (!b) {
	DB(printf("QoSEntry$MapAssoc: out of memory.\n"));
	RAISE_Heap$NoMemory();
    }

    /* Initialise the binding */
    bzero(b, sizeof(binding_t));
    b->rdy   = rdy;
    b->ack   = ack;
    b->io    = io;
    b->entry = st;
    b->qos.p = SECONDS(1); /* No QoS until they set QoS up */
    b->qos.s = 0;
    b->qos.l = 0;
    b->qos.x = False;	   /* default is no eXtra time neither */
    b->d     = Time$Now(st->t) + b->qos.p;
    b->r     = 0;
    LINK_INIT(&b->link);
    CL_INIT( b->ack_cn_cl, &cn_op, b );
    CL_INIT( b->rdy_cn_cl, &cn_op, b );

    /* For now, we don't know who's bound to us, so leave the name
     * and peer DCB zeroed.  We fill in the DCB on the first Notify(),
     * and then the peername in Rendezvous(). */
    b->peername  = NULL;
    b->peerdcbro = NULL;

    ENTRY_Lock(st);
    {
	/* record client's binding in the master binding list */
	b->allnext = st->allbindings;
	st->allbindings = b;
	st->num_eps++;

	if (!st->lastX) {
	    st->lastX = st->allbindings;
	}
	
	b->state = QoSEntry_State_Idle;
	LINK_INSERT(&st->idle, &b->link);
	/* Register a notification closure on this channel */
	b->next_rdy_cn = ActivationF$Attach(st->actf, &b->rdy_cn_cl, b->rdy);
	b->prev_rdy_cn = NULL;
	if(b->next_rdy_cn) {
	    ChainedHandler$SetLink((ChainedHandler_clp)b->next_rdy_cn, 
				   ChainedHandler_Position_Before, 
				   (ChainedHandler_clp)&b->rdy_cn_cl);
	}

	b->next_ack_cn = ActivationF$Attach(st->actf, &b->ack_cn_cl, b->ack);
	b->prev_ack_cn = NULL;
	if(b->next_ack_cn) {
	    ChainedHandler$SetLink((ChainedHandler_clp)b->next_ack_cn,
				   ChainedHandler_Position_Before,
				   (ChainedHandler_clp)&b->ack_cn_cl);
	}
    }
    ENTRY_Unlock(st);

    TRC(printf("QoSEntry$Bind: done\n"));
}

/*
** QoSEntry$Unbind
**
** Remove a binding of a channel end-point to an IO
*/
static void Unbind_m(IOEntry_clp	self, 
		     Channel_Endpoint 	chan)
{
    QoSEntry_st *st = self->st;
    binding_t * NOCLOBBER b = NULL;

    /* Check the channel exists */
    if ( !( b = FindBinding (st, chan)) ) {
	DB(printf("QoSEntry$Unbind: channel not found.\n"));
	RAISE_Channel$Invalid( chan );
    }

    ENTRY_Lock(st);
    {	
	binding_t *tmp;

	/* Remove it from scheduler queue */
	LINK_REMOVE (&b->link);

	/* Remove our notification handler */
	if(b->prev_rdy_cn) {
	    ChainedHandler$SetLink((ChainedHandler_clp)b->prev_rdy_cn,
				   ChainedHandler_Position_After, 
				   (ChainedHandler_clp)b->next_rdy_cn);
	} else {
	    
	    /* If we were the head of the notify queue, attach the
	       previous handler to the endpoint */
	    
	    ActivationF$Attach(st->actf,  b->next_rdy_cn, chan);
	    
	}
	
	if(b->next_rdy_cn) {
	    ChainedHandler$SetLink((ChainedHandler_clp)b->next_rdy_cn,
				   ChainedHandler_Position_Before,
				   (ChainedHandler_clp)b->prev_rdy_cn);
	}
	
	
	/* and from the "allbindings" list */
	tmp = st->allbindings;
	if (tmp == b) {
	    st->allbindings = tmp->allnext;
	} else {
	    while(tmp->allnext != b) tmp=tmp->allnext;
	    tmp->allnext = b->allnext;
	}
	if (st->lastX == b) st->lastX = b->allnext;

	/* Reduce count */
	st->num_eps--;

	/* Fix up the qosctl stuff */
	QC_UpdateClients(st);
    }
    ENTRY_Unlock(st);

    FREE (b);
}


static void Suspend_m(IOEntry_clp self, IO_clp io)
{
    QoSEntry_st *st = self->st;
    binding_t  *b;

    if (!(b = FindBindingFromIO(st, io)))
    {
	TRC(printf("QoSEntry$Suspend: IO_clp not found.\n"));
	RAISE_QoSEntry$InvalidIO(); 
    }

    /* Ok got it: now move it onto the idle queue */
    ENTRY_Lock(st);
    LINK_REMOVE(&b->link);
    b->state = QoSEntry_State_Idle;
    LINK_ADD_TO_TAIL(&st->idle, &b->link) ;
    ENTRY_Unlock(st);

    /* That's it I think */
    return;
}

/*
** QoSEntry$SetQoS
**
** Set the QoS level of this binding (p,s,x,l).
*/
static void SetQoS_m(QoSEntry_clp self, IO_clp io, Time_T p, 
		     Time_T s, bool_t x, Time_T l)
{
    QoSEntry_st	*st   = self->st;
    binding_t   *b;

#ifdef CONFIG_QOSENTRY_LOGGING
    /* XXX PRB This is vile but it lets us debug things */
    if (io == (addr_t)666) {
	dumptrace(st);
	return ;
    }
#endif

    /* find the binding */
    if (!(b = FindBindingFromIO(st, io)))
    {
	ntsc_dbgstop();
	TRC(printf("QoSEntry$SetQoS: IO_clp not found.\n"));
	RAISE_QoSEntry$InvalidIO(); 
    }
    
    /* Set the values */
    b->qos.p = p;
    b->qos.s = s;
    b->qos.x = x;
    b->qos.l = l;
}

/*
** QoSEntry$Charge
** 
** Dock "time" nanoseconds from the remaining time on "io" 
*/
static void Charge_m(QoSEntry_clp self, IO_clp io, Time_T time)
{
    QoSEntry_st	*st = self->st;
    binding_t	*b;

    TRC(eprintf("Charge called: self:%p, io:%p, time:%qx\n",
	       self, io, time));
    
    /* XXX PRB HACK: This allows me to add sneaky log entries for
       transactions performed outside qosentry */
    if (!io) { LOG(NOW(), CHARGE, 0, time/MICROSECS(1)); return; }

    b = st->current;
    if (!b || b->io != io)
	b = FindBindingFromIO(st, io);
    if (!b) {
	TRC(eprintf("QoSEntry$Charge: IO_clp not found.\n"));
	RAISE_QoSEntry$InvalidIO();
    }

    LOG(NOW(), CHARGE, b->rdy, time/MICROSECS(1));

    /* only actually charge if running in contracted (i.e. not extra) time */
    if (b->r > 0) {
	b->r    -= time;
	b->ac_g += time;
	ETRC((st, "Docked %qx from %p\n", time, b));
    } else {
	b->ac_x += time;
	st->besteffort -= time;
	ETRC((st, "Would have docked %qx from %p\n", time, b));
    }
}
/*
** QoSEntry$Rendezvous
** 
** The main man. 
**
** Return the next IO channel which should be serviced. 
** This is determined using an algorithm based on the atropos scheduler.
**
** If all are blocked then block the thread until an IO channel
** becomes ready.  "to" is the absolute time to time out.
*/
static IO_clp Rendezvous_m(IOEntry_clp self, Time_ns timeout)
{
    QoSEntry_st	           *st   = self->st;
    binding_t * NOCLOBBER  todo = NULL;
    Time_T      NOCLOBBER  time;
    Time_ns     to, laxity; 
    binding_t *            current;
    bindlink_t * NOCLOBBER head;

    ENTRY_Lock(st);

    TRC(eprintf("QoSEntry$Rendezvous\n"));
    QTRC("------");

    /* 'deschedule' current if necessary.
       If no guaranteed time (r <= 0), put binding on the wait Q */
    current = st->current;
    if (current && current->r <= 0) {
	LINK_REMOVE (&(current->link));
	current->state = QoSEntry_State_Waiting;
	LINK_INSERT (&st->wait, &current->link);
	QTRC("%x -> W\n", (int)(current->rdy));
    } 

    for(;;) {

	/* do we need to fixup the clients list for QoSCtl? */
	if (st->needupdate)
	{
	    st->needupdate = False;
	    /* must turn activations back on for the QC_UpdateClients, since
	     * it might need to do a Context$Get() to the trader */
	    ENTRY_Unlock(st);
	    QC_UpdateClients(st);
	    ENTRY_Lock(st);
	}

	QTRC("  R:");
	QDUMP(&st->run);
	QTRC("  W:");
	QDUMP(&st->wait);
	QTRC("  I:");
	QDUMP(&st->idle);
	todo = st->allbindings;
	QTRC("  A:");
	while(todo) {
	    QTRC(" %x", (int)(todo->rdy));
	    if (todo == st->lastX) QTRC("*");
	    todo = todo->allnext;
	}
	QTRC("\n");
	
	time = Time$Now(st->t);

	QTRC("1\n");
	/* 
	** Loop around waiting Q allocating time if start of 
	** next allocation period has arrived. 
	*/
	head = &st->wait;
	while(head->next != &st->wait) {
	    binding_t *b = (binding_t *) (head)->next;

	    if (time < b->d) break;

	    LINK_REMOVE(&b->link);

	    b->r += b->qos.s;
	    QTRC("A %x, r=%qx\n", (int)(b->rdy), b->r);
	    b->d = b->d + b->qos.p;

	    LOG(NOW(), ALLOC,    b->rdy, b->r / MICROSECS(1));
	    LOG(NOW(), DEADLINE, b->rdy, (b->d-st->logstart) / MICROSECS(1));
	    LOG(NOW(), AC_G,     b->rdy, b->ac_g / MICROSECS(1));
	    LOG(NOW(), AC_X,     b->rdy, b->ac_x / MICROSECS(1));

	    /* Update the accounting information */
	    b->ac_tg += b->ac_g;
	    b->ac_tx += b->ac_x;
	    b->ac_lp = time;	/* XXX AND: is this up to date? */
	    b->ac_g = 0;
	    b->ac_x = 0;
	    
	    /* if there's time remaining, place the binding on the run
	     * Q, otherwise re-insert in the wait Q (we've changed
	     * the deadline above) */
	    if (b->r > 0) {
		b->state = QoSEntry_State_Runnable; 
		QTRC("%x -> R\n", (int)(b->rdy));
		LINK_INSERT(&st->run, &b->link);
	    } else {
		/* Push further down the wait queue */
		QTRC("%x -> W\n", (int)(b->rdy));
		LINK_INSERT(head, &b->link);
	    }

	    /* advance to next item in waiting Q */
	    head = head->next;
	}

	/*
	** Take head of runnnable and see if has any packets pending 
	*/
	QTRC("2\n");
	todo = NULL;
	while(!LINK_EMPTY(&st->run)) {
	    Time_T   now, cost;
	    uint32_t nrecs = 0; 
	    
	    /* Pick up the head of the run queue */
	    todo = (binding_t *) (st->run.next);

	    /* See if there are any packets on this channel */
	    if(todo->qos.l > 0) {

		laxity = MIN(todo->r, todo->qos.l); 
		now    = NOW();

		ENTRY_Unlock(st);
		IO$QueryGet(todo->io, now + laxity, &nrecs);
		ENTRY_Lock(st);

		cost     = NOW() - now; 
		todo->r -= cost; 
		LOG(NOW(), LAXITY, todo->rdy, cost/MICROSECS(1));
	    } 

	    /* 
	    ** SMH: even if we did a QueryGet with timeout, we need 
	    ** to poll again here since we may have received an event
	    ** after the timeout, but before we turned activations
	    ** off again. 
	    */
	    IO$QueryGet(todo->io, 0, &nrecs);

	    if (nrecs) {
		QTRC("G %x", (int)(todo->rdy));
		goto found; 
	    } else {
		QTRC("%x -> I\n", (int)(todo->rdy));
		todo->state = QoSEntry_State_Idle;
		LINK_REMOVE(&todo->link);
		LINK_ADD_TO_TAIL (&st->idle, &todo->link);
		LOG(NOW(), BLOCK, todo->rdy,  pending(st->vp, todo->rdy));
		todo = NULL;
	    }
	    /* If not, try next one etc etc */
	}
	
	QTRC("3\n");

	/* XXX TEMP 'borrow' the allbindings list since it's 
	   pretty much as good as a list of best-effort clients */
	
	TRC(eprintf("WAITQ\n"));
	/* No point going round this loop at all if nobody on WAITQ */
	if (!LINK_EMPTY(&st->wait)) {
	    int n = 0;

	    todo = st->lastX;

	    do  {
		Time_T   now, cost;
		uint32_t nrecs = 0;

		if (!todo) todo = st->allbindings;

		QTRC(" %x", (int)(todo->rdy));
		
		/* If the current besteffort quantum hasn't expired then 
		   we consider running the same client again - avoids 
		   excessive context switches which is important for disk */
		if ((st->besteffort > 0) &&
		    (todo->state == QoSEntry_State_Waiting) && 
		    todo->qos.x) {

		    /* Poll the channel to see if there are any packets */

		    /* If this client is allowed some laxity then we 
		       are prepared to hand around a bit waiting for 
		       packets to arrive (reduces context switches and 
		       the probability of blocking */
		    if(todo->qos.l > 0) {
			
			laxity = MIN(todo->r, todo->qos.l); 
			now    = NOW();
			
			ENTRY_Unlock(st);
			IO$QueryGet(todo->io, now + laxity, &nrecs);
			ENTRY_Lock(st);
			
			cost     = NOW() - now; 
			st->besteffort -= cost;
			LOG(NOW(), LAXITY, todo->rdy, cost/MICROSECS(1));
		    } 
		    IO$QueryGet(todo->io, 0, &nrecs);

		    if (nrecs) {
			QTRC("X %x", (int)(todo->rdy));
			st->lastX = todo;
			goto found;
		    } else { 
			/* This one is now considered idle */
			QTRC("%x -> I\n", (int)(todo->rdy));
			todo->state = QoSEntry_State_Idle;
			LINK_REMOVE(&todo->link);
			LINK_ADD_TO_TAIL (&st->idle, &todo->link);
			LOG(NOW(), BLOCK, todo->rdy, 
			    pending(st->vp, todo->rdy));
		    } 
		}
		/* Loop round ALL bindings */
		todo = (binding_t *)todo->allnext;
		if (!todo) todo = st->allbindings;
		st->besteffort = BESTEFFORT_QUANTUM;

		if (++n > st->num_eps) ntsc_dbgstop();

	    } while (todo != st->lastX);

	    /* Nothing sensible on the waitQ */
	    todo = NULL;
	} 
	QTRC("\n");

	/*
	** Can't find anything to do so we are going to block 
	*/

	st->current = NULL;  

	/* work out when we need to wake up */
	to = NOW() + SECONDS(20); /* FOREVER; */
	head = &st->wait;
	if (!LINK_EMPTY(head)) {
	    binding_t *b = (binding_t *) (head)->next;
	    to = MIN(b->d, to);
	} 

	QTRC("B %qx %qx\n", to, time);
	LOG(NOW(), IDLE, 0, (to-st->logstart)/MICROSECS(1));
	if (ActivationF$AddTimeout(st->actf, &st->kick_cl, to, (word_t)st)) {
	    st->thread = Pvs(thd);	/* So notify can find us! */
	    st->blocked = True;
	    ThreadF$BlockYield(st->threadf, to);
	    ActivationF$DelTimeout(st->actf, to, (word_t)st);
	}
	LOG(NOW(), WOKEN, 0, 0);
	QTRC("U\n");
    }
found:
    /* We now have something... set current & run with it */
    st->current = todo;
    st->tlast = time;
    todo->lastrun = time;

    QTRC("< %x\n", (int)(todo->rdy));

    LOG(NOW(), RENDEZVOUS, todo->rdy, todo->r / MICROSECS(1));

    ENTRY_Unlock(st);
    
    /* Return IO_clp to do work on */
    return todo->io;  
}

/*
 * Close down this entry
 */
static void Close_m(IOEntry_clp self)
{
    QoSEntry_st *st = self->st;

    /* Check that we can go away */
    if (st->num_eps) {
	DB(printf("QoSEntry$Close: entry has endpoints outstanding\n"));
	RAISE_IOEntry$TooManyChannels();
    }
    
    if (!Q_EMPTY (&st->idle) || !Q_EMPTY (&st->idle))
    {
	DB(printf("QoSEntry$Close: entry has bindings outstanding.\n"));
	RAISE_IOEntry$Inconsistent();
    }

    FREE (st);
}

void dbg_m(QoSEntry_cl *self)
{
    
}

static void SetLink_m(ChainedHandler_clp self,
		      ChainedHandler_Position pos,
		      ChainedHandler_clp link) {

    binding_t  *b  = self->st;
    
    if((ChannelNotify_clp)self == &b->rdy_cn_cl) {
	if(pos == ChainedHandler_Position_Before) {
	    b->prev_rdy_cn = (ChannelNotify_clp)link;
	} else {
	    b->next_rdy_cn = (ChannelNotify_clp)link;
	}
    } else {
	if(pos == ChainedHandler_Position_Before) {
	    b->prev_ack_cn = (ChannelNotify_clp)link;
	} else {
	    b->next_ack_cn = (ChannelNotify_clp)link;
	}
    }
}

/*
** ChannelNotify$Notify
**
** Called by ActivationF whenver a channel's state changes
*/
static void Notify_m(ChannelNotify_clp 	self, 
		     Channel_Endpoint 	chan,
		     Channel_EPType  	type,
		     Event_Val		val, 
		     Channel_State	state )
{
    binding_t  *b  = self->st;
    QoSEntry_st *st = b->entry; 
    Time_ns time = Time$Now(st->t);
    Event_Val    ack;

    QTRC("N:%x\n", (int)(b->rdy));
    LOG(time, NOTIFY, chan, pending(st->vp, chan));

    /* XXX Ack the event.. theoretically we can do this just before
       blocking somebody to avoid excessive notification.  We only
       need notifies on the RDY channel until somebody calls
       QoSEntry$Suspend when we become interested in the client
       removing packets from its end of the IO channel (events on the
       ACK channel) */
    do {
	ack = val;
	VP$Ack(st->vp, chan, ack);
	val = VP$Poll(st->vp, chan);
    } while (ack != val);

    /*
    ** Dead binding.  
    **
    ** XXX Could still have packets in the channel, but what
    ** on earth do we do with them?  Can't send the acks to a dead
    ** domain so we might as well bin them.  Punt it. 
    */
    if (state == Channel_State_Dead) {
	/* XXX PRB What do we do with dead bindings?  Leave
	** it for higher levels to notice? We will probably have to
	** unblock the binding though. */
	TRC(eprintf("%p DEAD\n", b));
    }

    /* If binding is blocked then unblock it. */
    if (b->state == QoSEntry_State_Idle) {

	/* This binding is going to come off the idle queue */
	LINK_REMOVE (&b->link);

	if (time > b->d) {	/*** 'LONG BLOCK' ***/
	    /* If it has been blocked for a long time then start 
	       a new period from NOW */
	    b->r = b->qos.s;
	    b->d = time + b->qos.p;
	} else {		/*** 'SHORT BLOCK' ***/
	    /* We're going to have to throw away its remaining time
	       to protect the innocent */
	    b->r = 0;
	}
	LOG(time, UNBLOCK, b->rdy, b->r / MICROSECS(1));

	if (b->r > 0) {
	    QTRC("%x -> R\n",(int)(b->rdy));
	    b->state = QoSEntry_State_Runnable;
	    LINK_INSERT (&st->run,&b->link);
	} else {
	    QTRC("%x -> W\n", (int)(b->rdy));
	    b->state = QoSEntry_State_Waiting;
	    LINK_INSERT (&st->wait,&b->link);
	}

	/* QoSCtl things: If we haven't yet got the DCB of our peer,
	 * then now is a good time to work out who the client domain
	 * of this binding is.  Clearly they're sending us events.  We
	 * can't find out the peername just yet, since that would
	 * involve doing IDC to the trader, and we're currently
	 * running from an activation handler here.  Just note the
	 * DCB, and mark the client list as needing updated - the
	 * actual update happens in Rendezvous(). */
	if (!b->peerdcbro)
	{
	    b->peerdcbro   = RO(st->vp)->epros[b->rdy].peer_dcb;
	    st->needupdate = True;
	}

    }
    /* Unblock the worker thread */
    if (st->blocked) {
	st->blocked = False;
	ThreadF$UnblockThread(st->threadf, st->thread, False);
    }

    /*
    ** SMH: the end-points bound in this entry are attached to event 
    ** counts. The old ChannelNotify closures we had, then, were 
    ** registered by the events code in order to allow the unblocking
    ** of threads blocked on one of the event counts. 
    ** 
    ** Since the whole point of using an Entry is that outside threads 
    ** don't need to block on the event counts (we do it for them), 
    ** we now don't bother upcalling the previous notifies. 
    **
    ** There is one exception: when we have a positive 'laxity' value
    ** on a binding, then we perform a blocking query to check for
    ** work on the IO channel. Hence in this case we need to punt
    ** the notifications up to the next level. 
    */
    if(b->qos.l > 0) {
	if (chan == b->rdy && b->next_rdy_cn)
	    ChannelNotify$Notify (b->next_rdy_cn, chan, type, val, state);
	if (chan == b->ack && b->next_ack_cn)
	    ChannelNotify$Notify (b->next_ack_cn, chan, type, val, state);
    }
}


static void TimeNotify_m(TimeNotify_cl *self,
			 Time_ns now, Time_ns deadline, word_t data)
{
    QoSEntry_st *st = self->st;

    QTRC("T\n");
    if (st->blocked) {
	st->blocked = False;
	ThreadF$UnblockThread(st->threadf, st->thread, False);
    }
}

/*
** QoSCtl Stuff 
*/

bool_t verify(QoSEntry_st *st, binding_t *check)
{
    binding_t *b;

    b = st->allbindings;

    while (b) {
	if (b == check) return True;
	b = b->allnext;
    }
    return False;
}


static void QC_UpdateClients(QoSEntry_st *st)
{
    binding_t *b;
    int i;

    b = st->allbindings;
    i = 0;
    while (b) {

	/* if we don't yet have a peername, but we do have a DCB,
	 * then go get the info */
	if (!b->peername && b->peerdcbro)
	{
	    Type_Any any;
	    char buf[64];

	    ENTER_KERNEL_CRITICAL_SECTION();
	    b->peerdomid = b->peerdcbro->id;
	    LEAVE_KERNEL_CRITICAL_SECTION();

	    sprintf(buf, "proc>domains>%016qx>name", b->peerdomid);
	    if (Context$Get(Pvs(root), buf, &any))
	    {
		b->peername = (char *)((word_t)any.val);
		TRC(eprintf("qctl: %p:peername=%s\n", b, b->peername));
	    }
	}
	
	if (b->peername)
	{
	    st->clients[i].hdl  = (QoSCtl_Handle)b;
	    st->clients[i].name = b->peername;
	    st->clients[i].did  = b->peerdomid;
	    i++;
	}
	b = b->allnext;
    }
    st->nclients = i;
    st->epoch++;
}

static bool_t QC_GetQoS_m (QoSCtl_cl *self, QoSCtl_Handle hdl /* IN */
			/* RETURNS */, Time_T *p, Time_T *s, Time_T *l )
{
    QoSEntry_st *st = self->st;
    binding_t *b = (binding_t *)hdl;
    qos_t *q;

    if (!verify(st, b)) 
	return False;

    q = &b->qos;

    *p = q->p;
    *s = q->s;
    *l = q->l;

    return q->x;
}

#ifdef CONFIG_QOSENTRY_LOGGING

static void dumptrace(QoSEntry_st *st)
{
    char *name;
    Time_ns start, time;
    FILE *f;
    uint32_t i;
    Type_Any any;
    
    binding_t * NOCLOBBER  todo = NULL;
    eprintf("  R:");
    qdump(&st->run);
    eprintf("  W:");
    qdump(&st->wait);
    eprintf("  I:");
    qdump(&st->idle);
    todo = st->allbindings;
    eprintf("  A:");
    while(todo) {
	eprintf(" %x", (int)(todo->rdy));
	if (todo == st->lastX) eprintf("*");
	todo = todo->allnext;
    }
    eprintf("\n");


    start = NOW();
    if (Context$Get(Pvs(root), "qelog", &any)) {
	name = (char *)any.val;
	printf("Opening %s...\n", name);
	f = fopen(name, "w"); 
	if (!f) {
	    eprintf("AAArgh!\n");
	    return;
	}
    } else { 
	name = "stderr";
	f = stderr;
	Pvs(err) = Pvs(console);
    }

    printf("Starting a trace....\n");
    ENTER_KERNEL_CRITICAL_SECTION();
    st->logindex = 0;
    st->lastlog  = start;
    st->logstart = start;
    LEAVE_KERNEL_CRITICAL_SECTION();

    while ((st->logindex < LOGSIZE) && (NOW()-start < SECONDS(20))) {
	PAUSE(SECONDS(1));
    }
    printf("Done\n");

    /* Dump the log */

    time = start / MICROSECS(1);
    
    printf("Dumping to %s...\n", name);

    fprintf(f, "#  TIME   |   ARG   # EVENT (client) \n");
    for (i = 0; i < st->logindex; i++) {
	fprintf(f, "%-10d %-10d # %-12s %d\n",
		time + st->log[i].delta,
		st->log[i].arg,
		eventnames[st->log[i].event],
		st->log[i].client);
    }

    if (f != stderr) fclose(f);

    printf("Done\n");
}
#endif

static bool_t QC_SetQoS_m (QoSCtl_cl *self, QoSCtl_Handle hdl /* IN */,
			   Time_T p, Time_T s, Time_T l, bool_t x)
{
    QoSEntry_st *st = self->st;
    binding_t *b = (binding_t *)hdl;
    qos_t *q;

#ifdef CONFIG_QOSENTRY_LOGGING

/*
 * XXX PRB Note that the hack below allows people to do the following
 * from a clanger prompt and cause a trace to be taken.  The filename
 * can be set up in the namespace.
 *
 * !svc>sd_qosctl$SetQoS[666,0,0,0,false] 
 */
    if (hdl == 666) {
	dumptrace(st);
	return True;
    }
#endif
	
    if (!verify(st, b)) 
	return False;

    q = &b->qos;
    
    ENTER_KERNEL_CRITICAL_SECTION();
    q->p = p;
    q->s = s;
    q->l = l;
    q->x = x;
    LEAVE_KERNEL_CRITICAL_SECTION();

//    QoSEntry$SetQoS(&st->entry_cl, b->io, p, s, x); 

    return True;
}
 

static Time_T QC_Query_m(QoSCtl_cl *self, QoSCtl_Handle hdl,
       /* RETURNS */ Time_T *tg, Time_T *sg, Time_T *tx, Time_T *sx )
{
    QoSEntry_st *st = self->st;
    binding_t *b = (binding_t *)hdl;
    Time_ns now = NOW();

    if (!verify(st, b)) 
	return 0;

    *tg = b->ac_tg;
    *tx = b->ac_tx;
    *sg = b->ac_lp;
    *sx = now;

    return 1;
}


static bool_t QC_ListClients_m(QoSCtl_cl *self, QoSCtl_Client **clients, 
			       uint32_t *nclients )
{
    QoSEntry_st *st = self->st;
    bool_t changes = False;

    if (st->epoch != st->polled) {
	ENTER_KERNEL_CRITICAL_SECTION();
	st->polled = st->epoch;
	LEAVE_KERNEL_CRITICAL_SECTION();
	changes = True;
    }
    *clients  = st->clients;
    *nclients = st->nclients;

    return      changes;
}


/* End */
