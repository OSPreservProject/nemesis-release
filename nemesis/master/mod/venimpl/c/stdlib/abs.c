/*  -*- Mode: C;  -*-
 * File: abs.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C library.
 **
 ** FUNCTION: Return absolute value of argument.
 **
 ** HISTORY:
 ** Created: Wed May 18 12:32:53 1994 (jch1003)
 ** Last Edited: Wed Sep 21 14:18:11 1994 By James Hall
 **
 ** $Id: abs.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

/*
 * exports
 */

extern int
abs(int i)
{
  return(i < 0 ? -i : i);
}

extern long int
labs(long int i)
{
  return(i < 0 ? -i : i);
}

/*
 * end abs.c
 */

