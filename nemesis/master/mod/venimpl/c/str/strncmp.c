/*  -*- Mode: C;  -*-
 * File: strncmp.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE Nemesis C Library.
 **
 ** FUNCTION: Compare at most n chars of s1 and s2
 **             - return <0 if s1 < s2
 **                       0 if s1 = s2
 **                     > 0 if s1 > s2
 **
 ** HISTORY:
 ** Created: Fri Apr 22 17:19:07 1994 (jch1003)
 ** Last Edited: Tue Sep 13 10:26:55 1994 By James Hall
 **
    $Log: strncmp.c,v $
    Revision 1.2  1995/02/28 10:02:45  dme
    reinstate copylefts

 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.2  1994/10/31  17:57:05  jch1003
 * Moved from../strncmp.c, naming changes.
 *
 * Revision 1.1  1994/08/22  11:32:54  jch1003
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
#include <string.h>
#include <memcopy.h>

/*
 * exports
 */

extern int strncmp(const char *s1, const char *s2, size_t n);
extern int
strncmp(const char *s1, const char *s2, size_t n)

{
  unsigned reg_char c1 = '\0';
  unsigned reg_char c2 = '\0';

  if (n >= 4) {

      size_t n4 = n >> 2;

      do {

	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;
	  c1 = (unsigned char) *s1++;
	  c2 = (unsigned char) *s2++;
	  if (c1 == '\0' || c1 != c2)
	    return c1 - c2;

	} while (--n4 > 0);

      n &= 3;

    }

  while (n > 0) {

      c1 = (unsigned char) *s1++;
      c2 = (unsigned char) *s2++;
      if (c1 == '\0' || c1 != c2)
	return c1 - c2;
      n--;

    }

  return c1 - c2;

}

/*
 * end strncmp.c
 */
