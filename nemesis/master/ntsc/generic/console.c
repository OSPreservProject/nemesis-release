/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      console.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      NTSC console management functions
** 
** ENVIRONMENT: 
**
**      NTSC
** 
** ID : $Id: console.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <kernel.h>
#include "console.h"

extern void local_putbyte(uint8_t byte);
extern void serial_putbyte(uint8_t byte);

static void console_putbufferbyte(uint8_t byte)
{
    k_st.console.buf[k_st.console.head++] = byte;
    if (k_st.console.head == CONSOLEBUF_SIZE) k_st.console.head = 0;
}

void console_putbyte(uint8_t byte)
{
    if (k_st.console.buffer_output) {
	console_putbufferbyte(byte);
    }
    if (k_st.console.local_output) {
	local_putbyte(byte);
    }
    if (k_st.console.serial_output) {
	serial_putbyte(byte);
    }
}

void console_putdom(Domain_ID dom)
{
    /* Not implemented */
}

/* Initialise the kernel console code. */
void console_init(void)
{
    k_st.console.head = 0;
    k_st.console.tail = 0;
    k_st.console.current_dom   = -1;
    k_st.console.buffer_output = True;
    k_st.console.local_output  = False;
    k_st.console.serial_output = False;
}

/* Output a line to the console. Records the domain that was the
   source of the line. */

void console_putline(Domain_ID dom, string_t data)
{
    console_putdom(dom);

    while (*data)
	console_putbyte(*data++);
}

static uint32_t console_getbyte(void)
{
    uint32_t r;
    if (k_st.console.tail == k_st.console.head) return -1; /* End of buffer */

    r = k_st.console.buf[k_st.console.tail++];

    if (k_st.console.tail == CONSOLEBUF_SIZE) k_st.console.tail = 0;

    return r;
}

void console_dump(void)
{
    uint32_t i;
    uint32_t j;
    Domain_ID d=-1;

    while ((i=console_getbyte()) != -1) {
	if (i == 0xff) {
	    /* Next eight bytes are a domain ID, lsb first */
	    d = 0;
	    for (j=0; j<8 && i!=-1; j++) {
		i   = console_getbyte();
		d <<= 8;
		d  |= i;
	    }
	    k_printf("[%qx]",d);
	} else {
	    console_putbyte(i);
	}
    }
}
