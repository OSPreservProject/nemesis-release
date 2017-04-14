/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      fake IO methods
** 
** FUNCTIONAL DESCRIPTION:
** 
**      IO methods common to all protocols, #include'ed by them.
** 
** ENVIRONMENT: 
**
**      Protocol modules
** 
** ID : $Id: fakeIO.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

/* XXX WARNING: This file is #included into multiple other files:
 * don't change without thinking about what you're doing. It is not
 * meant for stand-alone compilation. */

/* You should #define the following before #including this file:
 *   MyIO_PutPkt()
 *   MyIO_GetPkt()
 */

#include <ntsc.h>
#include "fakeIO.h"

/* uses the "pc" to re-initialise "recs" and punt them down "io" */
static void recycle(io_st *st, pktcx_t *pc, IO_Rec *recs)
{
    pktcx_t *next;
    int i;

    eprintf("recycle called\n");

    if (!pc)
    {
	eprintf("No packet context, losing buffers\n");
	ntsc_dbgstop();
	return;
    }

    while(pc)
    {
	eprintf("pc=%p\n", pc);
	next = pc->next;
	pc->next = NULL;	/* break any chain */
	pc->error = 0;
	pc->cid = 0;
	for(i=0; i<pc->ri_nrecs; i++)
	    recs[i] = pc->ri_recs[i];

	MyIO_PutPkt(st, pc->ri_nrecs, recs, (word_t)pc, FOREVER);

	pc = next;
    }
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
	return MyIO_PutPkt(st, nrecs, recs, value, until); /* send empties */
    }
    else
    {
	/* encaps and transmit */
	if (tx_encaps(st, nrecs, recs))
	    return MyIO_PutPkt(st, nrecs, recs, value, until);
	else return False;
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
    IO_cl      *self,
    uint32_t    max_recs  /* IN */,
    IO_Rec_ptr  recs      /* IN */,
    Time_T      timeout   /* IN */,
    uint32_t   *nrecs     /* OUT */, 
    word_t     *value     /* OUT */ )
{
    io_st *st = (io_st *) self->st;
    int action;
    pktcx_t *pc;

    if(st->rxp)
    {
	/* get packet and strip headers */
	do {
	    if(!MyIO_GetPkt(st, max_recs, recs, timeout, nrecs, value))
		return False; /* timed out */
	    if (*nrecs > max_recs) return False; /* not enough room */
	    pc = *(pktcx_t**)value;
	    
	    action = rx_check(st, nrecs, recs, max_recs, pc, value);
	    pc = *(pktcx_t**)value;
	    
	    if (pc && (action == AC_DITCH))
		recycle(st, pc, recs);
	} while(action != AC_TOCLIENT);
	return True;
    }
    else
    {
	return MyIO_GetPkt(st, max_recs, recs, timeout, nrecs, value);
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

/* End of fakeIO.c */
