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
**      security.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Manages security tags of domains
** 
** ENVIRONMENT: 
**
**      Domain manager
** 
** ID : $Id: security.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>

#include <IDCCallback.ih>
#include <IDCTransport.ih>

#include "DomainMgr_st.h"

#define __Security_STATE PerDomain_st
#include <Security.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static  Security_CreateTag_fn           Security_CreateTag_m;
static  Security_DeleteTag_fn           Security_DeleteTag_m;
static  Security_GiveTag_fn             Security_GiveTag_m;
static  Security_SetInherit_fn          Security_SetInherit_m;
static  Security_CheckTag_fn            Security_CheckTag_m;
static  Security_CheckExists_fn         Security_CheckExists_m;

static  Security_op     security_ms = {
    Security_CreateTag_m,
    Security_DeleteTag_m,
    Security_GiveTag_m,
    Security_SetInherit_m,
    Security_CheckTag_m,
    Security_CheckExists_m
};

static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

static  IDCCallback_op  callback_ms = {
    IDCCallback_Request_m,
    IDCCallback_Bound_m,
    IDCCallback_Closed_m
};

static bool_t check_domain_has_tag(DomainMgr_st *dm_st, PerDomain_st *dom,
				   Security_Tag tag);
static bool_t add_tag_to_domain(DomainMgr_st *dm_st, PerDomain_st *st,
				Security_Tag tag, bool_t inherit);
static bool_t remove_tag_from_domain(DomainMgr_st *dm_st,
				     PerDomain_st *dom, Security_Tag tag);
static bool_t delete_tag_if_free(DomainMgr_st *dm_st, Security_Tag tag);

/*---------------------------------------------------- Entry Points ----*/

static bool_t Security_CreateTag_m (
    Security_cl     *self
    /* RETURNS */,
    Security_Tag   *tag )
{
    PerDomain_st   *st = self->st;
    DomainMgr_st   *dm_st = st->dm_st;
    Security_Tag new_tag;
    dcb_link_t *hp;
    addr_t tmp;

    TRC(eprintf("Security: CreateTag "));

    MU_LOCK(&dm_st->mu);
    new_tag = dm_st->next_tag++;

    TRC(eprintf("%qx\n",new_tag));

    /* Create an entry for the tag */

    hp=Heap$Malloc(Pvs(heap), sizeof(*hp));
    if (!hp) {
	eprintf("Security: out of memory\n");
	MU_RELEASE(&dm_st->mu);
	return False;
    }

    LINK_INITIALISE(hp);
    LongCardTbl$Put(dm_st->tag_tbl, new_tag, hp);


    if (!add_tag_to_domain(dm_st, st, new_tag, False)) {
	/* Didn't work, for some reason. Nobody else will have this tag
	   right now, so we can happily delete it */
	eprintf("Security: CreateTag: nope\n");
	FREE(hp);
	LongCardTbl$Delete(dm_st->tag_tbl, new_tag, &tmp);
	MU_RELEASE(&dm_st->mu);
	return False;
    }

    /* Happy happy happy */

    MU_RELEASE(&dm_st->mu);

    *tag=new_tag;
    
    return True;
}

static bool_t Security_DeleteTag_m (
    Security_cl     *self,
    Security_Tag   tag    /* IN */ )
{
    PerDomain_st   *st = self->st;
    DomainMgr_st *dm_st = st->dm_st;
    bool_t ok;

    TRC(eprintf("Security: DeleteTag %qx from dom %qx\n",tag,st->rop->id));

    MU_LOCK(&dm_st->mu);
    ok=remove_tag_from_domain(dm_st, st, tag);
    MU_RELEASE(&dm_st->mu);

    return ok;
}

static bool_t Security_GiveTag_m (
    Security_cl     *self,
    Security_Tag   tag    /* IN */,
    Domain_ID       domain  /* IN */,
    bool_t          inherit /* IN */ )
{
    PerDomain_st  *st = self->st, *tst;
    DomainMgr_st *dm_st = st->dm_st;
    bool_t NOCLOBBER ok = False;

    TRC(eprintf("Security: GiveTag %qx to %qx from %qx\n",
		tag, domain, st->rop->id));

    if (!check_domain_has_tag(dm_st, st, tag)) {
	return False; /* We don't have this tag to give away */
    }

    IncDomRef(dm_st, domain, &tst);

    TRY {
	MU_LOCK(&dm_st->mu);
	
	if (tst->parent != st->rop->id) {
	    eprintf("Security: awooga!\n");
	} else {
	
	    /* We're the parent, and we have the tag. Give! */
	    ok = add_tag_to_domain(dm_st, tst, tag, inherit);
	}
	MU_RELEASE(&dm_st->mu);
    } FINALLY {
	DecDomRef(dm_st, tst);
    } ENDTRY;

    return ok;
}

static bool_t Security_SetInherit_m (
    Security_cl     *self,
    Security_Tag   tag    /* IN */,
    bool_t  inherit /* IN */ )
{
    PerDomain_st   *st = self->st;
    bool_t found;
    uint32_t i;

    TRC(eprintf("Security: SetInherit tag %qx dom %qx val %s\n",
		tag, st->rop->id, inherit?"yes":"no"));

    found=False;
    for (i=0; i<SECURITY_TAGS_PER_DOMAIN; i++) {
	if (st->tags[i]==tag) {
	    st->inherit[i]=inherit;
	    found=True;
	}
    }

    return found;
}

static bool_t Security_CheckTag_m (
    Security_cl     *self,
    Security_Tag   tag    /* IN */,
    Domain_ID       domain  /* IN */ )
{
    PerDomain_st   *st = self->st, *tst;
    DomainMgr_st *dm_st = st->dm_st;
    bool_t ok;
    
    TRC(eprintf("Security: CheckTag %qx with dom %qx\n",tag,domain));

    IncDomRef(dm_st, domain, &tst);

    MU_LOCK(&dm_st->mu);

    ok=check_domain_has_tag(dm_st, tst, tag);

    MU_RELEASE(&dm_st->mu);

    DecDomRef(dm_st, tst);

    TRC(eprintf("Security: CheckTag returns %s\n",ok?"true":"false"));

    return ok;
}

static bool_t Security_CheckExists_m (
    Security_cl     *self,
    Security_Tag   tag    /* IN */ )
{
    PerDomain_st   *st = self->st;
    DomainMgr_st *dm_st = st->dm_st;
    bool_t found;
    dcb_link_t *hp;

    TRC(eprintf("Security: CheckExists %qx\n",tag));

    MU_LOCK(&dm_st->mu);

    found=LongCardTbl$Get(dm_st->tag_tbl, tag, (addr_t *)&hp);

    MU_RELEASE(&dm_st->mu);

    TRC(eprintf("Security: CheckExists result %d %p\n",found,hp));

    return found;
}

/*---------------------------------------------------- Entry Points ----*/

static bool_t IDCCallback_Request_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    Domain_ID       dom     /* IN */,
    ProtectionDomain_ID    pdom    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Binder_Cookies  *svr_cks        /* OUT */ )
{
    return True; /* Anyone can bind */
}

static bool_t IDCCallback_Bound_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    Domain_ID       dom     /* IN */,
    ProtectionDomain_ID    pdom    /* IN */,
    const Binder_Cookies    *clt_cks        /* IN */,
    Type_Any        *server /* INOUT */ )
{
    DomainMgr_st        *dm_st = self->st;
    PerDomain_st    *st;

    if(!IncDomRefNoExc(dm_st, dom, &st)) {
	/* Hmm, we lost the domain */
	return False;
    }
    
    TRC(eprintf("Setting security closure for domain %qx to %p\n", 
		dom, &st->security_cl));
    server->val = (pointerval_t) (&st->security_cl);

    return True;
}

static void IDCCallback_Closed_m (
    IDCCallback_cl  *self,
    IDCOffer_clp    offer   /* IN */,
    IDCServerBinding_clp    binding /* IN */,
    const Type_Any  *server /* IN */ )
{
    DomainMgr_st        *st = self->st;

    Security_clp clp = NARROW(server, Security_clp);

    /* Allow the system to free the rop */
    DecDomRef(st, clp->st);

    TRC(eprintf("Security: IDCCallback: Closed\n"));
}

/*------------------------------------------- Convenience functions ----*/

/* All of these functions must be called with dm_st->mu locked */

static bool_t check_domain_has_tag(DomainMgr_st *dm_st, PerDomain_st *dom,
				   Security_Tag tag)
{
    uint32_t i;

    for (i=0; i<SECURITY_TAGS_PER_DOMAIN; i++) {
	if (dom->tags[i]==tag) return True;
    }
    return False;
}

/* Add a tag to a domain: check that the domain doesn't already have
   the tag, then check that there is room for the tag. Returns True
   if successful. */
static bool_t add_tag_to_domain(DomainMgr_st *dm_st, PerDomain_st *st,
				Security_Tag tag, bool_t inherit)
{
    uint32_t i;
    dcb_link_t *dom, *hp;
    bool_t found;

    found=LongCardTbl$Get(dm_st->tag_tbl, tag, (addr_t *)&hp);

    if (!found) return False; /* Unknown tag */

    dom=Heap$Malloc(Pvs(heap), sizeof(*dom));
    if (!dom) {
	eprintf("Security: out of memory\n");
	return False;
    }

    for (i=0; i<SECURITY_TAGS_PER_DOMAIN; i++) {
	if (st->tags[i]==0) {
	    st->tags[i]=tag;
	    st->inherit[i]=False;
	    break;
	}
    }

    if (i==SECURITY_TAGS_PER_DOMAIN) {
	FREE(dom);
	return False; /* No room */
    }

    dom->dcb = st->rop;
    LINK_ADD_TO_HEAD(hp, dom);

    return True;
}

/* Remove a tag from a domain: check that the domain has the tag, then
   remove it from the domain. Delete the tag if no domains own it. */
static bool_t remove_tag_from_domain(DomainMgr_st *dm_st,
				     PerDomain_st *dom, Security_Tag tag)
{
    uint32_t i;
    dcb_link_t *hp, *d;
    bool_t found;

    /* Find the tag */
    found=LongCardTbl$Get(dm_st->tag_tbl, tag, (addr_t *)&hp);

    if (!found) return False; /* Unknown tag */

    found=False;
    /* Look for the domain in the list */
    for (d=hp->next; d!=hp; d=d->next) {
	if (d->dcb==dom->rop) {
	    found=True;
	    break;
	}
    }

    if (!found) return False; /* Domain doesn't own tag */

    /* Ok, the domain owns the tag; remove it from the dcb and from
       the list */
    for (i=0; i<SECURITY_TAGS_PER_DOMAIN; i++) {
	if (dom->tags[i]==tag) {
	    dom->tags[i]=0;
	    dom->inherit[i]=False;
	}
    }

    LINK_REMOVE(d);

    delete_tag_if_free(dm_st, tag);

    return True;
}

/* Delete the tag from the table of valid tags if no domains currently
   own it. Returns True if the tag was deleted. */
static bool_t delete_tag_if_free(DomainMgr_st *dm_st, Security_Tag tag)
{
    dcb_link_t *hp;
    bool_t found;

    found=LongCardTbl$Get(dm_st->tag_tbl, tag, (addr_t *)&hp);

    if (!found) return False;

    if (LINK_EMPTY(hp)) {
	FREE(hp);
	LongCardTbl$Delete(dm_st->tag_tbl, tag, (addr_t *)&hp);
	return True;
    }

    return False;
}

/*---------------------------------------- Hooks for domain manager ----*/

void Security_InitDomain(DomainMgr_st *dm_st,
			 PerDomain_st *st,
			 StretchAllocator_clp salloc)
{
    PerDomain_st *pst;
    uint32_t i;

    TRC(eprintf("Security: init domain\n"));
    CL_INIT(st->security_cl, &security_ms, st);
    
    TRY {
	IncDomRef(dm_st, st->parent, &pst);
	
	for (i=0; i<SECURITY_TAGS_PER_DOMAIN; i++) {
	    if (pst->tags[i] && pst->inherit[i]) {
		add_tag_to_domain(dm_st, st, pst->tags[i], pst->inherit[i]);
		eprintf("Security: tag %qx inherited\n");
	    }
	}
    } FINALLY {
	DecDomRef(dm_st, pst);
    } ENDTRY;
    
}

void Security_FreeDomain(DomainMgr_st *dm_st, PerDomain_st *bst)
{
    uint32_t i;

    TRC(eprintf("Security: free domain\n"));

    /* For all tags owned by this domain, delete the domain from the
       list of owners of the tag. Send announcements for tags that no
       longer exist (and clean up the table). */

    for (i=0; i<SECURITY_TAGS_PER_DOMAIN; i++) {
	remove_tag_from_domain(dm_st, bst, bst->tags[i]);
    }
    
}

void Security_Init(DomainMgr_st *dm_st)
{
    IDCOffer_clp offer;
    IDCService_clp service;
    Type_Any sec_any;
    IDCTransport_clp shmt = 
	NAME_FIND("modules>ShmTransport", IDCTransport_clp);

    PerDomain_st *bst = ROP_TO_PDS(dm_st->dm_dom);

    TRC(eprintf("Security: initialising\n"));

    dm_st->tag_tbl=LongCardTblMod$New(dm_st->lctmclp, Pvs(heap));
    dm_st->next_tag=1;

    CL_INIT(dm_st->security_cb, &callback_ms, dm_st);
    CL_INIT(bst->security_cl, &security_ms, bst);

    ANY_INIT(&sec_any, Security_clp, &bst->security_cl);

    offer = IDCTransport$Offer(shmt, &sec_any, &dm_st->security_cb,
			       Pvs(heap), Pvs(gkpr), Pvs(entry), &service);

    CX_ADD("sys>SecurityOffer", offer, IDCOffer_clp);
}
