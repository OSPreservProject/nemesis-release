/*
 *	assert.c
 *	--------
 *
 * $Id: assert.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 * Copyright (c) 1989, 1990, 1991, 1994, 1995 University of Cambridge
 *
 * From Wanda, via Fawn
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

__NORETURN(badassertion(char *file, int line));

void badassertion(char *file, int line)
{
    eprintf("assertion failed in %s, line %d\n", file, line);
    abort();
}

__NORETURN(badassertionm(char *file, int line, char *msg));

void badassertionm(char *file, int line, char *msg)
{
    eprintf("assertion failed in %s, line %d: %s\n", file, line, msg);
    abort();
}

__NORETURN(badcompare(char *file, int line, long a));

void badcompare(char *file, int line, long a)
{
    eprintf("comparison failed in %s, line %d (%ld)\n", file, line, a);
    abort();
}

/* End of $Id: assert.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $ */
