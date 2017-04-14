/*
 *	veneer.h
 *	--------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * ID : $Id: veneer_tgt.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 *
 * This provides library veneers for the INTEL platforms
 */

#include <pip.h>
#include <dcb.off.h>

/* For veeners where no closure is involved - just a jump table */

#define VENEER_NO_CL(name,entry,perv)		\
	.globl	name;				\
name:	movl	$INFO_PAGE_ADDRESS, %eax;	\
	movl	PIP_PVSPTR(%eax), %eax;		\
	movl	(%eax), %eax;			\
	movl	perv*4(%eax), %eax;		\
	movl	entry*4(%eax), %eax;		\
	jmp	*%eax

/* End of veneer.h */
