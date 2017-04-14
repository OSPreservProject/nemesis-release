/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
 *
 *	strdup.c
 *	--------
 *
 * $Id: strdup.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#include <nemesis.h>
#include <string.h>

char *strduph(const char *s, Heap_clp h)
{
    char *res;

    if (!s) return NULL;
    
    res = Heap$Malloc (h, strlen(s) + 1);
    if (!res) return NULL;
    
    strcpy(res, s);
    return res;
}

char *strdup(const char *s)
{
    return strduph(s,Pvs(heap));
}
