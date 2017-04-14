##########################################################################
#                                                                       #
#  Copyright 1998, University of Cambridge Computer Laboratory          #
#                                                                       #
#  All Rights Reserved.                                                 #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#   sys/primal
#
# DESCRIPTION:
#
#   Special case crt0 for primal. 
#
# $Id: pcrt0_ix86.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $

.text
	.globl	start
start:
	pushl   %edx	/* Kernel state pointer */
	pushl	%ecx	/* mem_size */
	pushl   %ebx	/* mem_start */
	pushl	%eax	/* Nexus pointer */
	xorl	%eax, %eax
	movl	%eax, %ebp
	call	Main	/* Doesn't return */
1:
	jmp	1b
