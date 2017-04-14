/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/customstubidc
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Allow standard IDC to be wrapped in custom client stubs
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: csidc.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <IDCMacros.h>
#include <stdio.h>
#include <CSIDCTransport.ih>
#include <contexts.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static  CSIDCTransport_Offer_fn         CSIDCTransport_Offer_m;

static  CSIDCTransport_op       transport_ms = {
    CSIDCTransport_Offer_m
};

static  IDCOffer_Type_fn                IDCOffer_Type_m;
static  IDCOffer_PDID_fn                IDCOffer_PDID_m;
static  IDCOffer_GetIORs_fn             IDCOffer_GetIORs_m;
static  IDCOffer_Bind_fn                IDCOffer_Bind_m;

static  IDCOffer_op     offer_ms = {
    IDCOffer_Type_m,
    IDCOffer_PDID_m,
    IDCOffer_GetIORs_m,
    IDCOffer_Bind_m
};

static  BinderCallback_Request_fn       IDCService_Request_m;
static  BinderCallback_SimpleRequest_fn IDCService_SimpleRequest_m;
static  IDCService_Destroy_fn           IDCService_Destroy_m;

static  IDCService_op   service_ms = {
    IDCService_Request_m,
    IDCService_SimpleRequest_m,
    IDCService_Destroy_m
};

static  IDCClientBinding_InitCall_fn          IDCClientBinding_InitCall_m;
static  IDCClientBinding_InitCast_fn          IDCClientBinding_InitCast_m;
static  IDCClientBinding_SendCall_fn          IDCClientBinding_SendCall_m;
static  IDCClientBinding_ReceiveReply_fn      IDCClientBinding_ReceiveReply_m;
static  IDCClientBinding_AckReceive_fn        IDCClientBinding_AckReceive_m;
static  IDCClientBinding_Destroy_fn           IDCClientBinding_Destroy_m;

static  IDCClientBinding_op     binding_ms = {
    IDCClientBinding_InitCall_m,
    IDCClientBinding_InitCast_m,
    IDCClientBinding_SendCall_m,
    IDCClientBinding_ReceiveReply_m,
    IDCClientBinding_AckReceive_m,
    IDCClientBinding_Destroy_m
};

static  const CSIDCTransport_cl       cl = { &transport_ms, NULL };

CL_EXPORT (CSIDCTransport, CSIDCTransport, cl);

/*-------------------------------------------------------- State -------*/

/* Server-side state */
typedef struct {
    IDCOffer_cl  offer;        /* This offer */
    IDCService_cl service;
    
    Type_Code clientcode;
    CSClientStubMod_clp stubmod;
    Type_Any server; /* The server; used for local binds */

    IDCOffer_clp serveroffer;  /* The underlying offer */
    IDCService_clp serverservice;
} CSIDCTransport_st;

/* Client-side state, per binding */
typedef struct {
    /* The client stub side of things */
    Type_Any clientstubs;
    CSClientStubMod_clp stubmod; /* Used for delete */
    IDCClientBinding_cl binding; 
    IDCClientBinding_clp server_binding;
} IDCClientBinding_st;

/*------------------------------------------ Transport Entry Points ----*/

static IDCOffer_clp CSIDCTransport_Offer_m (
    CSIDCTransport_cl       *self,
    const Type_Any  *server /* IN */,
    Type_Code       cstype  /* IN */,
    CSClientStubMod_clp     csmod   /* IN */,
    IDCCallback_clp scb     /* IN */,
    Heap_clp        heap    /* IN */,
    Gatekeeper_clp  gk      /* IN */,
    Entry_clp       entry   /* IN */
    /* RETURNS */,
    IDCService_clp  *service )
{
    CSIDCTransport_st     *st = self->st;
    IDCTransport_clp shmt;
    Type_Any *local;

    TRC(printf("CSIDC: Offer clientstubs %p\n",csmod));

    shmt=NAME_FIND("modules>ShmTransport",IDCTransport_clp);

    /* Allocate state */
    st = Heap$Malloc(heap, sizeof(*st));

    /* Fill in info */
    st->clientcode=cstype;
    st->stubmod=csmod;
    ANY_COPY(&st->server,server);

    /* Create the real service */
    st->serveroffer=IDCTransport$Offer(shmt, server, scb, heap, gk, entry,
				       &st->serverservice);

    TRC(printf("CSIDC: Internal offer at %p\n",st->serveroffer));

    /* Now fill in our own fields */
    CL_INIT(st->offer, &offer_ms, st);
    CL_INIT(st->service, &service_ms, st);

    TRC(printf("CSIDC: External offer at %p\n",&st->offer));

    /* We should put our offer into the object table, together with the
       result of a local bind. */
    local=CSClientStubMod$New(st->stubmod, &st->server);
    ObjectTbl$Export(Pvs(objs), &st->service, &st->offer, local);
    FREE(local);
    
    /* Return stuff */
    *service=&st->service;
    return &st->offer;
}

/*------------------------------------------- IDCOffer Entry Points ----*/

static Type_Code IDCOffer_Type_m (
    IDCOffer_cl     *self )
{
    CSIDCTransport_st     *st = self->st;

    TRC(printf("CSIDC: we were asked for our typecode\n"));

    return st->clientcode;
}

static ProtectionDomain_ID IDCOffer_PDID_m (
    IDCOffer_cl     *self )
{
    CSIDCTransport_st     *st = self->st;

    TRC(printf("CSIDC: we were asked for our pdom\n"));

    return IDCOffer$PDID(st->serveroffer);
}

static IDCOffer_IORList *IDCOffer_GetIORs_m (
    IDCOffer_cl     *self )
{
    CSIDCTransport_st     *st = self->st;

    TRC(printf("CSIDC: we were asked for our IORs\n"));

    /* XXX XXX THIS IS NOT THE RIGHT THING TO DO, but hey, it'll be fun. */
    return IDCOffer$GetIORs(st->serveroffer);
}

static IDCClientBinding_clp IDCOffer_Bind_m (
    IDCOffer_cl     *self,
    Gatekeeper_clp  gk      /* IN */,
    Type_Any        *cl     /* OUT */ )
{
    CSIDCTransport_st     *st = self->st;
    IDCClientBinding_st *binding;
    Type_Any server;

    TRC(printf("CSIDC: about to bind\n"));

    /* Right. Instantiate the client stubs, passing in the bind parameters.
       Return whatever they return. Trust them... */

    binding=Heap$Malloc(Pvs(heap), sizeof(*binding)); /* XXX which heap? */
    binding->stubmod=st->stubmod;
    CL_INIT(binding->binding, &binding_ms, binding);

    TRC(printf("CSIDC: client stubmod is %p, binding state at %p\n",
	       binding->stubmod, binding));

    binding->server_binding=IDCOffer$Bind(st->serveroffer,gk,&server);

    binding->clientstubs=
	*CSClientStubMod$New(binding->stubmod, &server);

    *cl=binding->clientstubs;

    return &binding->binding;
}

/*----------------------------------------- IDCService Entry Points ----*/

/* The IDCService interface (along with many of the other IDC
   interfaces) should really be split apart. This will happen in the
   Great IDC Cleanup (to be announced). In the meantime we have a lot
   of unused methods. */

static void IDCService_Request_m (
    BinderCallback_cl   *self,
    Domain_ID       client  /* IN */,
    ProtectionDomain_ID  pdom    /* IN */,
    Binder_Port     port    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Channel_Pairs   *chans  /* OUT */,
    Binder_Cookies  *svr_cks        /* OUT */ )
{
}

static void IDCService_SimpleRequest_m (
    BinderCallback_cl   *self,
    Domain_ID       client  /* IN */,
    ProtectionDomain_ID    pdom    /* IN */,
    Binder_Port     port    /* IN */,
    const Binder_Cookie     *clt_ck /* IN */,
    Channel_Pair    *svr_eps        /* OUT */,
    Binder_Cookie   *svr_ck /* OUT */ )
{
}

static void IDCService_Destroy_m (
    IDCService_cl   *self )
{
    CSIDCTransport_st     *st = self->st;

    /* Delete everything */
    IDCService$Destroy(st->serverservice);
    FREE(st);

    /* Is that it? */
    /* XXX no, delete the local stubs too */
}

/*----------------------------------- IDCClientBinding Entry Points ----*/

static IDC_BufferDesc IDCClientBinding_InitCall_m (
    IDCClientBinding_cl     *self,
    word_t  proc    /* IN */,
    string_t        name    /* IN */ )
{
    return 0;
}

static IDC_BufferDesc IDCClientBinding_InitCast_m (
    IDCClientBinding_cl     *self,
    word_t  ann     /* IN */,
    string_t        name    /* IN */ )
{
    return 0;
}

static void IDCClientBinding_SendCall_m (
    IDCClientBinding_cl     *self,
    IDC_BufferDesc  b       /* IN */ )
{
    return;
}

static word_t IDCClientBinding_ReceiveReply_m (
    IDCClientBinding_cl     *self
    /* RETURNS */,
    IDC_BufferDesc  *b,
    string_t        *name )
{
    return 0;
}

static void IDCClientBinding_AckReceive_m (
    IDCClientBinding_cl     *self,
    IDC_BufferDesc  b       /* IN */ )
{
}

static void IDCClientBinding_Destroy_m (
    IDCClientBinding_cl     *self )
{
    IDCClientBinding_st   *st = self->st;

    TRC(printf("csidc: about to delete client binding\n"));
    /* Delete everything */
    CSClientStubMod$Destroy(st->stubmod, &st->clientstubs);
    IDCClientBinding$Destroy(st->server_binding);
    FREE(st);
}

/*
 * End 
 */
