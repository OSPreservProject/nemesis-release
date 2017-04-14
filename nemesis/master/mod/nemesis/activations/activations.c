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
**      mod/nemesis/activations/activations.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Activation hooks for event channel processing, etc.
** 
** ENVIRONMENT: 
**
**      Activation handler mostly.
** 
** ID : $Id: activations.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
#include <bstring.h>
#include <string.h>
#include <setjmp.h>
#include <links.h>
#include <time.h>

#include <VPMacros.h>
#include <sdom.h>

typedef struct _ActivationF_st ActivationF_st;
#define __ActivationF_STATE ActivationF_st
#include <ActivationF.ih>
#include <ActivationMod.ih>
#include <ChannelNotify.ih>
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



/*#define PARANOIA*/
#ifdef PARANOIA
#define ASSERT(cond,fail) {						\
    if (!(cond)) { 							\
	eprintf("%s assertion failed in function %s (line 0x%x)\n", 	\
		__FILE__, __FUNCTION__, __LINE__);			\
	{fail}; 							\
	while(1); 							\
    }									\
}
#else
#define ASSERT(cond,fail)
#endif



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
do { (mode) = VP$ActivationMode (vpp); VP$ActivationsOff (vpp); } while(0)

#define LEAVE(vpp,mode)				\
do {						\
  if (mode)					\
  {						\
    VP$ActivationsOn (vpp);			\
    if (VP$EventsPending (vpp))			\
      VP$RFA (vpp);				\
  }						\
} while (0);



/* Internal functions */
static bool_t CheckEvents   (ActivationF_st *st);
static bool_t CheckTimeouts (ActivationF_st *st, Time_T now);


/*
 * Module Stuff
 */

static  ActivationF_Attach_fn           Attach_m;
static  ActivationF_MaskEvent_fn        MaskEvent_m;
static  ActivationF_UnmaskEvent_fn      UnmaskEvent_m;
static  ActivationF_MaskEvents_fn       MaskEvents_m;
static  ActivationF_UnmaskEvents_fn     UnmaskEvents_m;
static  ActivationF_AddTimeout_fn       AddTimeout_m;
static  ActivationF_DelTimeout_fn       DelTimeout_m;
static  ActivationF_MaskTimeouts_fn     MaskTimeouts_m;
static  ActivationF_UnmaskTimeouts_fn   UnmaskTimeouts_m;
static  ActivationF_SetHandler_fn       SetHandler_m;
static  ActivationF_Reactivate_fn       Reactivate_m;

static  ActivationF_op  ms = {
    Attach_m,
    MaskEvent_m,
    UnmaskEvent_m,
    MaskEvents_m,
    UnmaskEvents_m,
    AddTimeout_m,
    DelTimeout_m,
    MaskTimeouts_m,
    UnmaskTimeouts_m,
    SetHandler_m,
    Reactivate_m
};


/* 
** Attachments are basically ChannelNotify closures, except that they
** are optimised such that only the address and first argument of the
** Notify operation are stored.  This argument is the closure
** pointer. 
*/

/* XXX - is this optimisation actually worth it? */

typedef struct _attachment attachment_t;
struct _attachment {
    ChannelNotify_Notify_fn *fn;
    ChannelNotify_clp        clp;
    bool_t                   masked;
    bool_t                   pending;
};

typedef SEQ_STRUCT(attachment_t) attseq_t;

#define ATTACHMENTS(st)    (&(st->attachments))
#define ATTCHMNT(st,i)     (SEQ_ELEM(ATTACHMENTS(st),i))


typedef struct _tlink tlink_t;
struct _tlink {
    tlink_t       *next;
    tlink_t       *prev;
    TimeNotify_cl *nfy;
    Time_T         deadline;
    word_t         handle;
};

/* State record for an instance of the ActivationF interface */
struct _ActivationF_st {
    ActivationF_cl cl;
    Activation_cl  act_cl;
    VP_clp         vpp;		 /* VP we are running over               */
    Time_clp       time;         /* Since Pvs(time) not valid in ah      */
    Heap_clp       heap;         /* Our (unlocked!) heap                 */
    Activation_clp next;         /* if !NULL, next handler to call       */
    jmp_buf        jb;           /* for sneaky reactivations             */

    /* Event channel notification state */
    bool_t         emasked;      /* Are event notifications masked?      */
    attseq_t       attachments;  /* channel <-> notify info              */

    /* Time notification state */
    uint32_t       nts;          /* Max no. of timeouts we can deal with */
    bool_t         tmasked;      /* Are time notifications masked?       */
    tlink_t        time_queue;   /* ordered queue of timeout notifys     */
    tlink_t        free_queue;   /* list of free tlink_t structures      */
};


static Activation_Go_fn          ActivationGo_m;
static Activation_op activation_ms = { ActivationGo_m };

#define EVTRC(_x) 


/*------------------------------------------ ActivationMod Entry Points ---*/

static ActivationMod_New_fn ActivationMod_New;

static ActivationMod_op am_ms = {
    ActivationMod_New,
};

static const ActivationMod_cl am_cl = { &am_ms, NULL };

CL_EXPORT (ActivationMod, ActivationMod, am_cl);


static ActivationF_cl *ActivationMod_New(ActivationMod_cl *self, 
					 VP_clp  vpp, Time_clp time,
					 Heap_clp heap, uint32_t nts,
					 /* OUT */ Activation_clp *avec)
{
    ActivationF_st *st; 
    Heap_Size sz = sizeof(*st) + (nts * sizeof(tlink_t));
    tlink_t *links;
    uint32_t i, nch;

    if(!(st= Heap$Malloc(heap, sz))) {
	DB(fprintf(stderr, "ActivationMod$New: malloc failed.\n"));
	return (ActivationF_cl *)NULL;
    }
    bzero(st, sz);

    st->vpp  = vpp; 
    st->time = time;
    st->heap = heap;

    st->emasked = False;       /* events not masked intially */ 
    st->tmasked = False;       /* nor timeouts               */

    nch = VP$NumChannels (vpp);
    SEQ_INIT(ATTACHMENTS(st), nch, st->heap);   /* all NULL */


    Q_INIT(&st->time_queue);
    Q_INIT(&st->free_queue);

    links = (tlink_t *)(st + 1);
    for(i=0; i < nts; i++) 
	Q_INSERT_BEFORE(&st->free_queue, (tlink_t *)(links + i));

    CL_INIT(st->cl, &ms, st);
    CL_INIT(st->act_cl, &activation_ms, st);

    *avec = &(st->act_cl);
    TRC(eprintf("Returning ActivationF_clp at %p\n", &(st->cl)));
    return &(st->cl);
}



/*---------------------------------------------------- Entry Points ----*/




/*
 * Attach an ChannelNotify to an event.
 * There is an argument here for fixing the following race condition:
 *
 *  1) Channel does not appear to have pending events.
 *  2) Events arrive on channel.
 *  3) Attach is performed without notification arriving.
 *
 * However, the intention is that AttachNotify is called by an "Entry"
 * itself as a result of a Bind operation. Hence, it's much easier for
 * the Entry to resolve the race after the Attach, since it can then
 * examine its own FIFO to prevent duplication.
 */


static ChannelNotify_clp 
Attach_m(ActivationF_cl *self, ChannelNotify_clp cn, Channel_RX rx)
{
    ActivationF_st     *st  = (ActivationF_st *)self->st;
    VP_cl              *vpp = st->vpp;
    Event_Val	 	rx_val, rx_ack;
    Channel_State	state;
    Channel_EPType	type;
    ChannelNotify_clp	res;
    bool_t              mode;
    attachment_t       *att;
    TRC (fprintf(stderr, "ActivationF$Attach: cn=%x rx=%x\n", cn, rx));

    /* QueryChannel will bail if the channel is invalid */
    state = VP$QueryChannel (vpp, rx, &type, &rx_val, &rx_ack);

    /* Since we can now chain handlers together, and once an endpoint
       has been closed (entered the Dead state) a handler might want
       to remove itself from the chain by plugging in the person above
       it, we can't make any assertions about the state of the channel
       other than that it shouldn't be Free. */

    if(state == Channel_State_Free) {
	RAISE_Channel$BadState(rx, state);
    }
    
    att = &ATTCHMNT(st, rx);
    
    ENTER(st->vpp, mode);    
    if (att->fn == NULL) 
	res = NULL;
    else
	res = att->clp;

    TRC(eprintf("ActF(chan %d): Old closure %p(%p, %p), "
		"New closure %p(%p, %p)\n", rx,  
		res, res?res->op:NULL, res?res->st:NULL,
		cn, cn?cn->op:NULL, cn?cn->st:NULL));
    
    att->clp = cn;
    att->fn = cn ? cn->op->Notify : NULL;
    att->masked = False;

    LEAVE(st->vpp, mode);
	
    return res;

}

static bool_t MaskEvent_m(ActivationF_cl *self, Channel_RX rx)
{
    ActivationF_st     *st  = self->st;
    VP_cl              *vpp = st->vpp;
    Channel_State	state;
    Channel_EPType      type;
    Event_Val	        rx_val, rx_ack;
    bool_t              mode, res;
    
    /* QueryChannel will bail if the channel is invalid */
    state = VP$QueryChannel (vpp, rx, &type, &rx_val, &rx_ack);
    
    if (type != Channel_EPType_RX || state == Channel_State_Free) {
	RAISE_Channel$BadState (rx, state);
    }

    if (SEQ_ELEM (ATTACHMENTS(st), rx).fn == NULL) {
	DB(fprintf(stderr, 
		   "ActivationF$Attach: vpp=%x attachments[%x] != NULL !\n",
		   vpp, rx));
	RAISE_Channel$Invalid(rx);
    }
    
    ENTER(vpp, mode);
    if(SEQ_ELEM (ATTACHMENTS(st), rx).masked) 
	res = False; 
    else res = SEQ_ELEM (ATTACHMENTS(st), rx).masked = True;
    LEAVE(vpp, mode);

    return res; 
}

static bool_t UnmaskEvent_m(ActivationF_cl *self, Channel_RX rx)
{
    ActivationF_st *st  = self->st;
    VP_cl          *vpp = st->vpp;
    attachment_t   *att;
    Channel_State   state;
    Channel_EPType  type;
    Event_Val	    rx_val, rx_ack;
    bool_t          mode, res;
    
    /* QueryChannel will bail if the channel is invalid */
    state = VP$QueryChannel (vpp, rx, &type, &rx_val, &rx_ack);
    
    if (type != Channel_EPType_RX || state == Channel_State_Free) {
	RAISE_Channel$BadState (rx, state);
    }

    if (SEQ_ELEM (ATTACHMENTS(st), rx).fn == NULL) {
	DB(fprintf(stderr, 
		   "ActivationF$Attach: vpp=%x attachments[%x] != NULL !\n",
		   vpp, rx));
	RAISE_Channel$Invalid(rx);
    }
    
    ENTER(vpp, mode);

    att = &SEQ_ELEM(ATTACHMENTS(st), rx);

    if(att->masked) {

	att->masked = False;

	/* Notify any pending events (as long as all events aren't masked) */
	if(!st->emasked && att->pending) {
	    att->fn (att->clp, rx, type, rx_val, state);
	    att->pending = False;
	}
	res = True;

    } else 
	res = False;

    LEAVE(vpp, mode);

    return res; 
}

static void MaskEvents_m(ActivationF_cl  *self)
{
    ActivationF_st     *st  = self->st;
    VP_cl              *vpp = st->vpp;
    bool_t              mode;
    
    ENTER(vpp, mode);
    st->emasked = True;
    LEAVE(vpp, mode);
    return;
}

static void UnmaskEvents_m(ActivationF_cl  *self)
{
    ActivationF_st *st  = self->st;
    VP_cl          *vpp = st->vpp;
    attachment_t   *att;
    bool_t          mode;
    Channel_EPType  type;
    Event_Val	    ack, val;
    Channel_State   state;
    int             i;

    ENTER(vpp, mode);

    st->emasked = False;

    /* Now notify on any unmasked and pending channels */
    for(i = 0; i < SEQ_LEN(ATTACHMENTS(st)); i++) {
	att = &SEQ_ELEM (ATTACHMENTS(st), i);

	if(att->pending && !att->masked) {
	    /* Invoke the closure associated with this end-point. */
	    state = VP$QueryChannel (vpp, i, &type, &val, &ack);
	    att->fn (att->clp, i, type, val, state);
	    att->pending = False;
	}
    }

    LEAVE(vpp, mode);
    return;
}



static bool_t AddTimeout_m(ActivationF_cl *self, TimeNotify_cl *tn, 
			   Time_T deadline, word_t handle)
{
    ActivationF_st *st  = self->st;
    Time_T NOCLOBBER now = Time$Now(st->time);
    tlink_t *new, *tq, *tqp;
    bool_t mode;

    if(deadline <= now)
	return False;

    EVTRC(eprintf("AddTimeout: now=%q, deadline=%q \n", now, deadline));

    ENTER(st->vpp, mode);
    
    CheckTimeouts(st, now);

    if(Q_EMPTY(&st->free_queue))
	RAISE_ActivationF$TooManyTimeouts();  /* OUCH! */

    new = st->free_queue.prev; 
    Q_REMOVE(new);

    new->nfy = tn;
    new->deadline = deadline;
    new->handle = handle;

    tq = &(st->time_queue);
    tqp = tq->next;

    while(tqp != tq && tqp->deadline < deadline)
	tqp = tqp->next;

    Q_INSERT_BEFORE(tqp, new);
    LEAVE(st->vpp, mode);
    
    return True;
}
    
static bool_t 
DelTimeout_m(ActivationF_cl *self, Time_T deadline, word_t handle)
{
    ActivationF_st *st  = self->st;
    tlink_t *tq, *tqp;
    bool_t mode;

    ENTER(st->vpp, mode);
    
    tq = &(st->time_queue);
    tqp = tq->next;

    while(tqp != tq) {
	if((tqp->deadline == deadline) && (tqp->handle == handle))
	    break;
	tqp = tqp->next;
    }

    if(tqp == tq) {
	/* Some idiot is removing something which is already gone  */
	LEAVE(st->vpp, mode);
	return False;
    }

    Q_REMOVE(tqp);

#ifdef PARANOIA
    tqp->nfy = NULL;
    tqp->deadline = 0;
    tqp->handle = 0;
#endif

    Q_INSERT_BEFORE(&st->free_queue, tqp);
    LEAVE(st->vpp, mode);
    
    return True;
}

static void MaskTimeouts_m(ActivationF_cl  *self)
{
    ActivationF_st     *st  = self->st;
    VP_cl              *vpp = st->vpp;
    bool_t              mode;
    
    ENTER(vpp, mode);
    st->tmasked = True;
    LEAVE(vpp, mode);
    return;
}

static void UnmaskTimeouts_m(ActivationF_cl *self)
{
    ActivationF_st     *st  = self->st;
    VP_cl              *vpp = st->vpp;
    bool_t              mode;
    
    ENTER(vpp, mode);
    st->tmasked = False;
    LEAVE(vpp, mode);
    return;
}

static Activation_clp SetHandler_m(ActivationF_cl *self, Activation_clp avec)
{
    ActivationF_st *st = self->st;
    Activation_cl *res;
    
    res = st->next;
    st->next = avec;
    return res;
    
}

/*-------------------------------------------------- Internal Routines ---*/


/*
**  CheckEvents: 
**    for each event, invoke the associated notify (if there is one).
**    Should be called with activations off.
*/

static bool_t CheckEvents(ActivationF_st *st )
{
    VP_clp	   vpp = st->vpp;
    Channel_EP	   ep;
    Channel_EPType type;
    Event_Val	   val;
    Channel_State  state;
    bool_t         res = False;
    attachment_t  *att

    ASSERT(!VP$ActivationMode(st->vpp), ntsc_dbgstop(); );

    while(VP$NextEvent (vpp, &ep, &type, &val, &state))
    {
	EVTRC(eprintf("  EVENT dom=%x ep=%x type=%x val=%x state=%x\n",
		      VP$DomainID(vpp), ep, type, val, state));

	if (ep < SEQ_LEN (ATTACHMENTS(st))) {

	    att = &SEQ_ELEM (ATTACHMENTS(st), ep);

	    if(att->fn) {
		if(!att->masked) {
		    /* Invoke the closure associated with this end-point. */
		    att->fn (att->clp, ep, type, val, state);
		    res = True;
		} else {
		    /* Currently masked; mark for later notifcation */
		    att->pending = True; 
		}
	    }
	}
    }

    return res;
}


static bool_t CheckTimeouts(ActivationF_st *st, Time_T now)
{
    tlink_t *tq, *cur;
    bool_t res = False;

    ASSERT(!VP$ActivationMode(st->vpp), ntsc_dbgstop(); );

    tq = &(st->time_queue);
    while(!Q_EMPTY(tq)) {

	cur = tq->next; /* look at the head of the queue */

	if(cur->deadline > now)  /* queue is ordered, so if deadline of */
	    break;               /* head is after now, we're done.      */

	/* Otherwise we have the head of the queue with a deadline which 
	   is <= now, and hence we wish to call its notify handler.     */
	TimeNotify$Notify(cur->nfy, now, cur->deadline, cur->handle);

	/* Okay: all done, so remove it from our queue and put it 
	   back onto the free queue to be used another time.            */
	Q_REMOVE(cur);
	Q_INSERT_BEFORE(&st->free_queue, cur);

	/* Something has happened, so we wish to return true */
	res = True;
    }
    return res;
}

/*-------------------------------------------------- Activation Handler ---*/




/*
 * Activation handler
 */
static void 
ActivationGo_m (Activation_cl *self, VP_clp vpp, Activation_Reason ar)
{
    ActivationF_st *st  = self->st; 

    /* Activations were on just before calling here, so we weren't in
       a critical section when we last lost the processor.  Activations
       are now off. */

    ASSERT(!VP$ActivationMode(vpp), ntsc_dbgstop(); );
    ASSERT((st->vpp == vpp), ntsc_dbgstop(); );

    if(!st->emasked) CheckEvents(st);
    if(!st->tmasked) CheckTimeouts(st, (RO(vpp))->sdomptr->lastschd);

    if(setjmp(st->jb)) /* reactivated via below */
	ar = Activation_Reason_Reactivated;

    /* invoke the next activation handler */
    if(st->next) 
	Activation$Go(st->next, vpp, ar);

/* NOTREACHED */
    ASSERT(False, ntsc_dbgstop(); );
}

static void Reactivate_m(ActivationF_cl *self)
{
    ActivationF_st *st = self->st; 
    bool_t events, timeouts;
    Time_T until = Q_EMPTY(&st->time_queue) ? FOREVER : 
	st->time_queue.next->deadline;

    ASSERT(!VP$ActivationMode(st->vpp), ntsc_dbgstop(); );

    events   = st->emasked ? False : CheckEvents(st);
    timeouts = st->tmasked ? False : CheckTimeouts(st, Time$Now(st->time));

    if(events || timeouts)
	_longjmp(st->jb);
    else VP$RFABlock(st->vpp, until);
    

/* NOTREACHED */
    ASSERT(False, ntsc_dbgstop(); );
}

/*
 * End 
 */
