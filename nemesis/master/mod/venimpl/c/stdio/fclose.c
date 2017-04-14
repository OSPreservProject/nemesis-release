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
**      fclose.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      POSIX/ANSI C compliant filesystem thingy
** 
** ENVIRONMENT: 
**
**      libc
** 
** ID : $Id: fclose.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <stdio.h>

#include <Rd.ih>
#include <Wr.ih>
#include <IO.ih>
#include <Heap.ih>

int fclose(FILE *f)
{
    if (!f)
    {
	fprintf(stderr, "fclose: passed NULL `FILE *'\n");
	return -1;
    }

    if (f->rd) Rd$Close(f->rd);
    if (f->wr) Wr$Close(f->wr);
    
    /* The file will have been closed by fsutils */
/*    IO$Dispose((IO_clp)f->file); */

    Heap$Free(Pvs(heap), f);

    return 0;
}

/* End of fclose.c */
