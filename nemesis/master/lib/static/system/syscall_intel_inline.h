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
**      System call functions
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Inlined system calls
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: syscall_intel_inline.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef syscall_ix86_inline_h
#define syscall_ix86_inline_h

#include <syscall.h>

/* Don't inline "reboot"; it'd be nice for it not to return, and I don't
   think there's a way to specify that with __asm__. */

/* Similarly, don't inline dbgstop */

static void INLINE ntsc_rfa(void)
{
    __asm__ __volatile__ ("int $" NTSC_RFA_STRING);
}

static void INLINE ntsc_rfa_resume(VP_ContextSlot cx)
{
    __asm__ __volatile__ ("int $" NTSC_RFA_RESUME_STRING
	    : /* No output (indeed, no return) */
	    : "a" (cx) );
}

static void INLINE ntsc_rfa_block(Time_T until)
{
    uint32_t hi,lo;
    hi=until>>32;
    lo=until&0xffffffff;
    __asm__ __volatile__ ("int $" NTSC_RFA_BLOCK_STRING
			  : /* No output */
			  : "a" (lo), "b" (hi) );
}

static void INLINE ntsc_block(Time_T until)
{
    uint32_t hi,lo;
    hi=until>>32;
    lo=until&0xffffffff;
    __asm__ __volatile__ ("int $" NTSC_BLOCK_STRING
			  : /* No output */
			  : "a" (lo), "b" (hi) );
}

static void INLINE ntsc_yield(void)
{
    __asm__ __volatile__ ("int $" NTSC_YIELD_STRING);
}

static ntsc_rc_t INLINE ntsc_send(Channel_TX tx, Event_Val val)
{
    ntsc_rc_t r;
    __asm__ __volatile__ ("int $" NTSC_SEND_STRING
	    : "=a" (r)
	    : "a" (tx), "b" (val) );
    return r;
}

static ntsc_rc_t INLINE ntsc_unmask_irq(int irq)
{
    ntsc_rc_t r;
    __asm__ __volatile__ ("int $" NTSC_UNMASK_IRQ_STRING "# %0"
	    : "=a" (r)
	    : "a" (irq) );
    return r;
}

static ntsc_rc_t INLINE ntsc_mask_irq(int irq)
{
    ntsc_rc_t r;
    __asm__ __volatile__ ("int $" NTSC_MASK_IRQ_STRING
	    : "=a" (r)
	    : "a" (irq) );
    return r;
}

static int INLINE ntsc_swpipl(int ipl)
{
    int r;
    __asm__ __volatile__ ("int $" NTSC_SWPIPL_STRING
	    : "=a" (r)
	    : "a" (ipl) );
    return r;
}

/* ... skip a few ... */

static void INLINE ntsc_flushtlb(void)
{
    __asm__ __volatile__ ("int $" NTSC_FLUSHTLB_STRING);
}

/* ... skip a few ... */

static void INLINE ntsc_entkern(void)
{
    __asm__ __volatile__ ("int $" NTSC_ENTKERN_STRING);
}

static void INLINE ntsc_leavekern(void)
{
    __asm__ __volatile__ ("int $" NTSC_LEAVEKERN_STRING);
}

static int INLINE ntsc_callpriv(word_t vec, 
				word_t arg1, 
				word_t arg2, 
				word_t arg3, 
				word_t arg4)
{
    int r;
    __asm__ __volatile__ ("int $" NTSC_CALLPRIV_STRING
			  : "=a" (r)
			  : "a" (vec), "b" (arg1), "c" (arg2), 
			  "d" (arg3), "D" (arg4));

    return r;
}

#endif /* syscall_ix86_inline_h */
