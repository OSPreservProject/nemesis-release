/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1994, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.					             *
 **                                                                          *
 *****************************************************************************
 **
 ** FACILITY:
 **
 **      sys/domains/Binder.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Implements Binder.if
 ** 
 ** ENVIRONMENT: 
 **
 **      Nemesis domain.
 **
 ** ID : $Id: Binder.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <domid.h>
#include <salloc.h>
#include <exceptions.h>
#include <contexts.h>
#include <pqueues.h>
#include <links.h>
#include <ecs.h>
#include <salloc.h>
#include <time.h>

#include <VPMacros.h>

#include <DomainMgr_st.h>

#include <Plumber.ih>
#include <Binder.ih>
#include <BinderCallback.ih>
#include <IDCCallback.ih>
#include <Closure.ih>
#include <Entry.ih>

#ifdef DEBUG
#define BINDER_TRACE
/* #define BINDER_CLIENT_TRACE */
#endif

#ifdef BINDER_TRACE
#  define TRC(_x) _x
#else
#  define TRC(_x)
#endif
#define DB(_x) _x

#ifdef BINDER_CLIENT_TRACE
#define CTRC(_x) _x
#else
#define CTRC(_x)
#endif

extern Plumber_clp Plumber;

/* 
 *  The Binder is like the main terminal at Paris Charles de Gaulle
 *  airport.  No, really.  Except that the escalators keep extending and
 *  retracting as they connect to each other.
 *  (PBM: And planes get stuck in infinite holding patterns too often ...)
 * 
 *  Corresponding to each domain is a service and associated
 *  PerDomain_st structure. The service performs incoming invocations
 *  across the Binder interface from a particular client.
 * 
 *  In addition, domains which have Register'ed their willingness to
 *  accept connect requests have an associated BinderCallback_clp.
 *  When a connect request come in, the client's invocation calls
 *  across to the target domain's BinderCallback closure.
 * 
 *  While this is in progress, the reference count on the
 *  PerDomain_st of the target domain is incremented so that the
 *  domain is destroy only after the operation is complete. Reference
 *  counts are modified under the global lock in the Domain Manager's
 *  state record.
 * 
 *  Furthermore, calls across a particular domain's callback interface
 *  are serialised under an internal mutex in the BinderCallback surrogate.
 */ 

static Binder_ConnectTo_fn 		ConnectTo_m;
static Binder_SimpleConnect_fn 		SimpleConnect_m;
static Binder_Close_fn 			Close_m;
static Binder_CloseChannels_fn 		CloseChannels_m;
static Binder_RegisterDomain_fn 	RegisterDomain_m;
static Binder_op binder_ops = {
    ConnectTo_m,
    SimpleConnect_m,
    Close_m,
    CloseChannels_m,
    RegisterDomain_m
};

/*
 * When started up, each domain gets and IDCOffer for the Binder. This
 * is actually a handle on a ShmTransport binding which is entirely
 * created except for the client thread-dependent state, i.e. event
 * counts and mutexes. The Bind operation therefore simply creates these
 * and fills in the Client_t. 
 */
static IDCOffer_Bind_fn		OfferBind_m;
static IDCOffer_Type_fn		OfferType_m;
static IDCOffer_GetIORs_fn      OfferGetIORs_m;
static IDCOffer_PDID_fn		OfferPDID_m;
static IDCOffer_op offer_ops = { 
    OfferType_m, 
    OfferPDID_m,
    OfferGetIORs_m,  
    OfferBind_m 
};

/* 
 * IDCClientBinding operations
 */
#ifdef CONFIG_BINDER_MUX
static IDCClientBinding_InitCall_fn	CltInitCallMUX_m;
#endif
static IDCClientBinding_InitCall_fn	CltInitCall_m;
static IDCClientBinding_InitCast_fn	CltInitCast_m;
static IDCClientBinding_SendCall_fn	CltSendCall_m;
static IDCClientBinding_ReceiveReply_fn CltReceiveReply_m;
static IDCClientBinding_AckReceive_fn   CltAckReceive_m;
static IDCClientBinding_Destroy_fn	CltDestroyBinder_m;
static IDCClientBinding_Destroy_fn      CltDestroyBinderCallback_m;

static IDCClientBinding_op	BinderClientBinding_op = {
#ifdef CONFIG_BINDER_MUX
    CltInitCallMUX_m,
#else
    CltInitCall_m,
#endif
    CltInitCast_m,
    CltSendCall_m,
    CltReceiveReply_m,
    CltAckReceive_m,
    CltDestroyBinder_m
};

static IDCClientBinding_op	BinderCallbackClientBinding_op = {
    CltInitCall_m,
    CltInitCast_m,
    CltSendCall_m,
    CltReceiveReply_m,
    CltAckReceive_m,
    CltDestroyBinderCallback_m
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

static IDCServerBinding_op	ShmServerBinding_op = {
    SvrReceiveCall_m,
    SvrAckReceive_m,
    SvrInitReply_m,
    SvrInitExcept_m,
    SvrSendReply_m,
    SvrClient_m,
    SvrDestroy_m
};

#ifdef CONFIG_BINDER_MUX
static IDCServerStubs_Dispatch_fn	      MuxDispatch_m;
static IDCServerStubs_op MuxStubs_ms =      { 
    MuxDispatch_m 
};


static void MuxDispatch_m(IDCServerStubs_clp self)
{
    MuxServer_t          *st     = self->st;
    ShmConnection_t      *conn   = st->conn;
    word_t               *id;

   /* If it's been smoked, then let dispatch return happily */
    if(DCB_EPRW(RO(Pvs(vp)), conn->eps.tx)->state == Channel_State_Dead)
	return;

    while(conn->call < VP$Poll (Pvs(vp), conn->eps.rx)) {

	/* Unmarshall the binding id (first in the buf)  */
	id = conn->rxbuf.base;    

	/* Frob the rxbuf so that the servers underneath us are happy. 
	   We will frob them back when we leave. */
	conn->rxbuf.base += sizeof(*id);  
	conn->rxsize     -= sizeof(*id);  
	conn->rxbuf.space = conn->rxsize; 
	
	/* We hijack the rx event count as marker so that the 
	   ServerBinding knows it is being called from the 
	   muxed dispatcher (rather than via the while {..} 
	   in the Dispatch method of the stubs. */
	conn->evs.rx      = NULL_EVENT;

	TRC(eprintf("call=%x, rx ep is %x (current val is %x).\n", 
		     conn->call, conn->eps.rx, 
		     VP$Poll(Pvs(vp), conn->eps.rx)));

	if(*id >= st->nstubs) {
	    eprintf("Critical Error: got binding_id as %d (nstubs is %d)!\n", 
		    *id, st->nstubs);
	    ntsc_dbgstop();
	}
	
	TRC(eprintf("=> server stubs are at %p\n", st->stubs[binding_id]));
	TRC(eprintf("=> op = %p, st = %p, op->Dispatch = %p\n", 
		     st->stubs[*id]->op, 
		     st->stubs[*id]->st, 
		     st->stubs[*id]->op->Dispatch));
	IDCServerStubs$Dispatch(st->stubs[*id]);
	TRC(eprintf("done dispatch!\n"));

	/* Now we frob the rxbuf back again */
	conn->rxbuf.base  -= sizeof(*id);  
	conn->rxsize      += sizeof(*id);  
	conn->rxbuf.space = conn->rxsize; 
    } 

    return;
}

static word_t RegisterServer(MuxServer_t *st, IDCServerStubs_cl  *svr)
{

    if(st->nstubs < (MAX_STUBS - 1)) { 
	st->stubs[st->nstubs] = svr;
	return st->nstubs++;
    }

    return (word_t)-1;
}
#endif
/* 
 * Data structures
 */




/*------------------------------------------------ Domain Manager Hooks ---*/


  
/* XXX - handle exceptions in NewDomain */

PerDomain_st *Binder_InitDomain(DomainMgr_st *dm_st, 
				dcb_ro_t *rop, dcb_rw_t *rwp,
				StretchAllocator_clp salloc)
{
    Client_t	*bcl;
    Server_t    *bsv;
    PerDomain_st  *bst;
    IDCStubs_Info stubinfo;
  
    /* Allocate bsv, bcl and bst */
    bst = ROP_TO_PDS(rop);
    memset(bst, 0, sizeof(*bst));
    bsv = &bst->s;
    bcl = (Client_t *)&(rwp->binder_rw);
    
    TRC (eprintf("Binder_NewDomain: state at %x\n", bst));

#if 0
    /* Block until the boot domains are ready */
/* XXX XXX see comment in DomainMgr.c about Intel and ARM */

    TRC(eprintf("Dom ID = %qx, boot_regs = %qx\n", rop->id, 
		EC_READ(dm_st->boot_regs)));

#ifdef SRCIT
#undef DOM_USER
#define DOM_USER DOM_SERIAL
#endif
#ifndef __ALPHA__
    EC_AWAIT (dm_st->boot_regs,
	      ((rop->id < DOM_USER) ? rop->id : DOM_USER)&0xffffffff);
#else
    EC_AWAIT (dm_st->boot_regs,
	      (rop->id < DOM_USER) ? rop->id : DOM_USER);
#endif
#endif /* 0 */
    
    /* Create IDC buffers for the new domain's channel to the binder */
    bst->args    = STR_NEW_SALLOC(dm_st->sysalloc, FRAME_SIZE);
    bst->results = STR_NEW_SALLOC(dm_st->sysalloc, FRAME_SIZE);
    
    /* Map the stretches. */
    SALLOC_SETPROT(dm_st->sysalloc, bst->results, rop->pdid, 
		   SET_ELEM(Stretch_Right_Read) );
    SALLOC_SETPROT(dm_st->sysalloc, bst->args, rop->pdid, 
		   SET_ELEM(Stretch_Right_Read)|
		   SET_ELEM(Stretch_Right_Write));

    /*
     * Next, initialise as much of the Client structure as we can at
     * this stage, given that we're in the Binder's domain and the
     * client won't have a threads package anyway. The rest of this
     * state will be initialised when the client domain calls Bind on 
     * the Binder offer.  Compare with lib/nemesis/IDC/ShmTransport.c:
     * notice that we have to init the state to what the methods in
     * Shm[Client|Server]Binding_op are expecting.
     */


    stubinfo = NAME_FIND (IDC_CONTEXT">Binder", IDCStubs_Info);
    
    TRC(eprintf("Binder_NewDomain: creating Binder client IDC state.\n"));
    {
	Binder_clp client_cl = 
	    NARROW ((Type_Any *)(&stubinfo->surr), Binder_clp);

	CL_INIT(bcl->client_cl,  (addr_t) client_cl->op,  bcl);
	CL_INIT(bcl->binding_cl, &BinderClientBinding_op, bcl);
    }
    ANY_INIT (&bcl->cs.srgt, Binder_clp, &bcl->client_cl);
    bcl->cs.binding = &bcl->binding_cl;
    bcl->cs.marshal = dm_st->shmidc;

    /* The client ShmConnection_t immediately follows the Client_t 
       (aka bcl) in the binder_rw. */
    bcl->conn      = (ShmConnection_t *)(bcl+1);

    if(stubinfo->clnt) {
	/* Let the stubs set up any required run-time state */
	IDCClientStubs$InitState(stubinfo->clnt, &bcl->cs);
    }

    /* Now setup the client side ShmConnection */

    /* XXX Leave mu_init to user */

    /* Transmit buffer rec */
    bcl->conn->txbuf.base  = STR_RANGE(bst->args, &bcl->conn->txsize);
    bcl->conn->txbuf.ptr   = bcl->conn->txbuf.base;
    bcl->conn->txbuf.space = bcl->conn->txsize;
    bcl->conn->txbuf.heap  = NULL;
    
    /* Receive buffer rec */
    bcl->conn->rxbuf.base  = STR_RANGE(bst->results, &bcl->conn->rxsize);
    bcl->conn->rxbuf.ptr   = bcl->conn->rxbuf.base;
    bcl->conn->rxbuf.space = bcl->conn->rxsize;
    bcl->conn->rxbuf.heap  = NULL;
    
    /* Event counts will be filled in by the domain */
    bcl->conn->evs.tx	  = NULL_EVENT;
    bcl->conn->evs.rx     = NULL_EVENT;
    
    /* We can call the new domain's VP interface to get end-points */
    bcl->conn->eps.tx	  = VP$AllocChannel (&rop->vp_cl);
    bcl->conn->eps.rx	  = VP$AllocChannel (&rop->vp_cl);
  
    bcl->conn->call       = 0;

    bcl->conn->dom        = dm_st->dm_dom->id;
    /* XXX SMH: for some reason we don't know our own pdom. Hoho. */
    bcl->conn->pdid       = NULL_PDID;

    /* bcl->offer is not used normally; in BINDER_MUX it is inited below */

    /*
     * Initialise the server idc state.
     */

    /* First we setup the server connection state */
    MU_INIT (&bst->sconn.mu); /* XXX - unused; entry SYNC on server */
  
    bst->sconn.txbuf      = bcl->conn->rxbuf;
    bst->sconn.txsize     = bst->sconn.txbuf.space;
    
    bst->sconn.rxbuf      = bcl->conn->txbuf;
    bst->sconn.rxbuf.heap = Pvs(heap); /* XXX */
    bst->sconn.rxsize     = bst->sconn.rxbuf.space;
  
    bst->sconn.call	  = 0;
    
    bst->sconn.eps.tx	  = Events$AllocChannel(Pvs(evs));
    bst->sconn.eps.rx	  = Events$AllocChannel(Pvs(evs));
    
    bst->sconn.evs.tx     = EC_NEW();
    bst->sconn.evs.rx     = NULL_EVENT; /* we use "Entry" synch. on server */

    bst->sconn.dom        = rop->id;
    bst->sconn.pdid       = rop->pdid; 


#ifdef CONFIG_BINDER_MUX
    /* Initialise the (de)muxing server. */
    TRC(eprintf("Binder_NewDomain: creating Binder MuxIDC server state.\n"));

    CL_INIT (bst->mux.cntrl_cl, &MuxStubs_ms, &bst->mux);
    CL_INIT (bst->mux.cntrl_cl, &MuxStubs_ms, &bst->mux);

    /* We use the connection state we setup above */
    bst->mux.conn = &bst->sconn;
    bst->mux.ss.service = NULL; /* There's no specific service */
    bst->mux.ss.binding = &bsv->binding_cl;
    
#endif
    TRC(eprintf("Binder_NewDomain: creating Binder IDC server state.\n"));
    bsv->ss.service = &bst->binder;
    bsv->ss.binding = &bsv->binding_cl;
    bsv->ss.marshal = dm_st->shmidc;


    CL_INIT (bsv->cntrl_cl,    stubinfo->stub->op,  bsv);
    CL_INIT (bsv->binding_cl, &ShmServerBinding_op, bsv);
    
    /* We use (or share!) the connection state we setup above */
    bsv->conn = &bst->sconn;

    /* Hijack offer field for PerDomain_st */
    bsv->offer            = (Offer_t *) bst;
    

    /* XXX PRB Moved this before the Plumber_Connect since otherwise
       Events$Attach does an ntsc_send causing the new domain to be woken
       up! */
    Events$Attach (Pvs(evs), bsv->conn->evs.tx, 
		   bsv->conn->eps.tx,  Channel_EPType_TX);
    
    /* Connect the event channels */
    Plumber$Connect (Plumber, dm_st->dm_dom, rop, 
		     bsv->conn->eps.tx, 
		     bcl->conn->eps.rx);
    Plumber$Connect (Plumber, rop, dm_st->dm_dom, 
		     bcl->conn->eps.tx, 
		     bsv->conn->eps.rx);

#ifdef CONFIG_BINDER_MUX
    /* Register the binder server with the demuxing server */
    bcl->offer = (Offer_t *)RegisterServer(&bst->mux, &bsv->cntrl_cl);
    
    if(bcl->offer == (Offer_t *)-1) {
	eprintf("Binder_NewDomain: addition of binder to mux svr failed!\n");
	ntsc_halt();
    }

#endif

    /* Initialise the rest of the per-domain state */
    TRC(eprintf("Binder_NewDomain: creating Binder per-domain state.\n"));
    bst->rop      = rop;
    bst->rwp      = rwp;
    bst->dm_st    = dm_st;

    /* refCount starts at 1, to allow for the following references to
       the rop/rwp to be held (update as necessary):
       
       1: the server binding (possibly muxed)
       
       */
       
    bst->refCount = 1;
    bst->callback = NULL;
    CL_INIT (bst->binder, &binder_ops, bst);

    /* The offer closure is a special hand crafted one */
    CL_INIT (bst->offer_cl, &offer_ops, bcl);

    rop->binder_offer = &bst->offer_cl;
    
#define panic(format, args...) \
do { eprintf(format, ## args); ntsc_halt(); } while(0)

    TRC(eprintf("Binder_NewDomain: registering entry.\n"));
    TRY {
#ifdef CONFIG_BINDER_MUX
	Entry$Bind (Pvs(entry), bst->mux.conn->eps.rx, &(bst->mux.cntrl_cl));
#else
	Entry$Bind (Pvs(entry), bsv->conn->eps.rx, &bsv->cntrl_cl );
#endif
    } CATCH_Entry$TooManyChannels() {
	eprintf("Too many channels registering Binder\n");
    } ENDTRY;

    TRC(eprintf("Binder_NewDomain: done.\n"));
    return bst;
}


/*
 * This routine is called by the domain manager for the _first_ domain
 * to be created. This is the domain which the Domain Manager and Binder
 * run in. Thus, there are various short cuts!
 */
void Binder_InitFirstDomain(DomainMgr_st *dm_st, dcb_ro_t *rop, dcb_rw_t *rwp)
{
    PerDomain_st *bst;
    Client_t     *bcl;
    
    /* 
    ** Init bst, bcl, etc: we don't actually use the server_t or 
    ** client_t bits since we will be invoking the binder directly.
    ** However we \emph{MUST} intialise at least some of the server 
    ** and client bits so that we can recognise 'direct communication'
    ** from the Nemesis domain later on (in partic. in Binder$Close).
    */
       
    bst = ROP_TO_PDS(rop);
    memset(bst, 0, sizeof(*bst));

    bcl = (Client_t *)&(rwp->binder_rw);    
    memset(bcl, 0, sizeof(*bcl));

    TRC(eprintf("Binder_InitFirstDomain: st=%x dm_st->dom_tbl=%x bst=%x \n",
		 dm_st, dm_st->dom_tbl, bst));
    
    /* Create binder per-domain state */
    TRC(eprintf("Binder_InitFirstDomain: creating Binder per-domain state\n"));
    bst->rop      = rop;
    bst->rwp      = rwp;
    bst->dm_st    = dm_st;
    bst->refCount = 0;
    bst->callback = NULL;
    CL_INIT (bst->binder, &binder_ops, bst);
    rop->binder_offer = &bst->offer_cl;
    
    /* Squirrel binder closure for DM domain */
    dm_st->binder = &bst->binder;

    /* the first domain will be put in the table mapping domain ID's to
       DCBRO's in DomainMgrMod_Done. */

    TRC(eprintf("Binder_InitFirstDomain: done.\n"));
}

/*
 * This routine is called by the domain manager to complete initialisation
 * of the first domain.
 */
void Binder_Done(DomainMgr_st *dm_st)
{
    PerDomain_st *bst = ROP_TO_PDS(dm_st->dm_dom);
    Closure_clp cl;

    bst->callback = dm_st->bcb;
    TRC(eprintf ("Binder_Done: bst=%x callback=%x.\n", bst, bst->callback));
    cl = Entry$Closure (Pvs(entry));
    Threads$Fork (Pvs(thds), cl->op->Apply, cl, 0);
    Threads$Fork (Pvs(thds), cl->op->Apply, cl, 0);
#if 0
    Threads$Fork (Pvs(thds), cl->op->Apply, cl, 0);
#endif /* 0 */
    TRC(eprintf ("Binder_Done: forked server thread\n"));
}

/*
 * Do what the Binder needs to do before a domain goes away
 */
void Binder_FreeDomain(DomainMgr_st *dm_st, dcb_ro_t *rop, dcb_rw_t *rwp)
{
    PerDomain_st *bst = ROP_TO_PDS(rop);
    VP_clp        vp = &rop->vp_cl;
    Channel_EP    ep;

    TRC (eprintf ("Binder_FreeDomain: %qx\n", rop->id));

    /* XXX Needs more thought here. */
    for (ep = 0; ep < VP$NumChannels(vp); ep++) {
	Plumber$Disconnect (Plumber, bst->rop, ep);
    }

}



#ifdef CONFIG_BINDER_MUX
IDCOffer_clp Binder_MuxedOffer(DomainMgr_st *dm_st, dcb_ro_t *rop, 
			       dcb_rw_t *rwp , Type_Any *server)
{
    PerDomain_st  *bst =  ROP_TO_PDS(rop);
    Client_t      *bcl = (Client_t *)&(rwp->binder_rw);
    _generic_cl   *client_ops;
    Server_t      *new_server; 
    Client_t      *new_client; 
    IDCOffer_cl   *offer; 
    IDCStubs_Info  NOCLOBBER stubinfo;
#define TC_STRING_LEN (2*sizeof(Type_Code))
    char      tname[TC_STRING_LEN+1];

    TRY	{
	Context_clp   stub_ctxt = NAME_FIND (IDC_CONTEXT, Context_clp);
	TRC(eprintf ("Binder_MuxedOffer: got stub context.\n"));

	sprintf(tname, "%Q", server->type);
	TRC(eprintf ("Binder_MuxedOffer: looking for '%s'.\n", tname));

	stubinfo = NAME_LOOKUP (tname, stub_ctxt, IDCStubs_Info);
    } CATCH_ALL {
	
	eprintf("Binder_MuxedOffer: failed to find stub info!\n");
	RERAISE;
    }
    ENDTRY;

    /* Sort out the client side first */
    new_client = Heap$Calloc(Pvs(heap), 1, sizeof(Client_t));
    offer      = Heap$Calloc(Pvs(heap), 1, sizeof(IDCOffer_cl));
    
    client_ops = ((_generic_cl *)(stubinfo->surr.val))->op;

    CL_INIT(new_client->client_cl,   client_ops,             new_client);
    CL_INIT(new_client->binding_cl, &BinderClientBinding_op, new_client);
    
    ANY_INITC(&new_client->cs.srgt, server->type, 
	      (pointerval_t)&new_client->client_cl);
    new_client->cs.binding = &new_client->binding_cl;
    new_client->cs.marshal = dm_st->shmidc;
    
    new_client->conn      = bcl->conn;
    
    if(stubinfo->clnt) {
	/* Let the stubs set up any required run-time state */
	IDCClientStubs$InitState(stubinfo->clnt, &new_client->cs);
    }

    /* Now sort out the server side */
    new_server = Heap$Calloc(Pvs(heap), 1, sizeof(Server_t));
    
    new_server->ss.service = (addr_t)(pointerval_t)server->val;
    new_server->ss.binding = &new_server->binding_cl;
    new_server->ss.marshal = dm_st->shmidc;
    
    CL_INIT(new_server->cntrl_cl, stubinfo->stub->op, new_server);
    CL_INIT(new_server->binding_cl, &ShmServerBinding_op, new_server);
    
    new_server->conn  = &bst->sconn;
    
    /* Hijack offer field for PerDomain_st */
    new_server->offer = (Offer_t *)bst;

    new_client->offer = (Offer_t *)RegisterServer(&bst->mux, 
						  &new_server->cntrl_cl);
    if(new_client->offer == (Offer_t *)-1) {
	eprintf("Binder_MuxedOffer: adding new server to mux svr failed!\n");
	ntsc_halt();
    }

    
    /* We use our special binder offer ops, as for the binder binding. 
       Note that this means we cannot use ObjectTbl$Import() to deal 
       with this offer - we must call IDCOffer$Bind ourselves. */
    CLP_INIT(offer, &offer_ops, new_client);

    return offer; 
}
#endif /* CONFIG_BINDER_MUX */

/*-----------------------------Client Binder offer operations ----*/

/*
 * When a domain starts up most of the state required for a valid
 * surrogate to the binder is already created, including event
 * channels, but excluding anything to do with the threads
 * package.  The client receives a IDCOffer to connect to the Binder,
 * which executes purely locally to create the required thread
 * state.  The Binder itself is already expecting the IDC connection to
 * be working.  No synchonisation is therefore required.  The following
 * procedures are the operation on the binder interface.
 */

/*
 * Return the type of the binder offer
 */
static Type_Code OfferType_m(IDCOffer_cl *self)
{
    return Binder_clp__code;
}


/*
 * Return the protection domain we're in: not implemented.
 */
static ProtectionDomain_ID OfferPDID_m(IDCOffer_cl *self)
{
    return NULL_PDID;
}


static IDCOffer_IORList *OfferGetIORs_m(IDCOffer_clp self) {

    /* What the hell are you passing your binder offer over an IDC
       channel for? Out Out!! */

    ntsc_dbgstop();
    return (IDCOffer_IORList *)NULL;
}

/* 
 * This method is called by the client to set up a binding to the
 * Binder. Most of the state of the Client_t has been created by the
 * routines above when the domain is started, so only the pervasive heap
 * and synchronisation primitives need to be added.
 *
 * If we are using the binder multiplexed IDC, then this method
 * will be invoked a number of times by the same domain, once for 
 * each service we've multiplexed. The idempotency of the operation, 
 * then, is rather useful.
 */
static IDCClientBinding_clp OfferBind_m(IDCOffer_cl *self, 
					Gatekeeper_clp gk, 
					Type_Any *cl)
{
    Client_t	      *st = self->st; /* dcb_rw->binder_rw */
    ShmConnection_t *conn = st->conn;

    CTRC(eprintf("BinderOffer$Bind: called.\n"));

    /* This operation should be idempotent */
    if (conn->evs.tx == NULL_EVENT)
    {
	conn->txbuf.heap = Pvs(heap);
	conn->rxbuf.heap = Pvs(heap);
    
	TRY
	{
	    MU_INIT(&conn->mu);
	    conn->evs.tx = EC_NEW();
	    conn->evs.rx = EC_NEW();
	    Events$AttachPair (Pvs(evs), &conn->evs, &conn->eps);
	    /* 
	    ** XXX SMH The above attach causes an event to be sent
	    ** to the Binder domain (i.e. the Nemesis domain), so we
	    ** should wake it up to enable its Entry to be notified
	    ** about it (otherwise we lose the event).  However I
	    ** cannot see a nice way of doing this, so for now just
	    ** block for a while. 
	    */
	    /* PAUSE(SECONDS(1)); */
	}
	CATCH_ALL
	{
	    eprintf(
		"BinderOffer$Bind: failure. This domain cannot continue.\n");

	    Events$FreeChannel (Pvs(evs), conn->eps.tx);
	    Events$FreeChannel (Pvs(evs), conn->eps.rx);
	    conn->evs.tx = NULL_EVENT;
	    conn->evs.rx = NULL_EVENT;
	    RERAISE;
	}
	ENDTRY;
    }
  
    ANY_COPY (cl, &st->cs.srgt);
    CTRC(eprintf("BinderOffer$Bind: done.\n"));

    return &st->binding_cl;
}

/*------------------------------------------------- Binder Entry Points ---*/

/*
 * The full monty. 
 */
static void ConnectTo_m (Binder_cl		*self,
			 Binder_ID		 id /* IN */,
			 Binder_Port		 port /* IN */,
			 const Channel_Pairs 	*clt_eps /* IN */,
			 const Binder_Cookies	*clt_cks /* IN */,
			 Binder_Cookies		*svr_cks /* OUT */ )
{
    PerDomain_st		 *bst       = (PerDomain_st *) self->st;
    PerDomain_st 		 *to_bst    = NULL;
    BinderCallback_clp	  NOCLOBBER cb        = NULL;
    Channel_Pairs		  svr_eps;
    Channel_Pair *NOCLOBBER from_pair = NULL;
    Channel_Pair *NOCLOBBER to_pair   = NULL;

    TRC (eprintf ("Binder$ConnectTo: from dom=%llx to dom=%llx\n",
		  bst->rop->id, id));

    svr_eps.base = NULL;

    SEQ_INIT_FIXED(svr_cks, NULL, 0);

    IncDomRef (bst->dm_st, id, &to_bst);

    TRY
    {
	if (! (cb = to_bst->callback)) {
	    eprintf("Binder$ConnectTo: Domain %qx has no callback\n", id);
	    ntsc_halt();
	    RAISE_Binder$Error (Binder_Problem_ServerRefused);
	}

	TRC (eprintf ("Binder$ConnectTo: server registered: cb=%x\n", cb));

	TRY
	{
	    /* Get the channels and cookies from the server */
	    BinderCallback$Request (cb,
				    bst->rop->id,
				    bst->rop->pdid,
				    port,
				    clt_cks,
				    &svr_eps,
				    svr_cks);
	    TRC (eprintf ("Binder$ConnectTo: callback returns\n"));

	    /* Check the server returned the correct number of end-points */
	    if (SEQ_LEN (clt_eps) != SEQ_LEN (&svr_eps)) {
		eprintf(
		    "Binder$Connect: bad no. of EPs returned from server.\n");
		RAISE_Binder$Error (Binder_Problem_ServerError);
	    }

	    /* Connect each pair of end-points */

	    for (from_pair = SEQ_START (clt_eps), 
		     to_pair = SEQ_START (&svr_eps);
		 from_pair < SEQ_END (clt_eps);
		 from_pair++, to_pair++)
	    {
		TRC (eprintf ("Binder$ConnectTo: (%x,%x)->(%x,%x)\n",
			      bst->rop, from_pair->tx, 
			      to_bst->rop, to_pair->rx ));

		if (! Plumber$Connect (Plumber, bst->rop, to_bst->rop,
				       from_pair->tx, to_pair->rx)) 
		{
		    eprintf("Binder$ConnectTo: forward failed "
			    "(%x,%x)->(%x,%x)\n",
			    bst->rop, from_pair->tx,
			    to_bst->rop, to_pair->rx);
		    RAISE_Binder$Error (Binder_Problem_Failure);
		}
	
		TRC (eprintf ("Binder$ConnectTo: (%x,%x)<-(%x,%x)\n",
			      bst->rop, from_pair->rx, 
			      to_bst->rop, to_pair->tx ));

		if (! Plumber$Connect (Plumber, to_bst->rop, bst->rop,
				       to_pair->tx, from_pair->rx ) ) 
		{
		    eprintf("Binder$ConnectTo: back failed "
			    "(%x,%x)<-(%x,%x)\n",
			    bst->rop, from_pair->rx,
			    to_bst->rop, to_pair->tx);
		    RAISE_Binder$Error (Binder_Problem_Failure);
		}
	    }
	}
	CATCH_ALL
	{
	    for (; from_pair >= SEQ_START (clt_eps); from_pair--, to_pair--) 
	    {
		Plumber$Disconnect (Plumber, bst->rop, from_pair->tx);
		Plumber$Disconnect (Plumber, bst->rop, from_pair->rx);
		Plumber$Disconnect (Plumber, to_bst->rop, to_pair->tx);
		Plumber$Disconnect (Plumber, to_bst->rop, to_pair->rx);
	    }
	    RERAISE;
	}
	ENDTRY;
    }
    FINALLY
    {
	SEQ_FREE_DATA (&svr_eps);
	DecDomRef (bst->dm_st, to_bst);
    }
    ENDTRY;
}

/*
 * The simpler version for simpler connections.
 */
static void SimpleConnect_m (Binder_cl		*self,
			     Binder_ID		 id /* IN */,
			     Binder_Port		 port /* IN */,
			     const Channel_Pair 	*clt_eps /* IN */,
			     const Binder_Cookie	*clt_ck	/* IN */,
			     Binder_Cookie		*svr_ck	/* OUT */)
{
    PerDomain_st		*bst    = (PerDomain_st *) self->st;
    PerDomain_st		*to_bst = NULL;
    BinderCallback_clp	cb      = NULL;
    Channel_Pair		svr_eps;

    TRC (eprintf ("Binder$SimpleConnect: from dom=%llx to dom=%llx\n",
		  bst->rop->id, id));

    IncDomRef (bst->dm_st, id, &to_bst);

    TRY
    {
	if (! (cb = to_bst->callback)) {
	    eprintf("Binder$SimpleConnect: Domain %qx has no callback\n", id);
	    RAISE_Binder$Error (Binder_Problem_ServerRefused);
	}

	TRC (eprintf ("Binder$SimpleConnect: server registered: cb=%x\n", cb));

	TRY
	{
	    /* Get the channels and cookies from the server */
	    BinderCallback$SimpleRequest (cb,
					  bst->rop->id,
					  bst->rop->pdid,
					  port,
					  clt_ck,
					  &svr_eps,
					  svr_ck);

	    TRC (eprintf ("Binder$SimpleConnect: callback returns\n"));

	    /* Connect the pairs of end-points */

	    TRC (eprintf ("Binder$SimpleConnect: (%x,%x)->(%x,%x)\n",
			  bst->rop, clt_eps->tx, to_bst->rop, svr_eps.rx ));

	    if (! Plumber$Connect (Plumber, bst->rop, to_bst->rop,
				   clt_eps->tx, svr_eps.rx)) 
	    {
		eprintf("Binder$SimpleConnect: forward failed "
			"(%x,%x)->(%x,%x)\n",
			bst->rop, clt_eps->tx,
			to_bst->rop, svr_eps.rx);
		RAISE_Binder$Error (Binder_Problem_Failure);
	    }
	  
	    TRC (eprintf ("Binder$SimpleConnect: (%x,%x)<-(%x,%x)\n",
			  bst->rop, clt_eps->rx, to_bst->rop, svr_eps.tx ));

	    if (! Plumber$Connect (Plumber, to_bst->rop, bst->rop,
				   svr_eps.tx, clt_eps->rx)) 
	    {
		eprintf("Binder$SimpleConnect: back failed "
			"(%x,%x)<-(%x,%x)\n",
			bst->rop, clt_eps->rx, to_bst->rop, svr_eps.tx);
		RAISE_Binder$Error( Binder_Problem_Failure );
	    }
	}
	CATCH_ALL
	{
	    Plumber$Disconnect (Plumber, bst->rop, clt_eps->tx);
	    Plumber$Disconnect (Plumber, bst->rop, clt_eps->rx);
	    Plumber$Disconnect (Plumber, to_bst->rop, svr_eps.tx);
	    Plumber$Disconnect (Plumber, to_bst->rop, svr_eps.rx);
	    eprintf("Binder$SimpleConnect: caught exception '%s'\n", 
		    exn_ctx.cur_exception);
	    eprintf("RA was %p\n", RA());
	    ntsc_halt();
	    RERAISE;
	}
	ENDTRY;
    }
    FINALLY
    {
	DecDomRef (bst->dm_st, to_bst);
    }
    ENDTRY;
}



static void Close_m (Binder_cl *self,
		     Channel_EP ep		/* IN */ )
{
    PerDomain_st    *bst  = (PerDomain_st *)self->st;
    Client_t        *bcl  = (Client_t *)&(bst->rwp->binder_rw);
    ShmConnection_t *conn = bcl->conn;
    DomainMgr_st    *dmst = bst->dm_st;
    dcb_ro_t        *rop;

    /* Note: bcl->conn will be zero iff the caller is the Nemesis domain */
    if(bcl->conn && ep == conn->eps.tx) {

	/* Kill that wascally wabbit */
	if (LongCardTbl$Get (dmst->dom_tbl, bst->rop->id, (addr_t *)&rop)) {
	    TRC(eprintf("Binder$Close binder binding=> destroying "
			"domain 0x%04x\n",
			(uint32_t)(bst->rop->id & 0xFFFF)));
	    DomainMgr$Destroy(&bst->dm_cl, bst->rop->id);
	}
	
	RAISE_Channel$BadState(ep, Channel_State_Dead);
    } else if (ep != NULL_EP) {
	Plumber$Disconnect (Plumber, bst->rop, ep);
    }
}

static void 
CloseChannels_m(Binder_cl 		*self,
		const Channel_Pairs	*chans /* IN */ )
{
    PerDomain_st *bst = (PerDomain_st *) self->st;
    Channel_Pair *pair;

    eprintf("Close channels\n");
    for (pair = SEQ_START (chans); pair < SEQ_END (chans); pair++)
    {
	if (pair->tx != NULL_EP) 
	    Plumber$Disconnect (Plumber, bst->rop, pair->tx);
	if (pair->rx != NULL_EP) 
	    Plumber$Disconnect (Plumber, bst->rop, pair->rx);
    }
}


/*
 * To register the domain we need to create a valid surrogate locally
 * for the BinderCallback interface. We use the Client_t structure in
 * the dcb_ro's PerDomain_st (not the Client_t in the dcb_rw: that's for
 * the forward interface), and as with the forward interface fake out a
 * ShmTransport connection.  At the far end the Object Table is expected
 * to do the same.
 */
static void RegisterDomain_m (Binder_cl	*self,
			      const Channel_Pair	*eps, /* IN */
			      const Binder_Cookie	*tx_buf, /* IN  */
			      Binder_Cookie	*rx_buf	/* OUT */ )
{
    PerDomain_st       *bst        = (PerDomain_st *) self->st;
    Client_t	       *bcl        = &bst->c;
    dcb_ro_t	       *binder_rop = bst->dm_st->dm_dom;

    TRC (eprintf ("Binder$RegisterDomain: dom=%llx\n", bst->rop->id));

    /* Already registered ? */
    if (bst->callback) {
	eprintf("already registered!\n");
	ntsc_halt();
	RAISE_Binder$Error (Binder_Problem_ServerError);
    }

    /* Setup the BinderCallback client side connection */
    bcl->conn      = &bst->cconn;

    bcl->conn->eps.tx     = NULL_EP;
    bcl->conn->eps.rx     = NULL_EP;
    bcl->conn->evs.tx     = NULL_EVENT;
    bcl->conn->evs.rx     = NULL_EVENT;
    bcl->conn->txbuf.base = NULL;
    bcl->conn->txbuf.heap = NULL;
  
    TRY
    {
	IDCStubs_Info	   stubinfo = NAME_FIND(IDC_CONTEXT">BinderCallback", 
						IDCStubs_Info);
	BinderCallback_clp client_cl = NARROW ((Type_Any *)&stubinfo->surr,
					       BinderCallback_clp);

	CL_INIT(bcl->client_cl,   (addr_t) client_cl->op,         bcl);
	CL_INIT(bcl->binding_cl, &BinderCallbackClientBinding_op, bcl);
	
	ANY_INIT (&bcl->cs.srgt, BinderCallback_clp, &bcl->client_cl);
	bcl->cs.binding = &bcl->binding_cl;
	bcl->cs.marshal = bst->dm_st->shmidc;
	
	if(stubinfo->clnt) {
	    IDCClientStubs$InitState(stubinfo->clnt, &bcl->cs);
	}

	MU_INIT (&bcl->conn->mu);
    
	/* sort out the transmit buffer */
	bcl->conn->txbuf.heap = 
	    Gatekeeper$GetHeap (Pvs(gkpr), bst->rop->pdid, 0, 
				SET_ELEM(Stretch_Right_Read), True);
	
	bcl->conn->txsize      = stubinfo->c_sz;
	bcl->conn->txbuf.base  = Heap$Malloc (bcl->conn->txbuf.heap, 
					      bcl->conn->txsize);
	bcl->conn->txbuf.ptr   = bcl->conn->txbuf.base;
	bcl->conn->txbuf.space = bcl->conn->txsize;

	if (!bcl->conn->txbuf.base) {
	    eprintf("txbuf deathness.\n");
	    ntsc_halt();
	    RAISE_Binder$Error (Binder_Problem_Failure);
	}
	rx_buf->a = bcl->conn->txbuf.base;
	rx_buf->w = bcl->conn->txsize;

	/* Receive buffer rec */
	bcl->conn->rxsize      = tx_buf->w;
	bcl->conn->rxbuf.base  = tx_buf->a;
	bcl->conn->rxbuf.ptr   = bcl->conn->rxbuf.base;
	bcl->conn->rxbuf.space = bcl->conn->rxsize;
	bcl->conn->rxbuf.heap  = Pvs(heap);

	/* Call number */
	bcl->conn->call     = 0;
    
	/* Allocate event counts and channels */
	bcl->conn->evs.tx  = EC_NEW();
	bcl->conn->evs.rx  = EC_NEW();
	bcl->conn->eps.tx  = Events$AllocChannel (Pvs(evs));
	bcl->conn->eps.rx  = Events$AllocChannel (Pvs(evs));

	/* This binding didn't come from an offer. */
	bcl->offer = NULL;

	/* Get the channels */
	if (!Plumber$Connect(Plumber, binder_rop, bst->rop, 
			     bcl->conn->eps.tx, eps->rx)   || 
	    !Plumber$Connect(Plumber, bst->rop, binder_rop, 
			     eps->tx, bcl->conn->eps.rx))
	{
	    eprintf("Binder$RegisterDomain: plumber failed.\n");
	    ntsc_halt();
	    RAISE_Binder$Error (Binder_Problem_ServerError);
	}
    
	/* Attach our events */
	Events$AttachPair (Pvs(evs), &bcl->conn->evs, &bcl->conn->eps);
	
	/* And we're done. */
	bst->callback   = (BinderCallback_clp) &bcl->client_cl;
	bst->cb_binding = &bcl->binding_cl;
    }
    CATCH_ALL
    {
	if (bcl->conn->eps.tx != NULL_EP ) {
	    Plumber$Disconnect (Plumber, binder_rop, bcl->conn->eps.tx);
	    Events$FreeChannel  (Pvs(evs), bcl->conn->eps.tx);
	}
	if (bcl->conn->eps.rx != NULL_EP ) {
	    Plumber$Disconnect (Plumber, binder_rop, bcl->conn->eps.rx);
	    Events$FreeChannel  (Pvs(evs), bcl->conn->eps.rx);
	}
	
	if (bcl->conn->txbuf.base) FREE (bcl->conn->txbuf.base);
    
	if (bcl->conn->evs.rx != NULL_EVENT) EC_FREE (bcl->conn->evs.rx);
	if (bcl->conn->evs.tx != NULL_EVENT) EC_FREE (bcl->conn->evs.tx);
	ntsc_halt();
	RERAISE;
    }
    ENDTRY;

    return;
}

/************************************************************************
 * 
 * Internal binder utility routines
 *
 ************************************************************************/


/*---IDCClientBinding-methods----------------------------------------------*/

/*
 * Initialise the buffer for a client call
 */
static IDC_BufferDesc 
CltInitCall_m( IDCClientBinding_cl *self, word_t proc, string_t name )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t *p;

    CTRC(eprintf("IDCClientBinding$InitCall: st=%x ptr=%x proc=%x '%s'\n",
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

#ifdef CONFIG_BINDER_MUX
/*
 * Initialise the buffer for a client call
 */
static IDC_BufferDesc 
CltInitCallMUX_m( IDCClientBinding_cl *self, word_t proc, string_t name )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t   *p;

    CTRC(eprintf("IDCClientBinding$InitCall: conn=%x ptr=%x proc=%x '%s'\n",
		conn, conn->txbuf.base, proc, name));

    /* Lock this channel */
    MU_LOCK(&conn->mu);

    /* Marshal binding id and procedure number */
    p = conn->txbuf.base;

    /*
    ** XXX SMH: normally the binding id should be found via the offer, 
    ** but in this case we've just hijacked the offer pointer itself
    ** and used it to hold the binding id. 
    */
    p[0] = (word_t)st->offer;        
    p[1] = proc;
    conn->txbuf.ptr   = p + 2;
    conn->txbuf.space = conn->txsize - (2 * sizeof(*p));
  
    /* return transmit buffer descriptor */
    return &conn->txbuf;
}
#endif /* CONFIG_BINDER_MUX */

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
  
    CTRC(eprintf("IDCClientBinding$SendCall: st=%x evs.tx=%x\n", 
		 st, conn->evs.tx));

    /* Kick the event count */
    EC_ADVANCE( conn->evs.tx, 1 );
}

/* Hack function to stop gcc optimising around the ntsc_dbgstop() call */
bool_t alwaysTrue(void)
{
    return True;
}

/* 
 * Wait for a packet to come back and set up the receive buffer. 
 * The string is not used.
 */
static word_t 
CltReceiveReply_m(IDCClientBinding_cl *self, IDC_BufferDesc *b, string_t *s)
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;
    word_t *p;

    CTRC(eprintf("IDCClientBinding$ReceiveReply: st=%x evs.rx=%x call=%x\n",
		st, conn->evs.rx, conn->call));

    TRY {
	/* Wait for the packet to come back. */
	Event_Val v;
	v=conn->call;
	conn->call = EC_AWAIT_UNTIL( conn->evs.rx, conn->call + 1,
				     NOW()+SECONDS(5));
	if (conn->call==v) {
	    eprintf("Binder deadlocked!\n");
	    if (alwaysTrue()) ntsc_dbgstop();
	    exit(42);
	}
    } CATCH_Thread$Alerted() {
	/* The server has shut down the binding */
	eprintf("BinderClientBinding$ReceiveReply: st=%x : binding closed\n", 
		st);
	RAISE_Thread$Alerted();
    } CATCH_ALL {
	eprintf("BinderClientBinding$ReceiveReply: st=%x : exception %s\n",
		st, exn_ctx.cur_exception);
	RAISE_IDC$Failure();
    }
    ENDTRY;

    /* Unmarshal reply number */
    p = conn->rxbuf.base;
    conn->rxbuf.ptr   = p + 1;
    conn->rxbuf.space = conn->rxsize - sizeof(*p);
    *b = &conn->rxbuf;

    CTRC(eprintf("IDCClientBinding$ReceiveReply: st=%x got reply %x\n",
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
CltAckReceive_m(IDCClientBinding_cl *self, IDC_BufferDesc b)
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    CTRC(eprintf("IDCClientBinding$AckReply: st=%x b=%x\n", st, b));

    MU_RELEASE(&conn->mu);
}


/*
 * Destroy a binding 
 */
static void 
CltDestroyBinder_m( IDCClientBinding_cl *self )
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    /* Closing the binder binding is deemed to be suicide */
    Binder$Close (Pvs(bndr), conn->eps.tx);

    eprintf("NOTREACHED\n");
}

static void CltDestroyBinderCallback_m(IDCClientBinding_clp self) 
{
    Client_t        *st   = self->st;
    ShmConnection_t *conn = st->conn;

    Binder$Close (Pvs(bndr), conn->eps.rx);
    Binder$Close (Pvs(bndr), conn->eps.tx);

    MU_FREE (&conn->mu);
    FREE(conn->txbuf.base);
    Heap$Destroy(conn->txbuf.heap);

    EC_FREE (conn->evs.tx);	/* will free attached channel */
    EC_FREE (conn->evs.rx);	/* ditto */

}


/*
 * IDCServerBinding operations
 */



#ifdef CONFIG_BINDER_MUX
/* 
** If we're muxing things over a single connection, we need to 
** be a bit cunning when it comes to the underlying stubs (which
** know nothing about the muxing in their current incarnation). 
**
** If we're not, we can end up with a situation where the 
**     "while todo = IDCServerBinding$ReceiveCall(...)"
** in the dispatch routine of the stubs might notice a second
** event on the channel which is actually intended for use by
** another server, and get terribly confused. 
** NB: this race only occurs when Nemesis is descheduled after
** the stubs have done a IDCServerBinding$SendReply(...), and
** before Nemesis gets the processor back the same client domain
** manages to produce another call on one of the Binder muxed 
** IDC services (binder/stretch allocator/fstack allocator). 
** 
** To solve this problem for now, we arrange that SvrReceiveCall 
** only ever allows itself notice a single call per dispatch; 
** hence in the case of the race above we'll reenter the 
** entry, and thence to our MuxedDispatch routine, and only
** then get here (and perhaps in another server). 
** A nicer way would be to incorporate the use of muxed IDC
** into the standard ShmTransport, which SMH/PBM will get sorted
** out in the near future.
**/
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

    /* If it's been smoked, then let dispatch return happily */
    if(DCB_EPRW(RO(Pvs(vp)), conn->eps.tx)->state == Channel_State_Dead)
	return False;


    /* 
    ** SMH: nasty; we need to mark the fact we're 'in progress' so 
    ** that we can return after processing at most one operation. 
    ** This is so that we get to demultiplex again on a second one 
    ** [since there is no guarantee it is for the same server]. 
    ** For now, we hijack the rx event count as a marker, since 
    ** this is not used on the server side.
    */
    if(conn->evs.rx != NULL_EVENT)
	return False;

    conn->evs.rx = 0xf00f;   /* mark for next time */
    
    if (conn->call < VP$Poll (Pvs(vp), conn->eps.rx))
    {
	word_t *p;

	p  = conn->rxbuf.base;
	*b = &conn->rxbuf;

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
#else

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

    /* If it's been smoked, then let dispatch return happily */
    if(DCB_EPRW(RO(Pvs(vp)), conn->eps.tx)->state == Channel_State_Dead)
	return False;

    if (conn->call < VP$Poll (Pvs(vp), conn->eps.rx))
    {
	word_t *p;

	p  = conn->rxbuf.base;
	*b = &conn->rxbuf;

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
#endif

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

    conn->txbuf.ptr   = p;
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

    conn->txbuf.ptr   = p;
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

    /* 
    ** Kick the event count.  In the case that we're responding to a
    ** client who is suiciding, the below will cause an exception
    ** Channel$BadState(). We handle it here, since this method isn't
    ** allowed to raise Channel$BadState() 
    */

    TRY {
	EC_ADVANCE(conn->evs.tx, 1);
    } CATCH_Channel$BadState(UNUSED ep, UNUSED st) {
    } ENDTRY;
}

static Domain_ID SvrClient_m (IDCServerBinding_cl *self,
			      ProtectionDomain_ID *pdid )
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
    PerDomain_st    *bst  = (PerDomain_st *)st->offer;

    TRC(eprintf("Binder: IDCServerBinding$Destroy\n"));

    
    /* Make sure that the other end is stopped */
    if(bst->rop->schst != DCBST_K_DYING) {
	DomainMgr$Remove(&bst->dm_cl, bst->rop->id);
    }

    TRY {
	Entry$Unbind(Pvs(entry), conn->eps.rx);
	Plumber$Disconnect(Plumber, RO(Pvs(vp)), conn->eps.rx);	
	Events$FreeChannel(Pvs(evs), conn->eps.rx); 
	Plumber$Disconnect(Plumber, RO(Pvs(vp)), conn->eps.tx);	
	Events$FreeChannel(Pvs(evs), conn->eps.tx); 
    } 
    CATCH_ALL {
    }
    ENDTRY;

    MU_FREE (&conn->mu);		/* XXX - unused so far! */

    /* Release our claim on the rop/rwp */
    DecDomRef(bst->dm_st, bst);
}





/*
 * End binder.c
 */

