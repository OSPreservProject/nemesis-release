/*  -*- Mode: C;  -*-
 * File: scanf.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Formatted input from stdin.
 **           At Nemesis entry level RAISES stdio_streamerr, stdio_cantlock.
 **
 ** HISTORY:
 ** Created: Wed Aug 10 13:53:43 1994 (jch1003)
 ** Last Edited: Mon Nov  7 13:44:00 1994 By James Hall
 **
 ** $Id: scanf.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

extern int _scanf (Rd_cl *stream, const char *format, va_list *ap);

PUBLIC int 
scanf (const char *format, ...)
{
  va_list   ap;
  NOCLOBBER int	    res;

  va_start(ap, format);
    
  TRY
    res = _scanf(Pvs(in), format, &ap);
  FINALLY
    va_end(ap);
  ENDTRY;
    
  return res;
}

/*
 * end scanf.c
 */


