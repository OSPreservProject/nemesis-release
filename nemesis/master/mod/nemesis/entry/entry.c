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
**      mod/nemesis/entry/entry.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Inplementation of "Entry.if": queueing invocation requests for
**	Nemesis threads to execute.
** 
** ENVIRONMENT: 
**
**      User space, part of ANSA-like runtime
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
#include <links.h>
#include <time.h>
#include <ecs.h>

#include <VPMacros.h>
#include <ActivationF.ih>
#include <EntryMod.ih>
#include <ChannelNotify.ih>


#ifdef ENTRY_TRACE
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
 * Create an entry
 */
static EntryMod_New_fn	New_m;
static EntryMod_op m_op = { New_m };
static const EntryMod_cl m_cl = { &m_op, NULL };

CL_EXPORT (EntryMod, EntryMod, m_cl);

/*
 * Entry interface operations 
 */
static Entry_Bind_fn	Bind_m;
static Entry_Unbind_fn	Unbind_m;
static Entry_Rendezvous_fn	Rendezvous_m;
static Entry_Closure_fn	Closure_m;
static Entry_Close_fn	Close_m;
static Entry_op e_op = { Bind_m, Unbind_m, Rendezvous_m, Closure_m, Close_m };

/*
 * Entry notify interface operations
 */
static ChainedHandler_SetLink_fn SetLink_m;
static ChannelNotify_Notify_fn	Notify_m;
static ChannelNotify_op en_op = { SetLink_m, Notify_m };

/*
 * Closure closure for tasks spawned on this entry
 */
static Closure_Apply_fn		Apply_m;
static Closure_op cl_op = { Apply_m };

/* 
 * Data structures
 */

#define defstruct(_x) typedef struct _x _x; struct _x

defstruct (bindlink_t) {
    bindlink_t		       *next;
    bindlink_t		       *prev;
};

defstruct (entry_st) {
    
    /* Closures */
    struct _Entry_cl 		entry_cl;
    struct _Closure_cl		task_cl;
    
    ActivationF_clp		actf;
    Events_clp			events; /* for use in notify    */
    VP_clp			vp;
    
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
       FIFO.  Bindings being serviced by threads are on the "active" queue. */
    bindlink_t	     	pending;
    bindlink_t	     	idle;
    bindlink_t	     	active;
};

/* Structure containing what the entry knows about the binding. */
defstruct (binding_t) {
    bindlink_t		       link;	/* Must be first in structure 	*/
    Channel_Endpoint	       ep;	/* Channel end-point		*/
    IDCServerStubs_clp	       stubs;	/* Stub entry points		*/
    entry_st		      *entry;	/* Entry state			*/
    Entry_State		       state;	/* Current state		*/
    struct _ChannelNotify_cl   cn_cl;	/* Closure for notification	*/
    ChannelNotify_clp          next_cn; /* Next handler in chain        */
    ChannelNotify_clp          prev_cn; /* Handler that calls us        */
};


#define DOM(st) (VP$DomainID (st->vp))

/*
 * Statics
 */
static binding_t *FindBinding (entry_st *st, Channel_Endpoint ep);


/*
 * User Level Critical sections. See Thread Scheduler.
 *
 * ENTER/LEAVE sections are not nestable in this entry.
 *
 */

static INLINE void ENTRY_Lock (entry_st *st)
{
    VP$ActivationsOff (st->vp);
}

static INLINE void ENTRY_Unlock (entry_st *st)
{
    VP$ActivationsOn (st->vp);
    if (VP$EventsPending (st->vp))
	VP$RFA (st->vp);
}


/*--------Build-an-entry--------------------------------------------*/

static Entry_clp New_m(EntryMod_clp	self,
		       ActivationF_clp  actf,
		       VP_clp		vp,
		       uint32_t		nch )
{
    entry_st *st;
    word_t    size = sizeof(*st);
    
    /* Allocate structure */
    if ( !(st = Heap$Malloc(Pvs(heap), size )) ) {
	DB(fprintf(stderr, "EntryMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }
    
    TRY {
	CL_INIT (st->entry_cl,  &e_op,  st);
	CL_INIT (st->task_cl,   &cl_op, st);
	
	st->actf   = actf;
	st->events = Pvs(evs);
	st->vp     = vp;
	
	st->events_in  = EC_NEW();
	st->events_out = 0;
	
	st->num_eps = 0;
	st->max_eps = nch;
	
	LINK_INIT( &st->pending );
	LINK_INIT( &st->idle );
	LINK_INIT( &st->active );
    } CATCH_Events$NoResources() {
	FREE(st);
	RERAISE;
    } 
    ENDTRY;
    
    return &st->entry_cl;
}

/*--------Entry-operations------------------------------------------*/

/*
 * Bind a channel end-point to an interface stub
 */
static void Bind_m(Entry_clp 	self, 
		   Channel_Endpoint 	chan,
		   IDCServerStubs_clp 	dispatch )
{
    entry_st *st = self->st;
    binding_t *b = Heap$Malloc(Pvs(heap), sizeof(*b));

    TRC(fprintf(stderr, "Entry$Bind: chan=%x dispatch=%x\n", chan, dispatch));

    /* Check we've got a binding to play with */
    if (!b) {
	DB(fprintf(stderr, "Entry$Bind: out of memory.\n"));
	RAISE_Heap$NoMemory();
    }

    /* Optimistically initialise the binding */
    b->ep    = chan;
    b->stubs = dispatch;
    b->state = Entry_State_Idle;
    b->entry = st;
    LINK_INIT(&b->link);
    CL_INIT( b->cn_cl, &en_op, b );

    /* Then dive into a critical section to install the entry in the
       scheduler */
    TRY	{
	USING (ENTRY, st,
	       {
		   if ( st->num_eps >= st->max_eps ) {
		       DB(fprintf(stderr, "Entry$Bind: Too many bindings!\n"));
		       RAISE_Entry$TooManyChannels();
		   }
		   
		   /* Notify the Thread scheduler we're interested */
		   b->next_cn = 
		       ActivationF$Attach(st->actf, &b->cn_cl, chan);
		   b->prev_cn = NULL;
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
    } CATCH_ALL	{
	FREE(b);
	RERAISE;
    }
    ENDTRY;
}

/*
 * Remove a binding of a channel end-point to an interface stub
 */
static void Unbind_m(Entry_clp 		self, 
		     Channel_Endpoint 	chan)
{
    entry_st *st = self->st;
    binding_t * NOCLOBBER b = NULL;

    USING (ENTRY, st, 
	   {
	       /* Check the channel exists */
	       if ( !( b = FindBinding (st, chan)) ) {
		   /* XXX SMH: ignore this since we sometimes called twice
		      to ensure that binding is detached. */
		   DB(fprintf(stderr, "Entry$Unbind: channel not found.\n"));
	       } else {
		   
		   /* Remove it */
		   LINK_REMOVE (&b->link);
		   
		   if(b->prev_cn) {
		       ChainedHandler$SetLink((ChainedHandler_clp)b->prev_cn,
					      ChainedHandler_Position_After, 
					      (ChainedHandler_clp)b->next_cn);
		   } else {
		       
		       /* If we were the head of the notify queue, attach the
			  previous handler to the endpoint (may be NULL)*/
		       ActivationF$Attach(st->actf,
					  b->next_cn, chan);
		   }
		   
		   if(b->next_cn) {
		       ChainedHandler$SetLink((ChainedHandler_clp)b->next_cn,
					      ChainedHandler_Position_Before,
					      (ChainedHandler_clp)b->prev_cn);
		   }
		   
		   /* Reduce count */
		   st->num_eps--;
	       }
	   });
    
    if (b) FREE (b);
}

/*
 * Wait for an invocation and then execute it.
 * "to" is the absolute time to time out.
 */
static bool_t Rendezvous_m(Entry_clp 	self, 
			   Time_ns	to )
{
    entry_st   *st   = self->st;
    Event_Val	out;
    Event_Val	new_in;
    Event_Val   NOCLOBBER	in;
    binding_t * NOCLOBBER	todo = NULL;

    TRC(fprintf(stderr, "Entry$Rendezvous(dom=%lx): to=%lx\n", DOM(st), to));


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

	    USING (ENTRY, st, 
		   {
		       out = st->events_out;
		       in  = EC_READ(st->events_in);
	
		       if (in > out)
		       {
			   /* Remove something from the queue */
			   todo = (binding_t *) (st->pending.next);
			   LINK_REMOVE (&todo->link);

			   /* We don't put dead things on the active queue */
			   if (todo->state != Entry_State_Dead) {
			       todo->state = Entry_State_Active;
			       LINK_ADD_TO_TAIL (&st->active, &todo->link);
			   } else {
			       LINK_ADD_TO_TAIL (&st->idle, &todo->link);
			   }
			   TRC(fprintf(stderr, "Entry$Rendezvous(dom=%lx): "
				       "work todo=%x\n", DOM(st), todo));

			   /* Bump outward count */
			   st->events_out++;
		       }
		   });
	}
	/* If we have an end-point, break out and run with it */
	if (todo) break;
    
	/* Wait until a new event has come in to the entry. */
	TRC(fprintf(stderr, 
		    "Entry$Rendezvous(dom=%lx): await event %x in=%x\n",
		    DOM(st), st->events_in, in));
	new_in = EC_AWAIT_UNTIL(st->events_in, in + 1, to);
    
	/* If nothing new has arrived, we've timed out so just return */
	if (new_in == in) return False;
    }
  
    /* By this stage we have a binding_t called "todo" in our hands,
       whose endpoint is either Connected or Dead.  Since we've just
       pulled it off the pending queue under a mutex, we know we're the
       only task executing it.  Thus, we can call the IDCServerStubs
       closure and call it.  The only fly in the ointment might be if
       the endpoint has been closed down, in which case we shut down our
       own side and remove it from the entry.  This is taken care of by
       the stub simply raising an exception.  If any exception gets down
       here we close the binding down.  This enables a server to close
       down a binding by raising Thread.Alerted, for example.  */

    /* XXX - It's unfortunate that this arrangement means that we have
       an extra TRY on the critical path of an IDC call.  When TRYs are
       free in the normal case, this won't matter. */

    if(todo->state == Entry_State_Dead) {
	IDCServerStubs_State *sst = (IDCServerStubs_State *)todo->stubs->st;

	/* Destroy the binding */
	Unbind_m(self, todo->ep);
	IDCServerBinding$Destroy(sst->binding); 

    } else {

	Event_Val	NOCLOBBER v1, v2;
	
	do {
	    v1 = VP$Poll (st->vp, todo->ep);
	    
	    IDCServerStubs$Dispatch (todo->stubs);
	    
	    /* now the stubs have seen at least v1 */
	    
	    USING (ENTRY, st, 
		   {
		       
		       v2 = VP$Ack (st->vp, todo->ep, v1);
		       /* ASSERT todo->ep.ack == v1 */
		       
		       if (v2 == v1) {
			   /* next event will cause Notify /\ stubs uptodate */
			   LINK_REMOVE (&todo->link);
			   todo->state = Entry_State_Idle;
			   LINK_ADD_TO_TAIL (&st->idle, &todo->link);
			   /* can't just "break": must finish USING :-( */
		       }
		   });
	} while (v1 != v2);
    }

    return True;  
}

/*
 * Return a thread closure suitable for forking threads on this entry.
 */
static Closure_clp Closure_m(Entry_clp 	self )
{
    entry_st *st = self->st;
    return &(st->task_cl);
}

/*
 * Close down this entry
 */
static void Close_m(Entry_clp 	self )
{
    entry_st *st = self->st;

    /* Check that we can go away */
    USING (ENTRY, st,
	   {
	       if (st->num_eps) {
		   DB(fprintf(stderr, "Entry$Close: "
			      "entry has endpoints outstanding.\n"));
		   RAISE_Entry$TooManyChannels();
	       }

	       if (!Q_EMPTY (&st->pending) || !Q_EMPTY (&st->idle) ||
		   !Q_EMPTY (&st->active))
	       {
		   DB(fprintf(stderr, "Entry$Close: "
			      "entry has bindings outstanding.\n"));
		   RAISE_Entry$Inconsistent();
	       }
	   });

    /* We now assume we're the only thread in this entry, otherwise the */
    /* client has made a mistake */
    EC_FREE(st->events_in);
    FREE (st);
}


static void SetLink_m(ChainedHandler_clp self,
		      ChainedHandler_Position pos,
		      ChainedHandler_clp link) {

    binding_t  *b  = self->st;
    
    if(pos == ChainedHandler_Position_Before) {
	b->prev_cn = (ChannelNotify_clp)link;
    } else {
	b->next_cn = (ChannelNotify_clp)link;
    }
}

/*--------ChannelNotify-operations----------------------------------------*/

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
    binding_t *b = self->st;

    TRC (eprintf("Entry$Notify(dom=%x): ch=%lx...", DOM(b->entry), chan));

    if(state == Channel_State_Dead) {
	/* Binding has been closed - put the binding on the queue, and
	   it will raise an exception in the entry 

	   XXX PBM Must think about race conditions, and problems caused
	   by removing binding from active queue, if it is currently
	   being serviced */
      
	entry_st *st = b->entry; 
	
	/* Move the binding on to the pending queue and kick the event
	   count */
	LINK_REMOVE (&b->link);
	b->state = Entry_State_Dead;
	LINK_ADD_TO_TAIL (&st->pending, &b->link);
	Events$Advance (st->events, st->events_in, 1);
	TRC (eprintf("kick: ec=%lx val=%lx\n",
		     st->events_in, Events$Read(st->events, st->events_in)));

	if(b->next_cn) {
	    ChannelNotify$Notify(b->next_cn, chan, type, val, state);
	}
    } else if (b->state == Entry_State_Idle)  {
	entry_st *st = b->entry; 
      
	/* Move the binding on to the pending queue and kick the event
	   count */
	LINK_REMOVE (&b->link);
	b->state = Entry_State_Pending;
	LINK_ADD_TO_TAIL (&st->pending,&b->link);
	Events$Advance (st->events, st->events_in, 1);
	TRC (eprintf("kick: ec=%lx val=%lx\n",
		     st->events_in, Events$Read(st->events, st->events_in)));
    } else if (b->state == Entry_State_Pending) {
	/* Ignore */
    } else
    {
	DB(fprintf(stderr, "ChannelNotify$Notify: Bogus channel %x\n", chan));
    }
}

/*--------Closure operation------------------------------------------*/

/*
 * This is the apply operation on the closure returned by Closure_m
 * above. Simply run around in a loop until alerted. 
 */
static void Apply_m(Closure_clp self)
{
    Entry_clp entry = self->st;
    NOCLOBBER bool_t alive;

    alive = True;

    while (alive) {
	TRY { 
	    TRC(fprintf(stderr, "Entry$Apply: enter Rendezvous loop.\n"));
	    while (True)
		(void) Entry$Rendezvous (entry, FOREVER);
	} CATCH_Thread$Alerted() {
	    TRC(fprintf(stderr, "Entry$Apply: alerted; exiting.\n"));
	    alive = False;
	} CATCH_Channel$Invalid(UNUSED ep) {
	    TRC(fprintf(stderr, "Entry$Apply: CAUGHT Channel$Invalid(%x).\n", 
			ep));
	} CATCH_Channel$BadState(ep, UNUSED st) {
	    TRC(eprintf("Entry$Apply: CAUGHT Channel$BadState(%x,%x).\n", 
			ep,st));
	    /* XXX SMH: We *should* have already unbound this, but ensure */
	    Entry$Unbind(entry, ep);
	}
	ENDTRY;
    }
}

/*
 * Given a channel end-point return the IDCServerStubs interface
 * associated with it.  Return NULL if no binding exists.
 */
static binding_t *FindBinding(entry_st	*st,
			      Channel_Endpoint ep )
{
    bindlink_t *b;
  
    for (b = st->pending.next; b != &st->pending; b = b->next)
	if ( ((binding_t *)b)->ep == ep )
	    return (binding_t *)b;

    for (b = st->idle.next; b != &st->idle; b = b->next)
	if ( ((binding_t *)b)->ep == ep )
	    return (binding_t *)b;

    for (b = st->active.next; b != &st->active; b = b->next)
	if ( ((binding_t *)b)->ep == ep )
	    return (binding_t *)b;

    return (binding_t *)0;
}

