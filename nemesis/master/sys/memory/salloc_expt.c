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
 **      StretchAllocator stuff
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Complete stretch allocator implementation for 'EXPT' memory 
 **      system. Includes 'system' stretch allocator, plus the normal 
 **      one exported to each domain. 
 ** 
 ** ENVIRONMENT: 
 **
 **      Nemesis domain. 
 ** 
 ** ID : $Id: salloc_expt.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <exceptions.h>
#include <kernel.h>
#include <ntsc.h>
#include <links.h>
#include <pip.h>

#include <MMU.ih>
#include <Stretch.ih>
#include <SAllocMod.ih>
#include <StretchAllocator.ih>
#include <StretchAllocatorF.ih>

#include "mmu.h"

#define ETRC(_x) 

// #define STRETCH_TRACE
#ifdef STRETCH_TRACE
#define TRC(_x) _x
#define DEBUG
#else
#define TRC(_x)
#endif

#define DEBUG
#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(x) 
#endif


#define UNIMPLEMENTED eprintf("%s: UNIMPLEMENTED", __FUNCTION__);



/* 
** Lightweight synchronization is achieved by enabling/disabling 
** activations within critical sections. 
** The following macros allow one to do this while preserving the
** original mode.
*/
#include <VPMacros.h>

#define ENTER(_vp, _mode)			\
do { 						\
    if(_vp) {					\
	(_mode) = VP$ActivationMode (_vp); 	\
	VP$ActivationsOff (_vp);		\
    }						\
} while(0)

#define LEAVE(_vp, _mode)			\
do {						\
  if (_vp && _mode)				\
  {						\
    VP$ActivationsOn (_vp);			\
    if (VP$EventsPending (_vp))			\
      VP$RFA (_vp);				\
  }						\
} while (0)





/*
 * Stretch Allocator factory closure
 */
static SAllocMod_NewF_fn      NewF_m;
static SAllocMod_Done_fn      Done_m;
static SAllocMod_op smod_op = { 
    NewF_m,
    Done_m
};

static const SAllocMod_cl smod_cl = { &smod_op, (addr_t)0 };
CL_EXPORT (SAllocMod,SAllocMod,smod_cl);
CL_EXPORT_TO_PRIMAL (SAllocMod, SAllocModCl, smod_cl);


/*
 * Basic stretch allocator state
 */
typedef struct SA_st SA_st;
typedef struct Client_st Client_st; 




/* Per-client state */
struct Client_st {
    struct link_t        link;  /* Link into the list of stretch allocators */
    SA_st               *sa_st;	/* Server (shared) state                    */
    ProtectionDomain_ID  pdid;	/* Protection domain of client              */
    ProtectionDomain_ID  parent;/* Protection domain of client's parent     */
    VP_clp               vp;	/* VP of client                             */

    struct _StretchAllocator_cl cl;

    /* We hang all of the allocated stretches here */
    struct link_t  stretches;
}; 

typedef struct _vas_region {
    Mem_VMemDesc       desc; 
    struct _vas_region *next; 
    struct _vas_region *prev;
} VASRegion;

/* a couple of region and related macros */
#define REG_SIZE(_r)          ((_r)->desc.npages << (_r)->desc.page_width)
#define NPAGES(_b,_w)         (((word_t)_b + (1UL<<(_w))-1) >> (_w))


/* Shared state */
struct SA_st {
    VASRegion     *regions;
    Frames_cl 	  *f;                  /* Only in nailed sallocs */
    Heap_cl	  *h;
    MMU_cl        *mmu;
    uint32_t      *sids;               /* Pointer to table of SIDs in use */
    Stretch_clp   *ss_tab;             /* SID -> Stretch_clp mapping */
    struct link_t  clients;
};

#define SID_ARRAY_SZ (SID_MAX/32)





static	Stretch_Range_fn 		Range_m;
static	Stretch_Info_fn 		Info_m;
static	Stretch_SetProt_fn 		SetProt_m;
static	Stretch_RemProt_fn 		RemProt_m;
static	Stretch_SetGlobal_fn 		SetGlobal_m;
static	Stretch_Query_fn 		Query_m;

static	Stretch_op s_op = {
    Range_m,
    Info_m,
    SetProt_m,
    RemProt_m,
    SetGlobal_m,
    Query_m
};


/*--Stretch methods--------------------------------------------------------*/
static addr_t Range_m(Stretch_cl *self, Stretch_Size *s)
{
    Stretch_st	*st = self->st;

    *s = st->s;
    return st->a;
}

static addr_t Info_m(Stretch_cl *self, Stretch_Size *s)
{
    Stretch_st	*st = self->st;
    
    *s = st->s;
    return st->a;
}


static void SetProt_m(Stretch_cl  *self, ProtectionDomain_ID pdid,
		      Stretch_Rights access)
{
    Stretch_st	*st = self->st;
    uint32_t vpn, npages;
    word_t rc; 

    TRC(eprintf("Stretch$SetProt(%p SID=%x, PDid %x, [%c%c%c%c])\n", 
		self, st->sid, pdid, 
		SET_IN(access, Stretch_Right_Meta)    ? 'M' : '-',
		SET_IN(access, Stretch_Right_Execute) ? 'E' : '-',
		SET_IN(access, Stretch_Right_Write)   ? 'W' : '-',
		SET_IN(access, Stretch_Right_Read)    ? 'R' : '-'));
    /* SMH check for meta rights in done in ntsc/palcode */
    vpn     = (word_t)st->a >> PAGE_WIDTH;
    npages  = (word_t)st->s >> PAGE_WIDTH;
    if(( rc = ntsc_prot(pdid, vpn, npages, access))) {
	eprintf("ntsc_prot returned error -%d\n", -((signed)rc));
	ntsc_dbgstop();
    }
    return;
}

static void RemProt_m(Stretch_cl *self, ProtectionDomain_ID pdid)
{
    Stretch_st	*st = self->st;
    uint32_t vpn, npages;
    word_t rc; 

    /* NB: the check for meta rights in done in ntsc/palcode */
    vpn     = (word_t)st->a >> PAGE_WIDTH;
    npages  = (word_t)st->s >> PAGE_WIDTH;
    if(( rc = ntsc_prot(pdid, vpn, npages, 0))) {
	eprintf("ntsc_prot returned error -%d\n", -((signed)rc));
	ntsc_dbgstop();
    }
    return;
}

static void SetGlobal_m(Stretch_cl *self, Stretch_Rights access)
{
    Stretch_st	*st = self->st;
    uint32_t vpn, npages;
    word_t rc; 

    /* NB: the check for meta rights in done in ntsc/palcode */
    vpn        = (word_t)st->a >> PAGE_WIDTH;
    npages     = (word_t)st->s >> PAGE_WIDTH;
    TRC(eprintf(":: calling gprot on addresses [%p - %p)\n", 
	    st->a, st->a+st->s));
    if( (rc = ntsc_gprot(vpn, npages, access))) {
	eprintf("ntsc_gprot returned error %d\n", rc);
	ntsc_dbgstop();
    } 
    return;
}

static Stretch_Rights Query_m(Stretch_cl *self, ProtectionDomain_ID pdid)
{
    Stretch_st	*st = self->st;
    Stretch_Rights res;
    res  = MMU$QueryProt(st->mmu, pdid, self);

    /* XXX not updated by SetGlobal; should do a trans */

    TRC(eprintf("Stretch$Query(%p, %p) -> va=[%p,%p) [%c%c%c%c]\n", 
		self, pdid, st->a, st->a+st->s, 
		SET_IN(res, Stretch_Right_Meta)    ? 'M' : '-',
		SET_IN(res, Stretch_Right_Execute) ? 'E' : '-',
		SET_IN(res, Stretch_Right_Write)   ? 'W' : '-',
		SET_IN(res, Stretch_Right_Read)    ? 'R' : '-'));
    
    return res;
}




static	StretchAllocator_New_fn 		NewStr_m, NewNailedStr_m;
static	StretchAllocator_NewList_fn 		NewList_m, NewNailedList_m;
static	StretchAllocator_NewAt_fn 	        NewAt_m, NewNailedAt_m;
static  StretchAllocator_Clone_fn               Clone_m, NailedClone_m;
static	StretchAllocator_DestroyStretch_fn 	DestroyStretch_m; 
static	StretchAllocator_Destroy_fn 		DestroyAllocator_m;

static	StretchAllocator_op sa_op = {
    NewStr_m,
    NewList_m,
    NewAt_m,
    Clone_m,
    DestroyStretch_m,
    DestroyAllocator_m
};

static	StretchAllocator_op nld_sa_op = {
    NewNailedStr_m,
    NewNailedList_m,
    NewNailedAt_m,
    NailedClone_m,
    DestroyStretch_m,
    DestroyAllocator_m
};


/*
 * StretchAllocatorF Method Suite
 */
static  StretchAllocatorF_NewClient_fn 	NewClient_m;
static	StretchAllocatorF_NewNailed_fn  NewNailed_m;
static	StretchAllocatorF_NewOver_fn 	NewOver_m;

static StretchAllocatorF_op saf_op = { 
    NewStr_m, 
    NewList_m,
    NewAt_m,
    Clone_m,
    DestroyStretch_m,
    DestroyAllocator_m,
    NewClient_m,
    NewNailed_m,
    NewOver_m
};



/* 
** Misc utilitiy functions 
*/
sid_t AllocSID(SA_st *st)
{
    int i, j, k;
    uint32_t sids;
    sid_t    sid;

    for (i = 0; i < SID_ARRAY_SZ; i++) {
	sids = st->sids[i];
	if (sids != (uint32_t)-1) {
	    for (j = 0, k = 1; (j < 32) && (sids & k); j++)
		k<<=1;
	    st->sids[i] = sids | k;
	    sid = (i<<5) + j;
	    TRC(eprintf("AllocSID(%p) -> %04x\n", st, sid));
	    return sid;
	}
    }
    TRC(eprintf("AllocSID(%p) FAILED\n", st));
    return SID_NULL;
}

void RegisterSID(SA_st *st, sid_t sid, Stretch_cl *s)
{
    st->ss_tab[sid]= s;
    return;
}


void FreeSID(SA_st *st, sid_t sid)
{
    TRC(eprintf("FreeSID(%p, %04x)\n", st, sid));
    st->sids[sid/32] &= ~(1<<(sid % 32));
}



/*
** VM_Alloc() allocates regions of the virtual address space for 
** stretches (or for stretch allocators). 
** The input parameters are:
**    - size     : overall size required (extent in bytes)
**    - start    : requested start address, or ANY_ADDRESS if don't care
**    - OUT a    : virtual address of the allocated region
**    - OUT np   : number of pages in allocated in region
**    - OUT pw   : page width of the allocated region.
** The OUT parameters are only valid if the region was allocated 
** sucessfully, as reported by the return value. 
*/
static bool_t VM_Alloc(SA_st *st, Stretch_Size size, addr_t start, 
		       addr_t *a, word_t *np, uint32_t *pw)
{
    VASRegion *region, *new_region;
    word_t npages, spage, rspage, rlpage, rpoff; 

    /* XXX SMH: although we have fields for 'page width' within regions, 
       for now we *only* deal with the case where page width == PAGE_WIDTH */
    npages = (size + PAGE_SIZE - 1) >> PAGE_WIDTH; 

    if((word_t)start&1) {   /* no specified address => get first fit */
	
	for(region= st->regions->next; region != (st->regions); 
	    region= region->next) {
	    
	    if(npages <= region->desc.npages)
		break;
	}
	
	if(region == st->regions) {
	    DB(eprintf("VM_Alloc: cannot alloc %lx bytes.\n", size));
	    return False;
	} 
	
	
	*a  = region->desc.start_addr;
	*np = npages;
	*pw = region->desc.page_width;
	
	if(region->desc.npages > npages) {
	    region->desc.start_addr += ROUNDUP(size, region->desc.page_width);
	    region->desc.npages     -= npages; 
	} else {
	    LINK_REMOVE(region);
	    Heap$Free(st->h, region);
	}

    } else { 

	/* have a requested start address; compute start page */
	spage = ((word_t)start + PAGE_SIZE-1) >> PAGE_WIDTH; 
	rspage = rlpage = rpoff = 0;

	for(region= st->regions->next; region != (st->regions); 
	    region= region->next) {

	    /* get the region start and end pages */
	    rspage = ((word_t)region->desc.start_addr+PAGE_SIZE-1)>>PAGE_WIDTH;
	    rlpage = rspage + region->desc.npages;

	    /* check if we're within the region */
	    if(spage >= rspage && (spage+npages) <= rlpage)
		break;
	}
	
	if(region == st->regions)
	    return False;
	
	if(((word_t)start & ((1UL << region->desc.page_width)-1) ) != 0) {
	    /* not aligned to a page boundary in this region */
	    eprintf("Requested address %p not aligned to page width %lx\n", 
		    start, region->desc.page_width);
	    ntsc_halt();
	}

	rpoff = (spage - rspage);

	*a  = region->desc.start_addr + (rpoff << PAGE_WIDTH);
	*np = npages;
	*pw = region->desc.page_width;

	/* 
	** Okay; three possibilties; 
	** 1) rpoff == 0; => allocating from start of region, works as above
	** 2) rpoff>0 && (npages + rpoff) == region->desc.npages => KILL top
	** 3) else; resize to get low half; create new top half;
	*/
	if(rpoff) {
	    if((rpoff + npages) == region->desc.npages) {
		/* new allocation takes us up to the end of the region */
		region->desc.npages -= npages;
	    } else {
		/* rpoff + npages  < region.npages; so have 2 bits */
		new_region = Heap$Malloc(st->h, sizeof(VASRegion));
		new_region->desc.start_addr = (addr_t)
		    (word_t)*a + ROUNDUP(size, region->desc.page_width);
		new_region->desc.npages = 
		    region->desc.npages - (rpoff + npages);
		new_region->desc.page_width = region->desc.page_width;
		region->desc.npages = rpoff;
		LINK_INSERT_AFTER(region, new_region);
	    }
	} else {
	    /* rpoff == 0 */
	    if(region->desc.npages > npages) {
		region->desc.start_addr += 
		    ROUNDUP(size, region->desc.page_width);
		region->desc.npages     -= npages;
	    } else {
		LINK_REMOVE(region);
		Heap$Free(st->h, region);
	    }
	}
    }

    ETRC(eprintf("VM_Alloc (st=%p): allocated [%p, %p]\n", st, *a, 
		 (*a + ROUNDUP(size, region->desc.page_width))));
    return True;
}

/*
** VM_Free() releases regions of the virtual address space.
** The input parameters are:
**    - base     : the start virtual address of the region to be freed. 
**    - len      : the extent of the region (in bytes)
**    - pw       : the page width of the region.
*/
static bool_t VM_Free(SA_st *st, addr_t base, Stretch_Size len, uint32_t pw)
{
    VASRegion *region, *next, *new;
    addr_t     end, cbase, cend, nbase, nend; 
    uint32_t   npages; 
    bool_t     mergelhs, mergerhs; 


    end    = base + len;
    npages = NPAGES(len, PAGE_WIDTH);
    ETRC(eprintf("VM_Free: [%p,%p) (len is %x)\n", base, end, len));

    for(region= st->regions->next; region != (st->regions); 
	region= region->next) {
	
	cbase = region->desc.start_addr; 
	cend  = cbase + (region->desc.npages << PAGE_WIDTH); 
	nbase = region->next == st->regions ? (addr_t)-1 : 
	    region->next->desc.start_addr; 
	if((base >= cend) && (end <= nbase))
	    break; 
    }

    if(region == st->regions) {

	/* We wrapped => no elem prior to us. Check if we can merge on rhs */
	next   = st->regions->next;
	cbase  = next->desc.start_addr; 

	if(end == cbase) {

	    next->desc.npages    += npages;
	    next->desc.start_addr = base;
	    cend                  = cbase + (next->desc.npages << PAGE_WIDTH);
	    return True;

	} else {

	    /* Otherwise, need a new region at head */
	    if((new = Heap$Malloc(st->h, sizeof(*new))) == NULL)
		return False;
	
	    new->desc.start_addr = base; 
	    new->desc.npages     = npages; 
	    new->desc.page_width = pw;
	    
	    LINK_ADD_TO_HEAD(st->regions, new);
	    return True;
	}
    }

    /* 
    ** Ok: we're in the right place: base is >= cend. 
    ** We want to merge regions if either: 
    **   (a) base == cend, or
    **   (b) end  == base of following region [if there is one]
    ** It is of course possible that both will be the case, i.e. 
    ** that we are completely filling a gap.
    */

    /* Check for merge on the left hand side */
    mergelhs = ((region->desc.page_width == pw) && (base == cend));
    
    /* Now check if we could merge on the rhs too */
    if(region->next != st->regions) {
	next      = region->next; 
	nbase     = next->desc.start_addr; 
	mergerhs  = ((next->desc.page_width == pw) && (nbase == end));
    } else {
	/* would wrap, so cannot merge on rhs*/
	next     = NULL;             /* stop GDB complaining at us */
	nbase    = (addr_t)0xf00d;   /*  "" */
	mergerhs = False;
    }

    if(!mergelhs && !mergerhs) {

	/* Cannot merge => need a new list element (after 'link'=='cur') */
	if((new = Heap$Malloc(st->h, sizeof(*new))) == NULL)
	    return False;
	
	new->desc.start_addr = base; 
	new->desc.npages     = npages; 
	new->desc.page_width = pw;
	
	LINK_INSERT_AFTER(region, new);
	return True;
    }
    
    
    if(mergelhs) {
	ETRC(eprintf("VM_Free: coalesce current [%p-%p) & new [%p-%p)\n", 
		     cbase, cend, base, end));
	region->desc.npages += npages; 
	cend                 = cbase + (region->desc.npages << PAGE_WIDTH);
	ETRC(eprintf("        i.e region is now at [%p-%p)\n", 
		     cbase, cend));
	
	
	if(mergerhs) {
	    ETRC(eprintf("VM_Free: plus can merge on rhs..\n"));
	    region->desc.npages += next->desc.npages;
	    cend                 = cbase + (region->desc.npages << PAGE_WIDTH);
	    ETRC(eprintf("        i.e region is now at [%p-%p)\n", 
			 cbase, cend));
	    
	    /* Now free 'next' */
	    LINK_REMOVE(next);
	    Heap$Free(st->h, next);
	} 
	return True;
    } 
    
    /* If get here, we can merge on rhs, but *not* of lhs */
    nend = nbase + (next->desc.npages << PAGE_WIDTH);
    ETRC(eprintf("VM_Free: coalesce new [%p-%p) and next [%p-%p)\n", 
		 base, end, nbase, nend));
    next->desc.npages    += npages;
    next->desc.start_addr = base; 
    nbase      = next->desc.start_addr;
    nend       = nbase + (next->desc.npages << PAGE_WIDTH);
    ETRC(eprintf("        i.e region is now at [%p-%p)\n", 
		 nbase, nend));
    return True;
}


/*
** Util: generic stretch creation.
*/
static Stretch_st *CreateStretch(SA_st *st, addr_t base, word_t npages)
{
    Stretch_st *res;

    res = Heap$Malloc(st->h, sizeof(*res) );
    
    if (!res ) {
      DB(eprintf("CreateStretch: Malloc failed.\n"));
      return (Stretch_st *)NULL;
    }

    /* Initalise the closure */
    CL_INIT(res->cl, &s_op, res);

    /* Fill in the the fields */
    res->a       = base; 
    res->s       = npages << PAGE_WIDTH;
    res->sid     = AllocSID(st);
    res->mmu     = st->mmu;

    RegisterSID(st, res->sid, &res->cl);
    return res;
}

/*--StretchAllocator methods-----------------------------------------------*/


/*
 * Create a single stretch
 */
static Stretch_clp NewStr_m(StretchAllocator_cl *self, Stretch_Size size, 
			    Stretch_Rights global)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    uint32_t mode = 0;
    Stretch_st *s;
    Mem_VMemDesc desc;

    ETRC(eprintf("StretchAllocator$New: VP=%p called with size %x\n",
		 cst->vp, size));

    /* Try to allocated some virtual address space */
    if(!VM_Alloc(st, size, ANY_ADDRESS, &desc.start_addr, 
		 (word_t *)&desc.npages, &desc.page_width)) {
	eprintf("StretchAllocator$New: VM_Alloc 0x%x bytes failed.\n", size);
	ntsc_halt();
    }

    if(!(s = CreateStretch(st, desc.start_addr, desc.npages))) {
	eprintf("StretchAllocator$New: CreateStretch failed.\n");
	ntsc_halt();
	/* XXX VM_Free() */
    }

    s->sa = self;

    ETRC(eprintf("StretchAllocator$New: allocated [%lx-%lx], SID=%x\n", 
		 s->a, s->a + s->s, s->sid));

    MMU$AddRange(st->mmu, &s->cl, &desc, global);

    /* Default access rights are user r/w/m, parent m */

    if(cst->pdid != NULL_PDID) {
	/* pdid will be NULL during some parts of the startup phase */
	MMU$SetProt(st->mmu, cst->pdid, &s->cl,
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) | 
		    SET_ELEM(Stretch_Right_Meta));
	if(cst->parent != NULL_PDID) 
	    MMU$SetProt(st->mmu, cst->parent, &s->cl,
			SET_ELEM(Stretch_Right_Meta));
    }

    ENTER(Pvs(vp), mode);
    LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
    LEAVE(Pvs(vp), mode);

    /* Done */
    ETRC(eprintf("StetchAllocator$New: returning stretch at %p\n", &s->cl));
    return &(s->cl);
}


/*
 * Create a sequence of stretches. The sequence is allocated using the
 * pervasive heap, the stretches themselves on the internal heap. 
 */
static StretchAllocator_StretchSeq 
*NewList_m (StretchAllocator_cl *self,
	   const StretchAllocator_SizeSeq *sizes, 
	    Stretch_Rights global)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    StretchAllocator_StretchSeq *shead;
    Stretch_st *s, **sbase = NULL;
    Stretch_Size tot_mem, *szp;
    uint32_t i, num, mode=0;
    Mem_VMemDesc desc;

    /* First work out how many stretches we need. */
    num = SEQ_LEN (sizes);
    ETRC(eprintf("StretchAllocator$NewList: VP=%p called with %x sizes\n", 
		 cst->vp, num));


    /* Now determine the _total_ amount of VM we require */
    for(tot_mem= i =0; i < num; i++)
	tot_mem += ROUNDUP((SEQ_ELEM(sizes, i)), FRAME_WIDTH);
    
    ETRC(eprintf("StretchAllocator$NewList: total of 0x%x bytes "
		 "VM required.\n", tot_mem));

    /* Go get the vm, if possible */
    if(!VM_Alloc(st, tot_mem, ANY_ADDRESS, &desc.start_addr, &desc.npages, 
		 &desc.page_width)) {
	eprintf("StretchAllocator$NewList: VM_Alloc() failed!\n");
	RAISE_Mem$Failure();
    }

    /* Create the sequence. */
    ETRC(eprintf("StretchAllocator$NewList: creating sequence.\n"));

    /* This code is called in the NTSC when creating the first domain.
       setjmp is a no-no in that environment, so we can't do a TRY here. */
    
#undef  SEQ_FAIL
#define SEQ_FAIL() goto malloc_loses
    
    shead = SEQ_NEW (StretchAllocator_StretchSeq, num, Pvs(heap));
    ETRC(eprintf("StretchAllocator$NewList: got sequence at %p\n", shead));
    
    /* Alloc some space to store the st pointers (needed if lose) */
    sbase= Heap$Malloc(st->h, num * sizeof(Stretch_st *));
    if(!sbase) {
	DB(eprintf(
	    "StretchAllocator$NewList: Malloc failed [num=%d,sz=%x]\n", 
	    num, num * sizeof(Stretch_st *)));
	RAISE_Heap$NoMemory();
    }
    
    /* Note: If we calloc all the states together, we can't destroy
       the stretches individually (since any stretch after the
       first will have a bogus (from heap point of view) state
       pointer. This is annoying, but either we alloc them idivid-
       ually, or we make a "DestroyList" method which is worse. */
    

    for(i=0, szp= SEQ_START(sizes); (i<num) && (szp < SEQ_END(sizes));
	szp++, i++) {
	

#define BYTES_TO_PAGES(_b) (((_b)+PAGE_SIZE-1) >> PAGE_WIDTH)
	desc.npages = BYTES_TO_PAGES(*szp);

	s = sbase[i] = CreateStretch(st, desc.start_addr, desc.npages); 
	if(!s) {
	    eprintf("StretchAllocator$NewList: CreateStretch failed.\n");
	    ntsc_halt();
	    /* XXX VM_Free() */
	}
	s->sa = self;
	
	
	MMU$AddRange(st->mmu, &s->cl, &desc, global);

	ETRC(eprintf("StretchAllocator$NewList: allocd [%lx-%lx], SID=%x\n", 
		     s->a, s->a + s->s, s->sid));
	
	/* Update the desc addr for the next time round */
	desc.start_addr += ROUNDUP(*szp, PAGE_WIDTH);

	/* Default access rights are user r/w/m, world none */
	if(cst->pdid != NULL_PDID) {
	    /* pdid will be NULL during some parts of the startup phase */
	    ETRC(eprintf("StretchAllocator$NewList: setting default prot.\n"));
	    MMU$SetProt(st->mmu, cst->pdid, &s->cl,
			SET_ELEM(Stretch_Right_Read) | 
			SET_ELEM(Stretch_Right_Write) | 
			SET_ELEM(Stretch_Right_Meta));
	    if(cst->parent != NULL_PDID) 
		MMU$SetProt(st->mmu, cst->parent, &s->cl,
			    SET_ELEM(Stretch_Right_Meta));
	}

	/* Add to client's list */
	ENTER(Pvs(vp), mode);
	LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
	LEAVE(Pvs(vp), mode);

	SEQ_ELEM(shead, i) = &(s->cl);
    }

    /* Done */
    ETRC(eprintf("StretchAllocator$NewList: done.\n"));
    return shead;
  
  malloc_loses:
  
    eprintf("StretchAllocator$NewList: freeing stretches.\n");
  
    if(sbase) Heap$Free(st->h, sbase);
  
    RAISE_StretchAllocator$Failure();
    return NULL;
#undef  SEQ_FAIL
#define SEQ_FAIL() { eprintf("SEQ_FAIL!\n"); RAISE_Heap$NoMemory() }
}


/*
** NewAt() allows for the creation of a stretch of a particular size 
** starting at a particular starting address. A physical memory
** descriptor may also be provided in which case the virtual region
** is mapped onto the given physical frames. 
** If a request start address of ANY_ADDRESS is given, then an 
** arbitrary virtual base address may be chosen, subject to the 
** restrictions contained in "attr". 
*/
static Stretch_clp NewAt_m(StretchAllocator_cl *self, word_t size, 
			   Stretch_Rights global, addr_t start, 
			   Mem_Attrs attr, Mem_PMem pmem)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    uint32_t mode = 0;
    Stretch_st *s;
    Mem_VMemDesc desc;

    TRC(eprintf("StretchAllocator$NewAt called: VP=%p, start=%p, size=%lx\n", 
		cst->vp,  start, size));

    /* If we have some pmem of non-standard frame size, need to roundup size */
    /* XXX SMH: should also check "start" is correctly aligned in VAS */
    if(pmem != NULL && pmem->frame_width != FRAME_WIDTH)
	size = ROUNDUP(size, pmem->frame_width);


    /* Try to allocate some virtual address space */
    if(!VM_Alloc(st, size, start,  &desc.start_addr, 
		 (word_t *)&desc.npages, &desc.page_width)) {

	eprintf("StretchAllocator$NewAt: VM_Alloc() of "
		"0x%lx bytes at %p failed!\n", size, start);
	RAISE_Mem$Failure();
    }

    if(!(s = CreateStretch(st, desc.start_addr, desc.npages))) {
	eprintf("StretchAllocator$NewAt: CreateStretch failed.\n");
	ntsc_halt();
	/* XXX VM_Free() */
    }

    s->sa = self;

    TRC(eprintf("StretchAllocator$NewAt: allocated [%lx-%lx], SID=%x\n", 
		 s->a, s->a + s->s, s->sid));

    desc.attr = 0;   /* we ignore these for now */

    if(pmem != NULL) {
	TRC(eprintf("About to add mapped range now [1st pmemdesc start=%p]\n", 
		    pmem[0].start_addr));
	MMU$AddMappedRange(st->mmu, &s->cl, &desc, pmem, global); 
    } else MMU$AddRange(st->mmu, &s->cl, &desc, global);
    /* Default access rights are user r/w/m, parent m */
    
    if(cst->pdid != NULL_PDID) {
	/* pdid will be NULL during some parts of the startup phase */
	MMU$SetProt(st->mmu, cst->pdid, &s->cl,
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) | 
		    SET_ELEM(Stretch_Right_Meta));
	if(cst->parent != NULL_PDID) 
	    MMU$SetProt(st->mmu, cst->parent, &s->cl,
			SET_ELEM(Stretch_Right_Meta));
    }

    ENTER(Pvs(vp), mode);
    LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
    LEAVE(Pvs(vp), mode);

    /* Done */
    TRC(eprintf("StetchAllocator$NewAt: returning stretch at %p\n", &s->cl));
    return &(s->cl);
}


/*
 * Create a stretch with the same permissions (and perhaps even size) 
 * as the given "template" stretch. 
 * XXX SMH: should sanity check the template stretch is valid.
 */
static Stretch_clp Clone_m(StretchAllocator_cl *self, 
			   Stretch_clp template, Stretch_Size size)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    uint32_t mode = 0;
    Stretch_Rights gaxs; 
    Stretch_st *tmpl_st, *s;
    Mem_VMemDesc desc;
    
    tmpl_st = (Stretch_st *)template->st;
    if(size == 0)  size = tmpl_st->s; 

    ETRC(eprintf("StretchAllocator$Clone: VP=%p called with size %x\n",
		 cst->vp, size));
    
    /* Try to allocate some virtual address space */
    if(!VM_Alloc(st, size, ANY_ADDRESS, &desc.start_addr, 
		 (word_t *)&desc.npages, &desc.page_width)) {
	eprintf("StretchAllocator$Clone: VM_Alloc 0x%x bytes failed.\n", size);
	ntsc_halt();
    }

    if(!(s = CreateStretch(st, desc.start_addr, desc.npages))) {
	eprintf("StretchAllocator$Clone: CreateStretch failed.\n");
	ntsc_halt();
	/* XXX VM_Free() */
    }

    s->sa = self;

    ETRC(eprintf("StretchAllocator$Clone: allocated [%lx-%lx], SID=%x\n", 
		 s->a, s->a + s->s, s->sid));

    /* Get the global rights from the template & set 'em in our new stretch */
    gaxs = MMU$QueryGlobal(st->mmu, template); 
    MMU$AddRange(st->mmu, &s->cl, &desc, gaxs);

    /* Default access rights are copied from the template stretch */
    if(cst->pdid != NULL_PDID) 
	MMU$CloneProt(st->mmu, &s->cl, template);

    ENTER(Pvs(vp), mode);
    LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
    LEAVE(Pvs(vp), mode);

    /* Done */
    ETRC(eprintf("StetchAllocator$Clone: returning stretch at %p\n", &s->cl));
    return &(s->cl);
}


#ifdef __ALPHA__
#define SYSALLOC_VA_BASE ((addr_t)0x12300000000UL)  /* arb. high bit pattern */
#define SYSALLOC_VA_SIZE ((word_t)(0x1UL << 32))    /* 4 gig  */
#else
#define SYSALLOC_VA_BASE ANY_ADDRESS
#define SYSALLOC_VA_SIZE ((word_t)(0x1UL << 28))    /* 256 Mb */
#endif


static void DestroyStretch_m(StretchAllocator_cl *self, Stretch_cl *str)
{
    Stretch_st *st = (Stretch_st *)(str->st);
    Client_st *cst = (Client_st *)(st->sa->st);
    SA_st   *sa_st = cst->sa_st;
    Mem_VMemDesc vmem;
    uint32_t NOCLOBBER mode = 0;

    TRC(eprintf("StretchAllocator$DestroyStretch (str=%p)\n", str));

    /* SMH: lightweight synch.- requires no IDC between ENTER/LEAVE */
    ENTER(Pvs(vp), mode);

    LINK_REMOVE(&st->link);

    if(!VM_Free(sa_st, st->a, st->s, PAGE_WIDTH)) {
	eprintf("StretchAllocator$DestroyStretch: VM_Free failed.\n");
	ntsc_dbgstop();
    }

    /* Invalidate the vm range */
    vmem.start_addr = st->a; 
    vmem.npages     = BYTES_TO_PAGES(st->s);
    vmem.page_width = PAGE_WIDTH; 
    vmem.attr       = 0;
    MMU$FreeRange(st->mmu, &vmem);

    if(sa_st->f) {
	/* Free the frames underneath */
#ifdef __ALPHA__ 
	Frames$Free(sa_st->f, st->a - SYSALLOC_VA_BASE, st->s); 
#else
	/* XXX Impl me for other archs; need to lookup to get pbase */
#endif
    }

    FreeSID(sa_st, st->sid);

    /* XXX SMH: if this salloc has any children, need to remove its 
       protection domain from their cst's. */

    /* Finally, free the state */
    Heap$Free(sa_st->h, st);

    LEAVE(Pvs(vp), mode);
    return;
}


static void DestroyAllocator_m(StretchAllocator_cl *self )
{
    Client_st *st = (Client_st *)(self->st);
    SA_st *sa_st  = st->sa_st;
    uint32_t mode = 0;

    TRC(eprintf("StretchAllocator$DestroyAllocator\n"));
    ENTER(Pvs(vp), mode);

    /* Destroy all stretches */
    while (!LINK_EMPTY(&st->stretches)) {
      Stretch_st *str = (Stretch_st *)(st->stretches.next);
      StretchAllocator$DestroyStretch(self, &str->cl);
    }

    LEAVE(Pvs(vp), mode);

    /* Free state */
    Heap$Free(sa_st->h, st );
}




/*--StretchAllocatorF methods ----------------------------------------------*/


static StretchAllocator_clp 
NewClient_m (StretchAllocatorF_clp self,  /* IN */
	     ProtectionDomain_ID  pdid,
	     VP_clp                vp,
	     ProtectionDomain_ID  parent_pdid)
{
    Client_st *cst = (Client_st *)(self->st);
    SA_st *st = cst->sa_st;
    
    Client_st *new;
    StretchAllocator_clp sa;

    ETRC(eprintf("StretchAllocator$NewClient: called by VP=%p newVP=%p\n",
		 cst->vp, vp));
    ETRC(eprintf("                      parent pdid =%x client pdid=%x\n", 
		 parent_pdid, pdid));
    new = Heap$Malloc(st->h, sizeof(Client_st) );
  
    if (!new) {
	DB(eprintf("StretchAllocator$NewClient: Malloc failed.\n"));
	RAISE_Heap$NoMemory();
	return (StretchAllocator_clp)0;
    }

    new->sa_st  = st;
    new->vp     = vp;
    new->pdid   = pdid;
    if(parent_pdid == NULL_PDID) {
	/* special case: we've been called within the Nemesis domain 
	   and so we set the parent to be the sallocF's pdid. */
	if(pdid == cst->pdid) {
	    eprintf("Warning: domain %qx is sharing Nemesis pdid!\n", 
		    VP$DomainID(vp));
	    new->parent = NULL_PDID;
	} else new->parent = cst->pdid; 
    }
    else {
	/* Parent not Nemesis => only record parent if it is different */
	new->parent = (parent_pdid == pdid) ? NULL_PDID : parent_pdid;
    }

    LINK_INITIALISE (&new->stretches);

    CL_INIT( new->cl, &sa_op, new );

    sa = &new->cl;
    ETRC(eprintf("StretchAllocator$NewClient: returning %p\n", sa));
    return sa;
}

/*
** NewOver() is a special stretch creation method which currently is
** restricted to the StretchAllocatorF. It provides the same functions
** as NewAt() with the additional ability to deal with virtual 
** memory regions which have already be allocated: in this case a 
** stretch is created over the existing region and the mappings updated
** only with the new stretch ID / rights. 
** XXX SMH: This latter case could cause the breaking of the stretch
** model if called unscrupulously. This is the main reason for
** the restriction of this method at the current time. 
*/
static Stretch_clp NewOver_m(StretchAllocatorF_cl *self, word_t size, 
			     Stretch_Rights global, addr_t start, 
			     Mem_Attrs attr, uint32_t pwidth, 
			     Mem_PMem pmem)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    uint32_t mode = 0;
    Stretch_st *s;
    Mem_VMemDesc desc;
    bool_t update = False;

    TRC(eprintf("StretchAllocatorF$NewOver called: VP=%p start=%p size=%lx\n", 
	    cst->vp, start, size));

    /* Try to allocate some virtual address space */
    if(!VM_Alloc(st, size, start,  &desc.start_addr, 
		 (word_t *)&desc.npages, &desc.page_width)) {
	/* 
	** If we fail, we assume [XXX] that the entire region is already
	** allocated and that we are performing a "map stretch over" 
	** type function. 
	*/
	desc.start_addr = (addr_t)((word_t)start & ~(PAGE_SIZE - 1)); 
	desc.page_width = pwidth; 
	desc.npages     = (size + PAGE_SIZE - 1) >> PAGE_WIDTH; 
	update = True; 
    }

    if(!(s = CreateStretch(st, desc.start_addr, desc.npages))) {
	eprintf("StretchAllocator$NewAt: CreateStretch failed.\n");
	ntsc_halt();
	/* XXX VM_Free() */
    }

    s->sa = (StretchAllocator_cl *)self;

    TRC(eprintf("StretchAllocatorF$NewOver: allocated [%lx-%lx], SID=%x\n", 
		 s->a, s->a + s->s, s->sid));

    if(update) 
	MMU$UpdateRange(st->mmu, &s->cl, &desc, global); 
    else MMU$AddRange(st->mmu, &s->cl, &desc, global);

    /* Default access rights are user r/w/m, parent m */

    if(cst->pdid != NULL_PDID) {
	/* pdid will be NULL during some parts of the startup phase */
	MMU$SetProt(st->mmu, cst->pdid, &s->cl,
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) | 
		    SET_ELEM(Stretch_Right_Meta));
	if(cst->parent != NULL_PDID) 
	    MMU$SetProt(st->mmu, cst->parent, &s->cl,
			SET_ELEM(Stretch_Right_Meta));
    }

    ENTER(Pvs(vp), mode);
    LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
    LEAVE(Pvs(vp), mode);

    /* Done */
    TRC(eprintf("StetchAllocatorF$NewOver: returning stretch at %p\n", 
		&s->cl));
    return &(s->cl);
}


/*
** NewNailed() is used to create a 'special' stretch allocator for 
** use by the system code for page tables, protection domains, 
** dcbs, etc. 
** On alphas, 'special' means that the stretches are all allocated
** from a 4GB region of memory; this memory is backed by physical memory
** in a simple manner - the high bits are zapped to give the physical 
** address. The fact of the magic prefix also simplifies the task
** of sanity checking arguments for the palcode. 
** On intel machines, all that 'special' means is that the 
** stretches are backed by physical memory on creation. 
*/

static StretchAllocator_clp NewNailed_m(StretchAllocatorF_clp self, 
					Frames_clp frames, Heap_clp heap)
{
    Client_st *new_cst, *cst = (Client_st *)(self->st);
    SA_st     *new_st, *st = cst->sa_st;
    StretchAllocator_cl *res;
    VASRegion *first;
    addr_t vaddr;
    word_t npages;
    uint32_t pwidth;

    ETRC(eprintf("StretchAllocatorF$NewNailed: called, size %lx\n", 
		 SYSALLOC_VA_SIZE));

    /* do a VM_Alloc to grab the mem. And then... */
    if(!VM_Alloc(st, SYSALLOC_VA_SIZE, SYSALLOC_VA_BASE, 
		 &vaddr, &npages, &pwidth)) {
	eprintf("StretchAlllocatorF$NewNailed: VM_Alloc() failed.\n");
	RAISE_Mem$Failure();
    }

    ETRC(eprintf("StretchAllocatorF$NewNailed: allocated (VIRT) [%lx-%lx]\n", 
		 vaddr, vaddr + (npages << pwidth)));

    /* Get State */
    new_cst = (Client_st *)Heap$Malloc(heap, sizeof(Client_st)+sizeof(SA_st));
    if (!new_cst) {
	DB(eprintf("StretchAllocator$NewNailed: Malloc failed.\n"));
	RAISE_Heap$NoMemory();
	return (StretchAllocator_cl *)NULL;
    }


    /* Setup shared state: we have a frames and heap from the params */
    new_st         = (SA_st *)(new_cst + 1);
    new_st->f      = frames;
    new_st->h      = heap;

    /* We use our own [unshared] region management structures. */
    new_st->regions = Heap$Malloc(heap, sizeof(VASRegion));
    LINK_INITIALISE(new_st->regions);

    /* And we have our own client list (should be just us!) */
    LINK_INITIALISE(&new_st->clients);

    /* Copy the rest from our creator */
    new_st->mmu    = st->mmu;    
    new_st->sids   = st->sids;
    new_st->ss_tab = st->ss_tab; 

    first = Heap$Malloc(heap, sizeof(VASRegion));
    first->desc.start_addr = vaddr;
    first->desc.npages     = npages;
    first->desc.page_width = pwidth;
    first->desc.attr       = 0;               /* XXX should be special attr? */
    LINK_ADD_TO_HEAD(new_st->regions, first);

    /* Client state */
    CL_INIT(new_cst->cl, &nld_sa_op, new_cst);
    new_cst->sa_st  = new_st;
    new_cst->vp     = NULL;
    new_cst->pdid   = NULL_PDID;
    new_cst->parent = NULL_PDID;

    LINK_INITIALISE (&new_cst->stretches);

    /* Keep a record of this 'client' */
    LINK_ADD_TO_HEAD(&new_st->clients, &new_cst->link);


    /* Done */
    res = &new_cst->cl;

    ETRC(eprintf("StretchAllocatorF$NewNailed: returning res at %p\n", res));
    return res;
}




/*-- Nailed versions of the New, NewList & NewAt methods --------------------*/
static Stretch_clp NewNailedStr_m(StretchAllocator_cl *self, 
				  Stretch_Size size, 
				  Stretch_Rights global)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    uint32_t mode = 0;
    addr_t start; 
    Stretch_st *s;
    Mem_VMemDesc vmdesc;
    Mem_PMemDesc pmdesc;

    ETRC(eprintf("StretchAllocator$New[Nailed]: VP=%p called with size %x\n",
		 cst->vp, size));

    /* Allocate some pmem (only allocates frames of standard size) */
    if((pmdesc.start_addr = Frames$Alloc(st->f, size, 
					 FRAME_WIDTH)) == NO_ADDRESS) {
	eprintf("StretchAllocator$New[Nailed]: failed to get pmem.\n");
	RAISE_Mem$Failure();
    }
	
    ETRC(eprintf("+++ got %x bytes physical at %p\n", 
		 size, pmdesc.start_addr));
    pmdesc.frame_width = FRAME_WIDTH;
    pmdesc.nframes     = (size + FRAME_SIZE -1) >> FRAME_WIDTH;

#ifdef __ALPHA__
    /* Allocate the relevant virtual address space */
    start = (addr_t)((word_t)pmdesc.start_addr + (word_t)SYSALLOC_VA_BASE);
#else
    /* Try to allocate any  virtual address space */
    start = ANY_ADDRESS;
#endif
    if(!VM_Alloc(st, size, start, &vmdesc.start_addr, 
		 (word_t *)&vmdesc.npages, &vmdesc.page_width)) {
	eprintf("StretchAllocator$New[Nailed]: VM_Alloc 0x%x bytes failed.\n", 
		size);
	Frames$Free(st->f, pmdesc.start_addr, size);
	RAISE_Mem$Failure();
    }
    ETRC(eprintf("+++ alloc'd %x bytes at %p\n", size, vmdesc.start_addr));
    if(!(s = CreateStretch(st, vmdesc.start_addr, vmdesc.npages))) {
	eprintf("StretchAllocator$New[Nailed]: CreateStretch failed.\n");
	Frames$Free(st->f, pmdesc.start_addr, size);
	/* XXX VM_Free() */
	RAISE_Mem$Failure();
    }

    s->sa = (StretchAllocator_cl *)self;
    MMU$AddMappedRange(st->mmu, &s->cl, &vmdesc, &pmdesc, global);

    /* Default access rights are user r/w/m, world none */
    if(cst->pdid != NULL_PDID) {
	/* pdid will be NULL during some parts of the startup phase */
	MMU$SetProt(st->mmu, cst->pdid, &s->cl,
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) | 
		    SET_ELEM(Stretch_Right_Meta));
	if(cst->parent != NULL_PDID) 
	    MMU$SetProt(st->mmu, cst->parent, &s->cl,
			SET_ELEM(Stretch_Right_Meta));
    } 

    ENTER(Pvs(vp), mode);
    LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
    LEAVE(Pvs(vp), mode);

    /* Done */
    TRC(eprintf("StetchAllocator$New[Nailed]: returning stretch at %p\n", 
		&s->cl));
    return &(s->cl);
}

static StretchAllocator_StretchSeq *NewNailedList_m(
    StretchAllocator_cl *self, 
    const StretchAllocator_SizeSeq *sizes, 
    Stretch_Rights global)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    StretchAllocator_StretchSeq *shead;
    Stretch_st *s, **sbase = NULL;
    Stretch_Size tot_mem, sz, *szp;
    uint32_t i, num, mode=0;
    addr_t start; 
    Mem_VMemDesc vmdesc;
    Mem_PMemDesc pmdesc;

    /* First work out how many stretches we need. */
    num = SEQ_LEN (sizes);
    TRC(eprintf(
	"StretchAllocator$NewList[Nailed]: VP=%p called with %x sizes\n", 
	cst->vp, num));


    /* Now determine the _total_ amount of VM we require */
    for(tot_mem= i =0; i < num; i++)
	tot_mem += ROUNDUP((SEQ_ELEM(sizes, i)), FRAME_WIDTH);
    
    TRC(eprintf("StretchAllocator$NewList[Nailed]: total of 0x%x bytes "
		 "VM required.\n", tot_mem));

    /* 
    ** For this stretch allocator (the 'sysalloc') we require nailed
    ** memory which is (on some architectures) mapped a certain way. 
    ** It is \emph{easiest} though not best if we simply allocate
    ** a contiguous lump of physical memory and map the contiguous 
    ** list of stretches onto this. 
    */

    if((pmdesc.start_addr = Frames$Alloc(st->f, tot_mem, 
					 FRAME_WIDTH)) == NO_ADDRESS) {
	eprintf("StretchAllocator$New[Nailed]: failed to get pmem.\n");
	RAISE_Mem$Failure();
    }
	
    TRC(eprintf("+++ got %x bytes physical at %p\n", tot_mem, 
		pmdesc.start_addr));
    pmdesc.frame_width = FRAME_WIDTH;
    pmdesc.nframes     = (tot_mem + FRAME_SIZE -1) >> FRAME_WIDTH;

#ifdef __ALPHA__
    /* Allocate the relevant virtual address space */
    start = (addr_t)((word_t)pmdesc.start_addr + (word_t)SYSALLOC_VA_BASE);
#else
    /* Try to allocate any  virtual address space */
    start = ANY_ADDRESS;
#endif

    /* Go get the vm, if possible */
    if(!VM_Alloc(st, tot_mem, start, &vmdesc.start_addr, &sz, 
		 &vmdesc.page_width)) {
	eprintf("StretchAllocator$NewList[Nailed]: VM_Alloc() failed!\n");
	RAISE_Mem$Failure();
    }
    TRC(eprintf("+++ alloc'd %x bytes at %p\n", tot_mem, vmdesc.start_addr));

    /* Create the sequence. */
    TRC(eprintf("StretchAllocator$NewList[Nailed]: creating sequence.\n"));

    /* This code is called in the NTSC when creating the first domain.
       setjmp is a no-no in that environment, so we can't do a TRY here. */
    
#undef  SEQ_FAIL
#define SEQ_FAIL() goto malloc_loses
    
    shead = SEQ_NEW (StretchAllocator_StretchSeq, num, Pvs(heap));
    TRC(eprintf("StretchAllocator$NewList[Nailed]: got sequence at %p\n", 
		 shead));
    
    /* Alloc some space to store the st pointers (needed if lose) */
    sbase = Heap$Malloc(st->h, num * sizeof(Stretch_st *));
    if(!sbase) {
	DB(eprintf(
	    "StretchAllocator$NewList[Nailed]: Malloc failed- num=%d,sz=%x\n", 
	    num, num * sizeof(Stretch_st *)));
	RAISE_Heap$NoMemory();
    }

    /* Note: If we calloc all the states together, we can't destroy
       the stretches individually (since any stretch after the
       first will have a bogus (from heap point of view) state
       pointer. This is annoying, but either we alloc them idivid-
       ually, or we make a "DestroyList" method which is worse. */
    

    for(i=0, szp= SEQ_START(sizes); (i<num) && (szp < SEQ_END(sizes));
	szp++, i++) {
	
	/* ASSERT: [pm|vm]desc.start_addr both correct at this point */

#define BYTES_TO_PAGES(_b)  (((_b)+PAGE_SIZE-1) >> PAGE_WIDTH)
#define BYTES_TO_FRAMES(_b) (((_b)+PAGE_SIZE-1) >> PAGE_WIDTH)
	vmdesc.npages = BYTES_TO_PAGES(*szp);

	s = sbase[i] = CreateStretch(st, vmdesc.start_addr, vmdesc.npages); 
	if(!s) {
	    eprintf("StretchAllocator$NewList[Nailed]: "
		    "CreateStretch failed!\n");
	    /* XXX VM_Free() && all the rest */
	    ntsc_halt();
	}

	s->sa = (StretchAllocator_cl *)self;

	/* 
	** Setup the pmem_desc: start_addr is set as a precond of loop, 
	** so only need to sort out the nframes field. 
	*/
	pmdesc.nframes     = BYTES_TO_FRAMES(*szp);

	/* And go map it! */
	MMU$AddMappedRange(st->mmu, &s->cl, &vmdesc, &pmdesc, global);

	TRC(eprintf("StretchAllocator$NewList[Nailed]: allocd [%lx-%lx], "
		     "SID=%x\n", s->a, s->a + s->s, s->sid));

	/* Setup the desc start_addrs for the next time round */
	vmdesc.start_addr += ROUNDUP(*szp, PAGE_WIDTH);
	pmdesc.start_addr += ROUNDUP(*szp, FRAME_WIDTH);


	/* Default access rights are user r/w/m, world none */
	if(cst->pdid != NULL_PDID) {

	    /* pdid will be NULL during some parts of the startup phase */
	    MMU$SetProt(st->mmu, cst->pdid, &s->cl,
			SET_ELEM(Stretch_Right_Read) | 
			SET_ELEM(Stretch_Right_Write) | 
			SET_ELEM(Stretch_Right_Meta));
	    if(cst->parent != NULL_PDID) 
		MMU$SetProt(st->mmu, cst->parent, &s->cl,
			    SET_ELEM(Stretch_Right_Meta));
	} 

	/* Add to client's list */
	ENTER(Pvs(vp), mode);
	LINK_ADD_TO_TAIL(&cst->stretches, &s->link);
	LEAVE(Pvs(vp), mode);

	SEQ_ELEM(shead, i) = &(s->cl);
    }

    /* Done */
    TRC(eprintf("StretchAllocator$NewList[Nailed]: done.\n"));
    return shead;
  
  malloc_loses:
  
    eprintf("StretchAllocator$NewList[Nailed]: freeing stretches.\n");
  
    Frames$Free(st->f, pmdesc.start_addr, tot_mem);
    /* XXX VM_Free() */
    if(sbase) Heap$Free(st->h, sbase);
    RAISE_StretchAllocator$Failure();
    return NULL;
#undef  SEQ_FAIL
#define SEQ_FAIL() { eprintf("SEQ_FAIL!\n"); RAISE_Heap$NoMemory() }
}

static Stretch_clp NewNailedAt_m(StretchAllocator_cl *self, word_t size,
				 Stretch_Rights global, addr_t start, 
				 Mem_Attrs attr, Mem_PMem pmem)
{
    UNIMPLEMENTED;
    return (Stretch_cl *)NULL;
}

static Stretch_clp NailedClone_m(StretchAllocator_cl *self, 
				 Stretch_clp template, Stretch_Size size)
{
    UNIMPLEMENTED;
    return (Stretch_cl *)NULL;
}


/*--SAllocMod methods ----------------------------------------------------*/

#undef REGDUMP

static StretchAllocatorF_cl *NewF_m(SAllocMod_cl    *self,
				    Heap_clp        h,
				    MMU_clp         mmu,
				    Mem_VMem        allvm,
				    Mem_VMem        usedvm
				    /* RETURNS */ )
{
    Client_st *cst;
    SA_st *st;
    VASRegion *reg;
#ifdef REGDUMP
    VASRegion *region;
#endif
    StretchAllocatorF_clp sa;
    int i, nr, j;
    uint32_t pw;
    Stretch_Size sz; 
    addr_t va; 

    ETRC(eprintf("SAllocMod$NewF: called.\n"));

    /* Get State */
    cst = (Client_st *)Heap$Malloc(h, sizeof(Client_st) + sizeof(SA_st));
    TRC(eprintf("SAllocMod$NewF: done malloc, cst at %p\n", cst));
  
    if (!cst ) {
	DB(eprintf("SAllocMod$NewF: Malloc failed.\n"));
	RAISE_Heap$NoMemory();
	return (StretchAllocatorF_clp)0;
    }

    /* Fill in the server state */
    st      = (SA_st *)(cst + 1);
    st->h   = h;
    st->mmu = mmu;
    st->f   = NULL;   /* only nailed sallocs need frames */
    LINK_INITIALISE(&st->clients);

    st->regions = Heap$Malloc(h, sizeof(VASRegion));
    LINK_INITIALISE(st->regions);

    /* Count the number of regions we have */
    for(nr = 0 ; allvm[nr].npages; nr++) 
#ifdef REGDUMP
	eprintf(" ++ VM region %02d: start=%08x, end=%08x, np=%08x\n", 
		nr, allvm[nr].start_addr, allvm[nr].start_addr +
		(allvm[nr].npages << allvm[nr].page_width), 
		allvm[nr].npages)
#endif
	    ;
    ETRC(eprintf("Total of %d VM regions.\n", nr));

    /* Allocate enough space to hold them */
    if(!(reg = Heap$Malloc(h, nr * sizeof(VASRegion)))) {
	eprintf("SAllocMod$NewF: failed to alloc space for regions.\n");
	ntsc_halt();
    }

    for(i=0; i < nr; i++) {
	memcpy(&(reg[i].desc), &allvm[i], sizeof(Mem_VMemDesc));
	LINK_ADD_TO_TAIL(st->regions, &reg[i]);
    }


    ETRC(eprintf("SAllocMod$NewF: allocating already used regions.\n"));
    for(j = 0 ; usedvm[j].npages; j++) {
	if(!VM_Alloc(st, (usedvm[j].npages << usedvm[j].page_width), 
		     usedvm[j].start_addr, &va, &sz, &pw)) {
	    eprintf("VM_Alloc of used region failed. Death.\n");
	    ntsc_halt();
	}
    }
    
#ifdef REGDUMP
    eprintf("*** Dumping regions (st->regions==%p)\n", st->regions);
    for(region=st->regions->next; region!=(st->regions); 
	region=region->next) {
	eprintf(" ** start=%08x, end=%08x, np=%08x\n", 
		region->desc.start_addr, region->desc.start_addr + 
		REG_SIZE(region), region->desc.npages);
    }
#endif

    /* Allocate space for the sid allocation table */
    if(!(st->sids = Heap$Malloc(h, SID_ARRAY_SZ * sizeof(uint32_t)))) {
	eprintf("SAllocMod$NewF: failed to alloc space for sidtab.\n");
	ntsc_halt();
    }

    /* Intialise the sid allocation table */
    for(i=0; i<SID_ARRAY_SZ; i++) 
	st->sids[i] = 0;

    /* Allocate space for the sid->stretch mapping */
    if(!(st->ss_tab = Heap$Malloc(h, SID_MAX * sizeof(Stretch_cl *)))) {
	eprintf("SAllocMod$NewF: failed to alloc space for ss_tab.\n");
	ntsc_halt();
    }

    /* Initialise the table to NULL */
    for(i=0; i<SID_MAX; i++) 
	st->ss_tab[i] = NULL;

    /* Poke it info the INFO_PAGE */
    INFO_PAGE.ss_tab = st->ss_tab; 

    /* Client state */
    CL_INIT(cst->cl, (StretchAllocator_op *)&saf_op, cst);
    cst->sa_st  = st;
    cst->vp     = NULL;
    cst->pdid   = NULL_PDID;
    cst->parent = NULL_PDID;

    LINK_INITIALISE (&cst->stretches);

    /* Keep a record of this 'client' [hehe] */
    LINK_ADD_TO_HEAD(&st->clients, &cst->link);
    
    /* Done */
    sa = (StretchAllocatorF_cl *)&cst->cl;

    ETRC(eprintf("Returning closure at %p\n", sa));
    return sa;
}

static void Done_m(SAllocMod_cl *self, StretchAllocatorF_cl *salloc, 
		   VP_clp vp, ProtectionDomain_ID pdid)
{
    Client_st *cst = (Client_st *)(salloc->st);
  
    ETRC(eprintf("SAllocMod$Done: vp=%p, pdid=%x\n", vp, pdid));
    if(cst->vp != NULL)
	eprintf("Urk! SAllocMod$Done(): vp was non NULL (was %p)\n", cst->vp);
    
    cst->vp   = vp;
    cst->pdid = pdid;
    
    return;
}
