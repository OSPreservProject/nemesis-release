/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      fread.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      libc
** 
** ID : $Id: fread.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <stdio.h>

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *f)
{
    size_t	nbytes = size * nmemb;
    uint32_t	nread;

    if (!f)
    {
	fprintf(stderr, "fread: passed NULL `FILE *'\n");
	return 0;
    }

    if (!size)
    {
	fprintf(stderr, "fread: passed 0 as item size\n");
	return 0;
    }

    nread = Rd$GetChars(f->rd, ptr, nbytes);
    return nread / size;
}

/* End of fread.c */
