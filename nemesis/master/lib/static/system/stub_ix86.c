/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved (except, who the hell wants them)       	    *
**                                                                          *
*****************************************************************************
**
** FACILITY:	
**
**	Generic device driver routines for Intel
**
** FUNCTIONAL DESCRIPTION:
**
**	first level interrupt handler.
**
** CALLING ENVIRONMENT:
**
**	Entered from kernel (running on kernel stack), in kernel mode.
**	IRQ in question has been masked in the PICs.
**	Two params passed: void * to private state
**                         kinfo_t * to useful info (see kernel_st.h)
**
** CREATED: 4th September 1996 (Austin Donnelly, after PRB)
**
** ID : $Id: stub_ix86.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <kernel_st.h>
#include <irq.h>

void _generic_stub(irq_stub_st *st, const kinfo_t *kinfo)
{
    kinfo->k_event(st->dcb, st->ep, 1);

/*    *(kinfo->reschedule) = True; */
}
	 
/* End of intel_stub.c */
