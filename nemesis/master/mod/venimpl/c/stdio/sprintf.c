/*  -*- Mode: C;  -*-
 * File: sprintf.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Write formatted 'output' to string s.
 **           Nemesis level entry RAISES stdio_sprintferr.
 **
 ** HISTORY:
 ** Created: Wed Aug 10 12:29:44 1994 (jch1003)
 ** Last Edited: Thu Oct  6 16:15:26 1994 By James Hall
 **
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

/*
 * Imports
 */

extern int libc_doprnt(const char *format, va_list ap, Wr_cl *stream);

extern int vsprintf(char *s, const char *format, va_list ap);

/*
 * Contains
 */
 
PUBLIC int 
sprintf(char *s, const char *format, ...)
{
  va_list ap;
  int res;

  va_start(ap, format);

  res = vsprintf(s, format, ap);

  va_end(ap);

  if(res < 0)
    RAISE("stdio_sprintferr", NULL);

  return res;
}

static Wr_LPutC_fn TrivWr_LPutC;

static Wr_op TrivWr_ms = {
  NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL,
  TrivWr_LPutC, NULL, NULL, NULL
};


PUBLIC int 
vsprintf(char *s, const char *format, va_list ap)
{
  int   res;
  Wr_cl wr;

  wr.op = &TrivWr_ms;
  wr.st = (void*) s;

  res = libc_doprnt(format, ap, &wr);

  *((char*)(wr.st)) = '\0';

  return res;

}

static void
TrivWr_LPutC (Wr_cl *self, int8_t c)

{
  *((uchar_t *) (self->st))++ = c;
  return;
}

/*
 * end sprintf.c
 */
