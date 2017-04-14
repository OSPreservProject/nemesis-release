/*  -*- Mode: C;  -*-
 * File: strrchr.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C LIbrary.
 **
 ** FUNCTION: Return pointer top last occurence of c in s, or NULL if not
 **                present.
 **
 ** HISTORY:
 ** Created: Tue Apr 26 14:49:30 1994 (jch1003)
 ** Last Edited: Fri Oct  7 15:08:36 1994 By James Hall
 **
    $Log: strrchr.c,v $
    Revision 1.2  1995/02/28 10:02:45  dme
    reinstate copylefts

 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.2  1994/10/31  18:01:36  jch1003
 * Moved from ../strrchr.c, naming changes.
 *
 * Revision 1.1  1994/08/22  11:34:08  jch1003
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

extern char *strrchr(const char *s, int c);

extern char *
strrchr(const char *s, int c)

{
  register const char *found, *p;

  c = (unsigned char) c;

  /* Since strchr is fast, we use it rather than the obvious loop.  */
  
  if (c == '\0')
    return strchr(s, '\0');

  found = NULL;
  while ((p = strchr(s, c)) != NULL)
    {
      found = p;
      s = p + 1;
    }

  return (char *) found;

}

/*
 * end strrchr.c
 */ 
