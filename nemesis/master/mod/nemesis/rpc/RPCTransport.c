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
 **      /mod/nemesis/rpc/RPCTransport.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Creates offers for RPC using a simple UDP implementation.
 ** 
 ** ENVIRONMENT: 
 **
 **      User-space.
 ** 
 ** ID : $Id: RPCTransport.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 */

#include "RPCTransport.h"

#include <StubGen.ih>

#define TIMEOUTVAL SECONDS(10)

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#if 1
#define DB(_x) _x
#else
#define DB(_x)
#endif

#define BASE_PORT 700

/* Wire format structures */

#define RPC_REQUEST_MSG 1
#define RPC_REPLY_MSG   2

#define ALIGN64(_p) (addr_t)(  (((word_t)(_p)) + 7) & -8)

typedef struct _RPCMsgHeader {
    uint64_t msgType;
    union {
	uint64_t binding;
	uint64_t success;
    } u;
    uint64_t transId;
} RPCMsgHeader;

typedef struct _RPCIORInfo {
    FlowMan_SAP sap;
    uint64_t    offerId;
} RPCIORInfo;

/*
 * Module stuff 
 */

static RPCMod_New_fn            RPCMod_New_m;
static RPCMod_op                rpcmod_op = { RPCMod_New_m };
static const RPCMod_cl rpcmod_cl = { &rpcmod_op, NULL };
CL_EXPORT(RPCMod, RPCMod, rpcmod_cl);

static IDCTransport_Offer_fn 	         RPCOffer_m;
static RPCTransport_ForgeOffer_fn        RPCForge_m;
static RPCTransport_MakeServerBinding_fn RPCMakeServer_m;
static RPCTransport_MakeClientBinding_fn RPCMakeClient_m;

static RPCTransport_op 		tr_op = { 
    RPCOffer_m, 
    RPCForge_m,
    RPCMakeServer_m,
    RPCMakeClient_m
};

/*
 * IDCOffer operations
 */
static IDCOffer_Type_fn		RPCOfferType_m, RPCSurrOfferType_m;
static IDCOffer_Bind_fn		RPCOfferBind_m, RPCSurrOfferBind_m;
static IDCOffer_GetIORs_fn      RPCOfferGetIORs_m, RPCSurrOfferGetIORs_m;
static IDCOffer_PDID_fn		RPCOfferPDom_m, RPCSurrOfferPDom_m;

static IDCOffer_op		offer_op = { 
    RPCOfferType_m, 
    RPCOfferPDom_m,
    RPCOfferGetIORs_m,
    RPCOfferBind_m 
};

static IDCOffer_op              surrogate_offer_op = {
    RPCSurrOfferType_m,
    RPCSurrOfferPDom_m,
    RPCSurrOfferGetIORs_m,
    RPCSurrOfferBind_m 
};

/* 
 * IDCClientBinding operations
 */
static IDCClientBinding_InitCall_fn	RPCCltInitCall_m;
static IDCClientBinding_InitCast_fn	RPCCltInitCast_m;
static IDCClientBinding_SendCall_fn	RPCCltSendCall_m;
static IDCClientBinding_ReceiveReply_fn RPCCltReceiveReply_m;
static IDCClientBinding_AckReceive_fn   RPCCltAckReceive_m;
static IDCClientBinding_Destroy_fn	RPCCltDestroy_m;

static IDCClientBinding_op	RPCClientBinding_op = {
    RPCCltInitCall_m,
    RPCCltInitCast_m,
    RPCCltSendCall_m,
    RPCCltReceiveReply_m,
    RPCCltAckReceive_m,
    RPCCltDestroy_m
};

/*
 * IDCServerBinding operations
 */
static IDCServerBinding_ReceiveCall_fn	RPCSvrReceiveCall_m;
static IDCServerBinding_AckReceive_fn	RPCSvrAckReceive_m;
static IDCServerBinding_InitReply_fn	RPCSvrInitReply_m;
static IDCServerBinding_InitExcept_fn	RPCSvrInitExcept_m;
static IDCServerBinding_SendReply_fn	RPCSvrSendReply_m;
static IDCServerBinding_Client_fn	RPCSvrClient_m;
static IDCServerBinding_Destroy_fn	RPCSvrDestroy_m;

static IDCServerBinding_op	RPCServerBinding_op = {
    RPCSvrReceiveCall_m,
    RPCSvrAckReceive_m,
    RPCSvrInitReply_m,
    RPCSvrInitExcept_m,
    RPCSvrSendReply_m,
    RPCSvrClient_m,
    RPCSvrDestroy_m
};

/*
 * Bouncer operations
 */

static Bouncer_BindTo_fn          Bouncer_BindTo_m;
static Bouncer_Close_fn           Bouncer_Close_m;

static Bouncer_op  bouncer_op = {
    Bouncer_BindTo_m,
    Bouncer_Close_m
};

/*
 * IORConverter operations 
 */

static IORConverter_Localise_fn   RPCLocalise_m;

static IORConverter_op converter_op = {
    RPCLocalise_m
};

/*
 * IDCServerStubs operations
 */

static IDCServerStubs_Dispatch_fn RPCDispatch_m;

static IDCServerStubs_op serverstubs_op = {
    RPCDispatch_m
};

/*
 * TimeNotify operations
 */

static TimeNotify_Notify_fn RPCTimeout;

static TimeNotify_op timeout_op = {
    RPCTimeout
};


static Bouncer_clp GetBouncer(RPC_st *st, const FlowMan_SAP *sap);
static SurrRPCOffer_st  *GetSurrogateOffer(RPC_st *st, 
					   const FlowMan_SAP *sap,
					   uint64_t offerId,
					   Type_Code tc,
					   const IDCOffer_IORList *iorlist);

static IDCStubs_Info GetStubs(RPC_st *st, Type_Code tc);
static Client_st *MakeClientBinding(RPCClientMUX_st *st, Type_Code tc, 
				    const FlowMan_SAP *sap,
				    uint64_t binding);

static Server_st *MakeServerBinding(RPCServerMUX_st *st, addr_t service,
				    const FlowMan_SAP *sap,
				    IDCStubs_Info info);

static void DestroyClientBinding(RPCClientMUX_st *st, Client_st *client);
static void DestroyServerBinding(RPCServerMUX_st *st, Server_st *server);

static void ReleaseClientRXBuffer(RPCComm_st *st, IO_Rec *recs);
static void ReleaseServerRXBuffer(RPCComm_st *st, IO_Rec *recs);

static void GetTXBuffer(RPCComm_st *comm, IO_Rec *rec, bool_t lock);

static RPCServerMUX_st *NewServerMUX(RPC_st *st, const FlowMan_SAP *sap, 
				     Entry_clp entry);

static RPCClientMUX_st *NewClientMUX(RPC_st *st);

static void RPCClientRXThread(RPCClientMUX_st *st);
static void RPCTXReapThread(RPCComm_st *st);

static void SetTarget(RPCComm_st *st, const FlowMan_SAP *sap);
static void SendRequest(RPCComm_st *st, IO_Rec *rec, const FlowMan_SAP *sap);

static BlockedRequest *GetBlockReq(Client_st *st, RPCClientMUX_st *mux);
static void ReleaseBlockReq(RPCClientMUX_st *mux, BlockedRequest *req);

/*---RPCMod methods------------------------------------------------------*/

static RPCTransport_clp RPCMod_New_m(RPCMod_clp self,  FlowMan_SAP *sap) {

    RPC_st *st;
    char buf[128];


    LongCardTblMod_clp tblMod = 
	NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);


    st = Heap$Malloc(Pvs(heap), sizeof(*st));

    if(!st) RAISE_RPCMod$Failure(RPCMod_Problem_NoResources);

    memset(st, 0, sizeof(*st));

    /* For now, only one server and one client stack ... */
    st->serverMUX = NewServerMUX(st, sap, Pvs(entry));
    st->clientMUX = NewClientMUX(st);
    

    CL_INIT(st->transport_cl,    &tr_op, st);
    CL_INIT(st->bouncer_cl,      &bouncer_op, st);
    CL_INIT(st->iorconverter_cl, &converter_op, st);
    CL_INIT(st->idc_cl,          &RPCMarshal_ms, st);

    {
	FlowMan_IPSAP *ipsap = &st->serverMUX->comm.localSap.u.UDP;
	sprintf(buf, "NemesisRPC:UDP:%08X.%04X", ipsap->addr.a, ipsap->port);
    }

    st->iorconverter_name = strdup(buf);

    sprintf(buf, "transports>%s", st->iorconverter_name);

    TRC(printf("RPCMod$New: Exporting '%s'\n", buf));

    CX_ADD(buf, &st->iorconverter_cl, IORConverter_clp);

    TRC(printf("RPCMod$New: Exporting 'transports>NemesisRPC:UDP'\n"));

    TRY {
	CX_ADD("transports>NemesisRPC:UDP", 
		  &st->iorconverter_cl, IORConverter_clp);
    } CATCH_Context$Exists() {
    } ENDTRY;

    TRC(printf("RPCMod$New: Building tables\n"));

    st->bouncerTbl = LongCardTblMod$New(tblMod, Pvs(heap));
    MU_INIT(&st->bouncerTblMu);


    st->remoteOfferTbl = LongCardTblMod$New(tblMod, Pvs(heap));
    MU_INIT(&st->remoteOfferTblMu);


    st->om = NAME_FIND("modules>IDCOfferMod", IDCOfferMod_clp);

    st->stubgen = NAME_FIND("sys>StubGen", StubGen_clp);

    st->offerCode = StubGen$GenType(st->stubgen, IDCOffer_IORList__code,
				    (IDCMarshalCtl_clp) &RPCMarshalCtl, 
				    Pvs(heap));
				    
    TRC(printf("RPCMod$New: Creating bouncer\n"));

    /* Create the stateless binding for the Bouncer */

    st->bouncerServer = MakeServerBinding(st->serverMUX, &st->bouncer_cl, 
					  NULL, 
					  GetStubs(st, Bouncer_clp__code));
    
    if(!st->bouncerServer) {
	fprintf(stderr, "Unable to create Bouncer\n");
	RAISE_RPCMod$Failure(RPCMod_Problem_Error);
    }
	
    /* Store it with a known binding ID */
    
    LongCardTbl$Put(st->serverMUX->bindingTbl, 0, st->bouncerServer);
    

    Threads$Fork(Pvs(thds), RPCClientRXThread, st->clientMUX, 0);
    
    return &st->transport_cl;
}

/*---Transport methods---------------------------------------------------*/

static IDCOffer_clp RPCOffer_m(IDCTransport_cl	*self, 
			       const Type_Any	*server,
			       IDCCallback_clp     scb,
			       Heap_clp		heap,
			       Gatekeeper_clp	gk,
			       Entry_clp		entry,
			       /* RETURNS */
			       IDCService_clp	*service )
{
    RPC_st           *st = self->st;
    RPCOffer_st      *offer;
    IDCStubs_Info    info;
    IDCOffer_IOR     ior;
    RPCIORInfo       iorinfo;

    info = GetStubs(st, server->type);

    /* Create the offer */
    if (!(offer = Heap$Malloc(heap, sizeof(*offer)))) {
	DB(eprintf("RPCTransport$Offer: failed to malloc offer!\n"));
	RAISE_Heap$NoMemory();
    }

    CL_INIT (offer->offer_cl,&offer_op,offer);
    ANY_COPY (&offer->server,server);
    offer->scb   = scb;
    offer->info = info;
    offer->pdom = VP$ProtDomID(Pvs(vp));
    offer->rpc = st;

    {
	IDCTransport_clp shm = NAME_FIND("modules>ShmTransport", 
					 IDCTransport_clp);

	offer->shmoffer = IDCTransport$Offer(shm, server, scb, heap, 
					     gk, entry, &offer->shmservice);

    }
	
    *service = NULL; /* XXX */

    MU_LOCK(&st->serverMUX->offerTblMu);

    offer->offerId = st->serverMUX->nextOffer++;
    LongCardTbl$Put(st->serverMUX->offerTbl, offer->offerId, offer);

    printf("Made offer %qx, tc %qx\n", offer->offerId, offer->server.type);

    MU_RELEASE(&st->serverMUX->offerTblMu);

    /* Build list of IORs */
    
    iorinfo.offerId = offer->offerId;
    iorinfo.sap = st->serverMUX->comm.localSap;

    offer->iorlist = SEQ_NEW(IDCOffer_IORList, 0, Pvs(heap));
    
    /* Standard RPC IOR */
    
    ior.transport = "NemesisRPC:UDP";
    SEQ_INIT(&ior.info, sizeof(iorinfo), Pvs(heap));
    *(RPCIORInfo *)(SEQ_START(&ior.info)) = iorinfo;
    
    SEQ_ADDL(offer->iorlist, ior);

    /* IOR to catch an offer from a different domain on the same
       box. The Shm IDC offer would have listed one of this IOR type,
       if we'd let it, but then we'd lose our RPCness */

    IDCOfferMod$NewLocalIOR(st->om, &offer->offer_cl, &ior);

    SEQ_ADDL(offer->iorlist, ior);

    /* IOR to catch one of our own offers coming back to us - allows
       better checking than using the Shm IOR */

    ior.transport = st->iorconverter_name;
    SEQ_INIT(&ior.info, sizeof(iorinfo), Pvs(heap));
    *(RPCIORInfo *)(SEQ_START(&ior.info)) = iorinfo;

    SEQ_ADDL(offer->iorlist, ior);

    return &offer->offer_cl;
}

static IDCOffer_clp RPCForge_m(RPCTransport_clp   self, 
			       const FlowMan_SAP  *sap,
			       uint64_t           offerID,
			       Type_Code          tc) {

    RPC_st *st = self->st;
    SurrRPCOffer_st *offer;

    if(sap->tag != FlowMan_PT_UDP) {
	RAISE_IDCOffer$UnknownIOR();
    }

    offer = GetSurrogateOffer(st, sap, offerID, tc, NULL);
    
    if(!offer) RAISE_Heap$NoMemory();

    if(!offer->iorlist) {

	/* XXXX This is a race condition */

	/* If we have just created the surrogate, we can't necessarily
	   give it a full set of IORs, since we have no way of knowing
	   its memory address at the remote end. So if someone on the
	   remote end other than the exporting domain (who can catch
	   it with the RPC specific IOR) imports it and binds to it,
	   they'll end up using UDP rather than Shm. Such is life. In
	   general Forge() should only be used for obtaining bootstrap
	   offers anyway. */

	IDCOffer_IOR ior;
	char buf[128];
	int iorlen = sizeof(FlowMan_SAP) + 8;
	addr_t p;
	
	offer->iorlist = SEQ_NEW(IDCOffer_IORList, 0, Pvs(heap));

	/* Standard RPC IOR */	

	ior.transport = strdup("NemesisRPC:UDP");
	SEQ_INIT(&ior.info, iorlen, Pvs(heap));
	p = SEQ_START(&ior.info);
	*((FlowMan_SAP *)p)++ = *sap;
	*((uint64_t *)p) = offerID;

	SEQ_ADDL(offer->iorlist, ior);

	/* IOR that the remote server domain can use to get back to
           their original offer */
	
	sprintf(buf, "NemesisRPC:UDP:%08X.%04X", 
		sap->u.UDP.addr.a, sap->u.UDP.port);
	
	ior.transport = strdup(buf);
	SEQ_INIT(&ior.info, iorlen, Pvs(heap));
	p = SEQ_START(&ior.info);
	*((FlowMan_SAP *)p)++ = *sap;
	*((uint64_t *)p) = offerID;
	
	SEQ_ADDL(offer->iorlist, ior);
	
    }

    return &offer->offer_cl;

}

static uint64_t RPCMakeServer_m(RPCTransport_clp      self,
				const FlowMan_SAP    *sap,
				const Type_Any       *any,
				/* RETURNS */
				IDCServerBinding_clp *binding) {

    RPC_st *st = self->st;

    Server_st *server;
    IDCStubs_Info info = GetStubs(st, any->type);
    
    if(sap->tag != FlowMan_PT_UDP) {
	RAISE_IDCOffer$UnknownIOR();
    }

    server = MakeServerBinding(st->serverMUX, (addr_t) (pointerval_t) any->val,
			       sap, info);
			       
    if(!server) {
	RAISE_Heap$NoMemory();
    }
    
    /* Store the binding information */
    
    MU_LOCK(&st->serverMUX->bindingTblMu);
    
    LongCardTbl$Put(st->serverMUX->bindingTbl, 
		    (uint64_t) (pointerval_t) server, server);
    
    MU_RELEASE(&st->serverMUX->bindingTblMu);
    
    *binding = &server->bind_cl;

    return (uint64_t) (word_t) server;

}
	
static IDCClientBinding_clp RPCMakeClient_m(RPCTransport_clp self,
					  const FlowMan_SAP *sap,
					  uint64_t objectHandle,
					  Type_Code tc,
					  Type_Any *any) {

    RPC_st *st = self->st;
    Client_st *client;

    if(sap->tag != FlowMan_PT_UDP) {
	RAISE_IDCOffer$UnknownIOR();
    }

    client = MakeClientBinding(st->clientMUX, tc, sap, objectHandle);

    any->type = tc;
    any->val = (uint64_t) (word_t) &client->client_cl;
    
    return &client->cntrl_cl;
}
		       

/*---Offer-methods---------------------------------------------------*/

/*
 * Return the type of an offer
 */
static Type_Code RPCOfferType_m(IDCOffer_cl *self)
{
    RPCOffer_st *offer = (self->st);

    return offer->server.type;
}


/*
IDCClient * Return the protection domain of an offer
 */
static ProtectionDomain_ID RPCOfferPDom_m(IDCOffer_cl *self)
{
    RPCOffer_st *offer = self->st;

    return offer->pdom;
}

/*
 * Attempt to bind to a server from the client side.
 */
static IDCClientBinding_clp RPCOfferBind_m(IDCOffer_cl *self,
					Gatekeeper_clp gk,
					Type_Any *cl
					/* RETURNS */ )
{
    RPCOffer_st *st = self->st;

    return IDCOffer$Bind(st->shmoffer, gk, cl);

}

static IDCOffer_IORList *RPCOfferGetIORs_m(IDCOffer_clp self) {

    RPCOffer_st *st = self->st;

    return IDCOfferMod$DupIORs(st->rpc->om, st->iorlist);

}
	
static IDCOffer_clp RPCLocalise_m(IORConverter_clp self,
				  const IDCOffer_IORList *iorlist,
				  uint32_t iornum,
				  Type_Code tc) {

    RPC_st *st = self->st;

    IDCOffer_IOR *ior;
    RPCOffer_st *offer;

    ior = &SEQ_ELEM(iorlist, iornum);

    TRC(printf("RPCLocalise: Checking IOR '%s'\n", ior->transport));

    if(!strcmp(st->iorconverter_name, ior->transport)) {
	
	/* This appears to be our IOR - we can ignore the
	   sap, since it's already been confirmed by the IOR
	   name */
	
	bool_t res;
	RPCIORInfo *iorinfo=(RPCIORInfo *)SEQ_START(&ior->info);
	
	TRC(printf("RPCLocalise: Looking up offer %qx\n", iorinfo->offerId));
	
	MU_LOCK(&st->serverMUX->offerTblMu);
	
	res = LongCardTbl$Get(st->serverMUX->offerTbl, iorinfo->offerId, 
			      (addr_t *)&offer);
	
	MU_RELEASE(&st->serverMUX->offerTblMu);
    
	if(res) {
	    
	    /* We have found an offer with the given id in the
	       table - check the type */
	    
	    if(offer->server.type == tc) {
		
		/* This is the offer that we wanted */
		
		TRC(printf("RPCLocalise: Found offer %p\n", offer));

		return &offer->offer_cl;
		
	    } else {
	    
		printf("RPCLocalise: Offer tc %qx, expected %qx\n", 
		       offer->server.type, tc);
		
	    }
	    
	} else {

	    printf("RPCLocalise: Couldn't find offer %qx\n", iorinfo->offerId);

	}
    }
	
    if(!strcmp("NemesisRPC:UDP", ior->transport)) {
	
	/* This is an RPC offer. It isn't local, since otherwise it
	   would have been caught by the Shm IOR. Forge an offer for
	   it if it isn't already in the remote offer table */
	
	SurrRPCOffer_st *offer_st;
	RPCIORInfo *iorinfo=(RPCIORInfo *)SEQ_START(&ior->info);

	offer_st = GetSurrogateOffer(st, &iorinfo->sap, 
				     iorinfo->offerId, tc, iorlist);

	TRC(printf("RPCLocalise: Got offer %p\n", offer_st));
	
	return &offer_st->offer_cl;

    }
	
    return NULL;
}
	    

static Type_Code RPCSurrOfferType_m(IDCOffer_clp self) {
    
    SurrRPCOffer_st *st = self->st;

    return st->tc;

}

static ProtectionDomain_ID RPCSurrOfferPDom_m(IDCOffer_clp self) {
    
    return 0;

}

static IDCOffer_IORList *RPCSurrOfferGetIORs_m(IDCOffer_clp self) {

    SurrRPCOffer_st *st = self->st;

    return IDCOfferMod$DupIORs(st->rpc->om, st->iorlist);

}

static IDCClientBinding_clp RPCSurrOfferBind_m(IDCOffer_clp self,
					       Gatekeeper_clp gk,
					       Type_Any *cl) {
    SurrRPCOffer_st *st = self->st;
    RPC_st          *rpc = st->rpc;
    Bouncer_clp      bouncer;
    uint64_t         binding;
    Client_st       *client;

    bouncer = GetBouncer(rpc, &st->remoteSap);

    TRC(printf("RPCSurrOfferBind: Binding to offer %qx, tc %qx\n", 
	       st->remoteOffer, st->tc));

    binding = Bouncer$BindTo(bouncer, st->remoteOffer, st->tc);

    client = MakeClientBinding(rpc->clientMUX, st->tc, 
			       &st->remoteSap, binding);
    
    ANY_COPY(cl, &client->cs.srgt);
    return(&client->cntrl_cl);
    
}

/*---Client-methods---------------------------------------------------*/

/*
 * Initialise the buffer for a client call
 */
static IDC_BufferDesc 
RPCCltInitCall_m( IDCClientBinding_cl *self, word_t proc, string_t name )
{
    Client_st *st = self->st;
    uint64_t   *p;
    RPCMsgHeader *hdr;
    RPCClientMUX_st *mux = st->mux;
    RPCComm_st *comm = &mux->comm;
    
    TRC(eprintf("RPCClientBinding$InitCall: st=%p ptr=%p proc=%p '%s'\n",
		st, st->txbuf.base, (addr_t) proc, name));


    /* Lock this channel */
    MU_LOCK(&st->txBufMu);

    /* Set up tx header */
    
    GetTXBuffer(comm, st->txrecs, True);

    hdr = st->txbuf.base = st->txrecs[1].base;

    hdr->msgType = RPC_REQUEST_MSG;
    hdr->u.binding = st->remoteBinding;
    st->transId = hdr->transId = NOW();

    /* Marshal procedure number */
    p = (uint64_t *) (hdr+1);
    *p = proc;
    st->txbuf.ptr   = ALIGN64(p+1);
    st->txbuf.space = comm->mtu - (st->txbuf.ptr - st->txbuf.base);
  
    /* return transmit buffer descriptor */
    return &st->txbuf;
}

/*
 * Initialise the buffer for a client ANNOUNCEMENT
 */
static IDC_BufferDesc 
RPCCltInitCast_m( IDCClientBinding_cl *self, word_t proc, string_t name )
{
    /* Simply do the same as for a call. */
    return RPCCltInitCall_m(self, proc, name );
}

/* 
 * Send a call off
 */
static void 
RPCCltSendCall_m( IDCClientBinding_cl *self, IDC_BufferDesc b )
{
    Client_st *st = self->st;
    RPCClientMUX_st    *mux = st->mux;
    RPCComm_st *comm = &mux->comm;
        
    /* XXX - ought to be associated with buffer */
    BlockedRequest *req = GetBlockReq(st, mux);

    TRC(eprintf("RPCClientBinding$SendCall: st=%p\n", st));
    
    MU_LOCK(&mux->blockTblMu);
    
    LongCardTbl$Put(mux->blockTbl, st->transId, req);

    MU_RELEASE(&mux->blockTblMu);

    TRC(printf("ClientSendCall: Added blocked request %p to table\n", req));

    /* Send the request to the remote machine */
    req->txrecs[0] = st->txrecs[0];
    req->txrecs[1].base = st->txrecs[1].base;
    req->txrecs[1].len = st->txbuf.ptr - st->txbuf.base;
    req->txrecs[2] = st->txrecs[2];

    st->blockReq = req;
    SendRequest(comm, req->txrecs, &st->remoteSap);

    /* Set our timeout */
    req->timeout = NOW() + TIMEOUTVAL;
    
}

/* 
 * Wait for a packet to come back and set up the receive buffer. 
 * The string is not used.
 */
static word_t 
RPCCltReceiveReply_m( IDCClientBinding_cl *self, IDC_BufferDesc *b,
		  string_t *s )
{
    Client_st *st = self->st;
    RPCClientMUX_st    *mux = st->mux;
    RPCComm_st         *comm = &mux->comm;

    uint64_t  *p;
    uint64_t   transId = st->transId;
    BlockedRequest *req = st->blockReq;
    RPCMsgHeader *hdr;
    IO_Rec recs[3];
    bool_t success;

    TRC(eprintf("RPCClientBinding$ReceiveReply: st=%p\n", st));

    /* Now that we have the transaction ID we can release the binding
       mutex. Ideally, InitCall would return a call ID so that we
       could release the mutex earlier */

    MU_RELEASE(&st->txBufMu);

    /* Wait for the reply */

    TRC(printf("RPCClientBinding$RecvReply: Waiting for reply from server\n"));

    success = EC_AWAIT_UNTIL(req->ec, req->waitval, req->timeout) == 1;

    /* If the reply hasn't returned yet, it's our job to remove
       ourselves from the request table - this is a race with the RX
       thread, so check the state of things again once we've acquired
       the mutex */

    if(!success) {
	MU_LOCK(&mux->blockTblMu);

	/* If Delete returns False, then the RX thread has just
           dropped a reply packet on us ... Deal with it in the normal
           way. */

	success = !LongCardTbl$Delete(mux->blockTbl, transId,
				      (addr_t *) &req);
	MU_RELEASE(&mux->blockTblMu);
    }
    recs[0] = req->rxrecs[0];
    recs[1] = req->rxrecs[1];
    recs[2] = req->rxrecs[2];
    
    ReleaseBlockReq(mux, req);

    if(!success) {
	eprintf("RPCClientBinding: Timed out!\n");
	RAISE_Thread$Alerted();
    }

    TRC(printf("RPCClientBinding$RecvReply: Received reply from server\n"));

    /* Get hold of the packet */

    if(recs[1].len > st->rxsize) {
	printf("RPCClientBinding$RecvReply: Invalid response size %u\n", 
	       (uint32_t)recs[1].len);
	while(1);
    }

    hdr = recs[1].base;

    if(!hdr->u.success) {

	TRC(printf("RPCClientBinding$RecvReply: "
		   "Unsuccessful RPC reply, binding %qx\n",
		   st->remoteBinding));
	ReleaseClientRXBuffer(comm, recs);
	RAISE_Thread$Alerted();

    }
    
    MU_LOCK(&st->rxBufMu);

    st->rxrecs[0] = recs[0];
    st->rxrecs[1] = recs[1];
    st->rxrecs[2] = recs[2];

    st->rxbuf.base = recs[1].base;

    p = (uint64_t *) (hdr + 1);
    
    st->rxbuf.ptr   = ALIGN64(p+1);
    st->rxbuf.space = st->rxsize - (st->rxbuf.ptr - st->rxbuf.base);
    *b = &st->rxbuf;

    if(s) *s = NULL;

    TRC(eprintf("RPCClientBinding$ReceiveReply: st=%p got reply %qx\n",
		st, *p));
    return *p;
}


/*
 * Release the receive buffer.
 */
static void 
RPCCltAckReceive_m( IDCClientBinding_cl *self, IDC_BufferDesc b )
{
    Client_st *st = self->st;

    IO_Rec recs[3];

    recs[0] = st->rxrecs[0];
    recs[1] = st->rxrecs[1];
    recs[2] = st->rxrecs[2];

    TRC(eprintf("RPCClientBinding$AckReply: st=%p\n", st));
    st->rxbuf.base = st->rxbuf.ptr = NULL;

    MU_RELEASE(&st->rxBufMu);

    ReleaseClientRXBuffer(&st->mux->comm, recs);

}


/*
 * Destroy a binding 
 */
static void 
RPCCltDestroy_m( IDCClientBinding_cl *self )
{
    
    Client_st *st = self->st;
    DestroyClientBinding(st->mux, st);

}


/*---Server-methods---------------------------------------------------*/
/*
 * IDCServerBinding operations
 */

/* There's no need to hold st->mu: Entry.if guarantees we're not
   called reentrantly. */

static bool_t
RPCSvrReceiveCall_m (IDCServerBinding_clp	self,
		  /* RETURNS */
		  IDC_BufferDesc	*b,
		  word_t		*op,
		  string_t		*s    )
{
    Server_st *st = self->st;
    uint64_t *p;
    RPCMsgHeader *hdr;
    
    TRC(eprintf("RPCServerBinding$RcvCall\n"));

    hdr = st->rxbuf.base;

    if(!hdr) {
	return False;
    }

    st->transId = hdr->transId;

    p = (uint64_t *) (hdr + 1);
    
    *b  = &st->rxbuf;
    
    *op = *p;
    
    st->rxbuf.ptr   = ALIGN64(p+1);
    st->rxbuf.space = st->rxsize - (st->rxbuf.ptr - st->rxbuf.base);
    
    TRC(eprintf(" ptr=%p op=%p\n", p, (addr_t)*op));
    
    return True;

}

static void
RPCSvrAckReceive_m (IDCServerBinding_clp self, IDC_BufferDesc b)
{

    Server_st *st = self->st;
    st->rxbuf.ptr = st->rxbuf.base = NULL;

}

static IDC_BufferDesc
RPCSvrInitReply_m (IDCServerBinding_clp self)
{
    Server_st *st = self->st;
    RPCServerMUX_st *mux = st->mux;
    RPCComm_st *comm = &mux->comm;
    uint64_t   *p;
    RPCMsgHeader *hdr;

    TRC(eprintf("IPCServerBinding$InitReply: st=%p\n", st));

    /* We are single threaded - no need to lock mutex */
    
    GetTXBuffer(comm, st->txrecs, False);

    st->txbuf.ptr = st->txbuf.base = st->txrecs[1].base;
    st->txbuf.space = st->txrecs[1].len;

    hdr = st->txbuf.base;

    hdr->msgType = RPC_REPLY_MSG;
    hdr->u.success = True;
    hdr->transId = st->transId;

    p = (uint64_t *) (hdr + 1);
    *p = 0;			/* normal return */

    st->txbuf.ptr   = ALIGN64(p+1);
    st->txbuf.space = comm->mtu - (st->txbuf.ptr - st->txbuf.base);

    return &st->txbuf;
}

static IDC_BufferDesc
RPCSvrInitExcept_m (IDCServerBinding_clp	self,
		 word_t 		exc,
		 string_t		name)
{
    Server_st *st = self->st;
    RPCComm_st *comm = &st->mux->comm;
    uint64_t   *p;
    RPCMsgHeader *hdr;

    TRC(eprintf("RPCServerBinding$InitExc: st=%p exc=%p `%s'\n", 
		st, (addr_t)exc, name));

    /* We are single threaded - no need to lock mutex */
    
    GetTXBuffer(comm, st->txrecs, False);

    st->txbuf.ptr = st->txbuf.base = st->txrecs[1].base;
    st->txbuf.space = st->txrecs[1].len;

    hdr = st->txbuf.base;
    hdr->msgType = RPC_REPLY_MSG;
    hdr->u.success = True;
    hdr->transId = st->transId;

    p = (uint64_t *) (hdr + 1);
    *p = exc;			/* exceptional return */

    st->txbuf.ptr   = ALIGN64(p+1);
    st->txbuf.space = comm->mtu - (st->txbuf.ptr - st->txbuf.base);

    return &st->txbuf;
}

static void
RPCSvrSendReply_m (IDCServerBinding_clp self, IDC_BufferDesc b)
{
    Server_st *st = self->st;
    RPCServerMUX_st *mux = st->mux;
    RPCComm_st *comm = &mux->comm;

    TRC(eprintf("RPCServerBinding$SendReply: st=%p\n", st));

    st->txrecs[1].len = st->txbuf.ptr - st->txbuf.base;

    SetTarget(comm, &st->remoteSap);
    IO$PutPkt(comm->txio, 3, st->txrecs, 0, FOREVER);
    
}

static Domain_ID RPCSvrClient_m (IDCServerBinding_cl     *self,
			      ProtectionDomain_ID    *pdom )
{
    *pdom = 0;
    return 0;
}

static void
RPCSvrDestroy_m (IDCServerBinding_clp self)
{
    Server_st *st = self->st;

    TRC(eprintf("RPCServerBinding$Destroy: Destroy binding %p\n", self));
    
    DestroyServerBinding(st->mux, st);

}

/* Bouncer methods */

uint64_t Bouncer_BindTo_m(Bouncer_clp self, uint64_t offerID, Type_Code tc) {

    RPC_st *st = self->st;
    RPCOffer_st *offer;
    Binder_Cookie clt_addr = { &st->bouncerServer->remoteSap, 
			       sizeof(FlowMan_SAP) };
			     
    Binder_Cookies clt_cks, svr_cks = {0, 0, NULL, NULL};
    
    Server_st * NOCLOBBER server = NULL;

    SEQ_INIT_FIXED(&clt_cks, &clt_addr, 1);

    TRC(fprintf(stderr, "Bouncer$BindTo(%p, %qx, %qx)\n", self, offerID, tc));

    USING(MUTEX, &st->serverMUX->offerTblMu, 
	  
	  /* Check offer exists */

	  if(!LongCardTbl$Get(st->serverMUX->offerTbl, offerID, 
			      (addr_t) &offer)) {
	      printf("Bouncer : Request for invalid offer %qx\n", offerID);
	      RAISE_Bouncer$Error(Bouncer_Problem_BadOffer);
	  }
	  
	  /* Check client has correct type code */
	  
	  if(offer->server.type != tc) {
	      printf("Bouncer : Offer %qx tc %qx, requested tc %qx\n", 
		     offer->offerId, offer->server.type, tc);
	      RAISE_Bouncer$Error(Bouncer_Problem_BadOffer);
	  }

	  /* Check that server is willing to accept connection */

	  if(offer->scb &&
	     !IDCCallback$Request(offer->scb, &offer->offer_cl, 
				  0, 0, 
				  &clt_cks, &svr_cks)) {

	      RAISE_Bouncer$Error(Bouncer_Problem_ServerRefused);
	  }	     


	  server = MakeServerBinding(st->serverMUX, 
				     (addr_t) (pointerval_t) offer->server.val,
				     &st->bouncerServer->remoteSap,
				     offer->info);

	  if(!server) {
	      RAISE_Bouncer$Error(Bouncer_Problem_NoResources);
	  }

	  /* Store the binding information */
	  
	  MU_LOCK(&st->serverMUX->bindingTblMu);
	  
	  LongCardTbl$Put(st->serverMUX->bindingTbl, 
			  (uint64_t) (pointerval_t) server, 
			  server);
	  
	  MU_RELEASE(&st->serverMUX->bindingTblMu);

	  server->offer = offer;
	  
	  TRC(printf("Bouncer: Stored binding - making callback\n"));
	  
	  if (offer->scb) {
	      
	      Type_Any any = offer->server;

	      if(!IDCCallback$Bound(offer->scb, &offer->offer_cl, 
				    &server->bind_cl, 0, 
				    0, &clt_cks, &any) ||
		 (!TypeSystem$IsType(Pvs(types), any.type, 
				     offer->server.type))) {

		  DestroyServerBinding(st->serverMUX, server);
		  RAISE_Binder$Error(Binder_Problem_ServerError);
	      }
	      
	      server->ss.service = (addr_t) (pointerval_t) any.val;
	  } else {
	      server->ss.service = (addr_t) (pointerval_t) offer->server.val;
	  }
	  

	);
    
    return (uint64_t) (word_t) server;

}

static void Bouncer_Close_m(Bouncer_clp self, uint64_t objectHandle) {

    RPC_st *st = self->st;
    Server_st *server;
    
    USING(MUTEX, &st->serverMUX->bindingTblMu,
	  
	  if(!LongCardTbl$Get(st->serverMUX->bindingTbl, objectHandle, 
			      (addr_t)&server)) {
	      printf("Bouncer$Close for invalid binding %qx\n", objectHandle);
	      RAISE_Bouncer$Error(Bouncer_Problem_BadHandle);
	  }
	  
	  if(memcmp(&st->bouncerServer->remoteSap, 
		    &server->remoteSap, 
		    sizeof(FlowMan_SAP))!=0) {
	      printf("Bouncer$Close for binding %qx - not owner\n", 
		     objectHandle);
	      RAISE_Bouncer$Error(Bouncer_Problem_NotOwner);
	  }
	  
	  printf("Destroying binding %qx\n", objectHandle);
	  
	  LongCardTbl$Delete(st->serverMUX->bindingTbl, objectHandle, 
			     (addr_t) &server);
	  
	  DestroyServerBinding(st->serverMUX, server);
	  
	);
}

	      
static IDCStubs_Info GetStubs(RPC_st *st, Type_Code tc) {
    
    return StubGen$GenIntf(st->stubgen, tc, 
			   (IDCMarshalCtl_clp)&RPCMarshalCtl, 
			   Pvs(heap));
}


Bouncer_clp GetBouncer(RPC_st *st, const FlowMan_SAP *sap) {
    
    uint64_t sapval;
    Bouncer_clp bouncer;
    Client_st *client;

    *(FlowMan_IPSAP *) (&sapval) = sap->u.UDP;
    MU_LOCK(&st->bouncerTblMu);
    
    if(!LongCardTbl$Get(st->bouncerTbl, sapval, (addr_t) &bouncer)) {

	/* Bouncer not found in table - create a binding */

	client = MakeClientBinding(st->clientMUX, Bouncer_clp__code, sap, 0);

	bouncer = (Bouncer_clp )&client->client_cl;
	
	LongCardTbl$Put(st->bouncerTbl, sapval, bouncer);

    }

    MU_RELEASE(&st->bouncerTblMu);
    
    return bouncer;
    
}
	

SurrRPCOffer_st *GetSurrogateOffer(RPC_st *st, 
				   const FlowMan_SAP *sap,
				   uint64_t offerId,
				   Type_Code tc,
				   const IDCOffer_IORList *iorlist) {

    SurrRPCOffer_st *offer;
    const FlowMan_IPSAP *ipsap;

    uint64_t tbl_key;
    bool_t res = True;

    /* Hmm, we need 12 bytes to uniquely identify the offer in the
       table, and only 8 bytes of key space. We mush them all
       together, throw in the type code for good measure, and
       increment in the unlikely case of collisions. This means we can
       never remove offers from the table, so maybe some form of
       chaining might be better in the long run */

    ipsap = &sap->u.UDP;

    TRC(printf("GetSurrOffer: addr %x, port %x, offerId %qx, tc %qx\n",
	       ipsap->addr.a, ipsap->port, offerId, tc));

    tbl_key = 
	(((uint64_t)ipsap->addr.a) << 32) ^
	(((uint64_t)ipsap->port) << 16) ^
	offerId ^ tc;

    TRC(printf("GetSurrOffer: tbl_key is %qx\n", tbl_key));

    MU_LOCK(&st->remoteOfferTblMu);

    while(res) {

	res = LongCardTbl$Get(st->remoteOfferTbl, tbl_key, (addr_t *)&offer);
	
	if(res) {

	    const FlowMan_IPSAP *offeripsap = &offer->remoteSap.u.UDP;

	    TRC(printf("GetSurrOffer: Found offer %p in table\n", offer));
	    TRC(printf("GetSurrOffer: addr %x, port %x, offerId %qx, tc %qx\n",
		       offeripsap->addr.a, offeripsap->port,
		       offer->remoteOffer, offer->tc));

	    if((offeripsap->addr.a == ipsap->addr.a) &&
	       (offeripsap->port == ipsap->port) &&
	       (offer->remoteOffer == offerId)) {
		
		if(offer->tc != tc) {
		    printf("GetSurrOffer: Offer tc %qx, asked for %qx\n",
			   offer->tc, tc);
		}
		
		MU_RELEASE(&st->remoteOfferTblMu);
		
		return offer;
		
	    } else {
		
		printf("GetSurrOffer: RemoteOffer collision! -- %qx\n", 
		       tbl_key);

		/* Try the next key in line */
		tbl_key++;
		
	    }
	} else {

	    TRC(printf("GetSurrOffer: Offer not found in table\n"));

	}
	
    }

    offer = Heap$Calloc(Pvs(heap), 1, sizeof(*offer));

    if(!offer) {
	printf("GetSurrOffer: Couldn't allocate offer\n");
	MU_RELEASE(&st->remoteOfferTblMu);
	
	return NULL;
    }

    CL_INIT(offer->offer_cl, &surrogate_offer_op, offer);
    offer->rpc = st;
    offer->tc = tc;
    offer->remoteSap = *sap;
    offer->remoteOffer = offerId;

    /* XXX Should we actually copy the iorlist? */
    offer->iorlist = (IDCOffer_IORList *) iorlist;

    TRC(printf("Created new surrogate offer %p\n", offer));

    LongCardTbl$Put(st->remoteOfferTbl, tbl_key, offer);

    MU_RELEASE(&st->remoteOfferTblMu);

    return offer;

}

static void DestroyClientBinding(RPCClientMUX_st *st, Client_st *client) {

    Bouncer_clp bouncer = GetBouncer(st->rpc, &client->remoteSap);

    Bouncer$Close(bouncer, client->remoteBinding);

    MU_FREE(&client->txBufMu);
    MU_FREE(&client->rxBufMu);
    FREE(client);
}

Client_st *MakeClientBinding(RPCClientMUX_st *st, 
			     Type_Code tc, const FlowMan_SAP *sap,
			     uint64_t binding) {

    IDCStubs_Info info = GetStubs(st->rpc, tc);
    Client_st *client;
    int txsize = info->c_sz + sizeof(RPCMsgHeader);
    int rxsize = info->s_sz + sizeof(RPCMsgHeader);
	
    addr_t client_ops;

#if 0
    /* We need more thought as to what the s_sz and c_sz really mean */
    if(txsize > st->comm.mtu || rxsize > st->comm.mtu) {
	printf("Marshalling buffers (%u, %u) too large for mtu (%u)\n",
	       txsize, rxsize, st->comm.mtu);
	while(1);
    }
#endif
    
    client= Heap$Calloc(Pvs(heap), 1, sizeof(*client));

    if(!client) {
	RAISE_Heap$NoMemory();
    }

    client_ops = ((_generic_cl *)(info->surr.val))->op;
    
    CL_INIT(client->client_cl, client_ops, client);
    CL_INIT(client->cntrl_cl, &RPCClientBinding_op, client);
    ANY_INITC(&client->cs.srgt, tc, (pointerval_t) &client->client_cl);
    client->cs.binding = &client->cntrl_cl;
    client->cs.marshal = &st->rpc->idc_cl;

    if(info->clnt) {
	IDCClientStubs$InitState(info->clnt, &client->cs);
    }

    TRC(eprintf("Created client state %p\n", client));
    MU_INIT(&client->txBufMu);
    MU_INIT(&client->rxBufMu);

    client->txbuf.base = client->txbuf.ptr = NULL;
    client->txsize = client->txbuf.space = txsize;
    client->txbuf.heap = Pvs(heap);
    
    client->rxbuf.base = client->rxbuf.ptr = NULL;
    client->rxsize = client->rxbuf.space = rxsize;
    client->rxbuf.heap = Pvs(heap);

    client->mux = st;
    client->remoteSap = *sap;
    client->remoteBinding = binding;
    
    client->blockReq = Heap$Malloc(Pvs(heap), sizeof(*client->blockReq));
    client->blockReq->ec = EC_NEW();
    return client;
    
}

static void DestroyServerBinding(RPCServerMUX_st *st, Server_st *server) {

    addr_t val;
    /* XXX Ought to reference count here in case someone is using it
       ... */

    MU_LOCK(&st->bindingTblMu);
    
    LongCardTbl$Delete(st->bindingTbl, (uint64_t)(word_t)server, &val);

    MU_RELEASE(&st->bindingTblMu);

    /* If the server registered callbacks then call them */
    if (server->offer && server->offer->scb) {
	Type_Any any = { server->offer->server.type, 
			 (pointerval_t) server->ss.service };
	
	IDCCallback$Closed(server->offer->scb, &server->offer->offer_cl,
			   &server->bind_cl, &any);
    }

    FREE(server);
}

static Server_st *MakeServerBinding(RPCServerMUX_st *st, addr_t service,
				    const FlowMan_SAP *sap,
				    IDCStubs_Info info) {

    Server_st *server;
    int txsize, rxsize;

    TRC(printf("MakeServerBinding: st %p, service %p, sap %p\n",
	       st, service, sap));

    txsize = info->s_sz + sizeof(RPCMsgHeader);
    rxsize = info->c_sz + sizeof(RPCMsgHeader);
    
#if 0
    if(txsize > st->comm.mtu || rxsize > st->comm.mtu) {
	printf("Marshalling buffers too large\n");
	return NULL;
    }
#endif
    
    /* Make the binding */
    server = Heap$Calloc(Pvs(heap), 1, sizeof(*server));
    if(!server) {
	printf("Couldn't allocate server\n");
	return NULL;
    }
    
    

    server->ss.service = service;
    server->ss.binding = &server->bind_cl;
    server->ss.marshal = &st->rpc->idc_cl;
    
    server->txbuf.base = server->txbuf.ptr = NULL;
    server->txsize = server->txbuf.space = txsize;
    server->txbuf.heap = Pvs(heap);
    
    server->rxbuf.base = server->rxbuf.ptr = NULL;
    server->rxsize = server->rxbuf.space = rxsize;
    server->rxbuf.heap = Pvs(heap);
    
    if(sap) {
	TRC(printf("Setting remote Sap for %p to %I:%u\n",
		   server, sap->u.UDP.addr, sap->u.UDP.port));
	server->remoteSap = *sap;
	server->checkSap = True;
    } else {
	server->checkSap = False;
    }

    CL_INIT(server->cntrl_cl, info->stub->op, server);
    CL_INIT(server->bind_cl, &RPCServerBinding_op, server);

    server->mux = st;
    	  
    TRC(printf("MakeServerBinding: Returning server %p\n", server));

    return server;
}

void SendErrorReply(RPCServerMUX_st *st, RPCMsgHeader *hdr, FlowMan_SAP *sap) {

    IO_Rec recs[3];
    RPCMsgHeader *newhdr;

    GetTXBuffer(&st->comm, recs, False);

    newhdr = recs[1].base;
    newhdr->msgType = RPC_REPLY_MSG;
    newhdr->u.success = False;
    newhdr->transId = hdr->transId;
    
    recs[1].len = sizeof(RPCMsgHeader);

    SetTarget(&st->comm, sap);

    IO$PutPkt(st->comm.txio, 3, recs, 0, FOREVER);

}
    
const char nibble_table[] = { '0', '1', '2', '3', '4', '5', '6', '7', 
			'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static void memdump(uint8_t *addr, int len) {
    char buffer[65];
    int bpos = 0;

    while(len --) {
	buffer[bpos] = nibble_table[((*addr) >> 4) & 0xf];
	buffer[bpos+1] = nibble_table[(*addr) & 0xf];
	bpos += 2;
	addr ++;
	
	if((bpos == 64) || (len == 0)) {
	    buffer[bpos] = 0;
	    fprintf(stderr, "%s\n", buffer);
	    bpos = 0;
	}
    }
}

static void RPCDispatch_m(IDCServerStubs_clp self) {    

    RPCServerMUX_st *st = self->st;

    uint32_t nrecs;
    word_t value;
    IO_Rec recs[3];
    FlowMan_SAP sap;
    RPCMsgHeader *hdr;

    Server_st *server;
    bool_t res;

    /* Get a packet, see which binding it is for and hit the server
       binding */
    
    while(IO$GetPkt(st->comm.rxio, 3, recs, 0, &nrecs, &value)) {
	
	/* We have a packet. See who it's from */
	
	if(!UDP$GetLastPeer(st->comm.udp, &sap)) {
	    fprintf(stderr, "RPC: Error getting peer!\n");
	    return;
	}

	TRC(fprintf(stderr, "Got packet, addr %p, len %d\n", 
		    recs[1].base, recs[1].len));

	hdr = recs[1].base;

	if(hdr->msgType != RPC_REQUEST_MSG) {
	    int i;
	    fprintf(stderr, "RPC Wrong header type %x\n", 
		    (word_t)hdr->msgType);
	    for(i = 0 ; i <3; i++) {
		fprintf(stderr, "Rec %u [%p, %u]:\n", 
			i, recs[i].base, recs[i].len);
		memdump(recs[i].base, recs[i].len);
	    }
	    
	    ReleaseServerRXBuffer(&st->comm, recs);
	    continue;
	}


	MU_LOCK(&st->bindingTblMu);
	    
	/* Does binding exist? */
	
	res = LongCardTbl$Get(st->bindingTbl, hdr->u.binding, 
			      (addr_t) &server);
	
	MU_RELEASE(&st->bindingTblMu);
	
	if(!res) {
	    printf("RPC Request received for unknown binding %qx\n",
		   hdr->u.binding);
	    SendErrorReply(st, hdr, &sap);
	    ReleaseServerRXBuffer(&st->comm, recs);
	    
	    continue;
	}
	
	/* Is the far end allowed to use it? */
       	if(server->checkSap && 
	   ((server->remoteSap.u.UDP.addr.a != sap.u.UDP.addr.a) ||
	    (server->remoteSap.u.UDP.port != sap.u.UDP.port))) {
	    
	    printf("RPC request for binding %qx - not owner\n"
		   "Owner is %I:%u, caller is %I:%u\n",
		   hdr->u.binding,
		   server->remoteSap.u.UDP.addr,
		   server->remoteSap.u.UDP.port,
		   sap.u.UDP.addr,
		   sap.u.UDP.port);
	    
	    SendErrorReply(st, hdr, &sap);
	    
	    ReleaseServerRXBuffer(&st->comm, recs);
	    
	    continue;
	} else {
	    
	    /* Special hack for the bouncer - could be rather more
	       elegant */
	    
	    server->remoteSap = sap;
	}
	
	/* Set up the server's RX bufs - we will free them afterwards */
	
	server->rxrecs[0] = recs[0];
	server->rxrecs[1] = recs[1];
	server->rxrecs[2] = recs[2];

	server->rxbuf.base = server->rxbuf.ptr = hdr;
	server->rxbuf.space = recs[1].len;
	
	TRC(printf("Dispatching on binding %qx\n", hdr->u.binding));
	IDCServerStubs$Dispatch(&server->cntrl_cl);
	
	ReleaseServerRXBuffer(&st->comm, recs);
    }
    
}

void RPCClientRXThread(RPCClientMUX_st *st) {

    while(1) {

	IO_Rec recs[3];
	word_t value;
	uint32_t nrecs;
	RPCMsgHeader *hdr;
	bool_t foundReq;
	BlockedRequest *req;

	IO$GetPkt(st->comm.rxio, 3, recs, FOREVER, &nrecs, &value);
	
	/* We have a packet */

	TRC(fprintf(stderr, "Got packet, addr %p, len %d\n", 
		    recs[1].base, recs[1].len));

	hdr = recs[1].base;
	
	/* Find the relevant transaction */
	
	TRC(printf("Processing reply message\n"));
	
	MU_LOCK(&st->blockTblMu);
	foundReq = LongCardTbl$Delete(st->blockTbl, hdr->transId, 
				      (addr_t *) &req);
	MU_RELEASE(&st->blockTblMu);

	if(!foundReq) {
	    fprintf(stderr, "Received invalid transaction reply - ignoring\n");
	    
	    IO$PutPkt(st->comm.rxio, 3, recs, 0, FOREVER);
	    continue;
	}
	    
	/* Give the thread the buffer */
	    
	req->rxrecs[0] = recs[0];
	req->rxrecs[1] = recs[1];
	req->rxrecs[2] = recs[2];
	
	TRC(printf("Kicking ec %p\n", req->ec));
	
	req->completed = True;
	EC_ADVANCE(req->ec, 1);
	    
    }
}
	    
static void NewStack(RPCComm_st *st, const FlowMan_SAP *sap) {

    UDPMod_clp udpmod;
    FlowMan_clp fman;
    addr_t txbufs, rxbufs;
    int i;
    int bufsize;

    /* XXX Need to consider what qos to ask for */

    Netif_TXQoS		txqos = {SECONDS(1), 0, True};

    udpmod = NAME_FIND("modules>UDPMod", UDPMod_clp);
    fman = IDC_OPEN("svc>net>iphost-default", FlowMan_clp);

    if(sap->tag != FlowMan_PT_UDP) {
	ANY_DECL(pt, FlowMan_PT, sap->tag);
	fprintf(stderr, "RPCMod: Unknown protocol type %a\n",
		&pt);
	RAISE_RPCMod$Failure(RPCMod_Problem_BadSAP);
    }
    
    memset(st, 0, sizeof(*st));

    
    st->localSap = *sap;

    memset(&st->remoteSap, 0, sizeof(FlowMan_SAP));
    st->remoteSap.tag = FlowMan_PT_UDP;

    st->udp = UDPMod$New(udpmod, fman, &st->localSap, &st->remoteSap, 
			 &txqos, UDPMod_Mode_Split,
			 &st->udprxheap, &st->udptxheap);
    
    st->header = Protocol$QueryHeaderSize((Protocol_clp)st->udp);
    st->mtu = Protocol$QueryMTU((Protocol_clp)st->udp);
    st->trailer = Protocol$QueryTrailerSize((Protocol_clp)st->udp);

    bufsize = (st->header + st->mtu + st->trailer + 7) & ~7;

    st->txbase = Heap$Malloc(st->udptxheap, NUM_BUFS * bufsize + 8);
    txbufs = ALIGN64(st->txbase);
    
    st->rxbase = Heap$Malloc(st->udprxheap, NUM_BUFS * bufsize + 8);
    rxbufs = ALIGN64(st->rxbase);
    
    if(!(txbufs && rxbufs)) {

	printf("RPCMod_New: Can't allocate buffers\n");

	if(txbufs) FREE(txbufs);
	if(rxbufs) FREE(rxbufs);

	Protocol$Dispose((Protocol_clp) st->udp);
	FREE(st);
	
	RAISE_RPCMod$Failure(RPCMod_Problem_NoResources);
    }


    st->txio = Protocol$GetTX((Protocol_clp)st->udp);
    st->rxio = Protocol$GetRX((Protocol_clp)st->udp);

    st->get_txbuf_sq = SQ_NEW();
    st->put_txbuf_sq = SQ_NEW();
    st->txbuf_ec = EC_NEW();

    st->busy_txbuf_ec = EC_NEW();

    for(i = 0; i < NUM_BUFS; i++) {
	IO_Rec *recs = st->txbufs[i];
	recs[0].base = (addr_t) (txbufs + bufsize * i);
	recs[0].len = st->header;
	recs[1].base = ALIGN64(recs[0].base + st->header);
	recs[1].len = st->mtu;
	recs[2].base = recs[1].base + st->mtu;
	recs[2].len = st->trailer;
	SQ_TICKET(st->put_txbuf_sq);
    }

    st->numtxbufs = NUM_BUFS;

    EC_ADVANCE(st->txbuf_ec, st->numtxbufs - 1);

    for(i = 0; i < NUM_BUFS; i++) {
	IO_Rec recs[3];
	recs[0].base = rxbufs + bufsize * i;
	recs[0].len = st->header;
	recs[1].base = ALIGN64(recs[0].base + st->header);
	recs[1].len = st->mtu;
	recs[2].base = recs[1].base + st->mtu;
	recs[2].len = st->trailer;

	IO$PutPkt(st->rxio, 3, recs, 0, FOREVER);
    }

    Threads$Fork(Pvs(thds), RPCTXReapThread, st, 0);
    TRC(printf("RPCMod$New: Allocated UDP stack\n"));

    MU_INIT(&st->txPutMu);
    MU_INIT(&st->rxGetMu);
    MU_INIT(&st->rxPutMu);
}

static void FreeStack(RPCComm_st *st) {

    /* XXX What else ought there to be here? And how do we stop the
       reaper thread? */

    IO$Dispose(st->rxio);
    IO$Dispose(st->txio);
    Protocol$Dispose((Protocol_clp)st->udp);

    MU_FREE(&st->txPutMu);
    MU_FREE(&st->rxGetMu);
    MU_FREE(&st->rxPutMu);

    SQ_FREE(st->get_txbuf_sq);
    EC_FREE(st->txbuf_ec);
    SQ_FREE(st->put_txbuf_sq);
    
    EC_FREE(st->busy_txbuf_ec);

    Heap$Destroy(st->udptxheap);
    Heap$Destroy(st->udprxheap);
    FREE(st->rxbase);
    FREE(st->txbase);

    /* Don't FREE(st) - is allocated by someone else */
}

static RPCServerMUX_st *NewServerMUX(RPC_st *rpc, 
				     const FlowMan_SAP *sap, 
				     Entry_clp entry) {
    
    LongCardTblMod_clp tblMod = 
	NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);

    RPCServerMUX_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));
    Channel_Pair eps;


    memset(st, 0, sizeof(*st));

    NewStack(&st->comm, sap);
    
    IO$QueryEndpoints(st->comm.rxio, &eps);

    if(eps.rx == NULL_EP) {
	fprintf(stderr, "RPC: IO does not support endpoints!\n");
	FreeStack(&st->comm);
	FREE(st);
	RAISE_RPCMod$Failure(RPCMod_Problem_Error);
    }
    st->rpc = rpc;
    st->offerTbl = LongCardTblMod$New(tblMod, Pvs(heap));
    MU_INIT(&st->offerTblMu);
    st->nextOffer = 0;
    
    st->bindingTbl = LongCardTblMod$New(tblMod, Pvs(heap));
    MU_INIT(&st->bindingTblMu);

    CL_INIT(st->serverstubs, &serverstubs_op, st);

    Entry$Bind(entry, eps.rx, &st->serverstubs);

    return st;
}

static RPCClientMUX_st *NewClientMUX(RPC_st *rpc) {

    LongCardTblMod_clp tblMod = 
	NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);

    RPCClientMUX_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));

    FlowMan_SAP sap;

    memset(st, 0, sizeof(*st));

    CL_INIT(st->notify_cl, &timeout_op, st);

    memset(&sap, 0, sizeof(sap));
    sap.tag = FlowMan_PT_UDP;

    NewStack(&st->comm, &sap);

    st->rpc = rpc;
    st->blockTbl = LongCardTblMod$New(tblMod, Pvs(heap));
    MU_INIT(&st->blockTblMu);


    return st;
}

static void ReleaseClientRXBuffer(RPCComm_st *st, IO_Rec *recs) {

    recs[0].len = st->header;
    recs[1].len = st->mtu;
    recs[2].len = st->trailer;

    MU_LOCK(&st->rxPutMu);

    IO$PutPkt(st->rxio, 3, recs, 0, FOREVER);

    MU_RELEASE(&st->rxPutMu);

}

static void ReleaseServerRXBuffer(RPCComm_st *st, IO_Rec *recs) {

    recs[0].len = st->header;
    recs[1].len = st->mtu;
    recs[2].len = st->trailer;

    IO$PutPkt(st->rxio, 3, recs, 0, FOREVER);

}

static void RPCTXReapThread(RPCComm_st *st) {

    word_t value;
    uint32_t nrecs;
    IO_Rec recs[3];

    while(1) {
	Event_Val val;

	IO$GetPkt(st->txio, 3, recs, FOREVER, &nrecs, &value);

	/* Place buffer on queue */
	val = SQ_TICKET(st->put_txbuf_sq);

	EC_AWAIT(st->txbuf_ec, val - 1);

	val = val % st->numtxbufs;

	st->txbufs[val][0] = recs[0];
	st->txbufs[val][1] = recs[1];
	st->txbufs[val][2] = recs[2];
	
	EC_ADVANCE(st->txbuf_ec, 1);
    }
}


static void GetTXBuffer(RPCComm_st *st, IO_Rec *recs, bool_t lock) {

    Event_Val val = SQ_TICKET(st->get_txbuf_sq);
    
    EC_AWAIT(st->txbuf_ec, val);

    val = val % st->numtxbufs;

    recs[0] = st->txbufs[val][0];
    recs[1] = st->txbufs[val][1];
    recs[2] = st->txbufs[val][2];
    recs[1].len = st->mtu;

}

static void SetTarget(RPCComm_st *st, const FlowMan_SAP *sap) {
    
    if(!memcmp(sap, &st->remoteSap, sizeof(FlowMan_SAP))) {
	return;
    }

    st->remoteSap = *sap;

    UDP$Retarget(st->udp, sap);
}

static void SendRequest(RPCComm_st *st, IO_Rec *recs, const FlowMan_SAP *sap) {

    MU_LOCK(&st->txPutMu);
    
    TRC(printf("Locked txMu\n"));
    SetTarget(st, sap);
    
    IO$PutPkt(st->txio, 3, recs, 0, FOREVER);

    MU_RELEASE(&st->txPutMu);
}

static void RPCTimeout(TimeNotify_clp self, Time_T now, 
		       Time_T deadline, word_t handle) {

    BlockedRequest *req = (BlockedRequest *)handle;

    /* Wake up the thread */

    EC_ADVANCE(req->ec, 1);

}
    
static BlockedRequest *GetBlockReq(Client_st *st, RPCClientMUX_st *mux) {

    /* XXX We really ought not to be mallocing this on every call */
    BlockedRequest *req = Heap$Malloc(Pvs(heap), sizeof(*req));


    req->ec = EC_NEW();
    req->waitval = 1;
    req->completed = False;

    return req;
#if 0
    st->blockReq->waitval = EC_READ(st->blockReq->ec) + 1;
    return st->blockReq;
#endif
}

static void ReleaseBlockReq(RPCClientMUX_st *mux, BlockedRequest *req) {

    EC_FREE(req->ec);
    FREE(req);

}
