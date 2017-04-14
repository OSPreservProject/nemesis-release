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
**      misc
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Miscellaneous tasks in the Nemesis domain
** 
** ENVIRONMENT: 
**
**      Nemesis domain
** 
** ID : $Id: misc.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef misc_h
#define misc_h

#include <NTSC.ih>
#include <Interrupt.ih>
#include <CallPriv.ih>
#include <ModData.ih>

extern void StartTrader(void);
extern NTSC_clp NTSC_access_init(kernel_st *st);
extern Interrupt_clp InterruptInit(kernel_st *kst);
extern CallPriv_clp CallPrivInit(addr_t kst);
extern ModData_clp ModDataInit(void);

#endif /* misc_h */
