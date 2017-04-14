/*
 *      lib/nemesis/ix86/crt0.S
 *      -----------------------
 *
 * In the new scheme of things, the Builder expects the start of a program
 * to be a closure with one operation.
 *
 * $Id: crt0_ix86.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

.text
	.globl	_start
	.globl	Main
	.globl	__closure
_start:
__closure:
	.long	__entry_point
	.long	0xdeadbeef
__entry_point:
	.long	Main

