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
**      mod/nemesis/tables/WordTblMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Hash table mapping words to addresses.
** 
** ENVIRONMENT: 
**
**      Requires Heap.
** 
** ID : $Id: WordTblMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>

#include <WordTblMod.ih>


#ifdef WTBL_TRACE
#define TRC(_x) _x
#define DEBUG
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#include <primouts.h>
#define DB(_x) _x
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}
#else
#define DB(_x)
#endif


/*
 * Data structures
 */

typedef WordTbl_Key    Key;
typedef WordTbl_Val    Val;

/* Entries in a bucket list are kept in order */

typedef struct Bucket Bucket;
struct Bucket {
    Bucket       *next;
    Key		key;   /* (key < next->key) */
    Val		val;
};

typedef struct _WordTblMod_st WordTblMod_st;
struct _WordTblMod_st {
    Heap_clp	  heap;
    uint32_t	  size;
    uint32_t  	  nBkts;
    Bucket        **bkts;
};

typedef struct {
    struct _WordTblIter_cl	cl;
    WordTblMod_st		       *tbl;
    Bucket		       *bkt;	/* != NULL => next bkt to return */
    word_t			i;	/* < tbl->nBkts => next chain    */
} WordTblIter_st;

static WordTblIter_Next_fn	IterNext_m;
static WordTblIter_Dispose_fn	IterDispose_m;
static WordTblIter_op		iter_ms = { IterNext_m, IterDispose_m };


/* 
 * Macros
 */

#define INITIAL_BKTS 23

#define CROWDED(st, nBkts) \
((nBkts) == MAXBKTS ? False : (st)->size > (nBkts) * 10)

#define HI_WORD(ll) ((word_t) ((ll)>>32))
#define LO_WORD(ll) ((word_t) ((ll) & 0xFFFFFFFF))



/*
 * Prototypes
 */

static WordTbl_Get_fn    Get_m;
static WordTbl_Put_fn    Put_m;
static WordTbl_Delete_fn Delete_m;
static WordTbl_Size_fn   Size_m;
static WordTbl_Destroy_fn Destroy_m;

static bool_t   Find (WordTblMod_st *st, Key k,
		      /*out*/ Bucket **b, Bucket ***prevp);
static void	Expand (WordTbl_cl *self);

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

static bool_t
Get_m (	WordTbl_cl	*self,
	WordTbl_Key	k,
	WordTbl_Val	*v )
{
    WordTblMod_st *st = (WordTblMod_st *) self->st;
    Bucket *b, **prev;

    TRC(eprintf("WordTbl$Get: %p\n", k));
    if (Find (st, k, &b, &prev)) {
	*v = b->val;
	TRC(eprintf("WordTbl$Get: found %p\n", *v));
	return True;
    }
  
    TRC(eprintf("WordTbl$Get: nothing found.\n"));
    return False;
}


static bool_t
Put_m ( WordTbl_cl	*self,
	WordTbl_Key	k,
	WordTbl_Val	v )
{
    WordTblMod_st *st = (WordTblMod_st *) self->st;
    Bucket *b, **prev;

    TRC(eprintf("WordTbl$Put: k=%p, v=%p\n", k, v));
    
    if (CROWDED (st, st->nBkts)) {
	TRC(eprintf("WordTbl$Put: expanding.\n"));
	Expand (self);
    }
    
    if (Find(st, k, &b, &prev)) {
	b->val = v;
	TRC(eprintf("WordTbl$Put: duplicate.\n"));
	return True;
    } else {
	Bucket *new = (Bucket *) Heap$Malloc (st->heap, sizeof (*new));
	new->key  = k;
	new->val  = v;
	new->next = b;
	*prev     = new;
	(st->size)++;
	TRC(eprintf("WordTbl$Put: done.\n"));
	return False;
    }
}

static bool_t
Delete_m (WordTbl_cl	*self,
	  WordTbl_Key	k,
	  WordTbl_Val	*v )
{
    WordTblMod_st *st = (WordTblMod_st *) self->st;

    Bucket *b, **prev;

    TRC(eprintf("WordTbl$Delete: k=%p\n", k));

    if (!Find (st, k, &b, &prev)) {
	TRC(eprintf("WordTbl$Delete: not found.\n"));
	return False;
    }

    *v    = b->val;
    *prev = b->next;
    Heap$Free (st->heap, b);
    (st->size)--;
    TRC(eprintf("WordTbl$Delete: done\n"));
    return True;
}

static uint32_t
Size_m (WordTbl_cl	*self )
{
    WordTblMod_st *st = (WordTblMod_st *) self->st;
    return st->size;
}

static WordTblIter_clp
Iterate_m (WordTbl_cl	*self)
{
    WordTblMod_st  *st   = (WordTblMod_st *) self->st;
    WordTblIter_st *iter = Heap$Malloc (Pvs(heap), sizeof (*iter)); 

    if (!iter) RAISE_Heap$NoMemory();

    CL_INIT (iter->cl, &iter_ms, iter);
    iter->tbl = st;
    iter->bkt = NULL;
    iter->i   = 0;

    return &iter->cl;
}

static void
Destroy_m (WordTbl_cl *self )
{
    WordTblMod_st *st = (WordTblMod_st *) self->st;
    int i;

    TRC(eprintf("WordTbl$Destroy: called.\n"));
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
/*
 * Iterator methods
 */
static bool_t
IterNext_m (WordTblIter_clp self, word_t *key, addr_t *val)
{
    WordTblIter_st *st = (WordTblIter_st *) self->st;

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
IterDispose_m (WordTblIter_cl *self)
{
    FREE (self);
}

/*------------------------------------------------- Internal Routines ---*/

/* Our hash function will typically be hashing */
/* addresses, so we use a hash function which moves higher-bits down a */
/* bit more than simple mod. */

#define Hash(_w) (((_w) ^ ((_w) >> 8 )))

static bool_t
Find (WordTblMod_st *st, Key k, /*out*/ Bucket **b, Bucket ***prev)
{
    Bucket *b1, **prev1;

    for (prev1 = &(st->bkts[Hash(k) % st->nBkts]), b1 = *prev1;
	 b1 != NULL;
	 prev1 = &(b1->next), b1 = *prev1)
    {

	TRC(eprintf("WordTblMod$Find: k=%p, b1->key=%p\n", k, b1->key));

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
Expand (WordTbl_cl *self)
{
    WordTblMod_st *st = (WordTblMod_st *) self->st;
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
	    (void) self->op->Put (self, b->key, b->val);
	    next = b->next;
	    Heap$Free (st->heap, b);
	    b = next;
	}
    }
    Heap$Free (st->heap, oldBkts);
}

/*----------------------------------------------------- Creator Closure ---*/

static WordTblMod_New_fn New;
static WordTblMod_op wtm_ms = { New };
static const WordTblMod_cl wtm_cl = { &wtm_ms, (addr_t) 0 };
CL_EXPORT(WordTblMod,WordTblMod,wtm_cl);

static WordTbl_op wt_ms = {
    Get_m,
    Put_m,
    Delete_m,
    Size_m,
    Iterate_m,
    Destroy_m
};

static WordTbl_clp
New (WordTblMod_cl *self, Heap_clp heap)
{
    struct _WordTbl_cl *res;
    WordTblMod_st      *st;
    uint32_t	i;

    res = Heap$Malloc (heap, sizeof (*res) + sizeof (*st));
    if (!res) {
	DB(eprintf("WordTblMod$New: failed to allocate closure.\n"));
	RAISE_Heap$NoMemory ();
	return NULL;
    };
  
    st = (WordTblMod_st *) (res + 1);

    st->heap  = heap;
    st->size  = 0;
    st->nBkts = INITIAL_BKTS;

    st->bkts  = (Bucket **) Heap$Malloc (heap, INITIAL_BKTS * sizeof(Bucket *));
    if (!st->bkts)
    { 
	DB(eprintf("WordTblMod$New: failed to allocate buckets.\n"));
	Heap$Free (heap, res);
	RAISE_Heap$NoMemory ();
	return NULL;
    }

    for (i = 0; i < INITIAL_BKTS; i++)
	st->bkts[i] = NULL;

    res->op = &wt_ms;
    res->st = (addr_t) st;

    return res;
}
