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
**      fgets.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      libc
** 
** ID : $Id: fgets.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <stdio.h>

#include <Rd.ih>

char *fgets(char *s, int size, FILE *f)
{
    uint32_t		nread;

    nread = Rd$GetLine(f->rd, s, size-1);

    /* null-terminate the string */
    s[nread] = 0;

#undef EOF
    if ((nread == 0) && Rd$EOF(f->rd))
	return NULL;
    else
	return s;
}
