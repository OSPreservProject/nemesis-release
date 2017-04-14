/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      mod/nemesis/heap/heapmod.c
**
** FUNCTIONAL DESCRIPTION:
**
**      Implements HeapMod.if
**
** ENVIRONMENT:
**
**      User space.
**
** ID : $Id: heapmod.c 1.2 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
**
**
*/
#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>
#include <kernel.h>
#include <string.h>
#include <salloc.h>
#include <stdio.h>
#include <contexts.h>
#include <HeapMod.ih>
#include <autoconf/heap_paranoia.h>
#include <autoconf/memsys.h>

#define ETRC(_x)

/*#define CTRACE*/

/* #define HEAP_TRACE */
#ifdef HEAP_TRACE
#define TRC(_x) _x
#define ETRCGROW(_x) _x
#define DEBUG
#else
#define TRC(_x)
#define ETRCGROW(_xx) 
#endif
#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(x)
#endif

#define QTRC(_x)

#ifdef CONFIG_HEAP_PARANOIA_FREE
#define CONFIG_HEAP_PARANOIA_HEADERS

#ifdef CONFIG_MEMSYS_EXPT
#define FREE_FILL_SIZE(_hrec)  (MIN((_hrec)->hr_size, PAGE_SIZE))
#else
#define FREE_FILL_SIZE(_hrec) ((_hrec)->hr_size)
#endif

#define FILL_FREE_BLOCK(_hrec) memset((_hrec) + 1, 0x42, FREE_FILL_SIZE(_hrec))
#else
#define FILL_FREE_BLOCK(_hrec)
#endif

#ifdef CONFIG_HEAP_PARANOIA_HEADERS
#define CONFIG_HEAP_PARANOIA_PC

#define HEAP_CHECK(_h) Heap$Check(_h, False)

#define HDR_CKSUM(_hrec) (((_hrec)->hr_prevbusy) ^ ((_hrec)->hr_size) ^ ((_hrec)->hr_index) ^ 0xf00baa)

#define MK_CKSUM(_hrec) (_hrec)->hr_check = HDR_CKSUM(_hrec)

#else

#define HEAP_CHECK(_h)
#define MK_CKSUM(_hrec)
#endif

#ifdef CONFIG_HEAP_PARANOIA_PC
#define SAVE_RA()               addr_t _svra = RA();
#define SET_BLOCK_RA(_node)     ((((addr_t *)(_node))[-2]) = _svra)
#else
#define SAVE_RA()               
#define SET_BLOCK_RA(_node)
#endif

/*
 * Misc. Macros
 */
#ifdef CTRACE
#define STATS(_x) _x
#define CVERB(_x) eprintf _x
#define CETRC(_x) eprintf _x
#else
#define STATS(_x)
#define CVERB(_x)
#define CETRC(_x)
#endif /* CTRACE */

#define MAXSIZE        sizeof(uint64_t)
#define BLOCK_ALIGN(_x) (((_x)+MAXSIZE-1) & -(MAXSIZE))

/*
 * Module stuff
 */
static HeapMod_New_fn		New_m, NewD_m;
static HeapMod_NewGrowable_fn	NewGrowable_m, NewGrowableD_m;
static HeapMod_NewRaw_fn	NewRaw_m, NewRawD_m;
static HeapMod_Where_fn         Where_m;
static HeapMod_Realize_fn       Realize_m;
static HeapMod_NewNested_fn	NewNested_m, NewNestedD_m;
static HeapMod_NewLocked_fn	NewLocked_m;

static HeapMod_op mod_op = {
    New_m,
    NewGrowable_m,
    NewRaw_m,
    Where_m,
    Realize_m,
    NewNested_m,
    NewLocked_m
};

static HeapMod_op dbgmod_op = {
    NewD_m,
    NewGrowableD_m,
    NewRawD_m,
    Where_m,
    Realize_m,
    NewNestedD_m,
    NewLocked_m
};

static const HeapMod_cl mod_cl = { &mod_op, NULL };
static const HeapMod_cl dbgmod_cl = { &dbgmod_op, NULL };

CL_EXPORT (HeapMod,HeapMod,mod_cl);
CL_EXPORT (HeapMod,DbgHeapMod,dbgmod_cl);
CL_EXPORT_TO_PRIMAL(HeapMod, HeapModCl, mod_cl);

static Heap_Query_fn    Query_m, LkQuery_m;
static Heap_Malloc_fn	Malloc_m,  LkMalloc_m, MallocD_m;
static Heap_Free_fn	Free_m,    LkFree_m,   FreeD_m;
static Heap_Calloc_fn	Calloc_m,  LkCalloc_m;
static Heap_Realloc_fn	Realloc_m, LkRealloc_m;
static Heap_Purge_fn	Purge_m,   LkPurge_m;
static Heap_Destroy_fn	Destroy_m, LkDestroy_m;
static Heap_Destroy_fn	DestroyNested_m;

static LockedHeap_Lock_fn     	 Lock_m;
static LockedHeap_Unlock_fn   	 Unlock_m;

static LockedHeap_LMalloc_fn  	 LMalloc_m;
static LockedHeap_LFree_fn   	 LFree_m;
static LockedHeap_LCalloc_fn  	 LCalloc_m;
static LockedHeap_LRealloc_fn 	 LRealloc_m;
static LockedHeap_LPurge_fn   	 LPurge_m;

static Heap_Check_fn Check_m, LkCheck_m;

static Heap_op h_op      = { Malloc_m, Free_m, Calloc_m, Realloc_m,
				 Purge_m, Destroy_m, Check_m, Query_m };
static Heap_op nested_op = { Malloc_m, Free_m, Calloc_m, Realloc_m,
				 Purge_m, DestroyNested_m, Check_m, Query_m };

static Heap_op hD_op      = { MallocD_m, FreeD_m, Calloc_m, Realloc_m,
				 Purge_m, Destroy_m, Check_m, Query_m };
static Heap_op nestedD_op = { MallocD_m, FreeD_m, Calloc_m, Realloc_m,
				 Purge_m, DestroyNested_m, Check_m, Query_m };

static LockedHeap_op locked_op = { 
    LkMalloc_m, LkFree_m, LkCalloc_m, 
    LkRealloc_m, LkPurge_m, LkDestroy_m, LkCheck_m, LkQuery_m,
    Lock_m, Unlock_m,
    LMalloc_m, LFree_m, LCalloc_m, LRealloc_m, LPurge_m
};


/*
 * Table of block sizes.
 *
 * The table is in three sections. The first section is for small
 * allocations and goes up in increments of the word size of the
 * machine. The SMALL_* macros give the size of this section, and the
 * largest block size it accomodates, along with a handy macro for
 * getting the correct index for a given block size.
 *
 * The second section of the table deals with larger blocks, up to
 * some maximum.
 *
 * The third section consists of just one large number and is used for
 * all blocks larger than any other table entries.
 *
 * From the previous comments to this code (via DME, RJB and Wanda):
 *
 * "The meaning of these numbers is:
 *
 * Consider the numbers 340, 408, 488:
 *
 * This means that the `408 list' holds free blocks 408 <= free-size < 488.
 * So the 408 list is used to satisfy requests 340 < req-size <= 408.
 *
 * The last list stores large blocks.
 * The penultimate list is also special, in that it stores blocks of exactly
 * 5184 bytes only, for satisfing requests 4320 < req-size <= 5184.
 *
 * The middle sequence of numbers appears to have been chosen each to be
 * about twenty per cent larger then its predecessor."
 *
 * Note that since those comments were written, the numbers in the
 * table have been modified to depend on the machine word size.
 */

#define _S(_x) (_x * WORD_SIZE)

#define SMALL_COUNT 16
#define SMALL_LIMIT _S(SMALL_COUNT)
#define SMALL_INDEX(x) ((x-1) / WORD_SIZE )

#define LARGE_COUNT 24
#define LARGE_LIMIT _S(1296)

#define COUNT (1 + LARGE_COUNT + SMALL_COUNT)
#define REST_INDEX (COUNT -1)
static const Heap_Size	all_sizes[COUNT] =
{
    _S(1),   _S(2),   _S(3),   _S(4),   _S(5),   _S(6),   _S(7),    _S(8),
    _S(9),   _S(10),  _S(11),  _S(12),  _S(13),  _S(14),  _S(15),   _S(16),
    
    _S(19),  _S(23),  _S(28),  _S(34),  _S(41),  _S(49),  _S(59),   _S(71),
    _S(85),  _S(102), _S(122), _S(146), _S(175), _S(210), _S(252),  _S(302),
    _S(362), _S(434), _S(521), _S(625), _S(750), _S(900), _S(1080), _S(1296),
    
    WORD_MAX
};


/*
 * Each piece of malloced store starts with a struct _heaprec,
 * and ends with a busy marker. The busy marker is in fact part of the
 * next heaprec structure.
 *
 * The check field is for a debugging checksum
 * The size field records the amount of user space in this piece.
 * The index field records which free list the piece should be returned
 * to when it is freed.
 *
 * The ra field contains the address at which this record was
 * allocated, if we're storing the RA. This is always just before the
 * HEAP_OF() pointer.
 *
 * If the piece is allocated (to the user), then the union points to
 * the heap structure (methods etc) for use by free and so on.
 *
 * If the piece is free, then the union field links the piece to the
 * next on its free list.
 *
 * After the end of the user space comes the busy field (a Heap_Size).
 * This is used in coalescing.
 *
 * If the piece is allocated, busy contains BUSYMAGIC.
 * If the piece is free, busy contains a copy of the size field
 * (in effect, a back pointer).  */

typedef struct _heaprec {
    Heap_Size		hr_prevbusy;
    Heap_Size		hr_size;
    word_t		hr_index;
#if CONFIG_HEAP_PARANOIA_VALUE > 0
    word_t		hr_check;
    addr_t              hr_ra; 
#endif
    union {
	Heap_cl			*hru_heap; /* when busy */
	struct _heaprec		*hru_next; /* when free */
    } h_un;
} heaprec;

#define hr_heap h_un.hru_heap
#define hr_next h_un.hru_next

/* NB. the heaprec structure must be aligned and padded such that
   the HEAP_OF macro works (see Heap.if) */

/*
 * Per-instance state
 */

#define MAXSEGMENTS 60

typedef enum {
    Heap_Type_Raw,
    Heap_Type_Stretch,
    Heap_Type_Nested 
} HeapType_t;


typedef struct _Heap_st Heap_st;
struct _Heap_st {
    struct _heaprec	*blocks[COUNT];
    addr_t		nullMalloc;
    HeapType_t		type;
    union {
	addr_t          where;
	Frames_clp	frames;
	Heap_clp	parent;
	Stretch_clp	stretch[MAXSEGMENTS];
    } u;
    Heap_Size		size;
    word_t		nSegments;    /* number of segments in the heap */
    word_t		maxSegments;  /* max number of segs in the heap */
    struct _Heap_cl	cl;
STATS(
      Heap_Size	bytesMalloc;
      Heap_Size	callsMalloc;
      Heap_Size	callsFree;
      Heap_Size	callsRealloc;
      Heap_Size	coalBytes;
      )
    VP_clp		vpp; /* XXX - DEBUG */
};

#define rest_blocks	blocks[REST_INDEX]

/* make BUSYMAGIC sign-extend on machines with
   sizeof(word_t) > sizeof(int32_t) */

#define BUSYMAGIC	((word_t) ((sword_t) ((int32_t) 0xff1a2c38L)))

#define NEXTBLOCK(X)	((heaprec *)(((char *)((X) + 1)) + (X)->hr_size))
#define PREVBLOCK(X)	((heaprec *)(((char *)((X) - 1)) - (X)->hr_prevbusy))

/* Size of minimum fragment: this should be heaprec + all_sizes[0] */
#define MINFRAG		(sizeof(heaprec) + _S(1))

/* ------------------------------------ */


#ifdef CTRACE
struct timeval {
    unsigned long tv_sec; /* seconds */
    long          tv_usec; /* and microseconds */
};
#endif


/*
 * Statics
 */
static Heap_clp CreateHeap(Heap_st 	*st,
			   Heap_Size    sizeBytes,
			   Heap_op	*ops );

static Stretch_clp	GetNewSegment (Heap_Size size, Stretch_clp tmpl);
static void		InitSegment   (Stretch_clp s, Heap_st *h);

typedef struct {
    addr_t ra;
    int count;
} usage_t;

#define NUM_COUNTS 100

#if CONFIG_HEAP_PARANOIA_VALUE > 0
static void DumpUsage(Heap_st *st) {

    usage_t u[NUM_COUNTS];
    int n, count = 0;

    int stretch;
    heaprec *rec, *nrec, *prec;

    if(st->type != Heap_Type_Stretch) {
	ntsc_dbgstop();
    }

    memset(u, 0, sizeof(u));
    eprintf("Dumping usage stats\n");

    for(stretch = 0; stretch < st->nSegments; stretch++) {
	
	Stretch_clp str = st->u.stretch[stretch];
	
	Stretch_Size size;
	
	addr_t addr;
	
	addr = Stretch$Range(str, &size);
	
	if(stretch != 0) {
	    
	    rec = addr;
	    
	} else {
	    
	    rec = addr + sizeof(Heap_st);
	    
	}
	
	prec = NULL;
	
	while(((addr_t)rec) < (addr+size - 2 * sizeof(heaprec))) {
	    nrec = NEXTBLOCK(rec);
	    
	    if(nrec->hr_prevbusy == BUSYMAGIC) { 
		
		TRC(eprintf("%u bytes at %p, pc %p\n", rec->hr_size,
			    rec+1, rec->hr_ra));
		
		for(n = 0; n < count; n++) {
		    if(u[n].ra == rec->hr_ra) {
			u[n].count++;
			
			break;
		    }
		}

		if(n == count) {
			
		    /* Didn't find this line */
		    
		    if(count < NUM_COUNTS) {
			/* If we've still space in the array, add it
			   to the end */
			
			count++;
			n = count;
		    } else {
			/* Otherwise add it at tenth place from
			   the end */
			memcpy(&u[NUM_COUNTS - 9], &u[NUM_COUNTS - 10],
			       sizeof(u[0]) * 9);
			n = NUM_COUNTS - 10;
		    }
		    u[n].ra = rec->hr_ra;
		    u[n].count = 1;
		    
		}
		/* Shuffle up to correct place */
		while((n >0) && (u[n].count > u[n-1].count)) {
		    
		    usage_t tmp = u[n-1];
		    u[n-1] = u[n];
		    u[n] = tmp;
		    n--;
		}
		    
	    }

	    prec = rec;
	    rec = nrec;
	    
	}
	
    }

    for(n = 0; n < count; n++) {
	eprintf("PC %p count %d\n", u[n].ra, u[n].count);
    }
}
#else
static void DumpUsage(Heap_st *st) {
    eprintf("No heap usage info available\n");
}
#endif

/* XXX see below ... */
bool_t alwaysTrue(Heap_clp self) {
    return True;
}

static void MemAbort(Heap_cl *self, string_t why, heaprec *rec, heaprec *prec)
{
    Heap_st *h;

    eprintf("MEMABT: as %s in domain %qx\n", why, VP$DomainID(Pvs(vp)));
    if (self)
    {
	h = self->st;
	eprintf("heap_cl:%p (st at %p)\n", self, h);
    }

#if CONFIG_HEAP_PARANOIA_VALUE > 0
#define DUMP(msg, _hr) \
do {						\
    eprintf(msg ": %p (PC %p), size %d\n", 	\
	    (_hr) + 1, (_hr)->hr_ra, (_hr)->hr_size);	\
} while(0)
#else
#define DUMP(msg, _hr) \
do {						\
    eprintf(msg ": %p, size %d\n",		\
	    (_hr) + 1, (_hr)->hr_size);		\
} while(0)
#endif

    if(rec)
	DUMP("Heap object", rec);
    
    if(prec)
	DUMP("Preceding heap object", prec);	

#undef DUMP
    
    /* TODO: abort this domain */
    
    /* XXX ntsc_dbgstop() is currently declared noreturn, which means
       GCC nicely optimises out the remainder of the function and
       confuses the back trace. So we have a dummy function that GCC
       can't optimise around ... */

    if(alwaysTrue(self)) {
	ntsc_dbgstop();
    }
    exit(42);
    
}

/* ------------------------------------ */
/*
 * Find list for REQUEST of given size.
 * NB: This is (usually) different from where a free block of the same
 *     size would be replaced.
 *
 * find_index(size) == i  <=>  all_sizes[i-1] < size <= all_sizes[i]
 */

static int find_index(Heap_Size size)
{
    int mi;

    ETRC(eprintf("Heap$Find_index: size= %x\n", size));

    if (size <= SMALL_LIMIT)
    {
        ETRC(eprintf("Heap$Find_index:smll blk %x\n", SMALL_INDEX(size)));
        return SMALL_INDEX(size);
    }
    else if (size <= LARGE_LIMIT)
    {
        int lo = SMALL_COUNT;
        int hi = REST_INDEX-1;

        for (; lo + 4 < hi;)
        {
            mi = (lo + hi) / 2;
            if (size == all_sizes[mi])
            {
		ETRC(eprintf("Heap$Find_index: large blk %x\n", mi));
                return mi;
            }
            else if (size < all_sizes[mi])
                hi = mi;
            else
                lo = mi;
        }
        for(mi = lo; mi <= hi; mi++)
        {
            if (size <= all_sizes[mi])
            {
                ETRC(eprintf("Heap$Find_index: large blk %x\n", mi));
                return mi;
            }
        }
    }
    ETRC(eprintf("Heap$Find_index: not found\n"));
    return REST_INDEX;
}

/*
 * Find list for FREE of given size.
 *
 * free_index(size) == i  <=>  all_sizes[i] <= size < all_sizes[i+1]
 */

static INLINE int
free_index(Heap_Size size)
{
    int index = find_index(size);
    if ((index != REST_INDEX) && (size != all_sizes[index]))
	--index;

    return index;
}


/************************************************************************\
 *									 *
 * Coalescing								 *
 *									 *
 \************************************************************************/

/*
 * See Coalesce() (below) for how these are used.
 */

/*
 * This coalesces adjacent free blocks, without moving them
 * (except, of course, that coalesced free blocks disappear).
 */

static void Coalesce_One(Heap_st *h, int index)
{
    heaprec	*free, **pfree, *nfree, *nextblock;

    CVERB(("%x", index % 10));
    for (pfree = &h->blocks[index]; (free = *pfree); )
    {
	if (free->hr_prevbusy != BUSYMAGIC)
        {
            nfree = PREVBLOCK(free);
	    if (NEXTBLOCK(nfree) != free)
		MemAbort(NULL, "Coalesce_One: sanity failed", free, nfree);

	    CVERB(("(C1 %x, %x)", nfree, free));

            nfree->hr_size += free->hr_size + sizeof(heaprec);
	    nextblock = NEXTBLOCK(free);
	    nextblock->hr_prevbusy = nfree->hr_size;

	    MK_CKSUM(nextblock);
	    MK_CKSUM(nfree);
	    
            *pfree = free->hr_next;

	    FILL_FREE_BLOCK(nfree);

        }
        else
        {
            pfree = &(free->hr_next);
        }
    }
}

/*
 * Coalesce_Two looks for blocks which could be moved into
 * a larger free list, and does so.
 *
 * (Note that a block can never be too small for the list it is in.
 * (Except blocks in the large block list which aren't large any more.))
 */

/* 0 <= index < REST_INDEX */

static void Coalesce_Two(Heap_st *h, int index)
{
    heaprec	*free, **pfree;
    Heap_Size	size;
    int		in;

    CVERB(("%x", index % 10));
    if (index == REST_INDEX-1) {
	size = all_sizes[index];
    } else {
	size = all_sizes[index+1];
    }
    for (pfree = &h->blocks[index]; (free = *pfree) ; )
    {
        if (free->hr_size >= size)
        {
	    CVERB(("(C2 %x)", free));
            in = free_index(free->hr_size);
            if (in != index)	/* Bites when index == REST_INDEX-1 */
            {
                *pfree = free->hr_next;
                free->hr_next = h->blocks[in];
                h->blocks[in] = free;
		free->hr_index = in;
		MK_CKSUM(free);

		continue;
            }
	}
	pfree = &(free->hr_next);

    }
}

/*
 * Move blocks from the large block list which aren't large any more
 * into the appropriately size list.
 *
 * index must be REST_INDEX  -- this is the only list which may
 * contain blocks smaller than all_sizes[index].
 */

static void Coalesce_Three(Heap_st *h, int index)
{
    heaprec	*free, **pfree;
    Heap_Size	size;
    int		in;

    /* assert(index == REST_INDEX); */
    CVERB(("%x", index % 10));
    size = all_sizes[REST_INDEX-1];
    for (pfree = &h->blocks[index]; (free = *pfree) ; )
    {
        if (free->hr_size <= size)
        {
	    CVERB(("(C3 %x)", free));
            in = free_index(free->hr_size);
	    *pfree = free->hr_next;
	    free->hr_next = h->blocks[in];
	    h->blocks[in] = free;
	    free->hr_index = in;
	    MK_CKSUM(free);
        }
        else
        {
            pfree = &(free->hr_next);
        }
    }
}

#ifdef CTRACE
static void print_lists(Heap_st *h)
{
    int i, j;
    heaprec *free;

    for (i = 0; i < REST_INDEX; i++)
    {
	j = 0;

        for (free = h->blocks[i]; free; free = free->hr_next)
	{
	    h->coalBytes += free->hr_size;

	    j++;
	    if (free->hr_index != free_index(free->hr_size))
		eprintf("Malloc: Something's wrong %x list: \
                      free: sz: %x index: %x find_index: %x\n",
			all_sizes[i], free->hr_size, free->hr_index,
			find_index(free->hr_size));
	}
	eprintf("%x=%x ", all_sizes[i], j);
	
    }
    eprintf("\n\n");
    
    for (free = h->rest_blocks; free; free = free->hr_next)
    {
	h->coalBytes += free->hr_size;
	eprintf("%x ", free->hr_size);
    }

    eprintf("coalBytes = %x, bytesMalloc = %x (sum = %x)\n",
	    h->coalBytes, h->bytesMalloc, h->coalBytes + h->bytesMalloc);
    eprintf("\n\n");
}
#endif


/*
 * Call the various coalescing routines in turn.
 *
 * Firstly, Coalesce_One combines adjacent free blocks.
 * This may create pieces which are oversize for their free list.
 * Secondly, Coalesce_Two moves oversize blocks to their proper free list.
 * Finally, Coalesce_Tree moves small blocks from the large block
 * list to their proper home.
 */

static void Coalesce(Heap_st *h)
{
    int i;

#ifdef CTRACE
    h->coalBytes = 0;
#if 0
    print_lists(h);
#endif /* 0 */
#endif /* CTRACE */

    CVERB(("COAL: phase 1..."));

    for (i = 0; i <= REST_INDEX; i++)
	Coalesce_One(h, i);

    CVERB(("\nCOAL: phase 2..."));

    for (i = 0; i < REST_INDEX; i++)
	Coalesce_Two(h, i);

    Coalesce_Three(h, REST_INDEX);

    CVERB(("\n"));

#ifdef CTRACE
    CETRC(("\n"));
    print_lists(h);
#endif

    CETRC(("COAL: free %x\n\n", h->coalBytes));
}


/* index < REST_INDEX  =>  size == all_sizes(index) */

static heaprec *get_new_int(Heap_st *h, Heap_Size size, int index)
{
    heaprec	**pfree, *free, *nfree;

    ETRC(eprintf("Heap$Get_new_int: called.\n"));
    for (pfree = &h->rest_blocks; (free = *pfree) ; pfree = &(free->hr_next))
    {
        if (free->hr_size >= size)
        {
            if (free->hr_size - size >= MINFRAG)
            {
		/* We allocate from the end of "free" */
                nfree = (heaprec *)((char *)free +free->hr_size -size);
                nfree->hr_size = size;
                nfree->hr_index = index;
		
                free->hr_size -= size + sizeof(heaprec);
		nfree->hr_prevbusy = free->hr_size;

		MK_CKSUM(nfree);
		MK_CKSUM(free);

		/* Dont need to set NEXTBLOCK(nfree)->hr_prevbusy
		 * because we're about to stamp BUSYMAGIC in it
		 */
		ETRC(eprintf("Heap$Get_new_int: found large block.\n"));
                return nfree;
            }
            free->hr_index = index;
	    MK_CKSUM(free);
            *pfree = free->hr_next;
            return free;
        }
    }
    ETRC(eprintf("Heap$Get_new_int: failed. returning null.\n"));

    return NULL;
}


/*
 *
 *  Algorithm: try to allocate, Coalesce, try again
 *             get a new block from frames, try one more
 *             time.
 */

static heaprec *get_new(Heap_st *h, Heap_Size size, int index)
{
    heaprec	*free;

    ETRC(eprintf("Heap$Get_new: called.\n"));
    if (index != REST_INDEX) size = all_sizes[index];

    if ((free = get_new_int(h, size, index)))
	return free;

    ETRC(eprintf("Heap$Get_new: get_new_int didn't work; coalescing.\n"));
    Coalesce(h);

    if ((free = get_new_int(h, size, index)))
	return free;

    if (h->type == Heap_Type_Stretch) {
	if(h->nSegments < h->maxSegments) {
	    
	    ETRCGROW (eprintf("HeapMod: grow heap %x (size %x, nsegs %x)\n",
			      h, h->size, h->nSegments));
	    
	    /* We might already have this segment if the heap grew and was
	     * subsequently Purged(). Do we have a segment? */
	    if (! h->u.stretch[h->nSegments])
		h->u.stretch[h->nSegments] = 
		    GetNewSegment(MAX(h->size, size + 2*sizeof(heaprec)), 
				  h->u.stretch[0]);
	    
	    InitSegment(h->u.stretch[h->nSegments], h);
	    
	    h->nSegments++;
	    
	    /* ok, got another stretch, now try again */
	    if ((free = get_new_int(h, size, index)))
		return free;
	} else {
	    eprintf("HeapMod [%qx]: cannot grow heap %p: size %x.\n", 
		    VP$DomainID(Pvs(vp)), h, h->size);
	    eprintf("No more segments: nsegs %x maxsegs %x\n", 
		    h->nSegments, h->maxSegments);
	    DumpUsage(h);
	    ntsc_dbgstop();
	}
    }

    return NULL;
}

static Stretch_clp
GetNewSegment (Heap_Size size, Stretch_clp tmpl)
{
    Stretch_clp res;

    if(Pvs(vp) && !VP$ActivationMode(Pvs(vp))) {
	eprintf("GetNewSement: Called with activations off.\n");
	ntsc_dbgstop();
    }

    ETRCGROW (eprintf ("HeapMod: get new stretch (size %x)\n", size));
    res = StretchAllocator$Clone(Pvs(salloc), tmpl, size); 
#ifdef CONFIG_MEMSYS_EXPT
    StretchDriver$Bind(Pvs(sdriver), res, PAGE_WIDTH);
#endif
    ETRCGROW (eprintf ("HeapMod: got new stretch at %p\n", res));

    return res;
}

static void
InitSegment (Stretch_clp s, Heap_st *h)
{
    Heap_Size	sizeBytes;
    heaprec      *rec = STR_RANGE(s, &sizeBytes);
    heaprec      *nextblock;

    rec->hr_prevbusy = BUSYMAGIC;
    rec->hr_size     = sizeBytes - 2 * sizeof (heaprec);
    rec->hr_index    = REST_INDEX;
    
    FILL_FREE_BLOCK(rec);
    nextblock = NEXTBLOCK(rec);
    nextblock->hr_prevbusy = rec->hr_size;
    nextblock->hr_size = 0;
    nextblock->hr_index = 0;
    rec->hr_next     = h->rest_blocks;

    MK_CKSUM(nextblock);
    MK_CKSUM(rec);
    h->rest_blocks   = rec;
}


/****************************************************************/
/*								*/
/*	Heap methods						*/
/*								*/
/****************************************************************/



/*
 * Implementation of Heap.if
 */

static addr_t Malloc_m (Heap_cl *self, Heap_Size size)
{
    SAVE_RA()
    Heap_st     *h = self->st;
    heaprec     *free;
    heaprec     *nextblock;
    int          index;
    addr_t       p;

    ETRC(eprintf("Heap$Malloc: size %x\n", size));
    if (size == 0)
	return h->nullMalloc;

    size = BLOCK_ALIGN(size);
    index = find_index(size);
    free = h->blocks[index];
    ETRC(eprintf("Heap$Malloc:size= %x, index= %x, free= %x\n", size, index, free));

    if ( (free == NULL) || (index == REST_INDEX) )
    {
	free = get_new(h, size, index);
	if (! free)
	{
	    return NULL;
	}
    }
    else
    {
	h->blocks[index] = free->hr_next;
    }
    ETRC(eprintf("Heap$Malloc: have heaprec at %p\n", free));
    free->hr_heap = self;

    nextblock = NEXTBLOCK(free);
    nextblock->hr_prevbusy = BUSYMAGIC;

    MK_CKSUM(nextblock);
    MK_CKSUM(free);

    STATS(h->bytesMalloc += free->hr_size + MINFRAG);
    STATS(h->callsMalloc++);

    ETRC(eprintf("Heap$Malloc: returning %x\n", free+1));

    p = free + 1;
    SET_BLOCK_RA(p);
    return p;
}

static addr_t MallocD_m (Heap_cl *self, Heap_Size size)
{
    SAVE_RA()
    char *res, *ptr;
    heaprec *rec;
    int i;

    res = Malloc_m(self, size);
    if (res)
    {
	rec = ((heaprec *)res) - 1;
	ptr = res;
	for(i=0; i < rec->hr_size; i++)
	    ptr[i] = 0xd0;
	SET_BLOCK_RA(res);
    }

    return res;
}
    
    

static void
Free_m(Heap_cl *self, addr_t p)
{
    Heap_st *h = self->st;
    heaprec *w_free, *nextblock;


    if ((p == NULL) || (p == h->nullMalloc))
	return;

    w_free = ((heaprec *)p) - 1;

    nextblock = NEXTBLOCK(w_free);

#ifdef CONFIG_HEAP_PARANOIA_HEADERS
    if (HDR_CKSUM(w_free) != w_free->hr_check) {
	MemAbort(self, "free: bad checksum on block", w_free, nextblock);
    }
    if (HDR_CKSUM(nextblock) != nextblock->hr_check) {
	MemAbort(self, "free: bad checksum on next block", w_free, nextblock);
    }
    if (nextblock->hr_prevbusy != BUSYMAGIC) {
	MemAbort(self, "free: freeing freed block", w_free, nextblock);
    }
    if (w_free->hr_heap != self)
	MemAbort(self, "free: block has wrong closure", w_free, NULL);
    if (w_free->hr_size & WORD_MASK)
        MemAbort(self, "free: block has bad size", w_free, NULL);
    if (w_free->hr_index > REST_INDEX)
        MemAbort(self, "free: block has invalid index", w_free, NULL);


#endif
    w_free->hr_next = h->blocks[w_free->hr_index];
    MK_CKSUM(w_free);
    FILL_FREE_BLOCK(w_free);
    h->blocks[w_free->hr_index] = w_free;

    nextblock->hr_prevbusy = w_free->hr_size;
    MK_CKSUM(nextblock);

    STATS(h->bytesMalloc -= w_free->hr_size + MINFRAG);
    STATS(h->callsFree++);

    return;
}

static void
FreeD_m(Heap_cl *self, addr_t p)
{
    Heap_st *h = self->st;
    heaprec *rec;

    if (p && p != h->nullMalloc)
    {
	rec = ((heaprec *)p) - 1;
	if(rec->hr_size == 0x42424242) ntsc_dbgstop();
	FILL_FREE_BLOCK(rec);
    }

    Free_m(self, p);

}


static addr_t
Calloc_m (Heap_cl *self, Heap_Size nelem, Heap_Size elsize)
{
    SAVE_RA()
    Heap_Size		size;
    addr_t		p;

    size = elsize * nelem;
    if ((p = Heap$Malloc(self, size))) {
        memset (p, 0, size);
	SET_BLOCK_RA(p);
    }

    return p;
}

static addr_t
Realloc_m (Heap_cl *self, addr_t p, Heap_Size size)
{
    SAVE_RA()
    Heap_st *h = self->st;
    addr_t	np;
    heaprec	*w_free;

    if ((p == NULL) || (p == h->nullMalloc))
	return Heap$Malloc(self, size);

    w_free = ((heaprec *)p) - 1;

    if (w_free->hr_heap != self)
	MemAbort(self, "realloc (closure)", w_free, NULL);

    if (w_free->hr_size >= size) {
	STATS(h->callsRealloc++);
	return p;
    }
    
    if ((np = Heap$Malloc(self, size))) {
	memcpy (np, p, w_free->hr_size);
	Heap$Free(self, p);
	SET_BLOCK_RA(np);
	return np;
    }
    return NULL;
}

/*
 * Empty a heap and start again
 */
static Heap_clp Purge_m(Heap_cl *self)
{
    Heap_st *st = (Heap_st *)(self->st);

    st->nSegments = 1;
    return CreateHeap(st, st->size, self->op );
}

/*
 * Destroy a heap: there are three varieties
 */

static void Destroy_m(Heap_cl *self)
{
    Heap_st *h = self->st;
    int i = MAXSEGMENTS;

    TRC(eprintf("Heap %p destroyed\n", self));
    if(h->type == Heap_Type_Stretch) {
	/* Free stretches in reverse order, to avoid freeing our state
	   until the end */

	while(i--) {
	    
	    if(h->u.stretch[i]) {
#if 0
		StretchAllocator$DestroyStretch(Pvs(salloc), h->u.stretch[i]);
#endif /* 0 */
		STR_DESTROY(h->u.stretch[i]);
	    }
	}
    } else if(h->type == Heap_Type_Raw) {
	/* XXX Do nothing */
    }
#if 0
    if (h->type == Heap_Type_Stretch && VP$ActivationMode(Pvs(vp))) {
	if (h->heapname[0]) {
	    eprintf("Destory heap %s of domain %qx\n", h->heapname, VP$DomainID(Pvs(vp)));
	    if (Pvs(root)) {
		Context$Remove(Pvs(root), h->heapname);
	    } else {
		eprintf("HeapMod: Unable to remove heap name %s because Pvs(root) is NULL\n", h->heapname);
	    }
	}
    }
#endif
    return;		    
}

static void DestroyNested_m(Heap_cl *self)
{
    Heap_st *st = (Heap_st *)(self->st);

    Heap$Free(st->u.parent, st);
}

static void Check_m(Heap_clp self, bool_t checkFreeBlocks) {

    Heap_st *st = (Heap_st *)(self->st);
    
    int stretch;
    /* Keep these volatile so they are available in a post-mortem
       debugging session */
    heaprec * volatile rec, *volatile nrec, *volatile prec;

    if(st->type != Heap_Type_Stretch) {
	return;
    }
    
    /* Check the integrity of all heap blocks */

    for(stretch = 0; stretch < st->nSegments; stretch++) {
	
	Stretch_clp str = st->u.stretch[stretch];
	
	Stretch_Size size;
	
	addr_t addr;
	
	addr = Stretch$Range(str, &size);
	
	if(stretch != 0) {
	    
	    rec = addr;
	    
	} else {
	    
	    rec = addr + sizeof(Heap_st);
	    
	}
	
	prec = NULL;
	
	while(((addr_t)rec) < (addr+size - 2 * sizeof(heaprec))) {

	    nrec = NEXTBLOCK(rec);
	    
#ifdef CONFIG_HEAP_PARANOIA_HEADERS
	    if(HDR_CKSUM(rec) != rec->hr_check) {
		MemAbort(self, "Header corrupted: bad checksum", rec, prec);
	    }
#endif

	    if(nrec->hr_prevbusy != BUSYMAGIC) { 
		
		/* This is a free block */
		if(PREVBLOCK(nrec) != rec) {
		    
		    MemAbort(self, "Header corrupted: backlink", nrec, rec);
		}
	    
		/* If this is a free block, hr_heap shouldn't be
                   pointing to us ... */
		if(rec->hr_heap == self) {
		    MemAbort(self, "Header corrupted: busy/heap", rec, prec);
		}

#ifdef CONFIG_HEAP_PARANOIA_FREE	

		/* Check that this free block has not been trampled on */
		if(checkFreeBlocks) {
		    uint8_t  * volatile p;
		    uint8_t  * volatile endp = 
			((uint8_t *) (rec+1)) + FREE_FILL_SIZE(rec);
		    
		    for(p = (uint8_t *) (rec+1); p < endp; p++) {
			if(*p != 0x42) {
			    MemAbort(self, "Free block corrupted", rec, prec);
			}
		    }
		}
#endif
	    }

	    if(rec->hr_size && (rec->hr_index > COUNT)) {
		MemAbort(self, "Header corrupted: index", rec, prec);
	    }
	    
	    if(rec->hr_size > size) {
		MemAbort(self, "Header corrupted: size", rec, prec);
	    }
    
	    prec = rec;
	    rec = nrec;
	    
	}
	
    }
    
}    

/****************************************************************/
/*								*/
/*	Creation functions for the unlocked classes of heap	*/
/*								*/
/****************************************************************/

/*
 * MINHEAP is the amount of space that we use in structures just to
 * initialise the heap. A Heap_cl, Heap_st and two heaprecs at
 * the start plus the heaprec at the very end.
 */

#define MINHEAP	(sizeof(Heap_st) \
		 + 3 * sizeof(heaprec))

/*
 * Creation function for Normal heaps
 */
static Heap_clp realNew (HeapMod_cl *self, Stretch_clp s, Heap_op *ops);

static Heap_clp New_m (HeapMod_cl *self,
                       Stretch_clp s)
{
    return realNew(self, s, &h_op);
}

static Heap_clp NewD_m (HeapMod_cl *self,
                       Stretch_clp s)
{
    return realNew(self, s, &hD_op);
}

static Heap_clp realNew (HeapMod_cl *self,
			 Stretch_clp s,
			 Heap_op *ops)
{
    Heap_Size	sizeBytes;
    Heap_st      *st;
    int i; 

    /* find out where the heap is to be */
    st = STR_RANGE(s, &sizeBytes );
    ETRC(eprintf("HeapMod$New: heap size %x at addr %p\n", sizeBytes, st));
    sizeBytes = BLOCK_ALIGN(sizeBytes);
    if (sizeBytes < MINHEAP) {
	DB(eprintf("HeapMod$New: Bad heap size. No heap.\n"));
	RAISE_Heap$NoMemory();
	return NULL;
    }

    /* Create it */
    memset (st, 0, sizeof (*st));
    st->type = Heap_Type_Stretch;

    for(i=0; i < MAXSEGMENTS; i++) 
	st->u.stretch[i] = (Stretch_cl *)NULL;

    st->u.stretch[0] = s;
    st->nSegments    = 1;
    st->maxSegments  = MAXSEGMENTS;
    return CreateHeap(st, sizeBytes, ops);
}


/*
 * Creation function for Growable heaps
 */
static Heap_clp realNewGrowable(HeapMod_cl *self, Stretch_clp s, 
				uint32_t nsegs, Heap_op *ops);

static Heap_clp NewGrowable_m(HeapMod_cl *self, Stretch_clp s, uint32_t nsegs)
{
    return realNewGrowable(self, s, nsegs, &h_op);
}

static Heap_clp NewGrowableD_m(HeapMod_cl *self, Stretch_clp s, uint32_t nsegs)
{
    return realNewGrowable(self, s, nsegs, &hD_op);
}

static Heap_clp realNewGrowable(HeapMod_cl *self, Stretch_clp s, 
				uint32_t nsegs, Heap_op *ops)
{
    Heap_Size	sizeBytes;
    Heap_st    *st;
    int i; 

    /* find out where the heap is to be */
    st = STR_RANGE(s, &sizeBytes );
    ETRC(eprintf("HeapMod$NewGrowable: heap size %x at addr %p\n", 
		 sizeBytes, st));
    sizeBytes = BLOCK_ALIGN(sizeBytes);
    if (sizeBytes < MINHEAP) {
	DB(eprintf("HeapMod$New: Bad heap size. No heap.\n"));
	RAISE_Heap$NoMemory();
	return NULL;
    }

    /* Create it */
    memset (st, 0, sizeof (*st));
    st->type = Heap_Type_Stretch;

    for(i=0; i < MAXSEGMENTS; i++) 
	st->u.stretch[i] = (Stretch_cl *)NULL;

    st->u.stretch[0] = s;
    st->nSegments    = 1;
    if(nsegs > MAXSEGMENTS) {
	eprintf("HeapMod$NewGrowable warning: nsegs=%x too big (max is %d)\n", 
		nsegs, MAXSEGMENTS); 
	nsegs = MAXSEGMENTS;
    }
    st->maxSegments  = nsegs; 
    return CreateHeap(st, sizeBytes, ops);
}

/*
 * Creation functions for Raw Heaps
 */
static Heap_clp realNewRaw(HeapMod_cl *self, addr_t start,
			    Heap_Size sizeBytes, Heap_op *ops);

static Heap_clp NewRaw_m(HeapMod_cl *self, addr_t start,
			 Heap_Size sizeBytes)
{
    return realNewRaw(self, start, sizeBytes, &h_op);
}

static Heap_clp NewRawD_m(HeapMod_cl *self, addr_t start,
			  Heap_Size sizeBytes)
{
    return realNewRaw(self, start, sizeBytes, &hD_op);
}

static Heap_clp realNewRaw(HeapMod_cl *self, addr_t start, 
			   Heap_Size sizeBytes, Heap_op *ops)
{
    Heap_st *st;

    ETRC(eprintf("HeapMod$NewRaw: size %x\n", sizeBytes));
    sizeBytes = BLOCK_ALIGN(sizeBytes);
    if (sizeBytes < MINHEAP) {
	DB(eprintf("HeapMod$New: Bad heap size. No heap.\n"));
	return NULL;
    }
    
    st = (Heap_st *)start;

    ETRC(eprintf("HeapMod$NewRaw: heap at %x\n", st));
    st->type    = Heap_Type_Raw;
    st->u.where = start;
    return CreateHeap(st, sizeBytes, ops);
}

static addr_t Where_m(HeapMod_cl *self, Heap_clp heap, word_t *size) 
{
    Heap_st *st = (Heap_st *)heap->st;

    *size = st->size;
    return (addr_t)st;
}

/*
** Realize is used to turn a 'raw' heap into a stretch-based one, 
** and requires that the given stretch maps exactly over the frames of 
** the original heap.
*/
static Heap_clp Realize_m(HeapMod_cl *self, Heap_clp rawheap, Stretch_clp s)
{
    Heap_st *st = (Heap_st *)rawheap->st;
    int i; 

    /* XXX SMH: sanity checking. Perhaps should RAISE an exn here? */
    if(st->type != Heap_Type_Raw)
	ntsc_dbgstop();

    ETRC(eprintf("Entered Realize: rawheap=%p, str=%p\n", rawheap, s));
    st->type = Heap_Type_Stretch;

    for(i=0; i < MAXSEGMENTS; i++) 
	st->u.stretch[i] = (Stretch_cl *)NULL;

    st->u.stretch[0] = s;
    st->nSegments    = 1;
    st->maxSegments  = MAXSEGMENTS;
    st->cl.op = &h_op;
    return &st->cl;
}

/*
 * Creation function for Nested Heaps
 */
static Heap_clp realNewNested(HeapMod_cl *self, Heap_clp h,
			      Heap_Size sizeBytes, Heap_op *ops);

static Heap_clp NewNested_m (HeapMod_cl *self,
			      Heap_clp h,
			      Heap_Size sizeBytes)
{
    return realNewNested(self, h, sizeBytes, &nested_op);
}

static Heap_clp NewNestedD_m (HeapMod_cl *self,
			      Heap_clp h,
			      Heap_Size sizeBytes)
{
    return realNewNested(self, h, sizeBytes, &nestedD_op);
}

static Heap_clp realNewNested(HeapMod_cl *self,
			      Heap_clp h,
			      Heap_Size sizeBytes,
			      Heap_op *ops)
{
    Heap_st       *st;

    ETRC(eprintf("HeapMod$NewNested: size %x\n", sizeBytes));
    sizeBytes = BLOCK_ALIGN(sizeBytes);
    if (sizeBytes < MINHEAP) {
	DB(eprintf("HeapMod$NewNested: Bad heap size. No heap.\n"));
	return NULL;
    }

    st = Heap$Malloc(h, (sizeBytes + FRAME_SIZE) >> FRAME_WIDTH);
    if (st == NULL) {
	DB(eprintf("HeapMod$NewNested: Failed to get memory. No heap.\n"));
	RAISE_Heap$NoMemory();
	return NULL;
    }

    ETRC(eprintf("HeapMod$NewNested: heap at %x\n", st));
    st->type = Heap_Type_Nested;
    st->u.parent = h;
    return CreateHeap(st, sizeBytes, ops);
}


/****************************************************************/
/*								*/
/*	Basic function for building any heap			*/
/*								*/
/****************************************************************/

/*
 * Creation function
 */
static Heap_clp CreateHeap(Heap_st 	*st,
			   Heap_Size    sizeBytes,
			   Heap_op	*ops )
{
    heaprec  *rec, *nextblock;
    int       i;
    

    DB (st->vpp = Pvs(vp));	/* XXX - Pvs must be valid */

    for (i = 0; i < COUNT; i++)
	st->blocks[i] = NULL;

    ETRC(eprintf("HeapMod$CreateHeap: st = %x\n", st));
    STATS( st->bytesMalloc = 0; st->callsMalloc  = 0;
	  st->callsFree   = 0; st->callsRealloc = 0; );

    /* Now put the nullMalloc entry in */
    rec = (heaprec *)(st + 1);
    rec->hr_prevbusy = BUSYMAGIC;
    rec->hr_size    = 0;
    rec->hr_index   = -1;
    rec->hr_heap = &st->cl;

    MK_CKSUM(rec);
    st->nullMalloc  = rec + 1;
    st->size	  = sizeBytes;
    ETRC(eprintf("HeapMod$CreateHeap: rec = %x\n", rec));

    /* Now put the free block in */
    rec++;
    rec->hr_prevbusy = BUSYMAGIC;
    rec->hr_size = sizeBytes - MINHEAP;

    rec->hr_index = REST_INDEX;
    rec->hr_next = NULL;
    
    MK_CKSUM(rec);

    if(st->type == Heap_Type_Stretch) {
	FILL_FREE_BLOCK(rec);
    }

    st->rest_blocks = rec;

    nextblock = NEXTBLOCK(rec);
    ETRC(eprintf("HeapMod$CreateHeap: recsize = %x\n", sizeof(*rec)));
    ETRC(eprintf("HeapMod$CreateHeap: rec = %x\n", rec));
    ETRC(eprintf("HeapMod$CreateHeap: NEXTBLOCK = %x\n", nextblock));
    nextblock->hr_prevbusy = rec->hr_size;
    nextblock->hr_size = 0;
    nextblock->hr_index = 0;
#if CONFIG_HEAP_PARANOIA_VALUE > 0
    nextblock->hr_ra = 0;
#endif
    MK_CKSUM(nextblock);

    ETRC(eprintf("HeapMod$CreateHeap: rec = %x\n", rec));
    CL_INIT(st->cl, ops, st );

    ETRC(eprintf("HeapMod$CreateHeap: done.\n"));
    return &(st->cl);
}


/****************************************************************/
/*								*/
/*	Locked Heaps						*/
/*								*/
/****************************************************************/



static LockedHeap_clp
InitLocked_m (Heap_clp parent, SRCThread_clp srcthds, SRCThread_Mutex *mu);

/*
 * Per-instance state
 */

typedef struct _LockedHeapMod_st LockedHeapMod_st;
struct _LockedHeapMod_st {
    struct _LockedHeap_cl	cl; /* This heap closure 	*/
    SRCThread_clp		threads; /* Thread sync package  */
    Heap_clp		heap; /* Heap we wrap up 	*/
    SRCThread_Mutex	mu; /* Mutex for heap	*/
    VP_clp		vpp; /* XXX - DEBUG */
};

/*
 * Implementation of Heap.if
 */

static Heap_Ptr
LkMalloc_m (Heap_cl *self, Heap_Size size)
{
    SAVE_RA()
    addr_t		res;
    LockedHeapMod_st     *st = self->st;

    ETRC(eprintf("Locked heap: MALLOC (state at : %x)\n", st));

    MU_LOCK(&st->mu);
    ETRC(eprintf("Locked heap: got lock\n"));

    HEAP_CHECK(st->heap);
    res = Heap$Malloc (st->heap, size);
    if (res) {
	HEAP_OF (res) = self;
	SET_BLOCK_RA(res);
    }

    MU_RELEASE(&st->mu);

    ETRC(eprintf("Locked heap: done\n"));

    return res;
}

static void
LkFree_m (Heap_cl *self, Heap_Ptr p)
{
    LockedHeapMod_st     *st = self->st;

    MU_LOCK(&st->mu);
    if (p) HEAP_OF (p) = st->heap;

    HEAP_CHECK(st->heap);
    Heap$Free (st->heap, p);
    MU_RELEASE( &st->mu);

    return;
}

static Heap_Ptr
LkCalloc_m (Heap_cl *self, Heap_Size nelem, Heap_Size elsize)
{
    SAVE_RA()
    LockedHeapMod_st     *st = self->st;
    void 		       *res;

    MU_LOCK( &st->mu);
    
    HEAP_CHECK(st->heap);
    res = Heap$Calloc (st->heap, nelem, elsize);
    if (res) {
	HEAP_OF (res) = self;
	SET_BLOCK_RA(res);
    }

    MU_RELEASE (&st->mu);

    return res;
}

static Heap_Ptr
LkRealloc_m (Heap_cl *self, Heap_Ptr p, Heap_Size size)
{
    SAVE_RA()
    LockedHeapMod_st     *st = self->st;
    Heap_Ptr 	        res;

    MU_LOCK( &st->mu);
    if (p) HEAP_OF (p) = st->heap;
    HEAP_CHECK(st->heap);
    res = Heap$Realloc (st->heap, p, size);
    if (res) { 
	HEAP_OF (res) = self;
	SET_BLOCK_RA(res);
    }
    MU_RELEASE(&st->mu);

    return res;
}

static Heap_clp
LkPurge_m (Heap_cl *self )
{
    LockedHeapMod_st     *st      = self->st;
    Heap_clp		parent  = st->heap;
    SRCThread_clp		srcthds = st->threads;
    SRCThread_Mutex	mu      = st->mu;
    LockedHeap_clp	res;

    MU_LOCK(&mu);
    parent = Heap$Purge (parent);
    res = InitLocked_m (parent, srcthds, &mu);
    MU_RELEASE(&mu);
    return (Heap_clp) res;
}

static bool_t
LkQuery_m (Heap_cl *self, Heap_Statistic kind, /* RETURNS */ uint64_t *value )
{
    LockedHeapMod_st     *st      = self->st;
    return Heap$Query(st->heap, kind, value);
}

static void
LkDestroy_m (Heap_cl *self )
{
    LockedHeapMod_st     *st = self->st;

    MU_FREE(&st->mu);
    Heap$Destroy(st->heap);
}

static void LkCheck_m (Heap_clp self, bool_t checkFreeBlocks) {

    LockedHeapMod_st     *st = self->st;
    
    MU_LOCK(&st->mu);
    Heap$Check(st->heap, checkFreeBlocks);
    MU_RELEASE(&st->mu);

}
    

/*
 * Locked Methods
 */

static void
Lock_m (LockedHeap_cl *self )
{
    LockedHeapMod_st     *st = self->st;
    MU_LOCK(&st->mu);

    HEAP_CHECK(st->heap);
}

static void
Unlock_m (LockedHeap_cl *self )
{
    LockedHeapMod_st     *st = self->st;

    HEAP_CHECK(st->heap);
    MU_RELEASE(&st->mu);
}

static Heap_Ptr
LMalloc_m (LockedHeap_cl *self, Heap_Size size)
{
    SAVE_RA()
    addr_t		res;
    LockedHeapMod_st     *st = self->st;

    res = Heap$Malloc (st->heap, size);
    if (res) {
	HEAP_OF (res) = self;
	SET_BLOCK_RA(res);
    }

    return res;
}

static void
LFree_m (LockedHeap_cl *self, Heap_Ptr p)
{
    LockedHeapMod_st     *st = self->st;

    if (p) HEAP_OF (p) = st->heap;
    Heap$Free (st->heap, p);

    return;
}

static Heap_Ptr
LCalloc_m (LockedHeap_cl *self, Heap_Size nelem, Heap_Size elsize)
{
    SAVE_RA()
    LockedHeapMod_st     *st = self->st;
    void 		       *res;

    res = Heap$Calloc (st->heap, nelem, elsize);
    if (res) {
	HEAP_OF (res) = self;
	SET_BLOCK_RA(res);
    }
    return res;
}

static Heap_Ptr
LRealloc_m (LockedHeap_cl *self, Heap_Ptr p, Heap_Size size)
{
    SAVE_RA()
    LockedHeapMod_st     *st = self->st;
    Heap_Ptr 	        res;

    if (p) HEAP_OF (p) = st->heap;
    res = Heap$Realloc (st->heap, p, size);
    if (res) {
	HEAP_OF (res) = self;
	SET_BLOCK_RA(res);
    }

    return res;
}

static LockedHeap_clp
LPurge_m (LockedHeap_cl *self )
{
    LockedHeapMod_st     *st      = self->st;
    Heap_clp		parent  = st->heap;
    SRCThread_clp		srcthds = st->threads;
    SRCThread_Mutex	mu      = st->mu;
    LockedHeap_clp	res;

    parent = Heap$Purge (parent);
    res = InitLocked_m (parent, srcthds, &mu);

    return res;
}


/*
 * The creation function
 */

static LockedHeap_clp
NewLocked_m (HeapMod_cl *self, Heap_clp parent, SRCThread_clp srcthds)
{
    return InitLocked_m (parent, srcthds, (SRCThread_Mutex *)NULL);
}

static LockedHeap_clp
InitLocked_m (Heap_clp parent, SRCThread_clp srcthds, SRCThread_Mutex *mu)
{
    LockedHeapMod_st      *st;

    ETRC(eprintf("HeapMod$InitLocked: srcthds= %x, parent= %x\n", srcthds, parent));
    st = Heap$Malloc (parent, sizeof (*st));
    if (!st) {
	DB(eprintf("LockedHeapMod_New: failed to get state.\n"));
	RAISE_Heap$NoMemory();
    }

    DB (st->vpp = Pvs(vp));	/* XXX - Pvs must be valid */

    st->cl.st = st;
    st->cl.op = &locked_op;

    st->threads = srcthds;
    st->heap    = parent;
    if(!mu) SRCThread$InitMutex(srcthds, &st->mu);
    /* else: we're just doing a purge, so mutex is ok */

    return &st->cl;
}

uint64_t calculate_current_size(Heap_st *st) {
    uint64_t size;
    addr_t base;
    word_t len;
    int i;

    size = 0;

    for (i=0; i<MAXSEGMENTS; i++) {
	if (st->u.stretch[i]) {
	    base = STR_RANGE(st->u.stretch[i], &len);
	    size += len;
	}
    }
    return size;
}

static bool_t Query_m (Heap_clp self, Heap_Statistic kind, /* RETURNS*/ uint64_t *value) {
    Heap_st     *st = (Heap_st *) self->st;
    int i;
    uint64_t totalfree = 0;

    QTRC(eprintf("Query on heap %p kind %d\n", self, (int) kind));
    if (kind == Heap_Statistic_CurrentSize) {
	*value = calculate_current_size(st);
	return True;
    }
    if (kind == Heap_Statistic_MaximumSize) {
	*value = st->size;
	if (st->type == Heap_Type_Stretch) {
	    *value *= st->maxSegments; /* XXX
					  this is a lie! the heap can grow
					  arbibtarily large; if mallocs
					  for larger than the initial
					  stretch size comes in, then
					  a stretch that size will be allocated
					  but that's probably rare so we'll
					  ignore it */
	}
	if (st->type == Heap_Type_Stretch) {
	    *value *= st->nSegments;
	}
	QTRC(eprintf("Query on heap %p kind %d result %qx\n", self, (int) kind, *value));
	return True;
    } 
	
    if (kind == Heap_Statistic_AllocatedData) {
	totalfree -= sizeof(*st);
    }
    for (i=0; i<COUNT; i++) {
	struct _heaprec *ptr;
	ptr = st->blocks[i];
	QTRC(eprintf("Starting list %d mode %d\n", i, (int)kind));
	while (ptr) {
	    QTRC(eprintf("Following free list %d pointer %p totalfree now %p\n", i, ptr, totalfree));
	    totalfree += ptr->hr_size;
	    if ( kind == Heap_Statistic_AllocatedData) 
		totalfree -= sizeof(*ptr);
	    
	    ptr = ptr->h_un.hru_next;
	}
	QTRC(eprintf("Done list %d\n", i));
    }
    *value = calculate_current_size(st) - totalfree;
    QTRC(eprintf("Query on heap %p kind %d result %qx\n", self, (int) kind, *value));
    return True;
}    


/* End of heapmod.c */
