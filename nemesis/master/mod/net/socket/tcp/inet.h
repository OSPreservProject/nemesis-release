/**************************************************************************\
*          Copyright (c) 1995 INRIA Sophia Antipolis, FRANCE.              *
*                                                                          *
* Permission to use, copy, modify, and distribute this material for any    *
* purpose and without fee is hereby granted, provided that the above       *
* copyright notice and this permission notice appear in all copies.        *
* WE MAKE NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY OF THIS     *
* MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", WITHOUT ANY EXPRESS   *
* OR IMPLIED WARRANTIES.                                                   *
\**************************************************************************/
/**************************************************************************\
*                                                                          *
*  File              : inet.h                                              *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

#define LITTLE_ENDIAN
#include <netorder.h>
#include <iana.h>
#include <Net.ih>
#include <IOMacros.h>

#ifdef ALPHA
#define __alpha
#endif

#ifdef ALPHA
#undef __alpha
#endif

#include "tcpu.h"
#include "tcpu_seq.h"
#include "tcpu_timer.h"
#include "tcpu_var.h"
#include "event.h"

#ifdef ALPHA
#define __alpha
#endif

