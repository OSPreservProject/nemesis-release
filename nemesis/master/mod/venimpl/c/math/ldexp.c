/*  -*- Mode: C;  -*-
 * File: ldexp.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return X times (two to the EXP power).
 **           Nemesis level entry RAISES
 **               mathlib_oflow: -/+ overflow
 **               mathlib_uflow: -/+ underflow
 **               mathlib_arginval: x is -/+ INF and exp < 0
 **               mathlib_argisnan: x is NAN
 **
 ** HISTORY:
 ** Created: Mon Jun 13 12:17:31 1994 (jch1003)
 ** Last Edited: Mon Oct 10 17:56:21 1994 By James Hall
 **
    $Log: ldexp.c,v $
    Revision 1.2  1995/02/28 10:10:04  dme
    reinstate copylefts

 * Revision 1.2  1994/11/01  10:41:02  jch1003
 * Fn name changed to Nlibc_ldexp(), naming changes, exceptions introduced.
 *
 * Revision 1.1  1994/08/22  12:01:17  jch1003
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

#include <nemesis.h>
#include <math.h>
#include <float.h>
#include <ieee754.h>


#define TRC(x) x

typedef union { uint64_t u; double d; } bogus_t;

double
ldexp(double x, int exp)

{
  union ieee754_double u;
  unsigned int exponent;
  bogus_t bogus;
  double *exn_arg;


  u.d = x;
#define	x u.d

  exponent = u.ieee.exponent;

  bogus.d = x;

  TRC (kprintf ("ldexp: x=%q exp=%x exponent=%x\n", bogus.u, exp, exponent));

  /* The order of the tests is carefully chosen to handle
     the usual case first, with no branches taken.  */

  if (exponent != 0)
    {
      /* X is nonzero and not denormalized.  */

      if (exponent <= DBL_MAX_EXP - DBL_MIN_EXP + 1)
  	{
	  /* X is finite.  When EXP < 0, overflow is actually underflow.  */

	  exponent += exp;

	  if (exponent != 0)
	    {
	      if (exponent <= DBL_MAX_EXP - DBL_MIN_EXP + 1)
		{
		  /* In range.  */
		  u.ieee.exponent = exponent;
		  return x;
		}

	      if (exp >= 0)
	      overflow:
		{
		  const int negative = u.ieee.negative;
		  u.d = HUGE_VAL;
		  u.ieee.negative = negative;
		  exn_arg = (double *)exn_rec_alloc(sizeof(double));
		  *exn_arg = u.d;
		  RAISE("mathlib_oflow", exn_arg);
		}
	      if (exponent <= - (unsigned int) (DBL_MANT_DIG + 1))
	      underflow:
		{
		  const int negative = u.ieee.negative;
		  u.d = 0.0;
		  u.ieee.negative = negative;
		  exn_arg = (double *)exn_rec_alloc(sizeof(double));
		  *exn_arg = u.d;
		  RAISE("mathlib_uflow", exn_arg);
		}
	    }

	  /* Gradual underflow.  */
	  u.ieee.exponent = 1;
	  u.d *= ldexp (1.0, (int) exponent - 1);
	  if (u.ieee.mantissa0 == 0 && u.ieee.mantissa1 == 0)
	    /* Underflow.  */
	  {
	    exn_arg = (double *)exn_rec_alloc(sizeof(double));
	    *exn_arg = u.d;
	    RAISE("mathlib_uflow", exn_arg);
	  } 
	  return u.d;
  	}

      /* X is +-infinity or NaN.  */
      if (u.ieee.mantissa0 == 0 && u.ieee.mantissa1 == 0)
  	{
	  /* X is +-infinity.  */
	  if (exp >= 0)
	    goto overflow;
	  else
	    {
	      /* (infinity * number < 1).  With infinite precision,
		 (infinity / finite) would be infinity, but otherwise it's
		 safest to regard (infinity / 2) as indeterminate.  The
		 infinity might be (2 * finite).  */
	     
	      const int negative = u.ieee.negative;
	      u.d = NAN;
	      u.ieee.negative = negative;
	      exn_arg = (double *)exn_rec_alloc(sizeof(double));
	      *exn_arg = u.d;
	      RAISE("mathlib_arginval", exn_arg);
	    }
	}

      /* X is NaN.  */
      exn_arg = (double *)exn_rec_alloc(sizeof(double));
      *exn_arg = u.d;
      RAISE("mathlib_argisnan", exn_arg);
    }

  /* X is zero or denormalized.  */
  if (u.ieee.mantissa0 == 0 && u.ieee.mantissa1 == 0)
    /* X is +-0.0. */
    return x;


  TRC (kprintf ("ldexp: recurse\n"));


  /* X is denormalized.
     Multiplying by 2 ** DBL_MANT_DIG normalizes it;
     we then subtract the DBL_MANT_DIG we added to the exponent.  */
  return ldexp (x * ldexp (1.0, DBL_MANT_DIG), exp - DBL_MANT_DIG);
}


/*
 * end ldexp.c
 */


