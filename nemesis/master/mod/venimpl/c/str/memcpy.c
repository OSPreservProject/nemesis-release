/*  -*- Mode: C;  -*-
 * File: memcpy.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C library
 **
 ** FUNCTION: Copy n bytes from s2 to s1 - return s1  
 **             - doesn't work if memory areas overlap
 **
 ** HISTORY:
 ** Created: Wed Apr 13 17:13:58 1994 (jch1003)
 **
 ** $Id: memcpy.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
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
#include <memcopy.h>		/* copying macros */

/*
 * Exports
 */

extern void *memcpy(void *s1, const void *s2, size_t n);
extern void bcopy(const void *src, void *dest, int n);

INLINE void *memcpy(void *s1, const void *s2, size_t n)

{
	
  unsigned long int dstp = (long int) s1;
  unsigned long int srcp = (long int) s2;

  /* Copy from the beginning to the end.  */

  /* If there not too few bytes to copy, use word copy.  */
  if (n >= OP_T_THRES)
    {
      /* Copy just a few bytes to make DSTP aligned.  */
      n -= (-dstp) % OPSIZ;
      BYTE_COPY_FWD (dstp, srcp, (-dstp) % OPSIZ);

      /* Copy from SRCP to DSTP taking advantage of the known
	 alignment of DSTP.  Number of bytes remaining is put
	 in the third argument, i.e. in n.  This number may
	 vary from machine to machine.  */

      WORD_COPY_FWD (dstp, srcp, n, n);

      /* Fall out and copy the tail.  */
    }

  /* There are just a few bytes to copy.  Use byte memory operations.  */
  BYTE_COPY_FWD (dstp, srcp, n);

  return s1;

}

void bcopy(const void *src, void *dest, int n)
{
    memcpy(dest, src, n);
}

/*
 * end memcpy.c
 */
