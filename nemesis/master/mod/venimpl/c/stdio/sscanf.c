/*  -*- Mode: C;  -*-
 * File: sscanf.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Read formatted 'input' from string s.
 **           Nemesis level entry RAISES stdio_sscanferr.
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

extern int libc_doscan(const char *format, va_list *ap, Rd_cl *stream);

/*
 * Contains
 */
static int _sscanf(const char *s, const char *format, va_list *ap);

 
PUBLIC int 
sscanf(const char *s, const char *format, ...)
{
  va_list ap;
  int res;

  va_start(ap, format);

  res = _sscanf(s, format, &ap);

  va_end(ap);

  if(res < 0)
    RAISE("stdio_sscanferr", NULL);

  return res;
}

static Rd_GetC_fn   TrivRd_GetC;
static Rd_UnGetC_fn TrivRd_UnGetC;

static Rd_op TrivRd_ms = {
  TrivRd_GetC, NULL, TrivRd_UnGetC, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL,
  NULL, NULL, NULL, NULL
};

PUBLIC int 
_sscanf(const char *s, const char *format, va_list *ap)

{
  int   res;
  Rd_cl rd;

  rd.op = &TrivRd_ms;
  rd.st = (void*) s;

  res = libc_doscan(format, ap, &rd);
  return res;
}

static int8_t 
TrivRd_GetC (Rd_cl *self)

{
  return *((uchar_t *) (self->st))++;
}

static void 
TrivRd_UnGetC (Rd_cl *self)

{
  ((uchar_t *) (self->st))--;
}

/*
 * end sscanf.c
 */
