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
**      /mod/nemesis/io/IOTransport.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      An IDCTransport which creates offers for IO's implemented via
**	IOMod.if.
** 
** ENVIRONMENT: 
**
**      User-space.
** 
** ID : $Id: IOTransport.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <stdio.h>
#include <VPMacros.h>
#include <string.h>
#include <salloc.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <mutex.h>
#include <time.h>
#include <ecs.h>

#include <IOMod.ih>
#include <IOTransport.ih>
#include <IDCCallback.ih>
#include <IDCService.ih>
#include <IDCStubs.ih>
#include <IDCOfferMod.ih>

#include "fifo.h"

extern IOMod_clp IOMod;

/* #define IO_TRACE  */

#ifdef IO_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif


/*
 * Module stuff 
 */
static IOTransport_Offer_fn 	Offer_m;
static IOTransport_op 		tr_op = { Offer_m };
static const IOTransport_cl		tr_cl = { &tr_op, NULL };
CL_EXPORT (IOTransport, IOTransport, tr_cl);

/*
 * IDCOffer operations
 */
static IDCOffer_Type_fn		OfferType_m;
static IDCOffer_GetIORs_fn      OfferGetIORs_m;
static IDCOffer_PDID_fn		OfferPDID_m;
static IDCOffer_Bind_fn		OfferBind_m;
static IOOffer_ExtBind_fn       OfferExtBind_m;
static IOOffer_QueryShm_fn	OfferQueryShm_m;
static IOOffer_op		iof_op = { 
    OfferType_m, 
    OfferPDID_m,
    OfferGetIORs_m,
    OfferBind_m,
    OfferExtBind_m,
    OfferQueryShm_m,
};

/*
 * IDCService operations
 */
static BinderCallback_Request_fn	ServiceBind_m;
static BinderCallback_SimpleRequest_fn	ServiceSimple_m;
static IDCService_Destroy_fn		ServiceDestroy_m;
static IDCService_op sv_op = { 
    ServiceBind_m, 
    ServiceSimple_m, 
    ServiceDestroy_m 
};


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
 * State record of an IO IDCOffer/IDCService
 */
typedef struct Offer_t Offer_t;
struct Offer_t {
    BindingLink_t		bindings;	/* List of bindings	*/
    struct _IOOffer_cl 	        offer_cl;	/* Closures.		*/
    struct _IDCService_cl	service_cl;	
    Type_Any                    server;
    uint32_t                    depth;          /* #slots to be in fifo */
    IO_Mode                     mode;           /* data direction mode  */
    IO_Kind                     alloc;          /* who should alloc mem */ 
    const IOData_Shm           *shm;            /* mem requirements     */
    IDCCallback_clp             iocb;	        /* Server callback      */
    IDCStubs_Info 	        info;		/* Stub information	*/
    Heap_clp 		        heap;		/* Heap to free stuff	*/
    Domain_ID		        dom;		/* Domain id of server	*/
    ProtectionDomain_ID         pdom;		/* Protection domain    */
    Gatekeeper_clp	        gk;		/* Gatekeeper for svr	*/
    IOEntry_clp		        entry;		/* Entry for svr	*/
    word_t                      handle;         /* random server state  */
    mutex_t		        mu;		/* Lock 		*/
};

/*---Transport methods---------------------------------------------------*/

static IOOffer_clp Offer_m(IOTransport_cl	*self, 
			   Heap_clp		 heap,
			   uint32_t              depth, 
			   IO_Mode               mode, 
			   IO_Kind               alloc, 
			   const IOData_Shm     *shm, 
			   Gatekeeper_clp	 gk,
			   IDCCallback_clp       iocb,
			   IOEntry_clp		 entry,
			   word_t                hdl, 
			   /* RETURNS */
			   IDCService_clp       *service )
{
    Offer_t      *offer;

    /* Create the offer */
    if (!(offer = Heap$Malloc(heap, sizeof(*offer)))) {
	DB(eprintf("IOTransport$Offer: failed to malloc offer!\n"));
	RAISE_Heap$NoMemory();
    }
    CL_INIT(offer->offer_cl,&iof_op,offer);
    CL_INIT(offer->service_cl,&sv_op,offer);

    ANY_INIT(&offer->server, IO_clp, NULL);

    offer->depth  = depth; 
    offer->mode   = mode; 
    offer->alloc  = alloc; 
    offer->shm    = shm; 
    offer->iocb   = iocb;
    offer->info   = NULL;	
    offer->heap   = heap;
    offer->dom    = VP$DomainID (Pvs(vp));
    offer->pdom   = VP$ProtDomID (Pvs(vp));
    offer->gk     = gk;
    offer->entry  = entry;
    offer->handle = hdl;
    /* List of bindings */
    LINK_INITIALISE(&offer->bindings);
  
    /* Mutex */
    SRCThread$InitMutex(Pvs(srcth),&offer->mu);
  
    /* Register the offer in the object table */
    TRC(eprintf("IOTransport$Offer: exporting offer.\n"));
    ObjectTbl$Export(Pvs(objs), 
		     &offer->service_cl,
		     (IDCOffer_cl *)&offer->offer_cl, 
		     &offer->server);
  
    /* And that's it. */
    *service = &offer->service_cl;
    return &offer->offer_cl;
}


/*---Offer-methods---------------------------------------------------*/

/*
 * Return the type of an offer
 */
static Type_Code OfferType_m(IDCOffer_cl *self)
{
    Offer_t *offer = (Offer_t *)(self->st);

    return offer->server.type;
}


/*
 * Return the protection domain of an offer
 */
static ProtectionDomain_ID
OfferPDID_m(IDCOffer_cl *self)
{
    Offer_t *offer = (Offer_t *)(self->st);
 
    return offer->pdom;
}

static IDCOffer_IORList *OfferGetIORs_m(IDCOffer_clp self) {

    IDCOfferMod_clp om = NAME_FIND("modules>IDCOfferMod", IDCOfferMod_clp);

    IDCOffer_IORList *iorlist = SEQ_NEW(IDCOffer_IORList, 1, Pvs(heap));

    IDCOfferMod$NewLocalIOR(om, self, SEQ_START(iorlist));

    return iorlist;

}


/*
 * Attempt to bind to a server from the client side.
 */
static IDCClientBinding_clp CommonBind(Offer_t *offer,    Gatekeeper_cl *gk, 
				       const IOData_Shm *shm, Type_Any *cl)
{
    Binder_Cookies clt_cks, svr_cks;
    Binder_Cookie  rx_fifo_buf, tx_fifo_buf;
    FIFO          *rx_fifo, *tx_fifo;
    IOData_Shm const * NOCLOBBER ioshm = shm;
    Binder_Cookie  shm_desc; 
    Event_Pairs	   events;
    Channel_Pairs  chans;
    Heap_cl       *heap;
    Event_Pair	*NOCLOBBER evp = NULL;
    Channel_Pair *NOCLOBBER chp = NULL;
    IO_clp NOCLOBBER io = NULL;
    Stretch_clp NOCLOBBER shmstr = NULL;

    svr_cks.base = NULL;

    /* Get a heap for the tx fifo from the Gatekeeper */
    heap = Gatekeeper$GetHeap(gk, offer->pdom, 0, 
			      SET_ELEM(Stretch_Right_Read), True);

    TRY
    {
	/*
	   There will eventually be 3 areas of shared memory:
	   
	   m0: client's tx_fifo's buf (server's rx_fifo's buf)
	   m1: main buffer (r/w to producer, r/o to consumer)
	   m2: server's tx_fifo's buf (client's rx_fifo's buf)
	   
	   And 2 event pairs:
	   
	   e0: client's tx_fifo's events (server's rx_fifo's events)
	   e1: client's rx_fifo's events (server's tx_fifo's events)
	   
	   m1 is allocated by the producer.  That means us (the client)
	   iff the offer's IO_Info says that the _server_ has IO_Kind_Read.
	   */

	SEQ_CLEAR (SEQ_INIT (&clt_cks, 3, Pvs(heap)));
		 
	/* Get our tx fifo's buffer */
	tx_fifo_buf.w = offer->depth * sizeof (IO_Rec);
	tx_fifo_buf.a = Heap$Malloc (heap, tx_fifo_buf.w);
	if (!tx_fifo_buf.a) RAISE_Heap$NoMemory();
	SEQ_ADDH (&clt_cks, tx_fifo_buf);

	shm_desc.a = NULL;
	shm_desc.w = 0;
	
	if(offer->alloc == IO_Kind_Master) {

	    if(offer->shm != NULL) {
		IOData_Area   *area   = &SEQ_ELEM(offer->shm, 0);
		Stretch_Rights access = 0;

		TRC(eprintf("IOOffer$Bind: allocate %x bytes.\n", 
			    area->buf.len));

		if(ioshm == NULL) {

		    /* Compute the access rights depending on the "mode" of 
		       the channel. Modes are interpreted from the point of 
		       view  of the master (which we are) and so "Rx" mode, 
		       for  example, means that the server requires only 
		       write access to the shared memory. */

		    switch(offer->mode) {
		    case IO_Mode_Rx:
			access = SET_ELEM(Stretch_Right_Write); 
#ifdef __ALPHA__
			/* If we end up having to do an unaligned memcpy 
			   on alpha, we need to be able to read the area
                           we're copying to as well ;-( */
			access |= SET_ELEM(Stretch_Right_Read); 
#endif
			break;
			
		    case IO_Mode_Tx:
			access = SET_ELEM(Stretch_Right_Read); 
			break;
			
		    case IO_Mode_Dx:
			access = SET_ELEM(Stretch_Right_Read) | 
			    SET_ELEM(Stretch_Right_Write); 
			break;
		    }

		    shmstr = Gatekeeper$GetStretch(gk, offer->pdom, 
						   area->buf.len, 
						   access, area->param.awidth, 
						   area->param.pwidth);
		    shm_desc.a = Stretch$Range(shmstr, &shm_desc.w);

		} else {

		    /* We've got some memory already; check it's ok */
		    /* XXX SMH: for now, assume it's fine (urk!)    */
		    shm_desc.a = SEQ_ELEM(ioshm, 0).buf.base; 
		    shm_desc.w = SEQ_ELEM(ioshm, 0).buf.len; 
		} 
		
		
		TRC(eprintf("IOOffer$Bind: got %x bytes at %p\n", 
			    shm_desc.w, shm_desc.a));

	    } else {

		/* No requirements on memory from server; do what we like */

		if(ioshm != NULL) {

		    /* We've got some memory already; its ok since no reqs */
		    shm_desc.a = SEQ_ELEM(ioshm, 0).buf.base; 
		    shm_desc.w = SEQ_ELEM(ioshm, 0).buf.len; 
		    TRC(eprintf("IOOffer$ExtBind: got base=%p, len=%lx\n", 
				shm_desc.a, shm_desc.w));

		} else {

		    /* XXX Here we don't alloc anything. This is the old 
		       per-packet validation mode we used to have */
		    shm_desc.a = (addr_t)0xabbadead; 
		    shm_desc.w = 0xdeadbeef; 

		}

	    }

	    /* Add our 'data area' to the cookies. */
	    SEQ_ADDH (&clt_cks, shm_desc);
	} TRC(eprintf("IO$OfferBind: alloc is slave, so nothing to do.\n"));


	/* Allocate both our fifos' event counts */ 
	SEQ_INIT (&events, 2, Pvs(heap));
	SEQ_INIT (&chans,  2, Pvs(heap));

	for (evp = SEQ_START(&events), chp = SEQ_START(&chans);
	     evp < SEQ_END(&events) && chp < SEQ_END(&chans);
	     evp++, chp++)
	{
	    evp->tx = evp->rx = NULL_EVENT;
	    chp->tx = chp->rx = NULL_EP;
	}

	for (evp = SEQ_START(&events), chp = SEQ_START(&chans);
	     evp < SEQ_END(&events) && chp < SEQ_END(&chans);
	     evp++, chp++)
	{
	    evp->tx = EC_NEW();
	    evp->rx = EC_NEW();

	    chp->tx = Events$AllocChannel (Pvs(evs));
	    chp->rx = Events$AllocChannel (Pvs(evs));
	}

	/* Call the binder */

	TRC(eprintf("IO$OfferBind: calling binder [connect %qx to %qx]\n", 
		    VP$DomainID(Pvs(vp)), offer->dom));
	Binder$ConnectTo (Pvs(bndr),
			  offer->dom,
			  /* cast to word_t to get rid of warning */
			  (Binder_Port)((word_t)&(offer->offer_cl)),
			  &chans,
			  &clt_cks,
			  &svr_cks);

	TRC(eprintf("IO$OfferBind: back from binder.\n"));
	switch (SEQ_LEN (&svr_cks))
	{
	case 2:
	    if (shm_desc.a) 
		RAISE_Binder$Error (Binder_Problem_ServerError);
	    shm_desc = SEQ_ELEM (&svr_cks, 1);
	    /* fall through */
	case 1:
	    rx_fifo_buf = SEQ_ELEM (&svr_cks, 0);
	    break;
	default:
	    RAISE_Binder$Error (Binder_Problem_ServerError);
	}

	for (evp = SEQ_START(&events), chp = SEQ_START(&chans);
	     evp < SEQ_END(&events) && chp < SEQ_END(&chans);
	     evp++, chp++)
	    Events$AttachPair (Pvs(evs), evp, chp);


	rx_fifo = FIFO_NewRx(&rx_fifo_buf, &SEQ_ELEM(&events, 1));
	tx_fifo = FIFO_NewTx(&tx_fifo_buf, &SEQ_ELEM(&events, 0));

	/* 
	** Setup our shm descriptor for IOMod so it knows the data areas 
	**  XXX SMH: we only deal with one area for now.
	*/
	ioshm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
	SEQ_ELEM(ioshm, 0).buf.base = shm_desc.a; 
	SEQ_ELEM(ioshm, 0).buf.len  = shm_desc.w; 
	if(offer->alloc == IO_Kind_Master && offer->shm)
	    SEQ_ELEM(ioshm, 0).param = SEQ_ELEM(offer->shm, 0).param;
			   
	io = IOMod$New (IOMod, Pvs(heap), IO_Kind_Master, offer->mode, 
			(FIFO_cl *)rx_fifo, (FIFO_cl *)tx_fifo, 
			offer->handle, ioshm);

	SEQ_FREE_DATA (&clt_cks);
	SEQ_FREE_DATA (&svr_cks);
	SEQ_FREE_DATA (&events);
    }
    CATCH_ALL
    {
	SEQ_FREE_DATA (&clt_cks);
	SEQ_FREE_DATA (&svr_cks);
	if (tx_fifo_buf.a) FREE (tx_fifo_buf.a);
	if (offer->alloc == IO_Kind_Master && shmstr) STR_DESTROY(shmstr);
	for (evp = SEQ_START(&events); evp < SEQ_END(&events); evp++)
	{
	    if (evp->tx != NULL_EVENT) EC_FREE (evp->tx);
	    if (evp->rx != NULL_EVENT) EC_FREE (evp->rx);
	}
	SEQ_FREE_DATA (&events);
	RERAISE;
    }
    ENDTRY;

    ANY_INIT (cl, IO_clp, io);
    TRC (eprintf("IO$OfferBind: done.\n"));
    return NULL;
}


/*
 * Attempt to bind to a server from the client side.
 */
static IDCClientBinding_clp
OfferBind_m (IDCOffer_cl *self, Gatekeeper_clp gk, Type_Any *cl /* OUT */)
{
    Offer_t *offer = (Offer_t *)(self->st);

    return CommonBind(offer, gk, NULL, cl);
}

static IDCClientBinding_clp
OfferExtBind_m(IOOffer_cl *self, const IOData_Shm *shm, 
	       Gatekeeper_clp gk, Type_Any *cl /* OUT */)
{
    Offer_t *offer = (Offer_t *)(self->st);

    return CommonBind(offer, gk, shm, cl);
}


/* Query the shm provided by the server for this offer. Usually called
 * by clients before doing an ExtBind.  Clients should not modify the
 * shm. */
static IOData_Shm *
OfferQueryShm_m(IOOffer_cl *self)
{
    Offer_t *offer = (Offer_t *)(self->st);

    /* AND: cast to get rid of the const: but ideally, I'd like to 
     * specify the MIDDL to get a const return type. */
    return (IOData_Shm*)offer->shm;
}


/*---Service-methods---------------------------------------------------*/

/*
 * Create a binding: typically called by the object table
 */
static void
ServiceBind_m(BinderCallback_cl		*self,
	      Domain_ID			client /* IN */,
	      ProtectionDomain_ID	pdom /* IN */,
	      Binder_Port		port /* IN */,
	      const Binder_Cookies	*clt_cks /* IN */,
	      Channel_Pairs		*chans /* OUT */,
	      Binder_Cookies		*svr_cks /* OUT */ )
{
    Offer_t	  *offer = self->st;
    FIFO          *rx_fifo, *tx_fifo;
    Binder_Cookie  tx_fifo_buf;
    Binder_Cookie  shm_desc;
    Event_Pair	   tx_events, rx_events;
    Heap_clp	   heap;
    IO_clp NOCLOBBER io = NULL;
    Stretch_cl *NOCLOBBER shmstr = NULL;
    IOData_Shm *ioshm  = NULL;

    TRC(eprintf("IO$Service_Bind: called from dom %qx, pd %p\n", 
		client, pdom));

    /* If the server registered callbacks then call them */
    if (offer->iocb && 
	!IDCCallback$Request(offer->iocb, (IDCOffer_cl *)&offer->offer_cl, 
			     client, pdom, clt_cks, svr_cks)) {
	RAISE_Binder$Error(Binder_Problem_ServerRefused);
    }

    rx_events.tx = rx_events.rx = tx_events.tx = tx_events.rx = NULL_EVENT;

    SEQ_INIT (chans, 2, Pvs(heap));

    TRY
    {
	/* Get a heap for the tx fifo from the Gatekeeper */
	heap = Gatekeeper$GetHeap(offer->gk, pdom, 0, 
				  SET_ELEM(Stretch_Right_Read), True);

	SEQ_CLEAR (SEQ_INIT (svr_cks, 2, Pvs(heap)));

	/* Get our tx fifo's buffer */
	tx_fifo_buf.w = offer->depth * sizeof (IO_Rec);
	tx_fifo_buf.a = Heap$Malloc (heap, tx_fifo_buf.w);
	if (!tx_fifo_buf.a) RAISE_Heap$NoMemory();
	SEQ_ADDH (svr_cks, tx_fifo_buf);

	if(offer->alloc == IO_Kind_Slave) {
	    
	    if (SEQ_LEN (clt_cks) != 1) 
		RAISE_Binder$Error (Binder_Problem_ServerRefused);
	    
	    if(offer->shm != NULL) {
		IOData_Area   *area   = &SEQ_ELEM(offer->shm, 0);
		Stretch_Rights access = 0;
		
		/* Compute the access rights depending on the "mode" of the 
		   channel. Modes are interpreted from the point of view 
		   of the master (we are the slave) and so "Rx" mode, for
		   example, means that the client requires only read 
		   access to the shared memory. */
		switch(offer->mode) {
		case IO_Mode_Rx:
		    access = SET_ELEM(Stretch_Right_Read); 
		    break;
		    
		case IO_Mode_Tx:
		    access = SET_ELEM(Stretch_Right_Write); 
		    break;
		    
		case IO_Mode_Dx:
		    access = SET_ELEM(Stretch_Right_Read) | 
			SET_ELEM(Stretch_Right_Write); 
		    break;
		}

		shmstr = Gatekeeper$GetStretch(offer->gk, pdom, 
					       area->buf.len, access, 
					       area->param.awidth, 
					       area->param.pwidth);
		shm_desc.a = Stretch$Range(shmstr, &shm_desc.w);
		
	    } else {
		
		/* XXX Here we don't alloc anything. This is the old 
		   per-packet validation mode we used to have */
		eprintf("Doing slave 'alloc' (not really though).\n");
		shm_desc.a = (addr_t)0xabbadead; 
		shm_desc.w = 0xdeadbeef; 

	    }
	    
	    /* Add our 'data area' to the cookies. */
	    SEQ_ADDH (svr_cks, shm_desc);
	    
	} else {
	    if (SEQ_LEN (clt_cks) != 2) 
		RAISE_Binder$Error (Binder_Problem_ServerRefused);
	    shm_desc = SEQ_ELEM (clt_cks, 1);
	}

	/* Allocate and attach both our fifos' event counts */ 
	rx_events.tx = EC_NEW(); rx_events.rx = EC_NEW();
	SEQ_ELEM(chans, 0).tx = Events$AllocChannel (Pvs(evs));
	SEQ_ELEM(chans, 0).rx = Events$AllocChannel (Pvs(evs));
	Events$AttachPair (Pvs(evs), &rx_events, &SEQ_ELEM(chans, 0));

	tx_events.tx = EC_NEW(); tx_events.rx = EC_NEW();
	SEQ_ELEM(chans, 1).tx = Events$AllocChannel (Pvs(evs));
	SEQ_ELEM(chans, 1).rx = Events$AllocChannel (Pvs(evs));
	Events$AttachPair (Pvs(evs), &tx_events, &SEQ_ELEM(chans, 1));


	/* Create the IO channel */
	rx_fifo = FIFO_NewRx(&SEQ_ELEM(clt_cks, 0), &rx_events);
	tx_fifo = FIFO_NewTx(&tx_fifo_buf, &tx_events);

	
	/* 
	** Setup our shm descriptor for IOMod so it knows the data areas 
	**  XXX SMH: we only deal with one area for now.
	*/
	ioshm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
	SEQ_ELEM(ioshm, 0).buf.base = shm_desc.a; 
	SEQ_ELEM(ioshm, 0).buf.len  = shm_desc.w; 
	if(offer->alloc == IO_Kind_Slave && offer->shm)
	    SEQ_ELEM(ioshm, 0).param = SEQ_ELEM(offer->shm, 0).param;
	    
	io = IOMod$New (IOMod, Pvs(heap), IO_Kind_Slave, offer->mode, 
			(FIFO_cl *)rx_fifo, (FIFO_cl *)tx_fifo, 
			offer->handle, ioshm);

	/* Register the new binding with the entry.  The entry will schedule
	   on the basis of events from the rx_fifo: these indicate the arrival
	   or return of packets from the far end.  (Events from the tx_fifo's
	   rx channel indicate only the consumption of packets by the far end,
	   not that they have actually been processed.) */

	TRC(eprintf("IOService_Bind: register with entry.\n"));
	if(offer->entry) {
	    IOEntry$Bind (offer->entry, SEQ_ELEM(chans, 0).rx, 
			  SEQ_ELEM(chans, 1).rx, io);
	}

	if (offer->iocb) {
	    ANY_DECL(any, IO_clp, io);
	    if(!IDCCallback$Bound(offer->iocb, (IDCOffer_cl *)&offer->offer_cl,
				  NULL, client, pdom, clt_cks, &any)) {
		RAISE_Binder$Error(Binder_Problem_ServerError);
	    }

	    /* XXX Should we swizzle the server here? That might
	       involve unbinding from the Entry and rebinding the new
	       io - we can't wait to bind to the Entry, since some
	       things rely on it being in the entry when
	       IDCCallback$Bound is called. */
	}
	
    }
    CATCH_ALL
    {
	if (offer->alloc == IO_Kind_Slave && shmstr) STR_DESTROY(shmstr); 
	if (tx_fifo_buf.a) FREE (tx_fifo_buf.a);
	if (io) IO$Dispose (io);
	RERAISE;
    }
    ENDTRY;
  
    TRC(eprintf("IOService_Bind: succeeded.\n"));
}

static void
ServiceSimple_m(BinderCallback_cl 	*self,	
		Domain_ID		client /* IN */,
		ProtectionDomain_ID	pdom /* IN */,
		Binder_Port		port /* IN */,
		const Binder_Cookie	*clt_ck	/* IN */,
		Channel_Pair		*svr_eps /* OUT */,
		Binder_Cookie		*svr_ck	/* OUT */)
{
    DB(eprintf("IOTransport: IDCService$SimpleRequest: not supported.\n"));
    RAISE_Binder$Error( Binder_Problem_Failure );
}

/* 
 * Destroy a service from the server side
 */
static void ServiceDestroy_m(IDCService_cl *self )
{
    Offer_t UNUSED      *offer 	= self->st;

    /* XXX Not yet implemented */
    TRC(eprintf("IO$ServiceDestroy: not implemented.\n"));
}

