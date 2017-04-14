/*  -*- Mode: C;  -*-
 * File: math.h
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: F.P. Maths header.
 **
 ** HISTORY:
 ** Created: Wed May  4 14:12:46 1994 (jch1003)
 ** Last Edited: Tue Aug 23 14:43:26 1994 By James Hall
 **
 ** $Id: math.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **
 **
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */


#ifndef _math_h_
#define _math_h_

#include <nemesis.h>

/* Get machine-dependent HUGE_VAL value (returned on overflow).  */
#include "huge_val.h"

/* Get machine-dependent NAN value (returned for some domain errors).  */
#include "nan.h"


/*
 * prototypes
 *
 */
/* (inlines on some architectures) */
#ifdef __IX86__
#include <math_inline.h>
#else
extern double	atan( double __x );
extern double	atan2( double __x, double __y );
extern double	exp( double __x );
extern double	cos( double __x );
extern double	log( double __x );
extern double	pow( double __x, double __y );
extern double	rint( double __x );
extern double	sin( double __x );
extern double	sqrt( double __x );
extern double	tan( double __x );
#endif /* __IX86__ */

extern double	acos( double __x );
extern double	acosh( double __x );
extern double	asin( double __x );
extern double	asinh( double __x );
extern double	atanh( double __x);
extern double	cbrt( double __x );
extern double	ceil( double __x );
extern double	cosh( double __x );
extern double	fabs( double __x );
extern double	floor( double __x );
extern double	fmod( double __x, double __y );
extern double	frexp( double __value, int *__eptr );
extern double	ldexp( double __value, int __exp );
extern double	log10( double __x );
extern double	modf( double __value, double *__iptr);
extern double	sinh( double __x );
extern double	tanh( double __x );
extern double	trunc( double __x );
extern double	atof( const char *__nptr );

/*
 * prototypes - supporting functions.
 */

extern int     isinf(double x);
extern int     isnan(double x);
extern int     finite(double x);
extern double  infnan(int error);
extern double  ldexp(double value, int exponent);
extern double  logb(double x);
extern double  copysign(double value, double sign);
extern double  drem(double numerator, double denominator);


#define	MAXFLOAT	((float)3.40282346638528860e+38)

/* These names are OK according to ANSI extension rules */

#define _ABS(x)	((x) < 0 ? -(x) : (x))
#define _REDUCE(TYPE, X, XN, C1, C2)	{ \
	double x1 = (double)(TYPE)X, x2 = X - x1; \
	X = x1 - (XN) * (C1); X += x2; X -= (XN) * (C2); }
#define _POLY1(x, c)	((c)[0] * (x) + (c)[1])
#define _POLY2(x, c)	(_POLY1((x), (c)) * (x) + (c)[2])
#define _POLY3(x, c)	(_POLY2((x), (c)) * (x) + (c)[3])
#define _POLY4(x, c)	(_POLY3((x), (c)) * (x) + (c)[4])
#define _POLY5(x, c)	(_POLY4((x), (c)) * (x) + (c)[5])
#define _POLY6(x, c)	(_POLY5((x), (c)) * (x) + (c)[6])
#define _POLY7(x, c)	(_POLY6((x), (c)) * (x) + (c)[7])
#define _POLY8(x, c)	(_POLY7((x), (c)) * (x) + (c)[8])
#define _POLY9(x, c)	(_POLY8((x), (c)) * (x) + (c)[9])

#ifndef M_PI
#define M_PI        3.14159265358979323846	/* pi */
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923	/* pi/2 */
#endif
#ifndef PI                      /* as in stroustrup */
#define PI  M_PI
#endif
#ifndef PI2
#define PI2  M_PI_2
#endif
#endif /* _math_h_ */


/*
 * end math.h
 */
