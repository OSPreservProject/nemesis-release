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
**      WrRedir
** 
** FUNCTIONAL DESCRIPTION:
** 
**      redirectable writer implementation
** 
** ENVIRONMENT: 
**
**      Random module: ie, anywhere you like
** 
** ID : $Id: wr.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>

#include <Heap.ih>
#include <WrRedir.ih>
#include <WrRedirMod.ih>
#include <Wr.ih>

/* State kept by a redirecting writer: just the writer we write to */
typedef struct {
    WrRedir_cl	cl;
    Wr_cl	*wr;
} wr_st;


/* XXX when should the resources for a writer be freed? */

/* ------------------------------------------------------------ */
/* WrRedirMod stuff */

static  WrRedirMod_New_fn               WrRedirMod_New_m;
static   WrRedirMod_op   ms = {
  WrRedirMod_New_m
};

static const WrRedirMod_cl cl = { &ms, NULL };

CL_EXPORT (WrRedirMod, WrRedirMod, cl);


/*************************************************************/
/* This is the redirector writer */
/*************************************************************/
static  Wr_PutC_fn              RedirWr_PutC_m;
static  Wr_PutStr_fn            RedirWr_PutStr_m;
static  Wr_PutChars_fn          RedirWr_PutChars_m;
static  Wr_Seek_fn              RedirWr_Seek_m;
static  Wr_Flush_fn             RedirWr_Flush_m;
static  Wr_Close_fn             RedirWr_Close_m;
static  Wr_Length_fn            RedirWr_Length_m;
static  Wr_Index_fn             RedirWr_Index_m;
static  Wr_Seekable_fn          RedirWr_Seekable_m;
static  Wr_Closed_fn            RedirWr_Closed_m;
static  Wr_Buffered_fn          RedirWr_Buffered_m;
static  Wr_Lock_fn              RedirWr_Lock_m;
static  Wr_Unlock_fn            RedirWr_Unlock_m;
static  Wr_LPutC_fn             RedirWr_LPutC_m;
static  Wr_LPutStr_fn           RedirWr_LPutStr_m;
static  Wr_LPutChars_fn         RedirWr_LPutChars_m;
static  Wr_LFlush_fn            RedirWr_LFlush_m;
static  WrRedir_SetWr_fn        RedirWr_SetWr_m;

static  WrRedir_op   redirwr_ms = {
    RedirWr_PutC_m,
    RedirWr_PutStr_m,
    RedirWr_PutChars_m,
    RedirWr_Seek_m,
    RedirWr_Flush_m,
    RedirWr_Close_m,
    RedirWr_Length_m,
    RedirWr_Index_m,
    RedirWr_Seekable_m,
    RedirWr_Closed_m,
    RedirWr_Buffered_m,
    RedirWr_Lock_m,
    RedirWr_Unlock_m,
    RedirWr_LPutC_m,
    RedirWr_LPutStr_m,
    RedirWr_LPutChars_m,
    RedirWr_LFlush_m,
    RedirWr_SetWr_m
};

/* ------------------------------------------------------------ */
/* Redirector methods */

/* Wr methods *************************************************************/

static void
RedirWr_PutC_m(Wr_cl *self, int8_t ch) {
    wr_st *st = (wr_st *)self->st;
    Wr$PutC(st->wr, ch);
}

static void
RedirWr_PutStr_m(Wr_cl *self, string_t s) {
    wr_st *st = (wr_st *)self->st;
    Wr$PutStr(st->wr, s);
}

static void
RedirWr_PutChars_m(Wr_cl *self, int8_t *s, uint32_t nb) {
    wr_st *st = (wr_st *)self->st;
    Wr$PutChars(st->wr, s, nb);
}

static void
RedirWr_Seek_m(Wr_cl *self, uint32_t n) {
    wr_st *st = (wr_st *)self->st;
    Wr$Seek(st->wr, n);
}

static void
RedirWr_Flush_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    Wr$Flush(st->wr);
}

static void
RedirWr_Close_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    Wr$Close(st->wr);
}

static uint32_t
RedirWr_Length_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    return Wr$Length(st->wr);
}

static uint32_t
RedirWr_Index_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    return Wr$Index(st->wr);
}

static bool_t
RedirWr_Seekable_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    return Wr$Seekable(st->wr);
}

static bool_t
RedirWr_Closed_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    return Wr$Closed(st->wr);
}

static bool_t
RedirWr_Buffered_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    return Wr$Buffered(st->wr);
}

static void
RedirWr_Lock_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    Wr$Lock(st->wr);
}

static void
RedirWr_Unlock_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    Wr$Unlock(st->wr);
}

static void
RedirWr_LPutC_m(Wr_cl *self, int8_t ch) {
    wr_st *st = (wr_st *)self->st;
    Wr$LPutC(st->wr, ch);
}

static void
RedirWr_LPutStr_m(Wr_cl *self, string_t s) {
    wr_st *st = (wr_st *)self->st;
    Wr$LPutStr(st->wr, s);
}

static void
RedirWr_LPutChars_m(Wr_cl *self, int8_t *s, uint32_t nb) {
    wr_st *st = (wr_st *)self->st;
    Wr$LPutChars(st->wr, s, nb);
}

static void
RedirWr_LFlush_m(Wr_cl *self) {
    wr_st *st = (wr_st *)self->st;
    Wr$LFlush(st->wr);
}


static void
RedirWr_SetWr_m(WrRedir_cl *self, Wr_clp wr)
{
    wr_st    *st = (wr_st *)self->st;

/*    printf("RedirWr: changing from %p to %p\n", st->wr, wr);*/
    st->wr = wr;
}

/* ------------------------------------------------------------ */

static WrRedir_clp
WrRedirMod_New_m (WrRedirMod_cl *self, Wr_cl  *wr)
{
    wr_st *st;

    st = Heap$Malloc(Pvs(heap), sizeof(wr_st));
    if (!st)
	RAISE_Heap$NoMemory();

    st->wr = wr;

    /* closure stuff */
    st->cl.op = &redirwr_ms;
    st->cl.st = st;

    return &st->cl;
}
