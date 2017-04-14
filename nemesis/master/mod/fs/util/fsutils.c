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
**      mod/fs/util
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Readers and writers on top of raw FileIO
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: fsutils.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <exceptions.h>
#include <mutex.h>
#include <time.h>
#include <salloc.h>

#include <FSUtil.ih>
#include <FileIO.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


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
    Wr_Buffered_m,
    Wr_Lock_m,
    Wr_Unlock_m,
    Wr_LPutC_m,
    Wr_LPutStr_m,
    Wr_LPutChars_m,
    Wr_LFlush_m
};

static  FSUtil_GetRd_fn                 FSUtil_GetRd_m;
static  FSUtil_GetWr_fn                 FSUtil_GetWr_m;
static  FSUtil_GetRdWr_fn		FSUtil_GetRdWr_m;

static  FSUtil_op       util_ms = {
    FSUtil_GetRd_m,
    FSUtil_GetWr_m,
    FSUtil_GetRdWr_m
};

static const FSUtil_cl       cl = { &util_ms, NULL };

CL_EXPORT (FSUtil, FSUtil, cl);

/*--------------------------------------------------- State records ----*/

typedef struct {
    Rd_cl rd_cl;
    Wr_cl wr_cl;
    FileIO_clp file;

    bool_t rd;
    bool_t wr;

    SRCThread_Mutex mu;

    uint64_t rd_cur;
    uint64_t wr_cur;
    uint64_t len;
    
    uint32_t blocksize;

    Stretch_clp buffer;

    uint8_t *buf; /* Buffer */
    uint64_t boff; /* Offset of buffer from start of file */
    uint32_t bnum;

    FileIO_Request *req;

    bool_t dirty; /* Does current buffer need flushing? */

    bool_t close; /* Close file when cleaning up? */
} Util_st;

/* Utility functions */

#define min(_a,_b) ((_a)<(_b)?(_a):(_b))
#define max(_a,_b) ((_a)>(_b)?(_a):(_b))

static void getblock(FileIO_clp file, FileIO_Request *req, uint8_t *buf,
		     uint32_t block, uint32_t blocklen)
{
    IO_Rec rec[2];

    uint32_t nrecs;
    word_t   valid;
    
    TRC(printf("fsutils: getblock: %p %p %d %d\n",
	       file, buf, block, blocklen));

    req->blockno   = block;
    req->nblocks   = 1;
    req->op        = FileIO_Op_Read;
    rec[0].base = req;
    rec[0].len  = sizeof(FileIO_Request);
    rec[1].base = buf;
    rec[1].len  = blocklen;

    /* Send read request */
    IO$PutPkt ((IO_clp)file, 2, rec, 2, FOREVER);

    /* Wait for response */
    IO$GetPkt((IO_clp)file, 2, rec, FOREVER, &nrecs, &valid);
}

static void putblock(FileIO_clp file, FileIO_Request *req, uint8_t *buf,
		     uint32_t block, uint32_t blocklen)
{
    IO_Rec rec[2];

    uint32_t nrecs;
    word_t   valid;

    TRC(printf("fsutils: putblock: %p %p %d %d\n",
	       file, buf, block, blocklen));

    req->blockno   = block;
    req->nblocks   = 1;
    req->op        = FileIO_Op_Write;
    rec[0].base = req;
    rec[0].len  = sizeof(FileIO_Request);
    rec[1].base = buf;
    rec[1].len  = blocklen;

    /* Send read request */
    IO$PutPkt ((IO_clp)file, 2, rec, 2, FOREVER);

    /* Wait for response */
    IO$GetPkt((IO_clp)file, 2, rec, FOREVER, &nrecs, &valid);
}

/* Is the current character available? */
#define available(_st,_cur) (((_cur)-(_st)->boff)<(_st)->blocksize)

/* Make the current character available */
#define fetch(_st,_cur) {\
    uint32_t block;\
    block = (_cur) / (_st)->blocksize;\
    (_st)->boff = block * (_st)->blocksize;\
    getblock((_st)->file, (_st)->req, (_st)->buf, block, (_st)->blocksize);\
    (_st)->bnum=block;\
}

static INLINE void flush(Util_st *st)
{
    /* If current block is dirty, flush it */
    if (st->dirty) {
	FileIO$SetLength(st->file, st->len);
	putblock(st->file, st->req, st->buf, st->bnum,
		 st->blocksize);
	st->dirty=False;
    }
}

static INLINE void update(Util_st *_st, uint64_t _cur)
{
    if (!available(_st,_cur))
    {
	flush(_st);
	fetch(_st, _cur);
    }
}

static void delete(Util_st *st)
{
    flush(st);

    if (st->rd || st->wr) return;

    /* Close the file if we're supposed to */
    if (st->close) IO$Dispose((IO_clp)st->file);

    /* Delete the stretch */
    STR_DESTROY(st->buffer);

    /* Release the mutex */
    MU_FREE(&st->mu);

    /* Release memory */
    FREE(st);
}

/*---------------------------------------------------- Entry Points ----*/

static int8_t Rd_GetC_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;
    int8_t ch;

/*
 * IF closed(rd) THEN RAISE Failure(0) END;
 * `Block until "avail(rd) > cur(rd)"`;
 * IF cur(rd) = len(rd) THEN
 *   RAISE EndOfFile
 * ELSE
 *   ch := src(rd)[cur(rd)]; INC(cur(rd)); RETURN ch
 * END
 */

    MU_LOCK(&st->mu);

    if (st->rd_cur >= st->len) {
	MU_RELEASE(&st->mu);
	RAISE_Rd$EndOfFile();
    }

    /* Maybe make a generic 'get me the right one flushing if necessary'? */
    update(st, st->rd_cur);

    /* We have the appropriate block; return a char and advance */
    ch = st->buf[st->rd_cur - st->boff];
    st->rd_cur++;

    MU_RELEASE(&st->mu);
    return ch;

}

static bool_t Rd_EOF_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;
    bool_t res;

    MU_LOCK(&st->mu);

    res = Rd_LEOF_m(self);

    MU_RELEASE(&st->mu);
    return res;
}

static void Rd_UnGetC_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);

    if (st->rd_cur > 0) st->rd_cur--;

    MU_RELEASE(&st->mu);
    return;
}

static uint64_t Rd_CharsReady_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;
    uint64_t res;

    MU_LOCK(&st->mu);
    res = st->len-st->rd_cur;
    MU_RELEASE(&st->mu);
    return res;
}

/* We should probably do a decent implementation of this, because it'll
   be used quite a lot */
static uint64_t Rd_GetChars_m (
    Rd_cl   *self,
    Rd_Buffer       buf     /* IN */,
    uint64_t        nb      /* IN */ )
{
    Util_st *st = self->st;
    uint64_t bytes=0;
    uint32_t avail, len;

    MU_LOCK(&st->mu);

    /* If we're getting near EOF cut down a little */
    if ((st->rd_cur+nb) >= st->len) {
	nb = st->len-st->rd_cur;
    }

    while (nb) {

	/* Is the appropriate block in the buffer? Fetch it if it isn't. */
	update(st,st->rd_cur);

	/* Work out how much we can get from the current block */
	avail = (st->boff+st->blocksize)-st->rd_cur;
	len = min(nb,avail);

	memcpy(buf+bytes,st->buf + (st->rd_cur - st->boff), len);
	st->rd_cur+=len;
	nb-=len;
	bytes+=len;
    }

    MU_RELEASE(&st->mu);

    return bytes;
}

static uint64_t Rd_GetLine_m (
    Rd_cl   *self,
    Rd_Buffer       buf     /* IN */,
    uint64_t        nb      /* IN */ )
{
    Util_st *st = self->st;
    uint64_t bytes=0;
    bool_t finished = False;
    uint32_t avail, len;
    int8_t ch;

    MU_LOCK(&st->mu);

    /* If we're getting near EOF cut down a little */
    if ((st->rd_cur+nb) >= st->len) {
	nb = st->len-st->rd_cur;
    }

    while (nb && !finished) {
	/* Is the appropriate block in the buffer? Fetch it if it isn't. */
	update(st,st->rd_cur);

	/* Work out how much we can get from the current block */
	avail = (st->boff+st->blocksize)-st->rd_cur;
	len = min(nb,avail);

	while (len && !finished) {
	    ch=buf[bytes]=*(st->buf + (st->rd_cur - st->boff));
	    st->rd_cur++; nb--; bytes++; len--;
	    if (ch == '\n') {
		finished=True;
	    }
	}
    }

    MU_RELEASE(&st->mu);
    return bytes;
}

static void Rd_Seek_m (
    Rd_cl   *self,
    uint64_t        nb      /* IN */ )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
    st->rd_cur = min(nb,st->len);
    MU_RELEASE(&st->mu);

}

static void Rd_Close_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;

    st->rd = False;

    delete(st);
}

static uint64_t Rd_Length_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;
    uint64_t res;

    MU_LOCK(&st->mu);
    res = st->len;
    MU_RELEASE(&st->mu);

    return res;
}

static uint64_t Rd_Index_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;
    uint64_t res;
    
    MU_LOCK(&st->mu);
    res=st->rd_cur;
    MU_RELEASE(&st->mu);
    return res;
}

static bool_t Rd_Intermittent_m (
    Rd_cl   *self )
{
    return False;
}

static bool_t Rd_Seekable_m (
    Rd_cl   *self )
{
    return True;
}

static void Rd_Lock_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
}

static void Rd_Unlock_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;

    MU_RELEASE(&st->mu);
}

static int8_t Rd_LGetC_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;
    int8_t ch;

    if (st->rd_cur >= st->len) {
	RAISE_Rd$EndOfFile();
    }

    update(st,st->rd_cur);

    /* We have the appropriate block; return a char and advance */
    ch = st->buf[st->rd_cur - st->boff];
    st->rd_cur++;

    return ch;
}

static bool_t Rd_LEOF_m (
    Rd_cl   *self )
{
    Util_st *st = self->st;

    return (st->rd_cur >= st->len);
}

/*---------------------------------------------------- Entry Points ----*/

static void Wr_PutC_m (
    Wr_cl   *self,
    int8_t  ch      /* IN */ )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
    Wr_LPutC_m(self, ch);
    MU_RELEASE(&st->mu);
}

static void Wr_PutStr_m (
    Wr_cl   *self,
    string_t        s       /* IN */ )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
    Wr_LPutStr_m(self, s);
    MU_RELEASE(&st->mu);
}

static void Wr_PutChars_m (
    Wr_cl   *self,
    Wr_Buffer       s       /* IN */,
    uint64_t        nb      /* IN */ )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
    Wr_LPutChars_m(self, s, nb);
    MU_RELEASE(&st->mu);
}

static void Wr_Seek_m (
    Wr_cl   *self,
    uint64_t        n       /* IN */ )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
    st->wr_cur=min(st->len,n);
    MU_RELEASE(&st->mu);
}

static void Wr_Flush_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
    flush(st);
    MU_RELEASE(&st->mu);
}

static void Wr_Close_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;
    
    st->wr = False;

    delete(st);
}

static uint64_t Wr_Length_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;
    uint64_t res;

    MU_LOCK(&st->mu);
    res=st->len;
    MU_RELEASE(&st->mu);

    return res;
}

static uint64_t Wr_Index_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;
    uint64_t res;

    MU_LOCK(&st->mu);
    res=st->wr_cur;
    MU_RELEASE(&st->mu);

    return res;
}

static bool_t Wr_Seekable_m (
    Wr_cl   *self )
{
    return True;
}

static bool_t Wr_Buffered_m (
    Wr_cl   *self )
{
    return True;
}

static void Wr_Lock_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;

    MU_LOCK(&st->mu);
}

static void Wr_Unlock_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;

    MU_RELEASE(&st->mu);
}

static void Wr_LPutC_m (
    Wr_cl   *self,
    int8_t  ch      /* IN */ )
{
    Util_st *st = self->st;

    update(st,st->wr_cur);

    st->buf[st->wr_cur - st->boff] = ch;
    st->wr_cur++;
    st->dirty=True;

    if (st->wr_cur > st->len) {
	st->len=st->wr_cur;
    }
}

static void Wr_LPutStr_m (
    Wr_cl   *self,
    string_t        s       /* IN */ )
{
    Wr_LPutChars_m(self, s, strlen(s));
}

static void Wr_LPutChars_m (
    Wr_cl   *self,
    Wr_Buffer       s       /* IN */,
    uint64_t        nb      /* IN */ )
{
    Util_st *st = self->st;
    uint32_t len,avail;
    uint64_t bytes=0;

    while (nb) {
	/* Is the appropriate block in the buffer? Fetch it if it isn't. */
	update(st,st->wr_cur);

	/* How much can we fit in this block? */
	avail = (st->boff+st->blocksize)-st->wr_cur;
	len = min(nb,avail);

	memcpy(st->buf + (st->wr_cur - st->boff), s+bytes, len);
	st->wr_cur+=len;
	nb-=len;
	if (st->wr_cur > st->len) {
	    st->len = st->wr_cur;
	}
	bytes+=len;
	st->dirty=True;

    }
}

static void Wr_LFlush_m (
    Wr_cl   *self )
{
    Util_st *st = self->st;

    flush(st);
}

/*---------------------------------------------------- Entry Points ----*/

static Util_st *init(FileIO_clp file, Heap_clp heap, bool_t close)
{
    Util_st *st;
    word_t len;
    
    st = Heap$Malloc(heap, sizeof(*st));

    st->file = file;
    MU_INIT(&st->mu);
    st->rd = False;
    st->wr = False;
    st->rd_cur = 0;
    st->wr_cur = 0;
    st->close = close;
    st->dirty = False;
    
    st->blocksize = FileIO$BlockSize(file);
    st->buffer    = STR_NEW(st->blocksize+sizeof(FileIO_Request));
    st->buf       = STR_RANGE(st->buffer, &len);

    /* XXX assert len >= blocksize */

    /* Fix up permissions so that we can share the buffer with USD */
    STR_SETGLOBAL(st->buffer, SET_ELEM(Stretch_Right_Read) |
		  SET_ELEM(Stretch_Right_Write));
    /* XXX SDE: share this with the USD only */

    st->req = (FileIO_Request *)(st->buf+st->blocksize);

    st->boff = 0;
    st->bnum = 0;

    st->len = FileIO$GetLength(st->file);

    if (st->len>0) {
	getblock(st->file, st->req, st->buf, 0, st->blocksize);
    }

    return st;
}

static Rd_clp FSUtil_GetRd_m (
    FSUtil_cl       *self,
    FileIO_clp      file    /* IN */,
    Heap_clp	    heap    /* IN */,
    bool_t          close   /* IN */ )
{
    Util_st *st;

    st=init(file, heap, close);

    CL_INIT(st->rd_cl, &rd_ms, st);
    st->rd = True;

    return &st->rd_cl;
}

static Wr_clp FSUtil_GetWr_m (
    FSUtil_cl       *self,
    FileIO_clp      file    /* IN */,
    Heap_clp        heap    /* IN */,
    bool_t          close   /* IN */ )
{
    Util_st *st;

    st = init(file, heap, close);

    CL_INIT(st->wr_cl, &wr_ms, st);
    st->wr = True;

    return &st->wr_cl;
}

static Rd_clp FSUtil_GetRdWr_m (
    FSUtil_cl      *self,
    FileIO_clp     file    /* IN */,
    Heap_clp       heap    /* IN */,
    bool_t         close   /* IN */,
    /* RETURNS */
    Wr_clp         *wr )
{
    Util_st *st;

    st = init(file, heap, close);

    CL_INIT(st->wr_cl, &wr_ms, st);
    CL_INIT(st->rd_cl, &rd_ms, st);
    st->rd = True;
    st->wr = True;

    *wr=&st->wr_cl;

    return &st->rd_cl;
}

/*
 * End 
 */
