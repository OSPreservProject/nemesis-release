/*  -*- Mode: C;  -*-
 * File: atof.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Convert s to double. Returns HUGE_VAL on o'flow, 0 on u'flow.
 **           At Nemesis entry RAISES stdlib_uflow, stdlib_oflow, 
 **           stdlib_arginval.
 **
 ** HISTORY:
 ** Created: Tue May  3 13:16:42 1994 (jch1003)
 ** Last Edited: Mon Oct 31 15:11:51 1994 By James Hall
 **
    $Log: atof.c,v $
    Revision 1.2  1995/02/28 10:10:40  dme
    reinstate copylefts

 * Revision 1.2  1994/10/31  15:12:08  jch1003
 * Moved from ../atof.c, naming changes, self passed as arg., exceptions
 * introduced.
 *

 * Revision 1.1  1994/08/22  11:07:57  jch1003
 * Initial revision
 *
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <nemesis.h>
#include <stdlib.h>

extern double
atof(const char *s)
{
  return strtod(s, (char **) NULL);
}

/*
 * end atof.c
 */






