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
**      klogd.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Kernel log daemon
** 
** ENVIRONMENT: 
**
**      User space (privileged domain)
** 
** ID : $Id: klogd.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _klogd_h_
#define _klogd_h_

#include <nemesis.h>
#include <kernel_st.h>
#include <Wr.ih>

void start_console_daemon(kernel_st *kst, Wr_clp output);

#endif /* _klogd_h_ */
