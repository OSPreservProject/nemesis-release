/*
 *	kprintf.c
 *	---------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * $Id: kprintf.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 * This provdes debugging printing for the kernel only. Its very simple.
 */

#include <kernel.h>
#include "console.h"
#include "crash.h"
#include <stdarg.h>
#include <stddef.h>
#ifdef __SMP__
#include <smp.h>
static unsigned long smp_printf_lock = 0;
#endif

static const char hex[16] = "0123456789abcdef";

#define printit(x)						\
do {								\
    int _i;							\
    console_putbyte('0');						\
    console_putbyte('x');						\
    for(_i=0; _i < 2*sizeof(x); _i++, (x)<<=4)			\
	console_putbyte(hex[((x) >> (8 * sizeof(x) -4)) & 0xf]);	\
} while(0)

static void doprnt(const char *format, va_list args)
{
    char	c;
    long        p;		/* %p */
    long	l;		/* %l */
    int		i;		/* %i */
    Time_ns	t;		/* %t */
    size_t	z;		/* %z */
    char	*s;		/* %s */
    word_t	w;		/* %w */
    uchar_t     b;		/* %b */

    console_putdom(-1);

    while ((c = *format++))
    {
	if (c == '%')
	{
	    switch ((c = *format++))
	    {
	    case 0:		/* shouldn't happen, but... */
		return;

	    case 'p':
		p = ((long) (va_arg(args, void *)));
		printit(p);
		break;

	    case 'l':
	    case 'x':
		l = va_arg(args, long);
		printit(l);
		break;
		
	    case 'i':
		i = va_arg(args, int);
		printit(i);
		break;
		
	    case 'w':
		w = va_arg(args, word_t);
		printit(w);
		break;
		
	    case 't':
		t = va_arg(args, Time_ns);
		printit(t);
		break;

	    case 'z':
		z = va_arg(args, size_t);
		printit(z);
		break;
		
	    case 's':
		s = va_arg(args, char *);
		if (s) while ((c = *s++)) console_putbyte(c);
		break;

	    case 'b':
		b = va_arg(args, size_t);
		printit(b);
		break;

	    case '%':
	    default:
		console_putbyte(c);
		break;
	    }
	}
	else
	    console_putbyte(c);
    }
}

void k_printf(const char *format, ...)
{
    va_list		ap;

#ifdef __SMP__
    SMP_SPIN_LOCK(&smp_printf_lock);
		console_putbyte('[');
		{ unsigned char b = (smp_cpuid_phys() & 0xff); printit(b); }
		console_putbyte(']'); console_putbyte(' ');
#endif
    
    va_start(ap, format);
    doprnt(format, ap);
    va_end(ap);

#ifdef __SMP__
    SMP_SPIN_UNLOCK(&smp_printf_lock);
#endif


}

/* We want to do a blue-screen-of-death of type 'panic' here really. */
/* NORETURN */ void k_panic(const char *format, ...)
{
    va_list		ap;
    
    start_crashdump("Kernel panic\n");

    va_start(ap, format);
    doprnt(format, ap);
    va_end(ap);
    console_putbyte('\n');

    end_crashdump();

    k_halt();
}

/* End of kprintf.c */
