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
**      mod/nemesis/Threads/thead_intel-smp.S
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Thread entry point code.
** 
** ENVIRONMENT: 
** 
** 	The architecture specific _thead routine is entered via 
** 	_longjmp with a pointer to the jmp_buf in the pervasives
** 	register.  The first four words of the jmp_buf contain the 
** 	entry point of ThreadTop, and its three arguments.  The 
** 	assembler stub _thead is reqired to sort out any necessary 
** 	procedure call standards. e.g. the PV register on Alpha
** 
** Modified for SMP by M. Welsh (mdw@cs.cornell.edu)
**
** ID : $Id: thead_intel-smp.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <pip.h>
#include <dcb.off.h>

#ifndef __LANGUAGE_C			/* so gcc -M will work for .d	*/

	.text
	.globl 	_thead
_thead:		
	movl	%esp, %ebp
	movl  (INFO_PAGE_MYCPU_ADDRESS+PIP_PCPU_DCBRW), %eax
	movl	PVS_DCBRW_OFFSET(%eax), %edi
	movl	16(%edi), %eax	/* data */ /* XXX mdw used to be 12 */
	pushl	%eax
	movl	8(%edi), %eax	/* entry */
	pushl	%eax
	movl	4(%edi), %eax	/* res */
	pushl	%eax
	movl	(%edi), %eax	/* ThreadTop */
	pushl	$_thead		/* Fake return address */
	jmp	%eax

	
#endif /* __LANGUAGE_C */
	
