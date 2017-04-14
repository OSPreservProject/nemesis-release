/* CLUless 
 *
 * ******************
 * ***NOT REQUIRED***
 * ****************** 
 *
 * Portable implementation of vsnprintf
 *
 * Dickon Reed, 1996
 * Richard Mortier, 1997
 *
 *
 * $Id: brokenprintf.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: brokenprintf.c,v $
 * Revision 1.1  1998/04/28 14:36:23  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:21:38  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 * (This file is now not required)
 *
 * Revision 1.10  1997/02/04 16:44:10  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 18:31:36  rmm1002
 * *** empty log message ***
 *
 * Revision 1.1  1997/02/03 17:03:18  rmm1002
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

int vsnprintf( char *str, size_t n, const char *format, va_list ap) {

#define OUTCHAR(c) if (count++<n-1) *(outptr++) = c; 
#define OUTSTR(str) if (strlen(str)+count<n-1) { strcpy(outptr, str); count += strlen(str); outptr += strlen(str); }

  int count;
  char *outptr;
  const char *ptr;

  count=0;
  ptr = format;
  outptr = str;
  while (*ptr) {
    switch (*ptr) {
      case '%':
	ptr++;
	switch (*ptr) {
	  case '%':
	    OUTCHAR('%');
	    break;

	  case 's': {
	    char *substr;
	    
	    substr = va_arg(ap, char *);
	    if (substr) {
		OUTSTR(substr);
	    } else {
		OUTSTR("*NULL*");
	    }
	  }
	  break;

	  case 'd': {
	    char buf[40];
	
	    sprintf(buf,"%d", va_arg(ap, int));
	    OUTSTR(buf);
	  }
	  break;

	  case 'f': {
	    char buf[80];
	
	    sprintf(buf,"%f", va_arg(ap, double));
	    OUTSTR(buf);
	  }
	  break;

	  default:
	    fprintf(stderr, "Unimplemented printf directive character %c\n", *ptr);
	}
	ptr++;
	break;

      default:
	OUTCHAR(*ptr);
    }
    ptr++;
  }
  return count;
}

char *strdup(const char *src) {
  int l;
  char *buf;

  l = strlen(src);
  buf = malloc( sizeof(char) * (l+1) );
  strcpy(buf, src);
  return buf;
}


