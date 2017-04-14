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
**      serial.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Trivial serial port driver
** 
** ENVIRONMENT: 
**
**      NTSC
** 
** ID : $Id: serial.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <kernel.h>
#include <io.h>

#define UART_BASEADDR (k_st.serial.baseaddr)
#define UART_BAUDRATE	12
#define UART_LCRVAL	0x03		/* B'00xx0011' */
#define UART_FCRVAL	0x6		/* B'00000110' */

/* Initialise a serial port. Only done at boot time if serial console is
   selected; if we use the serial port later we keep the options set by
   the user space driver. */
void init_serial(void)
{
    outb(0x80,          UART_BASEADDR + 3);             /* set dlab */
    outw(UART_BAUDRATE, UART_BASEADDR);
    outb(UART_LCRVAL,   UART_BASEADDR + 3);             /* clear dlab */
    outb(0x08,          UART_BASEADDR + 4);             /* MCR */
}

void uart_putch(uint8_t c)
{
    while(!(inb(UART_BASEADDR + 5) & 0x20));
    outb(c,             UART_BASEADDR);
    while(!(inb(UART_BASEADDR + 5) & 0x20));
}

uint8_t uart_getch(void)
{
    int y;
    
    while(!(y = inb(UART_BASEADDR + 5) & 0x01));
    return inb(UART_BASEADDR);
}

void serial_putbyte(uint8_t byte)
{
    if (byte=='\n') uart_putch('\r');
    uart_putch(byte);
}
