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
**      sys/Domains/sched_ix86.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Machine dependent and scheduler dependent initialisation.
** 
** ENVIRONMENT: 
**
**	Called from sched.c
**
*/


#include <nemesis.h>
#include <kernel.h>		/* for kernel_st  */
#include <Timer.ih>
#include <DomainMgr_st.h>
#include <dcb.h>
#include <sdom.h>
#include <mmu_tgt.h>
#include <pip.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

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


    /* Setup the protection domain things */
    rop->pdbase = ((word_t *)INFO_PAGE.pd_tab)[PDIDX(rop->pdid)]; 
    rop->asn    = 0xdeadbeef;  /* no ASNs on x86 */
    
    if(!rop->pdbase) { /* Paranoia */
	eprintf("Arch_InitDomain: bad protection domain ID %x\n", rop->pdid); 
	ntsc_dbgstop();
    }  
    
    /* XXX Copy pervasives from 
       parent; this is a nasty
       hack for boot-time */
    rop->ctxts->pvs = (word_t) *(INFO_PAGE.pvsptr);   

    TRC(eprintf("Arch_InitDomain(ix86): rop=%lx rwp=%lx psmask=%lx\n",
		rop, rwp, rop->psmask));
}

PUBLIC void Arch_StartScheduler(kernel_st *kst) {
    
    TRC(eprintf(" + Setting Timer to %qx\n", kst->cur_sdom->deadline));

    Timer$Set(kst->t, kst->cur_sdom->deadline);
}


/* End */
