/*  -*- Mode: C;  -*-
 * File: atoi.c
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library.
 **
 ** FUNCTION: Convert s to int. Returns INT_MIN/MAX on o'flow.
 **           Entry at Nemesis level RAISES stdlib_oflow.
 **
 ** HISTORY:
 ** Created: Tue May  3 15:25:59 1994 (jch1003)
 ** Last Edited: Fri Oct  7 15:31:42 1994 By James Hall
 **
    $Log: atoi.c,v $
    Revision 1.3  1998/04/16 13:21:15  smh22
    fixed includes

    Revision 1.2  1995/02/28 10:10:40  dme
    reinstate copylefts

 * Revision 1.2  1994/10/31  15:18:08  jch1003
 * Moved from ../atoi.c, naming changes, self passed as arg., exceptions
 * introduced.
 *
 * Revision 1.1  1994/08/22  11:10:21  jch1003
 * Initial revision
 *
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */


#include <nemesis.h>
#include <exceptions.h>
#include <stdlib.h>
#include <limits.h>

extern int 
atoi(const char *s)

{
  long res = strtol(s, (char **) NULL, 10);

  if(res > INT_MAX)
    RAISE("stdlib_poflow", NULL);
  if(res < INT_MIN)
    RAISE("stdlib_noflow", NULL);
  return (int)res;

}


/*
 * end atoi.c
 */
