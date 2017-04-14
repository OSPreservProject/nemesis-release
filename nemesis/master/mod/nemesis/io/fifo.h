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
**      mod/nemesis/io/fifo.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Inline FIFO stuff. 
** 
** ENVIRONMENT: 
**
**      User-land.
** 
** ID : $Id: fifo.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <IO.ih>

/*
 * Data structures
 */

typedef struct {

    uint32_t	num_slots;

    IO_Rec	*buf;
    IO_Rec	*limit;		/* buf <= sp < limit => sp in a slot	    */
    IO_Rec	*rsp;		/* addr of next slot to be read, or NULL    */
    IO_Rec	*tsp;		/* addr of next slot to be written, or NULL */

    word_t	size;		/* allocated size of buffer		    */

    Event_Count	tx_event;	/* slots processed at this end		    */
    Event_Val   tx_eventval;    /* local copy of tx_event.val               */
    Event_Count	rx_event;	/* slots processed at other end             */


  /* Invariants:

       limit == buf + num_slots * slot_bytes

       producer: sp == buf + (tx_event.val % num_slots) * slot_bytes /\
                 0 <= tx_event.val - rx_event.val <= num_slots

       consumer: sp == buf + (rx_event.val % num_slots) * slot_bytes /\
                 0 <= rx_event.val - tx_event.val <= num_slots   */

} FIFO;


extern FIFO *FIFO_New(IDC_Buffer *, Event_Pair *, bool_t rx);
#define FIFO_NewRx(_idcbuf, _evpair) FIFO_New(_idcbuf, _evpair, True)
#define FIFO_NewTx(_idcbuf, _evpair) FIFO_New(_idcbuf, _evpair, False)


#ifdef FIFO_INLINES

static INLINE bool_t FIFO_Put (FIFO *fifo, uint32_t nslots, addr_t data, 
			       word_t value, Time_T until)
{
    IO_Rec   *sp = fifo->tsp;
    IO_Rec   *dp = (IO_Rec *)data;

    bool_t    block = IN_FUTURE(until);

    Event_Val val, ec_val; 
    uint32_t  i;

    if(!sp) RAISE_IO$Failure();

    /* 
    ** We need  nslots + 1 (for header) free slots:
    ** there are num_slots - (tx - rx) slots free, so we want
    **     num_slots - (tx - rx) > nslots + 1; 
    **  => rx > nslots + 1 + tx - num_slots.
    */
    val = fifo->tx_eventval - fifo->num_slots + nslots + 1;

    if (block) {
	/* The ec_val we get returned here isn't necessarily the
           accurate value - however, the real value is at least as
           high as the returned value */

	ec_val = EC_AWAIT_UNTIL(fifo->rx_event, val, until);
    } else {
	ec_val = EC_READ(fifo->rx_event);
    }
 
   if(EC_LT(ec_val, val)) 
	return False;

    /* ok, now there is space */
    sp->base = (addr_t)(word_t)nslots;
    sp->len  = value;

    if ((++sp) >= fifo->limit)
	sp = fifo->buf;

    for (i = 0; i < nslots; i++)
    {
	*sp++ = *dp++;
	if (sp >= fifo->limit)
	    sp = fifo->buf;
    }

    fifo->tsp = sp;

    EC_ADVANCE (fifo->tx_event, nslots + 1);
    fifo->tx_eventval += nslots+1;

    return True;
}


static INLINE bool_t 
FIFO_Get(FIFO *fifo, uint32_t max_slots, addr_t data, Time_T until, 
	 /* OUT */ uint32_t *nrecs, /* OUT */  word_t *value)
{
    IO_Rec    *sp = fifo->rsp;
    IO_Rec    *dp = (IO_Rec *) data;
    uint32_t   i;
    Event_Val  val, ec_val;
    uint32_t   nslots;
    
    bool_t     block = IN_FUTURE(until);
    
    if(!sp) RAISE_IO$Failure();

    /* make sure there is something new to read */
    val = fifo->tx_eventval + 1;

    if(block) 
    {
	/* The ec_val we get returned here isn't necessarily the
           accurate value - however, the real value is at least as
           high as the returned value */

	ec_val = EC_AWAIT_UNTIL(fifo->rx_event, val, until);
    } else {
	ec_val = EC_READ(fifo->rx_event);
    }

    if (EC_LT (ec_val, val)) {
	*nrecs = 0; 
	return False;  /* would block */
    }


    /* 
    ** Now there is unacknowledged data in the pipe; how much? 
    ** (there must always be at least 1 record) 
    */
    if((nslots = (uint32_t)(word_t)sp->base) == 0)
	RAISE_IO$Failure();
    
    if (nslots > max_slots) {
	*nrecs = nslots; 
	return False; 
    }


    /* since we're not going to fail, fill in the value field */
    *value = sp->len;    
    
    if (++sp >= (fifo->limit))
	sp = fifo->buf;
    
    /* now there are nslots more valid slots from which to copy data */
    for (i = 0; i < nslots; i++)
    {
	*dp++ = *sp++;
	if (sp >= fifo->limit)
	    sp = fifo->buf;
    }
    /* acknowledge what we've read */
    EC_ADVANCE (fifo->tx_event, nslots + 1);
    fifo->tx_eventval += nslots + 1;
    
    fifo->rsp = sp;
    *nrecs    = nslots; 

    return True;
}


static INLINE uint32_t FIFO_SlotsFree (FIFO *fifo)
{
    Event_Val diff;

    /* must only be called by producer */
    if(!fifo->tsp) RAISE_IO$Failure();
    
    diff = fifo->tx_eventval - EC_READ (fifo->rx_event);
    
    if (diff < fifo->num_slots)
	return fifo->num_slots - diff;
    else return 0;
}

static INLINE bool_t FIFO_SlotsToGet (FIFO *fifo, uint32_t *nslots)
{
    /* must only be called by consumer */
    if(!fifo->rsp) RAISE_IO$Failure();
    
    if (EC_LT (fifo->tx_eventval, EC_READ (fifo->rx_event))) {
	*nslots = (uint32_t)(word_t)fifo->rsp->base;
	return True;
    } else {
	*nslots = 0;
	return False;
    }
}

static INLINE uint32_t FIFO_Slots (FIFO *fifo)
{
    return fifo->num_slots;
}


/* 
 * Destruction
 */

static INLINE void FIFO_Dispose (FIFO *fifo)
{
    EC_FREE (fifo->rx_event);
    EC_FREE (fifo->tx_event);
}

#endif /* FIFO_INLINES */

