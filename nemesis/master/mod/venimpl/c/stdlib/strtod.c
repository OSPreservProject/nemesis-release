/*  -*- Mode: C;  -*-
 * File: strtod.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Convert prefix of st to a double (ignoring leading white space).
 **           If endp is not NULL store pointer to any unconverted suffix in endp.
 **           
 **           If o'flow: return appropriately signed HUGE_VAL.
 **           If u'flow: return 0.
 **           In either case errno set to ERANGE.
 **
 **           Nemesis level entry RAISES stdlib_poflow, stdlib_noflow, 
 **           stdlib_uflow, stdlib_arginval.
 **
 ** HISTORY:
 ** Created: Tue May  3 16:04:15 1994 (jch1003)
 ** Last Edited: Tue Oct 11 11:57:54 1994 By James Hall
 **
 ** $Id: strtod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
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

#include <nemesis.h>
#include <exceptions.h>
#include <stdlib.h>


#ifdef __LOCALES__
#include <localeinfo.h>	/* locales not yet fully supported */
#endif
#include <float.h>
#include <ctype.h>
#include <math.h>


#define _pow pow

extern double
strtod(const char *st, char **endp)

{
  register const char *s;
  short int sign;

#ifdef __LOCALES__
  wchar_t decimal;	/* Decimal point character.  */
#endif


  double num;           /* The number so far.  */
  double po;		/* Temp repositry for results of pow() */

  int got_dot;		/* Found a decimal point.  */
  int got_digit;	/* Seen any digits.  */

  /* The exponent of the number.  */
  long int exponent;

  if (st == NULL)
    {
      if(endp != NULL)
	*endp = (char*)st;

      RAISE("stdlib_arginval", NULL);
    }

#ifdef __LOCALES__
  /* Figure out the decimal point character.  */
  if (mbtowc(&decimal, _numeric_info->decimal_point, 1) <= 0)
    decimal = (wchar_t) *_numeric_info->decimal_point;
#endif

  s = st;

  /* Eat whitespace.  */
  while (isspace(*s))
    ++s;

  /* Get the sign.  */
  sign = *s == '-' ? -1 : 1;
  if (*s == '-' || *s == '+')
    ++s;

  num = 0.0;
  got_dot = 0;
  got_digit = 0;
  exponent = 0;
  for (;; ++s)
    {
      if (isdigit (*s))
	{
	  got_digit = 1;

	  /* Make sure that multiplication by 10 will not overflow.  */
	  if (num > DBL_MAX * 0.1)
	    /* The value of the digit doesn't matter, since we have already
	       gotten as many digits as can be represented in a `double'.
	       This doesn't necessarily mean the result will overflow.
	       The exponent may reduce it to within range.

	       We just need to record that there was another
	       digit so that we can multiply by 10 later.  */
	    ++exponent;
	  else
	    num = (num * 10.0) + (*s - '0');

	  /* Keep track of the number of digits after the decimal point.
	     If we just divided by 10 here, we would lose precision.  */
	  if (got_dot)
	    --exponent;
	}
#ifdef __LOCALES__
      else if (!got_dot && (wchar_t) *s == decimal)
#else
      else if (!got_dot && *s == '.')
#endif
	/* Record that we have found the decimal point.  */
	got_dot = 1;
      else
	/* Any other character terminates the number.  */
	break;
    }

  if (!got_digit)
    goto noconv;

  if (tolower(*s) == 'e')
    {
      /* Get the exponent specified after the `e' or `E'.  */

      char *end;
      long int exp;

      ++s;

      TRY
	{
	  exp = strtol(s, &end, 10);
	}
	  /* if oflow raised the exponent overflowed a `long int'.  
	     It is probably a safe assumption that an exponent that cannot be 
	     represented by a `long int' exceeds the limits of a `double'.  */

      CATCH("stdlib_poflow")
	{
	  if (endp != NULL)
	    *endp = end;
	  if(sign == -1)
	    RAISE("stdlib_noflow", NULL);
	  else
	    RAISE("stdlib_poflow", NULL);
	}
      CATCH("stdlib_noflow")
	{
	  if (endp != NULL)
	    *endp = (char*)st;
	  RAISE("stdlib_uflow", NULL);
	}
      ENDTRY;

      if (end == s)
	/* There was no exponent.  Reset END to point to
	   the 'e' or 'E', so *ENDPTR will be set there.  */
	end = (char *) s - 1;

      s = end;
      exponent += exp;
    }

  if (endp != NULL)
    *endp = (char *) s;

  if (num == 0.0)
    return 0.0;

  /* Multiply NUM by 10 to the EXPONENT power,
     checking for overflow and underflow.  */

  TRY
    {
      po = _pow(10.0, (double) -exponent);
    }
  CATCH_ALL
    {
      RAISE("stdlib_arginval", NULL);
    }
  ENDTRY;

  if (exponent < 0)
    {
      if (num < DBL_MIN * po)
	goto underflow;
    }
  else if (exponent > 0)
    {
      if (num > DBL_MAX * po)
	goto overflow;
    }

  TRY
    {
      num *= _pow(10.0, (double) exponent);
    }
  CATCH_ALL
    {
      RAISE("stdlib_arginval", NULL);
    }
  ENDTRY;

  return num * sign;

 overflow:
  /* Return an overflow error.  */
  if(sign == -1)
    RAISE("stdlib_noflow", NULL);
  else
    RAISE("stdlib_poflow", NULL);

 underflow:
  /* Return an underflow error.  */
  if (endp != NULL)
    *endp = (char *) st;
  RAISE("stdlib_uflow", NULL);

 noconv:
  /* There was no number.  */
  if (endp != NULL)
    *endp = (char *) st;
  return 0.0;

}

/*
 * end strtod.c
 */




