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
**      mod/nemesis/threads/threads.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Experimental ANSA-style task scheduler, modification of Threads.c
**	Multiplexes threads over a Nemesis virtual processor.
** 
** ENVIRONMENT: 
**
**      User-land;
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <salloc.h>

#include <VPMacros.h>

#include <ThreadsPackage.ih>
#include <ThreadF.ih>
#include <ExnSystem.ih>
#include <HeapMod.ih>
#include <ActivationMod.ih>
#include <EventsMod.ih>
#include <SRCThreadMod.ih>
#include <TypeSystem.ih>
#include <Exception.ih>

#include "stack.h"



#include <autoconf/memsys.h>

/*
 * Source configuration
 */

// #define THREAD_TRACE
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

#ifdef DEBUG
#define DB(_x) _x
#define NOTREACHED						\
{								\
  fprintf(stderr, "Threads_%s: executing NOTREACHED code\n", __FUNCTION__);	\
}
#else
#define DB(_x) 
#define NOTREACHED
#endif




#define PARANOIA
#ifdef PARANOIA
#if 0
#define ASSERT(cond,fail) {						\
    if (!(cond)) { 							\
	eprintf("%s assertion failed in function %s (line 0x%x)\n", 	\
		__FILE__, __FUNCTION__, __LINE__);			\
	{fail}; 							\
	while(1); 							\
    }									\
}
#endif
#define ASSERT(cond,fail) {						\
    if (!(cond)) { 							\
	{fail}; 							\
	while(1); 							\
    }									\
}
#else
#define ASSERT(cond,fail)
#endif


static Threads_Fork_fn		 Fork_m;
static Threads_EnterCS_fn	 EnterCS_m;
static Threads_LeaveCS_fn	 LeaveCS_m;
static Threads_Yield_fn		 Yield_m;
static Threads_Exit_fn		 Exit_m;

static ThreadF_CurrentThread_fn  Current_m;
static ThreadF_BlockThread_fn    BlockThread_m;
static ThreadF_UnblockThread_fn  UnblockThread_m;
static ThreadF_BlockYield_fn     BlockYield_m;
static ThreadF_UnblockYield_fn   UnblockYield_m;
static ThreadF_RegisterHooks_fn  RegisterHooks_m;


static ThreadF_op threads_ms = { 
    Fork_m,              /* standard Threads operations */
    EnterCS_m,
    LeaveCS_m,
    Yield_m, 
    Exit_m,
    Current_m,           /* ThreadF only operations     */
    BlockThread_m, 
    UnblockThread_m,
    BlockYield_m,
    UnblockYield_m,
    RegisterHooks_m, 
};


static Thread_Alert_fn		 Alert_m;
static Thread_GetStackInfo_fn	 GetStackInfo_m; 
static Thread_SetDaemon_fn       SetDaemon_m;
static Thread_op  Thread_ms  = { Alert_m, GetStackInfo_m, SetDaemon_m };

static Activation_Go_fn          ActivationGo_m;
static Activation_op Activation_ms = { ActivationGo_m };



/*
** Module closures: the same set of routines in this file can operate 
** as either a preemptive or a non-preemptive threads package, simply 
** by having slightly different threads package state. 
** Hence we export two different closures and domains which initialise
** threads packages may choose which one they prefer to use. 
*/
static ThreadsPackage_New_fn	New_m;
static ThreadsPackage_op	tp_ms   = { New_m };
static const ThreadsPackage_cl	ptp_cl  = { &tp_ms, (void *)True };
static const ThreadsPackage_cl	nptp_cl = { &tp_ms, (void *)False };

CL_EXPORT (ThreadsPackage, PreemptiveThreads, ptp_cl);
CL_EXPORT (ThreadsPackage, NonPreemptiveThreads, nptp_cl);

/* Make _thead available to other threads packages */

extern void _thead();

const typed_ptr_t THead_export = TYPED_PTR(addr_t__code, &_thead);

/*--------------------------------------------------------------------------*/


/*
 * Data structures
 */

typedef void (*entry_t)(addr_t data);

typedef struct _thread thread_t;
typedef struct _link   link_t;
typedef struct _hooks  hooks_t;

/* links in queues of threads and hooks */

struct _link {
    link_t	*next;
    link_t	*prev;
    thread_t    *thread;
};


struct _hooks {
    hooks_t       *next;
    hooks_t	  *prev;
    ThreadHooks_clp  hook;
};

/* threads' stacks */

typedef struct {
    addr_t	bot;
    word_t	bytes;
    Stretch_clp	stretch;	/* Containing [bot, bot+bytes)		*/
    Stretch_clp	guard;		/* An unreadable, unwritable page	*/
} stack_t;

typedef SEQ_STRUCT(stack_t)	stacks_t;

typedef enum {
    Runable,
    Blocked, 
    Stopped
} state_t;

/* threads themselves */
struct _thread {
    link_t       link;		/* link for runnable and event queues	*/
    VP_ContextSlot context;	/* context slot in VP			*/
    stack_t	 stack;		/* the thread's stack			*/
    bool_t	 yielded;	/* true iff context written by setjmp   */
    bool_t	 alerted;	/* true iff alerted			*/
    state_t      state;         /* running, blocked, etc                */
    int8_t       critical;      /* Count of enterCS's for this thread   */
    int8_t       vpcritical;    /* Marker (leaving VP critical section) */
    struct _Thread_cl cl;	/* Closure to this thread		*/
    Threads_clp	 threads;	/* Package this thread came from 	*/
    addr_t       forkRA;        /* RA of code that forked this thread   */
    bool_t       daemon;        /* true iff thread is not to be counted */
    Pervasives_Rec pvs_rec;	/* Pervasive structures			*/
};


/* Forward declaration */
typedef struct _ThreadMod_st ThreadMod_st;


/* threads package state */

struct _ThreadMod_st {
    ThreadF_cl 	        cl;		/* Main closure			*/
    Activation_cl       actcl;	        /* Activation handler closure	*/
    ActivationF_clp     actf;
    Heap_clp	        heap;		/* for allocating queues, etc. 	*/
    uint32_t		defaultStackBytes;
    VP_clp		vpp;		/* VP we are running over	*/
    ExnSystem_clp	exns;
    bool_t		preemptive;
    VP_ContextSlot      critcx;         /* cx for current critical thd  */
    link_t		run_queue;
    hooks_t		hooks;
    uint32_t		nthreads;
    uint32_t		active_threads; /* non-daemon threads           */
    stacks_t		stacks;		/* free-list of stacks		*/
    Stretch_clp		u_stretch;	/* for user data 		*/
    Pervasives_Rec	tmpl_pvs;	/* Used at start of day		*/
};


/*
** Sometimes we wish to indicate that no context slot has been assigned /
** is in use in a particular case. For this we hava NULL_CX value.  
*/
#define NULL_CX ((VP_ContextSlot)-1)    



/*
 * Macros
 */

#define RUNQUEUE(st)       (&(((ThreadMod_st *)(st))->run_queue))
#define VPP(st)            (((ThreadMod_st *)(st))->vpp)
#define CURRENT_THREAD(st) ((RUNQUEUE (st))->next)
#define TQ_INIT(qp)	  { Q_INIT (qp); (qp)->thread = NULL; }


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
} while (0)



/*--------------------------------------------------------------------------*/


/* 
 * Prototypes
 */
static bool_t	  InternalYield (ThreadMod_st *st, VP_clp vpp, thread_t *from);


NORETURN          (RunNext(ThreadMod_st *st, VP_clp vpp));
static void	  Preempt	(ThreadMod_st *st);
static thread_t *CreateThread	(ThreadMod_st *st, VP_clp vpp, addr_t entry,
				 addr_t data, uint32_t stackBytes,
				 Pervasives_Rec  *parent_pvs,
				 Pervasives_Rec **new_pvs);
static void	  ThreadTop	(thread_t	*th,
				 entry_t	 entry,
				 addr_t	 data);
static bool_t	  AllocStack	(ThreadMod_st *st, stack_t *stack);
static void	  FreeStack	(ThreadMod_st *st, stack_t stack);

#ifdef SWIZZLE_STACK
extern NORETURN(_run_next(addr_t, ThreadMod_st *, VP_cl *));
#endif

/*------------------------------------------------ Threads Entry Points ---*/

static Thread_clp
Fork_m (Threads_cl *self, addr_t entry, addr_t data, uint32_t stackBytes)
{
    ThreadMod_st 	*st  = self->st;
    VP_cl	       	*vpp = VPP(st);
    thread_t		*NOCLOBBER forkee = NULL;
    link_t       	*current;
    Pervasives_Rec	*new_pvs;
    hooks_t      	*hook;
    bool_t               mode;
    addr_t               svra = RA();

    /* Create a new thread; st->nthreads > 1 here */
    TRY {
	forkee = CreateThread (st, vpp, entry, data, 
			       stackBytes, PVS(), &new_pvs);

	/* Save the RA so that we can tell who started the thread when
           debugging */
	forkee->forkRA = svra; 

	/* get per-thread state for configured libraries for forkee thread
	   This should be done with hooks registered by library during
	   init code called early in the main thread.                       */
  
	for (hook = st->hooks.next; hook != &st->hooks; hook = hook->next)
	    if (hook->hook->op->Fork)
		ThreadHooks$Fork (hook->hook, new_pvs);
    } CATCH_ALL {
	forkee = NULL;
    } ENDTRY;

    if(forkee) {
	/* We need to protect modification to the runq since this may 
	   be manipulated by an activation handler (e.g. ThreadF$Block) */
	ENTER(vpp, mode);

	current = CURRENT_THREAD(st);
	ASSERT((current->thread == Pvs(thd)->st), ntsc_dbgstop(); );
	Q_INSERT_AFTER (current, &(forkee->link));

	LEAVE(vpp, mode);

	/* Return closure of thread */
	return &(forkee->cl);
    } else return (Thread_cl *)NULL;
}


/*
** Note: EnterCS/LeaveCS should only be called when running as 
** a thread - i.e. should not be called from activation handlers 
** or from certain parts of the scheduler. 
*/
static void EnterCS_m(Threads_cl *self, bool_t vpcs)
{
    ThreadMod_st  *st  = self->st;
    VP_cl         *vpp = VPP(st);
    thread_t      *t;
    bool_t         mode;


    ENTER(vpp, mode);

    t = (thread_t *)CURRENT_THREAD(st);

    if(!t->critical) /* if first time through: set st->critcx */
	st->critcx = t->context;

    t->critical++;

    if(vpcs) { 

	if(!t->vpcritical) {

	    /* First time here with vpcs set. */

#ifdef CONFIG_MEMSYS_EXPT
	    t->vpcritical = -t->critical; /* negative => in progress */

	    {
		StretchDriver_Result res; 
		thread_t *t =(thread_t *)Pvs(thd)->st;
		volatile addr_t cursp = SP();
	    
		LEAVE(vpp, mode); 
		if(cursp - t->stack.bot >= PAGE_SIZE) {
		    /* try to map the next page to avoid taking page faults
		       whilst activations are off */
		    res = StretchDriver$Map(Pvs(sdriver), t->stack.stretch, 
					    cursp - PAGE_SIZE + 1);
		    if(res != StretchDriver_Result_Success) 
			ntsc_dbgstop();
		}
	    }
#endif
	    /* Mark vpcritical for future reference (so leave works) */
	    t->vpcritical = t->critical;

	    VP$ActivationsOff(vpp);
	    return; 

	} 

#ifdef CONFIG_MEMSYS_EXPT
	if(t->vpcritical < 0) {
	    /* we've been recursively called from Map above: ensure we 
	     have activations off, but thats about it (don't want to 
	     recursively try to map our stack...) */
	    VP$ActivationsOff(vpp);
	    return; 
	} 
#endif

    }

    LEAVE(vpp, mode);
    

    return; 
}


static void LeaveCS_m(Threads_cl *self)
{
    ThreadMod_st  *st  = self->st;
    VP_cl         *vpp = VPP(st);
    thread_t      *t;
    bool_t         mode, leavevpcs = False;

    ENTER(vpp, mode);

    t   = (thread_t *)CURRENT_THREAD(st);

    if(t->vpcritical == t->critical) { 
	t->vpcritical = 0;
	leavevpcs = True; 
    } 

    t->critical--;

    if(t->critical == -t->vpcritical) /* Recursive leave */
	leavevpcs = True; 

    if(!t->critical) { /* Final leave => reset st->critcx */
	st->critcx = NULL_CX;
	if(t->vpcritical != 0) {
	    eprintf("Urk! Bogusness in LeaveCS.\n");
	    ntsc_halt();
	}
    }

    
    LEAVE(vpp, mode);

    if(leavevpcs) { 
	VP$ActivationsOn(vpp);
	if(VP$EventsPending(vpp)) 
	    VP$RFA(vpp);
    }

    return; 
}


/*
** This thread yields.
**
** Remove this thread from the run queue, put it on the tail, and if
** there is now a new thread on the head of the run queue, call the
** internal yield routine to reschedule.
**
** Preserves activation mode.
*/

static void
Yield_m (Threads_cl *self)
{
    ThreadMod_st  *st  = self->st;
    VP_cl         *vpp = VPP(st);
    link_t        *current, *run_queue;
    bool_t         mode, alerted = False;
 
    /* 
    ** XXX SMH: we hand craft the ENTER/LEAVE bits here so that 
    ** we can call this routine whether activations are on or off.    
    */

    if((mode = VP$ActivationMode(vpp)))
	VP$ActivationsOff(vpp);

    run_queue = RUNQUEUE(st);
    current   = CURRENT_THREAD(st);
    ASSERT((current->thread == Pvs(thd)->st), ntsc_dbgstop(); );

    Q_REMOVE (current);
    Q_INSERT_BEFORE (run_queue, current); /* put us back on tail of run q */

    if ( current != CURRENT_THREAD(st) )
	alerted = InternalYield (st, VPP(st), current->thread);
    else if (current->thread->alerted) {
	current->thread->alerted = False;
	alerted = True;
    }

    if(mode) {
	VP$ActivationsOn(vpp);
	if (VP$EventsPending (vpp))
	    VP$RFA (vpp); /* XXX maybe a ActivationF$Reactivate() ? */
	if (alerted)
	    RAISE_Thread$Alerted ();
    }

    return;
}



static void
Exit_m (Threads_cl *self)
{
    ThreadMod_st *st  = self->st;
    VP_cl	 *vpp = VPP(st);
    link_t     	 *current;
    hooks_t      *hook;
    bool_t        mode;

    for (hook = st->hooks.prev; hook != &st->hooks; hook = hook->prev)
	if (hook->hook->op->ExitThread)
	    ThreadHooks$ExitThread (hook->hook);

    ENTER(vpp, mode);

    current = CURRENT_THREAD(st);
    ASSERT((current->thread == Pvs(thd)->st), ntsc_dbgstop(); );
    st->nthreads--;
    if (!current->thread->daemon) st->active_threads--;

    if (st->active_threads == 0)
    {
	IDCClientBinding_clp binder_binding;
	
	TRC(eprintf("Threads$Exit: no more threads\n"));
	LEAVE (vpp, mode);
	
	for (hook = st->hooks.prev; hook != &st->hooks; hook = hook->prev)
	    if (hook->hook->op->ExitDomain) {
		ThreadHooks$ExitDomain (hook->hook);
	    }
	
	/* destroy ourselves */
	TRC(eprintf("Threads$Exit (%qx): last thread exiting => "
		    "destroying domain\n", RO(vpp)->id));
	binder_binding = NAME_FIND("sys>Binder", IDCClientBinding_clp);
	IDCClientBinding$Destroy(binder_binding);

    } else {


	Q_REMOVE (current);
       
	TRC(fprintf(stderr, "Threads$Exit: Freeing Context and Stack\n"));
	FreeStack(st, current->thread->stack); /* does NOT Heap_Free */
	VP$FreeContext (vpp, current->thread->context);

	TRC(fprintf(stderr, "Threads$Exit: Freeing state\n"));
	FREE (current->thread);

#ifdef SWIZZLE_STACK
	_run_next(NEWSP(vpp), st, vpp);
#else
	RunNext(st, vpp);
#endif
	NOTREACHED;
    }
}

/*------------------------------------------------ ThreadF Entry Points ---*/


static void
RegisterHooks_m (ThreadF_clp self, ThreadHooks_clp h)
{
    ThreadMod_st *st  = self->st;
    VP_cl	 *vpp = VPP(st);
    hooks_t	 *hook;
    bool_t        mode;

    ENTER(vpp,mode);
    hook = Heap$Malloc (st->heap, sizeof (*hook));
    LEAVE(vpp,mode);
    if (!hook) RAISE_Heap$NoMemory ();
    
    hook->hook = h;
    
    ENTER(vpp,mode);
    Q_INSERT_BEFORE (&(st->hooks), hook);
    LEAVE(vpp,mode);
}


static Thread_clp Current_m(ThreadF_cl *self)
{
    ThreadMod_st *st = self->st;
    thread_t     *t;

    ASSERT(!VP$ActivationMode(st->vpp), ntsc_dbgstop(););

    t = (thread_t *)CURRENT_THREAD(st);
    return (Thread_cl *)&t->cl;
}

/*
** BlockThread: remove a thread from the runqueue. 
** Must be called from within an activation handler (i.e. *not* from 
** a running thread). 
** It is the responsbility of the caller to 'remember' the thread 
** for future unblocking (if that is what it wants).
** Returns "True" iff the thread being blocked was 
**      (a) 'running' [head of the runqueue], and
**      (b) in a critical section.
*/
static bool_t BlockThread_m(ThreadF_clp self, Thread_clp thd, Time_T until)
{
    ThreadMod_st *st   = self->st;
    thread_t     *t    = (thread_t *)thd->st;
    bool_t        incs = False; 

    ASSERT(!VP$ActivationMode(st->vpp), ntsc_dbgstop(););
    ASSERT((t->state == Runable), ntsc_dbgstop(); );

    /* Check if the thread being blocked is critical */
    if(t->critical) {
	incs = True;
	/* If it's also the running thread, need to zap critcx */
	if(CURRENT_THREAD(st)->thread == t)
	    st->critcx = NULL_CX; 	/* No longer critical */
    }

    /* Ok; just remove it from the queue and mark it blocked. */
    Q_REMOVE(&t->link);
    t->state = Blocked;

    return incs;
}

/*
** UnblockThread: moves a thread back onto the run queue.
** If the thread was critical, we place it after the last
** critical thread currently in the runqueue. 
** 
** Note: since this \emph{must} be called from an activation handler, 
** there is no problem in replacing the head of the runqueue in the 
** situation where there are no other critical threads. 
*/
static void UnblockThread_m(ThreadF_cl *self, Thread_clp thd, bool_t cs)
{
    ThreadMod_st *st  = self->st;
    thread_t     *t   = (thread_t *)thd->st;
    link_t       *rq  = RUNQUEUE (st);
    link_t       *cur; 

    /* assert state == Blocked */
    ASSERT(!VP$ActivationMode(st->vpp), ntsc_dbgstop(););
    ASSERT(((CURRENT_THREAD(st))->thread != t), ntsc_dbgstop(); );
    ASSERT((t->state == Blocked), ntsc_dbgstop(); );

    /* SMH: if the thread we are unblocking was in a critical section, 
       then it either (a) lost the processor voluntarily by calling
       BlockYield and hence will have its context restored by RunNext, 
       or (b) it lost the processor by causing a page fault. 
       In the latter case, we need to try to put it as close to 
       the top of the runqueue as possible. */

    t->state = Runable;

    if(cs) {
	/* SMH: if the thread we are unblocking was in a critical section, 
	   then we try to place it back near the front of the runqueue. */
	for(cur = rq->next; cur != rq; cur = cur->next) {
	    if( !((thread_t *)cur)->critical ) 
		break;
	}

    } else cur = rq; 

    Q_INSERT_BEFORE(cur, &t->link);
    return;
}


/*
** BlockYield(): the current thread is blocked (i.e. removed from the 
** runqueue) and then yields the processor to another thread. 
**
** Hopefully someone out there has 'remembered' this thread and will 
** unblock it at some point. On its return here, it will be informed 
** as to whether it was 'alerted' or not.
*/
static bool_t BlockYield_m(ThreadF_cl *self, Time_T until)
{
    ThreadMod_st *st  = self->st;
    VP_cl 	 *vpp = VPP(st);
    thread_t     *ct;
    bool_t        mode, alerted;
    
    ENTER(vpp, mode);

    /* First block ourselves */
    ct = (thread_t *)CURRENT_THREAD(st);
    Q_REMOVE(&ct->link);
    ct->state = Blocked;


    /* If we were in a critical section, then we're not anymore */
    /* XXX SMH: taken care of in InternalYield I think. */

    /*
    ** And finally call InternalYield - we'll be resumed once 
    **  (a) we're back on the runqueue (someone calls UnblockThread), and 
    **  (b) we get 'scheduled' by RunNext. 
    */
    alerted = InternalYield(st, vpp, ct);

    LEAVE(vpp, mode);
    return alerted;
}

/*
** UnblockYield(): the thread "thd" is unblocked (replaced on the 
** runqueue) and the current thread is blocked. 
** If the thread being unblocked was in a critical section, then 
** it is placed after the last critical thread currently in the 
** runqueue --- since the current thread is yielding anyway, 
** there is no problem in replacing the head of the runqueue in the 
** situation where there are no other critical threads. 
*/
static bool_t UnblockYield_m(ThreadF_cl *self, Thread_cl *thd, bool_t cs)
{
    ThreadMod_st *st  = self->st;
    VP_cl 	 *vpp = VPP(st);
    thread_t     *ct, *bt; 
    link_t       *rq, *cur; 
    bool_t        mode, alerted;

    ENTER(vpp, mode);

    bt  = (thread_t *)thd->st;

    /* assert state == Blocked */
    ASSERT((bt->state == Blocked), ntsc_dbgstop(); );

    bt->state = Runable;

    rq  = RUNQUEUE(st);
    ct  = (thread_t *)rq->next; 

    if(cs) {
	/* SMH: if the thread we are unblocking was in a critical section, 
	   then we try to place it back near the front of the runqueue. */
	for(cur = rq->next; cur != rq; cur = cur->next) {
	    if( !((thread_t *)cur)->critical ) 
		break;
	}
    } else cur = rq; 


    Q_INSERT_BEFORE(cur, &bt->link); /* insert before 'cur' */


    /*
    ** And finally call InternalYield - we'll be resumed once 
    **  (a) we're back on the runqueue (someone calls UnblockThread), and 
    **  (b) we get 'scheduled' by RunNext. 
    */
    alerted = InternalYield(st, vpp, ct);

    LEAVE(vpp, mode);
    return alerted;
}


/*------------------------------------------------- Thread Entry Points ---*/

/*
 * The only thread operation supported once was Alert, which is just
 * the setting of a flag in the thread block.  This must be done in a
 * critical section to prevent alerts from being lost.
 */
static void
Alert_m (Thread_cl *self)
{
    thread_t	*t   = (thread_t *) (self->st);
    VP_cl       *vpp = VPP(t->threads->st);
    bool_t       mode;

    ENTER(vpp,mode);
    t->alerted = True; 
    LEAVE(vpp,mode);
}

/* The GetStackInfo thread operation is included so that garbage collectors
 * can scan the stacks of threads to find otherwise unreferenced objects.
 * This is a bit inelgant, but I can't think of a better place to put it
 * at the moment.
 */
static void *
GetStackInfo_m (Thread_cl *self, void **stackTop, void **stackBot)
{
    thread_t *t   = (thread_t *)(self->st);
    VP_cl    *vpp = VPP(t->threads->st);
    void     *sp;
    bool_t    mode;

    ENTER(vpp,mode);
    {
	*stackBot = t->stack.bot;
	*stackTop = t->stack.bot + t->stack.bytes;
	if(t->yielded) {
	    /* The context slot contains a jmpbuf */
	    sp = JMPBUFSP(VP$Context(vpp, t->context));
	} else {
	    /* The context slot contains a context */
	    sp = CTXTSP(VP$Context(vpp, t->context));
	}
    }
    LEAVE(vpp,mode);
    return sp;
}

static void
SetDaemon_m (Thread_cl *self)
{
    thread_t    *t = (thread_t *)(self->st);
    ThreadMod_st *st = t->threads->st;
    VP_cl       *vpp = VPP(t->threads->st);
    bool_t mode;

    ENTER(vpp,mode);
    if (!t->daemon) {
	t->daemon=True;
	st->active_threads--;
    }
    LEAVE(vpp,mode);
    return;
}

/*
 *  InternalYield: save current context and run next thread, returning
 *  when current thread is resumed.  Return True iff resumed by being
 *  alerted.
 *
 *  pre:  activations off, "from" is the current thread
 *  post: activations off
 */

static bool_t
InternalYield (ThreadMod_st *st, VP_clp vpp, thread_t *from)
{
    bool_t alerted;

    ASSERT ((VP$GetSaveSlot (vpp) == from->context),
	    eprintf("Threads$InternalYield: context botch: %x != %x\n",
		    VP$GetSaveSlot (vpp), from->context); ntsc_dbgstop(); );

    /* XXX - we use a context_t slot as a jmp_buf, on the ASSUMPTION
       that it's big enough.  Context slots that hold the state of
       yielded threads are never saved to or resumed from by loss
       of the processor/activation, because (a) activations are off
       here (=> save/restore from resume slot), and (b) RunNext pays
       attention to the threads' "yielded" flags.

       I can't help feeling there should be a better way.  If thread_t
       included a jmp_buf, that would be fine, but would waste space,
       unless one simply decreed that there were precisely 3 context_t's
       in the dcb, namely the activations-on save cx, the activations-off
       save/load cx and the activation-load cx.  But this complicates
       preempting a running thread on activation quite a bit.

       At one time, setjmp and longjmp could interwork with the kernel
       context save/restore mechanism (ie. jmp_bufs WERE context_ts),
       but doing that's pretty heavyweight too: it saves/restores many
       more registers than necessary.
       */

    if (setjmp((addr_t)VP$Context(vpp, from->context)))
    {
	/* resumed */
	from->yielded = False;

	alerted = from->alerted;
	from->alerted = False;
	return alerted;
    }
    else
    {
	/* 
	** SMH: If we were in a critical section, we come out of it now so 
	** that other threads can run as long as we are yielded. We will
	** restore our critical status later when we resume (in RunNext). 
	** Note: there are other potentially useful semantics here - e.g. 
	** arrange that no other thread may run as long as one thread 
	** it critical. This gives a stronger guarantee of 'criticality' 
	** to the thread, but at the cost of poor performance. It is 
	** also not clear why a thread would wish to remain in a critical
	** section during a yield - typically a yield means IDC and this
	** is concurreny protected seperately. 
	*/
	if(st->critcx != NULL_CX) {
#ifdef PARANOIA
	    if(!from->critical) 
		ntsc_dbgstop();
#endif
	    st->critcx = NULL_CX;
	}

	from->yielded = True;

#ifdef SWIZZLE_STACK
	_run_next(NEWSP(vpp), st, vpp);
#else
	RunNext (st, vpp);
#endif
	NOTREACHED;
    }
    NOTREACHED;
}

/*
 *  Preempt: move head of run queue to end of run queue
 *
 *  pre:  activations off
 *  post: activations off
 */

static void
Preempt (ThreadMod_st *st)
{
    link_t *run_queue = RUNQUEUE(st);
    link_t *current;

    if (Q_EMPTY (run_queue))
	return;

    current = run_queue->next;
    Q_REMOVE (current);
    Q_INSERT_BEFORE (run_queue, current);
}

/*
 *  RunNext: switch to the thread at the head of the run queue,
 *  or enable activations and block domain if none.
 *
 *  pre:  activations off
 *  never returns
 */

#ifndef SWIZZLE_STACK
static 
#endif
void RunNext (ThreadMod_st *st, VP_clp vpp)
{
    link_t         *run_queue = RUNQUEUE(st);
    thread_t       *to;
    jmp_buf        *cx;
    
    TRC (fprintf(stderr, "Threads$RunNext(dom=%lx): run_queue=%x ",
		 VP$DomainID(vpp), run_queue));

    if (Q_EMPTY (run_queue)) {

	if(st->critcx != NULL_CX)
	    /* Run-queue empty, but there is apparently a critical thd */
	    ntsc_dbgstop();

	ActivationF$Reactivate(st->actf);
	ASSERT(False, ntsc_dbgstop(); );

    } else {

	to = run_queue->next->thread;

	TRC (fprintf(stderr, "run thread %x context=%x\n", to, to->context));
	cx = (jmp_buf *)VP$SetSaveSlot (vpp, to->context);

	/* 
	** SMH: if this thread was in a critical section before it 
	** lost the processor, we restore that status now.
	*/
	if(to->critical)
	    st->critcx = to->context; 

	if (to->yielded)
	{
	    TRC (fprintf(stderr, "-- _longjmp to cx %p\n", cx));
	    _longjmp ((addr_t)cx);
	}
	else
	{
	    /* This thread must have lost the processor with activations on.
	       NB. we can't just sc_rfa(); _longjmp (cx), because we
	       might ourselves lose the processor at the semi-colon, thus
	       trampling on the context we're about to restore. */

	    TRC  (fprintf(stderr, "-- RFAResume to cx %p\n", cx));
	    VP$RFAResume (vpp, to->context);
	}
    }

/* NOTREACHED */
    ASSERT(False, ntsc_dbgstop(); );
}


/*
 * Activation handler
 */
static void
ActivationGo_m (Activation_cl *self, VP_clp vpp, Activation_Reason ar)
{
    ThreadMod_st *st      =  self->st;


    /* Activations were on just before calling here, so we weren't in
       a critical section when we last lost the processor.  Activations
       are now off. */

    ASSERT(!VP$ActivationMode(vpp), ntsc_dbgstop(); );

    TRC2 (fprintf(stderr, "ThreadMod: activated: reason %x threads %llx\n",
		  ar, &st->cl));
    TRC2 (fprintf(stderr, "ThreadMod: self %llx, st %llx, vpp %llx\n",
		  self, st, vpp ));
    
    if(st->critcx != NULL_CX) {
#ifdef PARANOIA
	if(((thread_t *)CURRENT_THREAD(st))->context != st->critcx) 
	    ntsc_dbgstop();
#endif
	VP$SetSaveSlot(vpp, st->critcx);
	VP$RFAResume(vpp, st->critcx);
	ASSERT(False, ntsc_dbgstop(); );
    } 
    
    if(st->preemptive) 
	Preempt(st);
    
    RunNext (st, vpp);
    
/* NOTREACHED */
    ASSERT(False, ntsc_dbgstop(); );
}



/*
 *  CreateThread: return a thread_t to run entry(data) under ThreadTop
 *
 *  Assumes stacks grow down.
 */
static thread_t *
CreateThread (ThreadMod_st	*st,
	      VP_cl		*vpp,
	      addr_t		entry,
	      addr_t		data,
	      uint32_t		stackBytes,
	      Pervasives_Rec   *parent_pvs,
	      Pervasives_Rec  **new_pvs)
{
    thread_t  	 *res;
    jmp_buf_t  	 *NOCLOBBER jb = NULL;
    bool_t NOCLOBBER mode; 

    TRC (fprintf(stderr, 
		 "Threads$CreateThread: vpp=%x st->heap=%x\n", vpp, st->heap));

    /* Need to protect our (unlocked) heap from activations */
    ENTER(vpp, mode); 
    res = Heap$Malloc (st->heap, sizeof (*res));
    LEAVE(vpp, mode); 
    if (!res) RAISE_Threads$NoResources();

    TRC (fprintf(stderr, "Threads$CreateThread: res=%x\n", res));

    res->link.next    = NULL;
    res->link.prev    = NULL;
    res->link.thread  = res;

    res->yielded      = True; /* _longjmp first time, not RFAR; => jmp_buf */
    res->alerted      = False;
    res->state        = Runable; /* hopefully */
    res->critical     = 0; 
    res->vpcritical   = 0; 
    res->daemon       = False;
    CL_INIT (res->cl, &Thread_ms, res);
    res->threads      = (Threads_clp)&st->cl;

    /* Now the context, stack and pvs_rec fields of rec are left to do */
    TRY {
	ENTER(vpp, mode); 
	res->context = VP$AllocContext (vpp);
	jb = (jmp_buf_t *) VP$Context (vpp, res->context);
    } CATCH_VP$NoContextSlots() {
	
	TRC (fprintf(stderr, "Threads$CreateThread: no context slots\n"));

	Heap$Free (st->heap, res);
	LEAVE(vpp, mode);
	RAISE_Threads$NoResources();
    } ENDTRY;
    LEAVE(vpp, mode)

    TRC (fprintf(stderr, "Threads$CreateThread: use jb %x=%x\n",
		 res->context, jb));

    /* Initialise the new thread's pvs_rec from parent */
    memcpy (&res->pvs_rec, parent_pvs, sizeof (Pervasives_Rec));
    res->pvs_rec.thd = &res->cl;

    /* The new thread needs its own exception handler stack. */
    TRY {
	res->pvs_rec.exns = (ExnSupport_clp) ExnSystem$New (st->exns);
    } CATCH_Heap$NoMemory() {

	TRC (fprintf(stderr, 
		     "ThreadMod_CreateThread: no exception runtime.\n"));
	ENTER(vpp, mode);
	VP$FreeContext (vpp, res->context);
	Heap$Free      (st->heap, res);
	LEAVE(vpp, mode);
	RAISE_Threads$NoResources();
    }
    ENDTRY;

    /* XXX PRB This requires a Stretch Allocator - for the first thread
       we will need to get a stack using alternative means. SMH had a 
       solution where the builder gives us stretched for the user heap and
       the first stack */

    /* Allocate a stack for the thread */
    res->stack.bytes = stackBytes;
    if (!AllocStack (st, &(res->stack)))
    {
	TRC (fprintf(stderr, "ThreadMod_CreateThread: can't get stack.\n"));
	/* XXX - ExnSupport$Free (res->pvs_rec.exns); */
	
	ENTER(vpp, mode);
	VP$FreeContext (vpp, res->context);
	Heap$Free      (st->heap, res);
	LEAVE(vpp, mode);
	RAISE_Threads$NoResources();
    }    

    TRC (fprintf(stderr, "Threads$CreateThread: stack.bot=%x ->bytes=%x\n",
		 res->stack.bot, res->stack.bytes));
    

    /*
     * The architecture specific _thead routine is entered via
     * _longjmp with a pointer to the jmp_buf in the pervasives
     * register.  The first four words of the jmp_buf contain the
     * entry point of ThreadTop, and its three arguments at the moment,
     * but this is open to change. The assembler stub _thead is reqired
     * to understand the jmp_buf format and to sort out any necessary
     * procedure call standards. e.g. the PV register on Alpha
     */
    {
	word_t stack_len, sp;
	addr_t stack_bot;

	memset (jb, 0, sizeof (context_t));

	stack_bot = STR_RANGE(res->stack.stretch, &stack_len);
	sp = (word_t) 
	    WORD_ALIGN ((word_t) stack_bot + stack_len - 2*WORD_SIZE);

	/* init_jb: mach dep setup; see h/$(MC)/jmp_buf.h & ./thead_$(MC).S */
	init_jb(jb, ThreadTop, _thead, sp, (word_t)jb,
		(word_t)res, (word_t)entry, (word_t)data);
	
    }

    ENTER(vpp, mode);
    st->nthreads++;
    st->active_threads++;
    LEAVE(vpp, mode);

    *new_pvs = &(res->pvs_rec);
    return res;
}


/* 
 *  ThreadTop: main function in a thread
 *  pre: activations off
 */

static void
ThreadTop (thread_t	*th,
	   entry_t	 entry,
	   addr_t	 data)
{
    Threads_clp	 self  = th->threads;
    ThreadMod_st  *st  = self->st;
    VP_cl	  *vpp = VPP(st);
    uint8_t	  *ex_src, *ex_dst;
    exn_context_t *ex_save, *ex_prev;
    int 	   count;
    string_t       ptr;
    PVS() = &(th->pvs_rec);
    TRC(fprintf(stderr, "\n\nThreads_ThreadTop: vpp %x: entered.\n", vpp));
   
    ASSERT((CURRENT_THREAD(st)->thread == th), ntsc_dbgstop(); );

    (CURRENT_THREAD(st))->thread->yielded = False;

    LEAVE (vpp, True);
 
    if (Pvs(heap) == st->heap)
    {
	/* XXX PRB The first thread will need to fix up its stack and 
	   bind to the StretchAllocator  - at this stage we don't even 
	   have a connection to the Binder! */

	/* This is the first thread; no others are running; replace
	   the current PVS heap (from which stacks, threads, event_count's
	   etc. are allocated in ENTER/LEAVE critical sections) with a
	   _new_ locked heap, from which exception arguments and user
	   data can be allocated under a mutex. */

	/* XXX PRB We could let the first thread create a locked heap once
	   it has a StretchAllocator? */
	HeapMod_clp HeapMod= NAME_FIND("modules>HeapMod", HeapMod_clp);
	Heap_clp new_heap = HeapMod$New (HeapMod, st->u_stretch);
	if (!new_heap) RAISE_Heap$NoMemory ();
	Pvs(heap) = (Heap_clp) HeapMod$NewLocked (HeapMod, new_heap, 
						  Pvs(srcth));
    }

    /* These hooks are used to allow thread specific initialisation 
       functions. e.g. by a BSD or Posix emulation module */
    TRY {
	hooks_t *hook;
	
	for (hook = st->hooks.next; hook != &st->hooks; hook = hook->next)
	    if (hook->hook->op->Forked)
		ThreadHooks$Forked (hook->hook);
	
	entry (data);

    } CATCH_ALL {

	/* Store the backtrace before we damage it. Can't push
	 * anything onto stack, so need exn_* to be on it already.
	 * Also means call to memmove() is out of the question. */
	ex_prev= &exn_ctx;
	ex_src = (void*)(exn_ctx.down);
	while(ex_src)
	{
	    ex_dst   = alloca(sizeof(exn_context_t));
	    ex_save  = (void*)ex_dst;
	    if (ex_dst - ex_src >= sizeof(exn_context_t))
	    {
		/* copy forwards */
		count = sizeof(exn_context_t);
		while(count-- > 0)
		    *ex_dst++ = *ex_src++;
	    }
	    else
	    {
		/* copy backwards */
		count = sizeof(exn_context_t);
		ex_dst += count;
		ex_src += count;
		while(count-- > 0)
		    *ex_dst-- = *ex_src--;
	    }

	    /* now hack the copy's "up" & "down" pointers */
	    ex_prev->down = ex_save;
	    ex_save->up = ex_prev;
	    ex_prev = ex_save;

	    /* process further down the stack */
	    ex_src = (void*)ex_save->down;
	}

	/* We now have private copy of exn_ctx records from the stack,
	 * and they're covered by alloca() regions, so compiler knows
	 * about them. We can now use the stack safely again. */

	/* print a backtrace */
	{
	    exn_context_t *bt = &exn_ctx;
	    int i = 0;
	    fprintf(stderr, "Exception backtrace: \n");
	    /* skip to the end, and go back up */
	    while(bt->down)
		bt = bt->down;
	    while(bt)

	    {
		fprintf(stderr, "#%d  %s:%u %s\n",
			i, bt->filename, bt->lineno, bt->funcname);
		i++;
		bt = bt->up;
	    }
	}

	{
	    string_t fullname = exn_ctx.cur_exception;
	    addr_t realargs = exn_ctx.exn_args;

	    TRY {
		Type_Any rep;
		Interface_clp intf;
		Exception_clp exn;
		Context_Names *exnlist;
		
		Type_Code intftc;
		string_t intfname; 
		string_t exnname; 
		
		intfname = strdup(fullname);
		exnname = strpbrk(intfname, "$");
		if (exnname)
		    *exnname++ = 0;
		else
		    exnname = intfname;	/* no '$' in the thing */
	   
		intftc = NAME_LOOKUP(intfname, 
				     (Context_clp) Pvs(types), Type_Code);

		TypeSystem$Info(Pvs(types), intftc, &rep);

		ANY_UNALIAS(&rep);
		intf = NARROW(&rep, Interface_clp);

		exn = NAME_LOOKUP(exnname, (Context_clp)intf, Exception_clp);
		
		exnlist = Context$List((Context_clp)exn);
	    
		if(SEQ_LEN(exnlist) != 0) {
		    Type_Any any;

		    any.type = NAME_LOOKUP(SEQ_ELEM(exnlist, 0), 
					   (Context_clp)exn,
					   Type_Code);
				
		    if(TypeSystem$IsLarge(Pvs(types), any.type)) {
			eprintf("Urk: large type in exception record ...\n");
		    } else {

			any.val = *(uint64_t *)realargs;

			/* arg to Context$NotFound is a REF CHAR not a
			 * STRING otherwise it would be a deep type. */
			if (!strcmp(fullname, "Context$NotFound"))
			{
			    fprintf(stderr, "ThreadTop: dom %qx: "
				    "UNHANDLED EXCEPTION %s(%s)\n", 
				    VP$DomainID(vpp), fullname,
				    /* XXX nasty internal knowledge of args */
				    *(char **)realargs);
			}
			else
			{
			    fprintf(stderr, "ThreadTop: dom %qx: "
				    "UNHANDLED EXCEPTION %s$%s(%a%s)\n", 
				    VP$DomainID(vpp), intfname, exnname, &any, 
				    (SEQ_LEN(exnlist)>1?",...":""));
			}
		    }
		} else {
		    fprintf(stderr, "ThreadTop: dom %q: "
			    "UNHANDLED EXCEPTION %s$%s\n",
			    VP$DomainID(vpp), intfname, exnname);
		}
	    
		SEQ_FREE(exnlist);

	    } CATCH_ALL {
	    
		fprintf (stderr, "ThreadTop: dom %qx: UNHANDLED EXCEPTION "
			 "%s while processing unhandled exception %s\n",
			 VP$DomainID(vpp), exn_ctx.cur_exception, fullname);
	    } ENDTRY;
	}

    } ENDTRY;
    
    Exit_m ( (Threads_clp)self );
    NOTREACHED;
}

/*---Stack Allocation------------------------------------------------------*/

/*
 * There is a problem with Exit in that after deleting the thread's
 * stack, we need to run on something to schedule another thread, so
 * we can't Free the stack in case the stretch allocator should reallocate that
 * space before we're done.
 * 
 * The solution to this is to keep a list of stacks
 * and allocate from them in an appropriate manner. 
 *
 */

static bool_t
AllocStack (ThreadMod_st *st, stack_t *stack)
{
    stack_t			*sp;
    StretchAllocator_SizeSeq	*sizes;
    StretchAllocator_StretchSeq	*stretches;
    bool_t mode; 

    /* Adjust stack size */
    if (!stack->bytes)
	stack->bytes = st->defaultStackBytes;

    TRC(fprintf(stderr, "AllocStack: entered, stack size is %d bytes.\n", 
		stack->bytes));

    /* Try the current free queue, first-fit, lazy deletion */
    ENTER(st->vpp, mode);
    for (sp = SEQ_START(&st->stacks); sp < SEQ_END(&st->stacks); sp++) {
	if (stack->bytes <= sp->bytes)	{
	    *stack = *sp;
	    sp->bytes = 0;
	    LEAVE(st->vpp, mode);
	    return True;
	}
    }
    LEAVE(st->vpp, mode);
    /* XXX - could free stacks other than the last Free'd if
       the free list gets too big. */

    /* Create a new stack */

    sizes = SEQ_NEW (StretchAllocator_SizeSeq, 2, st->heap);
    SEQ_ELEM(sizes, 0) = FRAME_SIZE;	/* the guard page: stacks grow down */
    SEQ_ELEM(sizes, 1) = stack->bytes;	/* the stack itself */



    stretches = STR_NEWLIST(sizes);

    TRC(fprintf(stderr, "AllocStack: stretch SEQ at %p\n", stretches));
    TRC(fprintf(stderr, "AllocStack: stretch SEQ len %x\n", 
		SEQ_LEN(stretches)));

    stack->guard   = SEQ_ELEM (stretches, 0);
    stack->stretch = SEQ_ELEM (stretches, 1);
#if 1
    if (NAME_EXISTS("self")) {
	string_t name = NAME_ALLOCATE("self>thread_%d_stack");
	
	CX_ADD(name, stack->stretch, Stretch_clp);
	free(name);
    }
#endif

    TRC(fprintf(stderr, "AllocStack: free size SEQ\n"));
    SEQ_FREE (sizes);
    TRC(fprintf(stderr, "AllocStack: free stretches SEQ\n"));
    SEQ_FREE (stretches);

    stack->bot = STR_RANGE(stack->stretch, &stack->bytes);

#ifdef CONFIG_MEMSYS_EXPT
    /* need to map (at least) the first page of the stack */
    StretchDriver$Map(Pvs(sdriver), stack->stretch, 
		      STKTOP(stack->bot, stack->bytes));
#else
    /* initialise it to d0d0 only on MEMSYS_STD, so
       that we can look in from Nash's ps command to see how near the
       bottom things are going */
    memset(stack->bot, 0xd0,  stack->bytes);
#endif

    TRC(fprintf(stderr, 
		"AllocStack: stack at %p, mapping into PDom\n", stack->bot));
    /* Map the stretches */
    {
	ProtectionDomain_ID pd = VP$ProtDomID(VPP(st));

	/* no access to guard page */
	STR_SETPROT(stack->guard, pd, 0); 
	STR_SETPROT(stack->stretch, pd, 
		    SET_ELEM (Stretch_Right_Read) |
		    SET_ELEM (Stretch_Right_Write)|
		    SET_ELEM (Stretch_Right_Meta));
    }
    TRC(fprintf(stderr, "AllocStack: done\n"));

    return (stack->bot != NULL);
}

/* activations off */
static void
FreeStack (ThreadMod_st *st, stack_t stack)
{
    stack_t	*sp;

    /* Fill gaps in the current free queue, if any */
    for (sp = SEQ_START(&st->stacks); sp < SEQ_END(&st->stacks); sp++) {
	if (sp->bytes == 0) {
	    *sp = stack;
	    return;
	}
    }

    /* No gaps, so append. */
    SEQ_ADDH (&st->stacks, stack);
}


/*-------------------------------------------- ThreadsPackage New fn ------*/


static ThreadF_clp New_m(ThreadsPackage_cl *self, addr_t entry,
			 addr_t data, const ThreadsPackage_Stack *protoStack,
			 Stretch_clp userStretch, uint32_t defStackBytes,
			 const Pervasives_Init	*prec, 
			 /* RETURNS */ ActivationF_clp *actf_res)
{
    ThreadMod_st *st;
    VP_clp	 vpp = prec->vp;
    bool_t	 mode;
    thread_t	*NOCLOBBER mainThread = NULL;
    ActivationMod_clp actm;
    Activation_clp avec;
    EventsMod_clp emod;
    /* the preemptiveness flag is in the state of the closure we export */
    bool_t preemptive = (bool_t)self->st; 
  
    /* activations off here; running on dcb_rw activation stack */
    TRC (fprintf(stderr, 
		 "Threads$New: prec=%x stackBytes=%x entry=%x data=%x\n",
		 prec, defStackBytes, entry, data));

    st = (ThreadMod_st *) Heap$Malloc (prec->heap, sizeof (*st));
    if (!st) {
	DB(fprintf(stderr, "ThreadMod_New: malloc failed.\n"));
	return (ThreadF_cl *)NULL;
    }

    TRC (fprintf(stderr, "Threads$New: init st\n"));

    CL_INIT (st->cl,    &threads_ms, st);
    CL_INIT (st->actcl, &Activation_ms, st);

    st->heap              = prec->heap;
    st->defaultStackBytes = defStackBytes;
    st->u_stretch	  = userStretch;
    st->vpp		  = vpp;
    st->exns		  = NAME_FIND("modules>ExnSystem", ExnSystem_clp);
    st->preemptive	  = preemptive;

    TQ_INIT ( RUNQUEUE(st) );
    Q_INIT (&st->hooks);

    st->nthreads       = 0;
    st->active_threads = 0;



#undef  SEQ_FAIL
#define SEQ_FAIL() return ((ThreadF_cl *)NULL)

    TRC (fprintf(stderr, "Threads$New: Just before creating stacks\n"));
    {
	stack_t *stack;
	/* We've been passed in a stack */
	SEQ_CLEAR (SEQ_INIT (&st->stacks, 5, st->heap));
	stack = SEQ_END(&st->stacks);
	stack->guard = protoStack->guard;
	stack->stretch = protoStack->stretch;
	stack->bot = STR_RANGE(stack->stretch, &stack->bytes);
	++SEQ_LEN(&st->stacks); 
#ifdef CONFIG_MEMSYS_EXPT
	/* need to map (at least) the first page of the stack */
	TRC(eprintf("Threads$New: mapping top of stack (page=%p)\n", 
		     STKTOP(stack->bot, stack->bytes)));
	StretchDriver$Map(Pvs(sdriver), stack->stretch, 
			  STKTOP(stack->bot, stack->bytes));
#endif

    }
    TRC (fprintf(stderr, "Threads$New: Just after creating stacks\n"));

    /* CreateThread copies st->tmpl_pvs, so it needs to be in good shape */
    st->tmpl_pvs.vp    = prec->vp;
    st->tmpl_pvs.heap  = prec->heap;
    st->tmpl_pvs.types = prec->types;
    st->tmpl_pvs.root  = prec->root;
    st->tmpl_pvs.time  = NAME_LOOKUP("modules>Time", prec->root, Time_clp);
    st->tmpl_pvs.libc  = Pvs(libc); /* XXX should prob get this from context */
    st->tmpl_pvs.thds  = (Threads_clp)&st->cl; 

    actm   = NAME_FIND("modules>ActivationMod", ActivationMod_clp);
    TRC(eprintf("Threads$New: found activationmod at %p\n", actm));

    st->actf = ActivationMod$New(actm, prec->vp, st->tmpl_pvs.time, 
				 prec->heap, VP$NumContexts(prec->vp), 
				 /* OUT */ &avec);
    TRC(eprintf("and created activationF at %p\n", st->actf));
    st->tmpl_pvs.actf  = st->actf;
    if(ActivationF$SetHandler(st->actf, &st->actcl) != NULL) {
	fprintf(stderr, "Urk! ActivationF$SetHandler returned non-NULL.\n");
	return ((ThreadF_cl *)NULL);
    }

    emod   = NAME_FIND("modules>EventsMod", EventsMod_clp);
    st->tmpl_pvs.evs = EventsMod$New(emod, prec->vp, st->actf, 
				     prec->heap, (ThreadF_cl *)&st->cl);

    {
	SRCThreadMod_clp srcthdmod;
	srcthdmod = NAME_FIND("modules>SRCThreadMod", SRCThreadMod_clp); 
	st->tmpl_pvs.srcth = SRCThreadMod$New(srcthdmod, prec->heap);
    }


    st->tmpl_pvs.sdriver = Pvs(sdriver); /* XXX SMH: for now, just copy it */


    /* We need to inherit stdin, stdout, stderr and console from the parent
       - this is quite possibly a nasty hack and should be changed after
       Crackle, but it will have to do for now. */
    st->tmpl_pvs.in      = Pvs(in);
    st->tmpl_pvs.out     = Pvs(out);
    st->tmpl_pvs.err     = Pvs(err);
    st->tmpl_pvs.console = Pvs(console);

#if 0
    /* 
    ** XXX SMH: we are going to try to enable most of the threads package
    ** stuff run using its own 'thread' - this is not a \emph{normal} 
    ** thread: it has no thread operations, and is not placed upon the
    ** run queue or scheduled. 
    ** Instead it performs the scheduling itself. 
    ** Its main thread-like property is that it has a context
    ** and so may run with activations on or off - it is RFAResumed 
    ** if necessary.
    */

    st->insched = False; /* don't RFAResume the first time */
    
    /* Get a context */
    TRY {
	st->schedcx = VP$AllocContext (vpp);
    } CATCH_VP$NoContextSlots() {
	eprintf("ThreadsPackage: cannot continue: no context slots\n");
	RAISE_Threads$NoResources();
    }
    ENDTRY;
#endif

    /*
    ** We also have the special case of threads within critical sections. 
    ** If a thread is within a CS, then its st->critcx is set to its 
    ** context slot number; otherwise st->critcx is NULL_CX. 
    */
    st->critcx   = NULL_CX; 

    /* Now create our main thread, and add it onto the runqueue. */
    TRY	{
	Pervasives_Rec *new_pvs;

	TRC (fprintf(stderr, "Threads$New: create main thread\n"));
	
	mainThread = CreateThread (st, vpp, entry, data, 0,
				   &st->tmpl_pvs, &new_pvs);
    } CATCH_Threads$NoResources() {
    } ENDTRY;

    if (!mainThread) return ((ThreadF_cl *)NULL);

    TRC(eprintf("Threads$New (d%x): main thread clp = %p\n", 
	    VP$DomainID(vpp) & 0xFF, &mainThread->cl));
    ENTER (vpp, mode); 
    Q_INSERT_AFTER (RUNQUEUE(st), &(mainThread->link));
    LEAVE (vpp, mode);


    /* Install the new activation handler */
    VP$SetActivationVector (vpp, avec);

    /* The main thread will be entered via the first activation.  To
       cause that activation, we must yield with activations on.  Thus,
       the save slot must be valid and different from the main thread's
       slot when we yield.  The current resume slot will do. */

    VP$SetSaveSlot(vpp, VP$GetResumeSlot (vpp));

    TRC (fprintf(stderr, "Threads$New: success\n"));
    *actf_res = st->actf;
    return (ThreadF_cl *)&(st->cl);
}



/*
 * End mod/nemesis/threads/threads.c
 */
