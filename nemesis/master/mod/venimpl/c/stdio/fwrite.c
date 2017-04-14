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
**      fwrite.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      libc
** 
** ID : $Id: fwrite.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <stdio.h>

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *f)
{
    size_t	nbytes = size * nmemb;

    if (!f)
    {
	fprintf(stderr, "fwrite: passed NULL `FILE *'\n");
	return 0;
    }

    /* XXX no way to find out how many chars were _actually_ written :( */
    /* XXX Wr.if discards const from ptr. Bad move, dude. */
    Wr$PutChars(f->wr, (char *)ptr, nbytes);

    return nmemb;
}

/* End of fread.c */
