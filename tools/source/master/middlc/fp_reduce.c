/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      fp_reduce.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Take the 128-bit ascii-hex md5 value on stdin and reduce it to a
**	48-bit ascii-hex value by xoring overlapping 48-bit chunks.
** 
** ENVIRONMENT: 
**
**      Used in middlc script
** 
** ID : $Id: fp_reduce.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
** 
**
*/

#include <stdio.h>

unsigned int octet[16] = {0, };

int main (int argc, char *argv[])
{
  if (scanf ("%02x%02x%02x%02x%02x%02x%02x%02x"
	     "%02x%02x%02x%02x%02x%02x%02x%02x",
	     &octet[0],  &octet[1],  &octet[2],  &octet[3], 
	     &octet[4],  &octet[5],  &octet[6],  &octet[7], 
	     &octet[8],  &octet[9],  &octet[10], &octet[11], 
	     &octet[12], &octet[13], &octet[14], &octet[15]) != 16)
    exit (1);

  /* there's 16 bits of overlap to use up; we put 8 bits at each end */

  octet [0] ^= octet [5];
  octet [1] ^= octet [6];
  octet [2] ^= octet [7];
  octet [3] ^= octet [8];
  octet [4] ^= octet [9];
  octet [5] ^= octet [10];

  octet [0] ^= octet [10];
  octet [1] ^= octet [11];
  octet [2] ^= octet [12];
  octet [3] ^= octet [13];
  octet [4] ^= octet [14];
  octet [5] ^= octet [15];

  printf ("fingerprint = '%02x%02x%02x%02x%02x%02x'\n",
	  octet [0], octet [1], octet [2], octet [3], octet [4], octet [5]);
  
  exit(0);
}
