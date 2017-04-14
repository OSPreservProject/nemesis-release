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
**      pktcx.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      per-packet context
** 
** ENVIRONMENT: 
**
**      Up and down the protocol stack
** 
** ID : $Id: pktcx.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef pktcx_h
#define pktcx_h

#include <IO.ih>
#include <FlowMan.ih>

/* A packet context "pktcx" describes state associated with a packet.
 * It can be considered similar to the meta-data held in BSD mbufs, or
 * in Linux skbufs.
 *
 * A pointer to a pktcx_t can be placed in the "value" field of a
 * packet descriptor in Rbuf operations. */


/* largest number of refresh info recs */
#define RI_MAXRECS 3

/* This structure is around 172 bytes long.  This will get rounded to
 * 256 bytes in a slab allocator.  It can also fit nicely after a 1516
 * byte packet in a 2048 byte buffer (532 bytes avail).  So take care
 * when adding to it. */
typedef struct _pktcx_t {
    uint32_t		error;		/* an error code */

    /* buffer allocation description (refresh info) */
    uint32_t		ri_maxrecs;	/* always RI_MAXRECS for now */
    /* the ri_recs[] array is valid for the Rbuf value of "nrecs" */
    uint32_t		ri_nrecs;
    IO_Rec		ri_recs[RI_MAXRECS];  /* XXX fixed! */

    /* demux info */
    FlowMan_ConnID 	cid;		/* connection this is part of */

    /* packets can be marked by a unique "pktid" so as they are
     * recognisable and can be processed specially.  pktid 0 means
     * ``not marked''. */
    word_t		pktid;

    /* if this packet was formed by merging others, then the "next"
     * entry will list the packet contexts for the others */
    struct _pktcx_t	*next;
} pktcx_t;

#define PKTERR_NONE	0	/* no error; data is valid */
/* ... */

#endif /* pktcx_h */
