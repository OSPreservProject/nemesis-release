/*  -*- Mode: C;  -*-
 * File: strlen.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C library
 **
 ** FUNCTION: Return no. of non-zero bytes in a 'string'
 **
 ** HISTORY:
 ** Created: Wed Apr 13 18:57:50 1994 (jch1003))
 ** Last Edited: Tue Sep 13 10:25:17 1994 By James Hall
 **
    $Log: strlen.c,v $
    Revision 1.4  1995/08/09 15:13:14  dme
    see updates

 * Revision 1.3  1995/05/25  12:56:49  rjb17
 * made portable.
 *
 * Revision 1.2  1995/02/28  10:02:45  dme
 * reinstate copylefts
 *
 * Revision 1.1  1995/02/02  11:58:05  dme
 * Initial revision
 *
 * Revision 1.3  1994/10/31  17:53:33  jch1003
 * Moved from ../strlen.c, naming changes.
 *
 * Revision 1.2  1994/08/22  15:50:29  jch1003
 * New, groovier version, alpha code added.
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

/*
 * Mips and Alpha versions are dissimiler enough that we
 * don't attempt to merge them.
 */


#include <nemesis.h>
#include <string.h>		/* for size_t */



/*
 * exports
 */

extern size_t strlen(const char *s);

extern size_t 
strlen(const char *s)

#if !defined(__ALPHA__)

{
  const char *char_ptr;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, himagic, lomagic;

  /* 
   * Handle the first few characters by reading one character at a time.
   * Do this until s is aligned on a 4-byte border. 
   */

  for (char_ptr = s; ((unsigned long int) char_ptr & 3) != 0; ++char_ptr)
    if (*char_ptr == 0)
      return char_ptr - s;

/* Scan for the null terminator quickly by testing four bytes at a time.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
   * the "holes."  Note that there is a hole just to the left of
   * each byte, with an extra at the end:
   *
   * bits:  01111110 11111110 11111110 11111111
   * bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD 

   *  The 1-bits make sure that carries propagate to the next 0-bit.
   * The 0-bits provide holes for carries to fall into. 
   */

  magic_bits = 0x7efefeff;

  himagic = 0x80808080;
  lomagic = 0x01010101;

  /* Instead of the traditional loop which tests each character,
   * we will test a longword at a time.  The tricky part is testing
   * if *any of the four* bytes in the longword in question are zero.
   */

  for (;;) {

    /* We tentatively exit the loop if adding MAGIC_BITS to
     * LONGWORD fails to change any of the hole bits of LONGWORD.
     *
     *   1) Is this safe?  Will it catch all the zero bytes?
     *   Suppose there is a byte with all zeros.  Any carry bits
     *	 propagating from its left will fall into the hole at its
     *	 least significant bit and stop.  Since there will be no
     *	 carry from its most significant bit, the LSB of the
     *	 byte to the left will be unchanged, and the zero will be
     *	 detected.
     *
     *   2) Is this worthwhile?  Will it ignore everything except
     *	 zero bytes?  Suppose every byte of LONGWORD has a bit set
     *	 somewhere.  There will be a carry into bit 8.  If bit 8
     *	 is set, this will carry into bit 16.  If bit 8 is clear,
     *	 one of bits 9-15 must be set, so there will be a carry
     *	 into bit 16.  Similarly, there will be a carry into bit
     *	 24.  If one of bits 24-30 is set, there will be a carry
     *	 into bit 31, so all of the hole bits will be changed.
     *
     *	 The one misfire occurs when bits 24-30 are clear and bit
     *	 31 is set; in this case, the hole at bit 31 is not
     *	 changed.  If we had access to the processor carry flag,
     *	 we could close this loophole by putting the fourth hole
     *	 at bit 32!
     *
     *	 So it ignores everything except 128's, when they're aligned
     *	 properly. 
     */

      longword = *longword_ptr++;

      if (
#if 0
	  /* Add MAGIC_BITS to LONGWORD.  */

	  (((longword + magic_bits)

	    /* Set those bits that were unchanged by the addition.  */

	    ^ ~longword)

	   /* Look at only the hole bits.  If any of the hole bits
	    * are unchanged, most likely one of the bytes was a
	    * zero. 
	    */

	   & ~magic_bits)
#else
	  ((longword - lomagic) & himagic)
#endif
	  != 0)

	{

	  /* Which of the bytes was the zero?  If none of them were, it was
	   * a misfire; continue the search.
	   */

	  const char *cp = (const char *) (longword_ptr - 1);

	  if (cp[0] == 0)
	    return cp - s;
	  if (cp[1] == 0)
	    return cp - s + 1;
	  if (cp[2] == 0)
	    return cp - s + 2;
	  if (cp[3] == 0)
	    return cp - s + 3;
	}
    }
}

#endif /* !__ALPHA__ */

#ifdef __ALPHA__

/* Scan for the null terminator quickly by testing eight bytes at a time.  */

{
  const char *char_ptr;
  const unsigned long int *longword_ptr;

  /* Handle the first few characters by reading one character at a time.
     Do this until STR is aligned on a 8-byte border.  */
  for (char_ptr = s; ((unsigned long int) char_ptr & 7) != 0; ++char_ptr)
    if (*char_ptr == '\0')
      return char_ptr - s;

  longword_ptr = (unsigned long int *) char_ptr;

  for (;;)
    {
      int mask;
      __asm__ ("cmpbge %1, %2, %0" : "=r" (mask) : "r" (0), "r" (*longword_ptr++));
      if (mask)
	{
	  /* Which of the bytes was the zero?  */

	  const char *cp = (const char *) (longword_ptr - 1);
	  int i;

	  for (i = 0; i < 7; i++)
	    if (cp[i] == 0)
	      return cp - s + i;
	  return cp - s + 7;
	}
    }
}

#endif /* __ALPHA__ */


/*
 * end strlen.c
 */
