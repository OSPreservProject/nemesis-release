/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
`*****************************************************************************
**
** FILE:
**
**      ix86/ntsc_tgt.h
** 
** DESCRIPTION:
** 
**      Target-dependent NTSC definitions; included by <ntsc.h>
**      
**      ix86 version.
** 
** ID : $Id: ntsc_tgt.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
*/

#ifndef __ntsc_tgt_h__
#define __ntsc_tgt_h__

#include <dcb.h>
#include <mmu_tgt.h>
#include <VP.ih>

/* 
 * Target-dependent NTSC calls for ix86
 */

/*  
 * After "#include <ntsc.h>", the following calls are defined (as macros
 * or externs), for use only by appropriately privileged domains:
 */

extern NORETURN                (ntsc_reboot     (void));
extern NORETURN                (ntsc_dbgstop    (void));

/* 
 * Also, the following calls are defined for use by any domain (see VP.if):
 *
 * Scheduling:
 */
 
static void   			ntsc_rfa        (void);

/* XXX this should be a NORETURN */
static void                     ntsc_rfa_resume (VP_ContextSlot cx);
static void			ntsc_rfa_block  (Time_T until);
static void			ntsc_block      (Time_T until);
static void			ntsc_yield      (void);
static int                      ntsc_callpriv   (word_t vector, word_t arg1,
						 word_t arg2, word_t arg3, 
						 word_t arg4);

/*
 * Event Channels:
 */

static ntsc_rc_t		ntsc_send  (Channel_TX tx, Event_Val val);

/*
 * Private or privileged syscalls
 */
static ntsc_rc_t		ntsc_unmask_irq(int irq);
static ntsc_rc_t                ntsc_mask_irq(int irq);
static int			ntsc_swpipl(int ipl);
extern void                     ntsc_kevent(dcb_ro_t *rop, Channel_RX rx,
					    Event_Val inc);
extern NORETURN                (ntsc_actdom(dcb_ro_t *dom, uint32_t reason));


/* For mmgmt support we have five more calls */

extern word_t                   ntsc_map(word_t vpn, word_t idx, word_t bits);
/* extern word_t                   ntsc_trans(word_t vpn); XXX now inlined */
extern word_t                   ntsc_nail(word_t vpn, word_t np, word_t xs);
extern word_t                   ntsc_prot(word_t pdid, word_t vpn, 
					  word_t np, word_t xs);
extern word_t                   ntsc_gprot(word_t vpn, word_t np, word_t xs);

extern void                     ntsc_wrpdom(addr_t pdom);
extern void                     ntsc_setpgtbl(addr_t hw, addr_t sw, addr_t t);
static void                     ntsc_flushtlb(void);

static void                     ntsc_entkern(void);
static void                     ntsc_leavekern(void);
extern void			ntsc_putcons(uint8_t c);

extern NORETURN	               (ntsc_chain(addr_t start, uint32_t length,
					   string_t cmdline));

/*
** Finally, a couple of macros. Yuk.
*/
#define ntsc_ipl_high() ntsc_swpipl(0)  /* disable all interrupts */

/* halt is the same as dbgstop on Intel, since we don't have a low level
 * monitor rom or console we can bail to */
#define ntsc_halt() ntsc_dbgstop()


/* Routines for doing perf counter reads: PPro and above only */
static __inline__ word_t read_pmctr0(void) 
{
    uint32_t __h,__l;
    __asm__ __volatile__ (
	"movl $0x0, %%ecx; rdpmc" 
	: "=a" (__l), "=d" (__h) /* outputs */
	: /* no inputs */
	: "eax", "ecx", "edx" /* clobbers */);
    return __l; 
}

static __inline__ word_t read_pmctr1(void) 
{
    uint32_t __h,__l;
    __asm__ __volatile__ (
	"movl $0x1, %%ecx; rdpmc" 
	: "=a" (__l), "=d" (__h) /* outputs */
	: /* no inputs */
	: "eax", "ecx", "edx" /* clobbers */);
    return __l; 
}


/* We're inlining system calls now; include the routines here */
#include "../../lib/static/system/syscall_intel_inline.h"

#endif /* __ntsc_tgt_h__ */
