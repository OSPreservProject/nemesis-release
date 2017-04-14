/*
 *    MORE CODE FROM SAI LAI LO
 */

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <Rd.ih>

#define Igetc(s)	((s)->op->GetC)(s)
#define Iungetc(c, s)   ({ ((s)->op->UnGetC)(s); c; })

#define NCHARS	(1 << 8)


#if defined(__IX86__)
#define HAVE_FLOAT
#include <math.h>
#endif

static unsigned char *
setup(fmt, tab)
     register unsigned char *fmt;
     register char *tab;
{
  register int b, c, d, t = 0;

  if(*fmt == '^') {
    t++;
    fmt++;
  }
  (void)memset(tab, !t, NCHARS);
  if((c = *fmt) == ']' || c == '-') { /* first char is special */
    tab[c] = t;
    fmt++;
  }
  while((c = *fmt++) != ']') {
    if(c == '\0')
      return(NULL);		/* unexpected end of format */
    if(c == '-' && (d = *fmt) != ']' && (b = fmt[-2]) < d) {
      (void)memset(&tab[b], t, d - b + 1);
      fmt++;
    } else
      tab[c] = t;
  }
  return(fmt);
}

static void
store_int(value,length,arg_list)
     long 	value;
     int	length;
     va_list	*arg_list ;
{
  switch(length) {
  case 'l': *va_arg(*arg_list, long *)  = value ;
    break ;
  case 'h': *va_arg(*arg_list, short *) = (short)value ;
    break ;
  default:  *va_arg(*arg_list, int *)   = (int)value ;
    break ;
  }
}

/***************************************************************
 * Functions to read the input stream in an attempt to match incoming
 * data to the current pattern from the main loop of libc_doscan().
 ***************************************************************/

static int
number(stow, type, len, size, stream, chars_scanned, listp)
     int stow, type, len, size;
     Rd_cl *stream;
     register int *chars_scanned;
     va_list *listp;
{
  char numbuf[64];
  register char *np = numbuf;
  register int c, base;
  int	digitseen = 0,
  dotseen = 0,
  expseen = 0,
  floater = 0,
  negflg = 0;
  long lcval = 0;

  switch(type) {
  case 'e':
  case 'f':
  case 'g':
    floater++;
    /* FALLTHROUGH */
  case 'd':
  case 'u':
    base = 10;
    break;
  case 'o':
    base = 8;
    break;
  case 'x':
    base = 16;
    break;
  case 'i':
    base = 0;			/* General integer */
    break ;
  default:
    return(0);			/* unrecognized conversion character */
  }

  switch(c = Igetc(stream)) {
  case '-':
    negflg++;
    if(type == 'u')		/* DAG -- unsigned `-' check */
      break;
  case '+':			/* fall-through */
    len--;
    c = Igetc(stream);
    (*chars_scanned)++ ;
  }
  if (base == 0) {		/* general integer, test format */
    if (c == '0') {
      c = Igetc(stream) ;
      if (c == 'x' || c == 'X') {
	/* 0x or 0X mean hex, get next digit and
	 * mark the 0[Xx] as scanned.
	 */
	base = 16 ;
	c = Igetc(stream);
	*chars_scanned += 2 ;
      } else if (isdigit(c)) {
	/* 0 followed by a digit is octal,
	 * mark 0 as scanned.
	 */
	base = 8 ;
	(*chars_scanned)++ ;
      } else {
	/* Decimal 0, restart scan at 0.
	 */
	Iungetc(c,stream); 
	c = '0';
	base = 10 ;
      }
    } else
      base = 10 ;
  }
  if(!negflg || type != 'u')	/* DAG -- unsigned `-' check */
    /* DAG -- added safety check on `np' */
    for( ; --len >= 0 && np < &numbuf[63]; *np++ = c, c = Igetc(stream)) {
      if(isdigit(c) || (base == 16 && isxdigit(c))) {
	int digit = c - (isdigit(c) ? '0' :
			 isupper(c) ? 'A' - 10 : 'a' - 10);
	if(digit >= base)
	  break;
	if(stow && !floater)
	  lcval = base * lcval + digit;
	digitseen++;
	(*chars_scanned)++ ;
	continue;
      }
      if(!floater)
	break;
      if(c == '.' && !dotseen++)
	continue;
      if((c == 'e' || c == 'E') && digitseen && !expseen++) {
	*np++ = c;
	c = Igetc(stream);
	if(isdigit(c) || c == '+' || c == '-')
	  continue;
      }
      break;
    }
  if(stow && digitseen)
    if(floater) {
#ifdef HAVE_FLOAT
      register double dval;
	
      *np = '\0';
      dval = atof(numbuf);
      if(negflg)
	dval = -dval;
      if(size == 'l')
	*va_arg(*listp, double *) = dval;
      else
	*va_arg(*listp, float *) = (float)dval;
#endif /* HAVE_FLOAT */
    } else {
      /* suppress possible overflow on 2's-comp negation */
/*** XXX
      if(negflg && lcval != HIBITL)
	lcval = -lcval;
***/
      store_int(lcval,size,listp);
    }

  if (Iungetc(c, stream) == EOF) return -1;

  return(digitseen);		/* successful match if non-zero */
}

static int
string(stow, type, len, tab, stream, chars_scanned, listp)
     register int stow, type, len;
     register char *tab;
     Rd_cl *stream;
     int *chars_scanned;
     va_list *listp;
{
  register int ch;
  register char *ptr;
  char *start;

  start = ptr = stow ? va_arg(*listp, char *) : NULL;
  if(type == 'c' && len == INT_MAX)
    len = 1;
  while((ch = Igetc(stream)) != EOF &&
	!((type == 's' && isspace(ch)) || (type == '[' && tab[ch]))) {
    if(stow)
      *ptr = ch;
    ptr++;
    (*chars_scanned)++;
    if(--len <= 0)
      break;
  }

  if(ch == EOF || (len > 0 && Iungetc(ch, stream) == EOF))
    return -1;			/* end of file */

  if(ptr == start)
    return(0);			/* no match */
  if(stow && type != 'c')
    *ptr = '\0';
  return(1);			/* successful match */
}

int
libc_doscan(fmt, pvar, stream)
     char    *fmt;
     va_list *pvar;
     Rd_cl   *stream;
{
  char tab[NCHARS];
  register int ch;
  int 	nmatch = 0,
  len,
  inchar,
  stow,
  size,
  chars_scanned = 0 ;

  /*******************************************************
   * Main loop: reads format to determine a pattern,
   *		and then goes to read input stream
   *		in attempt to match the pattern.
   *******************************************************/
  for( ; ; ) {
    if((ch = *fmt++) == '\0') {
      return(nmatch);		/* end of format */
    }

    if(isspace(ch)) {
      while(isspace(inchar = Igetc(stream)))
	chars_scanned++;
      if(Iungetc(inchar, stream) != EOF)
	continue;
      break;
    }
    if(ch != '%' || (ch = *fmt++) == '%') {
      if((inchar = Igetc(stream)) == ch) {
	chars_scanned++;
	continue;
      }
      if(Iungetc(inchar, stream) != EOF) {
	return(nmatch);		/* failed to match input */
      }
      break;
    }

    if(ch == '*') {
      stow = 0;
      ch = *fmt++;
    } else
      stow = 1;

    for(len = 0; isdigit(ch); ch = *fmt++)
      len = len * 10 + ch - '0';
    if(len == 0)
      len = INT_MAX;

    if((size = ch) == 'l' || size == 'h')
      ch = *fmt++;

    if(ch == '\0' ||
       (ch == '[' && (fmt = setup(fmt, tab)) == NULL))
      return(EOF);		/* unexpected end of format */

    if(isupper(ch)) {		/* no longer documented */
      size = 'l';
      ch = tolower(ch);
    }

    if (ch == 'n') {
      if (stow) {
	store_int((long)chars_scanned,size,pvar);
	nmatch++ ;
      }
      continue;
    }

    if(ch != 'c' && ch != '[') {
      while(isspace(inchar = Igetc(stream)))
	chars_scanned++;
      if(Iungetc(inchar, stream) == EOF)
	break;
    }

    if((size = (ch == 'c' || ch == 's' || ch == '[') ?
	string(stow,ch,len,tab,stream,&chars_scanned,pvar) :
	number(stow,ch,len,size,stream,&chars_scanned,pvar))
       > 0)
      nmatch += stow;

    if(size < 0)
      break;

    if(size == 0) {
      return(nmatch);		/* failed to match input */
    }
  }
  return(nmatch != 0 ? nmatch : EOF); /* end of input */
}
