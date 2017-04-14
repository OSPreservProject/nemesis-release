/*
 *      ntsc/ix86/stub-support.c
 *      ------------------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * $Id: stub-support.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 *
 * This code supports the gdb stubs in Nemesis, along with i386-stub.c
 */

/* XXX This stealing of code from Linux just has to stop... */
#include <io.h>
#include "i386-stub.h"
#include "asm.h"
#include <kernel_tgt.h>

extern void uart_putch(uint8_t ch);
extern uint8_t uart_getch(void);

void init_debug(void)
{
    /* Call the stubs initialisation routine */
    set_debug_traps();
    /* Do we need to do anything else? Probably. But I'm not sure what. */
}


/* These should make sure that the UART is in a sane state too, but
 * it'll work for now */
int getDebugChar(void)
{
    int c;

    c = uart_getch();

    if (c == '\004')  /* ctrl-D */
    {
	const char *p = "\r\n\r\nRebooting...\r\n\r\n";
	while (*p)
	    uart_putch(*p++);

	k_reboot();
    }

    return c;
}

int putDebugChar(int c)
{
    uart_putch(c);
    return 1; /* Presumably this worked... */
}

void exceptionHandler(int nr, void *address)
{
    /* This is called by set_debug_traps() to install exception
     * handlers.  We need to write into the IDT here. Do we need to
     * tell the processor that we've done this at all? */

    idt_table[nr*2]&=0xffff0000;
    idt_table[nr*2]|=((unsigned int)address)&0xffff;
    idt_table[nr*2+1]&=0xffff;
    idt_table[nr*2+1]|=((unsigned int)address)&0xffff0000;
}    
