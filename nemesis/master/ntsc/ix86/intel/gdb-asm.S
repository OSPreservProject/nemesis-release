/* $Id: gdb-asm.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $ */

/* Public domain */

/* Restore the program's registers (including the stack pointer, which
 * means we get the right stack and don't have to worry about popping our
 * return address and any stack frames and so on) and return.  */

.text
	.globl return_to_prog
return_to_prog:
	movw registers+44, %ss
	movl registers+16, %esp
	movl registers+4, %ecx
	movl registers+8, %edx
	movl registers+12, %ebx
	movl registers+20, %ebp
	movl registers+24, %esi
	movl registers+28, %edi
	movw registers+48, %ds
	movw registers+52, %es
	movw registers+56, %fs
	movw registers+60, %gs
	movl registers+36, %eax
	pushl %eax  /* saved eflags */
	movl registers+40, %eax
	pushl %eax  /* saved cs */
	movl registers+32, %eax
	pushl %eax  /* saved eip */
	movl registers, %eax
	/* use iret to restore pc and flags together so
	 * that trace flag works right.  */
	iret

/* GDB stores segment registers in 32-bit words (that's just the way
   m-i386v.h is written).  So zero the appropriate areas in registers.  */
#define SAVE_REGISTERS1() \
	movl %eax, registers	; \
	movl %ecx, registers+4	; \
	movl %edx, registers+8	; \
	movl %ebx, registers+12	; \
	movl %ebp, registers+20	; \
	movl %esi, registers+24	; \
	movl %edi, registers+28	; \
	movw $0, %ax		; \
	movw %ds, registers+48	; \
	movw %ax, registers+50	; \
	movw %es, registers+52	; \
	movw %ax, registers+54	; \
	movw %fs, registers+56	; \
	movw %ax, registers+58	; \
	movw %gs, registers+60	; \
	movw %ax, registers+62

#define SAVE_ERRCODE() \
	popl %ebx		; \
	movl %ebx, gdb_i386errcode

#define SAVE_REGISTERS2() \
	popl %ebx /* old eip */	; \
	movl %ebx, registers+32	; \
	popl %ebx /* old cs */	; \
	movl %ebx, registers+40	; \
	movw %ax, registers+42	; \
	popl %ebx /* old eflags */ ; \
	movl %ebx, registers+36	; \
  /* Now that we've done the pops, we can save the stack pointer */ \
	movw %ss, registers+44	; \
	movw %ax, registers+46	; \
	movl %esp, registers+16


/* These functions are called when a i386 exception occurs.  They save
 * all the cpu regs in the _registers array, munge the stack a bit,
 * and invoke an exception handler (remcomHandler).
 *
 * stack on entry:                       stack on exit:
 *   old eflags                          vector number
 *   old cs (zero-filled to 32 bits)
 *   old eip
 *
 */
#define exception_no_error(nr) \
	.align 16		; \
	.globl catchException##nr ; \
catchException##nr:		; \
	SAVE_REGISTERS1()	; \
	SAVE_REGISTERS2()	; \
	pushl $nr		; \
	call remcomHandler

#define exception_error(nr) \
	.align 16		; \
	.globl catchException##nr ; \
catchException##nr:		; \
	SAVE_REGISTERS1()	; \
	SAVE_ERRCODE()		; \
	SAVE_REGISTERS2()	; \
	pushl $nr		; \
	call remcomHandler

#define exception_fault(nr) \
	.align 16		; \
	.globl catchException##nr ; \
catchException##nr:		; \
	SAVE_REGISTERS1()	; \
	SAVE_ERRCODE()		; \
	SAVE_REGISTERS2()	; \
	pushl $nr		; \
	call remcomHandler

.text
	exception_no_error (0)
	exception_no_error (1)
	exception_no_error (3)
	exception_no_error (4)
	exception_no_error (5)
	exception_no_error (6)
	exception_no_error (7)
	exception_error (8)
	exception_no_error (9)
	exception_error (10)
	exception_fault (11)
	exception_error (12)
	exception_fault (13)
	exception_fault (14)
	exception_no_error (16)

.text
remcomHandler:
	popl %eax	/* pop off return address */
	popl %eax	/* get exception number */
	movl stackPtr, %esp	/* move to remcom stack area */
	pushl %eax		/* pass argument */
	call handle_exception /* NORETURN */
