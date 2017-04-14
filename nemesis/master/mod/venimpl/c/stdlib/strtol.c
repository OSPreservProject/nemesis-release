/*  -*- Mode: C;  -*-
 * File: strtol.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Convert NPTR to a `long int' in base BASE.
 **           If BASE is 0 the base is determined by the presence of a leading
 **           zero, indicating octal or a leading "0x" or "0X", indicating 
 **           hexadecimal.
 **           If BASE is < 2 or > 36, it is reset to 10.
 **           If ENDPTR is not NULL, a pointer to the character after the last
 **           one converted is stored in *ENDPTR.
 **
 **           Entry at Nemesis Level RAISES stdlib_noflow, stdlib_poflow. 
 **
 ** HISTORY:
 ** Created: Wed May  4 17:44:38 1994 (jch1003)
 ** Last Edited: Tue Sep 13 16:40:47 1994 By James Hall
 **
 ** $Id: strtol.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
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
#include <limits.h>
#include <ctype.h>

#ifndef	UNSIGNED
#define	UNSIGNED	0
#endif

#ifndef	BITS64
#define	BITS64		0
#endif


/*
 * strtol/strtoul/strto64/strtou64
 */
  

#if BITS64

# define WORKTYPE	uint64_t
# define WORKTYPE_MAX	0xffffffffffffffffULL
# define WT_SIGNED_MIN	0x8000000000000000ULL
# define WT_SIGNED_MAX	0x7fffffffffffffffULL
# if UNSIGNED
#  define RESTYPE 	uint64_t
#  define STRTO   	strtou64
# else
#  define RESTYPE 	int64_t
#  define STRTO   	strto64
# endif

#else

# define WORKTYPE	unsigned long
# define WORKTYPE_MAX	ULONG_MAX
# define WT_SIGNED_MIN	(- (WORKTYPE) LONG_MIN)
# define WT_SIGNED_MAX	((WORKTYPE) LONG_MAX)
# if UNSIGNED
#  define RESTYPE 	unsigned long
#  define STRTO   	strtoul
# else
#  define RESTYPE 	long
#  define STRTO   	strtol
# endif

#endif


extern RESTYPE
STRTO (const char *nptr, char **endptr, int base)
{
  int 			negative;
  register WORKTYPE	cutoff;
  register unsigned int cutlim;
  register WORKTYPE	i;
  register const char 	*s;
  register unsigned char c;
  const char 		*save;
  int 			overflow;

  if (base < 0 || base == 1 || base > 36)
    base = 10;

  s = nptr;

  /* Skip white space.  */
  while (isspace(*s))
    ++s;
  if (*s == '\0')
    goto noconv;

  /* Check for a sign.  */
  if (*s == '-')
    {
      negative = 1;
      ++s;
    }
  else if (*s == '+')
    {
      negative = 0;
      ++s;
    }
  else
    negative = 0;

  if (base == 16 && s[0] == '0' && toupper(s[1]) == 'X')
    s += 2;

  /* If BASE is zero, figure it out ourselves.  */
  if (base == 0)
    if (*s == '0')
      {
	if (toupper(s[1]) == 'X')
	  {
	    s += 2;
	    base = 16;
	  }
	else
	  base = 8;
      }
    else
      base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;

  cutoff = WORKTYPE_MAX / (WORKTYPE) base;
  cutlim = WORKTYPE_MAX % (WORKTYPE) base;

  overflow = 0;
  i = 0;
  for (c = *s; c != '\0'; c = *++s)
    {
      if (isdigit(c))
	c -= '0';
      else if (isalpha(c))
	c = toupper(c) - 'A' + 10;
      else
	break;
      if (c >= base)
	break;
      /* Check for overflow.  */
      if (i > cutoff || (i == cutoff && c > cutlim))
	overflow = 1;
      else
	{
	  i *= (WORKTYPE) base;
	  i += c;
	}
    }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr != NULL)
    *endptr = (char *) s;

#if !UNSIGNED

  /* Check for a value that is within the range of
     `unsigned long int', but outside the range of `long int'.  */
  if (i > (negative ? WT_SIGNED_MIN : WT_SIGNED_MAX))
    overflow = 1;

#endif

  if (overflow)
    {
#if UNSIGNED
      RAISE("stdlib_uoflow", NULL);
#else
      if(negative)
	RAISE("stdlib_noflow", NULL);
      else
	RAISE("stdlib_poflow", NULL);
#endif 

    }

  /* Return the result of the appropriate sign.  */
  return (negative ? - i : i);

 noconv:
  /* There was no number to convert.  */
  if (endptr != NULL)
    *endptr = (char *) nptr;
  return 0;

}


/*
 * end strtol.c
 */
