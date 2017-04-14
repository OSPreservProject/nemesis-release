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
**      unistd.h
** 
** DESCRIPTION:
** 
**      Bits of the normal unistd.h
**      
** 
** ID : $Id: unistd.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _unistd_h_
#define _unistd_h_

__NORETURN(_exit(int status));
int on_exit(void (*function)(int , void *), void *arg);





#endif
