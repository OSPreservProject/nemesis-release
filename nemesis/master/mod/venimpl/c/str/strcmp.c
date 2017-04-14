/*  -*- Mode: C;  -*-
 * File: strcmp.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library
 **
 ** FUNCTION: Compare p1 and p2, returning less than, equal to or
 **             greater than zero if S1 is lexiographically less than,
 **             equal to or greater than S2. 
 **
 ** HISTORY:
 ** Created: Wed Apr 20 14:35:27 1994 (jch1003)
 ** Last Edited: Tue Sep 13 10:22:55 1994 By James Hall
 **
    $Log: strcmp.c,v $
    Revision 1.3  1996/09/12 11:35:37  and1000
    Added linux libc's strcasecmp(0 function

 * Revision 1.2  1995/02/28  10:02:45  dme
 * reinstate copylefts
 *
 * Revision 1.2  1995/02/28  10:02:45  dme
 * reinstate copylefts
 *
 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.2  1994/10/31  17:46:29  jch1003
 * Moved from ../strcmp.c, naming changes.
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
#include <ctype.h>
#include <memcopy.h>

/*
 * exports
 */

extern int strcmp(const char *p1, const char *p2);

extern int
strcmp(const char *p1, const char *p2)

{
  register const unsigned char *s1 = (const unsigned char *) p1;
  register const unsigned char *s2 = (const unsigned char *) p2;
  unsigned reg_char c1, c2;

  do {
    
    c1 = (unsigned char) *s1++;
    c2 = (unsigned char) *s2++;
    if (c1 == '\0')
      return c1 - c2;
    
  } while (c1 == c2);
  
  return c1 - c2;
  
}


extern int
strcasecmp(const char *s1, const char *s2)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  register int ret;
  unsigned char c1;

  if (p1 == p2)
      return 0;

  for (; !(ret = (c1 = tolower(*p1)) - tolower(*p2)); p1++, p2++)
      if (c1 == '\0') break;
  return ret;
}
