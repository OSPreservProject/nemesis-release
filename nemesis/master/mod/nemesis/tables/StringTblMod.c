/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      mod/nemesis/tables/StringTblMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Hash tables mapping strings to addresses
** 
** ENVIRONMENT: 
**
**      Anywhere; Requires Heap.
** 
** ID : $Id: StringTblMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <string.h>

#include <StringTblMod.ih>


#define TRC(x) x
#define DEBUG(x)
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}


/*
 * Data structures
 */

typedef StringTbl_Key    Key;
typedef StringTbl_Val    Val;

/* Entries in a bucket list are kept in order */

typedef struct Bucket Bucket;
struct Bucket {
    Bucket      *next;
    Key		key;   /* strcmp (key, next->key) < 0 */
    Val		val;
};

typedef struct _StringTblMod_st StringTblMod_st;
struct _StringTblMod_st {
    Heap_clp	heap;
    uint32_t  	size;
    uint32_t	nBkts;
    Bucket      **bkts;
};

typedef struct {
    struct _StringTblIter_cl	cl;
    StringTblMod_st	       *tbl;
    Bucket		       *bkt;	/* != NULL => next bkt to return */
    word_t			i;	/* < tbl->nBkts => next chain    */
} StringTblIter_st;

static StringTblIter_Next_fn	IterNext_m;
static StringTblIter_Dispose_fn	IterDispose_m;
static StringTblIter_op		iter_ms = { IterNext_m, IterDispose_m };


/* 
 * Macros
 */

#define INITIAL_BKTS 23

#define CROWDED(st, nBkts) \
((nBkts) == MAXBKTS ? False : (st)->size > (nBkts) * 10)


/*
 * Prototypes
 */


static uint32_t  Hash (Key s);
static bool_t    Find (StringTblMod_st *st, Key k,
		       /*out*/ Bucket **b, Bucket ***prevp);
static void	 Expand (StringTbl_cl *self);

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

/* XXX - DEBUG */


#ifdef DEBUG_STRING_TBL
PUBLIC void
StringTblDump (StringTbl_cl *self)
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;
    int		   i;
    Bucket	  *b;

    eprintf ("StringTbl: %x: size=%x nBkts=%x bkts=%x Pvs(vp)=%x\n",
	     self, st->size, st->nBkts, st->bkts, Pvs(vp));

    for (i = 0; i < st->nBkts; i++) {
	eprintf ("StringTbl: %x bkt %x:\n", self, i);
	for (b = st->bkts[i]; b; b = b->next) {
	    uint32_t *p = (uint32_t *) b->key;
	    eprintf ("  bkt %x: next=%x val=%x key=%x (%x %x %x %x, %s)\n",
		     b, b->next, b->val, b->key, p[0],  p[1], p[2], p[3], b->key);
	}
    }
}
#endif
/*------------------------------------------------------ Entry points ---*/

static bool_t
Get_m (	StringTbl_cl	*self,
	StringTbl_Key	k,
	StringTbl_Val	*v )
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;
    Bucket *b, **prev;

    if (k == NULL) return False;

    if (Find (st, k, &b, &prev)) {
	*v = b->val;
	return True;
    }

    return False;
}

static bool_t
Put_m ( StringTbl_cl	*self,
	StringTbl_Key	k,
	StringTbl_Val	v )
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;
    Bucket *b, **prev;

    if (k == NULL) return False;

    if (CROWDED (st, st->nBkts)) Expand (self);

    if (Find (st, k, &b, &prev)) {
	b->val = v;
	return True;
    } else {
	Bucket *new = (Bucket *) Heap$Malloc (st->heap, sizeof (*new));
	new->key  = strduph (k, st->heap);
	new->val  = v;
	new->next = b;
	*prev     = new;
	(st->size)++;
	return False;
    }
}

static bool_t
Delete_m (StringTbl_cl	*self,
	  StringTbl_Key	k,
	  StringTbl_Val	*v )
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;

    Bucket *b, **prev;

    if (k == NULL || !Find (st, k, &b, &prev)) return False;

    *v    = b->val;
    *prev = b->next;
    Heap$Free (st->heap, b->key);
    Heap$Free (st->heap, b);
    (st->size)--;
    return True;
}

static uint32_t
Size_m (StringTbl_cl	*self )
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;
    return st->size;
}

static StringTblIter_clp
Iterate_m (StringTbl_cl	*self)
{
    StringTblMod_st  *st   = (StringTblMod_st *) self->st;
    StringTblIter_st *iter = Heap$Malloc (Pvs(heap), sizeof (*iter)); 

    if (!iter) RAISE_Heap$NoMemory();

    CL_INIT (iter->cl, &iter_ms, iter);
    iter->tbl = st;
    iter->bkt = NULL;
    iter->i   = 0;

    return &iter->cl;
}

static void
Dispose_m (StringTbl_cl *self )
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;
    int i;

    for (i = 0; i < st->nBkts; i++) {
	Bucket *b = st->bkts[i], *next;
	while (b != NULL) {
	    next = b->next;
	    Heap$Free (st->heap, b->key);
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
IterNext_m (StringTblIter_clp self, string_t *key, addr_t *val)
{
    StringTblIter_st *st = (StringTblIter_st *) self->st;

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
IterDispose_m (StringTblIter_cl *self)
{
    FREE (self);
}

/*------------------------------------------------- Internal Routines ---*/

#define MULT ((uint32_t) 2654435769u)

static uint32_t
Hash (Key s)
{
    uint32_t h = 0;

    while (*s != '\0')
	h = *s++ + MULT * h;

    return h;    
}

static bool_t
Find (StringTblMod_st *st, Key k, /*out*/ Bucket **b, Bucket ***prev)
{
    Bucket *b1, **prev1;

    for (prev1 = &(st->bkts[Hash (k) % st->nBkts]), b1 = *prev1;
	 b1 != NULL;
	 prev1 = &(b1->next), b1 = *prev1)
    {
	int cmp = strcmp (k, b1->key);
	if (cmp < 0)
	    break;
	else if (cmp == 0) {
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
Expand (StringTbl_cl *self)
{
    StringTblMod_st *st = (StringTblMod_st *) self->st;
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
	    Heap$Free (st->heap, b->key);
	    Heap$Free (st->heap, b);
	    b = next;
	}
    }
    Heap$Free (st->heap, oldBkts);
}

/*----------------------------------------------------- Creator Closure ---*/

/* StringTblMod_New_m must be public so that TypeSytem can initialise */

PUBLIC StringTblMod_New_fn StringTblMod_New_m;

static StringTblMod_op StringTblMod_ms = { StringTblMod_New_m };

static const StringTblMod_cl _StringTblMod = {
    &StringTblMod_ms,
    (addr_t) 0
};

CL_EXPORT (StringTblMod,StringTblMod,_StringTblMod);
CL_EXPORT_TO_PRIMAL (StringTblMod,StringTblModCl,_StringTblMod);


static StringTbl_op StringTbl_ms = {
    Get_m,
    Put_m,
    Delete_m,
    Size_m,
    Iterate_m,
    Dispose_m
};

PUBLIC StringTbl_clp
StringTblMod_New_m (StringTblMod_cl *self, Heap_clp heap)
{
    StringTbl_clp		res;
    StringTblMod_st      *st;
    uint32_t	i;

    res = Heap$Malloc (heap, sizeof (*res) + sizeof (*st));
    if (!res) RAISE_Heap$NoMemory ();

    st = (StringTblMod_st *) (res + 1);

    st->heap  = heap;
    st->size  = 0;
    st->nBkts = INITIAL_BKTS;

    st->bkts  = (Bucket **) Heap$Malloc (heap, INITIAL_BKTS * sizeof(Bucket *));
    if (!st->bkts)
    {
	Heap$Free (heap, res);
	RAISE_Heap$NoMemory ();
    }

    for (i = 0; i < INITIAL_BKTS; i++)
	st->bkts[i] = NULL;

    res->op = &StringTbl_ms;
    res->st = (addr_t) st;

    return res;
}
