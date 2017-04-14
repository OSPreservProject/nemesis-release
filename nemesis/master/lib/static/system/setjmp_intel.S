/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      lib/nemesis/ix86/setjmp.S
** 
** FUNCTIONAL DESCRIPTION:
** 
**      setjmp and longjmp for ix86
** 
** ID : $Id: setjmp_intel.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <jmp_buf.h>
#include <pip.h>
#include <dcb.off.h>

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
	movl	INFO_PAGE_ADDRESS+PIP_PVSPTR, %edi
	movl	(%edi), %ebx    # get pervasives ptr into ebx
	movl	%ebx, 24(%eax)  # pervasives pointer
#if 0
	movl	INFO_PAGE_ADDRESS+PIP_DCBRO, %edi
	testl	$1, DCB_L_FEN(%edi) # is fp enabled
	movl	$0, 28(%eax)
	jz	1f		# nope...
	movl	$1, 28(%eax)
	fsave	32(%eax)	# possible exception
#endif /* 0 */
1:	movl	4(%ebp), %edx	# return address
	movl	%edx, 20(%eax)
	movl	(%ebp), %edx	# callers ebp
	movl	%edx, 12(%eax)
	leal	8(%ebp), %edx	# esp upon return from setjmp
	movl	%edx, 16(%eax)
	movl	(%eax), %ebx	# restore ebx (callee saves)
	movl	8(%eax), %edi	# restore edi (callee saves)
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

	movl	24(%ecx), %esi  # get pervasives pointer back from jmpbuf
	movl	INFO_PAGE_ADDRESS+PIP_PVSPTR, %edi
	movl	%esi, (%edi)    # and restore it 
#if 0
	testl	$1, 28(%ecx)
	jz	1f
	frstor	32(%ecx)	# warning - exception may be raised
#endif /* 0 */
1:	movl	(%ecx),	%ebx
	movl	4(%ecx), %esi
	movl	8(%ecx), %edi
	movl	12(%ecx), %ebp
	movl	16(%ecx), %esp
	jmp	*20(%ecx)
	
	/* 
	 * _longjmp
	 *
	 */
	.globl	_longjmp
_longjmp:
	pushl	%ebp
	movl	%esp, %ebp
	movl	8(%ebp), %ecx	# jmp_buf
	movl	$1, %eax
	
	movl	24(%ecx), %esi  # get pervasives pointer back from jmpbuf
	movl	INFO_PAGE_ADDRESS+PIP_PVSPTR, %edi
	movl	%esi, (%edi)    # and restore it 
#if 0
	testl	$1, 28(%ecx)
	jz	1f
	frstor	32(%ecx)	# warning - exception may be raised
#endif /* 0 */
1:	movl	(%ecx),	%ebx
	movl	4(%ecx), %esi
	movl	8(%ecx), %edi
	movl	12(%ecx), %ebp
	movl	16(%ecx), %esp
	jmp	*20(%ecx)

#endif /* ndef __LANGUAGE_C */

/*
 * End lib/nemesis/ix86/setjmp.S
 */
