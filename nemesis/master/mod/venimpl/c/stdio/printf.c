/*  -*- Mode: C;  -*-
 * File: printf.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Formatted output to stdout.
 **           At Nemesis entry level RAISES stdio_streamerr, stdio_cantlock.
 **
 ** HISTORY:
 ** Created: Wed Aug 10 13:53:43 1994 (jch1003)
 ** Last Edited: Mon Nov  7 13:44:00 1994 By James Hall
 **
 ** $Id: printf.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 **
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

extern int _printf (Wr_cl *stream, const char *format,  va_list ap);

extern int libc_doprnt(const char *format, va_list ap, Wr_cl *stream);


PUBLIC int 
libc_printf (const char *format, ...)
{
  va_list   	    ap;
  NOCLOBBER int	    res;

  va_start(ap, format);
    
  TRY
    res = _printf(Pvs(out), format, ap);
  FINALLY
    va_end(ap);
  ENDTRY;
    
  return res;
}

/* This needs to be callable from almost all environments. So, it
 * can't use exceptions: it relies on it's underlying writer not
 * raising any. No locking performed: you'd better not call this from
 * multiple threads. */
#ifdef PRINTF_DOMAIN_COLOURS
#warning Using COLOUR for printf

#define FG(c, s)				\
Wr$PutC((s), 27);				\
Wr$PutC((s), 91);				\
Wr$PutC((s), 51);				\
Wr$PutC((s), 48+(c));				\
Wr$PutC((s), 109);

#define DOMAIN(d, s) FG(1+(d), s);
#define NODOMAIN(s)  FG(11, s)

#endif

#ifdef PRINTF_DOMAIN_IDS
#warning Using Domain IDs for printf
static const char hex_table[] = { '0', '1', '2', '3', '4', '5', '6', '7',
			   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

#define DOMAIN(d, s)				\
Wr$PutC((s), hex_table[((d)>>4)&15]);           \
Wr$PutC((s), hex_table[(d)&15]);                \
Wr$PutC((s), ':');				\
Wr$PutC((s), ' ');				\

#endif

extern Wr_cl triv_wr_cl;

PUBLIC int 
eprintf (const char *format, ...)
{
  va_list   	ap;
  int		res;
  Wr_clp dbw = &triv_wr_cl;

  va_start(ap, format);

#ifdef DOMAIN
    DOMAIN(Pvs(vp) ? (VP$DomainID(Pvs(vp)) & 255) : 0, dbw);
#endif DOMAIN

  res = libc_doprnt(format, ap, dbw);

#ifdef NODOMAIN
    NODOMAIN(dbw);
#endif NODOMAIN

  va_end(ap);

  return res;
}

PUBLIC int 
wprintf (Wr_cl *stream, const char *format, ...)
{
  va_list   	ap;
  NOCLOBBER int	res;

  va_start(ap, format);
    
  TRY
    res = _printf(stream, format, ap);
  FINALLY
    va_end(ap);
  ENDTRY;
    
  return res;
}


/*
 * end printf.c
 */
