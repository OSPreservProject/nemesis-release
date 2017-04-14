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
**      mod/nemesis/IO/IOCallPriv.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Implements IOCallPriv.if
** 
** ENVIRONMENT: 
**
**      User-level library
** 
** ID : $Id: IOCallPriv.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <ecs.h>
#include <time.h>
#include <ntsc.h>
#include <exceptions.h>


#include <IOCallPriv.ih>

#define TRC(x)
#define DEBUG(x)
#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}


/*
 * Module stuff 
 */

static IOCallPriv_New_fn	New;
static IOCallPriv_op		ms = { New };
static const IOCallPriv_cl	cl = { &ms, NULL };

CL_EXPORT (IOCallPriv, IOCallPriv, cl);


/*
 * Data structures
 */

/* Just a circular buffer of IO_Recs */

typedef struct {
  IO_Rec       *buf;
  IO_Rec       *limit;
  uint32_t      num_slots;

  Event_Count   slots_in_ev;	/* To keep track of buffer usage */
  IO_Rec       *head;
  Event_Count   slots_out_ev;
  IO_Rec       *tail;

  word_t	callpriv;	/* The ID of the callpriv to use */
  word_t        arg;		/* The 1st arg of the callpriv */
  IO_cl         cl;
} IOCallPriv_st;


/*
 * Prototypes
 */

static IO_PutPkt_fn		PutPkt_m;
static IO_AwaitFreeSlots_fn     AwaitFreeSlots_m;
static IO_GetPkt_fn		GetPkt_m;
static IO_QueryDepth_fn         QueryDepth_m;
static IO_QueryKind_fn          QueryKind_m;
static IO_QueryShm_fn           QueryShm_m;
static IO_QueryEndpoints_fn     QueryEndpoints_m;
static IO_Dispose_fn		Dispose_m;

/*-------------------------------------------------------- Entry Points ---*/

static bool_t PutPkt_m (IO_cl *self, uint32_t nrecs, IO_Rec *recs, 
			word_t value, Time_T until)
{
    IOCallPriv_st	*st = (IOCallPriv_st *) self->st;
    IO_Rec        *sp = st->head;
    IO_Rec        *dp = recs;
    word_t	 i;
    Event_Val   val = EC_READ (st->slots_in_ev) - st->num_slots + nrecs + 1;
/*  printf("Entering PutPkt: rx = %d, tx = %d\n", EC_READ(st->rx_event),
    EC_READ(st->tx_event));*/
    
    if(EC_AWAIT_UNTIL (st->slots_out_ev, val, until) < val) return False;
    
    sp->base = (IO_Rec *)nrecs;
    sp->len  = value;

    if ((++sp) >= st->limit)
	sp = st->buf;
    
    for (i = 0; i < nrecs; i++)  {
	*sp++ = *dp++;
	if (sp >= st->limit)
	    sp = st->buf;
    }
    
    st->head = sp;
    
    ntsc_callpriv(st->callpriv, (word_t)st->arg, (word_t)recs, (word_t)nrecs, 0);
    
    EC_ADVANCE (st->slots_in_ev, nrecs + 1);
    
/*  printf("Leaving PutPkt: rx = %d, tx = %d\n", EC_READ(st->rx_event),
    EC_READ(st->tx_event));*/
    
    return True;
}

static bool_t AwaitFreeSlots_m(IO_clp self, uint32_t nslots, Time_T until) {
    IOCallPriv_st	*st = (IOCallPriv_st *) self->st;
    
    Event_Val val = EC_READ (st->slots_in_ev) - st->num_slots + nslots + 1;

    return EC_AWAIT_UNTIL(st->slots_out_ev, val, until) >= val;
}

static bool_t GetPkt_m (IO_cl *self, uint32_t max_recs, IO_Rec *recs,
			Time_T until, uint32_t *nrecs, word_t *value)
{
    IOCallPriv_st	*st = (IOCallPriv_st *) self->st;
    IO_Rec        *sp = st->tail;
    IO_Rec        *dp = recs;
    word_t	 i;
    Event_Val      val;
    uint32_t numrecs;
    
    val = EC_READ (st->slots_out_ev) + 1;
    
    if(EC_AWAIT_UNTIL (st->slots_in_ev, val, until) < val) {
	*nrecs = 0;
	return False;
    }

    /* now there is unacknowledged data in the pipe; how much? */
    *nrecs = numrecs = (uint32_t)sp->base;
    
    if(numrecs > max_recs) {
	/* too much */
	return False;
    }

    *value      = sp->len;
    
    if ((++sp) >= st->limit)
	sp = st->buf;
    
    /* now there are nrecs more valid slots from which to copy data */
    
    for (i = 0; i < numrecs; i++) {
	*dp++ = *sp++;
	if (sp >= st->limit)
	    sp = st->buf;
    }
    
    /* acknowledge what we've read */
    EC_ADVANCE (st->slots_out_ev, numrecs + 1);
    
    st->tail = sp;
    
    return True;
}

static uint32_t QueryDepth_m(IO_clp self) {
    IOCallPriv_st	*st = (IOCallPriv_st *) self->st;
    
    return st->num_slots;
}

static IO_Kind QueryKind_m(IO_clp self) {
    
    return IO_Kind_Master;
}

static IO_Mode QueryMode_m(IO_clp self) {
    
    return IO_Mode_Tx;

}

static IOData_Shm *QueryShm_m(IO_clp self) {
    
    ntsc_dbgstop();
}

static void QueryEndpoints_m(IO_clp self, Channel_Pair *eps) {

    eps->tx = NULL_EP;
    eps->rx = NULL_EP;
}

/* 
 * Destruction
 */

static void
Dispose_m (IO_cl *self)
{
    IOCallPriv_st *st = (IOCallPriv_st *) self->st;
    
    EC_FREE(st->slots_in_ev);
    EC_FREE(st->slots_out_ev);
    FREE(st->buf);
    FREE(st); 
}


/*----------------------------------------------------- Creator Closure ---*/

static IO_op IO_ms = {
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

static IO_clp New (IOCallPriv_cl	*self,
		   const IOCallPriv_Info   info)
{
  IOCallPriv_st     *st;

  st = Heap$Malloc(Pvs(heap), sizeof(IOCallPriv_st));
  if(!st) RAISE_Heap$NoMemory();

  st->buf        = Heap$Malloc (Pvs(heap), info->slots * sizeof(IO_Rec));
  if (!st->buf) 
  {
      FREE(st);
      RAISE_Heap$NoMemory ();
  }

  st->limit      = st->buf + info->slots;
  st->num_slots  = info->slots;

  st->slots_in_ev   = EC_NEW();	/* LOCAL event counts */
  st->slots_out_ev  = EC_NEW();
  st->head          = st->buf;
  st->tail          = st->buf;

  st->callpriv   = info->callpriv;
  st->arg        = info->arg;

  st->cl.op = &IO_ms;
  st->cl.st = st;

  return &st->cl;
}



/*
 * End IOCallPriv.c
 */


