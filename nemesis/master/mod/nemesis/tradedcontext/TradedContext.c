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
**      TradedContext
** 
** FUNCTIONAL DESCRIPTION:
** 
**      A Context extended for use over IDC
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: TradedContext.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <exceptions.h>
#include <mutex.h>
#include <links.h>
#include <IDCMacros.h>

#include <TradedContextMod.ih>
#include <TradedContext.ih>
#include <Security.ih>
#include <IDCCallback.ih>
#include <CSIDCTransport.ih>
#include <CSClientStubMod.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG_RESOLVE
#define RTRC(_x) _x
#else
#define RTRC(_x)
#endif

#ifdef DEBUG_IDC
#define ITRC(_x) _x
#else
#define ITRC(_x)
#endif

static  Context_List_fn                     TradedContext_List_m;
static  Context_Get_fn                      TradedContext_Get_m;
static  Context_Add_fn                      TradedContext_Add_m;
static  Context_Remove_fn                   TradedContext_Remove_m;
static  Context_Dup_fn                      TradedContext_Dup_m;
static  Context_Destroy_fn                  TradedContext_Destroy_m;
static  TradedContext_AuthAdd_fn            TradedContext_AuthAdd_m;
static  TradedContext_AuthRemove_fn         TradedContext_AuthRemove_m;
static  TradedContext_Owner_fn              TradedContext_Owner_m;
static  TradedContext_AddTradedContext_fn   TradedContext_AddTradedContext_m;

static  TradedContext_op        tc_ms = {
    TradedContext_List_m,
    TradedContext_Get_m,
    TradedContext_Add_m,
    TradedContext_Remove_m,
    TradedContext_Dup_m,
    TradedContext_Destroy_m,
    TradedContext_AuthAdd_m,
    TradedContext_AuthRemove_m,
    TradedContext_Owner_m,
    TradedContext_AddTradedContext_m
};

static  CSClientStubMod_New_fn          CSClientStubMod_New_m;
static  CSClientStubMod_Destroy_fn      CSClientStubMod_Destroy_m;

static  CSClientStubMod_op      cs_ms = {
    CSClientStubMod_New_m,
    CSClientStubMod_Destroy_m
};

static const CSClientStubMod_cl cs_cl = { &cs_ms, NULL };

static  Context_List_fn                     TCS_List_m;
static  Context_Get_fn                      TCS_Get_m;
static  Context_Add_fn                      TCS_Add_m;
static  Context_Remove_fn                   TCS_Remove_m;
static  Context_Dup_fn                      TCS_Dup_m;
static  Context_Destroy_fn                  TCS_Destroy_m;
static  TradedContext_AuthAdd_fn            TCS_AuthAdd_m;
static  TradedContext_AuthRemove_fn         TCS_AuthRemove_m;
static  TradedContext_Owner_fn              TCS_Owner_m;
static  TradedContext_AddTradedContext_fn   TCS_AddTradedContext_m;

static  TradedContext_op        tcs_ms = {
    TCS_List_m,
    TCS_Get_m,
    TCS_Add_m,
    TCS_Remove_m,
    TCS_Dup_m,
    TCS_Destroy_m,
    TCS_AuthAdd_m,
    TCS_AuthRemove_m,
    TCS_Owner_m,
    TCS_AddTradedContext_m
};

static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  cb_ms = {
    IDCCallback_Request_m,
    IDCCallback_Bound_m,
    IDCCallback_Closed_m
};

static  TradedContextMod_New_fn  TradedContextMod_New_m;

static  TradedContextMod_op     mod_ms = {
    TradedContextMod_New_m
};

static const TradedContextMod_cl     cl = { &mod_ms, NULL };

CL_EXPORT (TradedContextMod, TradedContextMod, cl);

typedef struct _client Client_st;
typedef struct _tc_st TradedContext_st;

/* We need to know who we're talking to so that we can check the
   security tags that they supply. Whenever someone connects we
   store their details. */
struct _client {
    TradedContext_cl cl;      /* Per-client closure */
    Domain_ID id;             /* ID of client */
    TradedContext_st *st;     /* Server state */
    IDCServerBinding_clp b;
    struct _client *next, *prev; /* Links */
};

struct _tc_st {
    Context_clp cx;        /* Underlying Context */
    Client_st clients;     /* List of clients; head of list is local domain */
    Heap_clp heap;         /* Place for storage */
    Security_clp security; /* Security service */
    Security_Tag owner;    /* Tag that's allowed to delete this service */
    mutex_t mu;            /* Protects all mutable state: cx + clients */
    IDCCallback_cl callback;
    IDCService_clp service;
};

/* We use a normal Context to store the data. It contains entries of
   the following type: */
typedef struct {
    Type_Any d;
    Security_Tag owner;
} tc_entry;

typedef struct {
    TradedContext_cl cl;
    TradedContext_clp s;
} TCS_st;

/*----------------------------------- TradedContextMod Entry Points ----*/

static IDCOffer_clp TradedContextMod_New_m (
    TradedContextMod_cl     *self,
    ContextMod_clp  cm      /* IN */,
    Heap_clp        h       /* IN */,
    TypeSystem_clp  ts      /* IN */,
    Entry_clp       entry   /* IN */,
    Security_Tag    owner   /* IN */ )
{
    TradedContext_st *st;
    IDCOffer_clp NOCLOBBER offer;
    Type_Any any;
    CSIDCTransport_clp cst =
	NAME_FIND("modules>CSIDCTransport",CSIDCTransport_clp);

    st=Heap$Malloc(h, sizeof(*st));
    if (!st) {
	TRC(fprintf(stderr,"TradedContextMod: couldn't allocate server "
		    "state\n"));
	RAISE_Heap$NoMemory();
	return NULL;
    }

    st->heap=h;
    st->owner=owner;
    CL_INIT(st->callback, &cb_ms, st);

    LINK_INIT(&st->clients);
    CL_INIT(st->clients.cl, &tc_ms, &st->clients);
    st->clients.id=VP$DomainID(Pvs(vp));
    st->clients.st=st;
    st->clients.b=NULL;

    TRY { /* Things that might raise exceptions */
	st->cx=ContextMod$NewContext(cm, h, ts);
    } CATCH_ALL {
	FREE(st);
	TRC(fprintf(stderr,"TradedContextMod: couldn't create internal "
		    "context\n"));
	RERAISE;
    } ENDTRY;

    TRY {
	MU_INIT(&st->mu);
    } CATCH_ALL {
	Context$Destroy(st->cx);
	FREE(st);
	RERAISE;
    } ENDTRY;

    TRY {
	st->security=IDC_OPEN("sys>SecurityOffer", Security_clp);
    } CATCH_ALL {
	Context$Destroy(st->cx);
	MU_FREE(&st->mu);
	FREE(st);
	RERAISE;
    } ENDTRY;

    offer=NULL;

    TRY {
	ANY_INIT(&any, TradedContext_clp, &st->clients.cl);
	offer=CSIDCTransport$Offer(cst, &any, TradedContext_clp__code,
				   &cs_cl, &st->callback, st->heap,
				   Pvs(gkpr), entry, &st->service);
    } CATCH_ALL {
	Context$Destroy(st->cx);
	MU_FREE(&st->mu);
	FREE(st);
	RERAISE;
    } ENDTRY;

    return offer;
}

/*----------------------------------------------- Utility functions ----*/

static void barf_if_impure(string_t name)
{
    if (strchr(name, '>')) {
	fprintf(stderr,"TradedContext: Impure!\n");
	RAISE_Context$NotContext();
    }
}

/*-------------------------------------- TradedContext Entry Points ----*/

static Context_Names *TradedContext_List_m (
    Context_cl        *self )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;
    Context_Names * NOCLOBBER r=NULL;

    MU_LOCK(&st->mu);

    TRY {
	r=Context$List(st->cx);
    } CATCH_ALL {
	MU_RELEASE(&st->mu);
	RERAISE;
    } ENDTRY;
    MU_RELEASE(&st->mu);

    return r;
}

static bool_t TradedContext_Get_m (
    Context_cl        *self,
    string_t        name    /* IN */,
    Type_Any        *obj    /* OUT */ )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;
    tc_entry *e;
    Type_Any any;

    /* All of the cunningness in TradedContexts is in the client
       stubs. The stubs have to split "name" up into arcs and do the
       appropriate operations. If we get given a name that isn't a
       single arc-name, raise an exception. */
    barf_if_impure(name);

    MU_LOCK(&st->mu);

    /* Since we're just looking up an arc name the call to
       Context$Get() shouldn't raise an exception. */
    if (Context$Get(st->cx, name, &any)) {
	/* Found it; fetch the real contents. */
	e=NARROW(&any, addr_t);

	ANY_COPY(obj, &e->d);
	MU_RELEASE(&st->mu);
	return True;
    }

    MU_RELEASE(&st->mu);
    return False;
}

static void TradedContext_Add_m (
    Context_cl        *self,
    string_t        name    /* IN */,
    const Type_Any  *obj    /* IN */ )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;
    tc_entry *e;
    Type_Any any;

    barf_if_impure(name);

    e=Heap$Malloc(st->heap, sizeof(*e));
    if (!e) RAISE_Context$Denied();

    ANY_COPY(&e->d, obj);
    /* We need to make a decision about the owner. For now let's just say
       '-1', and treat it as 'anybody'. */
    e->owner=-1;

    ANY_INIT(&any, addr_t, e);

    MU_LOCK(&st->mu);

    TRY {
	Context$Add(st->cx, name, &any);
    } CATCH_ALL {
	MU_RELEASE(&st->mu);
	Heap$Free(st->heap, e);
	RERAISE;
    } ENDTRY;

    MU_RELEASE(&st->mu);
}

static void TradedContext_Remove_m (
    Context_cl        *self,
    string_t        name    /* IN */ )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;
    tc_entry *e;
    Type_Any any;

    barf_if_impure(name);

    MU_LOCK(&st->mu);

    if (Context$Get(st->cx, name, &any)) {
	/* Well, it exists; is the caller allowed to remove it? */
	e=NARROW(&any, addr_t);

	if ((e->owner!=-1) && !Security$CheckTag(st->security,
						 e->owner, cst->id)) {
	    /* Oh dear. They aren't allowed to do this. */
	    MU_RELEASE(&st->mu);
	    RAISE_Context$Denied();
	    return;
	}

	/* They are allowed to remove it, so let's do so */
	Context$Remove(st->cx, name);
	MU_RELEASE(&st->mu);
	Heap$Free(st->heap, e);
	return;
    }
    MU_RELEASE(&st->mu);
    RAISE_Context$NotFound(name);
}

static Context_clp TradedContext_Dup_m (
    Context_cl        *self,
    Heap_clp        h       /* IN */,
    WordTbl_clp     xl      /* IN */ )
{
    RAISE_Context$Denied();
    return NULL;
}

static void TradedContext_Destroy_m (
    Context_cl        *self )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;

    if (!Security$CheckTag(st->security, st->owner, cst->id)) {
	RAISE_Context$Denied();
	return;
    }

    /* We should delete the service. XXX implement later */
}

static void TradedContext_AuthAdd_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */,
    const Type_Any  *obj    /* IN */,
    Security_Tag    tag     /* IN */,
    const Security_CertSeq  *certificates   /* IN */ )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;
    tc_entry *e;
    Type_Any any;

    /* Check that the caller has the tag they claim is to be the owner */
    if (!Security$CheckTag(st->security, tag, cst->id)) {
	printf("AuthAdd: bad tag\n");
	RAISE_Context$Denied();
	return;
    }

    barf_if_impure(name);

    e=Heap$Malloc(st->heap, sizeof(*e));
    if (!e) {
	fprintf(stderr,"TradedContext: AuthAdd: out of memory\n");
	RAISE_Context$Denied();
    }

    ANY_COPY(&e->d, obj);
    e->owner=tag;

    ANY_INIT(&any, addr_t, e);

    MU_LOCK(&st->mu);

    TRY {
	Context$Add(st->cx, name, &any);
    } CATCH_ALL {
	MU_RELEASE(&st->mu);
	Heap$Free(st->heap, e);
	RERAISE;
    } ENDTRY;

    MU_RELEASE(&st->mu);
}

static void TradedContext_AuthRemove_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */,
    const Security_CertSeq  *certificates   /* IN */ )
{
/*    Client_st *cst = self->st; */
/*    TradedContext_st *st = cst->st; */

    /* For now this is the same as Remove */
    TradedContext_Remove_m((Context_clp)self, name);
}

static Security_Tag TradedContext_Owner_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */ )
{
    Client_st *cst = self->st;
    TradedContext_st *st = cst->st;
    tc_entry *e;
    Type_Any any;
    Security_Tag t;

    barf_if_impure(name);

    MU_LOCK(&st->mu);

    if (Context$Get(st->cx, name, &any)) {
	e=NARROW(&any, addr_t);
	t=e->owner;
	MU_RELEASE(&st->mu);
	return t;
    }

    MU_RELEASE(&st->mu);

    RAISE_Context$NotFound(name);
    return -1;
}

static void TradedContext_AddTradedContext_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */,
    IDCOffer_clp    cx      /* IN */,
    Security_Tag    tag     /* IN */,
    const Security_CertSeq  *certificates   /* IN */ )
{
/*    Client_st *cst = self->st; */
/*    TradedContext_st *st = cst->st; */
    Type_Any any;

    /* Depending on how the implementation goes this method might have
       to do something special in the future. For now we just put the
       IDCOffer in the context and rely on the client-side stubs (and,
       in the local case the implementation of Get) to bind to it. */

    ANY_INIT(&any, IDCOffer_clp, cx);
    TradedContext_AuthAdd_m(self, name, &any, tag, certificates);
}

/*---------------------------------------- IDCCallback Entry Points ----*/

static bool_t IDCCallback_Request_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    Domain_ID       did     /* IN */,
    ProtectionDomain_ID     pdid    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Binder_Cookies  *svr_cks        /* OUT */ )
{
    ITRC(eprintf("TradedContext: callback request, domain %qx\n",did));

    return True; /* Open to all */
}

static bool_t IDCCallback_Bound_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    Domain_ID       did     /* IN */,
    ProtectionDomain_ID     pdid    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Type_Any        *server /* INOUT */ )
{
    TradedContext_st        *st = self->st;
    Client_st *cst;

    ITRC(eprintf("TradedContext: client id %qx bound\n",did));

    cst=Heap$Malloc(st->heap, sizeof(*cst));
    if (!cst) {
	ITRC(fprintf(stderr,"TradedContext: no memory for bind\n"));
	return False; /* No memory -> bind fails */
    }

    CL_INIT(cst->cl, &tc_ms, cst);
    cst->id=did;
    cst->st=st;

    MU_LOCK(&st->mu);
    LINK_ADD_TO_HEAD(&st->clients, cst);
    MU_RELEASE(&st->mu);

    ANY_INIT(server, TradedContext_clp, &cst->cl);

    ITRC(eprintf("TradedContext: client bind complete\n"));

    return True;
}

static void IDCCallback_Closed_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    const Type_Any  *server /* IN */ )
{
    TradedContext_st        *st = self->st;
    Client_st *cst;
    TradedContext_clp clp;

    ITRC(eprintf("TradedContext: callback closed\n"));

    clp=NARROW(server, TradedContext_clp);
    cst=clp->st;

    MU_LOCK(&st->mu);
    LINK_REMOVE(cst);
    MU_RELEASE(&st->mu);

    FREE(cst);
}

/*------------------------------------ CSClientStubMod Entry Points ----*/

static Type_Any *CSClientStubMod_New_m (
    CSClientStubMod_cl      *self,
    const Type_Any *server   /* IN */ )
{
    TCS_st *st;
    Type_Any *anyp;

    st=Heap$Malloc(Pvs(heap), sizeof(*st));
    CL_INIT(st->cl, &tcs_ms, st);

    st->s=NARROW(server, TradedContext_clp);

    anyp=Heap$Malloc(Pvs(heap), sizeof(*anyp));
    ANY_INIT(anyp, TradedContext_clp, &st->cl);

    return anyp;
}

static void CSClientStubMod_Destroy_m (
    CSClientStubMod_cl      *self,
    const Type_Any  *stubs  /* IN */ )
{
    TradedContext_clp clp;
    TCS_st *st;

    clp=NARROW(stubs, TradedContext_clp);
    st=clp->st;

    FREE(st);
}

/*------------------------ TradedContext (client side) Entry Points ----*/

/* Utility functions */

/* returns: 0 = ok; 1 = not found; 2 = not context */
static int resolve_step(Context_clp cx, string_t arcname, Type_Any *target)
{
    bool_t found;
    IDCOffer_clp offer;
    TradedContext_clp stub;
    
    RTRC(printf("resolve_step: %s\n",arcname));
    
    found=Context$Get(cx, arcname, target);
    
    if (!found) {
	RTRC(printf(" (arcname missing)\n"));
	return 1; /* Arcname missing */
    }
    
    if (ISTYPE(target, Context_clp)) {
	return 0; /* Done; this is alright because we're running in
		     the client, so the client is running the
		     risk. */
    }

    if (ISTYPE(target, IDCOffer_clp)) {
	offer=NARROW(target, IDCOffer_clp);
	/* It might be an offer for a TradedContext; if it is, bind to
	   it and return the stub instead */
	if (TypeSystem$IsType(Pvs(types), IDCOffer$Type(offer),
			      TradedContext_clp__code)) {
	    RTRC(printf(" (offer for TradedContext; binding)\n"));
	    stub=IDC_BIND(offer, TradedContext_clp);
	    ANY_INIT(target, TradedContext_clp, stub);
	    return 0;
	}
	RTRC(printf(" (wrong type of offer)\n"));
	return 2;
    }

    /* Not anything I recognise, so we can't follow it. */
    RTRC(printf(" (can't resolve to a context)\n"));
    return 2;
}

/* Take a full name. If it has multiple arcnames in it, follow them
   until we're left with a pure arcname. Return the pure arcname and
   the context in which it's valid. The pure arcname is allocated on
   the heap; the memory for "target" is expected to be supplied by the
   caller.  "name" is not modified.

   If at any point we come across something we can't treat as a
   context - i.e. something that isn't a Context_clp or an IDCOffer
   for a TradedContext - we return NULL as a signal for our caller to
   RAISE_Context$NotContext() */

static string_t resolve_to_context(TCS_st *st, string_t name,
				   Type_Any *target)
{
    Type_Any any;
    string_t m, arc, sep;
    Context_clp cx=(Context_clp)st->s;
    bool_t ok;
    
    m=arc=strdup(name);

    RTRC(printf("resolve_to_context(%s)\n",name));
    
    while ((sep=strchr(arc,'>'))) {
	/* We don't have a pure arcname, so we need to do a resolution step.
	   Terminate the first arcname. */
	*sep=0;
	
	ok=resolve_step(cx, arc, &any);
	
	if (ok) {
	    /* We couldn't resolve it; tough. */
	    FREE(m);
	    if (ok==1) {
		RAISE_Context$NotFound(NULL);
	    } else {
		RAISE_Context$NotContext();
	    }
	    return NULL;
	}
	
	cx=NARROW(&any, Context_clp);

	/* XXX we should really deal with passing auth information to
	   TradedContexts, once we implement one which actually uses it */

	arc=sep+1;
    }

    /* At this point we have a pure arcname, and a Context_clp
	   in our hands. */
    ANY_INIT(target, Context_clp, cx);

    arc=strdup(arc);
    FREE(m);
    RTRC(printf(" + returns %s\n",arc));

    return arc;
}

static Context_Names *TCS_List_m (
    Context_cl        *self )
{
    TCS_st      *st = self->st;

    /* Pass straight through */
    return Context$List((Context_clp)st->s);
}

static bool_t TCS_Get_m (
    Context_cl        *self,
    string_t        name    /* IN */,
    Type_Any        *obj    /* OUT */ )
{
    TCS_st      *st = self->st;
    Context_clp cx;
    string_t NOCLOBBER arc;
    Type_Any any;
    bool_t NOCLOBBER ok;

    TRC(printf("TCS_Get: %s\n",name));

    ok=True;
    TRY {
	arc=resolve_to_context(st, name, &any);
    } CATCH_Context$NotFound(UNUSED e) {
	ok=False;
    } ENDTRY;

    if (!ok) return False; /* Not found exception */
	    
    cx=NARROW(&any, Context_clp);

    TRY {
	ok=Context$Get(cx, arc, obj);
    } CATCH_ALL {
	FREE(arc);
	RERAISE;
    } ENDTRY;

    FREE(arc);

    if (ok) {
	/* If it's an IDCOffer for a TradedContext then we magically
	   bind to it. */
	if (ISTYPE(obj, IDCOffer_clp)) {
	    IDCOffer_clp offer;
	    offer=NARROW(obj, IDCOffer_clp);
	    if (TypeSystem$IsType(Pvs(types), IDCOffer$Type(offer),
				  TradedContext_clp__code)) {
		TradedContext_clp tcx;
		tcx=IDC_BIND(offer, TradedContext_clp);
		ANY_INIT(obj, TradedContext_clp, tcx);
	    }
	}
    }

    return ok;
}

static void TCS_Add_m (
    Context_cl        *self,
    string_t        name    /* IN */,
    const Type_Any  *obj    /* IN */ )
{
    TCS_st      *st = self->st;
    Context_clp cx;
    string_t arc;
    Type_Any any;

    TRC(printf("TCS_Add: %s\n",name));

    arc=resolve_to_context(st, name, &any);
    cx=NARROW(&any, Context_clp);

    TRY {
	Context$Add(cx, arc, obj);
    } FINALLY {
	FREE(arc);
    } ENDTRY;
}

static void TCS_Remove_m (
    Context_cl        *self,
    string_t        name    /* IN */ )
{
    TCS_st      *st = self->st;
    Context_clp cx;
    string_t arc;
    Type_Any any;

    TRC(printf("TCS_Remove: %s\n",name));

    arc=resolve_to_context(st, name, &any);
    cx=NARROW(&any, Context_clp);

    TRY {
	Context$Remove(cx, arc);
    } FINALLY {
	FREE(arc);
    } ENDTRY;
}

static Context_clp TCS_Dup_m (
    Context_cl        *self,
    Heap_clp        h       /* IN */,
    WordTbl_clp     xl      /* IN */ )
{
    RAISE_Context$Denied();
    return NULL;
}

static void TCS_Destroy_m (
    Context_cl        *self )
{
    TCS_st      *st = self->st;

    Context$Destroy((Context_clp)st->s);
}

static void TCS_AuthAdd_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */,
    const Type_Any  *obj    /* IN */,
    Security_Tag    tag     /* IN */,
    const Security_CertSeq  *certificates   /* IN */ )
{
    TCS_st      *st = self->st;
    Context_clp cx;
    string_t arc;
    Type_Any any;

    TRC(printf("TCS_AuthAdd: %s\n",name));

    arc=resolve_to_context(st, name, &any);
    cx=NARROW(&any, Context_clp);

    TRY {
	Context$Add(cx, arc, obj);
    } FINALLY {
	FREE(arc);
    } ENDTRY;
}

static void TCS_AuthRemove_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */,
    const Security_CertSeq  *certificates   /* IN */ )
{
    TCS_st      *st = self->st;
    Context_clp cx;
    string_t arc;
    Type_Any any;

    TRC(printf("TCS_AuthRemove: %s\n",name));

    arc=resolve_to_context(st, name, &any);
    cx=NARROW(&any, Context_clp);

    TRY {
	Context$Remove(cx, arc);
    } FINALLY {
	FREE(arc);
    } ENDTRY;
}

static Security_Tag TCS_Owner_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */ )
{
    TCS_st      *st = self->st;

    return TradedContext$Owner(st->s, name);
}

static void TCS_AddTradedContext_m (
    TradedContext_cl        *self,
    string_t        name    /* IN */,
    IDCOffer_clp    cx      /* IN */,
    Security_Tag    tag     /* IN */,
    const Security_CertSeq  *certificates   /* IN */ )
{
    TCS_st      *st = self->st;
    TradedContext_clp tcx;
    string_t arc;
    Type_Any any;

    TRC(printf("TCS_AddTradedContext: %s\n",name));

    arc=resolve_to_context(st, name, &any);
    tcx=(TradedContext_clp)NARROW(&any, Context_clp);

    TRY {
	TradedContext$AddTradedContext(tcx, arc, cx, tag, certificates);
    } FINALLY {
	FREE(arc);
    } ENDTRY;

}

/*
 * End 
 */
