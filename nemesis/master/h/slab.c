/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      slab.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of slab.h
** 
** ENVIRONMENT: 
**
**      Anywhere
** 
** ID : $Id: slab.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/


#include <slab.h>

//#define SLAB_TRACE

#ifdef SLAB_TRACE
#define SLAB_TRC(x) x
#else
#define SLAB_TRC(x)
#endif


slaballoc_t *slaballoc_new(Heap_clp heap, int slabsize, int number)
{
    slaballoc_t *sa;
    uint32_t ss = slabsize;
    int bits;
    int i;

    SLAB_TRC(printf("slaballoc_new(heap=%p, slabsize=%#x, number=%d)\n",
		    heap, slabsize, number));

    /* round slabsize to next nearest power of 2 */
    if (ss == 0)
    {
	printf("slaballoc_new: slabsize==0, don't be silly\n");
	return NULL;
    }
    bits = 0;
    while(ss != 1)
    {
	ss >>= 1;
	bits++;
    }
    if (slabsize & ((1U<<bits)-1))	/* need to round up */
	bits++;
    ss = 1U<<bits;	/* ss is now a power of 2 */
    SLAB_TRC(printf("slaballoc_new: rounded slabsize(%#x) to ss(%#x)\n",
		    slabsize, ss));


    /* meta data */
    sa = Heap$Malloc(heap, sizeof(slaballoc_t) + sizeof(char*) * number);
    if (!sa)
	return NULL;
    sa->stack = (addr_t)(sa + 1);


    /* arena (maybe this should come from a different heap?) */
    sa->alloc = Heap$Malloc(heap, ss * (number+1));
    /* align naturally */
    sa->base = (addr_t)(((word_t)sa->alloc + (ss-1)) & ~(ss-1));

    SLAB_TRC(printf("slaballoc_new: state=%p, stackbase=%p; "
		    "alloc=%p (base=%p)\n",
		    sa, sa->stack, sa->alloc, sa->base));

    /* push the slab addresses onto the stack now */
    for(i=0; i<number; i++)
    {
	sa->stack[i] = sa->base + (i * ss);
	SLAB_TRC(printf(" + stack pos %3d: %p\n", i, sa->stack[i]));
    }


    /* stack starts full.  SP always points to empty slot. */
    sa->sp = &sa->stack[number];

    return sa;
}


/* XXX should really check whether there are any slabs still not free'd */
void slaballoc_free(slaballoc_t *sa)
{
    Heap_clp heap = sa->heap;
    Heap$Free(heap, sa->alloc);
    Heap$Free(heap, sa);
}


/* End of slab.c */
