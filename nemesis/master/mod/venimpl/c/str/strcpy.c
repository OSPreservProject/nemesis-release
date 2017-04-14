/*  -*- Mode: C;  -*-
 * File: strcpy.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library
 **
 ** FUNCTION: Copy s1 to s2 (copy non-zero bytes from s1 to s2)
 **
 ** HISTORY:
 ** Created: Wed Apr 13 17:55:34 1994 (jch1003)
 ** Last Edited: Tue Sep 13 10:22:54 1994 By James Hall
 **
    $Log: strcpy.c,v $
    Revision 1.2  1995/02/28 10:02:45  dme
    reinstate copylefts

 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.2  1994/10/31  17:48:00  jch1003
 * Moved from ../strcpy.c, naming changes.
 *
 * Revision 1.1  1994/08/22  11:31:36  jch1003
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

extern char *strcpy(char *s2, const char *s1);

extern char *
strcpy(char *s2, const char *s1)

{
  register char c;
  char *s = (char *) s1;
  const ptrdiff_t off = s2 - s1 - 1;

  do {

    c = *s++;
    s[off] = c;

  } while (c != '\0');

  return s2;
}

/*
 * end strcpy.c
 */
