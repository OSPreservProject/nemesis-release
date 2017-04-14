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
**      ferror.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      Libc
** 
** ID : $Id: ferror.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <stdio.h>

int ferror(FILE *f)
{
    return f->error;
}

void clearerr(FILE *f)
{
    f->error = False;
}
