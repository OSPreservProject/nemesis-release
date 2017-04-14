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
 **      /mod/nemesis/idc/ShmTransport.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Creates offers for IDC using a simple shared memory implementation.
 ** 
 ** ENVIRONMENT: 
 **
 **      User-space.
 ** 
 ** ID : $Id: ShmTransport.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 */

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <ecs.h>
#include <VPMacros.h>
#include <string.h>
#include <stdio.h>
#define NO_TYPE_ANY_OVERRIDE 1
#include <ShmTransport.h>
#include <IDCOfferMod.ih>

/* #define IDC_TRACE */
#ifdef IDC_TRACE
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
static IDCTransport_Offer_fn 	Offer_m;
static IDCTransport_op 		tr_op = { Offer_m };
static const IDCTransport_cl		tr_cl = { &tr_op, NULL };
CL_EXPORT (IDCTransport, ShmTransport, tr_cl);

/*
 * IDCOffer operations
 */
static IDCOffer_Type_fn		OfferType_m;
static IDCOffer_Bind_fn		OfferBind_m;
static IDCOffer_GetIORs_fn      OfferGetIORs_m;
static IDCOffer_PDID_fn		OfferPDID_m;
static IDCOffer_op		of_op = { 
    OfferType_m, 
    OfferPDID_m,
    OfferGetIORs_m,
    OfferBind_m 
};

/* 
 * IDCClientBinding operations
 */
static IDCClientBinding_InitCall_fn	CltInitCall_m;
static IDCClientBinding_InitCast_fn	CltInitCast_m;
static IDCClientBinding_SendCall_fn	CltSendCall_m;
static IDCClientBinding_ReceiveReply_fn CltReceiveReply_m;
static IDCClientBinding_AckReceive_fn   CltAckReceive_m;
static IDCClientBinding_Destroy_fn	CltDestroy_m;

static IDCClientBinding_op	ShmClientBinding_op = {
    CltInitCall_m,
    CltInitCast_m,
    CltSendCall_m,
    CltReceiveReply_m,
    CltAckReceive_m,
    CltDestroy_m
};

/*
 * IDCServerBinding operations
 */
static IDCServerBinding_ReceiveCall_fn	SvrReceiveCall_m;
static IDCServerBinding_AckReceive_fn	SvrAckReceive_m;
static IDCServerBinding_InitReply_fn	SvrInitReply_m;
static IDCServerBinding_InitExcept_fn	SvrInitExcept_m;
static IDCServerBinding_SendReply_fn	SvrSendReply_m;
static IDCServerBinding_Client_fn	SvrClient_m;
static IDCServerBinding_Destroy_fn	SvrDestroy_m;

PUBLIC IDCServerBinding_op	ShmServerBinding_op = {
    SvrReceiveCall_m,
    SvrAckReceive_m,
    SvrInitReply_m,
    SvrInitExcept_m,
    SvrSendReply_m,
    SvrClient_m,
    SvrDestroy_m
};

/*
 * IDCService operations
 */
static BinderCallback_Request_fn	ServiceRequest_m;
static BinderCallback_SimpleRequest_fn	ServiceBind_m;
static IDCService_Destroy_fn		ServiceDestroy_m;
static IDCService_op sv_op = { 
    ServiceRequest_m, 
    ServiceBind_m, 
    ServiceDestroy_m 
};

/*---Transport methods---------------------------------------------------*/

static IDCOffer_clp Offer_m(IDCTransport_cl	*self, 
			    const Type_Any	*server,
			    IDCCallback_clp     scb,
			    Heap_clp		heap,
			    Gatekeeper_clp	gk,
			    Entry_clp		entry,
			    /* RETURNS */
			    IDCService_clp	*service )
{
#define TC_STRING_LEN (2*sizeof(Type_Code))
    char      tname[TC_STRING_LEN+1];
    Offer_t  *offer;

    /* Create the offer */
    if (!(offer = Heap$Malloc(heap, sizeof(*offer)))) {
	DB(eprintf("IDCTransport$Offer: failed to malloc offer!\n"));
	RAISE_Heap$NoMemory();
    }
    LINK_INITIALISE (&offer->bindings);
    CL_INIT (offer->offer_cl,&of_op,offer);
    CL_INIT (offer->service_cl,&sv_op,offer);
    ANY_COPY (&offer->server,server);
    offer->scb   = scb;
    offer->heap  = heap;
    offer->dom   = VP$DomainID (Pvs(vp));
    offer->pdid  = VP$ProtDomID(Pvs(vp));
    offer->gk    = gk;
    offer->entry = entry;
    IDCOfferMod$NewLocalIOR(NAME_FIND("modules>IDCOfferMod", IDCOfferMod_clp),
			    &offer->offer_cl,
			    &offer->ior);

    /* Now look up the stubs in the context and put them into the fields */
    TRY
    {
	Context_clp   stub_ctxt = NAME_FIND (IDC_CONTEXT, Context_clp);

	TRC (eprintf ("IDCTransport$Offer: got stub context.\n"));

	sprintf(tname, "%Q", server->type);
	TRC (eprintf ("IDCTransport$Offer: looking for '%s'.\n", tname));

	offer->info = NAME_LOOKUP (tname, stub_ctxt, IDCStubs_Info);

	/* Mutex */
	MU_INIT (&offer->mu);
    }
    CATCH_ALL
    {
	DB(eprintf(
	    "ShmTransport$Offer: failed to find stub record or get mutex.\n"));

	FREE(offer->ior.transport);
	SEQ_FREE_DATA(&offer->ior.info);
	FREE(offer);
	RERAISE;
    }
    ENDTRY;

    /* Register the offer in the object table */
    TRC(eprintf("IDCTransport$Offer: exporting offer.\n"));
    ObjectTbl$Export (Pvs(objs),
		      &offer->service_cl,
		      &offer->offer_cl,
		      &offer->server );
  
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

    return offer->pdid;
}

static IDCOffer_IORList *OfferGetIORs_m(IDCOffer_clp self) {

    Offer_t *offer = (Offer_t *)(self->st);
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
    Offer_t	    *offer = self->st;
    Client_t	    *st    = NULL;
    _generic_cl	    *client_ops;
    Heap_clp	     heap;
    Binder_Cookie    clt_buf;
    Binder_Cookie    svr_buf;

    /* Get a heap for communication from the Gatekeeper */
    heap = Gatekeeper$GetHeap (gk, offer->pdid, 0, 
			       SET_ELEM(Stretch_Right_Read), True);

    /* Create client state for the stub using the pervasive heap */
    if (!(st = Heap$Calloc(Pvs(heap), 1, sizeof(*st)))) {
	eprintf("IDCOffer$Bind: failed to malloc state.\n");
	RAISE_Heap$NoMemory();
    }

    /* Create connection state for the client */
    if (!(st->conn = Heap$Calloc(Pvs(heap), 1, sizeof(ShmConnection_t)))) {
	eprintf("IDCOffer$Bind: failed to malloc connection state.\n");
	RAISE_Heap$NoMemory();
    }

    TRC(eprintf("IDCOffer$Bind: got state.\n"));

    st->conn->eps.tx = NULL_EP;
    st->conn->eps.rx = NULL_EP;
    st->conn->evs.tx = NULL_EVENT;
    st->conn->evs.rx = NULL_EVENT;
    st->conn->mu.ec  = NULL_EVENT;

    /* Create a shared area of memory using the supplied heap */
    TRC(eprintf("IDCOffer$Bind: getting tx buffer.\n"));
    clt_buf.w = offer->info->c_sz;
    if (!(clt_buf.a = Heap$Malloc (heap, clt_buf.w))) {
	eprintf("IDCOffer$Bind: failed to malloc txbuf.\n");
	FREE (st);
	RAISE_Heap$NoMemory();
    }
  
    /* Fill in the state */
    TRC(eprintf("IDCOffer$Bind: filling in state.\n"));
  
    client_ops = ((_generic_cl *)(offer->info->surr.val))->op;
    CL_INIT       (st->client_cl,    client_ops,          st);
    CL_INIT       (st->binding_cl,  &ShmClientBinding_op, st);
    ANY_INITC     (&st->cs.srgt,  offer->server.type, 
		   (pointerval_t)&st->client_cl);

    st->cs.binding = &st->binding_cl;
    st->cs.marshal = ShmIDC;

    if(offer->info->clnt) {
	/* Let the stubs set up any required run-time state */
	IDCClientStubs$InitState(offer->info->clnt, &st->cs);
    }

    st->conn->call = 0;
    st->offer = offer;

    /* Fill in txbuf descriptor */
    st->conn->txbuf.base  = clt_buf.a;
    st->conn->txbuf.ptr   = clt_buf.a;
    st->conn->txbuf.space = clt_buf.w;
    st->conn->txbuf.heap  = heap;
    st->conn->txsize      = clt_buf.w;

    TRY
    {
	/* init the lock */
	MU_INIT(&(st->conn->mu));

	/* Get event channel end-points */
	st->conn->eps.tx = Events$AllocChannel (Pvs(evs));
	st->conn->eps.rx = Events$AllocChannel (Pvs(evs));

	st->conn->evs.tx = EC_NEW();
	st->conn->evs.rx = EC_NEW();
    
	/* Call the binder */
	TRC(eprintf("IDCOffer$Bind: calling binder.\n"));
	Binder$SimpleConnect (Pvs(bndr),
			      offer->dom,
			      (Binder_Port) (pointerval_t) &(offer->offer_cl),
			      &st->conn->eps,
			      &clt_buf,
			      &svr_buf);

	TRC(eprintf("IDCOffer$Bind: returned from binder.\n"));

	/* Fill in rxbuf descriptor */
	st->conn->rxbuf.base  = svr_buf.a;
	st->conn->rxbuf.ptr   = svr_buf.a;
	st->conn->rxbuf.space = svr_buf.w;
	st->conn->rxbuf.heap  = Pvs(heap);
	st->conn->rxsize      = svr_buf.w;
	if(offer->pdid == 0) 
	    eprintf("Offer$Bind: got rxbuf (from server) at %p\n", svr_buf.a);

	Events$AttachPair (Pvs(evs), &st->conn->evs, &st->conn->eps);
    }
    CATCH_ALL
    {
	eprintf("IDCOffer$Bind: Caught exception %s\n", 
		    exn_ctx.cur_exception);

	if (st->conn->mu.ec != NULL_EVENT) MU_FREE (&st->conn->mu);
    
	FREE (clt_buf.a);

	if (st->conn->evs.tx != NULL_EVENT) EC_FREE (st->conn->evs.tx);
	if (st->conn->evs.rx != NULL_EVENT) EC_FREE (st->conn->evs.rx);

	if (st->conn->eps.tx != NULL_EP) 
	    Events$FreeChannel (Pvs(evs),st->conn->eps.tx);
	if (st->conn->eps.rx != NULL_EP) 
	    Events$FreeChannel (Pvs(evs),st->conn->eps.rx);

	FREE(st);
	RERAISE;
    }
    ENDTRY;

    ANY_COPY (cl, &st->cs.srgt);
    TRC(eprintf("IDCOffer$Bind: done.\n"));
    return &st->binding_cl;
}

/*---Client-methods---------------------------------------------------*/

/*
 * Initialise the buffer for a client call
 */
static IDC_BufferDesc 
CltInitCall_m( IDCClientBinding_cl *self, word_t proc, string_t name )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t          *p;

    TRC(eprintf("IDCClientBinding$InitCall: st=%x ptr=%x proc=%x '%s'\n",
		st, conn->txbuf.base, proc, name));

    /* Lock this channel */
    MU_LOCK(&conn->mu);

    /* Marshal procedure number */
    p = conn->txbuf.base;
    *p = proc;
    conn->txbuf.ptr   = p + 1;
    conn->txbuf.space = conn->txsize - sizeof(*p);
  
    /* return transmit buffer descriptor */
    return &conn->txbuf;
}

/*
 * Initialise the buffer for a client ANNOUNCEMENT
 */
static IDC_BufferDesc 
CltInitCast_m( IDCClientBinding_cl *self, word_t proc, string_t name )
{
    /* Simply do the same as for a call. */
    return CltInitCall_m(self, proc, name );
}

/* 
 * Send a call off
 */
static void 
CltSendCall_m( IDCClientBinding_cl *self, IDC_BufferDesc b )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
  
    TRC(eprintf("IDCClientBinding$SendCall: st=%x evs.tx=%x\n", 
		st, conn->evs.tx));

    /* Kick the event count */
    EC_ADVANCE(conn->evs.tx, 1);
}

/* 
 * Wait for a packet to come back and set up the receive buffer. 
 * The string is not used.
 */
static word_t 
CltReceiveReply_m( IDCClientBinding_cl *self, IDC_BufferDesc *b,
		  string_t *s )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t          *p;

    TRC(eprintf("IDCClientBinding$ReceiveReply: st=%x evs.rx=%x call=%x\n",
		st, conn->evs.rx, conn->call));

    TRY {
	/* Wait for the packet to come back. */
	conn->call = EC_AWAIT( conn->evs.rx, conn->call + 1 );
    } CATCH_Thread$Alerted() {
	/* The server has shut down the binding */
	fprintf(stderr, 
		"IDCClientBinding$ReceiveReply: st = %p : binding closed\n", 
		st);
	
	RAISE_Thread$Alerted();
    } CATCH_ALL {
	fprintf(stderr, 
		"IDCClientBinding$ReceiveReply: st = %p : exception %s\n",
		st, exn_ctx.cur_exception);
	RAISE_IDC$Failure();
    }
    ENDTRY;

    /* Unmarshal reply number */
    p = conn->rxbuf.base;
    conn->rxbuf.ptr   = p + 1;
    conn->rxbuf.space = conn->rxsize - sizeof(*p);
    *b = &conn->rxbuf;

    TRC(eprintf("IDCClientBinding$ReceiveReply: st=%x got reply %x\n",
		st, *p));

    /* ShmTransport doesn't pass exception names */
    if(s) {
	*s = NULL;
    }
    return *p;
}


/*
 * Release the receive buffer.
 */
static void 
CltAckReceive_m( IDCClientBinding_cl *self, IDC_BufferDesc b )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    TRC(eprintf("IDCClientBinding$AckReply: st=%x b=%x\n", st, b));
    MU_RELEASE(&conn->mu);
}


/*
 * Destroy a binding 
 */
static void 
CltDestroy_m( IDCClientBinding_cl *self )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    Binder$Close (Pvs(bndr), conn->eps.rx);
    Binder$Close (Pvs(bndr), conn->eps.tx);

    /* Make sure we're not in the object table.
       XXX - do we really want this here? */
    ObjectTbl$Delete (Pvs(objs), &st->offer->offer_cl);

    MU_FREE(&conn->mu);
    FREE(conn->txbuf.base);
    Heap$Destroy(conn->txbuf.heap);

    EC_FREE (conn->evs.tx);	/* will free attached channel */
    EC_FREE (conn->evs.rx);	/* ditto */

    if(st->offer->info->clnt) {
	IDCClientStubs$DestroyState(st->offer->info->clnt, &st->cs);
    }

    FREE(conn);
    FREE(st);
}

/*---Service-methods---------------------------------------------------*/

/*
 * Create a binding: typically called by the object table; we only
 * deal with BinderCallback_SimpleRequests.
 */
static void
ServiceRequest_m(BinderCallback_cl	*self,
		 Domain_ID		client /* IN */,
		 ProtectionDomain_ID	pdid /* IN */,
		 Binder_Port		port /* IN */,
		 const Binder_Cookies	*clt_cks /* IN */,
		 Channel_Pairs		*chans /* OUT */,
		 Binder_Cookies	        *svr_cks /* OUT */ )
{
    eprintf("IDCService$Request: not supported.\n");
    RAISE_Binder$Error( Binder_Problem_Failure );
}


static void
ServiceBind_m(BinderCallback_cl 	*self,	
	      Domain_ID			client /* IN */,
	      ProtectionDomain_ID	pdid /* IN */,
	      Binder_Port		port /* IN */,
	      const Binder_Cookie	*clt_ck	/* IN */,
	      Channel_Pair		*svr_eps /* OUT */,
	      Binder_Cookie		*svr_ck	/* OUT */)
{
    Offer_t         *offer = self->st;
    Server_t        *st    = NULL;
    Binder_Cookies clt_cks, svr_cks;


    SEQ_INIT_FIXED(&clt_cks, (Binder_Cookie *) clt_ck, 1);
    TRC(eprintf("IDCService$Bind: called from dom %qx : pdid %x\n", 
		client, pdid));
  
    /* If the server registered callbacks then call them */
    if (offer->scb && 
	!IDCCallback$Request(offer->scb, &offer->offer_cl, 
			     client, pdid, 
			     &clt_cks, &svr_cks)) {
	eprintf("Server refusing connection from domain %qx\n", client);
	RAISE_Binder$Error(Binder_Problem_ServerRefused);
    }

    /* Create server state for the stub using the pervasive heap */
    if (!(st = Heap$Calloc (Pvs(heap), 1, sizeof(*st)))) {
	eprintf("IDCOffer$Bind: failed to malloc state.\n");
	RAISE_Heap$NoMemory();
    }

    /* Create server connection state */
    if (!(st->conn = Heap$Calloc (Pvs(heap), 1, sizeof(ShmConnection_t)))) {
	eprintf("IDCOffer$Bind: failed to malloc connection state.\n");
	RAISE_Heap$NoMemory();
    }

    st->conn->mu.ec  = NULL_EVENT;
    st->conn->evs.tx = st->conn->evs.rx = NULL_EVENT;
    st->conn->eps.tx = st->conn->eps.rx = NULL_EP;

    TRY
    {
	st->ss.binding = &st->binding_cl;
	st->ss.marshal = ShmIDC;

	MU_INIT (&st->conn->mu); /* XXX - not actually used on server side */

	st->conn->dom         = client;
	st->conn->pdid        = pdid;

	st->conn->rxbuf.heap  = Pvs(heap);
	st->conn->rxsize      = clt_ck->w;
	st->conn->rxbuf.base  = clt_ck->a;
	st->conn->rxbuf.space = st->conn->rxsize;
	st->conn->rxbuf.ptr   = st->conn->rxbuf.base;

	st->conn->txbuf.heap  = 
	    Gatekeeper$GetHeap (offer->gk, pdid, 0, 
				SET_ELEM(Stretch_Right_Read), True);
	st->conn->txsize      = offer->info->s_sz;
	st->conn->txbuf.base  = Heap$Malloc (st->conn->txbuf.heap, 
					     st->conn->txsize);
	st->conn->txbuf.space = st->conn->txsize;
	st->conn->txbuf.ptr   = st->conn->txbuf.base;
    
	if (!(st->conn->txbuf.base)) RAISE_Heap$NoMemory();
    
	st->conn->call = 0;
    
	st->conn->evs.tx = EC_NEW();
	st->conn->evs.rx = NULL_EVENT; /* we use "Entry" on server side */
 	st->conn->eps.tx = Events$AllocChannel (Pvs(evs));
	st->conn->eps.rx = Events$AllocChannel (Pvs(evs));
	Events$Attach (Pvs(evs), st->conn->evs.tx, 
		       st->conn->eps.tx, Channel_EPType_TX);

	CL_INIT (st->cntrl_cl,    offer->info->stub->op, st);
	CL_INIT (st->binding_cl, &ShmServerBinding_op,   st);

	st->offer = offer;

	/* Register the new binding with the entry */
	Entry$Bind (offer->entry, st->conn->eps.rx, &st->cntrl_cl);


	if (offer->scb) {
	    Type_Any any = offer->server;

	    if(!IDCCallback$Bound(offer->scb, &offer->offer_cl, 
				  &st->binding_cl, client, 
				  pdid, &clt_cks, &any)) {
		RAISE_Binder$Error(Binder_Problem_ServerError);
	    }
	    
	    if(!TypeSystem$IsType(Pvs(types), any.type, offer->server.type)) {
		RAISE_Binder$Error(Binder_Problem_ServerError);
	    }
	    
	    st->ss.service = (addr_t) (pointerval_t) any.val;
	} else {
	    st->ss.service = (addr_t) (pointerval_t) offer->server.val;
	}

	/* Link to the list of bindings */
	st->link.server = st;
	MU_LOCK(&offer->mu);
	LINK_ADD_TO_TAIL(&offer->bindings, &st->link);
	MU_RELEASE(&offer->mu);

    }
    CATCH_ALL
    {
	if (st->conn->txbuf.base) FREE (st->conn->txbuf.base);

	if (st->conn->mu.ec  != NULL_EVENT) MU_FREE (&st->conn->mu);
	if (st->conn->evs.tx != NULL_EVENT) EC_FREE (st->conn->evs.tx);
	if (st->conn->eps.rx != NULL_EP) 
	    Events$FreeChannel (Pvs(evs), st->conn->eps.rx);
	if (st->conn->eps.tx != NULL_EP) 
	    Events$FreeChannel (Pvs(evs), st->conn->eps.tx);
    
	FREE (st);
	eprintf("Error: Refusing connection from domain %qx\n", client);
	RAISE_Binder$Error (Binder_Problem_ServerRefused);
    }
    ENDTRY;

    TRC(eprintf("IDCService$Bind: succeeded.\n"));

    *svr_eps  = st->conn->eps;
    svr_ck->a = st->conn->txbuf.base;
    svr_ck->w = st->conn->txbuf.space;

    return;
}

/* 
 * Destroy an export from the server side.
 */
static void ServiceDestroy_m (IDCService_cl *self )
{

    Offer_t *st = self->st;
    ObjectTbl$Delete(Pvs(objs), &st->offer_cl);
    FREE(st->ior.transport);
    SEQ_FREE_DATA(&st->ior.info);
    FREE(st);
}

/*---Server-methods---------------------------------------------------*/
/*
 * IDCServerBinding operations
 */

/* There's no need to hold st->mu: Entry.if guarantees we're not
   called reentrantly. */

static bool_t
SvrReceiveCall_m (IDCServerBinding_clp	self,
		  /* RETURNS */
		  IDC_BufferDesc	*b,
		  word_t		*op,
		  string_t		*s    )
{
    Server_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    TRC(eprintf("IDCServerBinding$RcvCall: st=%x eps.rx=%x call=%x...\n",
		st, conn->eps.rx, conn->call));

    if (conn->call < VP$Poll (Pvs(vp), conn->eps.rx))
    {
	word_t *p;

	p = conn->rxbuf.base;

	*b  = &conn->rxbuf;

	*op = *p++;

	conn->rxbuf.ptr   = p;
	conn->rxbuf.space = conn->rxsize - sizeof(*p);
	conn->call++;

	TRC(eprintf(" ptr=%x op=%x\n", p, *op));

	return True;
    }
    else
	return False;
}

static void
SvrAckReceive_m (IDCServerBinding_clp self, IDC_BufferDesc b)
{
    /* SKIP - reply will acknowledge call */
}

static IDC_BufferDesc
SvrInitReply_m (IDCServerBinding_clp self)
{
    Server_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t          *p    = conn->txbuf.base;

    TRC(eprintf("IDCServerBinding$InitReply: st=%x\n", st));

    /* No need to wait - arrival of call n+1 acks results of call n
       because of the serialisation on the client side. */

    *p++ = 0;			/* normal return */

    conn->txbuf.ptr = p;
    conn->txbuf.space = conn->txsize - sizeof(*p);

    return &conn->txbuf;
}

static IDC_BufferDesc
SvrInitExcept_m (IDCServerBinding_clp	self,
		 word_t 		exc,
		 string_t		name)
{
    Server_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t          *p    = conn->txbuf.base;

    TRC(eprintf("IDCServerBinding$InitExc: st=%x exc=%x `%s'\n", 
		st, exc, name));

    /* No need to wait - arrival of call n+1 acks results of call n
       because of the serialisation on the client side. */

    *p++ = exc;			/* exceptional return */

    conn->txbuf.ptr = p;
    conn->txbuf.space = conn->txsize - sizeof(*p);

    return &conn->txbuf;
}

static void
SvrSendReply_m (IDCServerBinding_clp self, IDC_BufferDesc b)
{
    Server_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    TRC(eprintf("IDCServerBinding$SendReply: st=%x evs.tx=%x\n",
		st, conn->evs.tx));

    /* Kick the event count */
    EC_ADVANCE( conn->evs.tx, 1 );
}

static Domain_ID SvrClient_m (IDCServerBinding_cl  *self,
			      ProtectionDomain_ID  *pdid)
{
    Server_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    *pdid = conn->pdid;
    return conn->dom;
}

static void
SvrDestroy_m (IDCServerBinding_clp self)
{
    Server_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    TRC(eprintf("IDCServerBinding$Destroy: Destroy binding %p\n", self));

    /* unbind from entry */
    if (st->offer) {
	Entry$Unbind (st->offer->entry, conn->eps.rx);
    }
    
    /* If the server registered callbacks then call them */
    if (st->offer && st->offer->scb) {
	Type_Any any = { st->offer->server.type, (word_t) st->ss.service };
	IDCCallback$Closed(st->offer->scb, &st->offer->offer_cl,
			   self, &any);
    }

    /* The binder is our friend and we let him share our code! 
       The only difference is that bindings to the binder don't use
       malloced buffers and don't maintain a linked list of clients
       */
    FREE(conn->txbuf.base);

    /* We got this heap from the Gatekeeper - it will be reference
       counted */
    Heap$Destroy(conn->txbuf.heap);

    /* remove from offer's list of bindings */
    MU_LOCK(&st->offer->mu);
    LINK_REMOVE (&st->link);
    MU_RELEASE(&st->offer->mu);


    MU_FREE (&conn->mu);		/* XXX - unused so far! */


    /* conn->evs.rx is *not* used any more with entry-style IDC .. */
    
    TRY {
	Binder$Close(Pvs(bndr), conn->eps.rx);
	Events$FreeChannel (Pvs(evs), conn->eps.rx);
    } CATCH_Channel$BadState(UNUSED ep, UNUSED state) {
	eprintf("Channel$BadState on rx free\n");
	ntsc_dbgstop();
    } ENDTRY;

    Events$Free(Pvs(evs), conn->evs.tx);

    FREE(conn);
    FREE(st);
}




