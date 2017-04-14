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
**      console.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      console buffer management for NTSC
** 
** ENVIRONMENT: 
**
**      NTSC
** 
** ID : $Id: console.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _console_h_
#define _console_h_

#include <Domain.ih>

extern void console_init(void);
extern void console_putdom(Domain_ID dom);
extern void console_putbyte(uint8_t byte);
extern void console_putline(Domain_ID dom, string_t data);
extern void console_dump(void);

#define k_putchar(_c) console_putbyte((_c))

#endif /* _console_h_ */
