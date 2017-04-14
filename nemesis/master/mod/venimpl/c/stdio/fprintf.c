/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
 *
 * File:
 *
 *      fprintf.c
 *
 * Description:
 *
 *	Format a string and print it on the specified output stream
 *      At Nemesis entry level RAISES stdio_streamerr, stdio_cantlock.
 *
 * Revision History:
 *
 *      $Id: fprintf.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 */

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

#ifdef PRINTF_DOMAIN_COLOURS
#warning Using COLOUR for printf

#define FG(c, s)				\
Wr$LPutC((s), 27);				\
Wr$LPutC((s), 91);				\
Wr$LPutC((s), 51);				\
Wr$LPutC((s), 48+(c));				\
Wr$LPutC((s), 109);

#define DOMAIN(d, s) FG(1+(d), s);
#define NODOMAIN(s)  FG(11, s)

#endif

#ifdef PRINTF_DOMAIN_IDS
#include <VPMacros.h>
#warning Using Domain IDs for printf

static const char hex_table[] = { '0', '1', '2', '3', '4', '5', '6', '7',
			   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

#define DOMAIN(d, s)				\
Wr$LPutC((s), hex_table[((d)>>4)&15]);          \
Wr$LPutC((s), hex_table[(d)&15]);               \
Wr$LPutC((s), ':');				\
Wr$LPutC((s), ' ');				\

#endif

/*
 * imports
 */

extern int libc_doprnt(const char *format, va_list ap, Wr_cl *stream);

/*
 * exports
 */

extern int _printf (Wr_cl *stream, const char *format,  va_list ap);


PUBLIC int 
fprintf(FILE *stream, const char *format, ...)
{
  va_list		ap;
  NOCLOBBER int		res;

  va_start(ap, format);

  TRY
    res = _printf(stream->wr, format, ap);
  FINALLY
    va_end(ap);  
  ENDTRY;
  return res;
}



PUBLIC int 
_printf (Wr_cl *stream, const char *format,  va_list ap)
{
  NOCLOBBER int	 res;

  Wr$Lock (stream);
  TRY
#ifdef DOMAIN
    DOMAIN(VP$DomainID(Pvs(vp)) & 255, stream);
#endif DOMAIN
    res = libc_doprnt(format, ap, stream);
#ifdef NODOMAIN
    NODOMAIN(stream);
#endif NODOMAIN
  FINALLY
  {
    Wr$LFlush (stream);
    Wr$Unlock (stream);
  }
  ENDTRY;

  if(res < 0) RAISE("stdio_streamerr", NULL);
  return res;
}

/*
 * End fprintf.c
 */
