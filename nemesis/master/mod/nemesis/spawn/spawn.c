/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	spawn.c
**
** FUNCTIONAL DESCRIPTION:
**
**	Invoke operations in new domains
**
** ENVIRONMENT: 
**
**	User space
**
** ID : $Id: spawn.c 1.2 Tue, 01 Jun 1999 14:52:06 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <IDCMacros.h>
#include <mutex.h>
#include <stdio.h>
#include <string.h>

#include <SpawnMod.ih>
#include <Spawn.ih>
#include <SpawnReturn.ih>
#include <Context.ih>
#include <ProtectionDomain.ih>
#include <IDCStubs.ih>
#include <IDC.ih>
#include <Exec.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define BUFFER_SPACE 2048

extern IDC_clp SpawnIDC;

/* SpawnMod: our module interface */
static  SpawnMod_Create_fn              SpawnMod_Create_m;

static  SpawnMod_op     spawnmod_ms = {
    SpawnMod_Create_m
};

/* Spawn: caller's handle on us */
static  Spawn_GetStub_fn                Spawn_GetStub_m;
static  Spawn_SetEnv_fn                 Spawn_SetEnv_m;
static  Spawn_SetPDID_fn                Spawn_SetPDID_m;
static  Spawn_SetName_fn                Spawn_SetName_m;
static  Spawn_Destroy_fn		Spawn_Destroy_m;

static  Spawn_op        spawn_ms = {
    Spawn_GetStub_m,
    Spawn_SetEnv_m,
    Spawn_SetPDID_m,
    Spawn_SetName_m,
    Spawn_Destroy_m
};

/* IDCClientBinding: client stub's handle on us */
static  IDCClientBinding_InitCall_fn     IDCClientBinding_InitCall_m;
static  IDCClientBinding_InitCast_fn     IDCClientBinding_InitCast_m;
static  IDCClientBinding_SendCall_fn     IDCClientBinding_SendCall_m;
static  IDCClientBinding_ReceiveReply_fn IDCClientBinding_ReceiveReply_m;
static  IDCClientBinding_AckReceive_fn   IDCClientBinding_AckReceive_m;
static  IDCClientBinding_Destroy_fn      IDCClientBinding_Destroy_m;

static  IDCClientBinding_op     clientbinding_ms = {
    IDCClientBinding_InitCall_m,
    IDCClientBinding_InitCast_m,
    IDCClientBinding_SendCall_m,
    IDCClientBinding_ReceiveReply_m,
    IDCClientBinding_AckReceive_m,
    IDCClientBinding_Destroy_m
};

/* SpawnReturn: server to collect return value from child */
static  SpawnReturn_Return_fn           SpawnReturn_Return_m;

static  SpawnReturn_op  return_ms = {
  SpawnReturn_Return_m
};

/* IDCCallback: used to make sure only our child connects */
static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  callback_ms = {
  IDCCallback_Request_m,
  IDCCallback_Bound_m,
  IDCCallback_Closed_m
};

/* Closure: entry point for new domain */
static  Closure_Apply_fn                Closure_Apply_m;

static  Closure_op      newdom_ms = {
    Closure_Apply_m
};

/* IDCServerBinding: used by server stubs to unmarshal/marshal data */
static  IDCServerBinding_ReceiveCall_fn IDCServerBinding_ReceiveCall_m;
static  IDCServerBinding_AckReceive_fn  IDCServerBinding_AckReceive_m;
static  IDCServerBinding_InitReply_fn   IDCServerBinding_InitReply_m;
static  IDCServerBinding_InitExcept_fn  IDCServerBinding_InitExcept_m;
static  IDCServerBinding_SendReply_fn   IDCServerBinding_SendReply_m;
static  IDCServerBinding_Client_fn      IDCServerBinding_Client_m;
static  IDCServerBinding_Destroy_fn     IDCServerBinding_Destroy_m;

static  IDCServerBinding_op     serverbinding_ms = {
  IDCServerBinding_ReceiveCall_m,
  IDCServerBinding_AckReceive_m,
  IDCServerBinding_InitReply_m,
  IDCServerBinding_InitExcept_m,
  IDCServerBinding_SendReply_m,
  IDCServerBinding_Client_m,
  IDCServerBinding_Destroy_m
};

static  const SpawnMod_cl     cl = { &spawnmod_ms, NULL };

CL_EXPORT (SpawnMod, SpawnMod, cl);

/*------------------------------------------------------------ Data ----*/

/* Typecode string length */
#define TC_STRING_LEN (2*sizeof(Type_Code)+1)

typedef struct _generic_cl _generic_cl;
struct _generic_cl {
    addr_t	op;
    addr_t	st;
};

typedef struct {
    /* Things to do with the client-side stub XXX must go first */
    IDCClientStubs_State cs;
    _generic_cl surrogate_cl; /* Thing we construct for the client to call */
    Type_Any module; /* Module to be invoked in new domain */

    /* Us */
    Spawn_cl   spawn_cl;   /* Handle we pass back to the caller */
    IDCClientBinding_cl clientbinding_cl; /* Used by stubs to make call */
    IDCStubs_Info info;    /* Stubs of an appropriate type */

    /* Info used to start new domain */
    Heap_clp heap;	   /* Heap for general use */
    Context_clp env;	   /* Environment for new domain */
    ProtectionDomain_ID pdid; /* Pdid for new domain */
    string_t name;         /* Name of new domain */
    Exec_clp exec;

    /* Closure for new domain */
    Closure_cl newdom_cl;

    /* A buffer in which to marshal arguments */
    addr_t send_buffer;
    uint32_t send_buffer_size;
    IDC_BufferRec send_desc;

    /* IDC service to collect return values from started domains */
    SRCThread_Mutex mu;
    SRCThread_Condition cv;  /* Used to wait for reply */
    SpawnReturn_cl return_cl;
    Domain_ID child;         /* Our current child */
    IDCOffer_clp return_offer;
    IDCService_clp return_service;
    IDCCallback_cl return_callback;
    addr_t return_buffer;
    uint32_t return_buffer_size;
    IDC_BufferRec return_desc;

} Spawn_st;

/*------------------------------------------- SpawnMod Entry Points ----*/

static Spawn_clp SpawnMod_Create_m (
    SpawnMod_cl     *self,
    const Type_Any  *module /* IN */,
    Heap_clp        heap    /* IN */ )
{
    Spawn_st   *st;
    char name[TC_STRING_LEN];
    Context_clp stubs_ctx;
    Type_Any any;
    IDCStubs_Info info;
    addr_t surrogate_ops;
    Exec_clp exec;
    IDCTransport_clp shmt;
    

    TRC(printf("SpawnMod_Create: entered, typecode %qx\n",module->type));

    /* Find useful modules */
    exec = NAME_FIND("modules>Exec", Exec_clp);
    shmt = NAME_FIND("modules>ShmTransport", IDCTransport_clp);

    stubs_ctx = NAME_FIND("stubs", Context_clp);

    /* Find stubs */
    sprintf(name,"%Q",module->type);

    if (!Context$Get(stubs_ctx, name, &any)) {
	fprintf(stderr,"SpawnMod_Create: couldn't find stubs '%s'\n",name);
	RAISE_SpawnMod$Failure();
	return NULL;
    }
    info=NARROW(&any, IDCStubs_Info);

    /* At this point we have everything we need, so we can go ahead */
    st = Heap$Malloc(heap, sizeof(*st));

    /* Initialise closures:
     * - spawn              is the handle we return to the caller
     * - the client binding is used by the stubs to make calls
     * - the return server  is used by the child domain to return a reply
     * - the surrogate      is used to make the call; the ops come from the
     *                      stubs, the state is created by us
     * - the server stubs   used to receive the call; the operation is stub
     *                      code, the state is set up by us
     */
    CL_INIT(st->spawn_cl, &spawn_ms, st);
    CL_INIT(st->clientbinding_cl, &clientbinding_ms, st);
    surrogate_ops = ((_generic_cl *)(info->surr.val))->op;
    /* NOTE: our state record starts with an IDCClientStubs_State record,
       which is used by the stubs. */
    CL_INIT(st->surrogate_cl, surrogate_ops, st);
    CL_INIT(st->newdom_cl, &newdom_ms, st);

    /* All the other fields */
    st->module = *module;
    st->info = info;
    st->heap = heap;
    st->env  = NULL;
    st->pdid = NULL_PDID;
    st->name = NULL;

    st->send_buffer=NULL;
    st->send_buffer_size=0;

    st->exec = exec;
    st->child = -1;

    /* Tell the stubs what to do */
    st->cs.binding = &st->clientbinding_cl;
    st->cs.marshal = SpawnIDC;
    if (info->clnt) {
	IDCClientStubs$InitState(info->clnt, &st->cs);
    } else {
	printf("Spawn: warning: no IDCClientStubs_clp\n");
    }

    ANY_INITC(&st->cs.srgt,  module->type, 
	      (pointerval_t)&st->surrogate_cl);

    /* Export a service to pick up the results */
    MU_INIT(&st->mu);
    CV_INIT(&st->cv);
    CL_INIT(st->return_cl, &return_ms, st);
    CL_INIT(st->return_callback, &callback_ms, st);
    ANY_INIT(&any, SpawnReturn_clp, &st->return_cl);
    st->return_offer=IDCTransport$Offer(shmt, &any, &st->return_callback,
					st->heap, Pvs(gkpr), Pvs(entry),
					&st->return_service);

    return &st->spawn_cl;
}

/*---------------------------------------------- Spawn Entry Points ----*/

static Type_Any *Spawn_GetStub_m (
    Spawn_cl        *self )
{
    Spawn_st      *st = self->st;
    Type_Any *ret;

    ret=Heap$Malloc(st->heap, sizeof(*ret));
    ANY_INITC(ret, st->module.type, (word_t)&st->surrogate_cl);

    return ret;
}

static void Spawn_SetEnv_m (
    Spawn_cl        *self,
    Context_clp     env     /* IN */ )
{
    Spawn_st      *st = self->st;

    st->env = env;
}

static void Spawn_SetPDID_m (
    Spawn_cl        *self,
    ProtectionDomain_ID    pdid   /* IN */ )
{
    Spawn_st      *st = self->st;

    st->pdid = pdid;
}

static void Spawn_SetName_m (
    Spawn_cl        *self,
    string_t        name    /* IN */ )
{
    Spawn_st      *st = self->st;

    if (st->name) FREE(st->name);
    if (name)
	st->name=strduph(name, st->heap);
    else st->name=NULL;
}

static void Spawn_Destroy_m (
    Spawn_cl        *self )
{
    Spawn_st *st = self->st;

    /* XXX; this code has a nasty race condition; don't use it
     */
#if 0
    TRC(printf("Spawn: Destroy %p\n",st));

    /* Withdraw the return service */
    IDCService$Destroy(st->return_service);

    /* Free stubs state */
    if (st->info->clnt) {
	IDCClientStubs$DestroyState(st->info->clnt, &st->cs);
    }

    if (st->name) FREE(st->name);

    MU_FREE(&st->mu);
    CV_FREE(&st->cv);

    FREE(st);
#endif
}

/*----------------------------------- IDCClientBinding Entry Points ----*/

static IDC_BufferDesc IDCClientBinding_InitCall_m (
    IDCClientBinding_cl     *self,
    word_t  proc    /* IN */,
    string_t        name    /* IN */ )
{
    Spawn_st   *st = self->st;
    IDC_BufferDesc b;

    TRC(printf("Spawn: InitCall %u %s\n",proc,name));

    /* XXX the buffer must be allocated in space readable from the new pdom -
       perhaps we should create the new pdom now if necessary */
    /* XXX actually, all of our state should be readable by the child */
    b=&st->send_desc;
    st->send_buffer=Heap$Malloc(st->heap, BUFFER_SPACE);
    st->send_buffer_size=BUFFER_SPACE;
    b->base=st->send_buffer;
    b->ptr=b->base;
    b->space=st->send_buffer_size;
    b->heap=st->heap;

    /* Store header information in the buffer */
    *((word_t *)(b->ptr))++ = proc;
    b->space-=sizeof(word_t);

    return b;
}

static IDC_BufferDesc IDCClientBinding_InitCast_m (
    IDCClientBinding_cl     *self,
    word_t  ann     /* IN */,
    string_t        name    /* IN */ )
{
    /* Identical functionality for the moment; in the future we may want
       to skip the 'return' stage if it's an announcement */
    return IDCClientBinding_InitCall_m(self, ann, name);
}

static void IDCClientBinding_SendCall_m (
    IDCClientBinding_cl     *self,
    IDC_BufferDesc  b       /* IN */ )
{
    Spawn_st   *st = self->st;
    Type_Any any;

    TRC(printf("Spawn: SendCall %p\n",b));

    /* The hard work. Start the new domain, passing in what we
       have. Get it to bind to us to return the result (if it wasn't
       an announcement).  Store the result so that it can be picked up
       in ReceiveReply. */

    ANY_INIT(&any, Closure_clp, &st->newdom_cl);

    /* Grab the mutex on the receiver */
    MU_LOCK(&st->mu);

    /* Start the new domain */
    st->child =
	Exec$Run(st->exec, &any, st->env, st->pdid,
		 st->name?st->name:"spawned");
}

static word_t IDCClientBinding_ReceiveReply_m (
    IDCClientBinding_cl     *self
    /* RETURNS */,
    IDC_BufferDesc  *b,
    string_t        *name )
{
    Spawn_st   *st = self->st;
    word_t reply;

    TRC(printf("Spawn: ReceiveReply\n"));

    /* Wait for a result from the new domain, and return it:
       Wait for a signal from the receiver service */
    WAIT(&st->mu, &st->cv);
    
    st->return_desc.base=st->return_buffer;
    st->return_desc.ptr=st->return_desc.base;
    st->return_desc.space=st->return_buffer_size;
    st->return_desc.heap=st->heap;

    /* Unmarshal reply number */
    reply=*((word_t *)(st->return_desc.ptr))++;
    st->return_desc.space-=sizeof(word_t);
    
    /* Name? Notused, apparently. Probably MUST be NULL. */
    if (name) *name=NULL;

    TRC(printf("Spawn: SendCall: received reply %u\n",reply));

    *b=&st->return_desc;
    return reply;
}

static void IDCClientBinding_AckReceive_m (
    IDCClientBinding_cl     *self,
    IDC_BufferDesc  b       /* IN */ )
{
    Spawn_st   *st = self->st;

    TRC(printf("Spawn: AckReceive %p\n",b));

    /* Signal the child that we've finished with the buffer */
    MU_RELEASE(&st->mu);
    SIGNAL(&st->cv);
}

static void IDCClientBinding_Destroy_m (
    IDCClientBinding_cl     *self )
{
    printf("Spawn: IDCClientBinding_Destroy_m: should not be called\n");
}

/*---------------------------------------- SpawnReturn Entry Points ----*/

/* This is an IDC server used to retrieve the result of the call from the
   new domain. */
static void SpawnReturn_Return_m (
        SpawnReturn_cl  *self,
        addr_t address, uint32_t length )
{
    Spawn_st        *st = self->st;
    
    TRC(printf("Spawn: in SpawnReturn$Return()\n"));

    st->return_buffer=address;
    st->return_buffer_size=length;

    /* We must signal the waiting thread, and then block ourselves until
       told that the receive buffer has been dealt with */
    SIGNAL(&st->cv);

    MU_LOCK(&st->mu);
    WAIT(&st->mu, &st->cv);
    MU_RELEASE(&st->mu);
}

/*---------------------------------------- IDCCallback Entry Points ----*/

static bool_t IDCCallback_Request_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdid    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Binder_Cookies  *svr_cks        /* OUT */ )
{
    Spawn_st        *st = self->st;

    TRC(printf("Spawn: IDCCallback: Request from %qx\n",dom));

    return (dom==st->child);
}

static bool_t IDCCallback_Bound_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdid    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Type_Any        *server /* INOUT */ )
{
    TRC(printf("Spawn: IDCCallback: Bound\n"));
    return True;
}

static void IDCCallback_Closed_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        const Type_Any  *server /* IN */ )
{
    TRC(printf("Spawn: IDCCallback: Closed\n"));
}

/*-------------------------------------------- Closure Entry Points ----*/

typedef struct {
    Spawn_st *parent;

    IDCServerStubs_State serverstubs_state;
    IDCServerStubs_cl    serverstubs_cl;
    IDCServerBinding_cl  serverbinding_cl;
    IDC_BufferRec        inbuf;
    IDC_BufferRec        outbuf;
    SpawnReturn_clp      return_clp;

    addr_t receive_buffer;
    uint32_t receive_buffer_size;

    bool_t done;
} Child_st;

/* The entry point of the new domain */
static void Closure_Apply_m (
        Closure_cl      *self )
{
    Spawn_st    *sst = self->st;
    Child_st *st;
    
    TRC(printf("Spawn: in new domain\n"));
    /* Remember: at this point state is probably read-only */

    /* Grab some memory for child state */
    st=Heap$Malloc(Pvs(heap), sizeof(*st));

    st->parent=sst;
    st->done=False;

    /* Fill in stubs state */
    st->serverstubs_state.service=(addr_t)((word_t)sst->module.val);
    st->serverstubs_state.binding=&st->serverbinding_cl;
    st->serverstubs_state.marshal=SpawnIDC;
    CL_INIT(st->serverstubs_cl, sst->info->stub->op, &st->serverstubs_state);
    CL_INIT(st->serverbinding_cl, &serverbinding_ms, st);

    st->inbuf.base=sst->send_buffer;
    st->inbuf.ptr=st->inbuf.base;
    st->inbuf.space=sst->send_buffer_size;
    st->inbuf.heap=Pvs(heap);

    /* Bind to return service */
    st->return_clp=IDC_BIND(sst->return_offer, SpawnReturn_clp);

    TRC(printf("Spawn: child: about to call server stubs\n"));
    IDCServerStubs$Dispatch(&st->serverstubs_cl);

    TRC(printf("Spawn: child: back from server stubs. Clearing up.\n"));

    TRY {
	IDC_CLOSE(sst->return_offer);
    } CATCH_ALL {
	/* Ignore all exceptions - one might be raised if the parent
	   is quick about destroying the connection */
    } ENDTRY;

    FREE(st);

/*    printf("Spawn: child: main thread about to exit\n"); */
}

/*----------------------------------- IDCServerBinding Entry Points ----*/

static bool_t IDCServerBinding_ReceiveCall_m (
    IDCServerBinding_cl     *self
    /* RETURNS */,
    IDC_BufferDesc  *b,
    word_t  *opn,
    string_t        *name )
{
    Child_st   *st = self->st;
    
    TRC(printf("Spawn: IDCServerBinding: ReceiveCall\n"));

    if (st->done) return False;

    st->done=True;

    *b=&st->inbuf;

    *opn=*((word_t *)(st->inbuf.ptr))++;
    *name=NULL;  /* XXX if this is non-NULL the generic stubs will search for
		    a method of that name, and fail if there isn't one. Ick. */

    return True;
}

static void IDCServerBinding_AckReceive_m (
    IDCServerBinding_cl     *self,
    IDC_BufferDesc  b       /* IN */ )
{
    TRC(printf("Spawn: IDCServerBinding: AckReceive\n"));
}

static IDC_BufferDesc IDCServerBinding_InitReply_m (
    IDCServerBinding_cl     *self )
{
    return IDCServerBinding_InitExcept_m(self,0,NULL);
}

static IDC_BufferDesc IDCServerBinding_InitExcept_m (
    IDCServerBinding_cl     *self,
    word_t  exc     /* IN */,
    string_t        name    /* IN */ )
{
    Child_st   *st = self->st;

    TRC(printf("Spawn: IDCServerBinding: InitExcept\n"));

    /* Allocate buffer space */
    /* XXX must make sure that this is readable by the parent */
    st->receive_buffer=Heap$Malloc(Pvs(heap), 2048);
    st->receive_buffer_size=2048; /* XXX */

    st->outbuf.base=st->receive_buffer;
    st->outbuf.ptr=st->outbuf.base;
    st->outbuf.space=st->receive_buffer_size;
    st->outbuf.heap=Pvs(heap);

    /* Put something on the start of the buffer to indicate it's a normal
       return */
    *((word_t *)(st->outbuf.ptr))++ = exc;
    st->outbuf.space-=sizeof(word_t);
    
    return &st->outbuf;
}

static void IDCServerBinding_SendReply_m (
    IDCServerBinding_cl     *self,
    IDC_BufferDesc  b       /* IN */ )
{
    Child_st   *st = self->st;

    TRC(printf("Spawn: IDCServerBinding: SendReply\n"));

    SpawnReturn$Return(st->return_clp, st->receive_buffer,
		       st->receive_buffer_size);

    /* The Return call blocks until the parent has dealt with the buffer */
    /* We can now free the buffer */

    FREE(st->receive_buffer);
    st->receive_buffer_size=0;
}

static Domain_ID IDCServerBinding_Client_m (
    IDCServerBinding_cl     *self
    /* RETURNS */,
    ProtectionDomain_ID    *pdid )
{
    printf("Spawn: IDCServerBinding_Client: called (aargh)\n");
    *pdid=0;
    return 0;
}

static void IDCServerBinding_Destroy_m (
    IDCServerBinding_cl     *self )
{
    printf("Spawn: IDCServerBinding_Destroy: called (aargh)\n");
}

/*
 * End 
 */
