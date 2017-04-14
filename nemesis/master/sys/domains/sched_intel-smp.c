/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      sys/domains/sched_intel-smp.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Intel SMP Machine dependent and scheduler dependent initialisation.
** 
** ENVIRONMENT: 
**
**	Called from DomainMgr.c
**
*/


#include <nemesis.h>
#include <kernel.h>		/* for kernel_st  */
#include <Timer.ih>
#include <DomainMgr_st.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static void sdomq_remove(SDomHeap_t *pq)
{
    PQ_REMOVE((*pq));
}

static void sdomq_insert(SDomHeap_t *pq, SDom_t *s)
{
    PQ_INSERT((*pq), s)
}

static void sdomq_del(SDomHeap_t *pq, word_t ix)
{
    PQ_DEL((*pq), ix)
}
static void sdomq_delete(SDomHeap_t *pq, SDom_t *s)
{
    uint32_t ix;
    for (ix = 1; ix <= PQ_SIZE((*pq)); ix++) {
	if (s == PQ_NTH((*pq), ix)) {
	    sdomq_del(pq, ix);
	    return;
	}
    }
    
    eprintf("sdomq_delete: Can't find sdom %p in q %p\n", s, pq);
}

#undef PQ_REMOVE
#define PQ_REMOVE(_pq) sdomq_remove(&(_pq))
#undef PQ_INSERT
#define PQ_INSERT(_pq,_s) sdomq_insert(&(_pq),_s)
#undef PQ_DEL
#define PQ_DEL(_pq,_ix) sdomq_del(&(_pq), _ix)


PUBLIC void
Arch_InitDomain(DomainMgr_st *st,
	  dcb_ro_t *rop,
	  dcb_rw_t *rwp,
	  bool_t k)
{
    rop->schst  = DCBST_K_STOPPED;
    rop->psmask = k ? PSMASK(PS_KERNEL_PRIV)|PSMASK(PS_IO_PRIV) : 0;
    rop->ps     = k ? PSMASK(PS_IO_PRIV) : 0;
    rop->fen    = 0;
    rop->ctxts->pvs = INFO_PAGE_MYCPU.dcb_rw->pvs; /* XXX Copy pervasives from 
						parent; this is a nasty
						hack for boot-time */

    TRC(eprintf("Arch_InitDomain(ix86): rop=%lx rwp=%lx psmask=%lx\n",
		rop, rwp, rop->psmask));
}


PUBLIC void
Scheduler_AddDomain(DomainMgr_st *st, dcb_ro_t *rop, dcb_rw_t *rwp)
{
    int cpu = smp_cpuid();
    kernel_st    *kst = st->kst;
    bool_t first_domain = (kst->cur_dcb[cpu] == NULL);

    /*
     * Fudge first domain - this does not need to be in the critical
     *   section since we will be in primal (? RJB).
     */
    if (first_domain) {
      TRC(eprintf("Scheduler_AddDomain: first domain.\n"));
	kst->cur_dcb[cpu]= rop;
	kst->cur_sdom[cpu]= rop->sdomptr;
    }

    /* XXX mdw: I don't bother not replacing the running domain, since
     * it's not at the head of the run queue.
     */

    rop->schst = DCBST_K_RUN;
    rop->sdomptr->state = sdom_run;
    SMP_SPIN_LOCK(&(kst->run_lock));
    PQ_INSERT(kst->run, &(rop->sdom));
    SMP_SPIN_UNLOCK(&(kst->run_lock));
    

    if(first_domain) { /* XXX start up the scheduler! */
	TRC(eprintf("Scheduler_AddDomain: got first domain!\n"));
	TRC(eprintf(" + Setting Timer to %q\n", kst->cur_sdom[cpu]->deadline));
	if (!is_boot_cpu()) {
	  /* mdw: Need to enable paging before setting the timer, as
	   * we'll start to be scheduled now...
	   * Note: This code is never called on non-boot CPUs at the
	   * present (instead, Nemesis sends a message to the AP's to 
	   * start scheduling). I'll leave this here anyway.
	   */
		 eprintf("Scheduler_AddDomain: XXX Non-boot-CPU code case being called from sched_intel-smp.c - this should not happen.\n");
	   ntsc_flushtlb();
	   ntsc_enablepaging();
        }

	Timer$Set(kst->t[cpu], kst->cur_sdom[cpu]->deadline);
    }

    /* All done */
    return;
}


/* XXX mdw: Completely untested for SMP. */
PUBLIC void Scheduler_RemoveDomain(DomainMgr_st *st, 
				   dcb_ro_t *rop, dcb_rw_t *rwp)
{
    kernel_st *kst = st->kst;
    SDom_t   *sdom = rop->sdomptr;
    SDomHeap_t  *q = NULL;
    unsigned long *lock;
    uint32_t ix;
    word_t ipl = ntsc_ipl_high();
    
    if(!((rop->schst == DCBST_K_DYING) || (rop->schst == DCBST_K_STOPPED))) {

	rop->schst = DCBST_K_STOPPED;
	
	switch(sdom->state) {
	case sdom_block:
	    q = &kst->blocked;
	    lock = &kst->blocked_lock;
	    break;
	case sdom_wait:
	    q = &kst->wait;
	    lock = &kst->wait_lock;
	    break;
	case sdom_run:
	    q = &kst->run;
	    lock = &kst->run_lock;
	    break;
	}
	SMP_SPIN_LOCK(lock);
	sdomq_delete(q, sdom);
	SMP_SPIN_UNLOCK(lock);
    } else {
	eprintf("Domain %qx already stopped/dying\n", rop->id);
    }
    ntsc_swpipl(ipl);

    return;
}

/* End */
