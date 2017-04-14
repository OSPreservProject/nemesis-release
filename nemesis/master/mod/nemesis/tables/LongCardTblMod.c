/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/tables/LongCardTblMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Hash table mapping long cardinals to addresses.
** 
** ENVIRONMENT: 
**
**      Requires Heap.
** 
** ID : $Id: LongCardTblMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <sequence.h>
#include <time.h>
#include <ecs.h>
#include <ntsc.h>

#include <LongCardTblMod.ih>


#define TRC(x)
#define DEBUG(x)
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}

typedef SEQ_STRUCT(addr_t) addrseq;
 
/*
 * Data structures
 */

typedef LongCardTbl_Key    Key;
typedef LongCardTbl_Val    Val;

/* Entries in a bucket list are kept in order */

typedef struct Bucket Bucket;
struct Bucket {
    Bucket       *next;
    Key		key;   /* (key < next->key) */
    Val		val;
};

typedef struct _LongCardTblMod_st LongCardTblMod_st;
struct _LongCardTblMod_st {
    LongCardTbl_cl cl;
    Heap_clp	   heap;
    uint32_t       size;
    uint32_t       nBkts;
    Bucket       **bkts;

    /* Information used by safe tables */
    bool_t         safe;
    volatile bool_t         updating;
    uint64_t       generation;
    Bucket        *freeBuckets;
    addrseq       *oldArrays;
};

typedef struct {
    struct _LongCardTblIter_cl	cl;
    LongCardTblMod_st	       *tbl;
    Bucket		       *bkt;	/* != NULL => next bkt to return */
    word_t			i;	/* < tbl->nBkts => next chain    */
} LongCardTblIter_st;

static LongCardTblIter_Next_fn	IterNext_m;
static LongCardTblIter_Dispose_fn	IterDispose_m;
static LongCardTblIter_op		iter_ms = { IterNext_m, IterDispose_m };


/* 
 * Macros
 */

#define INITIAL_BKTS 23

#define CROWDED(st, nBkts) \
((nBkts) == MAXBKTS ? False : (st)->size > (nBkts) * 10)



/*
 * Prototypes
 */

    static LongCardTbl_Get_fn    Get_m;
    static LongCardTbl_Put_fn    Put_m;
    static LongCardTbl_Delete_fn Delete_m;
    static LongCardTbl_Size_fn   Size_m;

    static bool_t   Find (LongCardTblMod_st *st, Key k,
			  /*out*/ Bucket **b, Bucket ***prevp);
    static void	       Expand (LongCardTbl_cl *self);

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

Bucket *GetBucket(LongCardTblMod_st *st) {

    if(st->safe && st->freeBuckets) {
	Bucket *res = st->freeBuckets;
	st->freeBuckets = res->next;
	return res;
    }

    return (Bucket *) Heap$Malloc(st->heap, sizeof(Bucket));
}
	
void ReleaseBucket(LongCardTblMod_st *st, Bucket *b) {
    
    if(st->safe) {
	b->next = st->freeBuckets;
	st->freeBuckets = b;
    } else {
	FREE(b);
    }
}

static bool_t
Get_m (	LongCardTbl_cl	*self,
	LongCardTbl_Key	k,
	LongCardTbl_Val	*v )
{
    LongCardTblMod_st *st = (LongCardTblMod_st *) self->st;
    Bucket *b, **prev;
    uint64_t generation = 0;
    bool_t res = False;
    bool_t safe = st->safe;
again:
    if(safe) {
	int retry = 0;
	while(st->updating) {
	    TRC(eprintf("!"));
	    /* We hit a race - hang around until st->updating is
               clear. The first few times just try yielding - in the
               case that the updating thread is in our domain, this
               might let it clear. If this does no good start
               pausing. */
	    
	    retry++;
	    if(retry <= 10) {
		Threads$Yield(Pvs(thds));
	    } else {
		PAUSE(MICROSECS(100));
	    }

	    /* If we've waited more than five seconds, something is
               probably wrong ... */
	    if(retry == 50000) {
		ntsc_dbgstop();
	    }
	}
	generation = st->generation;
    }

    if (Find (st, k, &b, &prev)) {
	*v = b->val;
	res = True;
    } else {
	res = False;
    }
    
    if(safe && generation != st->generation) {
	goto again;
    }

    return res;
}

static bool_t
Put_m ( LongCardTbl_cl	*self,
	LongCardTbl_Key	k,
	LongCardTbl_Val	v )
{
    LongCardTblMod_st *st = (LongCardTblMod_st *) self->st;
    Bucket *b, **prev;
    
    bool_t safe = st->safe;
    bool_t res;
    bool_t toplevel = False;

    if(safe) {
	toplevel = !st->updating;
	if(toplevel) {
	    st->updating = True;
	    st->generation++;
	}
    }

    if (CROWDED (st, st->nBkts)) Expand (self);

    if (Find (st, k, &b, &prev)) {
	b->val = v;
	res = True;
    } else {
	Bucket *new = GetBucket(st);
	new->key  = k;
	new->val  = v;
	new->next = b;
	*prev     = new;
	(st->size)++;
	res = False;
    }

    if(safe & toplevel) {
	st->updating = False;
    }

    return res;
}

static bool_t
Delete_m (LongCardTbl_cl	*self,
	  LongCardTbl_Key	k,
	  LongCardTbl_Val	*v )
{
    LongCardTblMod_st *st = (LongCardTblMod_st *) self->st;

    Bucket *b, **prev;

    bool_t safe = st->safe;
    bool_t res;

    if(safe) {
	st->updating = True;
	st->generation++;
    }

    if (!Find (st, k, &b, &prev)) {
	res = False;
    } else {
	*v    = b->val;
	*prev = b->next;
	ReleaseBucket(st, b);
	(st->size)--;
	res = True;
    }

    if(safe) {
	st->updating = False;
    }

    return res;
    
}

static uint32_t
Size_m (LongCardTbl_cl	*self )
{
    LongCardTblMod_st *st = (LongCardTblMod_st *) self->st;
    return st->size;
}

static LongCardTblIter_clp
Iterate_m (LongCardTbl_cl	*self)
{
    LongCardTblMod_st  *st   = (LongCardTblMod_st *) self->st;
    LongCardTblIter_st *iter = Heap$Malloc (Pvs(heap), sizeof (*iter)); 

    if (!iter) RAISE_Heap$NoMemory();

    CL_INIT (iter->cl, &iter_ms, iter);
    iter->tbl = st;
    iter->bkt = NULL;
    iter->i   = 0;

    return &iter->cl;
}

static void
Dispose_m (LongCardTbl_cl *self )
{
    LongCardTblMod_st *st = (LongCardTblMod_st *) self->st;
    Bucket *b, *next;
    int i;

    for (i = 0; i < st->nBkts; i++) {
	b = st->bkts[i];
	while (b != NULL) {
	    next = b->next;
	    Heap$Free (st->heap, b);
	    b = next;
	}
    }

    b = st->freeBuckets;
    while(b) {
	next = b->next;
	FREE(b);
	b = next;
    }
	
    FREE(st->bkts);

    if(st->oldArrays) {
	SEQ_FREE_ELEMS(st->oldArrays);
	SEQ_FREE(st->oldArrays);
    }

    Heap$Free (st->heap, self);
}

/*
 * Iterator methods
 */
static bool_t
IterNext_m (LongCardTblIter_clp self, Key *key, Val *val)
{
    LongCardTblIter_st *st = (LongCardTblIter_st *) self->st;

    while (st->bkt == NULL && st->i < st->tbl->nBkts)
	st->bkt = st->tbl->bkts [st->i++];

    if (!st->bkt)
	return False;

    *key = st->bkt->key;
    *val = st->bkt->val;

    st->bkt = st->bkt->next;

    return True;
}

static void
IterDispose_m (LongCardTblIter_cl *self)
{
    FREE (self);
}

/*------------------------------------------------- Internal Routines ---*/

/* The '%' operator on long long uses __umoddi3 in libgcc on MIPS, which
   in our case we have not got.  So for now, the secret is to bang the bits
   together, guys: */

/* (the above comment applies to Intel as well) */

#if defined (__MIPS__) || defined (__IX86__)
#define Hash(lc) ((uint32_t) (HIGH32(lc) ^ LOW32(lc)))
#else
#define Hash(lc) (((lc) ^ ((lc) >> 8 )))
#endif

static bool_t
Find (LongCardTblMod_st *st, Key k, /*out*/ Bucket **b, Bucket ***prev)
{
    Bucket *b1, **prev1;

    for (prev1 = &(st->bkts[Hash (k) % st->nBkts]), b1 = *prev1;
	 b1 != NULL;
	 prev1 = &(b1->next), b1 = *prev1)
    {
	TRC (eprintf ("LongCardTblMod$Find: k=%llx b1->key=%llx\n", k, b1->key));

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
Expand (LongCardTbl_cl *self)
{
    LongCardTblMod_st *st = (LongCardTblMod_st *) self->st;
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
    st->bkts  = (Bucket **) Heap$Calloc (st->heap, newSize, sizeof(Bucket *));
    st->nBkts = newSize;
    
    for (i = 0; i < oldNBkts; i++) {
	Bucket *b = oldBkts[i], *next;
	while (b != NULL) {
	    Put_m (self, b->key, b->val);
	    next = b->next;
	    ReleaseBucket (st, b);
	    b = next;
	}
    }

    if(st->safe) {
	if(!st->oldArrays) {
	    st->oldArrays = SEQ_NEW(addrseq, 0, st->heap);
	}
	SEQ_ADDH(st->oldArrays, oldBkts);
    } else {
	FREE(oldBkts);
    }
}

/*----------------------------------------------------- Creator Closure ---*/

static LongCardTblMod_New_fn LongCardTblMod_New_m;

static LongCardTblMod_op tblmod_ms = { LongCardTblMod_New_m };
static const LongCardTblMod_cl tblmod_cl   = {&tblmod_ms, NULL};

static LongCardTblMod_New_fn SafeLongCardTblMod_New_m;

static LongCardTblMod_op safetblmod_ms = { SafeLongCardTblMod_New_m };
static const LongCardTblMod_cl safetblmod_cl   = {&safetblmod_ms, NULL};

CL_EXPORT(LongCardTblMod, LongCardTblMod, tblmod_cl);
CL_EXPORT_TO_PRIMAL(LongCardTblMod, LongCardTblModCl, tblmod_cl);

CL_EXPORT(LongCardTblMod, SafeLongCardTblMod, safetblmod_cl)
CL_EXPORT_TO_PRIMAL(LongCardTblMod, SafeLongCardTblModCl, safetblmod_cl);

static LongCardTbl_op LongCardTbl_ms = {
    Get_m,
    Put_m,
    Delete_m,
    Size_m,
    Iterate_m,
    Dispose_m
};

static LongCardTbl_clp NewTable(Heap_clp heap, bool_t safe) {

    LongCardTblMod_st      *st;
    uint32_t	i;

    st = Heap$Malloc (heap, sizeof (*st));
    if (!st) RAISE_Heap$NoMemory ();

    st->heap  = heap;
    st->size  = 0;
    st->nBkts = INITIAL_BKTS;

    st->safe  = safe;
    st->freeBuckets = NULL;
    st->oldArrays = NULL;
    st->generation = 0;
    st->updating = False;

    st->bkts  = (Bucket **) Heap$Malloc (heap, INITIAL_BKTS * sizeof(Bucket *));
    if (!st->bkts) {
	FREE(st);
	RAISE_Heap$NoMemory ();
    }

    for (i = 0; i < INITIAL_BKTS; i++)
	st->bkts[i] = NULL;

    CL_INIT(st->cl, &LongCardTbl_ms, st);

    return &st->cl;
    
}
static LongCardTbl_clp 
LongCardTblMod_New_m (LongCardTblMod_cl *self, Heap_clp heap)
{
    return NewTable(heap, False);
}

static LongCardTbl_clp
SafeLongCardTblMod_New_m(LongCardTblMod_cl *self, Heap_clp heap)
{
    return NewTable(heap, True);
}
