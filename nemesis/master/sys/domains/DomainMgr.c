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
 **      Domain Manager
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Creates and manages domains.
 ** 
 ** ENVIRONMENT: 
 **
 **	Called as library code in the Nemesis domain.
 **
 ** 
 ** ID : $Id: DomainMgr.c 1.2 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
 ** 
 **
 */


#include <nemesis.h>
#include <domid.h>
#include <dcb.h>
#include <sdom.h>
#include <stdio.h>
#include <exceptions.h>
#include <contexts.h>
#include <pqueues.h>
#include <links.h>
#include <ecs.h>
#include <salloc.h>
#include <autoconf/measure_kernel_accounting.h>

#include <VPMacros.h>

#include <Plumber.ih>
#include <Binder.ih>
#include <IDCTransport.ih>
#include <LongCardTblMod.ih>
#include <GatekeeperMod.ih>
#include <EntryMod.ih>
#include <ObjectTblMod.ih>

#include <DomainMgr_st.h>

#include <mmu_tgt.h>

#include <pip.h>
#include <IDCMacros.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define DB(_x)

/*
 * DomainMgr Methods
 */
static DomainMgr_NewDomain_fn     NewDomain;     
static DomainMgr_AddContracted_fn AddContracted; 
static DomainMgr_AddBestEffort_fn AddBestEffort; 
static DomainMgr_Remove_fn        Remove;        
static DomainMgr_Destroy_fn       Destroy;
static DomainMgr_op dm_ms = { 
    NewDomain,
    AddContracted,
    AddBestEffort,
    Remove,
    Destroy,
};

/*
 * Module stuff
 */
static DomainMgrMod_New_fn New;
static DomainMgrMod_Done_fn Done;
static DomainMgrMod_op mod_ms = { New, Done };
static const DomainMgrMod_cl mod_cl = { &mod_ms, (addr_t) 0 };
CL_EXPORT(DomainMgrMod,DomainMgrMod,mod_cl);

/*
 * IDCCallback methods 
 */
static	IDCCallback_Request_fn 		DMgr_Request_m, SAlloc_Request_m;
static	IDCCallback_Bound_fn 		DMgr_Bound_m,   SAlloc_Bound_m;
static	IDCCallback_Closed_fn 		DMgr_Closed_m,  SAlloc_Closed_m;

static	IDCCallback_op	dm_callback_ms = {
  DMgr_Request_m,
  DMgr_Bound_m,
  DMgr_Closed_m
};

static  IDCCallback_op salloc_callback_ms = {
  SAlloc_Request_m,
  SAlloc_Bound_m,
  SAlloc_Closed_m
};

extern Plumber_clp Plumber;	/* from Plumber.c */



/*
 * Create a domain manager
 */

static DomainMgr_clp New(DomainMgrMod_cl *self, StretchAllocatorF_clp sa,
			 LongCardTblMod_clp lctm, FramesF_clp framesF,
			 MMU_cl *mmu, VP_clp vp, Time_clp tp, addr_t k)
{
    StretchAllocator_cl *sysalloc; 
    Stretch_clp	  stretch;
    Stretch_Size  ssize;
    PerDomain_st *st;
    DomainMgr_st *dm_st;
    word_t 	  req;

    TRC(eprintf("Entered DomainMgrMod$New: \n"));

    TRC(eprintf("DomainMgrMod$New: finding SysAlloc.\n"));
    sysalloc = NAME_FIND("sys>SysAlloc", StretchAllocator_clp);

    TRC(eprintf("DomainMgrMod$New: grabbing stretch for state\n"));
    req     = sizeof(DomainMgr_st) + sizeof(PerDomain_st);
    stretch = STR_NEW_SALLOC(sysalloc, req);
    dm_st   = STR_RANGE(stretch, &ssize);

    /* For initialisation purposes, we point the initial domainmgr
       state at a rather sparsely initialised PerDomain_st. We'll set
       it to point to a real PerDomain_st (in Nemesis' rop) later. */

    st      = (PerDomain_st *)(dm_st + 1);
    memset(dm_st, 0, req);
    TRC(eprintf("DomainMgrMod$New: state at %p, dm_st at %p\n", st, dm_st));

    CL_INIT(st->dm_cl, &dm_ms, st);     /* fill in the closure    */
    st->rop   = NULL;                   /* NULL means nemesis dom */
    st->dm_st = dm_st;                  /* point to shared state  */

    /* setup the shared state */

    /* Save kernel state pointer for future reference */
    dm_st->kst = k;
    
    /* save closures we were given */
    dm_st->lctmclp  = lctm;
    dm_st->framesF  = framesF;
    dm_st->mmuclp   = mmu;
    dm_st->vpclp    = vp;
    dm_st->time     = tp;


    /* Stretch information for later */
    dm_st->stretch   = stretch;
    dm_st->saF 	     = sa;
    dm_st->sysalloc  = sysalloc;
    dm_st->sa_entry  = (Entry_cl *)NULL;
    CL_INIT(dm_st->callback, &dm_callback_ms, dm_st);

    /* Next domain ID to be allocated */
    dm_st->next = DOM_NEMESIS;
  
    /* XXX Find the ShmIDC ops */
    TRC(eprintf("DomainMgrMod$New: finding Shm IDC closure\n"));
    {
      IDC_clp shmidc;

      shmidc = NAME_FIND ("modules>ShmIDC", IDC_clp);
      TRC(eprintf(" + BinderShmInfo->ShmIDC = %p\n", shmidc));
      dm_st->shmidc = shmidc;
    }

    TRC(eprintf("DomainMgrMod$New: creating sdom heap.\n"));
    {
	HeapMod_clp	  hm  = NAME_FIND("modules>HeapMod", HeapMod_clp);
	Stretch_clp s;

	/* We allocate enough space for a bunch of sdoms */
	s = STR_NEW_SALLOC(sysalloc, (4*CFG_DOMAINS) * sizeof(SDom_t));
	SALLOC_SETGLOBAL(sysalloc, s, SET_ELEM(Stretch_Right_Read));
	dm_st->sdomheap = HeapMod$NewGrowable(hm, s, 1); 
    }



    /* Initialise the list of all domains */
    LINK_INITIALISE(&(dm_st->list));
    dm_st->list.dcb = (dcb_ro_t *)0;
    dm_st->initComplete = False;

    /* XXX SMH: simple admission control: store parts per million of CPU */
    dm_st->ppm = (uint64_t)0;

    TRC(eprintf("DomainMgrMod$New: done.\n"));
    return &st->dm_cl;
}

/*----------------------------------------- IDCCallback Entry Points ----*/

static bool_t DMgr_Request_m (
    IDCCallback_cl	     *self,
    IDCOffer_clp	      offer	/* IN */,
    Domain_ID	              dom	/* IN */,
    ProtectionDomain_ID       pdid	/* IN */,
    const Binder_Cookies     *clt_cks	/* IN */,
    Binder_Cookies	     *svr_cks	/* OUT */ )
{
    return True;
}

static bool_t DMgr_Bound_m (
    IDCCallback_cl	   *self,
    IDCOffer_clp	    offer	/* IN */,
    IDCServerBinding_clp    binding	/* IN */,
    Domain_ID               dom         /* IN */,
    ProtectionDomain_ID     pdid        /* IN */,
    const Binder_Cookies   *clt_cks     /* IN */,
    Type_Any               *server      /* IN OUT */)
{
    DomainMgr_st *dm_st = self->st;
    PerDomain_st    *st;
    dcb_ro_t *rop;
    bool_t found;

    MU_LOCK(&dm_st->mu);
    
    found = LongCardTbl$Get(dm_st->dom_tbl, dom, (addr_t *)(&rop));

    MU_RELEASE(&dm_st->mu);

    if(!found) {
	/* Bizarre - we've lost the domain - maybe it's currently
           being destroyed? */
	return False;
    }

    st = ROP_TO_PDS(rop);
    
    TRC(eprintf("Setting DM closure for domain %x to %p\n", 
		(uint32_t)dom, &st->dm_cl));
    server->val = (pointerval_t) (&st->dm_cl);

    return True;
}

static void DMgr_Closed_m (
    IDCCallback_cl	       *self,
    IDCOffer_clp	        offer	/* IN */,
    IDCServerBinding_clp	binding	/* IN */,
    const Type_Any         *server  /* IN */)
{

}

static bool_t SAlloc_Request_m (
    IDCCallback_cl	     *self,
    IDCOffer_clp	      offer	/* IN */,
    Domain_ID	              dom	/* IN */,
    ProtectionDomain_ID       pdid	/* IN */,
    const Binder_Cookies     *clt_cks	/* IN */,
    Binder_Cookies	     *svr_cks	/* OUT */ )
{
    PerDomain_st *st = self->st;
    
    /* The owning domain can bind to this SAlloc. So can the parent,
       but only until the child binds */

    if ((dom == st->rop->id) && (st->binding == NULL)) {
	return True;
    }

    if((dom == st->parent) && (st->parent_binding == NULL)) {
	return True;
    }

    eprintf("Refusing SAlloc connection from domain %qx\n", dom);
    eprintf("Owning domain = %qx, parent = %qx\n", st->rop->id, st->parent);
    eprintf("Owning binding = %p, parent binding = %p\n",
	    st->binding, st->parent_binding);

    return False;

}

static bool_t SAlloc_Bound_m (
    IDCCallback_cl	   *self,
    IDCOffer_clp	    offer	/* IN */,
    IDCServerBinding_clp    binding	/* IN */,
    Domain_ID               dom         /* IN */,
    ProtectionDomain_ID     pdid        /* IN */,
    const Binder_Cookies   *clt_cks     /* IN */,
    Type_Any               *server      /* IN OUT */)
{

    PerDomain_st *st = self->st;
    
    if(dom == st->rop->id) {

	Domain_ID parent = st->parent;
	TRC(eprintf("SAlloc: Domain %04x bound to own SAlloc\n",
		    (uint32_t) dom));
	
	/* This is the domain who owns the stretch allocator */
	
	/* If the parent is currently connected, disconnect them */
	
	/* Reset the parent field so the parent can no longer access
           the StretchAllocator.  */
	st->binding = binding;
	st->parent = 0;
	
	if(st->parent_binding) {
	
	    eprintf("SAlloc: Disconnecting parent %04x\n", 
		    (uint32_t) parent);
    
	    IDCServerBinding$Destroy(st->parent_binding);

	}



    } else if (dom == st->parent) {

	/* This is the parent of the domain, using the stretch
           allocator to set things up for the child */
	
	TRC(eprintf("SAlloc: Domain %04x bound to SAlloc of child (%04x)\n",
		    (uint32_t) dom, (uint32_t) st->rop->id));

	st->parent_binding = binding;

    }
    
    return True;
}

static void SAlloc_Closed_m (
    IDCCallback_cl	       *self,
    IDCOffer_clp	        offer	/* IN */,
    IDCServerBinding_clp	binding	/* IN */,
    const Type_Any             *server  /* IN */)
{
    PerDomain_st *st = self->st;
    
    if(st->parent_binding == binding) {
	TRC(eprintf("SAlloc: Domain %04x's SAlloc released by parent\n",
		    (uint32_t) st->rop->id));

	st->parent_binding = NULL;
    } else {

	/* This is a disconnection from the child */
	TRC(eprintf("SAlloc: domain %04x is dying (closing salloc binding)\n", 
		    (uint32_t) st->rop->id));
	st->binding = NULL;
    }
}


/*
 * Finish initialisation of the Domain Manager once its main
 * thread is up.  Called from Nemesis.c.  Compare with Builder_Proto_Main.
 */
static void Done(DomainMgrMod_cl *self, DomainMgr_clp d)
{
    DomainMgr_st *st = ((PerDomain_st *)d->st)->dm_st;

    TRC(eprintf("DomainMgrMod$Done: called. dm id=%x\n", st->dm_dom->id));

    MU_INIT(&st->mu);
    st->initComplete = True;
    st->dom_tbl = LongCardTblMod$New (st->lctmclp, Pvs(heap));
    LongCardTbl$Put (st->dom_tbl, st->dm_dom->id, st->dm_dom);

#if 0
    st->boot_regs = EC_NEW();
/* XXX on Intel an Event_Val is only 32 bits, while a domain ID is 64 bits.
   We truncate the domain ID to 32 bits for this. */
/* XXX this may also be necessary for ARM. Is there a define that
   says whether we are 32 bit or 64 bit? */
#ifndef __ALPHA__
    {
	Event_Val wibble;
	wibble=st->dm_dom->id+1;
	EC_ADVANCE(st->boot_regs, wibble);
    }
#else
    EC_ADVANCE (st->boot_regs, (Event_Val) (st->dm_dom->id + 1));
#endif
#endif /* 0 */

    /* Slap our stretch allocator into pvs */
    Pvs(salloc)= (StretchAllocator_cl *)st->saF;

    TRC(eprintf("DomainMgrMod$Done: creating gatekeeper.\n"));
   {
	HeapMod_clp	  hm  = NAME_FIND("modules>HeapMod", HeapMod_clp);
	GatekeeperMod_clp gkm = NAME_FIND("modules>GatekeeperMod", 
					  GatekeeperMod_clp);
	Stretch_clp s;

	/* Get a stretch big enough to hold idc buffers to and from each
	   domain that will be created.  2 frames per domain and one for
	   the pot.  XXX - eventually use a sensible Gatekeeper impl. and
	   heaps that can grow. */
	s= STR_NEW_SALLOC(st->sysalloc, FRAME_SIZE*(2*CFG_DOMAINS + 1));
	Pvs(gkpr) = GatekeeperMod$NewGlobal (gkm, s, hm, Pvs(heap));
    }

    TRC(eprintf("DomainMgrMod$Done: creating entry.\n"));
    {
	EntryMod_clp etm = NAME_FIND ("modules>EntryMod", EntryMod_clp);
    
	Pvs(entry) = EntryMod$New (etm, Pvs(actf), Pvs(vp), 4*CFG_DOMAINS);
    } 

    /* The rest of this proc arranges that the domain manager domain can export
       services itself. */

    TRC(eprintf("DomainMgrMod$Done: get local binder closure.\n"));
    Pvs(bndr) = st->binder;	/* initialised by Binder_InitFirstDomain */

    TRC(eprintf("DomainMgrMod$Done: creating object table.\n"));
    {
	ObjectTblMod_clp otm = 
	    NAME_FIND("modules>ObjectTblMod", ObjectTblMod_clp);
	Pvs(objs) = ObjectTblMod$New(otm, Pvs(heap), Pvs(bndr), &st->bcb);
    }

    TRC(eprintf("DomainMgrMod$Done: initialising stub generator.\n"));
    {
	Closure_clp sg_init = NAME_FIND("modules>StubGenInit", Closure_clp);
	Closure$Apply(sg_init);
    }

    { 
	IDCTransport_clp shmt = 
	    NAME_FIND("modules>ShmTransport", IDCTransport_clp);

	IDCService_clp service;
	IDCOffer_clp offer;
	Type_Any dm_any;
	Context$Get(Pvs(root), "sys>DomainMgr", &dm_any);



	TRC(eprintf("DomainMgrMod$Done: exporting sys>DomainMgrOffer.\n"));

	offer = IDCTransport$Offer(shmt, &dm_any, &st->callback,
				   Pvs(heap), Pvs(gkpr),
				   Pvs(entry), &service);
	
	CX_ADD("sys>DomainMgrOffer", offer, IDCOffer_clp);
	
    }

    Binder_Done (st);

    Security_Init(st);

    TRC(eprintf("DomainMgrMod$Done: done.\n"));
}

/************************************************************************
 * 
 * DomainMgr interface implementation: create domains
 *
 ************************************************************************/

/*
** Create a new domain: this method may be used regardless of 
** of QoS requirements. The Addxxx() methods are subsequently used
** to add the created domain to the scheduler.
*/
static VP_clp NewDomain(DomainMgr_cl *self, Activation_clp avec, 
			ProtectionDomain_ID *pdid, uint32_t nctxts, 
			uint32_t neps, uint32_t nframes, uint32_t actstrsz, 
			bool_t k, string_t name, /* RETURNS */ 
			Domain_ID *id, IDCOffer_clp *salloc_offer)
{
    DomainMgr_st *st = ((PerDomain_st *)self->st)->dm_st;
    PerDomain_st *newclient_st;
    StretchAllocator_SizeSeq    *sizes;
    StretchAllocator_StretchSeq *stretches;
    Stretch_Size sizero, sizerw;
    Stretch_clp	 stretchro, stretchrw, actstr;
    Context_clp  domcx; 
    dcb_ro_t 	*newro, *parent;
    dcb_rw_t  	*newrw;
    bool_t       first_domain;
    word_t       i; 

    TRC(eprintf("DomainMgr$NewDomain: entered.\n"));
    parent = ((PerDomain_st *)self->st)->rop;

    /* Create a protection domain if we haven't been given one */
    if (*pdid == NULL_PDID) {

	TRC(eprintf("DomainMgr$New: creating new pdom via MMU!\n"));
	if( (*pdid = MMU$NewDomain(st->mmuclp)) == NULL_PDID) {
	    eprintf("DomainMgr$New: failed to allocate protection domain\n"); 
	    return (VP_cl *)NULL;
	}

    }

    /* Update the protection domain's reference count */
    MMU$IncDomain(st->mmuclp, *pdid); 

    /* Fix up the number of contexts: minimum 4, and a power of 2 */
    for( i = 4; i != ((word_t) 1) << (WORD_SIZE * 8 - 2); i = i << 1 )
	if ( i >= nctxts ) { nctxts = i; break; }

    /* Do the same for end-points */ 
    for( i = 4; i !=  ((word_t) 1) << (WORD_SIZE * 8 - 2); i = i << 1 )
	if ( i >= neps ) { neps = i; break; }
  
    /* Work out the sizes of the stretches needed */
    sizes = SEQ_NEW(StretchAllocator_SizeSeq, 2, Pvs(heap));


    /* Get the minimum size */
    SEQ_ELEM(sizes,0) = sizeof(dcb_ro_t) + neps * sizeof(ep_ro_t);
    SEQ_ELEM(sizes,1) = sizeof(dcb_rw_t) 
	+ nctxts *  sizeof(context_t) 
	+ neps   * (sizeof(ep_rw_t) + sizeof(Channel_EP));

    /* Acquire two contiguous stretches for the two halves of the dcb */
    TRC(eprintf("DomainMgr$NewDomain: Grabbing stretches...\n"));
    stretches = STR_NEWLIST_SALLOC(st->sysalloc, sizes);
    TRC(eprintf("DomainMgr$NewDomain: returned %p\n", stretches));
    stretchro = SEQ_ELEM(stretches, 0);
    stretchrw = SEQ_ELEM(stretches, 1);
    TRC(eprintf("DomainMgr$NewDomain: Got stretches RO:%p RW:%p\n",
		stretchro, stretchrw));

    TRC(eprintf("DomainMgr$NewDomain: freeing:\n"
		"  sizes heap=%p\n" 
		"  stretches heap=%p\n"
		"  pvs(heap)=%p ... ",
		HEAP_OF(sizes), HEAP_OF (stretches), Pvs(heap)  ));
    SEQ_FREE(sizes);
    SEQ_FREE(stretches);
    TRC(eprintf ("done\n"));
  
    /* Where are they? */
    newro = STR_RANGE(stretchro, &sizero );
    newrw = STR_RANGE(stretchrw, &sizerw );

    memset(newro, 0, sizero);
    memset(newrw, 0, sizerw);

    TRC(eprintf("DomainMgr$New: dcbro at %p +%x\n", newro, sizero));
    TRC(eprintf("DomainMgr$New: dcbrw at %p +%x\n", newrw, sizerw));


#ifndef CONFIG_MEMSYS_EXPT
    /* XXX PBM: nasty hack again; map IO rights to dom if k. If the
       domain is sharing a pdom with someone else, they have IO access
       now too ... */
    if (k && (!parent || PRIV(parent))) {

	Stretch_clp iostr = NAME_FIND("sys>IOStretch", Stretch_clp);
	
	/* XXX SMH: need to grant meta rights since we ourselves come 
	   through this piece of code. This is all vile vile vile. */
	TRC(eprintf("DomainMgr$New: Giving access to IO stretch\n"));
	MMU$SetProt(st->mmuclp, *pdid, iostr, 
		       SET_ELEM(Stretch_Right_Read)  | 
		       SET_ELEM(Stretch_Right_Write) |
		       SET_ELEM(Stretch_Right_Meta));
    }
#endif

    /* 
     * Build Dcbro
     */
    newro->id        = (st->next)++;
    newro->fen       = 0;      /* Floating point not enabled by default */
    newro->sdomptr   = NULL;   /* Note that there is *no* sdom yet      */
    newro->pdid     = *pdid;   /* Store our protection domain ID        */
    newro->num_eps   = neps;
    newro->ep_mask   = neps - 1;
    newro->epros     = (ep_ro_t *)(newro + 1);
    newro->eprws     = (ep_rw_t *)(newrw + 1);
    newro->ep_fifo   = (word_t *)(newro->eprws + neps);
    newro->dcbrw     = newrw;
    newro->ctxts     = (context_t *)(newro->ep_fifo + neps);
    newro->num_ctxts = nctxts;
    newro->ctxts_mask= nctxts - 1;
    newro->rostretch = stretchro;
    newro->rwstretch = stretchrw;
#if defined(INTEL) || defined(__ARM__)
    newro->outevs_processed = 0;
#endif

    CL_INIT(newro->vp_cl, st->vpclp->op, newrw );
    LINK_INITIALISE(&newro->link);

    TRC(eprintf("DomainMgr$New: about to zero the fifo.\n"));

    /* Zero the fifo: there is no real need for this, but it makes it
       easier to read crash dumps! */
    for (i = 0; i < neps; i++)
	newro->ep_fifo[i] = 0;

    /*
     * Build Dcbrw
     */
    TRC(eprintf("DomainMgr: building Dcbrw at %p\n", newrw));

    for( i=0; i < DCBRW_W_STKSIZE / sizeof(word_t); i++ )
	newrw->stack[i] = 0;

    newrw->activ_clp = avec;
    /* not everyone supplies a valid activation vector */
    if (avec && avec->op 
	/* XXX extra paranoia heuristics by RJB */
	/* Either avec->op should point to just after avec as in
         * crt0.S, or it came from a CL_EXPORT in which case one would
         * expect a typed_ptr_t in between */
	&& (((word_t *)avec->op == (word_t *)avec+1) 
	    || ((word_t *)(avec->op)+1 == (word_t *)avec) )
	&& avec->op->Go)
    {
	newrw->activ_op  = avec->op->Go;
    }
    newrw->actctx = newro->num_ctxts; /* Activ ctxt invalid (VP.if)	*/
    newrw->resctx = 0;		/* Use context 0 when actns off	*/
    newrw->mode   = 0;		/* Activate first		*/
    newrw->status = 0;		/* Status null			*/

    /* Initialise the context slots and free-list */
    newrw->free_ctxts = 1;	/* 0 is used for resumption	*/
    for (i = 1; i < newro->num_ctxts; i++)
    {
	context_t *cx = newro->ctxts + i;
	memset (cx, 0, sizeof (context_t));
	cx->CX_FREELINK = (word_t) (i + 1);
    }

    newrw->ep_head = newrw->ep_tail = 0;

    /* Setup the memory management fields */
    newrw->mm_ep     = NULL_EP;
    newrw->mm_code   = 0;
    newrw->mm_va     = 0;
    newrw->mm_str    = NULL;

    /* Setup the back pointer [fstackmod goes though vp] */
    newrw->dcbro   = newro;

#if defined(INTEL) || defined(__ARM__)
    /* Set up the outgoing event FIFO */
    newrw->outevs_pushed = 0;
#endif

    TRC(eprintf("DomainMgr$New: creating client frames allocator.\n"));
    {
	/* Create a client frames allocator */
	uint32_t newroPA, gtdf, xtraf, nf; 
	word_t   pte;

	if((pte = ntsc_trans((word_t)newro >> PAGE_WIDTH)) == MEMRC_BADVA) {
	    eprintf("Urk! Cannot trans dcbro [va = %p]\n", newro);
	    ntsc_dbgstop();
	}
	newroPA = PFN_PTE(pte) << PAGE_WIDTH; 

	TRC(eprintf("DCBRO va = %p, pa = %p\n", newro, newroPA));

#ifdef CONFIG_MEMSYS_EXPT
	gtdf   = nframes * 4;   /* XXX guess; set quota at 4 times initial */
	xtraf  = gtdf;          /* XXX guess; set opt quota at same        */
	nf     = nframes;       /* allocate nframes at start               */
#else 
	gtdf    = -1;               /* No quota on guaranteed frames       */
	xtraf   = -1;               /* No quota on extra frames            */
	nf      = 0;                /* Don't allocate any frames initially */
#endif
	
	newro->frames = FramesF$NewClient(st->framesF, newro, newroPA, gtdf, 
					  xtraf,  nframes); 
	TRC(eprintf("DomainMgr$New: got new frames iface at %p\n", 
		    newro->frames));
    }

    /* These will be filled in by the caller (since it should know them!) */
    newrw->textstr = NULL;
    newrw->datastr = NULL;
    newrw->bssstr  = NULL;

    /* Allocate a nailed stretch for use by the new domain */
    if(actstrsz) {
	TRC(eprintf("DomainMgr$New: d%qx='%s' allocing %dKb for actstr.\n", 
		    newro->id, name, (actstrsz >> 10)));
	actstr = STR_NEW_SALLOC(st->sysalloc, actstrsz);
	TRC(eprintf("DomainMgr$New: got activation stretch at %p\n", actstr));
    } else {
	TRC(eprintf("DomainMgr$New: d%x no activation stretch requested.\n", 
		newro->id));
	actstr = NULL;
    }
    newrw->actstr = actstr;

    /*
     * Map the stretches.
     */
    SALLOC_SETPROT(st->sysalloc, stretchro, *pdid, 
		   SET_ELEM(Stretch_Right_Read));
    SALLOC_SETPROT(st->sysalloc, stretchrw, *pdid, 
		   SET_ELEM(Stretch_Right_Read)
		   | SET_ELEM(Stretch_Right_Write) );
    if(actstr) 
	SALLOC_SETPROT(st->sysalloc, actstr, *pdid, 
		       SET_ELEM(Stretch_Right_Read) |
		       SET_ELEM(Stretch_Right_Write) |
		       SET_ELEM(Stretch_Right_Meta) );
    
    /* If the parent has a different pdid, map dcb & actstr there too */
    if(parent && parent->pdid != *pdid) {
	SALLOC_SETPROT(st->sysalloc, stretchro, parent->pdid,
		       SET_ELEM(Stretch_Right_Read));
	SALLOC_SETPROT(st->sysalloc, stretchrw, parent->pdid, 
		       SET_ELEM(Stretch_Right_Read)
		       | SET_ELEM(Stretch_Right_Write));
	if(actstr)
	    SALLOC_SETPROT(st->sysalloc, actstr, parent->pdid,
			   SET_ELEM(Stretch_Right_Read)
			   | SET_ELEM(Stretch_Right_Write) );
    }

    /* Initialise all of the event datastructures */
    Plumber$Init(Plumber, newro);

#if (defined(DEBUG) || defined(TRACE_DOMAIN_START))
    eprintf("DomainMgr$NewDomain('%s', %x, %p)\n", 
	    name, (uint32_t) newro->id, newro);
#endif
    /* Go do the architecture dependent initialisation */
    Arch_InitDomain(st, newro, newrw, (k && (!parent || PRIV(parent))));
    /*
     * Add the domain to the complete list.
     */
    newro->link.dcb = newro;

    first_domain = !st->initComplete;
    if(first_domain) {
	LINK_ADD_TO_TAIL(&st->list,&newro->link);
    } else {
	MU_LOCK(&st->mu);
	LINK_ADD_TO_TAIL(&st->list, &newro->link);

	/* Enter the domain in the table mapping domain ID's to DCBRO's.*/
	LongCardTbl$Put(st->dom_tbl, newro->id, newro);
	MU_RELEASE(&st->mu);
    }

    /* export the dcbro and name */
    TRC(eprintf("DCBRO: exporting dcbro\n"));
    {
	char domname[32];
	ContextMod_clp cmod;
	Context_clp proc_cx;

	/* Create new context for this domain */
	cmod   = NAME_FIND("modules>ContextMod", ContextMod_clp);
	domcx  = ContextMod$NewContext(cmod, Pvs(heap), Pvs(types));
	
	/* Add domain info into the new context */
	CX_ADD_IN_CX(domcx, "dcbro", newro, addr_t);
	CX_ADD_IN_CX(domcx, "kpriv", k, bool_t);
	CX_ADD_IN_CX(domcx, "neps", neps, uint32_t);
	CX_ADD_IN_CX(domcx, "nctxts", nctxts, uint32_t);
	CX_ADD_IN_CX(domcx, "protdom", *pdid, ProtectionDomain_ID);
	CX_ADD_IN_CX(domcx, "avec", avec, Activation_clp);
	CX_ADD_IN_CX(domcx, "vp", &newro->vp_cl, VP_clp);
	
	/* Finally, add the new context into >proc>domains> */
	proc_cx = NAME_FIND("proc>domains", Context_clp);
	sprintf(domname, "%qx", newro->id);
	if (!first_domain) {
	    IDCOffer_clp offer;
	    IDCService_clp service;
	    Type_Any domcx_any;
	    IDCTransport_clp shmt = 
		NAME_FIND("modules>ShmTransport", IDCTransport_clp);

	    ANY_INIT(&domcx_any, Context_clp, domcx);

	    /* export this context in itself, so people can bind to it and
	       add things if need arises */

	    
#if 1
	    offer = IDCTransport$Offer(shmt, &domcx_any, NULL,
				       Pvs(heap), Pvs(gkpr),
				       Pvs(entry), & service);
#endif

	    CX_ADD_IN_CX(domcx, "thiscontext", offer, IDCOffer_clp);
	}


	CX_ADD_IN_CX(proc_cx, domname, domcx, Context_clp);
    }

    /*
     * Initialise Binder connection - first domain is special.
     */
    if (first_domain) {
	PerDomain_st *pds;
	st->dm_dom = newro; /* This is the Nemesis domain */

	/* Swizzle our state to point to the real PerDomain_st */
	pds = ROP_TO_PDS(newro);
	self->st = pds;
	pds->dm_st = st;
	pds->rop = NULL;
	
	TRC(eprintf("DomainMgr: Binding to binder (first domain).\n"));
	Binder_InitFirstDomain (st, newro, newrw);

	*salloc_offer = NULL;
	newclient_st  = self->st;
	newclient_st->parent = 0;

	
    } else {
        StretchAllocator_clp sa; 
	IDCService_clp service;
	IDCTransport_clp shmt = 
	    NAME_FIND("modules>ShmTransport", IDCTransport_clp);
	Type_Any fs_any, sa_any;
	ProtectionDomain_ID ppdid;

	ppdid = parent ? parent->pdid : NULL_PDID;


	/* Now setup the stretch allocator */
	TRC(eprintf("DomainMgr: Creating a new StretchAllocator client.\n"));
	sa = StretchAllocatorF$NewClient(st->saF, *pdid, &newro->vp_cl, ppdid);
	
	TRC(eprintf("DomainMgr: Binding to binder.\n"));
	newclient_st = Binder_InitDomain (st, newro, newrw, sa);
	CL_INIT(newclient_st->dm_cl, &dm_ms, newclient_st);

	ANY_INIT(&fs_any, Frames_clp, newro->frames);
#if defined(CONFIG_BINDER_MUX) 
	newro->frames_offer = Binder_MuxedOffer(st, newro, newrw, &fs_any);
#else
	newro->frames_offer = IDCTransport$Offer(shmt, &fs_any, NULL,
						 Pvs(heap), Pvs(gkpr),
						 Pvs(entry), &service);
#endif  /* CONFIG_BINDER_MUX */


	/* XXX PRB We need to provide the new domain with a way of binding
	   to the stretch allocator.  This can probably be done by
	   providing its gatekeeper with an initial stretch for
	   communication with the Nemesis domain, and an IDCOffer for the
	   StretchAllocator.  This shouldn't be as much trouble as the
	   Binder connection!  */

	newclient_st->salloc = sa;
	newclient_st->parent = parent ? parent->id : DOM_NEMESIS;
	newclient_st->parent_binding = NULL;
	
	ANY_INIT(&sa_any, StretchAllocator_clp, sa);

	/* XXX SMH; we give the stretch allocator its own entry, since the 
	   Gatekeeper may need (later) to invoke the stretch allocator whilst
	   in the middle of an IDC. */
	if(st->sa_entry == (Entry_cl *)NULL) { /* first time here */
	    EntryMod_clp emod; 
	    Closure_clp cl;

	    TRC(eprintf("DomainMgr: Creating StretchAllocator Entry\n"));
	    emod = NAME_FIND("modules>EntryMod", EntryMod_clp);
	    st->sa_entry = EntryMod$New (emod, Pvs(actf), Pvs(vp), 
					 VP$NumChannels (Pvs(vp)));

	    /* Fork a service thread for the stretch allocator entry */
	    TRC(eprintf("DomainMgr: forking SAlloc Entry Thread.\n"));
	    cl = Entry$Closure (st->sa_entry);
	    Threads$Fork (Pvs(thds), cl->op->Apply, cl, 0);

	}
	

	CL_INIT(newclient_st->salloc_cb, &salloc_callback_ms, newclient_st);
	    
	*salloc_offer = IDCTransport$Offer(shmt, &sa_any, 
					   &newclient_st->salloc_cb,
					   Pvs(heap), Pvs(gkpr),
					   st->sa_entry, 
					   &newclient_st->salloc_service);


#ifdef CONFIG_BINDER_MUX
	newro->salloc_offer  = Binder_MuxedOffer(st, newro, newrw, &sa_any);
#else 
	newro->salloc_offer = *salloc_offer;
#endif

	Security_InitDomain(st, newclient_st, sa);
   }

    /* Finally, export the name */
    newclient_st->name = strdup(name);
    CX_ADD_IN_CX(domcx, "name", newclient_st->name, string_t);

    TRC(eprintf("DomainMgr$New: done.\n"));
    
    *id = newro->id;
    return (VP_cl *)&newro->vp_cl;
}



/*
** Add a contracted domain to the scheduler.  */
static void AddContracted(DomainMgr_cl *self, Domain_ID id, 
			       Time_ns p, Time_ns s, Time_ns l, bool_t x)
{
    DomainMgr_st   *st 	= self->st->dm_st;
    PerDomain_st   *pds;
    
    if(st->initComplete) {
	IncDomRef(st, id, &pds);
    } else {
	/* We're in Primal */
	pds = self->st;
    }


    TRY {
	dcb_ro_t       *rop = pds->rop;
	SDom_t         *sdomp;
	uint64_t	requested_ppm = 0;
	Time_ns         time;
	
	if (!p || !l)
	    RAISE_DomainMgr$AdmissionDenied();
	
#define MILLION         ((uint64_t)1000000)
#define MAX_PPM         ((uint64_t) 980000) /* madcap guess FIXME */
	/* SMH: simple check if we can admit this new domain */
	if((((requested_ppm=(s*MILLION)/p)+st->ppm)>MAX_PPM) && False) {
	    eprintf("DomainMgr$AddContracted: cannot admit domain!\n");
	    eprintf("(current ppm is %d, req ppm is %d, max ppm is %d)\n", 
		    (uint32_t)st->ppm, (uint32_t)requested_ppm, 
		    (uint32_t)MAX_PPM);
	    RAISE_DomainMgr$AdmissionDenied();
	} else {
	    st->ppm+= requested_ppm;
	    TRC(
	    if(!x) 
		eprintf(
		    "DomainMgr$NewContracted: admitting domain with no XTRA: "
		    "new CPU %02d %%\n", (uint32_t)((st->ppm*100)/MILLION)));
	    TRC(eprintf("DomainMgr$AddContracted: admitting domain,"
			"new CPU %02d %%\n", 
			(uint32_t)((st->ppm*100)/MILLION)));
	}
	
	TRC(eprintf("DomainMgr$AddContracted: getting time.\n"));
	time = Time$Now(st->time); 
	
	TRC(eprintf("DomainMgr$AddContracted: filling in state at %p\n", rop));
	
	/*
	 * Give it its own scheduling domain.
	 */
	
	/*
	** XXX SMH: for now, we just malloc the sdom on our heap. This is
	** not too bad, but it might be nicer to have a specialist allocation 
	** thing. Also could stick some more per-domain things (e.g. binder 
	** stuff) on heap and not in dcb => makes domain death easier. 
	*/
	sdomp = Heap$Calloc(st->sdomheap, 1, sizeof(*sdomp)); 
	TRC(eprintf("DomainMgr$AddContracted: allocated sdom at %p\n", sdomp));
	sdomp->period   = p;
	sdomp->slice    = s;
	sdomp->deadline = time + p;
	sdomp->prevddln = time;
	sdomp->latency  = l;
	sdomp->remain   = s;
	sdomp->lastschd = time;
	sdomp->lastdsch = 0;
	sdomp->type     = sdom_contracted;
	sdomp->dcb      = rop;
	sdomp->xtratime = x;
	sdomp->ac_g     = 0;
	sdomp->ac_x     = 0;
	sdomp->ac_lp    = 0;
	sdomp->ac_time  = 0;

#ifdef CONFIG_MEASURE_KERNEL_ACCOUNTING
	sdomp->ac_m_lp  = 0;
	sdomp->ac_m_lm  = 0;
	sdomp->ac_m_tm  = 0;

#endif
	
	/* Once valid, setup the sdomptr */
	rop->sdomptr = sdomp; 
	/*
	 * Link domain onto run queue here
	 */
	TRC(eprintf("DomainMgr$AddContracted: adding to scheduler\n"));
	Scheduler_AddDomain(st, rop, rop->dcbrw);
	
	
	TRC(eprintf("DomainMgr: Contracted domain added.\n"));
    } FINALLY {
	if(st->initComplete) {
	    DecDomRef(st, pds);
	}
    } ENDTRY;

    return;
}



/*
** Add a best-effort domain to the scheduler.
*/
static void AddBestEffort(DomainMgr_cl *self, Domain_ID id)
{
#if 0
    DomainMgr_st *st = ((PerDomain_st *)self->st)->dm_st;
    dcb_ro_t     *ro = RO(vp);

    /* Quick check on numbers */
    if (  st->be.n >= CFG_BEDOMAINS ) {
	DB(eprintf("DomainMgr: One too many best-effort domains!\n"));
	/* XXX Raise exception */
	return;
    }
 
    /* Setup the scheduling domain */
    rop->sdomptr = &(st->be.sdom);
  
    /* Link it in to the best-effort queue (currently round-robin!) */
    st->be.domains[ st->be.n ] = ro;
  
    /* Register its existence */
    (st->be.n)++;

    return ro->id;
#endif /* 0 */

}

/*
** Remove a domain from the scheduler.
** Typically used prior to domain destruction.
*/

static void StopDomain(DomainMgr_st *st, dcb_ro_t *rop) {
	
    if(rop->id==DOM_NEMESIS) {
	eprintf("DomainMgr$Remove: cannot remove Nemesis Domain.\n");
	RAISE_Binder$Error(Binder_Problem_Failure);
    }
    
    if(rop->schst == DCBST_K_DYING) {
	eprintf("DomainMgr$Destroy: Trying to kill dying domain - aborting\n");
	RAISE_Binder$Error(Binder_Problem_Dying);
    }
    
    /* Remove the DCB from the scheduler */
    if(rop->schst != DCBST_K_STOPPED) {
	TRC(eprintf("DomainMgr$Remove: removing DCB from scheduler.\n"));
	Scheduler_RemoveDomain(st, rop, DCB_RO2RW(rop));
	
	st->ppm -= ((rop->sdomptr->slice * MILLION) / 
		    rop->sdomptr->period);

	TRC(eprintf("DomainMgr$Remove: removed domain,"
		    "new CPU %02d %%\n", (uint32_t)((st->ppm*100)/MILLION)));
    } else {
	TRC(eprintf("DomainMgr$Remove: Domain is already stopped\n"));
    }
    return;
}

static void Remove(DomainMgr_cl *self, Domain_ID id)
{
    PerDomain_st     *pds;
    DomainMgr_st     *st= ((PerDomain_st *)self->st)->dm_st;
    
    TRC(eprintf("DomainMgr$Remove: entered, domain id is %qx\n", id));

    IncDomRef(st, id, &pds);
    
    TRY {
	StopDomain(st, pds->rop);
    } FINALLY {
	DecDomRef(st, pds);
    } ENDTRY;
}



/*
** Destroy a domain and all its earthly goods.
*/
static void Destroy(DomainMgr_clp self, Domain_ID id) {
    
    PerDomain_st        *pds;
    DomainMgr_st	*st= ((PerDomain_st *)self->st)->dm_st;
    
    IncDomRef(st, id, &pds);
    
    TRY {
	addr_t tmp;
	dcb_ro_t *rop = pds->rop;

	MU_LOCK(&st->mu);
	

	StopDomain(st, rop);

	/* If two people try to destroy the domain at once this could
           return False - so don't try to blow stuff away if
           this happens */

	pds->rop->schst = DCBST_K_DYING;

	if(LongCardTbl$Delete(st->dom_tbl, id, &tmp)) {
	    
	    LINK_REMOVE(&rop->link);

	    /* Perform cleanup on our non-standard connections to the domain */

	    /* Set parent id to zero (an invalid domain id) to prevent them
	       connecting */
	    
	    pds->parent = 0;
	    
	    /* Close SAlloc connections first */
	    if(pds->parent_binding) {
		IDCServerBinding$Destroy(pds->parent_binding);
	    }
	    
	    if(pds->binding) {
		IDCServerBinding$Destroy(pds->binding);
	    }

	    IDCService$Destroy(pds->salloc_service);
	    
	    /* Ask the Binder to forcibly shut down all connections */
	    TRC(eprintf("DomainMgr$Destroy: closing all bindings.\n"));
	    Binder_FreeDomain(st, rop, DCB_RO2RW(rop));

	    /* This will block if we're in the middle of a bcb call to
               this domain - but only briefly, since we've just blown
               the connection away */

	    if(pds->cb_binding) {
		IDCClientBinding$Destroy(pds->cb_binding);
	    }

	    
	    /* Remove its entry from the proc context */
	    TRC(eprintf("DomainMgr$Destroy: removing from proc>\n"));
	    {
		string_t name; 
		char buff[32];
		Context_clp  proc_cx, dom_cx;
		
		proc_cx = NAME_FIND("proc>domains", Context_clp);
		
		sprintf(buff, "%qx", rop->id);
		dom_cx =  NAME_LOOKUP(buff, proc_cx, Context_clp);
		
		Context$Remove(proc_cx, buff);
		
		Context$Remove(dom_cx, "dcbro");
		Context$Remove(dom_cx, "vp");
		name = NAME_LOOKUP("name", dom_cx, string_t);
		TRC(eprintf("DomainMgr$Destroy: killing domain '%s'\n", name));
		Context$Remove(dom_cx, "name");
		
		Context$Destroy(dom_cx);
	    }

	    Security_FreeDomain(st, pds);
	    
	}
    } FINALLY {
	/* This will eventually cause the various chunks of memory to
           be freed */
	MU_RELEASE(&st->mu);

	DecDomRef(st, pds);
    } ENDTRY;
}
    
void FreeDomainState(DomainMgr_st *st, PerDomain_st *pds) {
    
    dcb_rw_t *rwp = pds->rwp;
    dcb_ro_t *rop = pds->rop;

    TRC(eprintf("DomainMgr: Freeing domain state for %qx\n", pds->rop->id));

    SALLOC_DESTROY(pds->salloc);

    /* Free the buffers we allocated for initial IDC */
    STR_DESTROY_SALLOC(st->sysalloc, pds->args);
    STR_DESTROY_SALLOC(st->sysalloc, pds->results);

    /* free up its activation stretch */
    /* XXX PBM This is a tad insecure, since it's in the rwp. Probably
       actstr ought to be in rop */
    STR_DESTROY_SALLOC(st->sysalloc, rwp->actstr);
    
    /* Zap the name (we allocated this on our heap) */
    FREE(pds->name);
    
    /* Decrement the reference count on its pdom  */
    TRC(eprintf( "DomainMgr$Destroy: delete PDom\n"));
    MMU$FreeDomain(st->mmuclp, rop->pdid); 

    /* free up the DCBRO and DCBRW stretches */
    TRC(eprintf( "DomainMgr$Destroy: destroy DCB\n"));
    STR_DESTROY_SALLOC(st->sysalloc, rop->rwstretch);
    STR_DESTROY_SALLOC(st->sysalloc, rop->rostretch);
    
}


/* 
 * Increment the reference count on a domain to ensure that it doesn't
 * get blown away under our feet.
 */
bool_t IncDomRefNoExc(DomainMgr_st *st, Binder_ID d,
		      /* RETURNS */
		      PerDomain_st  **svr ) {
    
    dcb_ro_t *rop;
    bool_t res = True;

    MU_LOCK(&st->mu);
    
    *svr = NULL;

    if (!LongCardTbl$Get (st->dom_tbl, d, (addr_t *)&rop)) {
	res = False;
    } else {
    
	*svr = ROP_TO_PDS(rop);
    
	if(rop->schst == DCBST_K_DYING) {
	    res = False;
	} else {

	    TRC(eprintf("Binder$IncDomRef: domain %x\n", rop));

	    (*svr)->refCount++;
	}
    }

    MU_RELEASE(&st->mu);

    return res;
}
	
    
void IncDomRef(DomainMgr_st *st, Binder_ID d, 
	       /* RETURNS */
	       PerDomain_st  **svr )
{
    
    bool_t ok = IncDomRefNoExc(st, d, svr);

    if(*svr == NULL) {
	RAISE_Binder$Error (Binder_Problem_BadID);
    }

    if(!ok) {
	RAISE_Binder$Error (Binder_Problem_Dying);
    }

}

    
/* 
 * Decrement the reference count on a domain, and destroy it if
 * necessary.
 */
void DecDomRef(DomainMgr_st    *st, 
	       PerDomain_st *pds)
{

    MU_LOCK(&st->mu);
    
    if (--(pds->refCount) == 0 && pds->rop->schst == DCBST_K_DYING)
	FreeDomainState(st, pds);

    MU_RELEASE(&st->mu);
}
