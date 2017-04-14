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
**      mod/nemesis/threads/thead_intel.S
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
** ID : $Id: thead_intel.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
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

	movl    INFO_PAGE_ADDRESS+PIP_PVSPTR, %eax
	movl	(%eax), %edi
	movl	12(%edi), %eax	/* data */
	pushl	%eax
	movl	8(%edi), %eax	/* entry */
	pushl	%eax
	movl	4(%edi), %eax	/* res */
	pushl	%eax
	movl	(%edi), %eax	/* ThreadTop */
	fninit
	pushl	$_thead		/* Fake return address */
	jmp	*%eax


	// Called from C with three args: (newsp, arg0, arg1)
	// => stack looks like:
	//                _________           ^
	//	      16 | arg1   |           |
	//            12 | arg0   |     Higher addresses 
	//             8 | newsp  |
	//             4 |  ebp   |     Lower Addresses 
	//       esp ->  | <FREE> |           |
	//               ----------           v
	.globl _run_next
_run_next:
	pushl	%ebp
	movl	%esp, %ebp	// Never return, so no need to save ebp
	movl	8(%ebp), %esp	// New stack pointer [newsp]
	pushl	16(%ebp)	// arg1 [vpp]
	pushl	12(%ebp)	// arg0 [st]
	pushl	$0xf00
//	pushl	4(%ebp)		// Return address XXX fake it?
	movl	$RunNext, %eax
	jmp	*%eax
	
#endif /* __LANGUAGE_C */
	
