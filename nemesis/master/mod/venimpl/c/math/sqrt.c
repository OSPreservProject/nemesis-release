/*  -*- Mode: C;  -*-
 * File: sqrt.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return the square root of X.
 **           At Nemesis level entry RAISES mathlib_argisnan, mathlib_arginval
 **           AND PASSES mathlib_uflow, mathlib_oflow RAISED by ldexp().
 **
 ** HISTORY:
 ** Created: Mon Jun 13 12:49:47 1994 (jch1003)
 ** Last Edited: Mon Oct 10 17:20:27 1994 By James Hall
 **
    $Log: sqrt.c,v $
    Revision 1.2  1995/02/28 10:10:04  dme
    reinstate copylefts

 * Revision 1.2  1994/11/01  11:09:01  jch1003
 * Fn name changed to Nlibc_sqrt(), naming changes, exceptions introduced.
 *
 * Revision 1.1  1994/08/22  12:05:09  jch1003
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
/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <nemesis.h>
#include <math.h>


double
sqrt(double x)

{
  double q, s, b, r, t;
  const double zero = 0.0;
  int m, n, i;

  /* sqrt (NaN) is NaN; sqrt (+-0) is +-0.  */
  if (isnan (x))
    {
      double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
      *exn_arg = NAN;
      RAISE("mathlib_argisnan", exn_arg);
    }
  else if(x == zero)
    return x;

  if (x < zero)
    {
      double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
      *exn_arg = NAN;
      RAISE("mathlib_arginval", exn_arg);
    }

  /* sqrt (Inf) is Inf.  */
  if (isinf (x))
    return x;

  /* Scale X to [1,4).  */
  n = logb (x);
  
  x = ldexp (x, -n);
  m = logb (x);
  if (m != 0)
    /* Subnormal number.  */
    x = ldexp (x, -m);
  
  m += n;
  n = m / 2;

  if ((n + n) != m)
    {
      x *= 2;
      --m;
      n = m / 2;
    }

  /* Generate sqrt (X) bit by bit (accumulating in Q).  */
  q = 1.0;
  s = 4.0;
  x -= 1.0;
  r = 1;
  for (i = 1; i <= 51; i++)
    {
      t = s + 1;
      x *= 4;
      r /= 2;
      if (t <= x)
	{
	  s = t + t + 2, x -= t;
	  q += r;
	}
      else
	s *= 2;
    }

  /* Generate the last bit and determine the final rounding.  */
  r /= 2;
  x *= 4;
  if (x == zero)
    goto end;
  (void) (100 + r);		/* Trigger inexact flag.  */
  if (s < x)
    {
      q += r;
      x -= s;
      s += 2;
      s *= 2;
      x *= 4;
      t = (x - s) - 5;
      b = 1.0 + 3 * r / 4;
      if (b == 1.0)
	goto end;		/* B == 1: Round to zero.  */
      b = 1.0 + r / 4;
      if (b > 1.0)
	t = 1;			/* B > 1: Round to +Inf.  */
      if (t >= 0)
	q += r;
    }				/* Else round to nearest.  */
  else
    {
      s *= 2;
      x *= 4;
      t = (x - s) - 1;
      b = 1.0 + 3 * r / 4;
      if (b == 1.0)
	goto end;
      b = 1.0 + r / 4;
      if (b > 1.0)
	t = 1;
      if (t >= 0)
	q += r;
    }

end:
  return ldexp (q, n);
}


/*
 * end sqrt.c
 */
