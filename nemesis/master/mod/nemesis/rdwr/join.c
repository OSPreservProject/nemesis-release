/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      join
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Joins a reader and writer together end to end
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: join.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <Rd.ih>
#include <Wr.ih>
#include <Closure.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
static  Closure_Apply_fn                Closure_Apply_m;

static  Closure_op      ms = {
    Closure_Apply_m
};

typedef struct {
    Closure_cl cl;
    Rd_clp rd;
    Wr_clp wr;
    char *buf;
    uint32_t bufsize;
    bool_t closeWr;
} Closure_st;

/*---------------------------------------------------- Entry Points ----*/

static void Closure_Apply_m (
        Closure_cl      *self )
{
    Closure_st    *st = self->st;
    uint64_t c;

    TRY {
	while (!Rd$EOF(st->rd)) {
	    c=Rd$GetChars(st->rd, st->buf, st->bufsize);
	    Wr$PutChars(st->wr, st->buf, c);
	}
    } FINALLY {
	Rd$Close(st->rd);
	if (st->closeWr) Wr$Close(st->wr);
	FREE(st->buf);
	FREE(st);
    } ENDTRY;
}

Closure_clp CreateJoin(Heap_clp heap, uint32_t bufsize, Rd_clp rd, Wr_clp wr,
		       bool_t closeWr)
{
    Closure_st *st;

    st=Heap$Malloc(heap, sizeof(*st));
    if (!st) {
	RAISE_Heap$NoMemory();
    }

    CL_INIT(st->cl, &ms, st);
    st->rd=rd;
    st->wr=wr;
    st->bufsize=bufsize;

    st->buf=Heap$Malloc(heap, bufsize);
    if (!st->buf) {
	FREE(st);
	RAISE_Heap$NoMemory();
    }

    st->closeWr=closeWr;
    return &st->cl;
}

/*
 * End 
 */
