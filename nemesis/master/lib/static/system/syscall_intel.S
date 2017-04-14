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
**      lib/nemesis/ix86/syscall.s
** 
** FUNCTIONAL DESCRIPTION:
** 
** 	Assembler stubs for system calls.
** 	
** ENVIRONMENT: 
**
**      intel user-land.
** 
** ID : $Id: syscall_intel.S 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <syscall.h>
	
#ifndef __LANGUAGE_C			/* so gcc -M will work for .d	*/

#define SYSNOARGS(name, num)                    \
	.align 16;                              \
	.globl name;                            \
name:						\
	int $##num;				\
	ret


#define SYS1ARG(name, num)                      \
	.align 16;                              \
	.globl name;                            \
name:						\
	pushl %ebp;				\
	movl %esp, %ebp;			\
	movl 8(%ebp), %eax;			\
	int $##num;				\
	movl %ebp, %esp;			\
	popl %ebp;				\
	ret


#define SYS2ARGS(name, num)                     \
	.align 16;                              \
	.globl name;                            \
name:						\
	pushl %ebp;				\
	movl %esp, %ebp;			\
	pushl %ebx;				\
	movl 8(%ebp), %eax;			\
	movl 12(%ebp), %ebx;			\
	int $##num;				\
	popl %ebx;				\
	movl %ebp, %esp;			\
	popl %ebp;				\
	ret

#define SYS3ARGS(name, num)                     \
	.align 16;                              \
	.globl name;                            \
name:						\
	pushl %ebp;				\
	movl %esp, %ebp;			\
	pushl %ebx;				\
	pushl %ecx;				\
	movl 8(%ebp), %eax;			\
	movl 12(%ebp), %ebx;			\
	movl 16(%ebp), %ecx;			\
	int $##num;				\
	popl %ecx;				\
	popl %ebx;				\
	movl %ebp, %esp;			\
	popl %ebp;				\
	ret

#define SYS4ARGS(name, num)                     \
	.align 16;                              \
	.globl name;                            \
name:						\
	pushl %ebp;				\
	movl %esp, %ebp;			\
	pushl %ebx;				\
	pushl %ecx;				\
	pushl %edx;				\
	movl 8(%ebp), %eax;			\
	movl 12(%ebp), %ebx;			\
	movl 16(%ebp), %ecx;			\
	movl 20(%ebp), %edx;			\
	int $##num;				\
	popl %edx;				\
	popl %ecx;				\
	popl %ebx;				\
	movl %ebp, %esp;			\
	popl %ebp;				\
	ret

#define SYS5ARGS(name, num)                     \
	.align 16;                              \
	.globl name;                            \
name:						\
	pushl %ebp;				\
	movl %esp, %ebp;			\
	pushl %ebx;				\
	pushl %ecx;				\
	pushl %edx;				\
	pushl %edi;				\
	movl 8(%ebp), %eax;			\
	movl 12(%ebp), %ebx;			\
	movl 16(%ebp), %ecx;			\
	movl 20(%ebp), %edx;			\
	movl 24(%ebp), %edi;			\
	int $##num;				\
	popl %edi;				\
	popl %edx;				\
	popl %ecx;				\
	popl %ebx;				\
	movl %ebp, %esp;			\
	popl %ebp;				\
	ret

	
#define SYS1ARGL(name,num)			\
	.align 16;                              \
	.globl name;                            \
name:						\
	pushl %ebp;				\
	movl %esp, %ebp;			\
	pushl %ebx;				\
	movl 8(%ebp), %eax;			\
	movl 12(%ebp), %ebx;			\
	int $##num;				\
	popl %ebx;				\
	movl %ebp, %esp;			\
	popl %ebp;				\
	ret
		
.text
/* Public system calls */
SYSNOARGS(ntsc_reboot,     NTSC_REBOOT_ENTRY)
SYSNOARGS(ntsc_dbgstop,    NTSC_DBGSTOP_ENTRY)
SYSNOARGS(ntsc_rfa,        NTSC_RFA_ENTRY)       
SYS1ARG  (ntsc_rfa_resume, NTSC_RFA_RESUME_ENTRY)
SYS1ARGL (ntsc_rfa_block,  NTSC_RFA_BLOCK_ENTRY) 
SYS1ARGL (ntsc_block,      NTSC_BLOCK_ENTRY)     
SYSNOARGS(ntsc_yield,      NTSC_YIELD_ENTRY)     
SYS2ARGS (ntsc_send,       NTSC_SEND_ENTRY)      
SYS3ARGS (ntsc_map,        NTSC_MAP_ENTRY)       
SYS3ARGS (ntsc_nail,       NTSC_NAIL_ENTRY)     
SYS4ARGS (ntsc_prot,       NTSC_PROT_ENTRY)      
SYS3ARGS (ntsc_gprot,      NTSC_GBLPROT_ENTRY)      
SYS1ARG  (ntsc_putcons,    NTSC_PUTCONS_ENTRY)    
  
/* Private system calls */
SYS1ARG  (ntsc_swpipl,     NTSC_SWPIPL_ENTRY)    
SYS1ARG  (ntsc_unmask_irq, NTSC_UNMASK_IRQ_ENTRY)
SYS3ARGS (ntsc_kevent,     NTSC_KEVENT_ENTRY)    
SYS2ARGS (ntsc_actdom,     NTSC_ACTDOM_ENTRY)    
SYS3ARGS (ntsc_wptbr,      NTSC_WPTBR_ENTRY)     
SYSNOARGS(ntsc_flushtlb,   NTSC_FLUSHTLB_ENTRY)  
SYS1ARG  (ntsc_wrpdom,     NTSC_WRPDOM_ENTRY)  
SYS1ARG  (ntsc_mask_irq,   NTSC_MASK_IRQ_ENTRY)  
SYS1ARG  (ntsc_setdbgreg,  NTSC_SETDBGREG_ENTRY) 
SYSNOARGS(ntsc_entkern,    NTSC_ENTKERN_ENTRY)   
SYSNOARGS(ntsc_leavekern,  NTSC_LEAVEKERN_ENTRY) 
SYS5ARGS (ntsc_callpriv,   NTSC_CALLPRIV_ENTRY)  
SYS3ARGS (ntsc_chain,      NTSC_CHAIN_ENTRY)     
SYS3ARGS (ntsc_evsel,      NTSC_EVSEL_ENTRY)     
	 	
#endif /* ! __LANGUAGE_C */

/* End lib/nemesis/intel/syscall.S */
