/*  -*- Mode: C;  -*-
 * File: strcspn.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Return length of prefix of s consisting of chars *not* in rej.
 **
 ** HISTORY:
 ** Created: Tue Apr 26 15:26:06 1994 (jch1003)
 ** Last Edited: Fri Oct  7 15:06:52 1994 By James Hall
 **
    $Log: strcspn.c,v $
    Revision 1.2  1995/02/28 10:02:45  dme
    reinstate copylefts

 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.2  1994/10/31  17:49:36  jch1003
 * Moved from ../strcspn.c, naming changes.
 *
 * Revision 1.1  1994/08/22  11:32:05  jch1003
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

extern size_t strcspn(const char *s, const char *rej);

extern size_t
strcspn(const char *s, const char *rej)

{
  register size_t count = 0;

  while (*s != '\0')
    if (strchr(rej, *s++) == NULL)
      ++count;
    else
      return count;

  return count;

}


/*
 * end strcspn.c
 */
