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
**      crash.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Set up system to output crash dump data
** 
** ENVIRONMENT: 
**
**      NTSC
** 
** ID : $Id: crash.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <kernel.h>
#include "console.h"

extern void bsod_init(string_t description);

/* Set the kernel up for a crashdump of some kind */
void start_crashdump(string_t description)
{
    /* At this point, user space is dead. Disable console output buffering. */
    k_st.console.buffer_output = False;

    /* Are we dumping to local display? */
    if (k_st.crash.local) {
	k_st.console.local_output = True;
	bsod_init(description);
	k_st.console.local_output = False;
    }

    /* Are we dumping to serial? */
    if (k_st.crash.serial) {
	k_st.console.serial_output = True;
	k_printf("\n=====CRASH (%s)=====\n", description);
    }

    if (k_st.crash.local) {
	k_st.console.local_output = True;
    }

    /* k_printf() has now been redirected to appropriate places. */
    k_printf("\nBuffered console output:\n");
    console_dump();
    k_printf("\n");

    /* Now return to let whoever called us print a crash dump */
}

void end_crashdump(void)
{
    /* We definitely don't want to write anything else on the local
       display. If serial debugging is in use then we return to allow
       the caller to enter the debugger, otherwise we halt forever. */
    k_st.console.local_output = False;

    if (!k_st.crash.serial) {
	while(1);
    }
}
