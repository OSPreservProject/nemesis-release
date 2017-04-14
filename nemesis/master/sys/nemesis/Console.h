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
**      Console
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Console output redirector
** 
** ENVIRONMENT: 
**
**      User space (probably System domain)
** 
** ID : $Id: Console.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef Console_h
#define Console_h

#include <ConsoleControl.ih>
#include <Wr.ih>

ConsoleControl_clp InitConsole(Wr_clp *console);

#endif /* Console_h */
