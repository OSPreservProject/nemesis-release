/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
 * File:
 *
 *      doprnt.c
 *
 * Description:
 *
 *	These routines form the guts of printf and friends
 *
 * Notes:
 *
 *	How to handle errno.
 *
 * Revision History:
 *
 *      $Id: doprnt.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 */

#include <nemesis.h>
#include <stdio.h>
#include <string.h>

#include <Wr.ih>
#include <TypeSystem.ih>

#define Iputc(c, s)	((s)->op->LPutC)((s), (c))

#define _ferror(s)	(0)

/*******************************
 * CODE from Sai Lai Lo.       *
 *******************************/

#define between(c,l,u)	((unsigned short) ((c) - (l)) <= ((u) - (l)))

#define read_dec(total,digit) \
  do { while (between(*digit,'0','9'))  \
	 total = total * 10 + ((unsigned short) (*(digit)++) - '0'); \
     } while(0)


/* These are or-ed together */

#define SP_LEFTJUST 	0x0001
#define SP_PADZERO	0x0002
#define SP_SIGNPLUS	0x0004
#define SP_SIGNSPACE	0x0008
#define SP_VARIANT	0x0010
#define SP_ULONG	0x0020
#define SP_USHORT	0x0040
#define SP_UNSIGN	0x0080
#define SP_ULLONG	0x0100

typedef struct {
    unsigned 		flags,width,precision;
    char		conversion;
    int			count; /* keep track of how much char we have output */
} Spec;

#define MAX_WIDTH	(32)

/*****************************************************************************\
*									      *
*   The format specification takes the following form:			      *
*      %<flags><field width><precision><length>conversion		      *
*									      *
*   flags								      *
*        Zero or more of the following:					      *
*        -, +, space, 0, #						      *
*   field width								      *
*        a decimal integer or an asterisk '*'				      *
*   precision								      *
*        Starts with a period '.' and follow by a decimal integer or an	      *
*        asterisk '*'							      *
*   length								      *
*        <empty> or "l" or  "ll" or "h"                                       *
*   conversion								      *
*        one of the following:						      *
*        d, i, u, o, p, q, Q, x, X, f, e, E, g, G, c, s, %		      *
*									      *
\*****************************************************************************/

static void
parse(Spec *s, char **format, va_list *pvar)
{
  bool_t done = False;

  s->flags = s->width = s->precision = 0;
  do
    {
      switch (**format)
	{
	case '-': s->flags |= SP_LEFTJUST; (*format)++; break;
	case '0': s->flags |= SP_PADZERO;  (*format)++; break;
	case '+': s->flags |= SP_SIGNPLUS; (*format)++; break;
	case ' ': s->flags |= SP_SIGNSPACE; (*format)++; break;
	case '#': s->flags |= SP_VARIANT; (*format)++;  break;
	default: done = True;
	}
    } while(! done);
  if (**format == '*')
    {
      s->width = va_arg(*pvar,int);
      (*format)++;
    }
  else
    read_dec(s->width,*format);
  if (**format == '.')
    {
      (*format)++;
      if (**format == '*')
	{
	  s->precision = va_arg(*pvar,int);
	  (*format)++;
	}
      else
	read_dec(s->precision,*format);
    }
  switch (**format)
    {
    case 'l':
      s->flags += SP_ULONG;  (*format)++;
      if ((**format) == 'l') { s->flags += SP_ULLONG;  (*format)++; }
      break;
    case 'h': s->flags += SP_USHORT; (*format)++; break;
    }
  s->conversion = **format;
  if (s->conversion == '\0')
      return;
  else
      (*format)++;
  if (s->conversion == 'q')
  {
      if (**format == '\0') return; /* missing needed specifier */
      (*format)++;  /* skip specifier: we can only print them as 'x' */
  }

  /* XXX HACK HACK HACK */
  if (s->flags & SP_ULLONG) s->conversion = 'q';
}

static void
printnum(long n, int base, Spec *s, Wr_cl *stream)
{
  register short mod;
  register unsigned long value;
  char a[MAX_WIDTH];
  char *v = a;
  char *digit;
  bool_t sign;				/* NOT used uninitialized */
  int pad1,pad2;
  int prefix = 0;

  if (s->conversion == 'X')
    digit = "0123456789ABCDEF";
  else
    digit = "0123456789abcdef";
  if ((base==10) && (sign= (!(s->flags & SP_UNSIGN))) && (n<0))
    value = (unsigned long) (-n);
  else
    value = (unsigned long) n;
  if (s->flags & SP_USHORT) value = (unsigned short) value;
  do
    {
      mod = value % base;
      value = value / base;
      *v++ = digit[mod];
    } while (value);
  if (!s->precision)
    s->precision = (v-a);
  pad2 = s->precision - (v - a);
  if (pad2 > 0) s->count += s->precision;
  else s->count += (v-a);
  pad1 = s->width - s->precision;
  if (base == 10)
    {
      if (sign)
	if (n>=0)
	  {
	    if (s->flags & SP_SIGNPLUS)
	      {
		*v++ = '+';
		prefix++;
		pad1--;
	      }
	    else if (s->flags & SP_SIGNSPACE)
	      {
		*v++ = ' ';
		prefix++;
		pad1--;
	      }
	  }
	else
	  {
	    *v++ = '-';
	    prefix++;
	    pad1--;
	  }
    }
  else
    if (s->flags & SP_VARIANT)
      switch (base)
	{
	case 8: *v++ = '0'; prefix++; pad1--; break;
	case 16:
	        if (s->conversion == 'X')
		  *v++ = 'X';
		else
		  *v++ = 'x';
	        *v++ = '0';
	        prefix += 2;
	        pad1 -= 2;
	        break;
	}
  if (pad1 > 0) s->count += prefix + pad1;
  else s->count += prefix;
  v--;
  if (! (s->flags & SP_LEFTJUST))
    {
      if (s->flags & SP_PADZERO)
	{
	  for (; prefix > 0; prefix--) Iputc(*v--,stream);
	  for (; pad1 >0; pad1--) Iputc('0',stream);
	  for (; pad2 >0; pad2--) Iputc('0',stream);
	}
      else
	{
	  for (; pad1 >0; pad1--) Iputc(' ',stream);
	  for (; prefix > 0; prefix--) Iputc(*v--,stream);
	  for (; pad2 > 0; pad2--) Iputc('0',stream);
	}
    }
  for(; v >= a; v--) Iputc(*v,stream);
  for(; pad1 > 0; pad1--) Iputc(' ',stream);
}

int
libc_doprnt(char *fmt, va_list pvar, Wr_cl *stream);

static int apr(Wr_clp stream, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

static int apr(Wr_clp stream, char *fmt, ...) {
    va_list alist;
    int res;

    va_start(alist, fmt);
    res = libc_doprnt(fmt, alist, stream);
    va_end(alist);

    return res;
}
    
    
static void print_any(Wr_clp stream, Type_Any *any, bool_t full) {

    Type_Code tc = TypeSystem$UnAlias(Pvs(types), any->type);
    Type_Any rep;
    Interface_clp scope;

    scope = TypeSystem$Info(Pvs(types), tc, &rep);

    switch(rep.type) {

    case TypeSystem_Predefined__code:
    {

	switch(any->type) {
	case string_t__code:
	    apr(stream, "\"%s\"", (string_t)(pointerval_t)(any->val));
	    break;
	
	case bool_t__code:
	    apr(stream, "%s", any->val?"True":"False");
	    break;
	    
	case uint8_t__code:
	case uint16_t__code:
	case uint32_t__code:
	case int8_t__code:  
	case int16_t__code: 
	case int32_t__code: 
	    apr(stream, "%d", (uint32_t)any->val);
	    break;

	case uint64_t__code:
	case int64_t__code:
	    apr(stream, "0x%qx", any->val);
	    break;
	
	case word_t__code:
	    if(sizeof(word_t) == 8) {
		apr(stream, "0x%qx", any->val);
	    } else {
		apr(stream, "0x%x", (uint32_t) any->val);
	    }
	    break;

	case addr_t__code:
	    apr(stream, "%p", (addr_t) (pointerval_t)  any->val);
	    break;

	default:
	    apr(stream, "0x%qx", any->val);
	    break;

	}

	break;
    }

    case TypeSystem_Iref__code:
    case TypeSystem_Ref__code:
	apr(stream, "%p", (addr_t) (pointerval_t) any->val);
	break;

    case TypeSystem_Enum__code:
    {
	Context_Names *names;
	uint32_t enumval = any->val;
	names = Context$List((Context_clp)(pointerval_t)rep.val);
	if(enumval < SEQ_LEN(names)) {
	    apr(stream, "%s", SEQ_ELEM(names, enumval));
	} else {
	    apr(stream, "UNKNOWN");
	}
	SEQ_FREE_ELEMS(names);
	SEQ_FREE(names);
	break;
    }

    default:
	apr(stream, "0x%qx", any->val);
	break;

    }

}
int
libc_doprnt(char *fmt, va_list pvar, Wr_cl *stream)
{
  char ch;
  char *format = fmt;
  Spec s;
  char *outstr;
  int outlen;
  char padchar;
  int padding;
  unsigned long tmp;

  s.count = 0;
  while ((ch = *format++))
    {
      if (ch == '%')
	{
	  (void) parse(&s,&format,&pvar);
	  switch(s.conversion)
	    {
	    case 'I':
		      tmp = va_arg(pvar, uint32_t);
#ifdef BIG_ENDIAN
		      printnum((tmp>>24),10,&s,stream);
		      s.flags = s.width = s.precision = 0;
		      Iputc('.',stream);
		      printnum((tmp>>16)&0xff,10,&s,stream);
		      s.flags = s.width = s.precision = 0;
		      Iputc('.',stream);
		      printnum((tmp>>8)&0xff,10,&s,stream);
		      s.flags = s.width = s.precision = 0;
		      Iputc('.',stream);
		      printnum((tmp)&0xff,10,&s,stream);
#else
		      printnum((tmp)&0xff,10,&s,stream);
		      s.flags = s.width = s.precision = 0;
		      Iputc('.',stream);
		      printnum((tmp>>8)&0xff,10,&s,stream);
		      s.flags = s.width = s.precision = 0;
		      Iputc('.',stream);
		      printnum((tmp>>16)&0xff,10,&s,stream);
		      s.flags = s.width = s.precision = 0;
		      Iputc('.',stream);
		      printnum((tmp>>24),10,&s,stream);
#endif
		      break;

	    case 'd':  
	    case 'i':
	              tmp = (s.flags & SP_ULONG) ?
			va_arg(pvar, unsigned long) : 
			  va_arg(pvar, unsigned int);	         
		      (void) printnum(tmp,10,&s,stream);
		      break;
	    case 'u':
		      s.flags += SP_UNSIGN;
	              tmp = (s.flags & SP_ULONG) ?
			va_arg(pvar, unsigned long) : 
			  va_arg(pvar, unsigned int);
		      (void) printnum(tmp,10,&s,stream);
		      break;
	    case 'o':
	              tmp = (s.flags & SP_ULONG) ?
			va_arg(pvar, unsigned long) : 
			  va_arg(pvar, unsigned int);
		      (void) printnum(tmp,8,&s,stream);
		      break;
	    case 'p':		/* print a pointer */
		      s.conversion = 'x';
		      s.flags = SP_PADZERO + SP_UNSIGN + SP_ULONG + SP_VARIANT;
		      s.width = 2 + (2 * sizeof(void *));
	    case 'x':
	    case 'X':
	              tmp = (s.flags & SP_ULONG) ?
			va_arg(pvar, unsigned long) : 
			  va_arg(pvar, unsigned int);
		      (void) printnum(tmp,16,&s,stream);
		      break;
	    case 'q':
	    case 'Q': /* Print a quadword (uint64_t) */
		      {
			uint64_t quad = va_arg(pvar,uint64_t);
			s.conversion = (s.conversion == 'q') ? 'x' : 'X';

			/* XXX This is a gross hack */
			s.flags = SP_PADZERO + SP_UNSIGN + SP_ULONG;
			s.width = (2 * sizeof(tmp));
			if (sizeof(tmp) < sizeof(uint64_t)) {
			  /* Print as two longs */
			  tmp = quad >> (8 * sizeof(tmp));
			  (void) printnum(tmp, 16, &s, stream);
			  s.precision = 0; /* since its nuked by printnum() */
			}
			tmp = (unsigned long)quad;
			(void) printnum(tmp, 16, &s, stream);
		      }
		      break;
	    case 's':
	    case 'S':
	              outstr = va_arg(pvar,char *);
		      outlen = strlen(outstr);
	              if ((s.precision) && (s.precision < outlen))
		          outlen = s.precision;
		      padding = s.width - outlen;
		      if (padding > 0)
			s.count += padding + outlen;
		      else
			s.count += outlen;
		      if (! (s.flags & SP_LEFTJUST))
			for (; padding > 0; padding--) Iputc(' ',stream);
		      for (; outlen > 0; outlen--)
			Iputc(*outstr++,stream);
		      for (; padding >0; padding--) Iputc(' ',stream);
	              break;
	    case 'c':
            case 'C':
		      padding = s.width - 1;
		      (padding > 0)? s.count +=padding + 1: s.count++;
		      if (s.flags & SP_PADZERO)
			padchar = '0';
		      else
			padchar = ' ';
		      if (! (s.flags & SP_LEFTJUST))
			for (; padding > 0; padding--) Iputc(padchar,stream);
		      Iputc((char) va_arg(pvar,int),stream);
		      for (; padding > 0; padding--) Iputc(' ',stream);
		      break; 
	    case '%':
		      padding = s.width - 1;
		      (padding > 0)? s.count +=padding + 1: s.count++;
		      if (s.flags & SP_PADZERO)
			padchar = '0';
		      else
			padchar = ' ';
		      if (! (s.flags & SP_LEFTJUST))
			for (; padding > 0; padding--) Iputc(padchar,stream);
		      Iputc('%',stream);
		      for (; padding > 0; padding--) Iputc(' ',stream);
		      break; 
	    case 'a':
		print_any(stream, va_arg(pvar, addr_t), False);
		break;

	    case 'A':
		print_any(stream, va_arg(pvar, addr_t), True);
		break;

	    case 'f': 
	    case 'e':
	    case 'E': /* unimplemented converion, just output the conversion*/
	    case 'g': /* char and skip the corresponding argument */
	    case 'G':
	              Iputc(s.conversion,stream); s.count++;

/* XXX - with -mno-fp-regs (currently required on Alpha), gcc barfs
   on even the slightest smell of a double.  Hence the following twaddle. */

#ifndef __ALPHA__
		      (void) va_arg (pvar, double);
#else
		      (void) va_arg (pvar, long);
		                          /* sizeof(long) == sizeof(double) */
#endif
	              break;
	    default:  
		      return(EOF);
	    }
	}
      else
	{
	  Iputc(ch,stream); s.count++;
	}
    }

  return _ferror(stream) ? EOF : s.count;
}

/*
 * Code from Sai Lai Lo is now finished
 */

/*
 * End doprnt.c
 */
