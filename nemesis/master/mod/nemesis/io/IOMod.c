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
**      mod/nemesis/io/IOMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Implements IOMod.if
** 
** ENVIRONMENT: 
**
**      User-level library
** 
** ID : $Id: IOMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <links.h>
#include <mutex.h>
#include <time.h>
#include <ecs.h>
#include <stdio.h>

#include <IOMod.ih>

#define FIFO_INLINES
#include "fifo.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}


/* XXX define below to check every put and get are within data area */
/* #define SANITY_CHECK  */


/*
 * Module stuff 
 */

static IOMod_New_fn	New;
static IOMod_op		ms = { New };
static const IOMod_cl	cl = { &ms, NULL };

CL_EXPORT (IOMod, IOMod, cl);


static  IO_PutPkt_fn            PutPkt_m;
static  IO_AwaitFreeSlots_fn    AwaitFreeSlots_m;
static  IO_GetPkt_fn            GetPkt_m;
static  IO_QueryDepth_fn        QueryDepth_m;
static  IO_QueryKind_fn         QueryKind_m;
static  IO_QueryMode_fn         QueryMode_m;
static  IO_QueryShm_fn          QueryShm_m;
static  IO_QueryEndpoints_fn    QueryEndpoints_m;
static  IO_Dispose_fn           Dispose_m;

static  IO_op   IO_ms = {
    PutPkt_m,
    AwaitFreeSlots_m,
    GetPkt_m,
    QueryDepth_m,
    QueryKind_m,
    QueryMode_m,
    QueryShm_m,
    QueryEndpoints_m,
    Dispose_m
};



/* Just the shared pool buffer and the fifos for exchanging control
   over various portions thereof. */

typedef struct {
    word_t        hdl; 
    IO_cl         cl;
    IO_Kind       kind; 
    IO_Mode       mode; 
    FIFO	 *rx_fifo;
    FIFO	 *tx_fifo;
    IOData_Shm   *shm; 
#ifdef SANITY_CHECK
    void         *base; 
    void         *limit; 
#endif
} IOMod_st;


/*
 * Initialisation
 */
FIFO *FIFO_New(IDC_Buffer *buffer, Event_Pair *events, bool_t rx)
{
    FIFO *fifo; 
    
    if(!(fifo = Heap$Malloc(Pvs(heap), sizeof(*fifo))))
	return NULL;

    fifo->buf        = buffer->a; 
    fifo->size       = buffer->w; 

    fifo->tx_event    = events->tx;
    fifo->tx_eventval = EC_READ(events->tx);
    fifo->rx_event    = events->rx;
    
    fifo->num_slots  = fifo->size / sizeof(IO_Rec);
    
    fifo->rsp        = rx ? buffer->a : NULL;
    fifo->tsp        = rx ? NULL : buffer->a; 
    fifo->limit      = fifo->buf + fifo->num_slots;
    
    return fifo;
}




/*-------------------------------------------------------- Entry Points ---*/

static bool_t PutPkt_m (IO_cl *self, uint32_t nrecs, IO_Rec *recs, 
			word_t value, Time_T until)
{
    IOMod_st	*st = (IOMod_st *) self->st;

#ifdef SANITY_CHECK    
    {
	bool_t valid; 
	int i; 

	/* Sanity check that all recs describe mem with our data area */
	valid = True; 
	for(i=0; i < nrecs; i++) {
	    if( recs[i].base < st->base || 
		(recs[i].base + recs[i].len) > st->limit) {
		eprintf("IO$PutPkt: invalid IO_Rec %d (base %p limit %p)\n", 
			i, recs[i].base, recs[i].base + recs[i].len);
		eprintf("but our data area is [%p, %p)\n", st->base, 
			st->limit);
		valid = False;
	    }
	}

	if(!valid)
	    ntsc_dbgstop();
	    
    }
#endif

    return FIFO_Put (st->tx_fifo, nrecs, recs, value, until);  /* put */; 
}

static bool_t AwaitFreeSlots_m (IO_cl *self, uint32_t nslots, Time_T until)
{
    IOMod_st	*st = (IOMod_st *) self->st;

    /* Check how much space there is in the up channel */
    if(FIFO_SlotsFree(st->tx_fifo) < nslots)
	return False;

    return True;
}

static bool_t GetPkt_m (IO_cl *self, uint32_t max_recs, IO_Rec *recs,
			Time_T until, uint32_t *nrecs, word_t *value)
{
    IOMod_st	*st = (IOMod_st *) self->st;
    bool_t res; 

    res = FIFO_Get (st->rx_fifo, max_recs, recs, until, nrecs, value);
    
#ifdef SANITY_CHECK    
    {
	bool_t valid; 
	uint32_t nr; 
	int i; 

	/* Sanity check that all recs describe mem with our data area */
	valid = True; 
	nr    = MIN(max_recs, *nrecs);

	for(i=0; i < nr; i++) {
	    if( recs[i].base < st->base || 
		(recs[i].base + recs[i].len) > st->limit) {
		eprintf("IO$GetPkt: invalid IO_Rec %d (base %p limit %p)\n", 
			i, recs[i].base, recs[i].base + recs[i].len);
		eprintf("but our data area is [%p, %p)\n", st->base, 
			st->limit);
		valid = False;
	    }
	}

	if(!valid)
	    ntsc_dbgstop();
	    
    }
#endif
    return res; 
}

static uint32_t QueryDepth_m (IO_cl *self)
{
    IOMod_st	*st = (IOMod_st *) self->st;
    
    return FIFO_Slots (st->tx_fifo);
}

static uint32_t QueryKind_m (IO_cl *self)
{
    IOMod_st	*st = (IOMod_st *) self->st;
    
    return st->kind; 
}

static uint32_t QueryMode_m (IO_cl *self)
{
    IOMod_st	*st = (IOMod_st *) self->st;
    
    return st->mode; 
}

static IOData_Shm *QueryShm_m(IO_cl *self) 
{
    IOMod_st	*st = (IOMod_st *) self->st;
    
    return st->shm; 
}


static void QueryEndpoints_m(IO_clp self, Channel_Pair *eps) {
    IOMod_st	*st = (IOMod_st *) self->st;

    Channel_EPType eptype;
    eps->tx = Events$QueryEndpoint(Pvs(evs), st->tx_fifo->rx_event, &eptype);
    eps->rx = Events$QueryEndpoint(Pvs(evs), st->rx_fifo->rx_event, &eptype);
}
    

/* 
 * Destruction
 */

static void Dispose_m (IO_cl *self)
{
    IOMod_st	*st = (IOMod_st *) self->st;

    FIFO_Dispose (st->rx_fifo);
    FIFO_Dispose (st->tx_fifo);

    FREE(st->rx_fifo);
    FREE(st->tx_fifo);

    FREE(st); /* self & st are allocated together */
}


/*----------------------------------------------------- Creator Closure ---*/


static IO_clp New (IOMod_cl *self, Heap_clp heap, 
		   IO_Kind kind, IO_Mode mode, 
		   FIFO_cl *rxfifo, FIFO_cl *txfifo, 
		   const word_t hdl, const IOData_Shm *shm)
{
    IOMod_st *st;

    TRC (printf ("IOMod$New: heap=%x bufs=(%x,%x) evs=(%x,%x) data=%x\n",
		 heap, tx_buf, rx_buf, tx_ev, rx_ev, data));
    
    st = Heap$Malloc (heap, sizeof(*st));
    if (!st) RAISE_Heap$NoMemory ();
    
    CL_INIT (st->cl, &IO_ms, st);

    st->hdl  = hdl; 

    st->kind = kind; 
    st->mode = mode; 

    /* XXX SMH: we actually pass in a 'fifo' itself rather than 
       a closure around it so that we can inline things. Nasty. */
    st->rx_fifo = (FIFO *)rxfifo; 
    st->tx_fifo = (FIFO *)txfifo; 

    st->shm = shm;

#ifdef SANITY_CHECK
    st->base   = SEQ_ELEM (shm, 0).buf.base;
    st->limit  = st->base + SEQ_ELEM (shm, 0).buf.len;
#endif


    return &st->cl; 
}



/*
 * End IOMod.c
 */


