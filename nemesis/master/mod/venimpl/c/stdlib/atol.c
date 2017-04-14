/*  -*- Mode: C;  -*-
 * File: atol.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Convert s to long. Return INT_MAX/MIN on o'flow.
 **           Nemesis entry level RAISES stdlib_oflow.
 **
 ** HISTORY:
 ** Created: Tue May  3 15:58:34 1994 (jch1003)
 ** Last Edited: Thu Oct  6 15:17:45 1994 By James Hall
 **
    $Log: atol.c,v $
    Revision 1.2  1995/02/28 10:10:40  dme
    reinstate copylefts

 * Revision 1.2  1994/10/31  15:22:05  jch1003
 * Moved from ../atol.c, naming changes, self passed as arg, exceptions
 * introduced.
 *
 * Revision 1.1  1994/08/22  11:10:49  jch1003
 * Initial revision
 *
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <nemesis.h>
#include <stdlib.h>

extern long int
atol(const char *s)
{
  return strtol(s, (char **) NULL, 10);
}

/*
 * end atol.c
 */



