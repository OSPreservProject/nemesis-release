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
**      ix86/kernel_tgt.h
** 
** DESCRIPTION:
** 
**      Target-dep. kernel defns.
**      
** 
** ID : $Id: kernel_tgt.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
*/

#ifndef _kernel_tgt_h_
#define _kernel_tgt_h_

#include <kernel_st.h>
#include <interrupt.h>

extern bool_t reschedule;
extern int_mask_reg_t int_mask;

extern NORETURN(ksched_scheduler (addr_t	st,
				  dcb_ro_t	*cur_dcb,
				  context_t	*cur_ctxt));

extern NORETURN(k_activate (dcb_ro_t *, uint32_t reason));

extern void k_swppdom(dcb_ro_t *);

extern NORETURN(k_irq (uint32_t nr));

extern NORETURN(k_exception (int nr, int error));

extern void k_mmgmt (int code, uint32_t, uint32_t);

extern NORETURN(k_idle(void));

extern NORETURN(k_reboot(void));

extern NORETURN(k_enter_debugger(context_t *cx, uint32_t cs,
				 uint32_t ds, uint32_t exception));

#endif /* _kernel_tgt_h_ */
