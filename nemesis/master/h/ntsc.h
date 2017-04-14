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
**      ntsc.h
** 
** DESCRIPTION:
** 
**      Defines target-indep Nemesis Trusted Supervisor Calls and
**      includes the target-dependent header.
** 
** ID : $Id: ntsc.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _ntsc_h_
#define _ntsc_h_

#include <nemesis.h>

/* 
 * NTSC Return Codes:
 */
typedef enum {
  rc_success      = 0,			    /* no problem		*/
  rc_ch_invalid   = -1,			    /* = Channel.Invalid	*/
  rc_ch_bad_state = -2,			    /* = Channel.BadState	*/
  rc_ch_no_slots  = -3,			    /* = Channel.NoSlots	*/
  rc_iid_invalid  = -4,			    /* bad interrupt id		*/
  rc_int_assigned = -5			    /* interrupt already bound	*/
} ntsc_rc_t;

typedef word_t	interrupt_mask_t;
typedef sword_t interrupt_id;    

#define NULL_IID ((interrupt_id) -1L)

/* 
 * After "#include <ntsc_tgt.h>", the following calls must be defined
 * (as macros or externs), for use only by appropriately privileged
 * domains:
 * 
 *   void   ntsc_halt ();                   -- halt the machine
 * 
 *   word_t ntsc_swap_status (word_t x);    -- swap processor status
 *   word_t ntsc_ipl_high ();               -- swap to all interrupts disabled
 *   word_t ntsc_mode_kernel ();            -- swap to kernel privilege
 *   word_t ntsc_kernel_critical ();        -- swap to ipl_high and mode_kernel
 * 
 *   ntsc_rc_t    ntsc_mask_irq     (word_t irq);
 *   ntsc_rc_t ntsc_unmask_irq (word_t irq); */

/* 
 * Also, the following calls must be defined for use by any domain (see VP.if):
 *
 * Scheduling:
 * 
 *   void   ntsc_rfa        (void)                  -- return from activation
 *   void   ntsc_rfa_resume (VP_ContextSlot cx)     -- rfa resuming context cx
 *   void   ntsc_rfa_block  (Time_T until);         -- Time_T = word_t
 *   void   ntsc_block      (Time_T until);
 *   void   ntsc_yield      (void);
 *
 * Event Channels:
 *
 *   ntsc_rc_t ntsc_send  (Channel_TX tx, Event_val val);
 *
 */

/* And finally, a nasty (but nice too) macro */
#define ENTER_KERNEL_CRITICAL_SECTION() 	\
{						\
    VP_clp _vpp= Pvs(vp);                       \
    bool_t _mode= VP$ActivationMode(_vpp);	\
    VP$ActivationsOff(_vpp);			\
    ntsc_entkern();

#define LEAVE_KERNEL_CRITICAL_SECTION() 	\
    ntsc_leavekern();				\
    if(_mode)					\
	VP$ActivationsOn(_vpp);			\
}


#include <ntsc_tgt.h>		/* target-dep definitions of the above	*/

#endif /* _ntsc_h_ */





