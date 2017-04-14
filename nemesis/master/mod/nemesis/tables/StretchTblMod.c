/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/tables/StretchTblMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Hash table mapping Stretches to StretchDrivers
** 
** ENVIRONMENT: 
**
**      Requires Heap.
** 
** ID : $Id: StretchTblMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>

#include <StretchTblMod.ih>

#define TRC(x)
#define DEBUG(x)
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}


/*
 * Data structures
 */

typedef Stretch_clp          Key; 
typedef uint32_t             Val1;
typedef StretchDriver_clp    Val2;

/* Entries in a bucket list are kept in order */

typedef struct Bucket Bucket;
struct Bucket {
    Bucket     *next;
    Key		key;   /* (key < next->key) */
    Val1	val1;
    Val2	val2;
};

typedef struct _StretchTblMod_st StretchTblMod_st;
struct _StretchTblMod_st {
    Heap_clp  heap;
    uint32_t  size;
    uint32_t  nBkts;
    Bucket  **bkts;
};

/* 
 * Macros
 */

#define INITIAL_BKTS 23

#define CROWDED(st, nBkts) ((nBkts) == MAXBKTS ? False : (st)->size > (nBkts) * 10)



/*
 * Prototypes
 */

static StretchTbl_Get_fn    Get_m;
static StretchTbl_Put_fn    Put_m;
static StretchTbl_Remove_fn Remove_m;

static bool_t   Find (StretchTblMod_st *st, Key k,
		      /*out*/ Bucket **b, Bucket ***prevp);
static void	       Expand (StretchTbl_cl *self);

/* 
 * Standard sizes for bucket arrays; prime and increasing roughly
 * geometrically.  Pinched from the SRC hash table implementation.
 */

static const int standardSize[] = {
    23,
    89,
    181,
    359,
    719,
    1447,
    2887,
    4093,
    5791,
    8191,
    11579,
    16381,
    23167,
    32749,
    46337,
    65521,
    92681,
    131071,
    185363,
    262139,
    370723,
    524287,
    741431,
    1048573,
    1482907,
    2097143,
    2965819,
    4194301,
    5931641,
    8388593,
    11863279,
    16777213,
    23726561,
    33554393,
    47453111,
    67108859,
    94906249,
    134217689
};

#define MAXBKTS 134217689


/*------------------------------------------------------ Entry points ---*/

static bool_t Get_m(StretchTbl_cl *self, Key k, Val1 *v1, Val2 *v2)
{
    StretchTblMod_st *st = (StretchTblMod_st *) self->st;
    Bucket *b, **prev;
    
    if (Find (st, k, &b, &prev)) {
	*v1 = b->val1;
	*v2 = b->val2;
	return True;
    }
    
    return False;
}

static bool_t Put_m(StretchTbl_cl *self, Key k, Val1 v1, Val2 v2)
{
    StretchTblMod_st *st = (StretchTblMod_st *) self->st;
    Bucket *b, **prev;
    
    if (CROWDED (st, st->nBkts)) Expand (self);

    if (Find (st, k, &b, &prev)) {
	b->val1 = v1;
	b->val2 = v2;
	return True;
    } else {
	Bucket *new = (Bucket *) Heap$Malloc (st->heap, sizeof (*new));
	new->key  = k;
	new->val1 = v1;
	new->val2 = v2;
	new->next = b;
	*prev     = new;
	(st->size)++;
	return False;
    }
}

static bool_t Remove_m(StretchTbl_cl *self, Key k, Val1 *v1, Val2 *v2)
{
    StretchTblMod_st *st = (StretchTblMod_st *) self->st;

    Bucket *b, **prev;

    if (!Find (st, k, &b, &prev)) return False;

    *v1   = b->val1;
    *v2   = b->val2;
    *prev = b->next;
    Heap$Free (st->heap, b);
    (st->size)--;
    return True;
}


static void
Dispose_m (StretchTbl_cl *self )
{
    StretchTblMod_st *st = (StretchTblMod_st *) self->st;
    int i;

    for (i = 0; i < st->nBkts; i++) {
	Bucket *b = st->bkts[i], *next;
	while (b != NULL) {
	    next = b->next;
	    Heap$Free (st->heap, b);
	    b = next;
	}
    }

    Heap$Free (st->heap, self);
}

/*------------------------------------------------- Internal Routines ---*/


/* Our hash function will typically be hashing */
/* addresses, so we use a hash function which moves higher-bits down a */
/* bit more than simple mod. */

#define Hash(_w) (((word_t)(_w) ^ ((word_t)(_w) >> 8 )))

static bool_t
Find (StretchTblMod_st *st, Key k, /*out*/ Bucket **b, Bucket ***prev)
{
    Bucket *b1, **prev1;

    for (prev1 = &(st->bkts[Hash (k) % st->nBkts]), b1 = *prev1;
	 b1 != NULL;
	 prev1 = &(b1->next), b1 = *prev1)
    {
	TRC(eprintf("StretchTblMod$Find: k=%llx b1->key=%llx\n", k, b1->key));

	if (k < b1->key)
	    break;
	else if (k == b1->key) {
	    *b = b1;
	    *prev = prev1;
	    return True;
	}
    }
    *b = b1;
    *prev = prev1;
    return False;
}

static void
Expand (StretchTbl_cl *self)
{
    StretchTblMod_st *st = (StretchTblMod_st *) self->st;
    Bucket         **oldBkts;
    uint32_t   oldNBkts;
    uint32_t   newSize;
    uint32_t   i = 0;

    while (CROWDED (st, standardSize[i]))
	i++;
    newSize = standardSize[i];

    oldBkts  = st->bkts;
    oldNBkts = st->nBkts;

    st->size  = 0;
    st->nBkts = newSize;
    st->bkts  = (Bucket **) Heap$Malloc (st->heap, newSize * sizeof(Bucket *));
    for (i = 0; i < newSize; i++)
	st->bkts[i] = NULL;

    for (i = 0; i < oldNBkts; i++) {
	Bucket *b = oldBkts[i], *next;
	while (b != NULL) {
	    (void) self->op->Put (self, b->key, b->val1, b->val2);
	    next = b->next;
	    Heap$Free (st->heap, b);
	    b = next;
	}
    }
    Heap$Free (st->heap, oldBkts);
}

/*----------------------------------------------------- Creator Closure ---*/
static StretchTblMod_New_fn StretchTblMod_New_m;
static StretchTblMod_op StretchTblMod_ms = { StretchTblMod_New_m };
static const StretchTblMod_cl _StretchTblMod = {
    &StretchTblMod_ms, 
    (addr_t) 0
};

CL_EXPORT(StretchTblMod, StretchTblMod, _StretchTblMod);
CL_EXPORT_TO_PRIMAL(StretchTblMod, StretchTblModCl, _StretchTblMod);

static StretchTbl_op StretchTbl_ms = {
    Get_m,
    Put_m,
    Remove_m,
    Dispose_m
};

PUBLIC StretchTbl_clp
StretchTblMod_New_m (StretchTblMod_cl *self, Heap_clp heap)
{
    StretchTbl_clp		res;
    StretchTblMod_st      *st;
    uint32_t	i;

    res = Heap$Malloc (heap, sizeof (*res) + sizeof (*st));
    if (!res) RAISE_Heap$NoMemory ();

    st = (StretchTblMod_st *) (res + 1);

    st->heap  = heap;
    st->size  = 0;
    st->nBkts = INITIAL_BKTS;

    st->bkts  = (Bucket **)Heap$Malloc(heap, INITIAL_BKTS * sizeof(Bucket *));
    if (!st->bkts)
    {
	Heap$Free (heap, res);
	RAISE_Heap$NoMemory ();
    }

    for (i = 0; i < INITIAL_BKTS; i++)
	st->bkts[i] = NULL;

    res->op = &StretchTbl_ms;
    res->st = (addr_t) st;

    return res;
}
