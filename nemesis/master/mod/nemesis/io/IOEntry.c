/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/io/IOEntry.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      An implementation of IOEntry: queueing IO channels for
**	Nemesis threads to service.
** 
** ENVIRONMENT: 
**
**      User space, part of ANSA-like runtime
** 
** ID : $Id: IOEntry.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <time.h>
#include <ecs.h>


#include <VPMacros.h>

#include <ActivationF.ih>
#include <ChannelNotify.ih>
#include <IOEntryMod.ih>

#undef IOENTRY_TRACE
#undef DEBUG

#ifdef IOENTRY_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif



/*
 * Create an IOEntry
 */
static IOEntryMod_New_fn	New_m;
static IOEntryMod_op m_op = { New_m };
static const IOEntryMod_cl m_cl = { &m_op, NULL };

CL_EXPORT (IOEntryMod, IOEntryMod, m_cl);

/*
 * IOEntry interface operations 
 */
static IOEntry_Bind_fn		Bind_m;
static IOEntry_Unbind_fn	Unbind_m;
static IOEntry_Rendezvous_fn	Rendezvous_m;
static IOEntry_Suspend_fn   	Suspend_m; 
static IOEntry_Close_fn		Close_m;
static IOEntry_op e_op = { 
    Bind_m, 
    Unbind_m, 
    Rendezvous_m, 
    Suspend_m, 
    Close_m,
};

/*
 * ChannelNotify interface operations
 */
static ChainedHandler_SetLink_fn SetLink_m;
static ChannelNotify_Notify_fn	 Notify_m;
static ChannelNotify_op cn_op = { SetLink_m, Notify_m };

/* 
 * Data structures
 */

#define defstruct(_x) typedef struct _x _x; struct _x

defstruct (bindlink_t) {
    bindlink_t		       *next;
    bindlink_t		       *prev;
};

defstruct (ioentry_st) {
    
    /* Closures */
    struct _IOEntry_cl 	entry_cl;

    ActivationF_clp     actf;
    Events_clp		events;
    VP_clp		vp;
    
    /* We use an event count to keep track of insertions into the
       pending queue. We also keep a count of removals from the queue to
       allow us to determine if the queue is empty without the need for
       synchronisation in some cases. */ 
    Event_Count	    	events_in;	/* no. of entries made	*/
    volatile Event_Val	events_out;	/* no. of extractions	*/
    
    /* Mapping of end-points to bindings */
    uint32_t	    	num_eps;	/* Current no. of bindings */
    uint32_t	    	max_eps;	/* Max no. of bindings */
    
    /* List of bindings with events pending or which need closing.
       "pending" means the binding is on the queue awaiting service,
       "idle" means there is no activity on the binding or it is in the
       FIFO.  */
    bindlink_t	     	pending;
    bindlink_t	     	idle;
};

/* Structure containing what the entry knows about the binding. */
defstruct (binding_t) {
    bindlink_t			link;	/* Must be first in structure 	*/
    Channel_Endpoint		ep;	/* Channel end-point		*/
    IO_clp			io;	/* IO channel			*/
    ioentry_st		       *entry;	/* Entry state			*/
    IOEntry_State	       	state;	/* Current state		*/
    struct _ChannelNotify_cl	cn_cl;	/* Closure for notification	*/
    ChannelNotify_clp		next_cn;/* Next handler in chain       	*/
    ChannelNotify_clp           prev_cn;/* Previous handler in chain    */
};


#define DOM(st) (VP$DomainID (st->vp))

/*
 * Statics
 */
static binding_t *FindBinding (ioentry_st *st, Channel_Endpoint ep);


/*
 * User Level Critical sections. See Thread Scheduler.
 *
 * ENTER/LEAVE sections are not nestable in this entry.
 *
 */

static INLINE void ENTRY_Lock (ioentry_st *st)
{
    VP$ActivationsOff (st->vp);
}

static INLINE void ENTRY_Unlock (ioentry_st *st)
{
    VP$ActivationsOn (st->vp);
    if (VP$EventsPending (st->vp))
	VP$RFA (st->vp);
}

/*--------Build-an-entry--------------------------------------------*/

static IOEntry_clp New_m(IOEntryMod_clp	 self,
			 ActivationF_clp actf, 
			 VP_clp		 vp,
			 uint32_t	 nch)
{
    ioentry_st *st;
    word_t      size = sizeof(*st);

    /* Allocate structure */
    if ( !(st = Heap$Malloc(Pvs(heap), size )) ) {
	DB(eprintf("IOEntryMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }

    TRY
    {
	CL_INIT (st->entry_cl,  &e_op,  st);

	st->actf   = actf;
	st->events = Pvs(evs);
	st->vp     = vp;

	st->events_in  = EC_NEW();
	st->events_out = 0;
    
	st->num_eps = 0;
	st->max_eps = nch;
    
	LINK_INIT( &st->pending );
	LINK_INIT( &st->idle );
    }
    CATCH_Events$NoResources()
    {
	FREE(st);
	RERAISE;
    }
    ENDTRY;

    return &st->entry_cl;
}

/*--------IOEntry-operations------------------------------------------*/

/*
 * Bind a channel end-point to an IO
 */
static void Bind_m(IOEntry_clp		self, 
		   Channel_Endpoint 	chan,
		   Channel_Endpoint 	acknotused,
		   IO_clp		io)
{
    ioentry_st *st = self->st;
    binding_t  *b  = Heap$Malloc(Pvs(heap), sizeof(*b));

    TRC(eprintf("IOEntry$Bind: chan=%x io=%x\n", chan, io));

    /* Check we've got a binding to play with */
    if (!b) {
	DB(eprintf("IOEntry$MapAssoc: out of memory.\n"));
	RAISE_Heap$NoMemory();
    }
    
    /* Optimistically initialise the binding */
    b->ep    = chan;
    b->io    = io;
    b->entry = st;
    b->state = IOEntry_State_Idle;
    LINK_INIT(&b->link);
    CL_INIT( b->cn_cl, &cn_op, b );

    /* Then dive into a critical section to install the entry in the
       scheduler */
    TRY
    {
	USING (ENTRY, st,
	   {
	       if ( st->num_eps >= st->max_eps ) {
		   DB(eprintf("IOEntry$Bind: Too many bindings!\n"));
		   RAISE_IOEntry$TooManyChannels();
	       }
	       
	       /* Notify the Thread scheduler we're interested */
	       b->next_cn = ActivationF$Attach(st->actf, &b->cn_cl, chan);

	       if(b->next_cn) {
		   ChainedHandler$SetLink((ChainedHandler_clp)b->next_cn,
					  ChainedHandler_Position_Before,
					  (ChainedHandler_clp)&b->cn_cl);
	       }
		   
	       
	       /* Bump up the count */
	       st->num_eps++;
      
	       /* Link into the idle queue */
	       LINK_ADD_TO_TAIL(&st->idle,&b->link);
	   });
    }
    CATCH_ALL
    {
	FREE(b);
	RERAISE;
    }
    ENDTRY;
}

/*
 * Remove a binding of a channel end-point to an IO
 */
static void Unbind_m(IOEntry_clp	self, 
		     Channel_Endpoint 	chan)
{
    ioentry_st *st = self->st;
    binding_t * NOCLOBBER b = NULL;

    USING (ENTRY, st, 
       {
	   /* Check the channel exists */
	   if ( !( b = FindBinding (st, chan)) ) {
	       DB(eprintf("IOEntry$Unbind: channel not found.\n"));
	       RAISE_Channel$Invalid( chan );
	   }
	   
	   /* Remove it */
	   LINK_REMOVE (&b->link);
    
	   /* If we attached "ec" to the actf, we need to detach it */
	   if(b->prev_cn) {
	       ChainedHandler$SetLink((ChainedHandler_clp)b->prev_cn,
				      ChainedHandler_Position_After, 
				      (ChainedHandler_clp)b->next_cn);
	   } else {
	       
	       /* If we were the head of the notify queue, attach the
		  old handler (maybe NULL) to the endpoint */
	       ActivationF$Attach(st->actf, b->next_cn, chan);
	       
	   }
	   
	   if(b->next_cn) {
	       ChainedHandler$SetLink((ChainedHandler_clp)b->next_cn,
				      ChainedHandler_Position_Before,
				      (ChainedHandler_clp)b->prev_cn);
	   }
	   
	   /* Reduce count */
	   st->num_eps--;
       });

    if (b) FREE (b);
}
static void Suspend_m(IOEntry_clp self, IO_clp io)
{
/*     ioentry_st *st = self->st; */
    
    /* XXX not implemented in this entry... */
    return;
}

/*
 * Wait for any IO channel to become ready.
 * "to" is the absolute time to time out.
 */
static IO_clp Rendezvous_m(IOEntry_clp 	self, 
			   Time_ns	to )
{
    ioentry_st	         *st   = self->st;
    Event_Val		  out;
    Event_Val		  new_in;
    Event_Val   NOCLOBBER	  in;
    binding_t * NOCLOBBER todo = NULL;

    TRC(eprintf("IOEntry$Rendezvous(dom=%lx): to=%lx\n", DOM(st), to));

    /* First, empty the FIFO. This updates our queues so that they
       reflect everything which has happened externally up to this
       point. */

    for(;;)
    {
	/* read both event counts in order to prevent race */
	out = st->events_out;
	in  = EC_READ(st->events_in);
    
	if (in > out)
	{
	    /* We might have something to do. Acquire the lock and look at
	       the queue to make sure. We only pass once through the
	       FIFO. If anything arrives in the meantime it will be picked
	       up by the next thread to do a rendezvous. */

	    /* XXX - the queueing mechanism used here is probably wrong.
	       We should probably return an "io" once per packet, and only
	       deem a binding idle if there are no more packets on it. */

	    USING (ENTRY, st, 
	       {
		   out = st->events_out;
		   in  = EC_READ(st->events_in);
		   
		   if (in > out)
		   {
		       Event_Val ackval;
		       /* Remove something from the queue */
		       todo = (binding_t *) (st->pending.next);
		       LINK_REMOVE (&todo->link);

		       /* Receive subsequent notifications (and ensure
                          there are some) 
		       
			  Make sure we Ack all events, including those
			  which might appear between calling VP$Poll
			  and VP$Ack */

		       do  {
			   ackval = VP$Poll(st->vp, todo->ep);
		       } while (ackval != VP$Ack(st->vp, todo->ep, ackval));

		       todo->state = IOEntry_State_Idle;
		       LINK_ADD_TO_TAIL (&st->idle, &todo->link);
		       
		       TRC(eprintf("IOEntry$Rendezvous(dom=%lx): work todo=%x\n",
				   DOM(st), todo));

		       /* Bump outward count */
		       st->events_out++;
		   }
	       });
	}
	/* If we have an end-point, break out and run with it */
	if (todo) break;
    
	/* Wait until a new event has come in to the entry. */
	TRC(eprintf("IOEntry$Rendezvous(dom=%lx): await event %x in=%x\n",
		    DOM(st), st->events_in, in));
	new_in = EC_AWAIT_UNTIL(st->events_in, in + 1, to);
    
	/* If nothing new has arrived, we've timed out so just return */
	if (new_in == in) return NULL;
    }
  
    /* By this stage we have a binding_t called "todo" in our hands,
       whose endpoint is either Connected or Dead.  Events received at
       the endpoint must be acked by higher levels.  This will normally
       occur automatically when the calling thread calls IO_GetPkt,
       which waits on an attached event count, which in turn does the ack. */

    return todo->io;  
}


/*
 * Close down this entry
 */
static void Close_m(IOEntry_clp self)
{
    ioentry_st *st = self->st;

    /* Check that we can go away */
    USING (ENTRY, st,
       {
	   if (st->num_eps) {
	       DB(eprintf("IOEntry$Close: entry has endpoints outstanding\n"));
	       RAISE_IOEntry$TooManyChannels();
	   }
	   
	   if (!Q_EMPTY (&st->pending) || !Q_EMPTY (&st->idle))
	   {
	       DB(eprintf("IOEntry$Close: entry has bindings outstanding.\n"));
	       RAISE_IOEntry$Inconsistent();
	   }
       });

    /* We now assume we're the only thread in this entry, otherwise the */
    /* client has really ballsed up */
    EC_FREE (st->events_in);
    FREE (st);
}

/*--------EventNotify-operations----------------------------------------*/

static void SetLink_m(ChainedHandler_clp self,
		      ChainedHandler_Position pos,
		      ChainedHandler_clp link) {

    binding_t  *b  = self->st;
    
    if(pos == ChainedHandler_Position_Before) {
	b->prev_cn = link;
    } else {
	b->next_cn = link;
    }
}


/*
 * Notification from the activation handler that an event channel has
 * been kicked. 
 * 
 * This MUST be called with activations off.
 */
static void Notify_m(ChannelNotify_clp 	self, 
		     Channel_Endpoint 	chan,
		     Channel_EPType  	type,
		     Event_Val		val, 
		     Channel_State	state )
{
    binding_t  *b  = self->st;
    ioentry_st *st = b->entry; 

    TRC (eprintf ("IOEntry$Notify(dom=%x): ch=%lx...", DOM(b->entry), chan));

    if (b->state == IOEntry_State_Idle)
    {
	/* Move the binding on to the pending queue and kick the event count */
	LINK_REMOVE (&b->link);
	b->state = IOEntry_State_Pending;
	LINK_ADD_TO_TAIL (&st->pending,&b->link);
	Events$Advance (st->events, st->events_in, 1);
	TRC (eprintf ("kick: ec=%lx val=%lx\n", st->events_in,
		      Events$Read (st->events, st->events_in) ));
    }
    else
    {
	DB({Channel_State	epstate;
	       Channel_EPType	eptype;
	       Event_Val	        epval;
	       Event_Val	        epack;

	       epstate = VP$QueryChannel (st->vp, chan,
					  &eptype, &epval, &epack);
	       eprintf("IOEntry$Notify: Warning: somebody is doing BLOCKING IO$GetPkt on IO %p\n\t(RX endpoing %x val=%x ep.val=%x ep.ack=%x)\n", b->io, chan, val, epval, epack);
	   })
    }

    /* Let the others have a go */
    if (b->next_cn)
	ChannelNotify$Notify (b->next_cn, chan, type, val, state);
}

/*
 * Given a channel end-point return the binding associated with it.
 * Return NULL if none.
 */
static binding_t *FindBinding(ioentry_st	*st,
			      Channel_Endpoint ep )
{
    bindlink_t *b;
  
    for (b = st->pending.next; b != &st->pending; b = b->next)
	if ( ((binding_t *)b)->ep == ep )
	    return (binding_t *)b;

    for (b = st->idle.next; b != &st->idle; b = b->next)
	if ( ((binding_t *)b)->ep == ep )
	    return (binding_t *)b;

    return (binding_t *)0;
}

