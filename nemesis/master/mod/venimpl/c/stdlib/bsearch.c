/*  -*- Mode: C;  -*-
 * File: bsearch.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Perform a binary search for KEY in BASE which has NMEMB elements
 **           of SIZE bytes each.  The comparisons are done by (*COMPAR)(). 
 **
 ** HISTORY:
 ** Created: Wed May 18 10:24:45 1994 (jch1003)
 ** Last Edited: Wed Sep 21 14:37:18 1994 By James Hall
 **
    $Log: bsearch.c,v $
    Revision 1.2  1995/02/28 10:10:40  dme
    reinstate copylefts

 * Revision 1.2  1994/10/31  15:27:49  jch1003
 * Moved from ../bsearch.c, naming changes.
 *
 * Revision 1.1  1994/08/22  11:12:16  jch1003
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
#include <stdlib.h>

extern void*
bsearch(register const void *key, register const void *base,
	     size_t nmemb, register size_t size,
	     register int (*compar) (const void *keyval, const void *datum))

{
  register size_t l, u, idx;
  register const void *p;
  register int comparison;

  l = 0;
  u = nmemb;
  while (l < u)
    {
      idx = (l + u) / 2;
      p = (void*) (((const char *) base) + (idx * size));
      comparison = (*compar)(key, p);
      if (comparison < 0)
	u = idx;
      else if (comparison > 0)
	l = idx + 1;
      else
	return (void*) p;
    }

  return NULL;
}


/*
 * end bsearch.c
 */
