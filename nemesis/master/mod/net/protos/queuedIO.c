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
**      queuedIO.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      IO.if implementation, with packet queues
** 
** ENVIRONMENT: 
**
**      Used by IP implementation (and eventually, TCP)
** 
** ID : $Id: queuedIO.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/


/* The idea is to split out the TX and RX portions of the data path,
 * and then the GetPKt and PutPkt versions of those. */

/* RX side:
 *   o rx_check() is called on receipt of a packet.
 *   o rx_avail() is called when new RX buffers become available.
 *
 * TX side:
 *   o tx_encaps() is called on TX of a packet.
 *   o tx_done() is called on completion of a transmission.
 */


#include <ntsc.h>

/* Gorey packet context manupulation tracing */
#define PCTRC(x)


/* By default, we don't allow a return with nrecs==0 and 
 * a packet context.  You might want to do this to return a pc with
 * an error code in it, though */
#ifdef ALLOW_ERROR_RETURNS
#define ERROR_CHECK()
#else
#define ERROR_CHECK() \
do {						\
    if (nrecs == 0 && *value)			\
	ntsc_dbgstop();				\
} while(0)
#endif



/* RX action codes: */
#define AC_TOCLIENT	0	/* send data towards client */
#define AC_DITCH	1	/* ditch data & recycle buffers */
#define AC_WAIT		2	/* wait for more data to arrive */
#define AC_DELIVER	4	/* send queued data/free buffers to client */






/* this stuff is heavily based on ../fakeIO.c, but is modified to take
 * account of the fact that IP needs a lot tighter intergration with
 * the buffereing policy than other protocol layers.  The others are
 * just filtering layers.  IP requires to be able to delay delivery of
 * some datagrams (eg due to reassembly). */


/* uses the "pc" to re-initialise "recs" and punt them down "io" */
static void recycle(io_st *st, pktcx_t *pc)
{
    pktcx_t *next;
    pktcx_t *t;
    int i;
    IO_Rec *recs;
    int max_recs;

    PCTRC(eprintf("recycle called\n"));

    if (!pc)
    {
	eprintf("No packet context, losing buffers\n");
	return;
    }

    /* find out how many recs we'll need */
    t = pc;
    max_recs = 0;
    while(t)
    {
	max_recs = MAX(max_recs, t->ri_nrecs);
	t = t->next;
    }

    /* need this so we don't have lower layers corrupting the packet context
     * recs */
    recs = alloca(sizeof(IO_Rec) * max_recs);

    while(pc)
    {
	PCTRC(eprintf("pc=%p\n", pc));
	next = pc->next;
	pc->next = NULL;	/* break any chain */
	pc->error = 0;
	pc->cid = 0;
	PCTRC(eprintf("rx into: "));
	for(i=0; i<pc->ri_nrecs; i++)
	{
	    recs[i] = pc->ri_recs[i];
	    PCTRC(eprintf("%p(%d) ", recs[i].base, recs[i].len));
	}
	PCTRC(eprintf("\n"));

	IO$PutPkt(st->io, pc->ri_nrecs, recs, (word_t)pc, FOREVER);

	pc = next;
    }
}



/* Send st->common->rxQ to the client, or return an error if there
 * are not enough client recs */
static int deliver_dgram(io_st *st, uint32_t max_recs,
			 uint32_t *nrecs,
			 IO_Rec *recs,
			 word_t *value)
{
    frag_t *fr;
    frag_t *next;
    pktcx_t *pc;
    int r;
    int i;

    FRTRC(eprintf("deliver_dgram: checking required recs...\n"));

    /* do we have enough client recs? */
    fr = st->common->rxQ;
    r = 0;
    while(fr)
    {
	r += fr->nrecs;
	fr = fr->next;
    }
    if (max_recs < r)
    {
	/* not enough recs available, so return the number needed */
	*nrecs = r;
	*value = 0;
	FRTRC(eprintf("IP: deliver_dgram: not enough recs (got %d, need %d)\n",
		      max_recs, r));
	return AC_TOCLIENT;	    
    }

    FRTRC(eprintf("...need %d recs (got %d)\n", r, max_recs));

    /* junk list shouldn't really have anything on it, though */
    if (st->common->junk)
    {
	pktcx_t *pc = st->common->junk;
	eprintf("deliver_dgram: junk list has stuff on it: ");
	while(pc)
	{
	    eprintf("%p ", pc);
	    pc = pc->next;
	}
    }

    /* ok to proceed, freeing as we go */
    fr = st->common->rxQ;
    r = 0;
    while(fr)
    {
	/* re-write the client recs */
	for(i=0; i<fr->nrecs; i++)
	{
	    recs[r].base = fr->recs[i].base;
	    recs[r].len  = fr->recs[i].len;
	    r++;
	}

	/* put the packet context onto the junk list for later return */
	pc = fr->pc;
	while(pc && pc->next)
	{
	    FRTRC(eprintf("%p\n", pc));
	    pc = pc->next;
	}
	if (pc)
	{
	    pc->next = st->common->junk;
	    st->common->junk = fr->pc;
	}

	/* move onto the next fragment */
	next = fr->next;
	slab_free(st->common->fr_alloc, fr);	    
	fr = next;
    }

    /* return the freshly build pc list, incorporating any junk. */
    *value = (word_t)st->common->junk;
    st->common->junk = NULL;

    /* clear the delivery state */	    
    st->common->rxQ	= NULL;

    *nrecs = r;
    return AC_TOCLIENT;
}



static bool_t PutPkt_m (
    IO_cl      *self,
    uint32_t    nrecs   /* IN */,
    IO_Rec_ptr  recs    /* IN */,
    word_t      value   /* IN */, 
    Time_T      until   /* IN */)
{
    io_st *st = (io_st *) self->st;

    if (st->rxp)
    {
	return IO$PutPkt(st->io, nrecs, recs, value, until); /* send empties */
    }
    else
    {
	/* encaps and transmit */
	return tx_encaps(st, nrecs, recs, value, until);
    }       
}

static bool_t AwaitFreeSlots_m(IO_cl *self, uint32_t nslots, Time_T until)
{
    io_st *st = (io_st *) self->st;

    /* XXX SMH: this is probably not correct, but is never used. 
       If someone is using an Entry on top of a network IO (e.g. 
       for an RPC server), they should fix this. */
    return IO$AwaitFreeSlots(st->io, nslots, until);
}

static bool_t GetPkt_m (
    IO_cl          *self,
    uint32_t        max_recs        /* IN */,
    IO_Rec_ptr      recs    /* IN */,
    Time_T          timeout /* IN */,
    uint32_t        *nrecs  /* OUT */,
    word_t          *value  /* OUT */ )
{
    io_st *st = (io_st *) self->st;
    int action;
    pktcx_t *pc;
    IO_Rec *largerecs;
    IO_Rec *userecs;
    uint32_t usemax_recs;
    int i;
    frag_t *fr;


    if (st->rxp)
    {
	/* get packet and strip headers */
	for(;;)
	{
	    /* if we have any junk recs that need recycling, then send
	     * them back down. */
	    if (st->common->junk)
	    {
		PCTRC(eprintf("GetPktUntil: junk\n"));
		recycle(st, st->common->junk);
		st->common->junk = NULL;
	    }

	    /* if we have a queued packet, then deliver that (along
	     * with any freed buffers we may have). */
	    if (st->common->rxQ)
	    {
		FRTRC(eprintf("pending, so deliver_dgram\n"));
		deliver_dgram(st, max_recs, nrecs, recs, value);
		return True;
	    }

	    /* XXX would quite like to free largerecs, if ! null, but
             * can't */
	    largerecs = NULL;

	    /* get ourselves a fresh packet from layer below us */
	    *value = 0;
	    IO$GetPkt(st->io, max_recs, recs, timeout, nrecs, value);
	    ERROR_CHECK();
	    PCTRC(eprintf("gpu: val=%x, nrecs=%d(%d)\n",
			  *value, *nrecs, max_recs));
	    if (*nrecs == 0) return False; /* timeout */
	    if (*nrecs > max_recs)  /* not enough room */
	    {
		/* can't return unless we know for a fact there's real
		 * data to be picked up, since otherwise people will
		 * think they can go select(); read() and not
		 * block. So we need to read in the data to our
		 * private data buffers, and assess its suitability. */
		usemax_recs = *nrecs;
		largerecs = alloca(sizeof(IO_Rec) * usemax_recs);
		userecs = largerecs;
		*value = 0;
		IO$GetPkt(st->io, usemax_recs, largerecs, timeout, 
			  nrecs, value);
		PCTRC(eprintf("gpu2: val=%x, nrecs=%d(%d)\n",
			      *value, *nrecs, usemax_recs));
		ERROR_CHECK();
		if (!*nrecs) /* timeout */
		{
		    eprintf("queuedIO: double timeout?\n");
		    return False;
		}
	    }
	    else
	    {
		userecs = recs;
		usemax_recs = max_recs;
	    }

	    action = rx_check(st, nrecs, userecs, usemax_recs, value);
	    ERROR_CHECK();
	    pc = *(pktcx_t**)value;

	    switch(action) {
	    case AC_WAIT:
		/* back to the top for more */
		/* fallthrough */
	    case AC_DELIVER:
		/* back to the top to check the "pending" flag */
		/* XXX would quite like to free largerecs, but can't */
		break;

	    case AC_TOCLIENT:
		/* If we have too many recs, then we need to postpone
                 * the delivery. */
		if (*nrecs > max_recs)
		{
		    FRTRC(eprintf("queuedIO: postponing large delivery\n"));
		    if (*nrecs > MAX_RECS)
		    {
			eprintf("queuedIO: too many recs (%d>%d)\n",
				nrecs, MAX_RECS);
			break;
		    }
		    fr = slab_alloc(st->common->fr_alloc);
		    if (!fr)
		    {
			eprintf("queuedIO: out of packet descriptors\n");
			break; /* ditch the packet */
		    }
		    fr->nrecs = *nrecs;
		    for(i=0; i< fr->nrecs; i++)
			fr->recs[i] = userecs[i];
		    fr->pc = pc;
		    fr->next = NULL;
		    if (st->common->rxQ) ntsc_dbgstop();
		    st->common->rxQ = fr;
		    break;
		}

		/* otherwise, we can just return it directly */
		return True;
		break;

	    case AC_DITCH:
		recycle(st, pc);
		break;

	    default:
		eprintf("queuedIO:" __FUNCTION__ ": bad action code %d\n",
			action);
		break;
	    }
	}
    }
    else
    {
	return tx_bufcol(st, max_recs, recs, timeout, nrecs, value);
    }
}


static uint32_t IO_QueryDepth_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    return IO$QueryDepth(st->io);
}

static IO_Kind IO_QueryKind_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    return IO$QueryKind(st->io);
}

static IO_Mode IO_QueryMode_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    return IO$QueryMode(st->io);
}

static IOData_Shm *IO_QueryShm_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    return IO$QueryShm(st->io);
}

static void IO_QueryEndpoints_m(IO_clp self, Channel_Pair *eps) {

    io_st *st = self->st;

    IO$QueryEndpoints(st->io, eps);
}

static void 
IO_Dispose_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    /* XXX more thought needed. Should we take this to mean destroy the
     * whole protocol? */
    IO$Dispose(st->io);
}

