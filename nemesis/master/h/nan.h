/*  -*- Mode: C;  -*-
 * File: nan.h
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: `NAN' constant for IEEE 754 machines.
 **
 ** HISTORY:
 ** Created: Mon Jun 13 12:42:29 1994 (jch1003)
 ** Last Edited: Tue Jun 14 10:27:52 1994 By James Hall
 **
    $Id: nan.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $

 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef	__nan_h__

#define	__nan_h__	1


#include <nemesis_tgt.h>

/* IEEE Not A Number.  */

#ifdef BIG_ENDIAN
#define	__nan_bytes		{ 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 }
#endif
#ifdef LITTLE_ENDIAN
#define	__nan_bytes		{ 0, 0, 0, 0, 0, 0, 0xf8, 0x7f }
#endif

static const char __nan[8] = __nan_bytes;
#define	NAN	(*(const double *) __nan)

#endif	/* nan.h */

/*
 * end nan.h
 */
