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
**      sys/Domains/sched.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Generic non-SMP scheduler functions.
** 
** ENVIRONMENT: 
**
**	Called from DomainMgr.c
**
*/


#include <nemesis.h>
#include <kernel.h>		/* for kernel_st  */
#include <DomainMgr_st.h>
#include <dcb.h>
#include <sdom.h>
#include <stdio.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

extern void Arch_StartScheduler(kernel_st *kst);

void sdomq_remove(SDomHeap_t *pq)
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
Scheduler_AddDomain(DomainMgr_st *st, dcb_ro_t *rop, dcb_rw_t *rwp)
{
    kernel_st    *kst = st->kst;
    bool_t        first_domain = (kst->cur_dcb == NULL);
    word_t        ipl;

    if(rop->schst == DCBST_K_DYING) {
	eprintf("Scheduler_AddDomain: Domain %qx is dying!\n", rop->id);
	return;
    }
    
    if(rop->schst != DCBST_K_STOPPED) {
	eprintf("Scheduler_AddDomain: Domain %qx already running!\n",
		rop->id);
	ntsc_dbgstop();
	return;
    }
    
    /*
     * Fudge first domain - this does not need to be in the critical
     *   section since we will be in primal (? RJB).
     */
    if (first_domain) {
	TRC(eprintf("Scheduler_AddDomain: first domain.\n"));
	kst->cur_dcb= rop;
	kst->cur_sdom= rop->sdomptr;
    }

    ipl = ntsc_ipl_high();

    /* Ensure we don't replace the running domain */

    if (PQ_SIZE(kst->run) &&
	(PQ_HEAD(kst->run))->deadline >= rop->sdomptr->deadline) {
	rop->sdomptr->deadline= (PQ_HEAD(kst->run))->deadline+1;
    }
    rop->schst             = DCBST_K_RUN;
    rop->sdomptr->state    = sdom_run;
    PQ_INSERT(kst->run, rop->sdomptr);

    ntsc_swpipl(ipl);

    TRC(eprintf("Scheduler_AddDomain: dumping kstate.\n"));
    TRC(eprintf(" + kst->cur_dcb   = %x\n", kst->cur_dcb));
    TRC(eprintf(" + kst->cur_sdom  = %x\n", kst->cur_sdom));
    TRC(eprintf(" + kst->domq      = %x\n", &kst->domq)); 
    TRC(eprintf(" + kst->t         = %x\n", kst->t));
    TRC(eprintf(" + kst->next      = %x\n", kst->next));
    TRC(eprintf(" + kst->next_optm = %x\n", kst->next_optm));
    TRC(eprintf(" + kst->run       = %x\n", &kst->run));
    TRC(eprintf(" + kst->run       = %x\n", &kst->wait));
    TRC(eprintf(" + kst->blocked   = %x\n", &kst->blocked));

    if (first_domain) { /* XXX start up the scheduler! */
	TRC(eprintf("Scheduler_AddDomain: got first domain!\n"));
	TRC(eprintf(" + Setting Timer to %qx\n", kst->cur_sdom->deadline));

	Arch_StartScheduler(kst);
    }

    /* All done */
    return;
}



PUBLIC void Scheduler_RemoveDomain(DomainMgr_st *st, 
				   dcb_ro_t *rop, dcb_rw_t *rwp)
{
    kernel_st *kst = st->kst;
    SDom_t   *sdom = rop->sdomptr;
    SDomHeap_t  *q = NULL;
    word_t ipl = ntsc_ipl_high();
    
    if(!((rop->schst == DCBST_K_DYING) || (rop->schst == DCBST_K_STOPPED))) {

	rop->schst = DCBST_K_STOPPED;
	
	switch(sdom->state) {
	case sdom_block:
	    q = &kst->blocked;
	    break;
	case sdom_unblocked:
	case sdom_wait:
	    q = &kst->wait;
	    break;
	case sdom_run:
	    q = &kst->run;
	    break;
	}
	sdomq_delete(q, sdom);
    } else {
	eprintf("Domain %qx already stopped/dying\n", rop->id);
    }
    ntsc_swpipl(ipl);

    return;
}

/* End */
