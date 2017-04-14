/*  -*- Mode: C;  -*-
 * File: limits.h
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION:  h/mips/limits.h
 **
 ** HISTORY:
 ** Created: Tue May  3 18:13:59 1994 (jch1003)
 ** Last Edited: Thu May  5 18:08:55 1994 By James Hall
 **
 ** $Id: limits.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 **
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

/* COPYING.LIB is in lib/c/COPYING.LIB */

#ifndef	_limits_h_
#define	_limits_h_	

/* 
 * Define ANSI <limits.h> for standard 32-bit words.
 * These assume 8-bit `char's, 16-bit `short int's,
 *  and 32-bit `int's and `long int's. 
 */

/* Number of bits in a `char'.  */
#define CHAR_BIT        8

/* Maximum length of any multibyte character in any locale.
   Locale-writers should change this as necessary.  */
#define MB_LEN_MAX      2

/* Minimum and maximum values a `signed char' can hold.  */
#define SCHAR_MIN       (-128)
#define SCHAR_MAX       127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0.)  */
#define UCHAR_MAX       255U

/* Minimum and maximum values a `char' can hold.  */
#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN        0
#define CHAR_MAX        UCHAR_MAX
#else
#define CHAR_MIN        SCHAR_MIN
#define CHAR_MAX        SCHAR_MAX
#endif

/* Minimum and maximum values a `signed short int' can hold.  */
#define SHRT_MIN        (-32768)
#define SHRT_MAX        32767

/* Maximum value an `unsigned short int' can hold.  (Minimum is 0.)  */
#define USHRT_MAX       65535

/* Minimum and maximum values a `signed int' can hold.  */
#define INT_MIN (- INT_MAX - 1)
#define INT_MAX 2147483647

/* Maximum value an `unsigned int' can hold.  (Minimum is 0.)  */
#define UINT_MAX        4294967295U

/* Minimum and maximum values a `signed long int' can hold.  */
#define LONG_MIN        INT_MIN
#define LONG_MAX        INT_MAX

/* Maximum value an `unsigned long int' can hold.  (Minimum is 0.)  */
#define ULONG_MAX       UINT_MAX


#endif	/* _limits_h_ */

/*
 * end limits.h
 */
