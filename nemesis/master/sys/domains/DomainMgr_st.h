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
**      Domain Manager state record
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State kept by the domain manager - lists of domains, etc.
** 
** ENVIRONMENT: 
**
**      Runs as privileged domain.
** 
** ID : $Id: DomainMgr_st.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _DomainMgr_st_h_
#define _DomainMgr_st_h_

/*
 * Definition of DomainMgr state structures
 */

typedef struct _PerDomain_st PerDomain_st;
typedef struct _DomainMgr_st DomainMgr_st;
#define __DomainMgr_STATE PerDomain_st

#include <kernel.h>
#include <stdio.h>


#include <DomainMgrMod.ih>
#include <FramesF.ih>
#include <LongCardTbl.ih>
#include <Security.ih>

#include <ShmTransport.h>

#include <autoconf/binder.h>

#define SECURITY_TAGS_PER_DOMAIN 8

#ifdef CONFIG_BINDER_MUX
/* A "MuxServer_t" is the state record for a demuxing IDCServerStubs. */
typedef struct MuxServer_t MuxServer_t;
struct MuxServer_t {
    IDCServerStubs_State        ss;
    /* Our muxed IDCServerStubs closure */
    IDCServerStubs_cl           cntrl_cl;

    /* State associated with the ShmTransport connection */
    ShmConnection_t            *conn; 

#define MAX_STUBS 8
    word_t                      nstubs;
    IDCServerStubs_clp          stubs[MAX_STUBS];   /* XXX should be a SEQ */
};
#endif /* CONFIG_BINDER_MUX */


/*
 * Actual state structure
 */

/* State record associated with each domain. This is stored in the 	*/
/* binder_ro section of the dcb; the binder_rw section holds the 	*/
/* prefabricated Client_t corresponding to the Server_t in this 	*/
/* structure. 								*/
struct _PerDomain_st {
    struct _DomainMgr_cl dm_cl;         /* Domain manager closure  */
    StretchAllocator_clp salloc;        /* domain's StretchAllocator    */
    IDCService_clp       salloc_service;/* Service for same             */
    IDCCallback_cl       salloc_cb;     /* And controlling callback     */
    IDCServerBinding_clp binding;       /* SAlloc binding for domain    */
    IDCServerBinding_clp parent_binding;/* SAlloc binding for parent    */
    dcb_ro_t	        *rop;		/* Domain read-only             */
    dcb_rw_t	        *rwp;		/* Domain read-write            */ 
    DomainMgr_st        *dm_st;	        /* Domain manager per dom state */
    uint32_t		 refCount;	/* Reference Count              */
    BinderCallback_clp	 callback; 	/* binder calls domain		*/
    IDCClientBinding_clp cb_binding;    /* BinderCallback binding       */
    Binder_cl		 binder;	/* closure 			*/
#ifdef CONFIG_BINDER_MUX
    MuxServer_t          mux;           /* Multiplexing server          */
#endif
    Server_t		 s;		/* Binder server         	*/
    ShmConnection_t      sconn;         /* Server side connection       */
    Client_t		 c;		/* BinderCallback client       	*/
    ShmConnection_t      cconn;         /* BinderCallback connection    */
    Stretch_clp          args;          /* Handle for argument stretch  */
    Stretch_clp          results;       /* Handle for results stretch   */
    IDCOffer_cl 	 offer_cl;	/* Offer for binder		*/
    Domain_ID            parent;        /* Domain which created it      */
    string_t             name;          /* Domain name                  */
    Security_Tag         tags[SECURITY_TAGS_PER_DOMAIN];
    bool_t               inherit[SECURITY_TAGS_PER_DOMAIN];
    Security_cl          security_cl;   /* Security server for this dom */
};

struct _DomainMgr_st {
    Binder_clp		  binder;	/* DM domain's binder closure	*/
    BinderCallback_clp	  bcb;		/* DM domain's callback closure	*/
    IDCCallback_cl        callback;     /* Callbacks for DMgr bindings  */

    IDC_clp               shmidc;	/* Shared Memory IDC */

    StretchAllocatorF_clp saF;		/* For NewClient calls, etc     */
    StretchAllocator_clp  sysalloc;	/* 'Special' salloc for sys mem */
    Stretch_clp		  stretch;	/* This stretch			*/
    Entry_clp             sa_entry;     /* Entry for salloc IDC         */

    LongCardTblMod_clp    lctmclp;      /* For creating new tables      */
    FramesF_clp           framesF;      /* For creating client frames   */
    Heap_clp              sdomheap;     /* For allocating sdoms         */
    MMU_clp               mmuclp;       /* For creating new prot doms   */
    VP_clp                vpclp;        /* Domain access to VP ops      */

    Domain_ID		  next;		/* Next ID to allocate		*/
    dcb_link_t		  list;		/* Linked list of all domains	*/
    LongCardTbl_clp 	  dom_tbl;	/* Maps "ID"s to "dcb_ro_t *"s	*/
    dcb_ro_t     	 *dm_dom;	/* Domain Manager domain	*/
    kernel_st            *kst;		/* Pointer to kernel state      */
    Time_clp              time;	        /* Used to read the time        */
    Security_Tag          next_tag;     /* Next tag to allocate        */
    LongCardTbl_clp       tag_tbl;      /* Maps tags to "dcb_link_t"s  */
    IDCCallback_cl        security_cb;  /* Callbacks for Security bindings */

    uint64_t              ppm;          /* Parts per million of CPU     */

    SRCThread_Mutex 	  mu;		/* Lock on structure	       	*/
    bool_t                initComplete; /* Flag to indicate init done   */
};



/* 
 * Routines to access PerDomain_st structures 
 */

/* Calling IncDomRef guarantees that the rop, rwp and associated state
 * of this domain will not be released until you have called
 * DecDomRef. It may raise Binder.Error if the domain is not found or
 * is in a bad state */

void 	IncDomRef	(DomainMgr_st *st, Binder_ID d, 
			 /* RETURNS */ PerDomain_st **pds );
                         /* RAISES Binder.Error */

/* IncDomRefNoExc behaves exactly as IncDomRef, but communicates its
   result only via a True/False return code, rather than an exception */

bool_t     IncDomRefNoExc (DomainMgr_st *st, Binder_ID d, 
			   /* RETURNS */ PerDomain_st **pds );

/* DecDomRef releases the state of a domain previously retrieves from
   IncDomRef */

void	DecDomRef 	(DomainMgr_st    *st, 
			 PerDomain_st    *pds);

#define ROP_TO_PDS(_rop) ((PerDomain_st *)&((_rop)->binder_ro))

/* 
 * The DM's interface to the Binder */

extern PerDomain_st *Binder_InitDomain(DomainMgr_st *st,
				       dcb_ro_t *rop, 
				       dcb_rw_t *rwp,
				       StretchAllocator_clp salloc);
extern void Binder_InitFirstDomain(DomainMgr_st *st, 
				   dcb_ro_t *rop, dcb_rw_t *rwp);
extern void Binder_Done(DomainMgr_st *st);
extern void Binder_FreeDomain(DomainMgr_st *st,
			      dcb_ro_t *rop, dcb_rw_t *rwp);

#ifdef CONFIG_BINDER_MUX
extern IDCOffer_clp Binder_MuxedOffer(DomainMgr_st *st, dcb_ro_t *rop, 
				      dcb_rw_t *rwp, Type_Any *server);
#endif

/*
 * The DM's interface to the Security service
 */

extern void Security_Init(DomainMgr_st *st);

extern void Security_InitDomain(DomainMgr_st *dm_st,
				PerDomain_st *st,
				StretchAllocator_clp salloc);

extern void Security_FreeDomain(DomainMgr_st *st, PerDomain_st *pds);

/* 
** The DM's interface to architecture- and scheduler-specific 
** domain initialisation, etc.
*/

extern void Arch_InitDomain(DomainMgr_st *st, dcb_ro_t *rop, 
			    dcb_rw_t *rwp, bool_t k);

extern void Scheduler_AddDomain(DomainMgr_st *st, dcb_ro_t *rop,dcb_rw_t *rwp);

extern void Scheduler_RemoveDomain(DomainMgr_st *st, dcb_ro_t *rop, 
				   dcb_rw_t *rwp);


/* some people don't like priv restrictions */
#include <autoconf/nopriv_create.h>

#ifdef CONFIG_NOPRIV_CREATE
#  define PRIV(__foo) (True)
#else
/* XXX SMH: require some way of knowing if a parent is priv or not */
#  if defined(__ALPHA__) || defined(__ARM__)
#    define PRIV(_rop) ((_rop)->psmask == PSMASK_PRIV)
#  elif defined(__IX86__) 
#    define PRIV(_rop) ((_rop)->ps == PSMASK(PS_IO_PRIV))
#  else
#    error You need to define PRIV() for your architecture.
#  endif
#endif

#endif /* _DomainMgr_st_h_ */
