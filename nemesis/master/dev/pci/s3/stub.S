/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/sandpiper/FB
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Assembler stubs for blitting 
** 
** ENVIRONMENT: 
**
**      call_pal CALLPRIV - i.e. same constraints as interrupt stubs.
** 
** ID : $Id: stub.S 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include        <asm.h>
#include	<pal/nempal.h>

#ifndef __LANGUAGE_C 	/* so gcc -M will work for dependencies */

	.text
	.set noreorder
	.globl	blit_raster
	.globl	blit_ava
	.globl	blit_dfs
	.globl	blit_video
	.globl	blit_dfs_8
	.globl  blit_raster_8
	.globl	process_mgr_pkt
	
LEAF(blit_raster_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   blit_raster		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(blit_raster_stub)

LEAF(blit_ava_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   blit_ava		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(blit_ava_stub)

LEAF(blit_dfs_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   blit_dfs		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(blit_dfs_stub)

LEAF(blit_video_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   blit_video		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(blit_video_stub)

LEAF(blit_dfs_8_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   blit_dfs_8		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(blit_dfs_8_stub)

LEAF(blit_raster_8_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   blit_raster_8		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(blit_raster_8_stub)

	
LEAF(process_mgr_pkt_stub)
	nop
	br      t0, 1f			// set t0 to point to next inst.
	.quad   process_mgr_pkt		// here it is
1:	ldq     pv, 0(t0)               // load it in; we now have a pv.
	bis     gp, gp, a5              // Move DCB to a3
	stq	ra, FRM_Q_A0(sp)	// Save return address
	jsr	ra, (pv)		// Call blit_fn
	ldq	ra, FRM_Q_A0(sp)	// Restore return address
	call_pal PAL_RTI_ENTRY		// Done.
END(process_mgr_pkt_stub)		

#endif /* __LANGUAGE_C */

/*
*** End of file
 */	



