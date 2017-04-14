/*
 *      fscanf.c
 *
 * Description:
 *
 *	Scan on the specified input stream
 *      At Nemesis entry level RAISES stdio_streamerr, stdio_cantlock.
 *
 * Revision History:
 *
 *      $Id: fscanf.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 */

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

/*
 * imports
 */

extern int libc_doscan(const char *format, va_list *ap, Rd_cl *stream);

extern int _scanf (Rd_cl *stream, const char *format, va_list *ap);

/*
 * exports
 */


PUBLIC int 
fscanf(FILE *stream, const char *format, ...)
{
  va_list		ap;
  NOCLOBBER int		res;

  va_start(ap, format);

  TRY
    res = _scanf(stream->rd, format, &ap);
  FINALLY
    va_end(ap);  
  ENDTRY;
  return res;
}



PUBLIC int 
_scanf (Rd_cl *stream, const char *format, va_list *ap)
{
  int	 res;

  res = libc_doscan(format, ap, stream);

  if(res < 0) RAISE("stdio_streamerr", NULL);
  return res;
}

/*
 * End fscanf.c
 */
