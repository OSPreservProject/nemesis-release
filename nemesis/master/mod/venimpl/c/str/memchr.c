/*  -*- Mode: C;  -*-
 * File: memchr.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library
 **
 ** FUNCTION: Return pointer to first occurence of 'char' c in s, or NULL
 **               if not present in first n 'characters'.
 **
 ** $Id: memchr.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
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

/*
 * Mips and Alpha versions differ considerably and are not merged.
 */

void *
memchr(const void *s, int c, size_t n)

#if !defined(__ALPHA__)

{
  const unsigned char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, charmask;

  c = (unsigned char) c;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a 4-byte border.  */
  for (char_ptr = s; n > 0 && ((unsigned long int) char_ptr & 3) != 0;
       --n, ++char_ptr)
    if (*char_ptr == c)
      return (void *) char_ptr;

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:
     
     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD 

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  magic_bits = 0x7efefeff;

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  while (n >= 4)
    {
      /* We tentatively exit the loop if adding MAGIC_BITS to
	 LONGWORD fails to change any of the hole bits of LONGWORD.

	 1) Is this safe?  Will it catch all the zero bytes?
	 Suppose there is a byte with all zeros.  Any carry bits
	 propagating from its left will fall into the hole at its
	 least significant bit and stop.  Since there will be no
	 carry from its most significant bit, the LSB of the
	 byte to the left will be unchanged, and the zero will be
	 detected.

	 2) Is this worthwhile?  Will it ignore everything except
	 zero bytes?  Suppose every byte of LONGWORD has a bit set
	 somewhere.  There will be a carry into bit 8.  If bit 8
	 is set, this will carry into bit 16.  If bit 8 is clear,
	 one of bits 9-15 must be set, so there will be a carry
	 into bit 16.  Similarly, there will be a carry into bit
	 24.  If one of bits 24-30 is set, there will be a carry
	 into bit 31, so all of the hole bits will be changed.

	 The one misfire occurs when bits 24-30 are clear and bit
	 31 is set; in this case, the hole at bit 31 is not
	 changed.  If we had access to the processor carry flag,
	 we could close this loophole by putting the fourth hole
	 at bit 32!

	 So it ignores everything except 128's, when they're aligned
	 properly.

	 3) But wait!  Aren't we looking for C, not zero?
	 Good point.  So what we do is XOR LONGWORD with a longword,
	 each of whose bytes is C.  This turns each byte that is C
	 into a zero.  */

      longword = *longword_ptr++ ^ charmask;

      /* Add MAGIC_BITS to LONGWORD.  */
      if ((((longword + magic_bits)
	
	    /* Set those bits that were unchanged by the addition.  */
	    ^ ~longword)
	       
	   /* Look at only the hole bits.  If any of the hole bits
	      are unchanged, most likely one of the bytes was a
	      zero.  */
	   & ~magic_bits) != 0)
	{
	  /* Which of the bytes was C?  If none of them were, it was
	     a misfire; continue the search.  */

	  const unsigned char *cp = (const unsigned char *) (longword_ptr - 1);

	  if (cp[0] == c)
	    return (void *) cp;
	  if (cp[1] == c)
	    return (void *) &cp[1];
	  if (cp[2] == c)
	    return (void *) &cp[2];
	  if (cp[3] == c)
	    return (void *) &cp[3];
	}

      n -= 4;
    }

  char_ptr = (const unsigned char *) longword_ptr;

  while (n-- > 0)
    {
      if (*char_ptr == c)
	return (void *) char_ptr;
      else
	++char_ptr;
    }

  return NULL;
}

#endif /* !__ALPHA__ */


#ifdef __ALPHA__

{
  const char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int charmask;
  size_t x;

  c = (unsigned char) c;

  /* Handle the first few characters by reading one character at a time.
     Do this until STR is aligned on a 8-byte border.  */
  for (char_ptr = s; n > 0 && ((unsigned long int) char_ptr & 7) != 0;
       --n, ++char_ptr)
    if (*char_ptr == c)
      return (void *) char_ptr;

  if (n == (size_t)0)
    return NULL;

  x = n;

  longword_ptr = (unsigned long int *) char_ptr;

  /* Set up a longword, each of whose bytes is C.  */
  charmask = c | (c << 8);
  charmask |= charmask << 16;
  charmask |= charmask << 32;
  charmask |= charmask << 64;

  for (;;)
    {
      const unsigned long int longword = *longword_ptr++;
      int ge, le;

      if (x < 4)
	x = (size_t) 0;
      else
	x -= 4;

      /* Set bits in GE if bytes in CHARMASK are >= bytes in LONGWORD.  */
      __asm__ ("cmpbge %1, %2, %0" : "=r" (ge) : "r" (charmask), "r" (longword));

      /* Set bits in LE if bytes in CHARMASK are <= bytes in LONGWORD.  */
      __asm__ ("cmpbge %2, %1, %0" : "=r" (le) : "r" (charmask), "r" (longword));

      /* Bytes that are both <= and >= are == to C.  */
      if (ge & le)
	{
	  /* Which of the bytes was the C?  */

	  unsigned char *cp = (unsigned char *) (longword_ptr - 1);
	  int i;

	  for (i = 0; i < 6; i++)
	    if (cp[i] == c)
	      return &cp[i];
	  return &cp[7];
	}

      if (x == (size_t)0)
	break;
    }

  return NULL;
}

#endif /* __ALPHA__ */

/*
 * end memchr.c
 */
