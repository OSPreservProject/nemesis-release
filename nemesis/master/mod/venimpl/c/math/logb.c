/*  -*- Mode: C;  -*-
 * File: logb.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return the base 2 signed integral exponent of X.
 **           At Nemesis level RAISES mathlib_oflow
 **               mathlib_uflow
 **               mathlib_arginval
 **               mathlib_argisnan
 **           (passed from ldexp()).
 **
 ** HISTORY:
 ** Created: Mon Jun 13 12:38:44 1994 (jch1003)
 ** Last Edited: Mon Oct 10 17:53:56 1994 By James Hall
 **
    $Log: logb.c,v $
    Revision 1.2  1995/02/28 10:10:04  dme
    reinstate copylefts

 * Revision 1.2  1994/11/01  11:05:19  jch1003
 * Fn name changed to Nlibc_logb(), naming changes, exceptions introduced.
 *
 * Revision 1.1  1994/08/22  12:02:55  jch1003
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
#include <float.h>
#include <ieee754.h>


double
logb(double x)

{
  union ieee754_double u;

  if (isnan (x))
    return x;
  else if (isinf (x))
    return HUGE_VAL;
  else if (x == 0.0)
    return - HUGE_VAL;

  u.d = x;

  if (u.ieee.exponent == 0)
    /* A denormalized number.
       Multiplying by 2 ** DBL_MANT_DIG normalizes it;
       we then subtract the DBL_MANT_DIG we added to the exponent.  */
    return (logb (x * ldexp (1.0, DBL_MANT_DIG)) - DBL_MANT_DIG);

  return (int) u.ieee.exponent - (DBL_MAX_EXP - 1);
}

/*
 * end logb.c
 */
