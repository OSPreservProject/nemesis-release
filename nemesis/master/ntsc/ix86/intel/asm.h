/*
 *      ntsc/ix86/asm.h
 *      ---------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This header contains declarations for subroutines written in assembler.
 *
 * $Id: asm.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

#ifndef ASM_H
#define ASM_H

#ifdef __LANGUAGE_C
#include <nemesis.h>
#include <context.h>
#endif

/* Types of context saved on exception or interrupt */
#define CX_VALID   0
#define CX_BADSLOT 1
#define CX_KERNEL  2

#ifdef __LANGUAGE_C

/* The IDT */
/* The _symbol_ is the address of the start of the table. This is not
 * a pointer. */
extern unsigned int idt_table[];

/* The nexus, accessed through _end */
extern char _end;

/* The start and end of the chain loader */
extern uint32_t chain_relocator, chain_relocator_end;
extern uint32_t gdt_desc, tss_end;

/* Reboot the machine */
extern NORETURN(re_enter_rom(void));

/* Resume a context */
extern NORETURN(k_resume(context_t *cx, uint32_t cs, uint32_t ds));

/* Jump to NemesisPrimal (resume with no pervasives restore) */
extern NORETURN(k_presume(context_t *cx, uint32_t cs, uint32_t ds));

extern void k_enable_paging(void);
extern void k_disable_paging(void);
extern void k_enable_pse(void); /* Enable page size extension */

extern void k_set_debug_registers(void *);
extern bool_t processor_cpuid_available(void);
extern void read_cpuid(int eax_in, void *out);

#endif /* __LANGUAGE_C */

#endif /* ASM_H */
