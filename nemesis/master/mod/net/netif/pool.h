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
**      pool.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      C interface for creating a FragPool
** 
** ENVIRONMENT: 
**
**      Internal to Netif
** 
** ID : $Id: pool.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#ifndef pool_h
#define pool_h

#include <PF.ih>
#include <IO.ih>



/* Prototype of function Netif needs to provide to deliver data to a
 * client "rx" */
typedef void (*to_client_fn)(void *netifst,
			     PF_Handle rx, uint32_t nrecs, IO_Rec *recs,
			     uint32_t hdrsz);

/* Prototype of function Netif will eventually provide to be called on
 * fragment buffer release.  Eventually, it should punt the buffers
 * back to the card-specific driver.  For now, it's provided
 * internally by the FragPool for it's own use. */
typedef void (*release_fn)(void *state, uint32_t nrecs, IO_Rec *recs);



/* FragPool creation function */
extern FragPool_cl *fragpool_new(uint32_t mtu,
				 void *netifst, to_client_fn to_client);
/* "netif" is the Netif this fragpool is related to.
 * "to_client" is the function the fragpool should call to deliver data 
 *   to a client "rx" */



#endif /* pool_h */
