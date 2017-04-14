/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
**
** ID : $Id: beep.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <io.h>
#include <Beep.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static	Beep_On_fn 		Beep_On_m;
static	Beep_Off_fn 		Beep_Off_m;

static	Beep_op	ms = {
    Beep_On_m,
    Beep_Off_m
};

static const Beep_cl cl = { &ms, NULL };

CL_EXPORT (Beep, Beep, cl);

/*---------------------------------------------------- Entry Points ----*/

static void Beep_On_m (
    Beep_cl	*self,
    uint32_t freq )
{
    uint32_t f;

    f=1193180/freq;

    /* Set frequency */
    outb(0xb6, 0x43);
    outb(f&0xff, 0x42);
    outb((f>>8)&0xff,0x42);

    /* Switch on gate */
    outb(inb(0x61)|3,0x61);
}

static void Beep_Off_m (
    Beep_cl	*self )
{
    /* Switch off gate */
    outb(inb(0x61)&0xfc,0x61);
}

/*
 * End 
 */
