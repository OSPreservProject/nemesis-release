/*  -*- Mode: C;  -*-
 * File: isnan.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return nonzero if VALUE is not a number.
 **
 ** HISTORY:
 ** Created: Mon Jun 13 12:14:46 1994 (jch1003)
 ** Last Edited: Mon Oct 10 11:35:20 1994 By James Hall
 **
    $Log: isnan.c,v $
    Revision 1.2  1995/02/28 10:10:04  dme
    reinstate copylefts

 * Revision 1.2  1994/11/01  10:38:28  jch1003
 * Fn name changed to UNlibc_isnan.c
 *
 * Revision 1.1  1994/08/22  12:00:46  jch1003
 * Initial revision
 *
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
/* Copyright (C) 1992 Free Software Foundation, Inc.
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

#include <math.h>
#include <ieee754.h>


int
isnan(double value)

{
  union ieee754_double u;

  u.d = value;

  /* IEEE 754 NaN's have the maximum possible
     exponent and a nonzero mantissa.  */
  return ((u.ieee.exponent & 0x7ff) == 0x7ff &&
	  (u.ieee.mantissa0 != 0 || u.ieee.mantissa1 != 0));

}

/*
 * end isnan.c
 */
