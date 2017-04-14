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
**      RdRedir
** 
** FUNCTIONAL DESCRIPTION:
** 
**      redirectable reader implementation
** 
** ENVIRONMENT: 
**
**      Random module: ie, anywhere you like
** 
** ID : $Id: rd.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>

#include <Heap.ih>
#include <RdRedir.ih>
#include <RdRedirMod.ih>
#include <Rd.ih>

/* State kept by a redirecting reader: just the reader we read from */
typedef struct {
    RdRedir_cl	cl;
    Rd_cl	*rd;
} rd_st;


/* XXX when should the resources for a reader be freed? */

/* ------------------------------------------------------------ */
/* RdRedirMod stuff */

static  RdRedirMod_New_fn               RdRedirMod_New_m;
static   RdRedirMod_op   ms = {
  RdRedirMod_New_m
};

static const RdRedirMod_cl cl = { &ms, NULL };

CL_EXPORT (RdRedirMod, RdRedirMod, cl);


/*************************************************************/
/* This is the redirector reader */
/*************************************************************/
static Rd_GetC_fn	RedirRd_GetC_m;
static Rd_EOF_fn 	RedirRd_EOF_m;
static Rd_UnGetC_fn 	RedirRd_UnGetC_m;
static Rd_CharsReady_fn RedirRd_CharsReady_m;
static Rd_GetChars_fn	RedirRd_GetChars_m;
static Rd_GetLine_fn 	RedirRd_GetLine_m;
static Rd_Seek_fn 	RedirRd_Seek_m;
static Rd_Close_fn 	RedirRd_Close_m;
static Rd_Length_fn 	RedirRd_Length_m;
static Rd_Index_fn 	RedirRd_Index_m;
static Rd_Intermittent_fn RedirRd_Intermittent_m;
static Rd_Seekable_fn 	RedirRd_Seekable_m;
static Rd_Closed_fn	RedirRd_Closed_m;
static Rd_Lock_fn	RedirRd_Lock_m;
static Rd_Unlock_fn 	RedirRd_Unlock_m;
static Rd_LGetC_fn 	RedirRd_LGetC_m;
static Rd_LEOF_fn 	RedirRd_LEOF_m;
static RdRedir_SetRd_fn RedirRd_SetRd_m;
RdRedir_op redirrd_ms = {
    RedirRd_GetC_m,
    RedirRd_EOF_m,
    RedirRd_UnGetC_m,
    RedirRd_CharsReady_m,
    RedirRd_GetChars_m,
    RedirRd_GetLine_m,
    RedirRd_Seek_m,
    RedirRd_Close_m,
    RedirRd_Length_m,
    RedirRd_Index_m,
    RedirRd_Intermittent_m,
    RedirRd_Seekable_m,
    RedirRd_Closed_m,
    RedirRd_Lock_m,
    RedirRd_Unlock_m,
    RedirRd_LGetC_m,
    RedirRd_LEOF_m,
    RedirRd_SetRd_m
};

/* ------------------------------------------------------------ */
/* Redirector methods */

/* Rd methods *************************************************************/

static int8_t
RedirRd_GetC_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$GetC(st->rd);
}

static bool_t
RedirRd_EOF_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$EOF(st->rd);
}

static void
RedirRd_UnGetC_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    Rd$UnGetC(st->rd);
}

static uint32_t
RedirRd_CharsReady_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$CharsReady(st->rd);
}

static uint32_t
RedirRd_GetChars_m(Rd_cl *self, int8_t *buf, uint32_t nb) {
    rd_st *st = (rd_st *)self->st;
    return Rd$GetChars(st->rd, buf, nb);
}

static uint32_t
RedirRd_GetLine_m(Rd_cl *self, int8_t *buf, uint32_t nb) {
    rd_st *st = (rd_st *)self->st;
    return Rd$GetLine(st->rd, buf, nb);
}

static void
RedirRd_Seek_m(Rd_cl *self, uint32_t nb) {
    rd_st *st = (rd_st *)self->st;
    Rd$Seek(st->rd, nb);
}

static void
RedirRd_Close_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    Rd$Close(st->rd);
}

static int32_t
RedirRd_Length_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$Length(st->rd);
}

static uint32_t
RedirRd_Index_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$Index(st->rd);
}

static bool_t
RedirRd_Intermittent_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$Intermittent(st->rd);
}

static bool_t
RedirRd_Seekable_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$Seekable(st->rd);
}

static bool_t
RedirRd_Closed_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$Closed(st->rd);
}

static void
RedirRd_Lock_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    Rd$Lock(st->rd);
}

static void
RedirRd_Unlock_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    Rd$Unlock(st->rd);
}

static int8_t
RedirRd_LGetC_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$LGetC(st->rd);
}

static bool_t
RedirRd_LEOF_m(Rd_cl *self) {
    rd_st *st = (rd_st *)self->st;
    return Rd$LEOF(st->rd);
}


static void
RedirRd_SetRd_m(RdRedir_cl *self, Rd_clp rd)
{
    rd_st    *st = (rd_st *)self->st;

/*    printf("RedirRd: changing from %p to %p\n", st->rd, rd);*/
    st->rd = rd;
}

/* ------------------------------------------------------------ */

static RdRedir_clp
RdRedirMod_New_m (RdRedirMod_cl *self, Rd_cl  *rd)
{
    rd_st *st;

    st = Heap$Malloc(Pvs(heap), sizeof(rd_st));
    if (!st)
	RAISE_Heap$NoMemory();

    st->rd = rd;

    /* closure stuff */
    st->cl.op = &redirrd_ms;
    st->cl.st = st;

    return &st->cl;
}
