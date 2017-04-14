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
**      fgetc.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      libc
** 
** ID : $Id: fgetc.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

#include <Rd.ih>

int fgetc(FILE *f)
{
    NOCLOBBER int c;

    TRY
    {
	c = Rd$GetC(f->rd);
    }
    CATCH_Rd$EndOfFile()
    {
	c = -1;
    }
    ENDTRY

    return c;
}
