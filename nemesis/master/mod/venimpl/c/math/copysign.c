/*  -*- Mode: C;  -*-
 * File: copysign.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return X with its signed changed to Y's.  
 **
 ** HISTORY:
 ** Created: Fri Jun 10 16:49:09 1994 (jch1003))
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


#include <math.h>
#include <ieee754.h>


double
copysign(double x, double y)

#ifdef __MIPS__

{
  union ieee754_double ux, uy;

  ux.d = x;
  uy.d = y;

  ux.ieee.negative = uy.ieee.negative;

  return ux.d;
}

#endif /* __MIPS__ */

#ifdef __ALPHA__

{

  __asm__ ("cpys %1, %2, %0" : "=f" (x) : "f" (y), "f" (x));
  return x;

}

#endif /* __ALPHA__ */


/*
 * end copysign.c
 */

