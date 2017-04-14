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
**      local.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Local VGA output
** 
** ENVIRONMENT: 
**
**      NTSC
** 
** ID : $Id: local.c 1.2 Tue, 25 May 1999 17:53:18 +0100 dr10009 $
** 
**
*/

#include <kernel.h>
#include <io.h>

static uint32_t x, y;

#define base (k_st.local_vga.baseaddr)
#define width (k_st.local_vga.width)
#define height (k_st.local_vga.height)

#define ATTR 0x4e

void bsod_init(string_t description)
{
    extern const char *k_ident;

    x=0; y=0;

    k_printf("Nemesis: System Crash: %s\n",description);
    k_printf("(kernel ID %s)\n",k_ident);
}

static void bsod_putchar(unsigned char c, uint8_t attr)
{
    int i;
    if (c>=32 && c<127) {
	writeb(c, base+(x+y*width)*2);
	writeb(attr, base+(x+y*width)*2+1);
	x++;
	if (x>=width) {
	    x=0; y++;
	}
    } else {
	switch (c) {
	case '\n':
	    x=0; y++;
	    if (y>height) y=0;
	    for (i=0; i<width; i++) {
		writeb(' ', base+(i+y*width)*2);
		writeb(attr, base+(i+y*width)*2+1);
	    }
	}
    }
}

void bsod_print(string_t line)
{
    while (*line) bsod_putchar(*line++, ATTR);
}

void local_putbyte(uint8_t byte)
{
    bsod_putchar(byte, ATTR);
}
