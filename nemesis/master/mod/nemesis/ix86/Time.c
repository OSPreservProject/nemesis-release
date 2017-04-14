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
**      mod/nemesis/ix86/Time.c
** 
** FUNCTIONAL DESCRIPTION:
** 
** 	Implementation of Time.if using the PIP (Public Info Page)
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: Time.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <pip.h>
#include <Time.ih>

/*
 * Module stuff
 */
static	Time_Now_fn	Now_m;
static  Time_op		ms = { Now_m };
static const Time_cl		cl = { &ms, INFO_PAGE_ADDRESS };

CL_EXPORT (Time, Time, cl);

static Time_ns Now_m (Time_clp self)
{
#ifdef INTEL_SMP
    return INFO_PAGE.now;
#else
    /* Use function from pip.h */
    return time_inline(False);
#endif
}

/* End of Time.c */
