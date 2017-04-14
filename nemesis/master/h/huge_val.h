/*  -*- Mode: C;  -*-
 * File: huge_val.h
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: `HUGE_VAL' constant for IEEE 754 machines 
 **            (where it is infinity).
 **            Used by <stdlib.h> and <math.h> functions for overflow.

 **
 ** HISTORY:
 ** Created: Mon Jun 13 11:40:18 1994 (jch1003)
 ** Last Edited: Tue Jun 14 10:19:40 1994 By James Hall
 **
    $Id: huge_val.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */


#ifndef	   __huge_val_h__
#define	   __huge_val_h__	1

#include <nemesis_tgt.h>

/* IEEE positive infinity.  */

#ifdef BIG_ENDIAN
#define	__huge_val_bytes	{ 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 }
#endif
#ifdef LITTLE_ENDIAN
#define	__huge_val_bytes	{ 0, 0, 0, 0, 0, 0, 0xf0, 0x7f }
#endif

static const char __huge_val[8] = __huge_val_bytes;
#define	HUGE_VAL	(*(const double *) __huge_val)


#endif	   /* __huge_val_h__ */

/*
 * end huge_val.h
 */
