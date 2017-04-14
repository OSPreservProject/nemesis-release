/*  -*- Mode: C;  -*-
 * File: pow.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: POW(X,Y)  
 **            RETURN X**Y 
 **            IEEE DOUBLE PRECISION 
 **
 **           At Nemesis level entry RAISES
 **            mathlib_uflow, mathlib_oflow and as noted below
 **           AND PASSES exceptions raised by ldexp().
 **
 ** HISTORY:
 ** Created: Fri Jun 10 14:23:43 1994 (jch1003)
 ** Last Edited: Mon Oct 10 17:53:08 1994 By James Hall
 **
    $Log: pow.c,v $
    Revision 1.2  1995/02/28 10:10:04  dme
    reinstate copylefts

 * Revision 1.2  1994/11/01  11:07:20  jch1003
 * Fn name changed to Nlibc_pow(), naming changes, exceptions introduced.
 *
 * Revision 1.1  1994/08/22  12:04:46  jch1003
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
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Required system supported functions:
 *      scalb(x,n)    (== ldexp(x,n))  
 *      logb(x)         
 *	copysign(x,y)	
 *	finite(x)	
 *	drem(x,y)
 *
 * Required kernel functions:
 *	exp__E(a,c)	...return  exp(a+c) - 1 - a*a/2
 *	log__L(x)	...return  (log(1+x) - 2s)/s, s=x/(2+x) 
 *	pow_p(x,y)	...return  +(anything)**(finite non zero)
 *
 * Method
 *	1. Compute and return log(x) in three pieces:
 *		log(x) = n*ln2 + hi + lo,
 *	   where n is an integer.
 *	2. Perform y*log(x) by simulating muti-precision arithmetic and 
 *	   return the answer in three pieces:
 *		y*log(x) = m*ln2 + hi + lo,
 *	   where m is an integer.
 *	3. Return x**y = exp(y*log(x))
 *		= 2^m * ( exp(hi+lo) ).
 *
 * Special cases:
 *	(anything) ** 0  is 1 ;
 *	(anything) ** 1  is itself;
 *	(anything) ** NaN is NaN; RAISES mathlib_argisnan.
 *	NaN ** (anything except 0) is NaN; RAISES argisnan.
 *	+-(anything > 1) ** +INF is +INF;
 *	+-(anything > 1) ** -INF is +0;
 *	+-(anything < 1) ** +INF is +0;
 *	+-(anything < 1) ** -INF is +INF;
 *	+-1 ** +-INF is NaN; RAISES mathlib_arginval.
 *	+0 ** +(anything except 0, NaN)  is +0;
 *	-0 ** +(anything except 0, NaN, odd integer)  is +0;
 *	+0 ** -(anything except 0, NaN)  is +INF; Raises mathlib_arginval.
 *	-0 ** -(anything except 0, NaN, odd integer) is +INF;Raises mathlib_arginval.
 *	-0 ** (odd integer) = -( +0 ** (odd integer) );
 *	+INF ** +(anything except 0,NaN) is +INF;
 *	+INF ** -(anything except 0,NaN) is +0;
 *	-INF ** (odd integer) = -( +INF ** (odd integer) );
 *	-INF ** (even integer) = ( +INF ** (even integer) );
 *	-INF ** -(anything except integer,NaN) is NaN;Raises mathlib_arginval.
 *	-(x=anything) ** (k=integer) is (-1)**k * (x ** k);
 *	-(anything except 0) ** (non-integer) is NaN; Raises mathlib_arginval.
 *
 * Accuracy:
 *	pow(x,y) returns x**y nearly rounded. In particular, on a SUN, a VAX,
 *	and a Zilog Z8000,
 *			pow(integer,integer)
 *	always returns the correct integer provided it is representable.
 *	In a test run with 100,000 random arguments with 0 < x, y < 20.0
 *	on a VAX, the maximum observed error was 1.79 ulps (units in the 
 *	last place).
 *
 */

#include <nemesis.h>
#include <math.h>
#include <limits.h>

#define TRC(x) x


static const double ln2hi = 6.9314718055829871446E-1;
static const double ln2lo = 1.6465949582897081279E-12;
static const double invln2 =1.4426950408889633870E0;
static const double sqrt2 = 1.4142135623730951455E0;

static const double zero=0.0, half=1.0/2.0, one=1.0, two=2.0, negone= -1.0;

/* Prototype - internal function */
static double libc_pow_p();

/* Prototypes - internal functions not declared elsewhere */
extern double log__L(double z);
extern double exp__E(double x, double c);


double 
pow(double x, double y)  	

{
  double t;

  if     (y==zero)
    return(one);
  else if(isnan (x))
  {
    double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
    *exn_arg = NAN;
    RAISE("mathlib_argisnan", exn_arg);
  }
  else if(y==one)
    return( x );      /* y=1 */
  else if(isnan(y)) 
  {
    double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
    *exn_arg = NAN;
    RAISE("mathlib_argisnan", exn_arg); 
  }
  else if(!finite(y))                     /* if y is INF */
    if((t=copysign(x,one))==one)
    {
      double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
      *exn_arg = NAN;
      RAISE("mathlib_arginval", exn_arg);
    }
  
    else if(t>one) 
      return((y>zero)?y:zero);
    else
      return((y<zero)?-y:zero);
  else if(y==two)       
    return(x*x);
  else if(y==negone)    
    return(one/x);
  
  /* sign(x) = 1 */
  else if(copysign(one,x)==one) 
    return(libc_pow_p(x,y));
  
  /* sign(x)= -1 */
  /* if y is an even integer */
  else if ( (t=drem(y,two)) == zero)	
    return( libc_pow_p(-x,y) );
  
  /* if y is an odd integer */
  else if (copysign(t,one) == one) 
    return( -libc_pow_p(-x,y) );
  
  /* Henceforth y is not an integer */
  else if(x==zero)	/* x is -0 */
    return((y>zero)?-x:one/(-x));
  else 
  {
    double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
    *exn_arg = HUGE_VAL;
    RAISE("mathlib_arginval", exn_arg);
  }
  
}

/* pow_p(x,y) return x**y for x with sign=1 and finite y */

static double libc_pow_p(double x, double y)       

{
  double c,s,t,z,tx,ty;

  float sx,sy;
  long k;
  int n,m;

  TRC (kprintf ("pow: libc_pow_p entered\n"));

  if(x==zero||!finite(x))           /* if x is +INF or +0 */
   /* return((y>zero)?x:one/x);*/ 
    {
      if(y > 0)
	return x;
      else
	{
	  if(x == 0)
	    {
	      double *exn_arg = (double *)exn_rec_alloc(sizeof(double));
	      *exn_arg = HUGE_VAL;
	      RAISE("mathlib_arginval", exn_arg);
	    }
	  else
	    return 0;
	}
    }

  if(x==1.0) return(x);	/* if x=1.0, return 1 since y is finite */

  TRC (kprintf ("pow: libc_pow_p: call logb/ldexp\n"));

  /* reduce x to z in [sqrt(1/2)-1, sqrt(2)-1] */
  z=ldexp(x,-(n=logb(x)));  

  TRC (kprintf ("pow: libc_pow_p: call logb/ldexp 1\n"));

  if(n <= -1022) {
    n += (m=logb(z)); 
    z=ldexp(z,-m);
  } 

  TRC (kprintf ("pow: libc_pow_p: 2\n"));

  if(z >= sqrt2 ) {
    n += 1; 
    z *= half;
  }  
  z -= one ;

  TRC (kprintf ("pow: libc_pow_p: 3\n"));

  /* log(x) = nlog2+log(1+z) ~ nlog2 + t + tx */
  s=z/(two+z); 
  c=z*z*half; 
  tx=s*(c+log__L(s*s)); 
  t= z-(c-tx); 
  tx += (z-t)-c;

  TRC (kprintf ("pow: libc_pow_p: 4\n"));

  /* if y*log(x) is neither too big nor too small */
  if((s=logb(y)+logb(n+t)) < 12.0) 
    if(s>-60.0) {

      /* compute y*log(x) ~ mlog2 + t + c */
      s=y*(n+invln2*t);
      m=s+copysign(half,s);   /* m := nint(y*log(x)) */ 
      /* (long int) (double) LONG_MIN overflows on some systems.  */
      if (y >= (double) LONG_MIN + 1 && y <= (double) LONG_MAX &&
	  (double) (long int) y == y)
	{
	  /* Y is an integer */
	  k = m - (long int) y * n;
	  sx=t; tx+=(t-sx);
	}
      else	
	{		/* if y is not an integer */    
	  k =m;
	  tx+=n*ln2lo;
	  sx=(c=n*ln2hi)+t; 
	  tx+=(c-sx)+t; 
	}
      /* end of reductions for integral/nonintegral y */

      sy=y; 
      ty=y-sy;          /* y ~ sy + ty */

      s=(double)sx*sy-k*ln2hi;        /* (sy+ty)*(sx+tx)-kln2 */

      z=(tx*ty-k*ln2lo);
      tx=tx*sy; ty=sx*ty;
      t=ty+z; t+=tx; t+=s;
      c= -((((t-s)-tx)-ty)-z);

      /* return exp(y*log(x)) */
      t += exp__E(t,c); 
      return(ldexp(one+t,m));
    }
  /* end of if log(y*log(x)) > -60.0 */
	    
    else
      /* exp(+- tiny) = 1 with inexact flag */
      {ln2hi+ln2lo; return(one);}
  else if(copysign(one,y)*(n+invln2*t) <zero)
    /* exp(-(big#)) underflows to zero */
    return(ldexp(one,-5000)); 
  else
    /* exp(+(big#)) overflows to INF */
    return(ldexp(one, 5000)); 

}


/*
 * end pow.c
 */





