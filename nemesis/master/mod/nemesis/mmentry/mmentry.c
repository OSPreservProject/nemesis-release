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
**      mod/nemesis/mmentry/mmentry.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      An implementation of MMEntry: application level memory 
**	fault handling.
** 
** ENVIRONMENT: 
**
**      User space.
**
*/
#include <nemesis.h>
#include <exceptions.h>
#include <links.h>
#include <time.h>

#include "mm_st.h"


#ifdef MMENTRY_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif


/* SMH: define below for extra sanity checking */
#define PARANOIA

/*
 * Create an MMEntry
 */
#include <MMEntryMod.ih>
static MMEntryMod_New_fn New_m;
static MMEntryMod_op m_op = { New_m };
static const MMEntryMod_cl m_cl = { &m_op, NULL };
CL_EXPORT (MMEntryMod, MMEntryMod, m_cl);



/*
 * MMEntry interface operations 
 */
static MMEntry_Satisfy_fn	Satisfy_m;
static MMEntry_Close_fn		Close_m;
static MMEntry_op e_op = { 
    Satisfy_m,
    Close_m, 
};



static void Worker(worker_t *worker);


/*--------Build-an-entry--------------------------------------------*/
static MMEntry_clp New_m(MMEntryMod_clp self, MMNotify_clp mmnfy, 
			 ThreadF_clp thdf, Heap_clp heap, uint32_t nf)
{

    MMEntry_st *st;
    fault_t    *faults;
    worker_t   *workers;
    int i;

    /* Allocate structure */
    if ( !(st = Heap$Malloc(heap, sizeof(*st))) ) {
	DB(eprintf("MMEntryMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }

    if (!(faults = Heap$Malloc(heap, sizeof(fault_t)*nf))) {
	DB(eprintf("MMEntryMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }

#define NWORKERS 2   /* XXX SMH: should be a parameter */

    if (!(workers = Heap$Malloc(heap, sizeof(worker_t)*NWORKERS))) {
	DB(eprintf("MMEntryMod$New: Allocation failed.\n"));
	RAISE_Heap$NoMemory();
    }

    CL_INIT (st->entry_cl,  &e_op,   st);
    
    st->mmnfy  = mmnfy;
    st->thdf   = thdf;
    st->heap   = heap;

    /* Initialise the dispatch queues */
    Q_INIT(&st->idle);
    Q_INIT(&st->pending);

    /* Intialise the worker pool queue */
    Q_INIT(&st->pool);

    st->maxf    = nf;

    /* 
    ** To save a malloc in the notify, we allocate a number of 
    ** 'free' fault structs and blat 'em onto the idle queue. 
    ** Hence Notify can fill one of them up, slap it on the pending 
    ** queue, and call Satisfy on us to let us know. 
    */
    for(i = 0; i < nf; i++) 
	Q_ENQUEUE(&st->idle, &(faults[i].link));
    
    /* Register our new entry with the notification handler */
    MMNotify$SetMMEntry(st->mmnfy, &st->entry_cl);

    /* Fork our worker threads. XXX SMH: currently only one */
    for(i = 0; i < NWORKERS; i++) {
	workers[i].mm_st  = st;
	workers[i].thread = Threads$Fork((Threads_cl *)thdf, Worker, 
					 &(workers[i]), 0);
    }
    
    /* That's it! */
    return &st->entry_cl;
}



/*--------MMEntry-operations------------------------------------------*/


#ifdef __ALPHA__    
static const char *const gp_reg_name[] = {
    "v0 ", "t0 ", "t1 ", "t2 ", "t3 ", "t4 ", "t5 ", "t6 ",
    "t7 ", "s0 ", "s1 ", "s2 ", "s3 ", "s4 ", "pvs", "FP ",
    "a0 ", "a1 ", "a2 ", "a3 ", "a4 ", "a5 ", "t8 ", "t9 ",
    "t10", "t11", "ra ", "pv ", "at ", "GP ", "SP ", "0  " };
#endif


static void cx_dump (context_t *cx)
{
#ifdef __ALPHA__    
    uint32_t      i;

    for (i = 0; i < 16; i++)
    {
	eprintf("%s: %p %s: %p\n", 
		 gp_reg_name[i],  cx->gpreg[i],
		 gp_reg_name[i+16], (i == 16) ? 0 : cx->gpreg[i+16]);
    }
    eprintf("PC : %p  PS : %p\n", cx->r_pc, cx->r_ps);
#endif
#ifdef __IX86__
    eprintf("eax=%p  ebx=%p  ecx=%p  edx=%p\nesi=%p  edi=%p  ebp=%p esp=%p\n",
	    cx->gpreg[r_eax], cx->gpreg[r_ebx], cx->gpreg[r_ecx],
	    cx->gpreg[r_edx], cx->gpreg[r_esi], cx->gpreg[r_edi],
	    cx->gpreg[r_ebp], cx->esp);
    eprintf("eip=%p  flags=%p\n",  cx->eip, cx->eflag);
#endif
    return;
}

/*
** Fork a thread to deal with memory faults.
*/
static bool_t Satisfy_m(MMEntry_clp self, const MMEntry_Info *info)
{
#ifdef CONFIG_MEMSYS_EXPT
    MMEntry_st *st = self->st;
    worker_t   *worker;
    fault_t    *fault;
#endif

    TRC(eprintf("MMEntry$Satisfy called: self=%p, info=%p\n", self, info));
#ifdef CONFIG_MEMSYS_EXPT
    if( (fault = (fault_t *)Q_DEQUEUE(&st->idle)) == NULL) {
	eprintf("MMEntry$Satisfy: cannot handle any more faults!\n");
	return False;
    }

    TRC(eprintf("MMEntry$Satisfy: got free fault structure at %p\n", fault));

    /* Copy accross the information. Note we cannot use bcopy/memcpy
       since Pvs may be bogus (and hence veneers unusable) */
    fault->info.code    = info->code;  
    fault->info.vaddr   = info->vaddr; 
    fault->info.str     = info->str; 
    fault->info.sdriver = info->sdriver; 
    fault->info.context = info->context;

    fault->thread = ThreadF$CurrentThread(st->thdf);

    TRC(eprintf("MMEntry$Satisfy: blocking faulting thread.\n"));
    fault->incs = ThreadF$BlockThread(st->thdf, fault->thread, FOREVER);

    /* If the pool isn't empty, unblock a[nother] worker thread */
    if((worker = (worker_t *)Q_DEQUEUE(&st->pool)) != NULL) {

	TRC(eprintf("MMEntry$Satisfy: unblocking worker thread.\n"));
	/* XXX SMH: unblock our worker with same cs as faulting thd */
	ThreadF$UnblockThread(st->thdf, worker->thread, fault->incs);
    }

    TRC(eprintf("MMEntry$Satisfy: enqueuing fault on pending queue.\n"));
    Q_ENQUEUE(&st->pending, &fault->link);
    TRC(eprintf("MMEntry$Satisfy: success!\n"));
    return True;
#endif
    return False;
}


/*
** Worker thread; looks for work to do on the pending queue
** and does it. Blocks if nothing to do.
*/
static void Worker(worker_t *worker)
{
    MMEntry_st *st = worker->mm_st;
#ifdef CONFIG_MEMSYS_EXPT
    StretchDriver_Result result;
#endif
    fault_t    *current; 
    bool_t      alerted; 

    alerted = False;
    Thread$SetDaemon(Pvs(thd));

    while(1) {

	/* Check queue; need to protect against activations */
	Threads$EnterCS((Threads_cl *)st->thdf, True);
	if((current = (fault_t *)Q_DEQUEUE(&st->pending)) == NULL) {
	    /* 
	    ** If nothing to do, we mark ourselves as blocked, and 
	    ** then yield => will not run until explicitly unblocked. 
	    ** For this reason, we enqueue ourselves onto the worker 
	    ** pool so that Satisfy can unblock us when a fault arrives.
	    */
	    Q_ENQUEUE(&st->pool, &worker->link); 
	    alerted = ThreadF$BlockYield(st->thdf, FOREVER);

#ifdef PARANOIA
	    /* Don't expect to be alerted. How could this happen?? */
	    if(alerted)
		eprintf("MMEntry$Worker: after BlockYield, were alerted!\n");
#endif
	    /* At this point we should have something to do in 'current' */
	    current = (fault_t *)Q_DEQUEUE(&st->pending);
	}
	Threads$LeaveCS((Threads_cl *)st->thdf);


#ifdef PARANOIA	
	if(!current) {
	    eprintf("MMEntry$Worker: something's wrong.\n");
	    eprintf("              : woken for NULL fault.\n");
	    ntsc_dbgstop();
	}
#endif
	switch(current->info.code) {
	case Mem_Fault_PAGE:
#ifdef __ALPHA__
	    TRC(eprintf("MMEntry$Worker %qx: page fault va=%p pc=%p\n", 
			RO(Pvs(vp))->id, current->info.vaddr, 
			((context_t *)current->info.context)->r_pc));
#endif
#ifdef __IX86__
	    TRC(eprintf("MMEntry$Worker %qx: page fault at EIP=%p\n", 
			RO(Pvs(vp))->id, current->info.vaddr, 
			((context_t *)current->info.context)->eip));
#endif

#ifdef CONFIG_MEMSYS_EXPT
	    result = StretchDriver$Map(current->info.sdriver, 
				       current->info.str, 
				       current->info.vaddr);
	    if(result != StretchDriver_Result_Success) {
		eprintf("MMEntry$Worker: failed to map vaddr %p!\n", 
			current->info.vaddr);
		eprintf("str     = %p\n", current->info.str);
		eprintf("sdriver = %p\n", current->info.sdriver);
		eprintf("context = %p\n", current->info.context);
		eprintf("thread  = %p\n", current->thread); 
		cx_dump(current->info.context);
		ntsc_dbgstop();
	    }
#endif
	    break;

	CASE_RECOVERABLE:
#ifdef CONFIG_MEMSYS_EXPT
	    StretchDriver$Fault(current->info.sdriver, current->info.str, 
				current->info.vaddr, current->info.code);
#endif
	    break;

	CASE_UNRECOVERABLE:
	    eprintf("MMEntry$Worker: got unrecoverable fault '%s' on va=%p\n", 
		    reasons[current->info.code], current->info.vaddr);
	    ntsc_dbgstop();
	    break;
	}

	/* Ok! We've sorted it out, so unblock the faulting thread */
	TRC(eprintf("MMEntry$Worker %qx: sorted it out!!!\n", 
		    RO(Pvs(vp))->id));
	
	Threads$EnterCS((Threads_cl *)st->thdf, True);
	Q_ENQUEUE(&st->idle, &current->link);
	alerted = ThreadF$UnblockYield(st->thdf, current->thread, 
				       current->incs);
	Threads$LeaveCS((Threads_cl *)st->thdf);
#ifdef PARANOIA
	/* Don't expect to be alerted. How could this happen?? */
	if(alerted)
	    eprintf("MMEntry$Worker: thread was \emph{alerted} ??? \n");
#endif
    }

    eprintf("MMEntry$Worker: not reached!\n");
    ntsc_dbgstop();
}



/*
** Close down this entry
*/
static void Close_m(MMEntry_clp self)
{
    MMEntry_st *st = self->st;

    eprintf("Urk! MMEntry$Close called (self=%p)\n", self);
    /* XXX SMH: requires more thought */
    FREE (st);
}


