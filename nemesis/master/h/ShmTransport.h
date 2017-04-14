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
**      ShmTransport.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State types for shared memory IDC transport.
** 
** ENVIRONMENT: 
**
**      Included by stubs and lib/nemesis/IDC/ShmTransport.c
** 
** ID : $Id: ShmTransport.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _SmhTransport_h_
#define _SmhTransport_h_

#include <mutex.h>

#include <IDC.ih>
#include <IDCTransport.ih>
#include <IDCStubs.ih>
#include <IDCCallback.ih>
#include <IDCClientStubs.ih>
#include <IDCServerStubs.ih>
#include <IDCOfferMod.ih>
#include <Entry.ih>

extern IDC_cl			ShmIDC_cl;
extern IDC_clp			ShmIDC;
extern IDCServerBinding_op	ShmServerBinding_op; /* for the Binder */
extern IDCClientBinding_op	ShmClientBinding_op; /* for the Binder */



/*
 * Name of context for finding IDC stub records
 */
#define IDC_CONTEXT "stubs"

/*
 * SAP for standard Nemesis same-machine IDCOffers 
 */

#define SAP_CONVERTER_NAME "ShmSAPConverter"

/*
 * Link structure for bindings 
 */
typedef struct BindingLink_t BindingLink_t;
struct BindingLink_t {
  BindingLink_t *next;
  BindingLink_t *prev;
  addr_t	 server; /* points to Server_t or IOServer_t */
};

/*
 * Generic closure for client and server states 
 */
typedef struct _generic_cl _generic_cl;
struct _generic_cl {
  addr_t	op;
  addr_t	st;
};



/*
 * State record of an IDCOffer/IDCService
 */
typedef struct Offer_t Offer_t;
struct Offer_t {
  BindingLink_t		bindings;	/* List of bindings	*/
  struct _IDCOffer_cl 	offer_cl;	/* Closures.		*/
  struct _IDCService_cl	service_cl;	
  Type_Any 		server;		/* Service we export	*/
  IDCCallback_clp       scb;	        /* Server callbacks     */
  IDCStubs_Info 	info;		/* Stub information	*/
  Heap_clp 		heap;		/* Heap to free stuff	*/
  Domain_ID		dom;		/* Domain id of server	*/
  ProtectionDomain_ID   pdid;		/* Protection domain    */
  Gatekeeper_clp	gk;		/* Gatekeeper for svr	*/
  Entry_clp		entry;		/* Entry for svr	*/
  mutex_t		mu;		/* Lock 		*/
  IDCOffer_IOR          ior;            /* Our local IOR        */ 
};

/*
 * Internal structures
 */

typedef struct ShmConnection_t ShmConnection_t;
struct ShmConnection_t {

    /* 
    ** A ShmConnection_t represents one half of a ShmTransport 'connection'
    ** i.e. it encapsulates a pair of bound channels and endpoints, plus
    ** their associated transmit and receive buffers. Both the client and 
    ** server ends of a connection will use one of these.
    */

    mutex_t		 mu;		/* Locking for connection       */
    
    IDC_BufferRec 	 txbuf;		/* Transmit buffer descriptor	*/
    IDC_BufferRec	 rxbuf;		/* Receive ditto		*/
    
    Heap_Size		 txsize;	/* Size of tx buffer 		*/
    Heap_Size		 rxsize;	/* Size of rx buffer		*/

    Event_Val		 call;		/* Number of this call		*/
    Event_Pair		 evs;		/* Event counts			*/
    Channel_Pair	 eps;		/* End-points			*/

    Domain_ID            dom;	        /* Domain ID of the peer domain */
    ProtectionDomain_ID  pdid;          /* PDom of the peer domain      */
};


typedef struct Client_t Client_t;
struct Client_t {

    /* By convention the state pointer of a client surrogate closure
       points to the following "State" record: */

    IDCClientStubs_State	cs; 	        /* Client state		*/

    /* Actual closures referred to from "cs" above */
    struct _generic_cl          client_cl;	/* client surrogate	*/
    struct _IDCClientBinding_cl binding_cl;     /* client binding	*/

    /* State associated with the ShmTransport connection */
    ShmConnection_t            *conn; 
  
    Offer_t                    *offer;		/* Offer this came from	*/
};

/*
 * A "Server_t" is the state record for an IDCServerStubs closure. The
 * IDCServerStubs_State record is therefore the first item in this record.
 */
typedef struct Server_t Server_t;
struct Server_t {

    IDCServerStubs_State	ss;		/* Server state		*/

    /* Actual closures referred to from "ss" above */
    struct _IDCServerStubs_cl   cntrl_cl;
    struct _IDCServerBinding_cl binding_cl;

    /* State associated with the ShmTransport connection */
    ShmConnection_t            *conn; 

    /* Link for lists of bindings */
    BindingLink_t               link;

    Offer_t                    *offer;
};


/*
 * Gate definitions for state structures
 */
#define STATE_MU_Lock(_st)   SRCThread$Lock(Pvs(srcth),&(_st)->mu)
#define STATE_MU_Unlock(_st) SRCThread$Release(Pvs(srcth),&(_st)->mu)


#endif /* _SmhTransport_h_ */

