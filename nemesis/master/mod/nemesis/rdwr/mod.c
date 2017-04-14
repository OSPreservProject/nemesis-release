/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	mod.c
**
** FUNCTIONAL DESCRIPTION:
**
**	Create all kinds of useful readers and writers
**
** ENVIRONMENT: 
**
**	User space
**
** ID : $Id: mod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <Rd.ih>
#include <Wr.ih>
#include <RdWrMod.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static  RdWrMod_GetBlobRd_fn            RdWrMod_GetBlobRd_m;
static  RdWrMod_NewPipe_fn              RdWrMod_NewPipe_m;
static  RdWrMod_NewJoin_fn              RdWrMod_NewJoin_m;
static  RdWrMod_NewBufferedWr_fn        RdWrMod_NewBufferedWr_m;
static  RdWrMod_NewBufferedRd_fn        RdWrMod_NewBufferedRd_m;
static  RdWrMod_NewMemRd_fn             RdWrMod_NewMemRd_m;

static  RdWrMod_op      ms = {
    RdWrMod_GetBlobRd_m,
    RdWrMod_NewPipe_m,
    RdWrMod_NewJoin_m,
    RdWrMod_NewBufferedWr_m,
    RdWrMod_NewBufferedRd_m,
    RdWrMod_NewMemRd_m
};

static const RdWrMod_cl      cl = { &ms, NULL };

CL_EXPORT (RdWrMod, RdWrMod, cl);

extern Rd_clp CreatePipe(Heap_clp heap, uint32_t bufsize,
			 uint32_t trigger, Wr_clp *wr);
extern Closure_clp CreateJoin(Heap_clp heap, uint32_t bufsize,
			      Rd_clp rd, Wr_clp wr, bool_t closeWr);
extern Wr_clp CreateBufferedWr(Heap_clp heap, uint32_t bufsize,
			       uint32_t blocksize, Wr_clp wr);
extern Rd_clp CreateBufferedRd(Heap_clp heap, uint32_t bufsize,
			       uint32_t blocksize, Rd_clp rd);
extern Rd_clp CreateMemRd(addr_t base, uint64_t bytes, Heap_clp heap);

typedef struct {
    addr_t base;
    word_t len;
} blob_t;

/*---------------------------------------------------- Entry Points ----*/

static Rd_clp RdWrMod_GetBlobRd_m (
    RdWrMod_cl      *self,
    RdWrMod_Blob    b       /* IN */,
    Heap_clp        heap    /* IN */ )
{
    blob_t *blob=b;

    return CreateMemRd(blob->base, blob->len, heap);
}

static Rd_clp RdWrMod_NewPipe_m (
    RdWrMod_cl      *self,
    Heap_clp        heap    /* IN */,
    uint32_t        bufsize /* IN */,
    uint32_t        trigger /* IN */
    /* RETURNS */,
    Wr_clp  *wr )
{
    return CreatePipe(heap, bufsize, trigger, wr);
}

static Closure_clp RdWrMod_NewJoin_m (
    RdWrMod_cl      *self,
    Heap_clp        heap,
    uint32_t        bufsize,
    Rd_clp          rd,
    Wr_clp          wr,
    bool_t          closeWr)
{
    return CreateJoin(heap, bufsize, rd, wr, closeWr);
}

static Wr_clp RdWrMod_NewBufferedWr_m (
    RdWrMod_cl      *self,
    Heap_clp        heap    /* IN */,
    uint32_t        bufsize /* IN */,
    uint32_t        blocksize       /* IN */,
    uint32_t        trigger /* IN */,
    Wr_clp  wr      /* IN */ )
{
    /* Let's raise a sensible exception rather than just returning
       NULL: */
    RAISE_Heap$NoMemory();
/*    return CreateBufferedWr(heap, bufsize, blocksize, wr); */
    return NULL;
}

static Rd_clp RdWrMod_NewBufferedRd_m (
    RdWrMod_cl      *self,
    Heap_clp        heap    /* IN */,
    uint32_t        bufsize /* IN */,
    uint32_t        blocksize       /* IN */,
    uint32_t        trigger /* IN */,
    Rd_clp  rd      /* IN */ )
{
    RAISE_Heap$NoMemory();
/*    return CreateBufferedRd(heap, bufsize, blocksize, rd); */
    return NULL;
}

static Rd_clp RdWrMod_NewMemRd_m (
    RdWrMod_cl      *self,
    addr_t  base    /* IN */,
    uint64_t  bytes   /* IN */,
    Heap_clp        heap    /* IN */ )
{
    return CreateMemRd(base, bytes, heap);
}

/*
 * End 
 */
