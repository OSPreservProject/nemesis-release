/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      math_inline.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Inline math functions for Intel
** 
** ENVIRONMENT: 
**
**      Anywhere
** 
** ID : $Id: math_inline.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _math_inline_h_
#define _math_inline_h_


static INLINE double sin(double a)
{
    double result;

    __asm__ ("fsin"
	     : "=t" (result)
	     : "0" (a) );
    return result;
}

static INLINE double cos(double a)
{
    double result;

    __asm__ ("fcos"
	     : "=t" (result)
	     : "0" (a) );
    return result;
}

static INLINE double fabs(double x) {
    double result;

    __asm__ ("fabs"
	     : "=t" (result)
	     : "0" (x) );
    return result;
}

static INLINE double sqrt(double a)
{
    double result;

    __asm__ ("fsqrt"
	     : "=t" (result)
	     : "0" (a) );
    return result;
}

static INLINE double atan2(double a, double b)
{
    double result;
    double throwaway;

    __asm__ ("fpatan ; fdecstp"
	     : "=t" (throwaway), "=u" (result)
	     : "0" (b), "1" (a)
	     : "st", "st(1)" );
    return result;
}


/* from linux libc-5.4.13 */
static INLINE double log(double x)
{
    /* domain check missed out: x <= 0.0 should fault the CPU */
    __asm__ __volatile__ ("fldln2\n\t"
			  "fxch %%st(1)\n\t"
			  "fyl2x"
			  :"=t" (x) : "0" (x));
    return x;
}

static INLINE double rint(double x)
{
  __asm__ __volatile__ ("frndint" : "=t" (x) : "0" (x));
  return (x);
}


/* Ripped from glibc-2.0.5 */

static INLINE double tan(double __x)
{
  register double __value;
  register double __value2 __attribute__ ((unused));
  __asm __volatile__
      ("fptan"
       : "=t" (__value2), "=u" (__value) : "0" (__x));
  
  return __value;
}

static INLINE double exp(double __x)
{
    register double __value, __exponent;
  __asm __volatile__
    ("fldl2e                    # e^x = 2^(x * log2(e))\n\t"
     "fmul      %%st(1)         # x * log2(e)\n\t"
     "fstl      %%st(1)\n\t"
     "frndint                   # int(x * log2(e))\n\t"
     "fxch\n\t"
     "fsub      %%st(1)         # fract(x * log2(e))\n\t"
     "f2xm1                     # 2^(fract(x * log2(e))) - 1\n\t"
     : "=t" (__value), "=u" (__exponent) : "0" (__x));
  __value += 1.0;
  __asm __volatile__
    ("fscale"
     : "=t" (__value) : "0" (__value), "u" (__exponent));

  return __value;
}

static INLINE double __log2 (double __x)
{
  register double __value;
  __asm __volatile__
    ("fld1\n\t"
     "fxch\n\t"
     "fyl2x"
     : "=t" (__value) : "0" (__x));

  return __value;
}

static INLINE double pow (double __x, double __y)
{
  register double __value, __exponent;
  long __p = (long) __y;

  if (__x == 0.0 && __y > 0.0)
    return 0.0;
  if (__y == (double) __p)
    {
      double __r = 1.0;
      if (__p == 0)
        return 1.0;
      if (__p < 0)
        {
          __p = -__p;
          __x = 1.0 / __x;
        }
      while (1)
        {
          if (__p & 1)
            __r *= __x;
          __p >>= 1;
          if (__p == 0)
            return __r;
          __x *= __x;
        }
      /* NOTREACHED */
    }
  __asm __volatile__
    ("fmul      %%st(1)         # y * log2(x)\n\t"
     "fstl      %%st(1)\n\t"
     "frndint                   # int(y * log2(x))\n\t"
     "fxch\n\t"
     "fsub      %%st(1)         # fract(y * log2(x))\n\t"
     "f2xm1                     # 2^(fract(y * log2(x))) - 1\n\t"
     : "=t" (__value), "=u" (__exponent) :  "0" (__log2 (__x)), "1" (__y));
  __value += 1.0;
  __asm __volatile__
    ("fscale"
     : "=t" (__value) : "0" (__value), "u" (__exponent));

  return __value;
}

static INLINE double atan (double __x)
{
    register double __value;
    __asm __volatile__
	("fld1\n\t"
	 "fpatan"
	 : "=t" (__value) : "0" (__x));
    
    return __value;
}

static INLINE 
double
floor (double x)
{
  volatile short cw, cwtmp;

  __asm__ volatile ("fnstcw %0" : "=m" (cw) : );
  /* rounding down */
  cwtmp = (cw & 0xf3ff) | 0x0400;
  __asm__ volatile ("fldcw %0" : : "m" (cwtmp));
  /* x = floor of x */
  __asm__ volatile ("frndint" : "=t" (x) : "0" (x));
  __asm__ volatile ("fldcw %0" : : "m" (cw));
  return (x);
}

static INLINE
double
ceil (double x)
{
  return -(floor(-x));
}


#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923      /* pi/2 */
#endif


static INLINE
double asin(double x) {
  double y;

  if (x <= -1.0 ) {
    if (x == -1.0) {
      return -M_PI_2;
    }
    return 0; /* XXX */
  } 

  if (x >= 1.0 ) {
    if (x == 1.0) {
      return M_PI_2;
    }
    return 0; /* XXX */
  } 

  y = sqrt (1.0 - x * x);
  __asm__ __volatile__ ("fpatan"
                        :"=t" (y) : "0" (y), "u" (x));
  return y;
}



/* ----------------------------------------------------------------------
 * Lifted from gclib __math.h, mcewanca@dcs.gla
 */
 

static INLINE double
__expm1 (double __x)
{
  register double __value, __exponent, __temp;
  __asm __volatile__
    ("fldl2e                    # e^x - 1 = 2^(x * log2(e)) - 1\n\t"
     "fmul      %%st(1)         # x * log2(e)\n\t"
     "fstl      %%st(1)\n\t"
     "frndint                   # int(x * log2(e))\n\t"
     "fxch\n\t"
     "fsub      %%st(1)         # fract(x * log2(e))\n\t"
     "f2xm1                     # 2^(fract(x * log2(e))) - 1\n\t"
     "fscale                    # 2^(x * log2(e)) - 2^(int(x * log2(e)))\n\t"
     : "=t" (__value), "=u" (__exponent) : "0" (__x));
  __asm __volatile__
    ("fscale                    # 2^int(x * log2(e))\n\t"
     : "=t" (__temp) : "0" (1.0), "u" (__exponent));
  __temp -= 1.0;
 
  return __temp + __value;
}
 

static INLINE double
__sgn1 (double __x)
{
  return __x >= 0.0 ? 1.0 : -1.0;
}
 
static INLINE double
fmod (double __x, double __y)
{
  register double __value;
  __asm __volatile__
    ("1:        fprem\n\t"
     "fstsw     %%ax\n\t"
     "sahf\n\t"
     "jp        1b"
     : "=t" (__value) : "0" (__x), "u" (__y) : "ax", "cc");
 
  return __value;
}


static INLINE double acos(double a)
{
  return atan2(sqrt(1.0 - a*a), a);
}


static INLINE double cosh(double a)
{
  double e2a = exp (a);
  
  return 0.5 * (e2a + 1.0 / e2a);
}

/*
static INLINE double fmod(double a, double b)
{
    double result;

    __asm__ ("fmod"
	     : "=t" (result)
	     : "0" (a) );
    return result;
}
*/

static INLINE double sinh(double a)
{
  double exm1 = __expm1 (fabs (a));
 
  return 0.5 * (exm1 / (exm1 + 1.0) + exm1) * __sgn1 (a);
}

/* ---------------------------------------------------------------------- */



#endif /* _math_inline_h_ */
