/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
 *
 * File:
 *
 *      vprintf.c
 *
 * Description:
 *
 *	Format a string and print it on the specified output stream.
 *      Varargs versions thereof.
 *      At Nemesis entry level RAISES stdio_streamerr, stdio_cantlock.
 *      Main purpose of this file is for POSIX compliance, not speed :)
 *
 * Revision History:
 *
 *      $Id: vprintf.c 1.2 Thu, 18 Feb 1999 14:18:09 +0000 dr10009 $
 *
 */

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
/*#include <stdarg.h>*/

/*
 * imports
 */

extern int _printf (Wr_cl *stream, const char *format,  va_list ap);
extern int _sprintf(char *s, const char *format, va_list ap);

/*
 * exports
 */

PUBLIC int 
vfprintf(FILE *stream, const char *format, va_list ap)
{
    return _printf(stream->wr, format, ap);
}

PUBLIC int
vprintf(const char *format, va_list ap)
{
    return _printf(Pvs(out), format, ap);
}

/* This one isn't actually used in the jumptable, _sprintf is. This
 * one is for people who want to link against libc statically */
#if 0
PUBLIC int
vsprintf(char *s, const char *format, va_list ap)
{
    return _sprintf(s, format, ap);
}
#endif /* 0 */

/* End vprintf.c */
