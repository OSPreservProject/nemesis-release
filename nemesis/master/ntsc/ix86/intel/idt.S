/*
 *
 *	idt.S
 *	-----
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * $Id: idt.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 *
 * This code handles the interupt description table for the ix86.
 */

/* First some descriptions.  Note that the table stored and defined
 * here is done in terms of two words.  In reality these must be
 * permuted in an ugly manner due to rediculous behaviour of the 286
 * compatibility inherent in the ix86 idt tables.  This permutation is
 * done in the initialisation code later.
 *
 * Now we do not want to have anything at all to do with task gates in
 * the interrupt tables (since as far as the 386 is concerned Nemesis
 * is a single "task") instead we use either "interrupt gates" or
 * "trap gates".  The difference between these is that for the former
 * the handler is executed with interrupts disabled, and for the
 * latter it is not.
 *
 * For both of these "gates" the CPU will automatically switch from
 * privelege ring 3 up to ring 0 and will load the stack from the TSS
 * in the process (saving the previous stack on the new stack).  If
 * the CPU is already running at ring 0 then it wont mess with stacks.
 *
 * Some of the exceptions also push a word of "status" / "error code"
 * onto the stack.
 *
 * Other points to note.  For security reasons we have to prevent
 * silly users being able to issue "int <n>" instructions where <n> is
 * a number which is important (e.g. like timer interrupts).  This is
 * done by filling in the "DPL" field in the IDT entry.
 * A 3 means a user can issue it.
 *
 * Number allocation. 0x00-0x1f are reserved for Intel and used by
 * various traps and exceptions.  0x20-0x2f are used by the hardware
 * interrupts from the devices (the 8259s are reprogrammed in the
 * 16bit startup code).  0x30-0x42 are used by Nemesis for individual
 * ntsc entries ("system calls").  If a user issues an interrupt
 * greater than 0x42 then this should cause one of the other types of
 * exceptions (I think a segment exception).
 */

#include "segment.h"			/* for KERNEL_CS */
#include <kernel_st.h>
#include <syscall.h>
#include <autoconf/memsys.h>
#include "asm.h"
#include <dcb.h>
#include <dcb.off.h>
#include <pip.h>

#include <timer.h>

#ifndef __LANGUAGE_C			/* so gcc -M will work for .d	*/

#define N_INTERRUPTS (0x30 + NUM_SYSCALLS)
	
/* maybe get these from i386.h ? */

#define IDT_INVALID	0x0000
#define IDT_VALID	0x8000

#define IDT_INTGATE	0x0e00
#define IDT_TRAPGATE	0x0f00

#define IDT_USERMAY	0x6000
#define IDT_PRIVMAY	0x4000

/* compounded definitions */

#define IDT_EXCEPTION		(IDT_VALID | IDT_INTGATE) /* XXX trapgate? */
#define IDT_INTERRUPT		(IDT_VALID | IDT_INTGATE)
#define IDT_SYSCALL		(IDT_VALID | IDT_INTGATE | IDT_USERMAY)
#define IDT_SYSCALL_PRIV	(IDT_VALID | IDT_INTGATE | IDT_PRIVMAY)
#define IDT_CALLPRIV		(IDT_VALID | IDT_INTGATE | IDT_USERMAY)	

/* idt entries */

#define idt(nr,val)		\
	.long	idt_##nr;	\
	.long	val;

#define noidt			\
	.long	0;		\
	.long	IDT_INVALID;

/* ------------------------------ */
/* Exception handler should be defined as:	 */

/* k_exception (int exc_nr, int context_type, int error); */

/* IDT Entry with error code on stack */
#define idt_exception_err(nr)	\
idt_##nr:			; \
	pushl	%edx		; \
	pushl	%ecx		; \
	pushl	%ebx		; \
	pushl	%eax		; \
	call	save_context	; \
	addl	$16, %esp	; \
	pushl	$nr		; \
	call	k_exception

/* IDT Entry without an error code on stack */
#define idt_exception(nr) 	\
	.align 16		; \
idt_##nr:			; \
	pushl	$0x0		; \
	pushl	%edx		; \
	pushl	%ecx		; \
	pushl	%ebx		; \
	pushl	%eax		; \
	call	save_context	; \
	addl	$16, %esp	; \
	pushl	$nr		; \
	call	k_exception

/* 
** IDT Entry for memory faults.
** All the faults which enter here have an error code of zero
** on the stack. We replace this error code with a memory 
** management fault code (e.g. UNA or ACV), save %eax--%edx 
** on the stack, and try to send a notification off to a user 
** domain.
*/
#define idt_mmfault(nr,why)     \
	.align 16		; \
idt_##nr:			; \
	addl	$4, %esp	; \
        pushl   $why	        ; \
	pushl	%edx		; \
	pushl	%ecx		; \
	pushl	%ebx		; \
	pushl	%eax		; \
        jmp     k_mmfault

	.align 16
k_mmfault:
	movl	24(%esp), %eax
	andl	$0xffff, %eax           /* chop off high bits           */
	movl	$k_st, %ebx
	movl	%eax, KST_CS(%ebx)      /* save it */
	cmpw	$KERNEL_CS, %ax         /* are we in the kernel?        */
	je	ntsc_mmgmt              /* if not, continue below       */

        /* Find current DCBRO */
	movl	KST_DCB(%ebx), %eax
	testl	%eax, %eax              /* Is it zero (null)?           */
	jz      ntsc_mmgmt              /* If so => ntsc debugger       */

        /* DCB not null => get the DCBRW */
	movl	DCB_A_DCBRW(%eax), %ecx
	movl	DCBRW_L_MODE(%ecx), %ebx  /* Load activation mode       */
	testl	%ebx, %ebx                /* Activations on?            */
	jnz     ntsc_mmgmt                /* If not => ntsc debugger    */

	/* Get the activation context for save */
	movl	DCBRW_L_ACTCTX(%ecx), %ebx
	movl	DCB_L_NUMCTXTS(%eax), %edx
	cmpl	%edx, %ebx
	jge	ntsc_mmgmt	          /* Context slot out of range */

	movl	DCB_A_CTXTS(%eax), %eax
	shll	$6, %ebx	
	leal	(%eax, %ebx, 4), %eax
	movl	$CX_VALID, %ebx

	/* Ready to save context */
	/* eax=context_t *   ebx=xxx  ecx=dcb_rw  edx=xxx */
	movl	PVS_DCBRW_OFFSET(%ecx), %edx
	movl	%edx, CX_PVS *4(%eax)
        call    do_cx_save

	/* Get back rop in %eax, rwp in %ecx */
	movl	k_st + KST_DCB, %eax
	movl	DCB_A_DCBRW(%eax), %ecx

	movl    DCBRW_L_MMEP(%ecx), %ebx  /* Load the mmgmt endpoint    */
#define NULL_EP	-1
	cmpl    $NULL_EP, %ebx            /* Has it been inited?        */
        je      ntsc_mmgmt                /* If not, bail to debugger   */


	/* Acts on, mmep ok. Store code & va, and dispatch event.       */
	movl    16(%esp), %edx
        movl    %edx, DCBRW_L_MMCODE(%ecx)
	movl    20(%esp), %edx
        movl    %edx, DCBRW_L_MMVA(%ecx)
        movl    $0, DCBRW_L_MMSTR(%ecx) 


        pushl   %ebx
        pushl   %eax
        call    k_dispatch_fault

/* 
** This is a special stub for the page fault handler.
** When entered, we have an error code on the stack which 
** tells us whether we have had a protection or a paging (i.e. 
** non-resident page) error. If the former, we try to patch 
** it up in C. If this fails, or if it is a paging error, then 
** we try to notify the faulting domain. If *this* fails, we
** enter the NTSC debugger. 
 */
	.align 16
idt_14:
	pushl	%edx
	pushl	%ecx
	pushl	%ebx
	pushl	%eax

	movl    16(%esp), %eax        /* get the error code */
	testl   $1, %eax              /* is it a protection error? */
	jnz     ntsc_fiddle           /* if so, try to patch it up */

	// Here we've taken a page-not-present fault: try to punt it to dom
	movl    $MM_K_PAGE, 16(%esp)  /* patch up error code */
	movl	28(%esp), %eax        /* get the calling CS  */
	andl	$0xffff, %eax         /* chop off high bits  */
	movl	$k_st, %ebx
	movl	%eax, KST_CS(%ebx)    /* save it for later reference */
	cmpw	$KERNEL_CS, %ax       /* are we in the kernel?       */
	je	ntsc_mmgmt            /* if so, bail into debugger   */

1:
        /* Find current DCBRO */
	movl	KST_DCB(%ebx), %eax
	testl	%eax, %eax              /* Is it zero (null)?           */
	jz      ntsc_mmgmt              /* If so => ntsc debugger       */

        /* DCB not null => get the DCBRW */
	movl	DCB_A_DCBRW(%eax), %ecx
	movl	DCBRW_L_MODE(%ecx), %ebx  /* Load activation mode       */
	testl	%ebx, %ebx                /* Activations on?            */
	jnz     ntsc_mmgmt                /* If not => ntsc debugger    */

4:	
	/* Get the activation context for save */
	movl	DCBRW_L_ACTCTX(%ecx), %ebx
	movl	DCB_L_NUMCTXTS(%eax), %edx
	cmpl	%edx, %ebx                /* Context slot out of range? */
	jge	ntsc_mmgmt	          /* If so => ntsc debugger     */

5:		
	movl	DCB_A_CTXTS(%eax), %eax
	shll	$6, %ebx	
	leal	(%eax, %ebx, 4), %eax
	movl	$CX_VALID, %ebx

	/* Ready to save context */
	/* eax=context_t *   ebx=xxx  ecx=dcb_rw  edx=xxx */
	movl	PVS_DCBRW_OFFSET(%ecx), %edx
	movl	%edx, CX_PVS *4(%eax)
        call    do_cx_save

	/* Get back rop in %eax, rwp in %ecx */
	movl	k_st + KST_DCB, %eax
	movl	DCB_A_DCBRW(%eax), %ecx

	movl    DCBRW_L_MMEP(%ecx), %ebx  /* Load the mmgmt endpoint    */
#define NULL_EP	-1
	cmpl    $NULL_EP, %ebx            /* Has it been inited?        */
        je      ntsc_mmgmt                /* If not, bail to debugger   */

	/* Acts on, mmep ok. Store code & va, and dispatch event.       */
        movl    $MM_K_ACV, %edx           /* assume ACV                 */
	movl    16(%esp), %ebp            /* get the error code again   */
	testl   $1, %ebp                  /* is it a protection error?  */
        jnz     2f                        /* if so, %edx is fine        */
        movl    $MM_K_PAGE, %edx          /* otherwise => a page fault  */
2:
        movl    %edx, DCBRW_L_MMCODE(%ecx)
	movl    %cr2, %edx
        movl    %edx, DCBRW_L_MMVA(%ecx)

        /* XXX SMH: Lookup the stretch in the sstab */
        movl    $0, DCBRW_L_MMSTR(%ecx) 

        pushl   %ebx
        pushl   %eax
        call    k_dispatch_fault

ntsc_fiddle:
	pushl	%edi
	pushl	%esi
	pushl	%ebp
	movl	%cr2, %eax          /* cr2 = linear address we faulted on */
	pushl	%eax
	call	k_prot_fault
	addl    $4, %esp            /* skip over the linear address */
	popl	%ebp
	popl	%esi
	popl	%edi
	testl   %eax, %eax          /* check return val                */
        jnz     1f                  /* If 'true', we're ok             */
	movl    $MM_K_ACV, 16(%esp) /* else: patch up the error code   */
	jmp     k_mmfault           /* go try to dispatch it to domain */
1:
	popl	%eax
	popl	%ebx
	popl	%ecx
	popl	%edx
	/* Add 4 to esp to dispose of error code */
	addl	$4, %esp
	iret
  	
	/* Get here if something goes wrong in our attempt to dispatch
           the fault to user land. In all cases we just store the ctxt
           in $bad_cx, and bail into k_mmgmt.
	   XXX SMH: later on want it so that only faults within NTSC 
	   cause NTSC debugger to be entered. If a domain faults with
	   acts off, for example, we can't communicate with it but 
	   we could just kill it.  */
ntsc_mmgmt:
	movl	$bad_cx, %eax           /* otherwise, fudge a context */
	call	do_cx_save              /* save context               */
	addl	$16, %esp
        call    k_mmgmt                 /* bail to ntsc debugger      */



  
/* k_irq (int irq_nr <0..15>); */
#define idt_interrupt(nr)	\
	.align 16		; \
idt_##nr:			; \
	pushl	$0 /* Fake error code */; \
	pushl	%edx		; \
	pushl	%ecx		; \
	pushl	%ebx		; \
	pushl	%eax		; \
	call	save_context	; \
	addl	$16, %esp	; \
	pushl	$(nr-0x20)	; \
	call	k_irq
	
/* k_syscall (int nr); */
#define idt_syscall(nr)	\
	.align 16		; \
idt_##nr:			; \
	pushl	$0 /* Fake error code */; \
	pushl	%edx		; \
	pushl	%ecx		; \
	pushl	%ebx		; \
	pushl	%eax		; \
	call	save_context	; \
	addl	$16, %esp	; \
	pushl	$(nr-0x30)	; \
	call	k_syscall

/* k_sc_whatever (void); */
/* XXX assumes that the system call entered this way does NOT edit the
  current context to return values */
#define idt_syscall_d(nr,name)	\
	.align 16		; \
idt_##nr:			; \
	pushl	$0 /* Fake error code */; \
	pushl	%edx		; \
	pushl	%ecx		; \
	pushl	%ebx		; \
	pushl	%eax		; \
	call	save_context	; \
	pushl	%edi		; \
	pushl	%esi		; \
	pushl	%ebp		; \
	call	name		; \
	popl	%ebp		; \
	popl	%esi		; \
	popl	%edi		; \
	popl	%eax		; \
	popl	%ebx		; \
	popl	%ecx		; \
	popl	%edx		; \
	addl	$4, %esp	; \
	iret
	
/* 
	callpriv dispatcher: 
	
	This looks up the specified callpriv vector in the kernel
	dispatch table and calls it with the following arguments:
	
	state (stored when callpriv registered) 
	dcb_rop of calling domain 
	3 user-defined arguments (passed by caller in ebx, ecx, edx)

	If a vector out of range or an unused vector is given, the
	kernel routine k_bad_callpriv will be called. Detection of
	unused vectors relies on: 
	
	a) k_init() filling each entry of the callpriv_dispatch
	structure with the pair 
	
	(<callpriv_unused_vector>, entry number)
	
	b) the callpriv allocator running in the Nemesis domain
	restoring this value when a vector is released.

	If desired, the allocator could replace callpriv_unused_vector
	with a privileged section of its own which performs some
	action when an unused vector is called - burying it in here
	just helps keep the allocator portable.
*/
			
	.align 16
idt_75:			
	cmpl	$N_CALLPRIVS, %eax	; /* In valid range? */
	jge     callpriv_out_of_range	
	pushl	k_st + KST_DCB          ;  /* dcb */
	pushl   %edi			;  /* arg4 */	
	pushl   %edx			;  /* arg3 */
	pushl   %ecx			;  /* arg2 */
	pushl   %ebx			;  /* arg1 */
	movl	k_st + KST_CALLPRIV, %ebx; 
	lea	(%ebx, %eax, 8), %eax	; /* callpriv entry */
	pushl   4(%eax)			; /* state */ 
	call	*(%eax)			; /* vector */
	addl	$24, %esp		
	iret

	.align 16
	.globl callpriv_unused_vector	
callpriv_unused_vector:			
	movl	4(%esp), %eax		; /* retrieve vector num */ 
	addl	$28, %esp		; /* pull garbage off stack */ 
	/* XXX fall through to callpriv_out_of_range */ 
callpriv_out_of_range:			; /* Vector is out of range */
	pushl	$0			; /* fake error code */
	pushl	%edx			
	pushl	%ecx			
	pushl	%ebx			
	pushl	%eax			
	call	save_context		
	addl	$16, %esp
	call	k_bad_callpriv

/* Custom 'send' syscall stub */
	.align 16
idt_54:
	pushl	%edi
	pushl	%esi
	pushl	%ebp /* Is this necessary? */
	pushl	%edx
	pushl	%ecx /* How about this? */
	pushl	%ebx
	pushl	%eax
	call	k_dsc_send
	addl	$4, %esp /* Don't restore eax - retval */
	popl	%ebx
	popl	%ecx
	popl	%edx
	popl	%ebp
	popl	%esi
	popl	%edi
	iret

 		
	
	
		
#define idt_ticker(nr)
#define idt_pagefault(nr)
#define idt_fpdisabled(nr)
#define idt_callpriv(nr)
#define idt_syscall_send(nr)

		
#if 0
/* This is a special stub for the ticker interrupt. There's only going to be
   one of these, so we may as well put it in the text segment now. */
	.align 16
idt_32:
	pushl	$0
	pushl	%edx
	pushl	%ecx
	pushl	%ebx
	pushl	%eax
	movb	$0x20, %al
	outb	%al, $0x20	/* Ack interrupt */
	movl	INFO_PAGE_ADDRESS+PIP_TIMELSW, %eax
	movl	INFO_PAGE_ADDRESS+PIP_TIMEMSW, %ebx
	movl	tst, %ecx
	movl	tst+4, %edx
	addl	$TICKER_INTERVAL, %eax
	adcl	$0, %ebx
	movl	%eax, INFO_PAGE_ADDRESS+PIP_TIMELSW
	movl	%ebx, INFO_PAGE_ADDRESS+PIP_TIMEMSW
	/* Do we need to reschedule? */
	cmpl	%edx, %ebx
	jb	1f
	cmpl	%ecx, %eax
	jb	1f
	/* Yes. */
	call	save_context
	movl	INFO_PAGE_ADDRESS+PIP_DCBRO, %eax
	pushl	%eax
	pushl	$k_st
	call ksched_scheduler
	/* Doesn't return */
1:	popl	%eax
	popl	%ebx
	popl	%ecx
	popl	%edx
	addl	$4, %esp
	iret
#endif /* 0 */


#if 0
/* This is the device-unavailable exception handler, now unused */
	.align 16
idt_7:
	pushl %eax
	/* Someone tried to use the fpu, but it's disabled */
	/* I hate to have to do this here, but we should really set a flag
	 * in their DCB to say that the domain now has floating point
	 * enabled */
	movl INFO_PAGE_ADDRESS+PIP_DCBRO, %eax
	movl $1, DCB_L_FEN(%eax)
	/* Disable alignment checks too */
	andl $~EFLAG_AC, 12(%esp)
	/* First things first: enable it */
	clts
	movl fp_save_cx, %eax
	fnsave CX_FPREG*4(%eax) /* don't check for fp exceptions */
	orl $CXFLAG_FPVALID, CX_CXFLAG*4(%eax)

	/* At this point the fpu is clear. Check the current 'restore'
	   context; if it has valid fp state then restore it */
	movl fp_restore_cx, %eax
	testl $CXFLAG_FPVALID, CX_CXFLAG*4(%eax)
	je 1f
	frstor CX_FPREG*4(%eax)
	/* At this point an fp exception may be raised. We are already in
	   the right domain, so any exception delivery mechanism will work.
	   If the exception handler returns we'll dtrt as well. Wow. */
1:	
        popl %eax
	iret
#endif /* 0 */

/* Calling convention for save_context:
	eax--edx may be trashed
	current flags may be trashed
	esp points to:
		ss3				40
		esp3				36
		flags				32
		cs		         	28
		eip				24
		error code	(ignored)	20
		edx				16
		ecx				12
		ebx				8
		eax				4
		return address			0
 */
	.align 16
save_context:
#ifdef NTSC_PROFILE
	/* Store our entry time in the kernel state structure */
	rdtsc
	movl	%eax, k_st+KST_KTIME
#endif /* NTSC_PROFILE */
	/* Check CS and jump to user save code if it's not KERNEL_CS */
	movl	28(%esp), %eax
	andl	$0xffff, %eax /* chop off high bits */
	movl	$k_st, %ebx
	movl	%eax, KST_CS(%ebx) /* save it */
	cmpw	$KERNEL_CS, %ax
	jne	1f
	movl	$bad_cx, %eax
	jmp	do_cx_save        /* save context */

1:	/* User-land code segment selector */
        /* Find current DCBRO */
	movl	k_st + KST_DCB, %eax
        /* Is it zero (null)? */
	testl	%eax, %eax
	jnz	1f	
	/* cur_dcb is null, so this is probably a system call from Primal */
	movl	$bad_cx, %eax
	jmp	do_cx_save

1:      /* DCB not null => calculate context slot address */
	movl	k_st + KST_DCB, %eax
	movl	DCB_A_DCBRW(%eax), %ecx
/* Activations on? */
	movl	DCBRW_L_MODE(%ecx), %ebx
	testl	%ebx, %ebx
	movl	DCBRW_L_RESCTX(%ecx), %ebx
	jnz	1f
/* Activations on */
	movl	DCBRW_L_ACTCTX(%ecx), %ebx
1:
	movl	DCB_L_NUMCTXTS(%eax), %edx
	cmpl	%edx, %ebx
	jge	3f	/* Context slot out of range */
/* Calculate address of context_t to use */
	/* eax=dcb_ro  ebx=context slot  ecx=dcb_rw  edx=xxx */
	movl	DCB_A_CTXTS(%eax), %eax
	shll	$6, %ebx	/* ebx:=ebx*64 */
	leal	(%eax, %ebx, 4), %eax
			/* eax:=eax+(ebx*4) */
	movl	$CX_VALID, %ebx
1:	/* Ready to save context */
	/* eax=context_t *   ebx=xxx  ecx=dcb_rw  edx=xxx */
	movl	PVS_DCBRW_OFFSET(%ecx), %edx
	movl	%edx, CX_PVS *4(%eax)
do_cx_save:
	/* jump here if there are no pervasives to save */
	/* eax=context_t *   ebx=xxx  ecx=xxx   edx=xxx */
#if 0
	/* If the fpu is enabled its state belongs in this context */
	movl	%cr0, %ebx
	testl	$CR0_TS, %ebx
	jne	5f
	/* If we get here, the fpu is enabled */
	movl	%eax, fp_save_cx
#endif /* 0 */
	fnsave	CX_FPREG*4(%eax)
5:	
	movl	$k_st, %edx
	movl	%eax, KST_CCXS(%edx)
	movl	4(%esp), %ecx  /* EAX */
	movl	%ecx, CX_EAX *4(%eax)
	movl	8(%esp), %ecx  /* EBX */
	movl	%ecx, CX_EBX *4(%eax)
	movl	12(%esp), %ecx /* ECX */
	movl	%ecx, CX_ECX *4(%eax)
	movl	16(%esp), %ecx /* EDX */
	movl	%ecx, CX_EDX *4(%eax)
	movl	%esi, CX_ESI *4(%eax)
	movl	%edi, CX_EDI *4(%eax)
	movl	%ebp, CX_EBP *4(%eax)
	/* Skip error code */
	movl	24(%esp), %ecx /* EIP */
	movl	%ecx, CX_EIP*4(%eax)
	/* Skip CS - it's saved at the start */
	movl	32(%esp), %ecx /* FLAGS */
	movl	%ecx, CX_EFLAG*4(%eax)
	movl	36(%esp), %ecx /* ESP3 */
	movl	%ecx, CX_ESP*4(%eax)
	ret

3:	/* Context slot out of range, save in bad_cx */
	movl	$k_st, %eax
	movl	%ebx, KST_BAD_SLOT(%eax)
	movl	%edx, KST_BAD_NCXS(%eax)
	xorl	%ecx, %ecx
	movl	%ecx, KST_CS(%eax)
	movl	$bad_cx, %eax
	jmp	do_cx_save /* can't save pervasives */
	

/* ------------------------------ */
/*
** Create interrupt handler stubs
**
*/
	idt_exception (0)
	idt_exception (1)
	idt_exception (2)
	idt_exception (3)
	idt_exception (4)
	idt_exception (5)
	idt_exception (6)
	idt_exception (7) /* Used to be FP disabled; now always an error */
	idt_exception (8)
	idt_exception (9)	
	idt_exception_err (10)
	idt_exception_err (11)
	idt_mmfault (12, MM_K_ACV)  
	idt_mmfault (13, MM_K_INST)
	idt_pagefault(14)          /* custom stub above */
	idt_exception (15)
	idt_exception (16)
	idt_mmfault (17, MM_K_UNA)
	idt_exception (18)

	idt_interrupt (32)
	idt_interrupt (33)
	idt_interrupt (34)
	idt_interrupt (35)
	idt_interrupt (36)
	idt_interrupt (37)
	idt_interrupt (38)
	idt_interrupt (39)
	idt_interrupt (40)
	idt_interrupt (41)
	idt_interrupt (42)
	idt_interrupt (43)
	idt_interrupt (44)
	idt_interrupt (45)
	idt_interrupt (46)
	idt_interrupt (47)

	/* First have the public system calls */
	idt_syscall (48)                    // Reboot
	idt_syscall_d (49, k_sc_rfa)        // RFA 
	idt_syscall_d (50, k_sc_rfa_resume) // RFA_Resume
	idt_syscall_d (51, k_sc_rfa_block)  // RFA_Block
	idt_syscall_d (52, k_sc_block)      // Block 
	idt_syscall_d (53, k_sc_yield)      // Yield
	idt_syscall_send (54)               // Send              
	idt_syscall (55)                    // Map
	idt_syscall (56)                    // Trans
	idt_syscall (57)                    // Prot
	idt_syscall (58)                    // Unmap
	idt_syscall (59)                    // Putcons
	idt_syscall (60)                    // Dbgstop
	idt_syscall (61)                    // XXX Unused
	idt_syscall (62)                    // XXX Unused
	idt_syscall (63)                    // XXX Unused

  
	/* And now the private system calls */
	idt_syscall (64)                    // Swpipl
	idt_syscall (65)                    // Unmask IRQ
	idt_syscall (66)                    // Kevent
	idt_syscall (67)                    // Actdom
	idt_syscall (68)                    // Write Ptbr
	idt_syscall (69)                    // Flush TLB
	idt_syscall (70)                    // Init Paging
	idt_syscall (71)                    // Mask IRQ
	idt_syscall (72)                    // SetDbgReg
    	idt_syscall_d (73, k_psc_entkern)   // Entkern
	idt_syscall_d (74, k_psc_leavekern) // Leavekern 
	idt_callpriv (75)                   // Callpriv
	idt_syscall (76)                    // Chain 
	idt_syscall (77)                    // XXX Unused
	idt_syscall (78)                    // XXX Unused
	idt_syscall (79)                    // XXX Unused

/*
 * Merge the preset offsets with the kernel CS
 */
	.globl	init_idt
init_idt:
	movl	$N_INTERRUPTS, %ecx
	movl	$idt_table, %edi	/* offset into table */
	movw	%cs, %ax
	
1:
	movw	2(%edi), %bx		/* Load the upper part of offset */
	movw	%ax, 2(%edi)		/* put the kernel CS here	*/
	movw	%bx, 6(%edi)		/* merge with flags */		
	addl	$8, %edi
	loop	1b

	lidt	idt_table_desc
	ret

/* ------------------------------ */

/* XXX If the debugger is enabled it will scribble over the exception entries
 * in this table */

#.section .perm
idt_table_desc:
	.word	N_INTERRUPTS * 8
	.long	idt_table

	.align	4
	.globl	idt_table
idt_table:
	/* 0 */
	idt(0, IDT_EXCEPTION)		/* Divide by zero */
	idt(1, IDT_EXCEPTION)		/* Debug */
	idt(2, IDT_EXCEPTION)		/* NMI */
	idt(3, IDT_SYSCALL)		/* Breakpoint */
	/* 4 */
	idt(4, IDT_SYSCALL)		/* Overflow */
	idt(5, IDT_EXCEPTION)		/* Bound range exceeded */
	idt(6, IDT_EXCEPTION)		/* Invalid opcode */
	idt(7, IDT_EXCEPTION)		/* Device not available */
	/* 8 */
	idt(8, IDT_EXCEPTION)		/* Double fault exception */
	idt(9, IDT_EXCEPTION)		/* CoPro segment overrun */
	idt(10, IDT_EXCEPTION)		/* Invalid TSS exception */
	idt(11, IDT_EXCEPTION)		/* Segment not present */
	/* 12 */
	idt(12, IDT_EXCEPTION)		/* Stack fault */
	idt(13, IDT_EXCEPTION)		/* General protection */
	idt(14, IDT_EXCEPTION)		/* Page fault */
	noidt				/* Reserved to Intel (great) */
	/* 16 */
	idt(16, IDT_EXCEPTION)		/* Floating point error */
	idt(17, IDT_EXCEPTION)		/* Alignment check exception */
	idt(18, IDT_EXCEPTION)		/* Machine check exception */
	noidt
	/* 20 */
	noidt
	noidt
	noidt
	noidt
	/* 24 */
	noidt
	noidt
	noidt
	noidt
	/* 28 */
	noidt
	noidt
	noidt
	noidt
	/* 0x20 - interupts */
	idt(32, IDT_INTERRUPT)
	idt(33, IDT_INTERRUPT)
	idt(34, IDT_INTERRUPT)
	idt(35, IDT_INTERRUPT)
	/* 0x24 */
	idt(36, IDT_INTERRUPT)
	idt(37, IDT_INTERRUPT)
	idt(38, IDT_INTERRUPT)
	idt(39, IDT_INTERRUPT)
	/* 0x28 */
	idt(40, IDT_INTERRUPT)
	idt(41, IDT_INTERRUPT)
	idt(42, IDT_INTERRUPT)
	idt(43, IDT_INTERRUPT)
	/* 0x2C */
	idt(44, IDT_INTERRUPT)
	idt(45, IDT_INTERRUPT)
	idt(46, IDT_INTERRUPT)
	idt(47, IDT_INTERRUPT)

  	/* 0x30 - public system calls */
	idt(48, IDT_SYSCALL) 
	idt(49, IDT_SYSCALL) 
	idt(50, IDT_SYSCALL) 
	idt(51, IDT_SYSCALL) 
	/* 0x34 */
	idt(52, IDT_SYSCALL) 
	idt(53, IDT_SYSCALL) 
	idt(54, IDT_SYSCALL) 
	idt(55, IDT_SYSCALL) 
	/* 0x38 */
	idt(56, IDT_SYSCALL) 
	idt(57, IDT_SYSCALL) 
	idt(58, IDT_SYSCALL) 
	idt(59, IDT_SYSCALL) 
	/* 0x3C */
	idt(60, IDT_SYSCALL) 
	idt(61, IDT_SYSCALL) 
	idt(62, IDT_SYSCALL) 
	idt(63, IDT_SYSCALL) 

  	/* 0x40 - private system calls */
	idt(64, IDT_SYSCALL) 
	idt(65, IDT_SYSCALL) 
	idt(66, IDT_SYSCALL) 
	idt(67, IDT_SYSCALL)
	/* 0x44 */
	idt(68, IDT_SYSCALL) 
	idt(69, IDT_SYSCALL) 
	idt(70, IDT_SYSCALL) 
	idt(71, IDT_SYSCALL) 
	/* 0x48 */
	idt(72, IDT_SYSCALL) 
  	idt(73, IDT_SYSCALL) 
	idt(74, IDT_SYSCALL_PRIV) /* XXX leavekern == 0x4A is special */
	idt(75, IDT_CALLPRIV)     /* XXX callpriv  == 0x4B is special */
	/* 0x44 */
	idt(76, IDT_SYSCALL) 
	idt(77, IDT_SYSCALL) 
	idt(78, IDT_SYSCALL) 
	idt(79, IDT_SYSCALL) 

#endif /* __LANGUAGE_C */

/* End */
