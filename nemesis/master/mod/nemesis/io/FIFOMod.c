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
**      mod/nemesis/io/FIFOMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Implements FIFOMod.if
** 
** ENVIRONMENT: 
**
**      User-level library
** 
** ID : $Id: FIFOMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <ecs.h>

#include <FIFOMod.ih>


#define DEBUG

#define TRC(x)

#define ASSERT(cond,fail) {if (!(cond)) { {fail}; while(1); }}

#ifdef DEBUG
#define DB(x) x
#else
#define DB(x) 
#endif

/*
 * Module stuff 
 */

static FIFOMod_New_fn	New;
static FIFOMod_op	ms = { New };
static const FIFOMod_cl	cl = { &ms, NULL };

CL_EXPORT (FIFOMod, FIFOMod, cl);


/*
 * Data structures
 */

typedef struct {
#ifdef DEBUG
    bool_t	used;
#endif
    uint32_t	slot_bytes;
    uint32_t	num_slots;
    
    addr_t	buf;
    addr_t	limit;		/* buf <= sp < limit => sp in a slot	    */
    word_t	size;		/* allocated size of buffer		    */
    Event_Count	tx_event;	/* slots processed at this end		    */
    Event_Count	rx_event;	/* slots processed at other end             */
    
    addr_t	sp;		/* addr of next slot to be read or written  */
    
    /* Invariants:
       
       limit == buf + num_slots * slot_bytes
       
       producer: sp == buf + (tx_event.val % num_slots) * slot_bytes /\
       0 <= tx_event.val - rx_event.val <= num_slots
       
       consumer: sp == buf + (rx_event.val % num_slots) * slot_bytes /\
       0 <= rx_event.val - tx_event.val <= num_slots   */
    
} FIFOMod_st;

/* 
 * Macros
 */


/*
 * Prototypes
 */

static FIFO_Put_fn		Put_m;
static FIFO_Get_fn		Get_m;
static FIFO_GetUntil_fn		GetUntil_m;
static FIFO_Slots_fn		Slots_m;
static FIFO_SlotsFree_fn	SlotsFree_m;
static FIFO_SlotsToGet_fn	SlotsToGet_m;
static FIFO_SlotBytes_fn	SlotBytes_m;
static FIFO_Dispose_fn		Dispose_m;


/*-------------------------------------------------------- Entry Points ---*/



static void
Put_m (FIFO_cl *self, uint32_t nslots, addr_t data, uint32_t val, bool_t block)
{
    FIFOMod_st	*st    = (FIFOMod_st *) self->st;
    uint32_t	 bytes = st->slot_bytes;
    addr_t         sp    = st->sp;
    uint32_t	 i;

#ifdef DEBUG			/* XXX DEBUG */
    if (!st->used)
    {
	if (EC_READ (st->rx_event))
	{
	    eprintf("FIFOMod: Put: event botch");
	    ntsc_dbgstop();
	}
	st->used = True;
    }
#endif

    /* Wait until there are nslots + 1 (for header) free slots:
       there are num_slots - (tx - rx) slots free, so we want
       num_slots - (tx - rx) > nslots + 1,
       ie.
       rx > nslots + 1 + tx - num_slots.
       */
    if (block)
	EC_AWAIT (st->rx_event,
		  EC_READ (st->tx_event) - st->num_slots + nslots + 1);

    /* if not block, caller has already established there is space */

    ((uint32_t *) sp)[0] = nslots;
    ((uint32_t *) sp)[1] = val;

    if ((sp += bytes) == st->limit)
	sp = st->buf;

    for (i = 0; i < nslots; i++)
    {
	TRC (eprintf ("FIFOMod$Put: writing %x bytes from data=%x to sp=%x\n",
		     bytes, data, sp));
	memcpy (sp, data, bytes);
	data += bytes;
	if ((sp += bytes) == st->limit)
	    sp = st->buf;
    }

    st->sp = sp;

    EC_ADVANCE (st->tx_event, nslots + 1);
}

static uint32_t
Get_m(FIFO_cl *self, uint32_t max_slots, addr_t data, bool_t block,
	    /* out */ uint32_t *value)
{
    FIFOMod_st	*st    = (FIFOMod_st *) self->st;
    uint32_t	 bytes = st->slot_bytes;
    addr_t       sp    = st->sp;
    uint32_t	 i;
    Event_Val       val;
    uint32_t	 nslots;

#ifdef DEBUG			/* XXX DEBUG */
    st->used = True;
#endif

    /* make sure there is something new to read */
    val = EC_READ (st->tx_event) + 1;

    if (!block && EC_LT (EC_READ (st->rx_event), val))
	return 0;		/* would block */

    EC_AWAIT (st->rx_event, val);

    /* now there is unacknowledged data in the pipe; how much? */

    nslots = ((uint32_t *) sp)[0];
    *value = ((uint32_t *) sp)[1];

    if (nslots > max_slots)
	return nslots;		/* too much */
 
    if ((sp += bytes) == st->limit)
	sp = st->buf;

    /* now there are nslots more valid slots from which to copy data */

    for (i = 0; i < nslots; i++)
    {
	TRC (eprintf ("FIFOMod$Get: reading %x bytes from sp=%x to data=%x\n",
		     bytes, sp, data));
	memcpy (data, sp, bytes);
	data += bytes;
	if ((sp += bytes) == st->limit)
	    sp = st->buf;
    }

    /* acknowledge what we've read */
    EC_ADVANCE (st->tx_event, nslots + 1);

    st->sp = sp;
    return nslots;
}


static uint32_t
GetUntil_m(FIFO_cl *self, uint32_t max_slots, addr_t data, bool_t block, 
		 Time_T time_out, /* out */  uint32_t *value)
{
    FIFOMod_st    *st    = (FIFOMod_st *) self->st;
    uint32_t   bytes = st->slot_bytes;
    addr_t     sp    = st->sp;
    uint32_t   i;
    Event_Val     val;
    uint32_t   nslots;

#ifdef DEBUG  /* XXX DEBUG */
    st->used = True;
#endif
    
    /* make sure there is something new to read */
    val = EC_READ (st->tx_event) + 1;
    
    if (!block && EC_LT (EC_READ (st->rx_event), val))
	return 0;  /* would block */
    
    EC_AWAIT_UNTIL (st->rx_event, val, time_out);
    if (EC_LT( EC_READ(st->rx_event), val ))
	return 0;
    
    /* now there is unacknowledged data in the pipe; how much? */
    
    nslots = ((uint32_t *) sp)[0];
    *value = ((uint32_t *) sp)[1];
    
    if (nslots > max_slots)
	return nslots; /* too much */
    
    if ((sp += bytes) == st->limit)
	sp = st->buf;
    
    /* now there are nslots more valid slots from which to copy data */
    
    for (i = 0; i < nslots; i++)
    {
	TRC (eprintf ("FIFOMod$Get: reading %x bytes from sp=%x to data=%x\n",
		     bytes, sp, data));
	memcpy (data, sp, bytes);
	data += bytes;
	if ((sp += bytes) == st->limit)
	    sp = st->buf;
    }
    /* acknowledge what we've read */
    EC_ADVANCE (st->tx_event, nslots + 1);
    
    st->sp = sp;
    return nslots;
}


static uint32_t
Slots_m (FIFO_cl *self)
{
    FIFOMod_st   *st   = (FIFOMod_st *) self->st;

    return st->num_slots;
}

static uint32_t
SlotsFree_m (FIFO_cl *self)
{
    /* must only be called by producer */

    FIFOMod_st   *st   = (FIFOMod_st *) self->st;
    Event_Val	diff = EC_READ (st->tx_event) - EC_READ (st->rx_event);

    if (diff < st->num_slots - 1) /* 1 for header */
	return st->num_slots - 1 - diff;
    else
	return 0;
}

static bool_t
SlotsToGet_m (FIFO_cl *self, uint32_t *nslots)
{
    /* must only be called by consumer */
    
    FIFOMod_st	*st    = (FIFOMod_st *) self->st;
    
    if (EC_LT (EC_READ (st->tx_event), EC_READ (st->rx_event)))
    {
	*nslots = *((uint32_t *) (st->sp));
	return True;
    }
    else
    {
	*nslots = 0;
	return False;
    }
}

static uint32_t
SlotBytes_m (FIFO_cl *self)
{
    FIFOMod_st	*st    = (FIFOMod_st *) self->st;

    return st->slot_bytes;
}


/* 
 * Destruction
 */

static void
Dispose_m (FIFO_cl *self)
{
    FIFOMod_st	*st = (FIFOMod_st *) self->st;

    EC_FREE (st->tx_event);
    EC_FREE (st->rx_event);

    /* XXX - TODO: free buf */

    FREE (self);		/* self & st are allocated together */
}


/*----------------------------------------------------- Creator Closure ---*/

static FIFO_op FIFO_ms = {
    Put_m,
    Get_m,
    GetUntil_m,
    Slots_m,
    SlotsFree_m,
    SlotsToGet_m,
    SlotBytes_m,
    Dispose_m
};

static FIFO_clp
New (FIFOMod_cl	*self,
     Heap_clp	heap,
     uint32_t	slot_bytes,
     const 	IDC_Buffer *buf,
     const 	Event_Pair *events)
{
    FIFO_clp		res;
    FIFOMod_st	       *st;

    TRC (eprintf ("FIFOMod$New: heap=%x buf=%x size=%x (tx, rx)=(%x, %x)\n",
		 heap, buf->a, buf->w, events->tx,  events->rx));

    if (slot_bytes < 2 * sizeof (uint32_t) || buf->w < slot_bytes)
	RAISE_FIFOMod$BadSlotSize ();

    if (EC_READ (events->tx) != 0 || EC_READ (events->rx) != 0)
	RAISE_FIFOMod$BadEvents ();

    res = Heap$Malloc (heap, sizeof (*res) + sizeof (*st));
    if (!res) RAISE_Heap$NoMemory ();

    st = (FIFOMod_st *) (res + 1);

#ifdef DEBUG
    st->used = False;
#endif

    st->buf        = buf->a;
    st->size       = buf->w;
    st->tx_event   = events->tx;
    st->rx_event   = events->rx;

    st->slot_bytes = slot_bytes;
    st->num_slots  = st->size / slot_bytes;

    st->sp         = st->buf;
    st->limit      = st->buf + st->num_slots * slot_bytes;

    res->op = &FIFO_ms;
    res->st = st;

    TRC (eprintf ("FIFOMod$New: res=%x\n", res));

    return res;
}

/*
 * End FIFOMod.c
 */


