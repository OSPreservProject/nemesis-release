/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      mod/nemesis/events/events.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of the Event interface over the Nemesis virtual
**	processor. 
** 
** ENVIRONMENT: 
**
**      User-land;
** 
** ID : $Id: events.c 1.2 Tue, 04 May 1999 18:45:30 +0100 dr10009 $
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <links.h>
#include <time.h>
#include <ecs.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include <VPMacros.h>
#include <domid.h>

#include <ActivationF.ih>
#include <ChannelNotify.ih>
#include <TimeNotify.ih>
#include <Events.ih>
#include <EventsMod.ih>
#include <ExnSystem.ih>
#include <HeapMod.ih>
#include <Thread.ih>
#include <ThreadF.ih>
#include <ThreadHooks.ih>
#include <VP.ih>

/*
** Source configuration
*/

#ifdef EVENT_TRACE
#define TRC(_x) _x
#define DEBUG
#else
#define TRC(_x)
#endif


#ifdef DEBUG
#define DB(_x) _x
#define NOTREACHED						\
{								\
  fprintf(stderr, "Events_%s: shouldn't be here!\n", __FUNCTION__);	\
}
#else
#define DB(_x) 
#define NOTREACHED
#endif



#define PARANOIA
#ifdef PARANOIA
#define ASSERT(cond,fail) {						\
    if (!(cond)) { 							\
	{fail}; 							\
	while(1); 							\
    }									\
}
#else
#define ASSERT(cond,fail)
#endif


#define EVTRC(_x) 


/*
 * Module Stuff
 */

static Events_New_fn	 	 NewEvent_m;
static Events_Free_fn	 	 FreeEvent_m;
static Events_Read_fn	 	 ReadEvent_m;
static Events_Advance_fn	 AdvanceEvent_m;
static Events_Await_fn	 	 AwaitEvent_m;
static Events_AwaitUntil_fn 	 AwaitEventUntil_m;
static Events_NewSeq_fn	 	 NewSeq_m;
static Events_FreeSeq_fn	 FreeSeq_m;
static Events_ReadSeq_fn	 ReadSeq_m;
static Events_Ticket_fn	 	 Ticket_m;        
static Events_Attach_fn	 	 Attach_m;        
static Events_AttachPair_fn	 AttachPair_m;        
static Events_QueryEndpoint_fn   QueryEndpoint_m;
static Events_AllocChannel_fn	 AllocChannel_m;        
static Events_FreeChannel_fn	 FreeChannel_m;        

static Events_op events_ms = {
    NewEvent_m,
    FreeEvent_m,
    ReadEvent_m,
    AdvanceEvent_m,
    AwaitEvent_m,
    AwaitEventUntil_m,
    NewSeq_m,
    FreeSeq_m,
    ReadSeq_m,
    Ticket_m,        
    Attach_m,        
    AttachPair_m,
    QueryEndpoint_m,
    AllocChannel_m,
    FreeChannel_m,
};



/*--------------------------------------------------------------------------*/


typedef struct _link  link_t;
struct _link {
    link_t	*next;
    link_t	*prev;         
};

typedef struct _qlink {
    link_t       waitq;
    link_t       timeq; 
    Event_Val    waitVal;
    Time_T       waitTime;
    Time_T       blockTime;      /* for debugging */
    Thread_cl 	*thread;
} qlink_t;


#define WQP2QP(_wqp) ((qlink_t *)(_wqp))
#define TQP2QP(_tqp) ((qlink_t *)((void *)_tqp - sizeof(link_t)))

#ifdef HEAVY_PARANOIA
#define ZAP(_linkp) { 				\
    word_t ra = RA();				\
    (_linkp)->next=NULL;			\
    (_linkp)->prev=ra;	                        \
}
#else  /* HEAVY_PARANOIA */
#define ZAP(_linkp) (_linkp)->next=NULL;
#endif /* HEAVY_PARANOIA */

/*
** Data structures for event counts and sequencers.
*/

typedef struct _Event_Count Event_Count_t;
typedef Event_Val sequencer_t;

typedef struct _Events_st Events_st;
typedef struct _Evt_st Evt_st;

struct _Event_Count {
    Event_Count_t   *next;	  /* links for st->counts               */
    Event_Count_t   *prev;
    Event_Val	     val;	  /* current value                      */
    Channel_EP	     ep;	  /* attached endpoint, unless NULL_EP	*/
    Channel_EPType   ep_type;     /* type if above not NULL_EP          */
    ChannelNotify_clp next_cn;    /* Chained notification handler       */
    ChannelNotify_clp prev_cn;    /* Previous notification handler      */
    link_t           wait_queue;  /* list of threads waiting on this ec */
    ChannelNotify_cl cn_cl;       /* op != NULL iff attached            */
    Evt_st          *evt_st;
};


#define WAIT_QUEUE(_ec)      (&(_ec->wait_queue))


/* State record for an instance of the Event interface */
struct _Evt_st {
    VP_clp          vpp;	/* VP we are running over	   */
    ActivationF_cl *actf;       /* For ChannelNotify hooks         */
    ThreadF_cl     *thdf;       /* Handle to (un)block threads     */
    ThreadHooks_cl  th_cl;      /* To setup the per-thread state   */
    Heap_clp        heap;       /* Our heap (NB: NOT locked)       */
    Event_Count_t   counts;	/* All event counts		   */
    link_t          time_queue; /* things waiting for timeouts     */
    Events_st      *exit_st;    /* Events structure used for exit  */
};


/* Per thread state; encapsulates a 'qlink_t' to block that thread. */
struct _Events_st {
    Events_cl       cl;         /* Per-thread events closure       */
    TimeNotify_cl   tn_cl;      /* For timeouts from actf          */
    Evt_st         *evt_st;     /* Pointer to shared state         */
    qlink_t         qlink;      /* Used to block/unblock this thd  */
};




/* prototypes for internal routines */
static bool_t Block(Events_st *st, Event_Count_t *ec, Thread_cl *thread,
		    Event_Val val, Time_T until);
static void RealUnblockEvent(Evt_st *evt_st, Event_Count_t *ec, 
			     bool_t alerted);


static void __inline__ UnblockEvent(Evt_st *evt_st, Event_Count_t *ec, 
				    bool_t alerted) {
    
    if(!Q_EMPTY(WAIT_QUEUE(ec))) {
	RealUnblockEvent(evt_st, ec, alerted);
    }
}



/*
 * Critical sections.
 *
 * ENTER/LEAVE sections ARE now nestable.
 *
 * If LEAVE enables activations, it should cause an activation if there
 * are pending events on incoming event channels, but neither ENTER nor
 * LEAVE should need a system call in the common case.
 *
 * ENTER/LEAVE sections protect VP state (such as context and
 * event allocation) as well as ULS state (such as the run and
 * blocked queues).
 *
 */

#define ENTER(vpp,mode)\
do { if(((mode) = VP$ActivationMode (vpp))) VP$ActivationsOff (vpp); } while(0)

#define LEAVE(vpp,mode)				\
do {						\
  if (mode)					\
  {						\
    VP$ActivationsOn (vpp);			\
    if (VP$EventsPending (vpp))			\
      VP$RFA (vpp);				\
  }						\
} while (0)



static ChainedHandler_SetLink_fn   ECSetLink_m;
static ChannelNotify_Notify_fn     ECNotify_m;
static ChannelNotify_op            ECNotify_ms = { ECSetLink_m, ECNotify_m };

static TimeNotify_Notify_fn        TimeNotify_m;
static TimeNotify_op               tn_ms = { TimeNotify_m };


/*
 * ThreadHooks stuff
 */
static	ThreadHooks_Fork_fn 		ThreadHooks_Fork_m;
static	ThreadHooks_Forked_fn 		ThreadHooks_Forked_m;
static	ThreadHooks_ExitThread_fn 	ThreadHooks_ExitThread_m;
static	ThreadHooks_ExitDomain_fn 	ThreadHooks_ExitDomain_m;

static	ThreadHooks_op	th_ms = {
  ThreadHooks_Fork_m,
  ThreadHooks_Forked_m,
  ThreadHooks_ExitThread_m,
  ThreadHooks_ExitDomain_m
};

/*-------------------------------------------ThreadHooks Entry Points ----*/

static void 
ThreadHooks_Fork_m (ThreadHooks_cl *self,
		    ThreadHooks_Rec_ptr	new_pvs	/* IN */ )
{
    /* Called in parent => can create per-thread state here. */
    Evt_st    *evt_st = (Evt_st *)self->st;
    Events_st *new_st;
    bool_t     mode;

    ENTER(evt_st->vpp, mode);
    new_st = Heap$Malloc(evt_st->heap, sizeof(*new_st));
    LEAVE(evt_st->vpp, mode);
    if (!new_st) RAISE_Events$NoResources ();

    /* Init per thread state */
    CL_INIT(new_st->cl, &events_ms, new_st);
    CL_INIT(new_st->tn_cl, &tn_ms, new_st);
    new_st->evt_st = evt_st;
    bzero(&new_st->qlink, sizeof(qlink_t));

    new_pvs->evs = &new_st->cl;
    TRC(eprintf("Fork (d%x): Pvs(evs) = st is %p, state at %p (evt_st=%p)\n", 
	    VP$DomainID(evt_st->vpp) & 0xFF, new_pvs->evs, evt_st));

    return;
}

static void 
ThreadHooks_Forked_m (ThreadHooks_cl *self )
{
    /* Called in new thread => finish setup of per-thread Pvs(evs) here. */
    Events_st *st = Pvs(evs)->st;

    st->qlink.waitTime = FOREVER;
    st->qlink.thread   = Pvs(thd);
    TRC(eprintf("Forked (d%x): set qlink.thread to %p\n", 
	    VP$DomainID(st->evt_st->vpp) & 0xFF, Pvs(thd)));
    return;
}

static void 
ThreadHooks_ExitThread_m (ThreadHooks_cl *self )
{
    /* Called by a dying thread */
    
    /* XXX SMH: should delete the per-thread instance here */

    Evt_st *evt_st = self->st;
    Events_st *st = Pvs(evs)->st;
    bool_t mode;

    ENTER(evt_st->vpp, mode);
    if(st->qlink.waitq.next) Q_REMOVE(&st->qlink.waitq);
    if(st->qlink.timeq.next) Q_REMOVE(&st->qlink.timeq);

    FREE(st);
    /* This is a little ugly: from now until the thread actually dies,
       it is running on the "exit" Events_clp closure. This is in case
       the threads package decides to kill the domain, in which case
       it needs to invoke the Binder, which requires a working events
       structure ... */

    Pvs(evs) = &evt_st->exit_st->cl;

    LEAVE(evt_st->vpp, mode);
    return;
}

static void 
ThreadHooks_ExitDomain_m (ThreadHooks_cl *self )
{
    /* Called when the last thread dies */
    /* Not that useful really */

    
    /* Maybe we should delete the shared state here, but the domain's
       about to die anyway, so it doesn't really matter */
    return;
}

/*----------------------------------------------- EventMod Entry Points ---*/

static EventsMod_New_fn EventsMod_New;

static EventsMod_op em_ms = {
    EventsMod_New,
};

static const EventsMod_cl em_cl = { &em_ms, NULL };

CL_EXPORT(EventsMod, EventsMod, em_cl);



static Events_cl *
EventsMod_New(EventsMod_cl *self, VP_clp  vpp, ActivationF_cl *actf, 
	      Heap_clp heap, ThreadF_clp tf)
{
    Events_st *st;     /* per thread state */
    Evt_st    *evt_st; /* shared state     */
    Events_st *exit_st;/* Structure for exiting threads */

    if( !(evt_st= Heap$Malloc(heap, sizeof(*evt_st))) ||
	!(st= Heap$Malloc(heap, sizeof(*st))) ||
	!(exit_st = Heap$Malloc(heap, sizeof(*exit_st)))) {
	DB(fprintf(stderr, "EventMod$New: malloc failed.\n"));
	return (Events_cl *)NULL;
    }

    /* Init shared state */
    evt_st->vpp  = vpp; 
    evt_st->actf = actf;
    evt_st->heap = heap;
    evt_st->thdf = tf;
    CL_INIT(evt_st->th_cl, &th_ms, evt_st);
    Q_INIT (&evt_st->counts);
    Q_INIT (&evt_st->time_queue);

    /* Init per thread state */
    CL_INIT(st->cl, &events_ms, st);
    CL_INIT(st->tn_cl, &tn_ms, st);
    st->evt_st = evt_st;
    bzero(&st->qlink, sizeof(qlink_t));

    /* Init exit structure state */
    CL_INIT(exit_st->cl, &events_ms, exit_st);
    CL_INIT(exit_st->tn_cl, &tn_ms, exit_st);
    exit_st->evt_st = evt_st;
    bzero(&exit_st->qlink, sizeof(qlink_t));
    exit_st->qlink.waitTime = FOREVER;
    exit_st->qlink.thread = NULL;
    evt_st->exit_st = exit_st;

    
    /* Setup hooks to allow creation of per-thread state */
    ThreadF$RegisterHooks(evt_st->thdf, &evt_st->th_cl);

    TRC(eprintf("EventsMod$New: returning Pvs(evs) [mainThread] at %p\n", 
		&st->cl));
    return &(st->cl);
}



/*-------------------------------------------------- Event Entry Points ---*/

static Event_Count
NewEvent_m (Events_cl *self)
{
    Events_st     *st     = (Events_st *)self->st;
    Evt_st        *evt_st = st->evt_st;
    VP_cl         *vpp    = evt_st->vpp;
    Event_Count_t *res;
    bool_t mode;

    ENTER (vpp, mode); 
    res = (Event_Count_t *) Heap$Malloc (evt_st->heap, sizeof (*res));
    LEAVE (vpp, mode);
    if (!res) RAISE_Events$NoResources ();

    res->val    = 0;
    res->ep     = NULL_EP;
    res->evt_st = evt_st;
    res->next_cn = res->prev_cn = NULL;
    Q_INIT  (&res->wait_queue);
    CL_INIT (res->cn_cl, NULL, NULL);


    /* add to the set of all counts */
    ENTER (vpp, mode); 
    Q_INSERT_BEFORE (&(evt_st->counts), res); 
    LEAVE (vpp, mode);

    return (Event_Count) res;
}


static void
FreeEvent_m (Events_cl *self, Event_Count ec)
{
    Events_st     *st     = self->st;
    Evt_st        *evt_st = st->evt_st;
    VP_cl         *vpp    = evt_st->vpp;
    Event_Count_t *ect    = (Event_Count_t *) ec;
    bool_t mode;

    ENTER (vpp, mode);
    {
	/* Alert all waiters on this event count. */
	UnblockEvent(evt_st, ect, True);
	
	/* Remove from the set of all counts */
	Q_REMOVE(ect);
	ZAP(ect);
	
	/* If we attached "ec" to the actf, we need to detach it */
	if(ect->ep != NULL_EP) {
	    if(ect->prev_cn) {
		ChainedHandler$SetLink((ChainedHandler_clp)ect->prev_cn,
				       ChainedHandler_Position_After, 
				       (ChainedHandler_clp)ect->next_cn);
	    } else {
		
		/* If we were the head of the notify queue, attach the
		   old handler (maybe NULL) to the endpoint */
		ActivationF$Attach(evt_st->actf, ect->next_cn, ect->ep);
		
	    }

	    if(ect->next_cn) {
		ChainedHandler$SetLink((ChainedHandler_clp)ect->next_cn,
				       ChainedHandler_Position_Before,
				       (ChainedHandler_clp)ect->prev_cn);
	    }

	}
    }
    LEAVE (vpp, mode);

    /* The closedown call to the binder must be outside the
       critical section: it does a blocking IDC call. */

    if (ect->ep != NULL_EP) {
	Binder$Close(Pvs(bndr), ect->ep);
	FreeChannel_m(self, ect->ep);
    }

    /* Finally, we can free the event count */
    ENTER (vpp, mode); 
    Heap$Free(evt_st->heap, ect);
    LEAVE (vpp, mode);
}

/*
 * ReadEvent must check any attached channel, since otherwise updates
 * are only propagated from the channel to the count on an activation,
 * and an activation will not occur unless someone is blocked on this
 * event count. This means that ReadEvent can raise the same
 * exceptions as VP_Poll, principally Channel.BadState. 
 */
static Event_Val
ReadEvent_m (Events_cl *self, Event_Count ec)
{
    Event_Count_t *ect = (Event_Count_t *) ec;
    VP_cl         *vpp = ((Events_st *)self->st)->evt_st->vpp;

    /* Check any attached event count */
    if ( ect->ep != NULL_EP && ect->ep_type == Channel_EPType_RX ) {
	ect->val = VP$Poll(vpp, ect->ep);
    }
    return ect->val;
}

/*
 * AdvanceEvent preserves VP$ActivationMode(vpp).  Thus, it can be
 * called within an activations-off critical section.
 */
static void
AdvanceEvent_m (Events_cl *self, Event_Count ec, Event_Val inc)
{
    Events_st     *st     = (Events_st *)self->st;
    Evt_st        *evt_st = st->evt_st;
    VP_cl         *vpp    = evt_st->vpp;
    Event_Count_t *ect    = (Event_Count_t *) ec;
    ntsc_rc_t      rc     = rc_success;
    bool_t mode;

    TRC (word_t ra = (word_t) RA();
	 fprintf(stderr, 
		 "ECAdvance(dom=%lx): ra=%x ec=%x val=%x inc=%x ep=%x ty=%x\n",
		 VP$DomainID(vpp), ra, ect, ect->val, inc,
		 ect->ep, ect->ep_type));

    ENTER (vpp, mode);
    {
	ect->val += inc;


	/* unblock any awoken threads */
	UnblockEvent(evt_st, ect, False);

	/* if "ec" is attached to a TX endpoint, send the new value */
	if (ect->ep != NULL_EP && ect->ep_type == Channel_EPType_TX)
	{

#ifdef INTEL
	    dcb_rw_t *rwp = RW(vpp);
	    dcb_ro_t *rop = rwp->dcbro;
	    ep_ro_t *eprop = DCB_EPRO (rop, ect->ep);
	    ep_rw_t *eprwp = DCB_EPRW (rop, ect->ep);
#endif

	    TRC (fprintf(stderr, "Events$Advance: send ep=%x val=%x\n", 
			 ect->ep, ect->val));
	    
#ifdef INTEL
	    if(eprop->type != Channel_EPType_TX) {
		rc = rc_ch_invalid;
	    } else if(eprwp->state != Channel_State_Connected) {
		rc = rc_ch_bad_state;
	    } else {
		dcb_event *ev = 
		    &rwp->outevs[(rwp->outevs_pushed + 1) % NUM_DCB_EVENTS];
		while(rwp->outevs_pushed == 
		      rop->outevs_processed + NUM_DCB_EVENTS) {
		    /* Need ntsc_flush_evs() */
		    ntsc_yield();
		}
		ev->ep = ect->ep;
		ev->val = ect->val;
		rwp->outevs_pushed++;
		rc = rc_success;
	    }
#else
	    rc = ntsc_send (ect->ep, ect->val);
#endif
	}

	/* This section is commented out for performance reasons, since    */
	/* TRY currently costs a setjmp. Calling ntsc_send directly rather */
	/* than going through the VP interface (as we strictly speaking    */
	/* should) removes the need for the FINALLY clause to get us out   */
	/* of the critical section cleanly.				   */
#if 0
	{
	    TRY /* XXX - ouch; can't move Send out of critical section without
	           allowing Sends to become misordered wrt. the wakeups
		   performed in this domain. */
		{
		    VP$Send (vpp, ect->ep, ect->val);
		}
	    FINALLY
		LEAVE (vpp, mode);
	    ENDTRY;
	    return;
	}
#endif /* 0 */
    }
    LEAVE (vpp, mode);

    switch (rc)
    {
    case rc_success:
	break;
    case rc_ch_bad_state:
	RAISE_Channel$BadState (ect->ep, 0 );
    case rc_ch_invalid:
    default:
	DB(fprintf(stderr, "Events$Advance: invalid channel.\n"));
	RAISE_Channel$Invalid (ect->ep);
    }

}

static Event_Val
AwaitEvent_m (Events_cl *self, Event_Count ec, Event_Val val)
{
    Events_st     *st     = (Events_st *)self->st;
    Evt_st        *evt_st = st->evt_st;
    VP_cl         *vpp    = evt_st->vpp;
    Event_Count_t *ect    = (Event_Count_t *) ec;
    Thread_cl     *current;
    bool_t	   alerted = False;
    Event_Val	   res;

#ifdef PARANOIA    
    if(!VP$ActivationMode(vpp))
	ntsc_dbgstop();
#endif

    if (EC_LE (val, ect->val)) return ect->val;

    Threads$EnterCS((Threads_cl *)evt_st->thdf, True);
    {
	if ( ect->ep != NULL_EP && ect->ep_type == Channel_EPType_RX ) {
	    Channel_State ch_st;
	    Channel_EPType ch_tp;
	    Event_Val v, a;
      
	    /* We must first get into a consistent state wrt. external 	    */
	    /* events, which may arrive at any time. The aim is the get to  */
	    /* the stage where we both have a value for the count, and are  */
	    /* guaranteed that the FIFO will register the count going above */
	    /* that value. 						    */
	    for(;;) {
	
		/* Read count and ack values				   */
		ch_st = VP$QueryChannel(vpp, ect->ep, &ch_tp, &v, &a);

		if ( EC_GT( v, val ) || v == a )
		    break;

		VP$Ack(vpp, ect->ep, v);
	    }

	    /* For RXEP events, the following condition now holds : 	   */
	    /*								   */
	    /*    v <= ep_val && ( v == ep_ack || v >= val ) 		   */
	    /*								   */
	    /* This is required to ensure events with the "new" semantics  */
	    /* are not lost by the scheduler.                              */
	    /* Finally update the local value and adjust threads.    	   */
	    ect->val = v;
	    UnblockEvent(evt_st, ect, False);
	}

	/* Get the result. */
	res = ect->val;

	/* We now block if val < ep_val. This test also takes care of the */
	/* local case when ep_val was advanced after the test outside the */
	/* critical section above.					  */

	if (EC_GT (val, res))
	{
	    /* Block's precondition holds */
	    current = Pvs(thd);
	    if(Block (st, ect, current, val, FOREVER))
		alerted = True; /* => "ect" might have been freed */
	    else res = ect->val;
	}
    }
    Threads$LeaveCS((Threads_cl *)evt_st->thdf);
#ifdef PARANOIA
    if(!VP$ActivationMode(vpp))
	ntsc_dbgstop();
#endif
    if (alerted) RAISE_Thread$Alerted();

    return res;
}

static Event_Val
AwaitEventUntil_m (Events_cl *self,
		   Event_Count ec, Event_Val val, Time_T until)
{
    Events_st     *st     = (Events_st *)self->st;
    Evt_st        *evt_st = st->evt_st;
    VP_cl         *vpp    = evt_st->vpp;
    Thread_cl     *current;
    Event_Count_t *ect;
    Event_Val      res;
    bool_t         alerted = False;

#ifdef PARANOIA
    if(!VP$ActivationMode(vpp))
	ntsc_dbgstop();
#endif

    if (until < 0) until = FOREVER;	/* Make PAUSE(FOREVER) work */

    /* Check for simple block with timeout (i.e. no event count) */
    if(ec == NULL_EVENT) {

	if(IN_FUTURE(until)) {
	    Threads$EnterCS((Threads_cl *)evt_st->thdf, True);
	    if(Block(st, NULL, Pvs(thd), val, until))
		alerted = True;
	    else res = 0;
	    Threads$LeaveCS((Threads_cl *)evt_st->thdf);
#ifdef PARANOIA
	    if(!VP$ActivationMode(vpp))
		ntsc_dbgstop();
#endif
	}

	return 0;
    } 

    ect = (Event_Count_t *)ec;
    if (EC_LE (val, ect->val) || until <= NOW()) return ect->val;

    Threads$EnterCS((Threads_cl *)evt_st->thdf, True);
    {
	if ( ect->ep != NULL_EP && ect->ep_type == Channel_EPType_RX ) {
	    Channel_State ch_st;
	    Channel_EPType ch_tp;
	    Event_Val v, a;
      
	    /* We must first get into a consistent state wrt. external 	    */
	    /* events, which may arrive at any time. The aim is the get to  */
	    /* the stage where we both have a value for the count, and are  */
	    /* guaranteed that the FIFO will register the count going above */
	    /* that value.                                                  */ 
	    for(;;) {
	
		/* Read count and ack values				*/
		ch_st = VP$QueryChannel(vpp, ect->ep, &ch_tp, &v, &a);

		/* Exit loop here if our postcondition is met 		*/
		if ( EC_GT( v, val ) || v == a  || !IN_FUTURE(until))
		    break;

		Threads$Yield(Pvs(thds)); 
		VP$Ack(vpp, ect->ep, v);
	    }
      
	    /* For RXEP events, the following condition now holds : 	    */
	    /*								    */
	    /*  v <= ep_val && ( v == ep_ack || v >= val || until >= NOW )  */
	    /*								    */
	    /* This is required to ensure events with the "new" semantics   */
	    /* are not lost by the scheduler.				    */

	    /* Finally update the local value and adjust threads. 	    */
	    ect->val = v;
	    UnblockEvent(evt_st, ect, False);
	} 
    
	/* Get the result. */
	res = ect->val;
    
	/* We now block if val < ep_val. This test also takes care of the   */
	/* local case when ep_val was advanced after the test but before    */
	/* the critical section above.					    */
    
	if (EC_GT (val, res) && IN_FUTURE(until)) {
      
	    /* Block's precondition holds */
	    current = Pvs(thd);
	    if(Block(st, ect, current, val, until))
		alerted = True; /* => "ect" might have been freed */
	    else res = ect->val;
	}
    }
    Threads$LeaveCS((Threads_cl *)evt_st->thdf);
#ifdef PARANOIA
    if(!VP$ActivationMode(vpp))
	ntsc_dbgstop();
#endif
    if (alerted) RAISE_Thread$Alerted();

    return res;
}


/*---------------Sequencer-Operations------------------------------------*/

/*
 * Create a new sequencer
 */
static Event_Sequencer
NewSeq_m (Events_cl *self)
{
    Events_st   *st     = self->st;
    Evt_st      *evt_st = st->evt_st;
    VP_cl       *vpp    = evt_st->vpp;
    sequencer_t *seq    = NULL;
    bool_t mode;

    ENTER (vpp, mode); 
    seq = Heap$Malloc (evt_st->heap, sizeof (*seq));
    LEAVE (vpp, mode);
    if (!seq) RAISE_Events$NoResources();

    /* Initialise it */
    *seq = 0;
    return (Event_Sequencer) seq;
}

/*
 * Destroy a sequencer 
 */
static void
FreeSeq_m (Events_cl *self, Event_Sequencer sq)
{
    Events_st   *st     = self->st;
    Evt_st      *evt_st = st->evt_st;
    VP_cl       *vpp    = evt_st->vpp;
    bool_t mode;

    ENTER (vpp, mode); 
    Heap$Free(evt_st->heap, (sequencer_t *) sq); 
    LEAVE (vpp, mode);
}

/*
 * Read a sequencer
 */
static Event_Val
ReadSeq_m (Events_cl *self, Event_Sequencer sq)
{
    sequencer_t  *sqt = (sequencer_t *) sq;
    return *sqt;
}

/*
 * Ticket a sequencer
 */
static Event_Val
Ticket_m (Events_cl *self, Event_Sequencer sq)
{
    VP_cl *vpp = ((Events_st *)self->st)->evt_st->vpp;
    sequencer_t *sqt = (sequencer_t *) sq;
    Event_Val res;
    bool_t mode;

    ENTER (vpp, mode);
    {
	res = *sqt;
	(*sqt)++;
    }
    LEAVE (vpp, mode);

    return res;
}

/* 
 * Events and Channels
 */

static void 
Attach_m (Events_cl *self, Event_Count event, Channel_EP	chan,
	  Channel_EPType type)
{
    Event_Count_t *ect     = (Event_Count_t *) event;
    Events_st     *st      = self->st;
    Evt_st        *evt_st  = st->evt_st;
    VP_cl	  *vpp     = evt_st->vpp;
    NOCLOBBER ntsc_rc_t rc = rc_success;
    Channel_State  state;
    Channel_EPType ro_ty;
    Event_Val	   rx_val, rx_ack;

    TRY {
	Threads$EnterCS((Threads_cl *)evt_st->thdf, True);
	{
	    state = VP$QueryChannel (vpp, chan, &ro_ty, &rx_val, &rx_ack);
    
	    if (state == Channel_State_Connected)
	    {
		if (type != ro_ty)
		    RAISE_Channel$Invalid (chan);
	    }
	    else if (state != Channel_State_Local)
		RAISE_Channel$BadState (chan, state);
      
	    if (ect->ep != NULL_EP)
		RAISE_Channel$Invalid (event); /* already attached */
      
	    ect->ep	 = chan;
	    ect->ep_type = type;


	    if(type == Channel_EPType_RX) {
		CL_INIT  (ect->cn_cl, &ECNotify_ms, ect);
		ect->next_cn = 
		    ActivationF$Attach(evt_st->actf, &(ect->cn_cl), chan);
		if(ect->next_cn) {
		    ChainedHandler$SetLink((ChainedHandler_clp)ect->next_cn, 
					   ChainedHandler_Position_Before, 
					   (ChainedHandler_clp)&ect->cn_cl);
		}
		ect->prev_cn = NULL;
	    }

	    if (state == Channel_State_Connected)      
	    {
		if (type == Channel_EPType_RX)
		{
		    ect->val = rx_val;
		    UnblockEvent(evt_st, ect, False);
		}
		else
		{
		    TRC (fprintf(stderr, "Attach: send ep=%x val=%x\n", 
				 ect->ep, ect->val));
		    rc= ntsc_send (ect->ep, ect->val);
		}
	    }
	}
    } FINALLY {
	Threads$LeaveCS((Threads_cl *)evt_st->thdf);
    } ENDTRY;

    switch (rc)
    {
    case rc_success:
	break;
    case rc_ch_bad_state:
	RAISE_Channel$BadState (ect->ep, 0 );
    case rc_ch_invalid:
    default:
	fprintf(stderr, "Events$Attach: invalid channel (%x).\n", ect->ep);
	RAISE_Channel$Invalid (ect->ep);
    }

}

static void 
AttachPair_m (Events_cl		 *self,
	      const Event_Pair	 *events /* IN */,
	      const Channel_Pair *chans	 /* IN */ )
{
    Events_st     *st      = (Events_st *)self->st;
    Evt_st        *evt_st  = st->evt_st;
    VP_cl         *vpp     = evt_st->vpp;
    Event_Count_t *rx      = (Event_Count_t *) (events->rx);
    Event_Count_t *tx      = (Event_Count_t *) (events->tx);
    NOCLOBBER ntsc_rc_t rc = rc_success;
    Channel_State rx_state, tx_state;
    Channel_EPType type;
    Event_Val rx_val, rx_ack;
    Event_Val tx_val, tx_ack;

    TRY {
	Threads$EnterCS((Threads_cl *)evt_st->thdf, True);
	{
	    tx_state =
		VP$QueryChannel (vpp, chans->tx, &type, &tx_val, &tx_ack );

	    if (tx_state == Channel_State_Connected)
	    {
		if (type != Channel_EPType_TX)
		    RAISE_Channel$Invalid (chans->tx);
	    }
	    else if (tx_state != Channel_State_Local)
		RAISE_Channel$BadState (chans->tx, tx_state);
      
	    rx_state =
		VP$QueryChannel (vpp, chans->rx, &type, &rx_val, &rx_ack );

	    if (rx_state == Channel_State_Connected)
	    {
		if (type != Channel_EPType_RX)
		    RAISE_Channel$Invalid (chans->rx);
	    }
	    else if (rx_state != Channel_State_Local)
		RAISE_Channel$BadState (chans->rx, rx_state);

      
	    if (tx->ep != NULL_EP)
		RAISE_Channel$Invalid (events->tx); /* already attached */
	    if (rx->ep != NULL_EP)
		RAISE_Channel$Invalid (events->rx); /* already attached */
      
	    tx->ep	  = chans->tx;
	    tx->ep_type = Channel_EPType_TX;
      
	    rx->ep	  = chans->rx;
	    rx->ep_type = Channel_EPType_RX;

	    CL_INIT  (rx->cn_cl, &ECNotify_ms, rx);

	    rx->next_cn = 
		ActivationF$Attach(evt_st->actf, &(rx->cn_cl), chans->rx);
	    if(rx->next_cn) {
		ChainedHandler$SetLink((ChainedHandler_clp)rx->next_cn,
				       ChainedHandler_Position_Before,
				       (ChainedHandler_clp)&rx->cn_cl);
	    }
	    rx->prev_cn = NULL;
				   
      
	    if (rx_state == Channel_State_Connected)
	    {
		rx->val = rx_val;
		UnblockEvent(evt_st, rx, False);
	    }
	    if (tx_state == Channel_State_Connected)
		rc= ntsc_send (tx->ep, tx->val);
	}
    } FINALLY {
	Threads$LeaveCS((Threads_cl *)evt_st->thdf);
    } ENDTRY;


    switch (rc)
    {
    case rc_success:
	break;
    case rc_ch_bad_state:
	RAISE_Channel$BadState (tx->ep, 0 );
    case rc_ch_invalid:
    default:
	fprintf(stderr, "Events$AttachPair: invalid channel (%x).\n", tx->ep);
	RAISE_Channel$Invalid (tx->ep);
    }

}

static Channel_Endpoint QueryEndpoint_m(Events_clp self,
					Event_Count ec,
					Channel_EPType *type) {

    Events_st     *st      = self->st;
    Evt_st        *evt_st  = st->evt_st;
    Event_Count_t *ect    = (Event_Count_t *) ec;

    Channel_EPType type_tmp;
    Channel_Endpoint ep_tmp;
    VP_cl         *vpp     = evt_st->vpp;
    bool_t mode;

    if(ec == NULL_EVENT) RAISE_Events$Invalid(ec);
    
    ENTER(vpp, mode);

    /* Assign this to a temporary that we know can't page fault */
    type_tmp = ect->ep_type;

    /* this may be NULL_EP */
    ep_tmp = ect->ep;
    LEAVE(vpp, mode);

    *type = type_tmp;
    return ep_tmp;
}

static Channel_EP
AllocChannel_m (Events_cl *self)
{
    VP_cl *vpp = ((Events_st *)self->st)->evt_st->vpp;
    Channel_EP NOCLOBBER res = (Channel_EP) -1;
    bool_t NOCLOBBER mode;

    TRY {
	ChannelNotify_clp oldcn;

	ENTER (vpp, mode);
	res = VP$AllocChannel (vpp);
	oldcn = ActivationF$Attach(Pvs(actf), NULL, res);
	if(oldcn) {
	    eprintf("Allocated channel %u, attached to %p\n", res, oldcn);
	    ntsc_dbgstop();
	}
    }
    FINALLY 
	LEAVE (vpp, mode);
    ENDTRY;

    return res;
}

static void 
FreeChannel_m (Events_cl *self, Channel_EP ep)
{
    VP_cl *vpp = ((Events_st *)self->st)->evt_st->vpp;
    bool_t NOCLOBBER mode;

    TRY {
	ChannelNotify_clp oldcn;

	ENTER (vpp, mode);
	oldcn = ActivationF$Attach(Pvs(actf), NULL, ep);
	if(oldcn) {
	    eprintf("Trying to free channel %u, attached to %p\n",
		    ep, oldcn);
	    ntsc_dbgstop();
	}
	VP$FreeChannel (vpp, ep);
    }
    FINALLY 
	LEAVE (vpp, mode);
    ENDTRY;
}



/*-------------------------------------------- ChannelNotify Entry Points ---*/

/*
** The `builtin` EntryNotify$Notify function is below. 
** Note that this is generally expected to be called with 
** activations \emph{off}, and hence should make no assumptions 
** about pervasives being so.
*/


static void ECSetLink_m(ChainedHandler_clp self, 
			ChainedHandler_Position pos,
			ChainedHandler_clp link) {

    Event_Count_t *ect    = self->st; 
    
    if(pos == ChainedHandler_Position_Before) {
	ect->prev_cn = (ChannelNotify_clp) link;
    } else {
	ect->next_cn = (ChannelNotify_clp)link;
    }
}

/*
** Attached channel event handler. This is called when an event 
** notification comes in for a channel which is attached to an 
** event count.
*/

static void 
ECNotify_m(ChannelNotify_cl *self, Channel_Endpoint ep,
	    Channel_EPType type, Event_Val val, Channel_State state)
{
    Event_Count_t *ect    = self->st; 
    Evt_st        *evt_st = ect->evt_st;

    ASSERT( (state != Channel_State_Connected || type == ect->ep_type),
	    fprintf(stderr, 
		    "Events$ECNotify: vpp=%x type %x != ect->type %x !\n",
		    evt_st->vpp, type, ect->ep_type ); ntsc_dbgstop();)

    if (state == Channel_State_Dead)
    {
	/* XXX -  need to make binder close call at some point;
	   for now, just alert the waiters - with any luck they'll
	   have a FINALLY Close.   But what about TX's ? Ans: they'll
	   raise Channel_BadState when they're next advanced. */

	UnblockEvent(evt_st, ect, True);
    } else {
	ect->val = val;
	UnblockEvent(evt_st, ect, False);
    }
    
    if(ect->next_cn) {
	/* Carry on along the chain */
	ChannelNotify$Notify(ect->next_cn, ep, type, val, state);
    }
}



/*--------------------------------------------------- Internal Routines ---*/


/*
**  Block: 
**    enqueue the current thread "current" waiting for "val" on event "ec", 
**    and, (if "until != FOREVER"), waiting for the timeout "until". 
**    Then block & yield the current thread (i.e. oneself). 
**    Will return from yield once the thread is 
**       (a) unblocked, and 
**       (b) run again.
**    Return True iff we were unblocked via being alerted.
**
**  pre:  ec.val < val /\ (until == FOREVER \/ NOW() < until)
**  post: unblocked.
*/
static bool_t Block(Events_st *st, Event_Count_t *ec, Thread_cl *current,
		    Event_Val val, Time_T until)
{
    Evt_st     *evt_st = st->evt_st;
    qlink_t    *cur    = &st->qlink;
    link_t     *wq, *wqp;
    bool_t      alerted = False;

    /* The "exit" Events_clp closure is only used by the last thread,
       whilst shutting down the domain. Since we didn't know who this
       would be in advance, initialise the "thread" field now */

    if(st==evt_st->exit_st) {
	st->qlink.thread = current;
    }

    ASSERT(!VP$ActivationMode(evt_st->vpp), ntsc_dbgstop(); );
    ASSERT(cur->thread == current, ntsc_dbgstop(); );

    cur->waitVal   = val;
    cur->waitTime  = until;
    cur->blockTime = NOW();

    /* If we have valid event count, insert on event queue */
    if(ec != NULL) {

	wq  = WAIT_QUEUE (ec);
	wqp = wq->next;

	while (wqp != wq && WQP2QP(wqp)->waitVal <= val)
	    wqp = wqp->next;

	Q_INSERT_BEFORE(wqp, &cur->waitq);
    } else
	ZAP(&cur->waitq);
	
    /* If we have sensible timeout, insert on clock queue */
    if (until != FOREVER) {
	link_t *tqp;

	if(!ActivationF$AddTimeout(evt_st->actf, &(st->tn_cl), 
				   until, (word_t)current))  
	{
	    /* we've taken a long time to get here! */
	    if(cur->waitq.next != NULL) {
		Q_REMOVE(&cur->waitq);
		ZAP(&cur->waitq);
		ZAP(&cur->timeq);
	    }
	    return alerted;
	} 

	tqp  = evt_st->time_queue.next;
	while (tqp != &(evt_st->time_queue) && 
	       TQP2QP(tqp)->waitTime <= until)
	    tqp = tqp->next;
	Q_INSERT_BEFORE (tqp, &cur->timeq);

    } else
	ZAP(&cur->timeq);

    /* Now we block ourself the thread in the ULS, and yield. */
    alerted = ThreadF$BlockYield(evt_st->thdf, until);

    ASSERT(!VP$ActivationMode(evt_st->vpp), ntsc_dbgstop(); );

    return alerted;
}


/*
 *  RealUnblockEvent: if there are runnable threads on event "ect", unblock 'em
 *                (and potentially yield to one of them). 
 *  pre:  activations on/off
 *  post: activation mode preserved
 */

static void RealUnblockEvent(Evt_st *evt_st, Event_Count_t *ect, 
			     bool_t alerted)
{
    link_t *wait_queue = WAIT_QUEUE (ect);

    ASSERT(!VP$ActivationMode(evt_st->vpp), ntsc_dbgstop(); );

    while ((! Q_EMPTY (wait_queue)) &&  
	   (alerted || EC_LE( WQP2QP(wait_queue->next)->waitVal, ect->val))) {

	qlink_t *cur = WQP2QP(wait_queue->next);
	
	TRC(fprintf(stderr, "UnblockEvent (dom %lx): thread at %p\n", 
		    VP$DomainID(evt_st->vpp), cur->thread));

	Q_REMOVE(&cur->waitq); /* remove it from our queue */
	ZAP(&cur->waitq);

	if(cur->timeq.next != NULL) {
	    ActivationF$DelTimeout(evt_st->actf, cur->waitTime, 
				   (word_t)cur->thread);
	    Q_REMOVE(&cur->timeq);
	    ZAP(&cur->timeq);
	}

	if(alerted)
	    Thread$Alert(cur->thread);

	/* 
	** SMH: even though we know that any thread being unblocked
	** here blocked within a critical section (since Block() is 
	** surrounded by one), we don't give "True" as the final param
	** to UnblockThread. This is because:
	**  (a) we call this routine both from activation handlers and 
	**      from standard threads. This is actually safe to do as 
	**      long as we don't as for the unblocking of a critical thd. 
	**  (b) even if the thread we're unblocking was in a 'real' CS 
	**      (i.e. one outside this file), it did a voluntary block 
	**      by means of Await{|Until} and so is prepared to have 
	**      lost the processor for a while. 
	** I realise this is a bit grungy, but it seems the simplest way
	** to cope with CS; otherwise we'd need to call UnblockThread() 
	** when we'd been called from ECNotify and UnblockYield() otherwise, 
	** which opens a whole other can of worms. 
	*/
	ThreadF$UnblockThread(evt_st->thdf, cur->thread, False);
    }
    
    return;
}

/*----------------------------------------------- TimeNotify Entry Points ---*/

/*
** Time Notifcation handler; called with activations off.
*/
static void 
TimeNotify_m(TimeNotify_cl *self, Time_T now, Time_T deadline, word_t handle)
{
    Events_st *st     = self->st;
    Evt_st    *evt_st = st->evt_st;
    link_t    *tq     = &(evt_st->time_queue);
    link_t    *tqp;
    qlink_t   *cur;

    ASSERT(!VP$ActivationMode(evt_st->vpp), ntsc_dbgstop(); );

    /* 
    ** XXX SMH: currently we iterate through the list to find the 
    ** entry which has 'thread' matching our handle. However it 
    ** would probably be better to have a handle which was a pointer
    ** into the time queue, which would save this looping behaviour.
    */
    tqp = tq->next;
    while (tqp != tq) {

	cur = TQP2QP(tqp);

	if(cur->waitTime > now) 
	    break;

	if((word_t)cur->thread == handle) {
	    Q_REMOVE(&cur->timeq);
	    ZAP(&cur->timeq);

	    if(cur->waitq.next != NULL) {
		Q_REMOVE(&cur->waitq);
		ZAP(&cur->waitq);
	    }

	    ThreadF$UnblockThread(evt_st->thdf, cur->thread, False);
	    return;
	}

	tqp = tqp->next;
    }
    
}



