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
**      kernel.h
** 
** DESCRIPTION:
** 
**      Defines the kernel environment - direct linkage to ntsc_xxx,
**      and k_printf, includes primouts.h.
** 
** ID : $Id: kernel.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _kernel_h_
#define _kernel_h_

#include <ntsc.h>
#include <kernel_tgt.h>

extern void		k_sched_init(void);
extern void		k_printf (const char *format, ...);
extern void		kdump   (context_t *cx);

extern NORETURN        (ksched_scheduler (addr_t, dcb_ro_t *, context_t *));
extern NORETURN	       (k_activate (dcb_ro_t *, Activation_Reason));
extern NORETURN	       (k_idle ());

extern NORETURN	       (k_panic (const char *why, ...));
extern NORETURN	       (k_halt  ());


extern void		gdb_stub(int sigval);


#endif /* _kernel_h_ */
