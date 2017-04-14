/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      abort.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      provide a minimal abort() function which causes a protection fault
** 
** ENVIRONMENT: 
**
**      Lib/c
** 
** ID : $Id: abort.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>

#include <stdlib.h>
#include <stdio.h>

void abort(void)
{
    eprintf("abort called\n");
    *(char *)BADPTR = 1;

    /* paranoia */
    eprintf("abort failed - halting\n");
    ntsc_dbgstop();
    for(;;)
	;
}

/* End of abort.c */
