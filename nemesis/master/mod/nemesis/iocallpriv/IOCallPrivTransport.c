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
**      /mod/nemesis/IOCallPriv/IOCallPrivTransport.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      An IDCTransport which creates offers for IO's implemented via
**	IOCallPriv.if
** 
** ENVIRONMENT: 
**
**      User-space.
** 
** ID : $Id: IOCallPrivTransport.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <string.h>

#include <IOCallPriv.ih>
#include <IDCClientBinding.ih>
#include <IDCTransport.ih>
#include <IDCCallback.ih>
#include <ProtectionDomain.ih>
#include <IDCOfferMod.ih>
#include <ShmTransport.h>
#include <stdio.h>
#include <exceptions.h>
#include <IDCMacros.h>
#include <links.h>

extern IOCallPriv_clp IOCallPriv;

#ifdef IO_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define DEBUG

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif


/*
 * Module stuff 
 */
static IDCTransport_Offer_fn 	Offer_m;
static IDCTransport_op 		tr_op = { Offer_m };
static const IDCTransport_cl		tr_cl = { &tr_op, NULL };
CL_EXPORT (IDCTransport, IOCallPrivTransport, tr_cl);

/*
 * IDCOffer operations
 */
static IDCOffer_Type_fn		OfferType_m;
static IDCOffer_Bind_fn		OfferBind_m;
static IDCOffer_PDID_fn         OfferPDom_m;
static IDCOffer_GetIORs_fn      OfferGetIORs_m;
static IDCOffer_op		of_op = { 
    OfferType_m, 
    OfferPDom_m,
    OfferGetIORs_m,
    OfferBind_m 
};


/* 
 * Data
 */

typedef struct IOClient_t IOClient_t;
struct IOClient_t {
  IO_clp		io; 		/* IO channel		*/
  Offer_t              *offer;		/* from whence we came	*/
};


/*---Transport methods---------------------------------------------------*/

static IDCOffer_clp Offer_m(IDCTransport_cl *self, 
			    const Type_Any *server,
			    IDCCallback_clp scb,
			    Heap_clp heap,
			    Gatekeeper_clp gk,
			    Entry_clp entry,
			    /* RETURNS */
			    IDCService_clp *service )
{
    Offer_t      *offer;
    
    /* Check that the "server" type really is an IOCallPriv_Info */
    if (! ISTYPE (server, IOCallPriv_Info))
    {
	DB(printf("IOTransport_Offer: server type not an IOCallPriv_Info.\n"));
	RAISE_TypeSystem$Incompatible ();
    }
    
    /* Create the offer */
    if (!(offer = Heap$Malloc(heap, sizeof(*offer)))) {
	DB(printf("IOTransport_Offer: failed to malloc offer!\n"));
	RAISE_Heap$NoMemory();
    }
    CL_INIT(offer->offer_cl,&of_op,offer);
    ANY_COPY(&offer->server,server);
    
    offer->info = NULL; /* The IOClosure_Info is in the "server" field */
    
    /* Fill in the remaining heap and dom fields */
    offer->heap = heap;
    offer->dom  = VP$DomainID (Pvs(vp));
    offer->pdid = VP$ProtDomID (Pvs(vp));
    offer->gk   = gk;
    {
	IDCOfferMod_clp om = NAME_FIND("modules>IDCOfferMod", IDCOfferMod_clp);
	IDCOfferMod$NewLocalIOR(om, &offer->offer_cl, &offer->ior);
    }
    /* List of bindings */
    LINK_INITIALISE(&offer->bindings);
    
    /* Mutex */
    SRCThread$InitMutex(Pvs(srcth),&offer->mu);
    
    /* Register the offer in the object table */
    TRC(fprintf(stderr, "IOTransport_Offer: exporting offer.\n"));
    ObjectTbl$Export(Pvs(objs),
		     &offer->service_cl,
		     &offer->offer_cl,
		     &offer->server );
    
    /* And that's it. */
    *service = NULL;
    return &offer->offer_cl;
}


/*---Offer-methods---------------------------------------------------*/

/*
 * Return the type of an offer
 */
static Type_Code OfferType_m(IDCOffer_cl *self)
{
  return IO_clp__code;
}

static ProtectionDomain_ID OfferPDom_m(IDCOffer_cl *self)
{
    Offer_t *offer = (Offer_t *)(self->st);
    
    return offer->pdid;
}

static IDCOffer_IORList *OfferGetIORs_m(IDCOffer_clp self) {

    Offer_t *offer = self->st;
    IDCOffer_IORList *iorlist = SEQ_NEW(IDCOffer_IORList, 1, Pvs(heap));

    *SEQ_START(iorlist) = offer->ior;

    return iorlist;
}

/*
 * Attempt to bind to a server from the client side.
 */
static IDCClientBinding_clp OfferBind_m(IDCOffer_cl *self,
			    Gatekeeper_clp gk,
			    Type_Any *cl
			    /* RETURNS */ )
{
  Offer_t	*offer = self->st;
  IOClient_t	*st = NULL;
  IOCallPriv_Info info;

  info = NARROW (&offer->server, IOCallPriv_Info);

  TRC(fprintf(stderr, "Binding to IOCallPriv offer %p\n"));

  st = Heap$Malloc(Pvs(heap), sizeof(IOClient_t));

  st->io = IOCallPriv$New (IOCallPriv, info);

  /* Fill in the state */
  TRC (printf ("IO_OfferBind: filling in state.\n"));
  st->offer = offer;

  ANY_INIT(cl, IO_clp, st->io);

  TRC (printf ("IO_OfferBind: done.\n"));
  return NULL; /* XXX will need to create some form of IDCClientBinding */
}

