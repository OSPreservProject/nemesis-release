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
**      fputc.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      libc
** 
** ID : $Id: fputc.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <stdio.h>

int fputc(int c, FILE *f)
{
    Wr$PutC(f->wr, c);
    return (int)((unsigned char)c);
}
