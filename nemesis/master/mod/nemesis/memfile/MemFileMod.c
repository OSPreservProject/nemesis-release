/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      lib/nemesis/RdWr/MemFileMod.c
**
** FUNCTIONAL DESCRIPTION:
**
**      Memory files.
**
** ENVIRONMENT:
**
**      Static, reentrant, anywhere
**
** ID : $Id: MemFileMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <string.h>
#include <stdio.h>
#include <salloc.h>

#include <MemFileMod.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/*
 * Module stuff
 */
static  MemFileMod_New_fn	New_m;
static  MemFileMod_op		ms = { New_m };
static  const MemFileMod_cl	cl = { &ms, NULL };

CL_EXPORT (MemFileMod, MemFileMod, cl);


static  File_OpenRd_fn                 MemFile_OpenRd_m;
static  File_OpenWr_fn                 MemFile_OpenWr_m;
static  File_Dispose_fn                MemFile_Dispose_m;
static  MemFile_Lock_fn                MemFile_Lock_m;
static  MemFile_Unlock_fn              MemFile_Unlock_m;
static  MemFile_IsDirty_fn             MemFile_IsDirty_m;
static  MemFile_MarkClean_fn           MemFile_MarkClean_m;
static  MemFile_LOpenRd_fn             MemFile_LOpenRd_m;
static  MemFile_LOpenWr_fn             MemFile_LOpenWr_m;

static  MemFile_op     mf_ms = {
    MemFile_OpenRd_m,
    MemFile_OpenWr_m,
    MemFile_Dispose_m,
    MemFile_Lock_m,
    MemFile_Unlock_m,
    MemFile_IsDirty_m,
    MemFile_MarkClean_m,
    MemFile_LOpenRd_m,
    MemFile_LOpenWr_m
};

static  Rd_GetC_fn              Rd_GetC_m;
static  Rd_EOF_fn               Rd_EOF_m;
static  Rd_UnGetC_fn            Rd_UnGetC_m;
static  Rd_CharsReady_fn        Rd_CharsReady_m;
static  Rd_GetChars_fn          Rd_GetChars_m;
static  Rd_GetLine_fn           Rd_GetLine_m;
static  Rd_Seek_fn              Rd_Seek_m;
static  Rd_Close_fn             Rd_Close_m;
static  Rd_Length_fn            Rd_Length_m;
static  Rd_Index_fn             Rd_Index_m;
static  Rd_Intermittent_fn      Rd_Intermittent_m;
static  Rd_Seekable_fn          Rd_Seekable_m;
static  Rd_Closed_fn            Rd_Closed_m;
static  Rd_Lock_fn              Rd_Lock_m;
static  Rd_Unlock_fn            Rd_Unlock_m;
static  Rd_LGetC_fn             Rd_LGetC_m;
static  Rd_LEOF_fn              Rd_LEOF_m;

static  Rd_op   rd_ms = {
    Rd_GetC_m,
    Rd_EOF_m,
    Rd_UnGetC_m,
    Rd_CharsReady_m,
    Rd_GetChars_m,
    Rd_GetLine_m,
    Rd_Seek_m,
    Rd_Close_m,
    Rd_Length_m,
    Rd_Index_m,
    Rd_Intermittent_m,
    Rd_Seekable_m,
    Rd_Closed_m,
    Rd_Lock_m,
    Rd_Unlock_m,
    Rd_LGetC_m,
    Rd_LEOF_m
};


static  Wr_PutC_fn              Wr_PutC_m;
static  Wr_PutStr_fn            Wr_PutStr_m;
static  Wr_PutChars_fn          Wr_PutChars_m;
static  Wr_Seek_fn              Wr_Seek_m;
static  Wr_Flush_fn             Wr_Flush_m;
static  Wr_Close_fn             Wr_Close_m;
static  Wr_Length_fn            Wr_Length_m;
static  Wr_Index_fn             Wr_Index_m;
static  Wr_Seekable_fn          Wr_Seekable_m;
static  Wr_Closed_fn            Wr_Closed_m;
static  Wr_Buffered_fn          Wr_Buffered_m;
static  Wr_Lock_fn              Wr_Lock_m;
static  Wr_Unlock_fn            Wr_Unlock_m;
static  Wr_LPutC_fn             Wr_LPutC_m;
static  Wr_LPutStr_fn           Wr_LPutStr_m;
static  Wr_LPutChars_fn         Wr_LPutChars_m;
static  Wr_LFlush_fn            Wr_LFlush_m;

static  Wr_op   wr_ms = {
    Wr_PutC_m,
    Wr_PutStr_m,
    Wr_PutChars_m,
    Wr_Seek_m,
    Wr_Flush_m,
    Wr_Close_m,
    Wr_Length_m,
    Wr_Index_m,
    Wr_Seekable_m,
    Wr_Closed_m,
    Wr_Buffered_m,
    Wr_Lock_m,
    Wr_Unlock_m,
    Wr_LPutC_m,
    Wr_LPutStr_m,
    Wr_LPutChars_m,
    Wr_LFlush_m
};

/*
 * Data Structures
 */

#define BLOCK_SIZE		FRAME_SIZE
#define BLOCK_WIDTH		FRAME_WIDTH

typedef struct {
    Stretch_clp	s;
    int8_t       *bytes;
} block_t;

typedef SEQ_STRUCT(block_t)	blocks_t;
    
typedef struct {
    MemFile_cl		 cl;
    mutex_t		 mu;
    StretchAllocator_clp sa;
    IDCTransport_clp	 shmt;
    uint32_t		 rds;
    uint32_t		 wrs;
    bool_t		 dirty;
    bool_t		 dying;
    blocks_t		 blocks;
    uint32_t		 unused;	/* bytes free in last block */
} MemFile_st;

typedef struct {
    Wr_cl		 cl;
    
    MemFile_st	         *mf;
    uint32_t		 index; /* LL >= {mf->mu} for now */
} MemFileWr_st;

typedef struct {
    Rd_cl		 cl;
    
    MemFile_st   	 *mf;
    uint32_t		 index; /* LL >= {mf->mu} for now */
} MemFileRd_st;

#define MF_LEN(mf)	((SEQ_LEN (&(mf)->blocks) << BLOCK_WIDTH) - mf->unused)
#define MF_BLK(mf,ix)	((SEQ_ELEM (&(mf)->blocks, (ix) >> BLOCK_WIDTH)).bytes)
#define MF_BYTE(ix)	((ix) & (BLOCK_SIZE - 1))

#define MF_BLK_SIZE(mf,ix)					\
  (BLOCK_SIZE -							\
    (								\
     (((ix) >> BLOCK_WIDTH) == SEQ_LEN (&(mf)->blocks) - 1) ?	\
     mf->unused : 0						\
    )								\
  )


/*--------------------------------------------------------- Creator ----*/

static MemFile_clp
New_m (MemFileMod_cl *self, Heap_clp h)
{
    MemFile_st   *st = Heap$Calloc (h, 1, sizeof (*st));

    if (!st) RAISE_Heap$NoMemory();

    CL_INIT (st->cl, &mf_ms, st);
    MU_INIT (&st->mu);
    st->sa   = Pvs(salloc); /* XXX */
    st->shmt = NAME_FIND ("modules>ShmTransport", IDCTransport_clp);
    SEQ_CLEAR (SEQ_INIT (&st->blocks, 16, h));

    TRC(printf("MemFile$New successful, returning MemFile_clp=%p\n",
		&(st->cl)));

    return &st->cl;
}


/*---------------------------------------------------- Entry Points ----*/

static Rd_clp
MemFile_OpenRd_m (File_cl *self)
{
    MemFile_st   *st  = (MemFile_st *) self->st;
    Rd_clp NOCLOBBER res = NULL;

    USING (MUTEX, &st->mu, res = MemFile_LOpenRd_m ((MemFile_clp) self,
						    Pvs(heap)); );
    /* XXX - use publicly readable heap             ^^^^^^^^ */
    return res;
}

static Wr_clp
MemFile_OpenWr_m (File_cl *self)
{
    MemFile_st   *st = (MemFile_st *) self->st;
    Wr_clp NOCLOBBER res = NULL;

    USING (MUTEX, &st->mu, res = MemFile_LOpenWr_m ((MemFile_clp) self,
						    Pvs(heap)); );
    /* XXX - use publicly readable heap             ^^^^^^^^ */
    return res;
}

static void
MemFile_Dispose_m (File_cl *self)
{
    MemFile_st   *st = (MemFile_st *) self->st;

    USING (MUTEX, &st->mu,
       {
	   st->dying = True;

	   if (!st->rds && !st->wrs)
	   {
	       block_t *bp;

	       for (bp = SEQ_START (&st->blocks); bp < SEQ_END (&st->blocks); bp++)
		   if (bp->s)
		   {
		       TRC (printf ("MemFileMod$Dispose: destroy stretch %x\n", bp->s));
		       STR_DESTROY(bp->s);
		   }
	       MU_FREE (&st->mu);
	       SEQ_FREE_DATA (&st->blocks);
	       FREE (st);
	       return;
	   } 
       });
}

static void
MemFile_Lock_m (MemFile_cl *self)
{
    MemFile_st *st = (MemFile_st *) self->st;
    MU_LOCK (&st->mu);
}

static void
MemFile_Unlock_m (MemFile_cl *self)
{
    MemFile_st *st = (MemFile_st *) self->st;
    MU_RELEASE (&st->mu);
}

static bool_t
MemFile_IsDirty_m (MemFile_cl *self)
{
    MemFile_st   *st = (MemFile_st *) self->st;
    return st->dirty;
}

static void
MemFile_MarkClean_m (MemFile_cl *self)
{
    MemFile_st   *st = (MemFile_st *) self->st;
    st->dirty = False;
}

static Rd_clp
MemFile_LOpenRd_m (MemFile_cl *self, Heap_clp h)
{
    MemFile_st    *st = (MemFile_st *) self->st;
    MemFileRd_st  *res;
    Type_Any      any;

    if (st->dying) RAISE ("MemFile:Dead!", NULL); /* => client bug */

    res = Heap$Malloc (h, sizeof (*res));
    if (!res) RAISE_Heap$NoMemory();
    CL_INIT (res->cl, &rd_ms, res);

    res->mf    = st;
    res->index = 0;
    st->rds++;

    return &res->cl;
}

static Wr_clp
MemFile_LOpenWr_m (MemFile_cl *self, Heap_clp h)
{
    MemFile_st    *st = (MemFile_st *) self->st;
    MemFileWr_st  *res;
    Type_Any      any;
  
    if (st->dying) RAISE ("MemFile:Dead!", NULL); /* => client bug */
  
    res = Heap$Malloc (h, sizeof (*res));
    if (!res) RAISE_Heap$NoMemory();
    CL_INIT (res->cl, &wr_ms, res);
  
    res->mf    = st;
    res->index = 0;
    st->wrs++;
  
    return &res->cl;
}


/*----------------------------------------------------------- Readers ---*/

static int8_t
Rd_GetC_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    NOCLOBBER int8_t	res = 0;

    USING (MUTEX, &mf->mu,
       {
	   if (st->index >= MF_LEN (mf))
	       RAISE_Rd$EndOfFile();

	   res = MF_BLK (mf, st->index) [MF_BYTE (st->index)];
	   st->index++;
       });

    return res;
}

static bool_t
Rd_EOF_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    NOCLOBBER bool_t   res = False;

    USING (MUTEX, &mf->mu, res = (st->index >= MF_LEN (mf)); );
    return res;
}

static void
Rd_UnGetC_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    USING (MUTEX, &mf->mu,
       {
	   if (st->index > 0 )
	       --(st->index);
	   else
	       RAISE_Rd$Failure (1);
       });
}

static uint32_t
Rd_CharsReady_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    NOCLOBBER uint32_t res = 0;

    USING (MUTEX, &mf->mu, res = MAX (MF_LEN (mf) - st->index, 1); );
    return res;
}

static uint32_t
Rd_GetChars_m (
	       Rd_cl   	*self,
	       Rd_anon0        buf /* IN */,
	       uint32_t        nb /* IN */ )
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    uint32_t		NOCLOBBER res = 0;

    USING (MUTEX, &mf->mu,
       {
	   if (nb > MF_LEN (mf) - st->index)
	       nb = MF_LEN (mf) - st->index;
	   res = nb;

	   while (nb)
	   {
	       int8_t	*bytes = MF_BLK (mf, st->index) + MF_BYTE (st->index);
	       uint32_t	 avail = MF_BLK_SIZE (mf, st->index) - MF_BYTE (st->index);
	       uint32_t	 copy  = MIN (nb, avail);

	       memcpy (buf, bytes, copy);

	       nb        -= copy;
	       buf       += copy;
	       st->index += copy;
	   }
       });
    return res;
}

static uint32_t
Rd_GetLine_m (
	      Rd_cl   *self,
	      Rd_anon0        buf /* IN */,
	      uint32_t        nb /* IN */ )
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    uint32_t		NOCLOBBER res = 0;

    USING (MUTEX, &mf->mu,
       {
	   while ((res != nb) &&
		  (res == 0 || buf[res-1] != '\n') &&
		  (st->index < MF_LEN (mf)))
	   {
	       buf[res++] = MF_BLK (mf, st->index) [MF_BYTE (st->index)];
	       st->index++;
	   }
       });
    return res;
}

static void
Rd_Seek_m (Rd_cl *self, uint32_t nb /* IN */ )
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    USING (MUTEX, &mf->mu, st->index = MIN (nb, MF_LEN (mf)); );
}

static void
Rd_Close_m (Rd_cl   *self)
{
    MemFileRd_st       *st      = (MemFileRd_st *) self->st;
    MemFile_st	       *mf      = st->mf;
    bool_t NOCLOBBER	dispose = False;

    USING (MUTEX, &mf->mu,
       {
	   mf->rds--;
	   dispose = (mf->dying && !mf->rds && !mf->wrs);
	   FREE (st);
       });

    if (dispose)
	MemFile_Dispose_m ((File_clp) &mf->cl);
}

static int32_t
Rd_Length_m (Rd_cl   *self )
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    NOCLOBBER uint32_t res = 0;

    USING (MUTEX, &mf->mu, res = MF_LEN (mf); );
    return res;
}

static uint32_t
Rd_Index_m (Rd_cl   *self )
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    return st->index;
}

static bool_t
Rd_Intermittent_m (Rd_cl   *self )
{
    return False;
}

static bool_t
Rd_Seekable_m (Rd_cl   *self )
{
    return True;
}

static bool_t
Rd_Closed_m (Rd_cl   *self )
{
    return False;
}

static void
Rd_Lock_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    MU_LOCK (&mf->mu);
}

static void
Rd_Unlock_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    MU_RELEASE (&mf->mu);
}

static int8_t
Rd_LGetC_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    int8_t		res;

    if (st->index >= MF_LEN (mf))
	RAISE_Rd$EndOfFile();

    res = MF_BLK (mf, st->index) [MF_BYTE (st->index)];
    st->index++;
    return res;
}

static bool_t
Rd_LEOF_m (Rd_cl   *self)
{
    MemFileRd_st       *st  = (MemFileRd_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    return (st->index >= MF_LEN (mf));
}

/*------------------------------------------------------------ Writers ---*/


static INLINE void InlineLPutC (MemFileWr_st *st, MemFile_st *mf, int8_t c);

static void
Wr_PutC_m (Wr_cl *self, int8_t ch)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    USING (MUTEX, &mf->mu, InlineLPutC (st, mf, ch); mf->dirty = True; );
}

static void
Wr_PutStr_m (Wr_cl *self, string_t s)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    USING (MUTEX, &mf->mu, Wr_LPutStr_m (self, s); );
}

static void
Wr_PutChars_m (Wr_cl *self, Wr_anon0 s, uint32_t nb)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    USING (MUTEX, &mf->mu, Wr_LPutChars_m (self, s, nb); );
}

static void
Wr_Seek_m (Wr_cl *self, uint32_t n)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    USING (MUTEX, &mf->mu, st->index = MIN (n, MF_LEN (mf)); );
}

static void
Wr_Flush_m (Wr_cl *self)
{
    /* NOP */
    return;
}

static void
Wr_Close_m (Wr_cl *self)
{
    MemFileWr_st       *st      = (MemFileWr_st *) self->st;
    MemFile_st	       *mf      = st->mf;
    bool_t NOCLOBBER	dispose = False;

    USING (MUTEX, &mf->mu,
       {
	   mf->wrs--;
	   dispose = (mf->dying && !mf->rds && !mf->wrs);
	   FREE (st);
       });

    if (dispose)
	MemFile_Dispose_m ((File_clp) &mf->cl);
}

static uint32_t
Wr_Length_m (Wr_cl *self)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;
    NOCLOBBER uint32_t res = 0;

    USING (MUTEX, &mf->mu, res = MF_LEN (mf); );
    return res;
}

static uint32_t
Wr_Index_m (Wr_cl *self)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    return st->index;
}

static bool_t
Wr_Seekable_m (Wr_cl *self)
{
    return True;
}

static bool_t
Wr_Closed_m (Wr_cl *self)
{
    return False;
}

static bool_t
Wr_Buffered_m (Wr_cl *self)
{
    return False;
}

static void
Wr_Lock_m (Wr_cl *self)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    MU_LOCK (&mf->mu);
}

static void
Wr_Unlock_m (Wr_cl *self)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    MU_RELEASE (&mf->mu);
}

static void
Wr_LPutC_m (Wr_cl *self, int8_t ch)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    InlineLPutC (st, mf, ch);
    mf->dirty = True;
}

static void
Wr_LPutStr_m (Wr_cl *self, string_t s)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    while (*s)
	InlineLPutC (st, mf, *s++);
    mf->dirty = True;
}

static void
Wr_LPutChars_m (Wr_cl *self, int8_t *s, uint32_t nb)
{
    MemFileWr_st       *st  = (MemFileWr_st *) self->st;
    MemFile_st	       *mf  = st->mf;

    for (; nb; nb--)
	InlineLPutC (st, mf, *s++);
    mf->dirty = True;
}

static void
Wr_LFlush_m (Wr_cl *self)
{
    /* NOP */
    return;
}


static INLINE void
InlineLPutC (MemFileWr_st *st, MemFile_st *mf, int8_t c)
{
    if (st->index == MF_LEN (mf))
    {
	if (!mf->unused)
	{
	    block_t		b;
	    Stretch_Size 	sz;

	    b.s     = STR_NEW_SALLOC(mf->sa, BLOCK_SIZE);
	    b.bytes = STR_RANGE(b.s, &sz);

	    SEQ_ADDH (&mf->blocks, b);
	    mf->unused = BLOCK_SIZE;
	}
	mf->unused--;
    }

    MF_BLK (mf, st->index) [MF_BYTE (st->index)] = c;
    st->index++;
}


/* 
 * End MemFileMod.c
 */
