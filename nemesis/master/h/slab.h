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
**      slab.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Fast allocation and free of fized-size memory "slabs"
** 
** ENVIRONMENT: 
**
**      Anywhere you like, even in the most stripped down environments
** 
** ID : $Id: slab.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef slab_h
#define slab_h

/* a "slaballoc_t" is the type of a slab allocator.  It is opaque. */
typedef struct _slaballoc_t slaballoc_t;

/* reveal a what a _slaballoc_t is (you're not suppose to use this -
 * this is so the inline stuff works) */

struct _slaballoc_t {
    char	*base;		/* base of the data arena */
    char	**stack;	/* base of the stack */
    char	**sp;		/* current sp */
    char	*alloc;		/* arena allocation base for free */
    Heap_clp	heap;		/* heap used for freeing purposes */
};


/*------------------------------------------------------------*/


/* A slab is a (typically small) memory area.  A slab allocator
 * manages a collection of slabs of predefined fixed power-of-two
 * size.  Slabs are contiguous in memory, and naturally aligned (that
 * is, a 64 byte slab will start 64 byte aligned). */

/* A call to "slaballoc_new" creates a new slab allocator, which
 * manages a fresh slab arena.  Each slab is "slabsize" bytes long
 * (rounded up to the next power of two), and there are initially
 * "number" free slabs available.  Slabs are pre-allocated from
 * "heap". */
extern slaballoc_t *slaballoc_new(Heap_clp heap, int slabsize, int number);

/* Free all resources associated with "sa".  You probably don't want to
 * do this if there are any slabs still in use. */
extern void slaballoc_free(slaballoc_t *sa);


/*------------------------------------------------------------*/


/* "slab_alloc" returns the address of an unused slab (ie, "slabsize"
 * bytes of memory).  It may return NULL if there are no free slabs. */
static INLINE void *slab_alloc(slaballoc_t *sa)
{
    if (sa->sp == sa->stack)
	return NULL;	/* attempt to pop off the end of the stack */
    sa->sp--;
//    eprintf("slab_alloc -> %p\n", *sa->sp);
    return *sa->sp;
}


/* "slab_free" returns an allocated slab to the free stack.  The "base"
 * parameter should have been returned by "slab_alloc" on "sa" at an
 * earlier time.  It is an unchecked error to attempt to free anything
 * else.  It is an unchecked error to free something twice. */
/* XXX no checking of stack overflow! */
static INLINE void slab_free(slaballoc_t *sa, void *base)
{
    *sa->sp = base;
    sa->sp++;
}



/* It is expected that slab allocation and free is a fast process,
 * able to be performed on the datapath.  It can also be performed in
 * limited environments, eg with activations off (where a Heap$Malloc)
 * would not be suitable in case the heap needs to grow).  */


/*------------------------------------------------------------*/


/*
 * locked access
 */

/* You need to define SLAB_LOCK() and SLAB_UNLOCK() macros */

#define slab_allocL(sa) \
({						\
    void *a;					\
    SLAB_LOCK();				\
    a = slab_alloc(sa);				\
    SLAB_UNLOCK();				\
    a;						\
})

#define slab_freeL(sa, b) \
do {						\
    SLAB_LOCK();				\
    slab_free(sa, b);				\
    SLAB_UNLOCK();				\
} while(0)


#endif /* slab_h */
