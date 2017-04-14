/*  -*- Mode: C;  -*-
 * File: strspn.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return length of prefix of s consisting entirely of chars in tar.
 **
 ** HISTORY:
 ** Created: Tue Apr 26 14:59:20 1994 (jch1003)
 ** Last Edited: Tue Sep 13 10:30:52 1994 By James Hall
 **
    $Log: strspn.c,v $
    Revision 1.2  1995/02/28 10:02:45  dme
    reinstate copylefts

 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.2  1994/10/31  18:03:02  jch1003
 * Moved from ../strspn.c, naming changes.
 *
 * Revision 1.1  1994/08/22  11:34:34  jch1003
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

/*
 * exports
 */

extern size_t strspn(const char *s, const char *tar);

extern size_t
strspn(const char *s, const char *tar)

{
  register const char *p;
  register const char *a;
  register size_t count = 0;

  for (p = s; *p != '\0'; ++p)
    {
      for (a = tar; *a != '\0'; ++a)
	if (*p == *a)
	  break;
      if (*a == '\0')
	return count;
      else
	++count;
    }

  return count;

}

/*
 * end strspn.c
 */
