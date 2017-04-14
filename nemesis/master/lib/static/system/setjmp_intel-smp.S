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
**      lib/nemesis/ix86/setjmp_intel-smp.S
** 
** FUNCTIONAL DESCRIPTION:
** 
**      setjmp and longjmp for Intel-SMP
** 
** ID : $Id: setjmp_intel-smp.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <jmp_buf.h>
#include <pip.h>
#include <dcb.off.h>
#include <smp.h>	

#ifndef __LANGUAGE_C			/* so gcc -M will work for .d	*/

	/* 
	 * setjmp
	 *
	 * Contexts saved by setjmp can only be restored by [_]longjmp.
	 * Contexts restored by [_]longjmp must be saved by setjmp.
	 * Therefore we needn't save or restore caller saves.
	 *
	 * Furthermore, we don't save/restore fp regs unless this domain
	 * has been misguided enough to use 'em.
	 */
	.globl	setjmp
setjmp:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %eax	# pointer to jmp_buf
	movl	%ebx, (%eax)	# store ebx
	movl	%esi, 4(%eax)	# store esi
	movl	%edi, 8(%eax)	# store edi
	movl	%edx, 12(%eax)  # store edx
	movl	%eax, %ebx	# save jmp_buf
	movl	(INFO_PAGE_MYCPU_ADDRESS + PIP_PCPU_DCBRW), %edx
	movl	PVS_DCBRW_OFFSET(%edx), %eax
	movl	%eax, 28(%ebx)  # pervasives pointer
	movl	(INFO_PAGE_MYCPU_ADDRESS + PIP_PCPU_DCBRO), %edi
	testl	$1, DCB_L_FEN(%edi) # is fp enabled
	movl	%ebx, %eax	# restore jmp_buf
	movl	$0, 32(%eax)
	jne	1f		# nope...
	movl	$1, 32(%eax)
	fsave	36(%eax)	# possible exception
1:	movl	4(%ebp), %edx	# return address
	movl	%edx, 24(%eax)
	movl	(%ebp), %edx	# callers ebp
	movl	%edx, 16(%eax)
	leal	8(%ebp), %edx	# esp upon return from setjmp
	movl	%edx, 20(%eax)
	movl	(%eax), %ebx	# restore ebx (callee saves)
	movl	8(%eax), %edi	# restore edi (callee saves)
	movl	12(%eax), %edx	# restore edx (callee saves)
	xorl	%eax, %eax	# return 0
	movl	%ebp, %esp
	popl	%ebp
	ret

	/* 
	 * longjmp
	 */
	.globl	longjmp
longjmp:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %ecx	# jmp_buf
	movl	12(%ebp), %eax	# val
	testl	%eax, %eax	# if val==0 then val:=1
	jne	1f
	movl	$1, %eax
1:
	movl	%eax, %ebx	# save %eax
	movl	(INFO_PAGE_MYCPU_ADDRESS+PIP_PCPU_DCBRW), %edi
	movl	28(%ecx), %esi
	movl	%esi, PVS_DCBRW_OFFSET(%edi) # restore pervasives
	testl	$1, 32(%ecx)
	je	1f
	frstor	36(%ecx)	# warning - exception may be raised
1:	movl	%ebx,	%eax	# restore %eax from above
	movl	(%ecx),	%ebx
	movl	4(%ecx), %esi
	movl	8(%ecx), %edi
	movl	12(%ecx), %edx
	movl	16(%ecx), %ebp
	movl	20(%ecx), %esp
	jmp	*24(%ecx)
	
	/* 
	 * _longjmp
	 *
	 */
	.globl	_longjmp
_longjmp:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %ecx	# jmp_buf
	movl	$1, %ebx

	movl	(INFO_PAGE_MYCPU_ADDRESS+PIP_PCPU_DCBRW), %edi
	movl	28(%ecx), %esi
	movl	%esi, PVS_DCBRW_OFFSET(%edi) # restore pervasives
	testl	$1, 32(%ecx)
	je	1f
	frstor	36(%ecx)	# warning - exception may be raised
1:	movl	%ebx, %eax	# val argument
	movl	(%ecx),	%ebx
	movl	4(%ecx), %esi
	movl	8(%ecx), %edi
	movl	12(%ecx), %edx
	movl	16(%ecx), %ebp
	movl	20(%ecx), %esp
	jmp	*24(%ecx)

#endif /* ndef __LANGUAGE_C */

/*
 * End lib/nemesis/ix86/setjmp.S
 */
