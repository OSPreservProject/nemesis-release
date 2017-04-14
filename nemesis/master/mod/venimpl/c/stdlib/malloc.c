/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
 *
 *	malloc.c
 *	--------
 *
 * $Id: malloc.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#include <nemesis.h>
#include <stdlib.h>

#include <Heap.ih>

void *calloc(size_t nmemb, size_t size)
{
    return Heap$Calloc(Pvs(heap), nmemb, size);
}
void free(void *ptr)
{
    Heap$Free(Pvs(heap), ptr);
}
void *malloc(size_t size)
{
    return Heap$Malloc(Pvs(heap), size);
}
void *realloc(void *ptr, size_t size)
{
    return Heap$Realloc(Pvs(heap), ptr, size);
}

/* End of $Id: malloc.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $ */
