/*  -*- Mode: C;  -*-
 * File: memset.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Place c into first n 'chars' of s, return s.
 **
 ** $Id: memset.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **
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
 * Exports
 */

extern void *memset(void *s, int c, size_t n);
extern void bzero(void *s, int n);

void *
memset(void *s, int c, size_t n)

{
  long int dstp = (long int) s;

  if (n >= 8)
    {
      size_t xlen;
      op_t cccc;

      cccc = (unsigned char) c;
      cccc |= cccc << 8;
      cccc |= cccc << 16;
      if (OPSIZ > 4)
	cccc |= cccc << 32;

      /* There are at least some bytes to set.
	 No need to test for n == 0 in this alignment loop.  */

      while (dstp % OPSIZ != 0) {
	
	  ((byte *) dstp)[0] = c;
	  dstp += 1;
	  n -= 1;

	}

      /* Write 8 `op_t' per iteration until less than 8 `op_t' remain.  */

      xlen = n / (OPSIZ * 8);
      while (xlen > 0) {

	  ((op_t *) dstp)[0] = cccc;
	  ((op_t *) dstp)[1] = cccc;
	  ((op_t *) dstp)[2] = cccc;
	  ((op_t *) dstp)[3] = cccc;
	  ((op_t *) dstp)[4] = cccc;
	  ((op_t *) dstp)[5] = cccc;
	  ((op_t *) dstp)[6] = cccc;
	  ((op_t *) dstp)[7] = cccc;
	  dstp += 8 * OPSIZ;
	  xlen -= 1;

	}
      n %= OPSIZ * 8;

      /* Write 1 `op_t' per iteration until less than OPSIZ bytes remain.  */

      xlen = n / OPSIZ;
      while (xlen > 0) {

	  ((op_t *) dstp)[0] = cccc;
	  dstp += OPSIZ;
	  xlen -= 1;

	}

      n %= OPSIZ;

    }

  /* Write the last few bytes.  */

  while (n > 0) {

      ((byte *) dstp)[0] = c;
      dstp += 1;
      n -= 1;

    }

  return s;

}


void bzero(void *s, int n)
{
  long int dstp = (long int) s;

  if (n >= 8)
    {
      size_t xlen;

      /* There are at least some bytes to set.
	 No need to test for n == 0 in this alignment loop.  */

      while (dstp % OPSIZ != 0) {
	
	  ((byte *) dstp)[0] = 0;
	  dstp += 1;
	  n -= 1;

	}

      /* Write 8 `op_t' per iteration until less than 8 `op_t' remain.  */

      xlen = n / (OPSIZ * 8);
      while (xlen > 0) {

	  ((op_t *) dstp)[0] = 0;
	  ((op_t *) dstp)[1] = 0;
	  ((op_t *) dstp)[2] = 0;
	  ((op_t *) dstp)[3] = 0;
	  ((op_t *) dstp)[4] = 0;
	  ((op_t *) dstp)[5] = 0;
	  ((op_t *) dstp)[6] = 0;
	  ((op_t *) dstp)[7] = 0;
	  dstp += 8 * OPSIZ;
	  xlen -= 1;

	}
      n %= OPSIZ * 8;

      /* Write 1 `op_t' per iteration until less than OPSIZ bytes remain.  */

      xlen = n / OPSIZ;
      while (xlen > 0) {

	  ((op_t *) dstp)[0] = 0;
	  dstp += OPSIZ;
	  xlen -= 1;

	}

      n %= OPSIZ;

    }

  /* Write the last few bytes.  */

  while (n > 0) {

      ((byte *) dstp)[0] = 0;
      dstp += 1;
      n -= 1;

    }

  return s;

}

/*
 * end memset.c
 */
