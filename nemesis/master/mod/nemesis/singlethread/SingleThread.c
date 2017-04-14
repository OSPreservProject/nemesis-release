/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      lib/nemesis/Threads/Threads.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Experimental ANSA-style task scheduler, modification of Threads.c
**	Multiplexes threads over a Nemesis virtual processor. Haha. XXX
** 
** ENVIRONMENT: 
**
**      User-land;
** 
** ID : $Id: SingleThread.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <salloc.h>

#include <VPMacros.h>

#include <ThreadsPackage.ih>
#include <ThreadF.ih>
#include <ExnSystem.ih>
#include <HeapMod.ih>
#include <Tasks.ih>
#include <EntryNotify.ih>
#include <ThreadBuilder.ih>
#include <DomainMgr.ih>


/*
 * Source configuration
 */


#undef THREAD_TRACE

#ifdef THREAD_TRACE
#define TRC(_x) _x
#define DEBUG
#else
#define TRC(_x)
#endif

/* #define THREAD_TRACE_ATTACH */
#ifdef THREAD_TRACE_ATTACH
#define TRCA(_x) _x
#else
#define TRCA(_x)
#endif

/* #define THREAD_TRACE_MORE */
#ifdef THREAD_TRACE_MORE
#define TRC2(_x) _x
#else
#define TRC2(_x)
#endif

#define DEBUG

#ifdef DEBUG
#define DB(_x) _x
#define NOTREACHED						\
{								\
  fprintf(stderr, "Threads_%s: shouldn't be here!\n", __FUNCTION__);	\
}
#else
#define DB(_x) 
#define NOTREACHED
#endif

/*#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}*/
#define ASSERT(cond,fail)




/*
 * Module Stuff
 */

static Event_New_fn	 	 NewEvent_m;
static Event_Free_fn	 	 FreeEvent_m;
static Event_Read_fn	 	 ReadEvent_m;
static Event_Advance_fn	 	 AdvanceEvent_m;
static Event_Await_fn	 	 AwaitEvent_m;
static Event_AwaitUntil_fn 	 AwaitEventUntil_m;
static Event_NewSeq_fn	 	 NewSeq_m;
static Event_FreeSeq_fn	 	 FreeSeq_m;
static Event_ReadSeq_fn	 	 ReadSeq_m;
static Event_Ticket_fn	 	 Ticket_m;        
static Event_Attach_fn	 	 Attach_m;        
static Event_AttachPair_fn	 AttachPair_m;        
static Event_AllocChannel_fn	 AllocChannel_m;        
static Event_FreeChannel_fn	 FreeChannel_m;        

static Event_op Event_ms = {
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
  AllocChannel_m,
  FreeChannel_m
};

static Threads_Fork_fn		 Fork_m;
static Threads_Yield_fn		 Yield_m;
static Threads_Exit_fn		 Exit_m;

static ThreadF_RegisterHooks_fn  RegisterHooks_m;
static ThreadF_SetDumpOnBlock_fn SetDumpOnBlock_m;
static ThreadF_SetPreemption_fn  SetPreemption_m;
static ThreadF_Dump_fn		 Dump_m;

static Tasks_AttachEntry_fn	 AttachEntry_m;
static Tasks_DetachEntry_fn	 DetachEntry_m;

static Tasks_op Tasks_ms = 
{ 
    Fork_m, Yield_m, Exit_m,
    RegisterHooks_m,
    SetDumpOnBlock_m, SetPreemption_m,
    Dump_m,
    AttachEntry_m, DetachEntry_m 
};

static Thread_Alert_fn		 Alert_m;
static Thread_GetStackInfo_fn	 GetStackInfo_m; 
static Thread_op  Thread_ms  = { Alert_m, GetStackInfo_m };

static Activation_Go_fn          ActivationGo_m;
static Activation_op Activation_ms = { ActivationGo_m };

/*
 * Module closure
 */
static ThreadsPackage_New_fn	New_m;
static ThreadsPackage_op	tp_ms = { New_m };
static const ThreadsPackage_cl	tp_cl = { &tp_ms, NULL };

CL_EXPORT (ThreadsPackage, SingleThread, tp_cl);

/*--------------------------------------------------------------------------*/


/*
 * Data structures
 */

typedef void (*entry_t)(addr_t data);

/* Attachments are basically EntryNotify closures, except that they
   are optimised such that only the address and first argument of the
   Notify operation are stored.  This argument is the closure
   pointer. */

/* Forward declaration */
typedef struct _SingleThread_st SingleThread_st;

/* event counts and sequencers */
typedef struct _simple_event simple_event_t;
struct _simple_event {
    Event_Val	  val;		/* current value 			*/
    Channel_EP	  ep;		/* attached endpoint, unless NULL_EP	*/
    Channel_EPType  ep_type;
    SingleThread_st *st;
};

typedef Event_Val sequencer_t;


/* threads package state */

struct _SingleThread_st {
  struct _Tasks_cl 	cl;		/* Main closure			*/
  struct _Activation_cl actcl;		/* Activation handler closure	*/
  struct _Event_cl      evcl;		/* Event closure		*/
  Heap_clp	        heap;		/* for allocating queues, etc. 	*/
  uint32_t		defaultStackBytes;
  VP_clp		vp;		/* VP we are running over	*/
  ExnSystem_clp		exns;
  bool_t		block_dump;	/* Dump when blocking?		*/
  Event_Count		null_ec;	/* the null ec			*/
  Stretch_clp		u_stretch;	/* for user data 		*/
  bool_t                started;        /* Whether the thread has started */
  Pervasives_Rec        pvs;
  VP_ContextSlot        context;
  jmp_buf_t             *jb;
  Thread_cl             thd_cl;
    
};

static void DomainEnd(SingleThread_st *st);

/*
 * Macros
 */


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


/*--------------------------------------------------------------------------*/


/*-------------------------------------------------- Event Entry Points ---*/

/* XXX PRB All of the Malloc/Frees on the UNLOCKED st->heap should be
   within ENTER/LEAVE critical sections! */

static Event_Count NewEvent_m (Event_cl *self)
{

    SingleThread_st *st = self->st;

    simple_event_t *ec;

    ec = (simple_event_t *) Heap$Malloc (st->heap, sizeof (*ec));
    if (!ec) RAISE_Event$NoSlots ();

    ec->ep = NULL_EP;
    ec->val = 0;
    return (Event_Count) ec;

}

static void
FreeEvent_m (Event_cl *self, Event_Count ec)
{

    simple_event_t *ect = (simple_event_t *)ec;

    if(ect->ep != NULL_EP) Binder$Close(Pvs(bndr), ect->ep);

    FREE(ect);

}

/*
 * ReadEvent must check any attached channel, since otherwise updates
 * are only propagated from the channel to the count on an activation,
 * and an activation will not occur unless someone is blocked on this
 * event count. This means that ReadEvent can raise the same
 * exceptions as VP_Poll, principally Channel.BadState. 
 */
static Event_Val
ReadEvent_m (Event_cl *self, Event_Count ec)
{
    simple_event_t *ect = (simple_event_t *) ec;
    SingleThread_st *st = (SingleThread_st *)self->st;

    /* Check any attached event count */
    if ( ect->ep != NULL_EP && ect->ep_type == Channel_EPType_RX ) {
	ect->val = VP$Poll(st->vp, ect->ep);
    }
    return ect->val;
}

/*
 * AdvanceEvent preserves VP$ActivationMode(vpp).  Thus, it can be
 * called within an activations-off critical section.
 */
static void
AdvanceEvent_m (Event_cl *self, Event_Count ec, Event_Val inc)
{
    
    simple_event_t *ect = (simple_event_t *)ec;
    SingleThread_st *st = (SingleThread_st *)self->st;
    
    ect->val += inc;
    
    /* if "ec" is attached to a TX endpoint, send the new value */
    if (ect->ep != NULL_EP && ect->ep_type == Channel_EPType_TX)
    {
	TRC (fprintf(stderr, 
		     "Event$Advance: send ep=%x val=%x\n", ect->ep, ect->val));
	VP$Send(st->vp, ect->ep, ect->val);
    }
    
}

static Event_Val AwaitEvent_m(Event_clp self, Event_Count ec, Event_Val val)
{
    return AwaitEventUntil_m(self, ec, val, FOREVER);
}

static Event_Val
AwaitEventUntil_m (Event_cl *self, Event_Count ec, Event_Val val, Time_T until)
{

    SingleThread_st *st = (SingleThread_st *)self->st;

    VP_clp         vp  = st->vp;
    simple_event_t *ect  = (simple_event_t *) ((ec == NULL_EVENT) ?
					       st->null_ec : ec);
    Channel_State state;
    Channel_EPType eptype;
    Event_Val rxval;
    Event_Val rxack;

    while((EC_LT(ect->val, val)) && (NOW() < until)) {
	
	if(ect->ep != NULL_EP && ect->ep_type == Channel_EPType_RX) {
	    
	    state = VP$QueryChannel(vp, ect->ep, &eptype, 
				    &rxval, &rxack);
	    
	    if(state == Channel_State_Dead) {
		RAISE_Thread$Alerted();
	    }
	    
	    ect->val = rxval;
	    VP$Ack(vp, ect->ep, ect->val);
	    
	    if(EC_LT(ect->val, val)) {
		
		Event_Val ev_val;
		Channel_EP ev_ep;
		Channel_EPType ev_type;
		Channel_State ev_state;

		VP$Block(vp, until);

		while(VP$NextEvent(vp, &ev_ep, &ev_type, &ev_val, &ev_state))
		{
		    /* Ignore all events */
		};
	    }
	}
    }

    return ect->val;
}	      


/*---------------Sequencer-Operations------------------------------------*/

/*
 * Create a new sequencer
 */
static Event_Sequencer
NewSeq_m (Event_cl *self)
{
  SingleThread_st *st = self->st;
  sequencer_t *seq = Heap$Malloc (st->heap, sizeof (*seq));

  if (!seq) RAISE_Event$NoSlots();

  /* Initialise it */
  *seq = 0;
  return (Event_Sequencer) seq;
}

/*
 * Destroy a sequencer 
 */
static void
FreeSeq_m (Event_cl *self, Event_Sequencer sq)
{
  FREE ((sequencer_t *) sq);
}

/*
 * Read a sequencer
 */
static Event_Val
ReadSeq_m (Event_cl *self, Event_Sequencer sq)
{
  sequencer_t  *sqt = (sequencer_t *) sq;
  return *sqt;
}

/*
 * Ticket a sequencer
 */
static Event_Val
Ticket_m (Event_cl *self, Event_Sequencer sq)
{
    
    SingleThread_st *st = (SingleThread_st *)self->st;
    VP_clp	vp = st->vp;
    sequencer_t  *sqt = (sequencer_t *) sq;
    Event_Val	res;
    
    res = *sqt;
    (*sqt)++;
    
    return res;
}

/* 
 * Events and Channels
 */

static void 
Attach_m (Event_cl *self, Event_Count event, Channel_EP	chan,
	  Channel_EPType type)
{

    simple_event_t *ev = (simple_event_t *)event;
    
    ev->ep = chan;
    ev->ep_type = type;

}

static void 
AttachPair_m (Event_cl		*self,
	      const Event_Pair	*events	/* IN */,
	const Channel_Pair	*chans	/* IN */ )
{

    simple_event_t *ev;

    ev = (simple_event_t *)events->tx;
    ev->ep = chans->tx;
    ev->ep_type = Channel_EPType_TX;

    ev = (simple_event_t *)events->rx;
    ev->ep = chans->rx;
    ev->ep_type = Channel_EPType_RX;

}

static Channel_EP
AllocChannel_m (Event_cl *self)
{
    
    SingleThread_st *st = (SingleThread_st *)self->st;
    return VP$AllocChannel (st->vp);

}

static void 
FreeChannel_m (Event_cl	*self, Channel_EP ep)
{
    
    SingleThread_st *st = (SingleThread_st *)self->st;
    VP$FreeChannel (st->vp, ep);
}



/*------------------------------------------------ Threads Entry Points ---*/

static Thread_clp
Fork_m (Threads_cl *self, addr_t entry, addr_t data, uint32_t stackBytes)
{

    RAISE_Threads$NoResources();
    return NULL;

}

/*
 * This thread yields.
 *
 */

static void
Yield_m (Threads_cl *self)
{
    return;
}



static void
Exit_m (Threads_cl *self)
{
    SingleThread_st *st = self->st;

    printf("Thread exited\n");

    DomainEnd(st);

}

/*------------------------------------------------ ThreadF Entry Points ---*/


static void
RegisterHooks_m (ThreadF_clp self, ThreadHooks_clp h)
{

}

static void
SetDumpOnBlock_m (ThreadF_clp self, bool_t dump)
{
}

static bool_t
SetPreemption_m (ThreadF_clp self, bool_t on)
{
    return False;
}

static void
Dump_m (ThreadF_clp self)
{

}

/*------------------------------------------------ Tasks Entry Points ---*/

/*
 * Attach an entry to a channel.
 * There is an argument here for fixing the following race condition:
 *
 *  1) Channel does not appear to have pending events.
 *  2) Events arrive on channel.
 *  3) Attach is performed without notification arriving at the entry.
 *
 * However, the intention is that AttachEntry is called by the Entry
 * itself as a result of a Bind operation. Hence, it's much easier for
 * the Entry to resolve the race after the Attach, since it can then
 * examine its own FIFO to prevent duplication.
 */
static EntryNotify_clp
AttachEntry_m (Tasks_clp self, EntryNotify_clp e, Channel_RX c)
{

    RAISE_Threads$NoResources();
}

/*
 * Removing an entry from an event channel end-point is, likewise,
 * assumed to be done by the entry concerned.
 */
static void
DetachEntry_m (Tasks_clp self, Channel_RX c)
{
    RAISE_Threads$NoResources();
}




/*------------------------------------------------- Thread Entry Points ---*/


static void
Alert_m (Thread_cl *self)
{
}

/* The GetStackInfo thread operation is included so that garbage collectors
 * can scan the stacks of threads to find otherwise unreferenced objects.
 * This is a bit grungy, but I can't think of a better place to put it
 * at the moment.
 */
static void *
GetStackInfo_m (Thread_cl *self, void **stackTop)
{

    /* Not sure what ought to happen here - blow up for now XXX */
    return NULL;
}


/*
 * Activation handler
 */
static void
ActivationGo_m (Activation_cl *self, VP_clp vpp, Activation_Reason ar)
{
  SingleThread_st *st      =  self->st;

  if(st->started) {
      
      eprintf("Got activated\n");
      
      ntsc_dbgstop();
  } else {
      
      st->started = True;

      /* switch to main thread */

      _longjmp((jmp_buf *)st->jb);
  }

}

static void
ThreadTop (SingleThread_st	*st,
	   entry_t	 entry,
	   addr_t	 data)
{
  VP_cl		*vpp   = st->vp;

  PVS() = &(st->pvs);

  st->null_ec = EC_NEW();

  TRC(fprintf(stderr, "\n\nThreads_ThreadTop: vpp %x: entered.\n", vpp));

  TRY {

      entry (data);
      
  } CATCH_Context$NotFound(name) {
      printf("Threads$ThreadTop: vpp %x: "
	     "UNHANDLED EXCEPTION Context$NotFound(\"%s\")\n", vpp, name);
  } CATCH_ALL {
      printf ("Threads$ThreadTop: vpp %x: UNHANDLED EXCEPTION %s\n",
	      vpp, exn_ctx.cur_exception);
  } ENDTRY;
  
  /* We have now finished - just die */

  DomainEnd(st);

}

static void DomainEnd(SingleThread_st *st) {
    
    /* Recover our binder binding */

    TRY {
	IDCClientBinding_clp binder_binding = 
	    NAME_FIND("sys>Binder",	IDCClientBinding_clp);
	
	IDCClientBinding$Destroy(binder_binding);

    } CATCH_ALL {
	
	/* We are unable to close our binder connection for some reason. Try a different tack */

	TRY {

	    DomainMgr_clp dm = IDC_OPEN("sys>DomainMgr", DomainMgr_clp);

	    DomainMgr$Destroy(dm, VP$DomainID(st->vp));
	} CATCH_ALL {

	    /* Oo-er */

	} ENDTRY;

    } ENDTRY;
    
    /* OK, so we're immortal ... */

    VP$ActivationsOff(st->vp);
    
    while(1) {
	VP$Block(st->vp, FOREVER);
    }

}

/*-------------------------------------------- ThreadsPackage New fn ------*/


static bool_t
New_m (ThreadsPackage_cl       *self,
       addr_t			entry,
       addr_t			data,
 const ThreadsPackage_Stack    *protoStack,
       Stretch_clp		userStretch,
       uint32_t                 defStackBytes,
       bool_t			preemptive,
 const Pervasives_Init	       *prec           )
{
    SingleThread_st	*st;
    VP_clp	 vp = prec->vp;
    bool_t	 mode;
    uint32_t	 nch;
    word_t 	 i;
    jmp_buf_t   *jb;
    void *stack;
    Stretch_Size size;
    
    /* activations off here; running on dcb_rw activation stack */
    
    TRC (fprintf(stderr, "Threads$New: prec=%x stackBytes=%x entry=%x data=%x\n",
		 prec, defStackBytes, entry, data));
    
    st = (SingleThread_st *) Heap$Malloc (prec->heap, sizeof (*st));
    if (!st) {
	DB(fprintf(stderr, "ThreadMod_New: malloc failed.\n"));
	return False;
    }
    
    stack = STR_RANGE(protoStack->stretch, &size);
    if(!stack) {
	DB(fprintf(stderr, "ThreadMod_New: bad stack.\n"));
	FREE(st);
	return False;
    }

    TRC (fprintf(stderr, "Threads$New: init st\n"));
    
    CL_INIT (st->cl,    &Tasks_ms, st);
    CL_INIT (st->actcl, &Activation_ms, st);
    CL_INIT (st->evcl,  &Event_ms,      st);
    CL_INIT (st->thd_cl,&Thread_ms,     st);

    st->heap              = prec->heap;
    st->defaultStackBytes = defStackBytes;
    st->u_stretch		= userStretch;
    st->vp		= vp;
    st->exns		= NAME_FIND("modules>ExnSystem", ExnSystem_clp);
    st->started           = False;

    st->context = VP$AllocContext(vp);
    jb = (jmp_buf_t *) VP$Context (vp, st->context);
    
    
    {
	ThreadBuilder_clp tb = 
	    NAME_FIND("modules>ThreadBuilder", ThreadBuilder_clp);
	if(!ThreadBuilder$InitThread(tb, protoStack->stretch, ThreadTop,
				     st, entry, data, jb)) {
	    TRC(fprintf(stderr, "Threads$CreateThread: " 
			"ThreadBuilder$InitThread failed\n"));
	    VP$FreeContext(vp, st->context);
	    FREE(st);
	    return False;
	}
    }
    
    st->jb = jb;
    
    /* Set up pervasives */

    memset(&st->pvs, 0xd0, sizeof(st->pvs));

    st->pvs.vp = prec->vp;
    st->pvs.heap = prec->heap;
    st->pvs.types = prec->types;
    st->pvs.root  = prec->root;
    st->pvs.time  = NAME_LOOKUP("modules>Time", prec->root, Time_clp);
    st->pvs.libc  = Pvs(libc); /* XXX should prob get this from context  */
    st->pvs.evs   = &st->evcl;
    st->pvs.exns = (ExnSupport_clp) ExnSystem$New (st->exns);
    st->pvs.thd   = &st->thd_cl;
    st->pvs.thds  = (Threads_clp)(&st->cl);
    st->pvs.srcth = NAME_LOOKUP("modules>SRCThread", 
				     prec->root, SRCThread_clp);
    st->pvs.in      = Pvs(in);
    st->pvs.out     = Pvs(out);
    st->pvs.err     = Pvs(err);
    st->pvs.console = Pvs(console);
   
    /* Install the new activation handler */
    VP$SetActivationVector (vp, &st->actcl);
    
    /* The main thread will be entered via the first activation.  To
       cause that activation, we must yield with activations on.  Thus,
       the save slot must be valid and different from the main thread's
       slot when we yield.  The current resume slot will do. */
    
    VP$SetSaveSlot(vp, VP$GetResumeSlot (vp));
    
    TRC (fprintf(stderr, "Threads$New: success\n"));

    return True;
}

/*
 * End lib/nemesis/Threads/Threads.c
 */
