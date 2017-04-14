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
**      mod/nemesis/sdriver
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements both "SDriverMod", which is used to create StretchDrivers,
**      and also a number of these StretchDrivers.
**
**      Currently there are four defined StretchDrivers:
**       a) NULLDriver: this is a minimal and trivial stretch driver
**          with no resources. It can't handle faults or perform any
**          mapping. Useful only for start-of-day system code. 
**       b) NailedDriver: this maps an entire stretch on "Bind", and 
**          unmaps it again on "Remove", and hence considers all faults
**          be non-recoverable.
**       c) PhysDriver: this driver deals with 'paged' memory via the 
**          'framestack' mechanism, satisfying faults by using physical
**          frames (on demand), but not dealing with disk stuff.
**       d) PagedDriver: this driver implements a typical VM type abstracton.
**          on top of a "FileIO" for backing store. Unlike (c), it cannot
**          currently obtain any additional physical memory on demand.
**
**      Each of these can deal with multiple stretches.
** 
** ENVIRONMENT: 
**
**      User space.
**
*/

#include <nemesis.h>
#include <links.h>
#include <salloc.h>

#include <time.h>          /* for clock refesh */

#include <IOMacros.h>
#include <IDCMacros.h>
#include <VPMacros.h>

#include <FileIO.ih>
#include <Frames.ih>
#include <HeapMod.ih>
#include <SDriverMod.ih>
#include <USDCtl.ih>

#include <ntsc.h>          /* for syscall prototypes */
#include <mmu_tgt.h>
#include <frames.h>
#include <pip.h>

#ifdef  STRETCH_DRIVER_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif

#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

#define ETRC(_x) 
#define MTRC(_x)

#define CHAINING_HANDLER


/*
** Module stuff : SDriverMod.if
*/
static	SDriverMod_NewNULL_fn 	        NewNULL_m;
static	SDriverMod_NewNailed_fn 	NewNailed_m;
static	SDriverMod_NewPhysical_fn 	NewPhysical_m;
static	SDriverMod_NewPaged_fn 	        NewPaged_m;

static	SDriverMod_op	mod_ms = {
    NewNULL_m,
    NewNailed_m,
    NewPhysical_m,
    NewPaged_m,
};

static	const SDriverMod_cl	mod_cl = { &mod_ms, NULL };
CL_EXPORT (SDriverMod, SDriverMod, mod_cl);
CL_EXPORT_TO_PRIMAL(SDriverMod, SDriverModCl, mod_cl);



/*
** Module stuff : StretchDriver.if
*/
static	StretchDriver_Bind_fn 	    Bind_m, NailedBind_m; 
static	StretchDriver_Remove_fn     Remove_m, 
    NailedRemove_m, 
    PhysicalRemove_m, 
    PagedRemove_m;
static	StretchDriver_GetKind_fn    GetKind_m;
static	StretchDriver_GetTable_fn   GetTable_m;
static	StretchDriver_Map_fn 	    NULLMap_m, 
    NailedMap_m, 
    PhysicalMap_m, 
    PagedMap_m;
static  StretchDriver_Fault_fn      Fault_m;
static  StretchDriver_AddHandler_fn AddHandler_m; 
static  StretchDriver_Lock_fn       Lock_m; 
static  StretchDriver_Unlock_fn     Unlock_m; 
static  StretchDriver_Revoke_fn     Revoke_m; 

static	StretchDriver_op nullsd_ms = {
    Bind_m,
    Remove_m,
    GetKind_m,
    GetTable_m,
    NULLMap_m,
    Fault_m, 
    AddHandler_m, 
    Lock_m, 
    Unlock_m, 
    Revoke_m, 
};

static	StretchDriver_op nsd_ms = {
    NailedBind_m,
    NailedRemove_m,
    GetKind_m,
    GetTable_m,
    NailedMap_m,
    Fault_m,
    AddHandler_m, 
    Lock_m, 
    Unlock_m, 
    Revoke_m, 
};

static	StretchDriver_op pysd_ms = {
    Bind_m,
    PhysicalRemove_m,
    GetKind_m,
    GetTable_m,
    PhysicalMap_m,
    Fault_m,
    AddHandler_m, 
    Lock_m, 
    Unlock_m, 
    Revoke_m, 
};

static	StretchDriver_op pgsd_ms = {
    Bind_m,
    PagedRemove_m,
    GetKind_m,
    GetTable_m,
    PagedMap_m,
    Fault_m,
    AddHandler_m, 
    Lock_m, 
    Unlock_m, 
    Revoke_m, 
};


/* 
** XXX SMH: each kind of stretch driver has its own state, but we 
** arrange that each one subsumes the earlier. This allows us to 
** cast any 'higher' stretch driver state pointer down to a 'lower' one. 
** This is *not* mandatory for stretch drivers by any means, but just
** provides a simple way for me to deal with common functionality. 
*/


/* 
** NULL Stretch Driver state. 
** Simply contains a bunch of minimal fields. 
*/
#define MAX_HDLRS        10 

typedef struct  {
    StretchDriver_cl     cl;
    StretchDriver_Kind   kind;
    VP_clp               vpp; 
    bool_t               mode; 
    Heap_clp             heap;
    StretchTbl_clp       strtab;
    FaultHandler_clp     overrides[MAX_HDLRS];
} NULLDriver_st;


/*
** Since some parts of a stretch driver run in an activation
** handler and others may run with activations on, we need 
** to protect shared structures. We do this using 'lightweight' 
** activation mode synchronisation. 
** Notes: 
**  a) in general one should use the Threads${Enter|Leave}CS
**     methods for this, but they're too heavyweight for us 
**     here, plus we assert that we can be page fault safe. 
**  b) we don't check if there are events pending or RFA. 
**     This is since until we satisfy the fault, it is quite
**     possible that other events should be deferred. 
*/
#define LOCK(_st) 					\
do {							\
    (_st)->mode = VP$ActivationMode((_st)->vpp); 	\
    VP$ActivationsOff((_st)->vpp); 			\
} while(0); 

#define UNLOCK(_st) if((_st)->mode) VP$ActivationsOn((_st)->vpp);

/* 
** An addcb_fn is called from fast_map when a new mapping is performed
** to allow the stretch driver to update its relevant state. 
*/
typedef bool_t addcb_fn(void * /* really st */, uint32_t vpn, 
			uint32_t pfn, bool_t on_disk);


/* 
** Nailed Stretch Driver state.
** In addition to the default fields, the nailed stretch driver
** keeps track of a linked list of free physical regions. These 
** are contiguous lumps of physical memory with the same logical 
** frame width. It does not (currently) keep track of the physical
** frames which it has used to map stretches; this information is
** already kept within the system mapping tables. 
** It also keeps track of its physical memory allocator (if any). 
*/

typedef struct {
    /* Common part */
    StretchDriver_cl     cl;
    StretchDriver_Kind   kind;
    VP_clp               vpp; 
    bool_t               mode; 
    Heap_clp             heap;
    StretchTbl_clp       strtab;
    FaultHandler_clp     overrides[MAX_HDLRS];

    /* List of pmem we own */
    flink_t              pmeminfo;

    /* Info about our pmem alloc (if any) */
    Frames_clp           frames;     /* Valid closure, or NULL         */
    IDCOffer_clp         frm_offer;  /* Offer to above, or NULL        */
    string_t             frm_name;   /* Name of (on of) above, or NULL */

    /* We don't actually keep any info per page mapped, but we do have
       a callback so we can share routines with other stretch drivers. */
    addcb_fn            *add_mapping; 

} NailedDriver_st; 



/*
** Physical Stretch Driver state. 
** It is a little more complex than the previous examples since 
** it maintains information about each VPN that it maps. 
** This is so that it can unmap the relevant pages when the 
** stretch is removed from the driver. 
** XXX SMH: could also contain info about 'transience' of mappings
** in the case that revocation becomes necessary in this driver. 
*/

typedef struct _vpnlink_t {
    struct _vpnlink_t *next;
    struct _vpnlink_t *prev;
} vpnlink_t;

/* The 'cluster' size below gives how many vpns we deal with per physinfo_t */
#define PHI_SIZE 16 
#define PHI_FULL 0xFFFF

typedef struct {
    vpnlink_t    link;
    uint32_t     cmap;            /* 'cluster' bitmap: 1 bit per array elem */
    uint32_t     vpns[PHI_SIZE];  /* vpns we've mapped (iff rel bit is set) */
} physinfo_t; 


typedef struct {
    /* Common part */
    StretchDriver_cl     cl;
    StretchDriver_Kind   kind;
    VP_clp               vpp; 
    bool_t               mode; 
    Heap_clp             heap;
    StretchTbl_clp       strtab;
    FaultHandler_clp     overrides[MAX_HDLRS];

    /* List of pmem we own */
    flink_t              pmeminfo;

    /* Info about our pmem alloc (if any) */
    Frames_clp           frames;     /* Valid closure, or NULL         */
    IDCOffer_clp         frm_offer;  /* Offer to above, or NULL        */
    string_t             frm_name;   /* Name of (on of) above, or NULL */

    /* We maintain a linked list of pagedinfo_t's for each VPN we've mapped; 
       it's updated by the callback function, st->add_mapping (below) */
    addcb_fn            *add_mapping; 

    vpnlink_t            coreinfo; /* list of all VPNs 'in core'          */
} PhysicalDriver_st; 


/* 
** Paged Stretch Driver state.
** This form of stretch driver requires the most state. 
** We have a number of different types of vpn we need to 
** keep information about. 
** 
**  1) "Virgin"  vpns: these are vpns which we manage (i.e. are 
**     part of some stretch or other which is bound to us), but 
**     which have never been mapped. They are similar to the 
**     demand zero pages of other operating systems. 
**     The PTE in this case has the valid bit cleared, and the 
**     PFN is 'NULL_PFN'. They cannot be on disk --- if we wish
**     to demand page in a real file, then we need to set up 
**     the PTEs to be "On Disk" vpns [see 3 below]. 
**  2) "In Core" vpns: these are the current 'working set' (we
**     hope!). All of these are mapped onto real physical frames 
**     and [potentially] stored on disk. The copy on disk may 
**     or may not be up to date (depending on whether the pte is 
**     dirty or not). 
**     The PTE in this case holds dirty/referenced bits, some 
**     sid, and the pfn which the vpn is mapped on to. 
**     Hence we (the sdriver) need to keep track of whether the
**     vpn is on disk or not. 
**  3) "On Disk" vpns: these are vpns which we have dealt with
**     before, which were written to, and then evicted and dumped
**     to disk [NB: *potentially* these are setup to be demand
**     filled from disk, but currently we don't do this]. 
**     The PTE in this case has the valid bit cleared, and the 
**     PFN is anything other than 'NULL_PFN'; it is interpreted
**     as a disk 'blok' number. 
**
** We could perhaps do with information about the persistence of 
** stretches, or perhaps we could just to that via a slight modification
** of this driver. Who knows; this is for later (as is demand fill
** from disk). 
**
** Hence the "virgin" and "on disk" vpns require no real state in 
** the stretch driver. The "in core" ones, on the other hand, require
** quite a bit. 
** 
*/


/*
** XXX SMH: the below structure is far too large --- but I can't be
** bothered optimising space at present; other things are rather
** more important. But should sort it out at some stage. 
*/

typedef struct {
    vpnlink_t    link;
    uint32_t     vpn;         /* a vpn we have in core              */         
    uint32_t     status;      /* extra info about each vpn          */
    uint32_t     blokno;      /* our blokno on swap [if applicable] */
} pagedinfo_t; 


#define NULL_VPN NULL_PFN

/* The status field tells us more about in-core vpns */
#define VPN_M_OND  0x01    /* the vpn has a copy on disk => blokno valid */
#define VPN_M_PIN  0x02    /* the vpn is *being* paged in */
#define VPN_M_POUT 0x04    /* the vpn is *begin* paged out */
#define VPN_M_REF  0x08
#define VPN_M_DTY  0x10
#define VPN_M_ALL  0xFF


#define IS_OND(_pi)  ((_pi)->status & VPN_M_OND)
#define IS_REF(_pi)  ((_pi)->status & VPN_M_REF)
#define IS_DTY(_pi)  ((_pi)->status & VPN_M_DTY)

#define SET_OND(_pi) ((_pi)->status |= VPN_M_OND)
#define SET_REF(_pi) ((_pi)->status |= VPN_M_REF)
#define SET_DTY(_pi) ((_pi)->status |= VPN_M_DTY)

#define CLR_OND(_pi) ((_pi)->status &= ~VPN_M_OND)
#define CLR_REF(_pi) ((_pi)->status &= ~VPN_M_REF)
#define CLR_DTY(_pi) ((_pi)->status &= ~VPN_M_DTY)



#define NBMAPS  8    
#define ALLBITS ((word_t)-1L)

/* 
** We keep track of swap space as a bitmap of *bloks* --- a 'blok' 
** is a contiguous set of disk blocks which is a multiple of the 
** size of a page. 
** XXX SMH: for now, a blok is always the same size as exactly one page. 
** 
** Our bitmap mgmt structures hold NBMAPS [currently 8] words of bitmap 
** => each allows up to (32 * 8) = 256 bloks on x86, or (64 * 8) = 512 bloks
** on alpha. We maintain a (singly) linked list of these structures, and 
** allocate first fit --- a hint pointer is maintained to the earliest 
** structure which is known to have free bloks.
** The last structure is termianted by NULL, and (potentially) has a 
** number of bloks marked allocated which in effect are off the end 
** of the swap area. 
*/

typedef uint32_t Blok;

typedef struct _blokinfo {
    word_t            bitmap[NBMAPS]; /* Bitmap of free bloks within chunk */
    Blok              base;           /* Base blok of this chunk           */
    struct _blokinfo *next;           /* Pointer to next struct, or NULL   */
} blokinfo_t;



#define VPN(_vaddr) ((word_t)(_vaddr) >> PAGE_WIDTH)
#define PFN(_paddr) ((word_t)(_paddr) >> FRAME_WIDTH)
    




typedef struct {
    /* Common part */
    StretchDriver_cl     cl;
    StretchDriver_Kind   kind;
    VP_clp               vpp; 
    bool_t               mode; 
    Heap_clp             heap;
    StretchTbl_clp       strtab;
    FaultHandler_clp     overrides[MAX_HDLRS];

    /* List of pmem we own */
    flink_t              pmeminfo;

    /* Info about our pmem alloc (if any) */
    Frames_clp           frames;     /* Valid closure, or NULL         */
    IDCOffer_clp         frm_offer;  /* Offer to above, or NULL        */
    string_t             frm_name;   /* Name of (on of) above, or NULL */

    /* We maintain a linked list of pagedinfo_t's for each VPN we've mapped; 
       it's updated by the callback function, st->add_mapping (below) */
    addcb_fn            *add_mapping;

    Time_clp             time;       /* Handle on a time closure             */
    Stretch_clp          iostr;      /* Stretch we use for comms with device */
    Stretch_Size         iosz;       /* Size of above stretch                */
    addr_t               iobase;     /* Base virtual address of above        */
    IO_Rec               recs[2];    /* 'the' IO rec pair (for paging).      */
    FileIO_Request       req;        /* 'the' request structure (for paging) */


    /* Info about our swap device */
    FileIO_clp           fio; 
    IOOffer_clp          fio_offer; 
    USDCtl_clp           partition; 
    IDCOffer_clp         part_offer; 
    string_t             swap_name; 
    uint32_t             blocks_per_blok; /* no of blocks per 'blok'         */
    uint32_t             bloks_per_map;   /* no of 'bloks' per blokinfo_t    */
    blokinfo_t          *swap_map;        /* alloc/free info about swapspace */

    pagedinfo_t          coreinfo;   /* list of all VPNs 'in core'           */
    pagedinfo_t         *hand;       /* current 'hand' position for clock    */
    bool_t               inthd;      /* XXX debug; are we currently in a thd */


} PagedDriver_st; 





/*
** Utility functions for various things (often shared between different
** stretch driver implementations). 
*/
static bool_t add_region(NailedDriver_st *st, addr_t base, word_t nframes, 
			 uint32_t fwidth, Heap_clp heap)
{
    flink_t *headp; 
    flink_t *link;
    flist_t *cur, *new, *next; 
    word_t npf, end, cbase, cend, nbase, nend; 
    bool_t mergelhs, mergerhs; 


    /* Compute the end address of the new region */
    end = (word_t)base + (nframes << fwidth);

    /* And get the number of physical frames it covers too */
    npf = nframes >> (fwidth - FRAME_WIDTH); 

    LOCK(st);

    /* Try to find the correct place to insert it */    
    headp = &st->pmeminfo;
    for(link = headp->next; link != headp; link = link->next) {

	cur = (flist_t *)link; 

	cbase = (word_t)cur->base; 
	cend  = cbase + (cur->npf << FRAME_WIDTH);

	if((word_t)base >= cend)
	    break;
    }

    if(link == headp) { 

	/* We wrapped => need a new list element (at head) */
	if((new = Heap$Malloc(heap, sizeof(*new))) == NULL) {
	    UNLOCK(st);
	    return False;
	}

	new->base    = base; 
	new->npf     = npf; 
	new->fwidth  = fwidth; 
	
	LINK_ADD_TO_HEAD(headp, &new->link);
	UNLOCK(st);
	return True;
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
    mergelhs = ((cur->fwidth == fwidth) && ((word_t)base == cend));
    
    /* Now check if we could merge on the rhs too */
    if(link->next != headp) {
	next      = (flist_t *)link->next; 
	nbase     = (word_t)next->base; 
	mergerhs  = ((next->fwidth == fwidth) && (nbase == end));
    } else {
	/* would wrap, so cannot merge on rhs*/
	next     = (flist_t *)NULL;    /* stop GDB complaining at us */
	nbase    = 0xf00d;             /*  "" */
	mergerhs = False;
    }

    if(!mergelhs && !mergerhs) {

	/* Cannot merge => need a new list element (after 'link'=='cur') */
	if((new = Heap$Malloc(heap, sizeof(*new))) == NULL) {
	    UNLOCK(st);
	    return False;
	}	

	new->base    = base; 
	new->npf     = npf; 
	new->fwidth  = fwidth; 
	
	LINK_INSERT_AFTER(link, &new->link);
	UNLOCK(st);
	return True;
    }


    if(mergelhs) {
	MTRC(eprintf("add_region: coalesce current [%p-%p) & new [%p-%p)\n", 
		     cbase, cend, base, end));
	cur->npf += npf; 
	cend      = cbase + (cur->npf << FRAME_WIDTH);
	MTRC(eprintf("        i.e region is now at [%p-%p)\n", 
		     cbase, cend));
	

	if(mergerhs) {
	    MTRC(eprintf("add_region: plus can merge on rhs..\n"));
	    cur->npf += next->npf; 
	    cend      = cbase + (cur->npf << FRAME_WIDTH);
	    MTRC(eprintf("        i.e region is now at [%p-%p)\n", 
			 cbase, cend));

	    /* Now free 'next' */
	    LINK_REMOVE(link->next);
	    Heap$Free(heap, next);
	} 

	UNLOCK(st);
	return True;
    } 

    /* If get here, we can merge on rhs, but *not* of lhs */
    nend = nbase + (next->npf << FRAME_WIDTH);
    MTRC(eprintf("add_region: coalesce new [%p-%p) and next [%p-%p)\n", 
		 base, end, nbase, nend));
    next->npf += npf; 
    next->base = base; 
    nbase      = (word_t)next->base;
    nend       = nbase + (next->npf << FRAME_WIDTH);
    MTRC(eprintf("        i.e region is now at [%p-%p)\n", 
		 nbase, nend));
    UNLOCK(st);
    return True;
}

/* 
** init_pmemst: intialise the parts of the state to do with physical 
** memory (i.e. list of free mem and handle on allocator). 
** Used when creating  Nailed, Physical or Paged stretch drivers. 
*/
static void init_pmemst(NailedDriver_st *st, Mem_PMem pmem, 
			const Type_Any *pmalloc) 
{
    bool_t ok; 
    int i; 

    LINK_INITIALISE(&st->pmeminfo);

    st->frames    = NULL;
    st->frm_offer = NULL;
    st->frm_name  = NULL;

    if(pmem) {
	MTRC(eprintf("init_pmemst: got some initial pmem at %p\n", pmem));
	ok = True; 
	for(i=0; ok && pmem[i].nframes; i++) {
	    /* Try to add regions to our state; if fail, ignore it */
	    ok = add_region(st, pmem[i].start_addr, pmem[i].nframes, 
			    pmem[i].frame_width, st->heap);
	}
    }
    
    if(pmalloc) {
	switch(pmalloc->type) {
	case Frames_clp__code:
	    MTRC(eprintf("init_pmemst: got a Frames clp!\n"));
	    st->frames = (Frames_cl *)(word_t)pmalloc->val;
	    break;

	case IDCOffer_clp__code:
	    MTRC(eprintf("init_pmemst: got an IDCOffer!\n"));
	    st->frm_offer = (IDCOffer_cl *)(word_t)pmalloc->val;
	    break;

	case string_t__code:
	    MTRC(eprintf("init_pmemst: got a string '%s'!\n", 
			 (char *)(word_t)pmalloc->val));
	    st->frm_name = strduph((char *)(word_t)pmalloc->val, st->heap);
	    break;

	default:
	    /* Got some weird Type.Any; ignore it (though spit a warning) */
	    eprintf("init_pmemst [sd kind %d]: got bad type.any (tc=%qx)\n", 
		    st->kind, pmalloc->type);
	    break;

	}
    }

    return;
}


/* 
** bind_pmalloc: attempts to get a handle on a frames allocator 
** for this stretch driver (which currently can only be a physical 
** stretch driver). Requires that activations be on (for IDC). 
*/
static bool_t bind_pmalloc(PhysicalDriver_st *st)
{
    Type_Any fany; 

    if(!st->frm_offer) {
	
	/* Hmm; don't have an offer; check if we have a name */
	if(!st->frm_name) {
	    eprintf("bind_pmalloc: cannot get a physical allocator.\n");
	    return False;
	}
	
	if(!Context$Get(Pvs(root), st->frm_name, &fany)) {
	    eprintf("bind_pmalloc: failed to lookup '%s' in root context.\n", 
		    st->frm_name);
	    return False;
	}
	
	switch(fany.type) {
	case Frames_clp__code:
	    st->frames = (Frames_cl *)(word_t)fany.val;
	    return True;
	    break;
	    
	case IDCOffer_clp__code:
	    st->frm_offer = (IDCOffer_cl *)(word_t)fany.val; 
	    break;
	    
	default:
	    /* Oops. Nothing useful after lookup. */
	    eprintf("bind_pmalloc: lookup of '%s' gave useless result"
		    "(typecode=%qx)\n", st->frm_name, fany.type);
	    return False;
	    break;
	}
    }
    
    /* Ok, got an offer, so try to bind to it */
    (void)IDCOffer$Bind(st->frm_offer, Pvs(gkpr), &fany);
    st->frames = NARROW(&fany, Frames_clp);
    return True;
}
	


/* 
** init_swapst: once we've obtained an actual FileIO, we can 
** intialise the parts of the state to do with the backing store 
** (e.g. the swapmap on disk, etc). 
** Requires that st->fio is valid. 
*/
static bool_t init_swapst(PagedDriver_st *st, Stretch_cl *iostr)
{
    FileIO_Size length; 
    uint32_t blocksize, nblocks, nbloks;
    uint32_t nmaps, cblok, remain;
    blokinfo_t *cur, **prev; 
    int i, j; 

    if(!st->fio) {
	eprintf("init_swapst: called before FileIO setup!\n");
	return False;
    }

    if(!iostr) {
	/* Oops; we're pretty doomed. */
	eprintf("init_swapst: got FileIO, but no stretch for it!\n");
	return False;   
    }
    
    st->iostr  = iostr; 
    st->iobase = Stretch$Range(st->iostr, &st->iosz); 
    MTRC(eprintf("init_swap: got iostr at %p [%lx bytes at %p]\n", 
		 st->iostr, st->iosz, st->iobase));
    
    if(st->iosz > PAGE_SIZE) {
	/* XXX: We don't yet handle multiple outstanding disk requests
	   in the same stretch driver => can't make use of any 
	   more than a single page of IO stretch. FIXME.  XXX. */
	eprintf("Warning: IO Stretch is too large (size %lx)\n", st->iosz);
    }
    
    /* Setup the 'hardwired' part of our request structure & IO_Rec's. */
	eprintf("About to do getlength, get size on fileIO at %p\n", st->fio);
	eprintf("st->fio->op->GetLength = %p\n", st->fio->op->GetLength);
	eprintf("st->fio->op->BlockSize = %p\n", st->fio->op->BlockSize);
    length     = FileIO$GetLength(st->fio); 
    blocksize  = FileIO$BlockSize(st->fio); 
    nblocks    = (length + blocksize - 1) / blocksize; 
    eprintf("init_swapst: blocksize of fio is %qx, length is %qx\n", 
	    blocksize, length); 
    eprintf("          => total of %qx blocks, or %qx pages.\n", 
	    nblocks, length >> PAGE_WIDTH); 

    /* Setup the swap_map */
    st->blocks_per_blok = PAGE_SIZE / blocksize;  
    st->bloks_per_map   = WORD_LEN * NBMAPS; 

    nbloks = nblocks / st->blocks_per_blok; 
    eprintf("nbloks is 0x%x\n", nbloks);
    nmaps  = (nbloks + st->bloks_per_map - 1) / st->bloks_per_map; 
    eprintf("bloks_per_map is %d; => nmaps required is %d\n", 
	    st->bloks_per_map, nmaps); 

    /* Allocate space for 'nmaps' blokinfo_t structures. We could do this
       lazilly, but since the amount of space is so small [generally] 
       we just do it now and get it over with. */
    cblok = 0; 
    cur   = NULL;
    prev  = &st->swap_map; 
    for(i = 0; i < nmaps; i++) {
	
	cur = Heap$Malloc(st->heap, sizeof(*cur)); 
	for(j = 0; j < NBMAPS; j++) 
	    cur->bitmap[j] = 0; 
	cur->base = cblok;
	*prev = cur; 
	prev  = &cur->next; 

	cblok += st->bloks_per_map;
    }
    
    remain = nbloks % st->bloks_per_map; 
    if(remain) {
	remain = st->bloks_per_map - remain; 
	eprintf("init_swapst: 'allocing' %d bloks end.\n", remain); 
	i = NBMAPS; 
	while(remain >= WORD_LEN) {
	    cur->bitmap[--i]  = ALLBITS; 
	    remain           -= WORD_LEN; 
	}

	if(remain) { 
	    eprintf("Need to do 'sub' alloc of %d bloks in bitmap %d\n", 
		    remain, i); 
	    eprintf("Not impl - fixme.\n");
	    ntsc_dbgstop();
	}
    }

	
    /* Cauterize list */
    if(cur) cur->next = NULL;


    /* Setup the file request structures; one for each 'IO page' we have. */
    st->req.nblocks  = st->blocks_per_blok;
    st->recs[0].base = &st->req; 
    st->recs[0].len  = sizeof(FileIO_Request); 
    st->recs[1].base = st->iobase; 
    st->recs[1].len  = PAGE_SIZE; 



    return True;
}

/* 
** init_swap: setup some basic info about the swap type any 
** we got passed into us (we're a Paged stretch driver).
*/
static bool_t init_swap(PagedDriver_st *st, Stretch_cl *iostr, 
			  const Type_Any *swap) 
{
    st->fio        = NULL;
    st->fio_offer  = NULL;
    st->partition  = NULL;
    st->part_offer = NULL;
    st->swap_name  = NULL;
    st->swap_map   = NULL; 

    switch(swap->type) {
    case FileIO_clp__code: 
	MTRC(eprintf("init_swap: got a FileIO clp!\n"));
	st->fio   = (FileIO_cl *)(word_t)swap->val;
	return init_swapst(st, iostr); 
	break;
	    
    case IOOffer_clp__code:
	MTRC(eprintf("init_swap: got an IOOffer!\n"));
	st->fio_offer = (IOOffer_cl *)(word_t)swap->val;
	break;

    case USDCtl_clp__code: 
	MTRC(eprintf("init_swap: got a USDCtl clp!\n"));
	st->partition = (USDCtl_cl *)(word_t)swap->val;
	break;
	    
    case IDCOffer_clp__code:
	MTRC(eprintf("init_swap: got an IDCOffer!\n"));
	st->part_offer = (IDCOffer_cl *)(word_t)swap->val;
	break;
	
    case string_t__code:
	MTRC(eprintf("init_swap: got a string '%s'!\n", 
		     (char *)(word_t)swap->val));
	st->swap_name = strduph((char *)(word_t)swap->val, st->heap);
	break;
	
    default:
	/* Got some weird Type.Any; not good. */
	eprintf("init_swap [sd kind %d]: got bad type.any (tc=%qx)\n", 
		st->kind, swap->type);
	return False;
	break;
	
    }
    return True;
}


/* 
** bind_swap: attempts to get a handle on a fileIO (for backing 
** storage) for this stretch driver (which currently can only be a 
** paged stretch driver). Requires that activations be on (for IDC). 
*/
static bool_t bind_swap(PagedDriver_st *st)
{
    USD_StreamID sid;
    USDCtl_Error usdrc;
    Type_Any any; 

    if(!st->fio_offer) {
	
	/* Hmm; don't have an offer; check if we have a partition */
	if(!st->partition) {
	    
	    /* No partition either; how about an offer for one? */
	    if(!st->part_offer) {
		
		/* Nothing useful; try the name .... */
		if(!st->swap_name) {
		    eprintf("bind_swap: cannot get any swap space ;-(\n");
		    return False;
		}
		
		
		if(!Context$Get(Pvs(root), st->swap_name, &any)) {
		    eprintf("bind_swap: failed to lookup '%s' in root "
			    "context.\n", st->swap_name);
		    return False;
		}
		
		switch(any.type) {
		case FileIO_clp__code: 
		    st->fio = (FileIO_cl *)(word_t)any.val;
		    return True;
		    break;
		    
		case IOOffer_clp__code:
		    st->fio_offer = (IOOffer_cl *)(word_t)any.val;
		    /* And now try to bind to it */
		    (void)IDCOffer$Bind((IDCOffer_cl *)st->fio_offer, 
					Pvs(gkpr), &any);
		    st->fio = NARROW(&any, FileIO_clp);
		    return True;
		    break;
		    
		case USDCtl_clp__code: 
		    st->partition = (USDCtl_cl *)(word_t)any.val;
		    break;
		    
		case IDCOffer_clp__code:
		    st->part_offer = (IDCOffer_cl *)(word_t)any.val;
		    (void)IDCOffer$Bind(st->part_offer, Pvs(gkpr), &any);
		    st->partition = NARROW(&any, USDCtl_clp);
		    break;
		    
		default:
		    /* Oh dear; severe shaftedness... */
		    eprintf("bind_swap: lookup of '%s' gave useless result "
			    "(typecode=%qx)\n", st->swap_name, any.type);
		    return False;
		    break;
		}
		
	    }
	}
	
	/* 
	** Here we have a partition (in st->partition), but no FileIO
	** as yet  => bind to it. 
	*/
	usdrc = USDCtl$CreateStream(st->partition, (USD_ClientID)0x666, 
				    &sid, (IDCOffer_clp *)&st->fio_offer);
	(void)IDCOffer$Bind((IDCOffer_cl *)st->fio_offer, Pvs(gkpr), &any);
	st->fio = NARROW(&any, FileIO_clp);
	return init_swapst(st, NULL /* XXX SMH: sort out iostr */); 
    }
	
    /* Got no FileIO yet, but do have an offer for one => bind */
    (void)IDCOffer$Bind((IDCOffer_cl *)st->fio_offer, Pvs(gkpr), &any);
    st->fio = NARROW(&any, FileIO_clp);
    return init_swapst(st, NULL /* XXX SMH: sort out iostr */); 
}


/*
** Find a free frame of width at least [XXX SMH currently exactly] 
** fwidth, remove it from the free list, and return its PFN. 
*/
static uint32_t find_frame(NailedDriver_st *st, uint32_t fwidth, Heap_clp heap)
{
    flink_t *headp, *link;
    flist_t *cur; 
    uint32_t pfn;
    uint32_t sanity = 0; 
    
    /* Find the first region of the correct frame width */
    LOCK(st); 
    headp = &st->pmeminfo;
    for(link = headp->next; link != headp; link = link->next) {
	sanity++; 
	cur = (flist_t *)link; 
	if(cur->npf && (cur->fwidth == fwidth))
	    break;
	if(sanity >= 0x100) ntsc_dbgstop();
    }

    if(link == headp) {   /* No luck */ 
	UNLOCK(st); 
	return -1; 
    }

    pfn        = PFN(cur->base); 
    cur->base += (1UL << fwidth);
    cur->npf  -= 1 << (fwidth - FRAME_WIDTH);

    if(cur->npf == 0) {
	LINK_REMOVE(link);
	Heap$Free(heap, cur);
    }

    UNLOCK(st);
    return pfn; 
}



/*
** Utility function to sanity check a vpn which we're going to map.
** [NB: may be called by many kinds of stretch driver and hence 
**      performs only the bare minium.]
*/
static bool_t sanity_check(StretchDriver_cl *self, Stretch_clp str, 
			   word_t vaddr, word_t *pte, word_t *pwidth)
{
    NULLDriver_st *st = (NULLDriver_st *)self->st;  
    StretchDriver_cl *sdriver;
    Stretch_Size sz;
    word_t saddr;

    /* First check if it's in the stretch table */
    if(!StretchTbl$Get(st->strtab, str, (uint32_t *)pwidth, &sdriver))
	return False;

    /* And if it belongs to *us* */
    if(sdriver != self) 
	return False; 


    /* Range check the address */
    saddr = (word_t)STR_INFO(str, &sz);
    if( ((word_t)vaddr < saddr) || ((word_t)vaddr > (saddr + sz)) ) {
	eprintf(" XXX vaddr %p not within range [%lx,%lx]\n", 
		vaddr, saddr, saddr+sz);
	/* Virtual address not within this stretch. Fail. */
	return False;
    }

    /* Now look up the translation of the vaddr & return the pte */
    
    if((*pte = ntsc_trans(VPN(vaddr))) == MEMRC_BADVA)
	return False;

    return True;
}


static INLINE bool_t do_map(uint32_t vpn, uint32_t pfn, word_t pte)
{
    word_t rc; 

    if((rc = ntsc_map(vpn, pfn, BITS(pte)))) {
	switch(rc) {
	case MEMRC_BADVA:
	    eprintf("ntsc_map: bad vpn %lx\n", vpn);
	    break;
	    
	case MEMRC_NOACCESS:
	    eprintf("ntsc_map: cannot map vpn %lx (no meta rights!)\n", vpn);
	    break;
	    
	case MEMRC_BADPA:
	    eprintf("ntsc_map: bad pfn %x\n", pfn);
	    eprintf("vpn %lx, pte = %lx\n", vpn, pte); 
	    ntsc_dbgstop();
	    break;
	    
	case MEMRC_NAILED:
	    eprintf("ntsc_map: vpn %lx is nailed down!\n", vpn);
	    break;
	    
	default:
	    eprintf("ntsc_map: screwed!!!! (rc=%x)\n", rc);
	    ntsc_dbgstop();
	    break;
	}
	return False;
    }
    return True; 
}



/*
** Nailed Stretch Driver callback to add information about a new mapping.
** As the nailed driver only maps (or unmaps) entire Stretch's, we don't
** actually maintain any per-page information at present. 
** Call from fast_map() on a successful ntsc_map(). 
*/
static bool_t nld_add_mapping(PhysicalDriver_st *st, word_t vpn, 
			      uint32_t pfn, bool_t on_disk)
{
    return True; 
}


/*
** Physical Stretch Driver callback to add information about a new mapping.
** Call from fast_map() on a successful ntsc_map(). 
*/
static bool_t phys_add_mapping(PhysicalDriver_st *st, word_t vpn, 
			       uint32_t pfn, bool_t on_disk)
{
    int i; 
    vpnlink_t  *vlk;
    physinfo_t *cur;

    LOCK(st);
    vlk = st->coreinfo.next; 
    while(vlk != &st->coreinfo) {
	if(((physinfo_t *)vlk)->cmap != PHI_FULL) 
	    break;
	vlk = vlk->next; 
    }
    
    if(vlk != &st->coreinfo) { /* Found an entry which ain't full */

	cur = (physinfo_t *)vlk;
	for(i = 0; i < PHI_SIZE; i++) {
	    if(!(cur->cmap & (1 << i))) 
		break;
	}
	cur->vpns[i] = vpn; 
	cur->cmap   |= (1 << i); 

    } else {

	/* No luck finding a free entry; create a new one */
	if(!(cur = Heap$Malloc(st->heap, sizeof(*cur)))) {
	    UNLOCK(st);
	    return False;
	}
	cur->vpns[0] = vpn; 
	cur->cmap    = 1; 
    
	/* Add it to the list (which is roughly ordered by map time) */
	LINK_ADD_TO_TAIL(&st->coreinfo, &cur->link);
	
    }

    UNLOCK(st); 
    return True; 
}



/*
** Paged Stretch Driver callback to add information about a new mapping.
** Call from fast_map() on a successful ntsc_map(). 
*/
static bool_t paged_add_mapping(PagedDriver_st *st, word_t vpn, 
				uint32_t pfn, bool_t on_disk)
{
    pagedinfo_t *new;
    
    /* Create a 'new' entry for the vfn we've just mapped */
    LOCK(st);
    if(!(new = Heap$Malloc(st->heap, sizeof(*new)))) {
	UNLOCK(st);
	return False;
    }

    new->vpn     = vpn;
    new->status  = on_disk ? VPN_M_OND : 0; 
    new->blokno  = 0; 

    /* Add it to the tail of the list (roughly ordered by map time) */
    LINK_ADD_TO_TAIL(&st->coreinfo.link, &new->link);
    UNLOCK(st);
    return True;
}




/*
** Find replacement: when we have no unmapped frames available, and 
** cannot (or don't wish to) get any more, we need to choose a 
** page to be replaced, which is performed by this little routine.
** 
** Essentially second-chance FIFO, implemented via single hand clock.
*/
static pagedinfo_t *find_replacement(PagedDriver_st *st, word_t *pte)
{
    pagedinfo_t *pi; 
    int sanity = 0; 	/* PARANOIA */

    LOCK(st); 

    /* PARANOIA */
    if(LINK_EMPTY(&st->coreinfo.link)) {
	eprintf("find_replacement: I have no in core pages at all!!\n");
	ntsc_dbgstop();
    }

    pi = st->hand; 
    while(True) {

	/* PARANOIA */
	if(sanity++ == 0x100) goto the_end; 

	if(pi->vpn == NULL_VPN) 
	    pi = (pagedinfo_t *)pi->link.next; 

	*pte = ntsc_trans(pi->vpn); 
	if(!IS_REFFED_PTE(*pte)) {
	    /* got one! */
	    st->hand = (pagedinfo_t *)pi->link.next; 
	    LINK_REMOVE(&pi->link); 
	    UNLOCK(st); 
	    return pi; 
	}

	ntsc_map(pi->vpn, PFN_PTE(*pte), UNREF(BITS(*pte))); 
	pi = (pagedinfo_t *)pi->link.next; 
    }

    /* NOTREACHED */
the_end:
    eprintf("find_replacement: terminated?\n");
    ntsc_dbgstop();
    return (pagedinfo_t *)NULL; /* keep gdb happy */
}


/*
** Utility function to map a frame (or frames) under a vpn 
** in the 'fast path' - i.e. without performing any IDC operations.
** This will only work when (a) we don't need to page in anything 
** from disk and (b) we have a spare frame lying around. 
*/
static StretchDriver_Result fast_map(PhysicalDriver_st *st, word_t vpn, 
				     word_t pte, uint32_t pwidth)
{
    uint32_t pfn;
    int nf;

    nf = 1 << (pwidth - PAGE_WIDTH); /* determine how many frames we need */

    if((pfn = find_frame((NailedDriver_st *)st, pwidth, st->heap)) == -1)
	/* Oops: no unmapped frames */
	return StretchDriver_Result_Retry;

    /* Now we have a frame to map info (mapframe), and we've got the 
       information (if any) in there already. Now just need to 
       update the TLBs, etc. */
 
    if(!do_map(vpn, pfn, VALID(pte))) 
	return StretchDriver_Result_Failure;

    /* Let the stretch driver do impl specific things */
    if(!st->add_mapping(st, vpn, pfn, False)) 
	return StretchDriver_Result_Failure; 

    return StretchDriver_Result_Success;
}



/*--------------------------------------SDriverMod Entry Points ----*/

/*
** NewNULL: create an (essentially) empty stretch driver. This 
** can perform no mapping or fault handling, but simply produces
** a closure of the correct type during system startup. 
** Not useful for standard user-domains. 
*/
static StretchDriver_clp NewNULL_m(SDriverMod_cl *self, Heap_clp heap, 
				   StretchTbl_clp strtab)
{
    NULLDriver_st *st;
    int i; 

    if(!(st = Heap$Malloc(heap, sizeof(*st))))
	/* no memory */
	return (StretchDriver_cl *)NULL;
    
    st->kind    = StretchDriver_Kind_NULL;
    st->vpp     = NULL;  /* don't have/need one of these */
    st->heap    = heap;    
    st->strtab  = strtab;

    for(i = 0; i < MAX_HDLRS; i++) 
	st->overrides[i] = (FaultHandler_cl *)NULL; 

    CL_INIT(st->cl, &nullsd_ms, st);
    TRC(eprintf("SDriverMod$NewNULL: returning %p\n", &st->cl));
    return (&st->cl);
}

/*
** NewNailed: creates a stretch driver which pre-maps the pages of
** the stretches it is given to manage. The nailed stretch driver 
** expects never to have to handle a page fault (i.e. demand mapping)
** but still needs to be able to cope with an explicit "Map" operation
** being called in order to guarantee the mapping of a particular 
** page. Similarly, it [currently] does not expect to handle 
** any recoverable faults, but this may change if it is necessary
** for GC, profiling, debugging, etc. 
*/
static StretchDriver_clp NewNailed_m(SDriverMod_cl *self, VP_clp vpp, 
				     Heap_clp heap, StretchTbl_clp strtab, 
				     Mem_PMem pmem, const Type_Any *pmalloc)
{
    NailedDriver_st *st;
    int i; 

    if(!(st = Heap$Malloc(heap, sizeof(*st))))
	/* no memory */
	return (StretchDriver_cl *)NULL;
    
    st->kind    = StretchDriver_Kind_Nailed;
    st->vpp     = vpp;     
    st->heap    = heap;    
    st->strtab  = strtab;

    for(i = 0; i < MAX_HDLRS; i++) 
	st->overrides[i] = (FaultHandler_cl *)NULL; 

    /* Initialise the physical memory structures */
    init_pmemst(st, pmem, pmalloc);
    
    /* Init the (NULL) callback */
    st->add_mapping = (addcb_fn *)nld_add_mapping;

    CL_INIT(st->cl, &nsd_ms, st);
    TRC(eprintf("SDriverMod$NewNailed: returning %p\n", &st->cl));
    return (&st->cl);
}


static StretchDriver_clp NewPhysical_m(SDriverMod_cl *self, VP_clp vpp, 
				       Heap_clp heap, StretchTbl_clp strtab, 
				       Mem_PMem pmem, const Type_Any *pmalloc)
{
    PhysicalDriver_st *st;
    int i; 

    if(!(st = Heap$Malloc(heap, sizeof(*st))))
	/* no memory */
	return (StretchDriver_cl *)NULL;
    
    st->kind   = StretchDriver_Kind_Physical;
    st->vpp    = vpp;     
    st->heap   = heap;    
    st->strtab = strtab;

    for(i = 0; i < MAX_HDLRS; i++) 
	st->overrides[i] = (FaultHandler_cl *)NULL; 

    /* Initialise the physical memory structures */
    init_pmemst((NailedDriver_st *)st, pmem, pmalloc);


    /* Init the vpn list and callback */
    st->add_mapping = (addcb_fn *)phys_add_mapping;
    LINK_INITIALISE(&st->coreinfo);
    
    /* Initialise the closure */
    CL_INIT(st->cl, &pysd_ms, st);

    /* And that's it! */
    TRC(eprintf("SDriverMod$NewPhysical: returning %p\n", &st->cl));
    return (&st->cl);
}


static StretchDriver_clp NewPaged_m(SDriverMod_cl *self, VP_clp vpp, 
				    Heap_clp heap, StretchTbl_clp strtab, 
				    Time_clp time, Mem_PMem pmem, 
				    Stretch_clp iostr, const Type_Any *swap)
{
    PagedDriver_st *st;
    int i; 

    if(!(st = Heap$Malloc(heap, sizeof(*st))))
	/* no memory */
	return (StretchDriver_cl *)NULL;
    
    st->kind   = StretchDriver_Kind_Paged;
    st->vpp    = vpp;     
    st->heap   = heap;    
    st->strtab = strtab;
    st->time   = time;

    for(i = 0; i < MAX_HDLRS; i++) 
	st->overrides[i] = (FaultHandler_cl *)NULL; 

    /* Initialise the physical memory structures */
    init_pmemst((NailedDriver_st *)st, pmem, NULL);

    /* Init the backing store structures  */
    if(!init_swap(st, iostr, swap)) {
	Heap$Free(heap, st); 
	return (StretchDriver_cl *)NULL;
    }

    /* Init the vpn lists and callback */
    LINK_INITIALISE(&st->coreinfo.link);
    st->coreinfo.vpn    = NULL_VPN;
    st->coreinfo.status = 0; 
    st->coreinfo.blokno = 0; 
    st->hand            = &st->coreinfo; 
    st->add_mapping     = (addcb_fn *)paged_add_mapping;

    /* XXX SMH: debugging */
    st->inthd = False;

    /* Initialise the closure */
    CL_INIT(st->cl, &pgsd_ms, st);

    /* And that's it! */
    TRC(eprintf("SDriverMod$NewPaged: returning %p\n", &st->cl));
    return (&st->cl);
}



/*-----------------------------------StretchDriver Entry Points ----*/

static void Bind_m(StretchDriver_cl *self, Stretch_clp str, uint32_t pwidth)
{
    NULLDriver_st *st = (NULLDriver_st *) self->st;
    Stretch_Size sz; 
    addr_t base; 
    bool_t res; 

    /* XXX Access control ? (e.g. meta rights) */
    TRC(eprintf("StretchDriver$Bind: self=%p, str=%p, pwidth=%x\n", 
		self, str, pwidth));

    if(pwidth < PAGE_WIDTH) {
	pwidth = PAGE_WIDTH; 
	eprintf("StretchDriver$Bind: warning, pwidth rounded up to %x\n",
		pwidth);
    } else if(pwidth > PAGE_WIDTH) {
	/* Check size is a multiple of the page width. 
	   XXX SMH: do we need to check base is aligned also??? */
	base = Stretch$Info(str, &sz);
	if(sz & ((1UL << pwidth) - 1)) {
	    eprintf("StretchDriver$Bind [Nailed]: stretch size %p not "
		    "valid for pwidth %x\n", sz, pwidth);
	    ntsc_dbgstop();
	}
    }

    res = StretchTbl$Put(st->strtab, str, pwidth, self);
    if(res) {
	eprintf("StretchDriver$Bind: rebound str=%p to self=%p\n", str, self);
	ntsc_dbgstop();
    }
    return;
}


/*
** Binding a stretch to the nailed stretch causes all of its pages
** to be mapped in advance (if necessary). 
*/
static void NailedBind_m(StretchDriver_cl *self, 
			 Stretch_clp str, uint32_t pwidth)
{
    NailedDriver_st *st = (NailedDriver_st *)self->st;
    Stretch_Size sz; 
    bool_t res; 
    addr_t base; 
    int i, npages;

    /* XXX Access control ? (e.g. meta rights) */

    eprintf("StretchDriver$Bind [Nailed]: self=%p, str=%p, pwidth=%x\n", 
	    self, str, pwidth);
    eprintf("Not yet implemented (properly).\n");
    ntsc_dbgstop();

    if(pwidth < PAGE_WIDTH) {
	pwidth = PAGE_WIDTH; 
	eprintf("StretchDriver$Bind [Nailed]: pwidth rounded up to %x\n",
		pwidth);
    } else if(pwidth > PAGE_WIDTH) {
	/* Check size is a multiple of the page width. 
	   XXX SMH: do we need to check base is aligned also??? */
	base = Stretch$Info(str, &sz);
	if(sz & ((1UL << pwidth) - 1)) {
	    eprintf("StretchDriver$Bind [Nailed]: stretch size %p not "
		    "valid for pwidth %x\n", sz, pwidth);
	    ntsc_dbgstop();
	}
    }

    /* Now we need to map frames under the stretch appropriately */
    npages = sz >> pwidth; 
    for(i=0; i < npages; i++) {
	
	/* finfo_xxx;  */
    }


    res = StretchTbl$Put(st->strtab, str, pwidth, self);
    if(res) 
	eprintf("StretchDriver$Bind: rebound str=%p to self=%p\n", str, self);

    return;
}

static void Remove_m(StretchDriver_cl *self,  Stretch_clp str)
{
    NULLDriver_st *st = (NULLDriver_st *) self->st;
    StretchDriver_cl *sdriver; 
    uint32_t pw; 
    bool_t res; 
    
    TRC(eprintf("StretchDriver$Remove: self=%p, str=%p\n", 
		self, str));

    res = StretchTbl$Remove(st->strtab, str, &pw, &sdriver);
    TRC(eprintf("SDriver$Remove: StrTbl$Remove gave %s; self=%p, str=%p "
		"pwidth = %x, sdriver=%p\n", res ? "True" : "False", 
		self, str, pw, sdriver));
    return;
}


static void NailedRemove_m(StretchDriver_cl *self,  Stretch_clp str)
{
    NailedDriver_st *st = (NailedDriver_st *)self->st;
    StretchDriver_cl *sdriver; 
    uint32_t pw; 
    bool_t res; 
    
    TRC(eprintf("StretchDriver$Remove: self=%p, str=%p\n", 
		self, str));

    res = StretchTbl$Remove(st->strtab, str, &pw, &sdriver);
    eprintf("SDriver$Remove [Nailed]: StrTbl$Remove gave %s; self=%p, str=%p "
	    "pwidth = %x, sdriver=%p\n", res ? "True" : "False", 
	    self, str, pw, sdriver);

    /* XXX Now we need to unmap all the frames under the stretch */
    eprintf("Not yet implemented (unmapping of frames under str)\n");
    ntsc_dbgstop();

    return;
}


static void PhysicalRemove_m(StretchDriver_cl *self, Stretch_clp str)
{
    PhysicalDriver_st *st = (PhysicalDriver_st *) self->st;
    StretchDriver_cl *sdriver; 
    Stretch_Size sz; 
    vpnlink_t *head, *cur, *next; 
    physinfo_t *pi; 
    uint32_t pw, pfn, bvpn, lvpn; 
    word_t pte; 
    bool_t res; 
    int i, idx; 

    res = StretchTbl$Remove(st->strtab, str, &pw, &sdriver);
    TRC(eprintf("SDriver$Remove: StrTbl$Remove gave %s; self=%p, str=%p "
		"pwidth = %x, sdriver=%p\n", res ? "True" : "False", 
		self, str, pw, sdriver));

    bvpn = VPN(Stretch$Info(str, &sz)); 
    lvpn = bvpn + (sz >> PAGE_WIDTH); 

    /* Now scan the list of vpns to see if we've any in core */
    /* XXX SMH: may be quicker to simply ntsc_trans() every 
       page in the stretch and unmap all of them... then 
       don't need buggery squat in our state record. */
    head = &st->coreinfo; 
    next = NULL;
    for(cur = head->next; cur != head; cur = next) {
	pi   = (physinfo_t *)cur; 
	next = cur->next; 
	for(i = 0; i < PHI_SIZE; i++) {
	    idx = 1 << i;
	    if(pi->cmap & idx) {
		if(pi->vpns[i] >= bvpn && pi->vpns[i] < lvpn) {
		    pte = ntsc_trans(pi->vpns[i]); 
		    pfn = PFN_PTE(pte); 
		    TRC(eprintf("Sdriver$Remove: reclaiming va %p (pa %p)\n", 
				(addr_t)((word_t)pi->vpns[i] << PAGE_WIDTH), 
				(addr_t)((word_t)pfn << FRAME_WIDTH)));
		    
		    if(!do_map(pi->vpns[i], 0x1 /* NULL_PFN */, UNMAP(pte)))
			ntsc_dbgstop();
		    
		    /* Add the pfn back onto our free list.... */
		    (void)add_region((NailedDriver_st *)st, 
				     (addr_t)((word_t)pfn << FRAME_WIDTH), 
				     1, FRAME_WIDTH, st->heap);
		}
		pi->cmap &= ~idx; 
	    }
	}
	if(pi->cmap == 0) {
	    /* Cluster now empty --- delete it */
	    TRC(eprintf("Sdriver$Remove: deleting empty cluster.\n"));
	    LINK_REMOVE(&pi->link);   
	    Heap$Free(st->heap, pi); 
	}
    }
    return;
}

static void PagedRemove_m(StretchDriver_cl *self, Stretch_clp str)
{
    PagedDriver_st *st = (PagedDriver_st *) self->st;
    StretchDriver_cl *sdriver; 
    Stretch_Size sz; 
    vpnlink_t *head, *cur, *next; 
    pagedinfo_t *pi; 
    uint32_t pw, pfn, bvpn, lvpn; 
    word_t pte; 
    bool_t res; 
    int i, idx; 

    res = StretchTbl$Remove(st->strtab, str, &pw, &sdriver);
    TRC(eprintf("SDriver$Remove: StrTbl$Remove gave %s; self=%p, str=%p "
		"pwidth = %x, sdriver=%p\n", res ? "True" : "False", 
		self, str, pw, sdriver));

    bvpn = VPN(Stretch$Info(str, &sz)); 
    lvpn = bvpn + (sz >> PAGE_WIDTH); 

#if 0
    /* Now scan the list of vpns to see if we've any in core */
    head = &st->coreinfo; 
    for(cur = head->next; cur != head; cur = next) {
	pi   = (pagedinfo_t *)cur; 
	next = cur->next; 
	for(i = 0; i < PHI_SIZE; i++) {
	    idx = 1 << i;
	    if(pi->cmap & idx) {
		if(pi->vpns[i] >= bvpn && pi->vpns[i] < lvpn) {
		    pte = ntsc_trans(pi->vpns[i]); 
		    pfn = PFN_PTE(pte); 
		    (eprintf("Sdriver$Remove: reclaiming va %p (pa %p)\n", 
			     (addr_t)((word_t)pi->vpns[i] << PAGE_WIDTH), 
			     (addr_t)((word_t)pfn << FRAME_WIDTH)));
		    
		    if(!do_map(pi->vpns[i], pfn, UNMAP(pte)))
			ntsc_dbgstop();
		    
		    /* Add the pfn back onto our free list.... */
		    (void)add_region((NailedDriver_st *)st, 
				     (addr_t)((word_t)pfn << FRAME_WIDTH), 
				     1, FRAME_WIDTH, st->heap);
		}
		pi->cmap &= ~idx; 
	    }
	}
	if(pi->cmap == 0) {
	    /* Cluster now empty --- delete it */
	    eprintf("Sdriver$Remove: deleting empty cluster.\n");
	    LINK_REMOVE(&pi->link);   
	    Heap$Free(st->heap, pi); 
	}
    }
    /* And scan the list of vpns to see if we've any on disk */
    head = &st->swapinfo; 
    next = NULL;
    for(cur = head->next; cur != head; cur = next) {
	pi   = (pagedinfo_t *)cur; 
	next = cur->next; 
	if(pi->vpn >= bvpn && pi->vpn < lvpn) {
	    eprintf("SDriver$Remove: 'freeing' va %p (diskblk %lx)\n", 
		    (addr_t)((word_t)pi->vpn << PAGE_WIDTH), pi->blockno);

	    /* XXX SMH: fix this when have disk layout bitmap */
	    LINK_REMOVE(&pi->link); 
	    Heap$Free(st->heap, pi); 
	}
    }
#else
#warning Need to rewrite PagedRemove to get blockno from page table.
#endif

    return;
}


static StretchDriver_Kind GetKind_m(StretchDriver_cl *self)
{
    NULLDriver_st *st = (NULLDriver_st *) self->st;
    
    return st->kind;
}


static StretchTbl_clp GetTable_m(StretchDriver_cl *self)
{
    NULLDriver_st *st = (NULLDriver_st *) self->st;
    
    return st->strtab;
}




static StretchDriver_Result NULLMap_m(StretchDriver_cl *self, 
				      Stretch_clp str, addr_t vaddr)
{
    eprintf("StretchDriver$Map (NULL driver) unimplemented. halting.\n");
    ntsc_halt();
    return StretchDriver_Result_Success;
}


/*
** For Nailed StretchDrivers, the Map operation should never really
** need to be called. However, some people call Map on a stretch/addr
** pair to ensure that a valid mapping does exist; so for use by such
** code we simply sanity check the arguments, and check that the page
** tables hold some sort of physical and valid mapping for "vaddr".
*/
static StretchDriver_Result NailedMap_m(StretchDriver_cl *self, 
					Stretch_clp str, 
					addr_t vaddr)
{
    NailedDriver_st *st = (NailedDriver_st *) self->st;
    word_t pte, pwidth;
    bool_t fhres; 

    ETRC(eprintf("StretchDriver$Map [Nailed]: self=%p, str=%p, vaddr=%p\n", 
	    self, str, vaddr));

    if(!sanity_check(self, str, (word_t)vaddr, &pte, &pwidth))
	return StretchDriver_Result_Failure;

    if(st->overrides[Mem_Fault_PAGE]) {
	fhres = FaultHandler$Handle(st->overrides[Mem_Fault_PAGE], 
			    str, vaddr, Mem_Fault_PAGE); 
#ifndef CHAINING_HANDLER
	return (fhres ? StretchDriver_Result_Success : 
		StretchDriver_Result_Failure); 
#else
	if (fhres) return StretchDriver_Result_Success;
#endif
    }

    if(!IS_VALID_PTE(pte)) 
	return StretchDriver_Result_Failure;

    return StretchDriver_Result_Failure;
}


static const uint32_t frame_quantum[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10,
    0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x02, 0x02, 
    0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 
};



    
static StretchDriver_Result PhysicalMap_m(StretchDriver_cl *self, 
					  Stretch_clp str, 
					  addr_t vaddr)
{
    PhysicalDriver_st *st = (PhysicalDriver_st *) self->st;
    StretchDriver_Result res; 
    word_t pte, pwidth;
    bool_t fhres; 

    ETRC(eprintf("StretchDriver$Map [Physical]: self=%p, str=%p, vaddr=%p\n", 
		 self, str, vaddr));
    
    if(!sanity_check(self, str, (word_t)vaddr, &pte, &pwidth)) {
	ntsc_dbgstop();
	return StretchDriver_Result_Failure;
    }

    if(st->overrides[Mem_Fault_PAGE]) {
	fhres = FaultHandler$Handle(st->overrides[Mem_Fault_PAGE], 
				    str, vaddr, Mem_Fault_PAGE); 
#ifndef CHAINING_HANDLER
	return ( fhres ? StretchDriver_Result_Success : 
		 StretchDriver_Result_Failure); 
#else
	if (fhres) return StretchDriver_Result_Success;
#endif
    }

    if(IS_VALID_PTE(pte)) {
	/* our work is done ;-) [XXX can this happen?] */
	ETRC(eprintf("Page pte=%p (vaddr=%p) is already mapped\n", 
		     pte, vpn << PAGE_WIDTH));
	return StretchDriver_Result_Success;
    }

    if(IS_NAILED_PTE(pte)) {
	/* urk! got invalid page which is *supposedly* nailed */
	ETRC(eprintf("Nailed page pte=%p (vaddr=%p) has come unmapped!\n", 
		     pte, vpn << PAGE_WIDTH));
	return StretchDriver_Result_Failure;
    }

    /* Ok, if we get this far, sanity checks are ok, and we can get on
       with mapping something or another into place. */

    res = fast_map(st, VPN(vaddr), pte, pwidth);

    if( (res != StretchDriver_Result_Retry) || !VP$ActivationMode(st->vpp) ) 
	return res;


    /* 
    ** If get here, then got a retry, and we've activations on. 
    ** => try to grab some more memory from the frames allocator. 
    */

    /* First ensure that we *have* a frames allocator */
    if(!st->frames && !bind_pmalloc(st)) 
	return StretchDriver_Result_Failure; 

    /* 
    ** Here have st->frames valid; go and get some more memory. 
    ** We need to determine how much to ask for --- too much, and 
    ** we may be refused. Too little and we may end up having to 
    ** perform IDC every time. For now we simply lookup the number of 
    ** frames to ask for depending on the frame width (which we want
    ** to be the same as the page width). 
    */
    {
	uint16_t fwidth = (uint16_t)pwidth; 
	uint32_t nframes = frame_quantum[fwidth]; 
	uint32_t nbytes  = (nframes << fwidth); 
	addr_t base; 

	if((base = Frames$Alloc(st->frames, nbytes, fwidth)) == NO_ADDRESS) {
	    eprintf("StretchDriver$Map [Physical]: failed to allocate any"
		    "more physical resources.\n");
	    return StretchDriver_Result_Failure; 
	}
	
	if(!add_region((NailedDriver_st *)st, base, 
		       nframes, fwidth, st->heap)) {
	    eprintf("StretchDriver$Map [Physical]: failed to add my new"
		    "physical memory region to my bookkeeping structure.\n");
	    return StretchDriver_Result_Failure; 
	}
    }
    
    /* Now we can try again */
    res = fast_map(st, VPN(vaddr), pte, pwidth);
    return res; 
}


static INLINE void ReadBlok(PagedDriver_st *st, uint32_t blokno)
{
    uint32_t nrecs; 
    word_t   handle; 

    st->req.blockno  = blokno * st->blocks_per_blok; 
    st->req.op       = FileIO_Op_Read; 
    IO$PutPkt((IO_cl *)st->fio, 2, st->recs, (word_t)0, FOREVER);
    IO$GetPkt((IO_cl *)st->fio, 2, st->recs, FOREVER, &nrecs, &handle);
    if(nrecs != 2) ntsc_dbgstop();  /* Paranoia */
}

static INLINE void WriteBlok(PagedDriver_st *st, uint32_t blokno)
{
    uint32_t nrecs; 
    word_t   handle; 

    st->req.blockno  = blokno * st->blocks_per_blok; 
    st->req.op       = FileIO_Op_Write; 
    IO$PutPkt((IO_cl *)st->fio, 2, st->recs, (word_t)0, FOREVER);
    IO$GetPkt((IO_cl *)st->fio, 2, st->recs, FOREVER, &nrecs, &handle);
    if(nrecs != 2) ntsc_dbgstop();  /* Paranoia */
}

static INLINE void XchgBloks(PagedDriver_st *st, uint32_t wblokno, 
			     uint32_t rblokno)
{
    uint32_t nrecs; 
    word_t   handle; 

    /* XXX SMH: currently FileIO's only support Read and Write operations, 
       so we need to do an explicit Write followed by an explicit Read 
       in order to exchange data. */
    st->req.blockno  = wblokno * st->blocks_per_blok; 
    st->req.op       = FileIO_Op_Write; 
    IO$PutPkt((IO_cl *)st->fio, 2, st->recs, (word_t)0, FOREVER);
    IO$GetPkt((IO_cl *)st->fio, 2, st->recs, FOREVER, &nrecs, &handle);
    if(nrecs != 2) ntsc_dbgstop();  /* Paranoia */

    st->req.blockno  = rblokno * st->blocks_per_blok; 
    st->req.op       = FileIO_Op_Read; 
    IO$PutPkt((IO_cl *)st->fio, 2, st->recs, (word_t)0, FOREVER);
    IO$GetPkt((IO_cl *)st->fio, 2, st->recs, FOREVER, &nrecs, &handle);
    if(nrecs != 2) ntsc_dbgstop();  /* Paranoia */
}


INLINE Blok ALLOC_BLOK(PagedDriver_st *st) 
{
    blokinfo_t *cur; 
    uint32_t res; 
    int i, j; 

    i   = 0; 
    cur = st->swap_map; 
    while(cur) {
	for(i = 0; i < NBMAPS; i++) {
	    if(cur->bitmap[i] != ALLBITS)
		goto found; 
	}
	cur = cur->next; 
    }

    eprintf("ALLOC_BLOCK: failed [out of swap space].\n");
    ntsc_dbgstop();

found:
    for(j = 0; j < WORD_LEN; j++) {
	if(!(cur->bitmap[i] & (1UL << j)))
	    break;
    }
    cur->bitmap[i] |= (1UL << j);
    res = cur->base + ((i * WORD_LEN) + j);

    TRC(eprintf("ALLOC_BLOCK: returning blok #%08d\n", res));
    return res; 
}

INLINE void FREE_BLOK(PagedDriver_st *st, Blok blok)
{
    eprintf("FREE_BLOK: not implemented.\n"); 
    return;
}

#if 0
static void PageThread(PagedDriver_st *st)
{
    while(True) {
	if (/* dirty blocks to page out */) {
	    WriteBlock(st, x); 
	    MarkClean(st, x); 
	}
	if (/* new blocks to page in */) {
	    ReadBlock(st, x); 
	}
    }
}
#endif

static StretchDriver_Result PagedMap_m(StretchDriver_cl *self, 
				       Stretch_clp str, 
				       addr_t vaddr)
{
    PagedDriver_st *st = (PagedDriver_st *) self->st;
    StretchDriver_Result res; 
    word_t pte, vpte, pwidth;
    uint32_t vpn = VPN(vaddr); 
    pagedinfo_t *victim; 
    uint32_t pfn, vpfn; 
    bool_t fhres, vdty; 

    if(st->inthd) ntsc_dbgstop();

    if(!sanity_check(self, str, (word_t)vaddr, &pte, &pwidth)) {
	ntsc_dbgstop();
	return StretchDriver_Result_Failure;
    }

    if(st->overrides[Mem_Fault_PAGE]) {
	fhres = FaultHandler$Handle(st->overrides[Mem_Fault_PAGE], 
				    str, vaddr, Mem_Fault_PAGE); 
#ifndef CHAINING_HANDLER
	return ( fhres ? StretchDriver_Result_Success : 
		 StretchDriver_Result_Failure); 
#else
	if (fhres) return StretchDriver_Result_Success;
#endif
    }

    if(IS_VALID_PTE(pte)) {
	/* our work is done ;-) [XXX can this happen?] */
	ETRC(eprintf("Page pte=%p (vaddr=%p) is already mapped\n", 
		     pte, vpn << PAGE_WIDTH));
	return StretchDriver_Result_Success;
    }

    if(IS_NAILED_PTE(pte)) {
	/* urk! got invalid page which is *supposedly* nailed */
	ETRC(eprintf("Nailed page pte=%p (vaddr=%p) has come unmapped!\n", 
		     pte, vpn << PAGE_WIDTH));
	ntsc_dbgstop();
	return StretchDriver_Result_Failure;
    }

    pfn = PFN_PTE(pte); 
    if(pfn != NULL_PFN) {  /* We need to page this in from disk */
	if(!VP$ActivationMode(st->vpp)) 
	    /* Can't do IDC to the backing store => retry later */
	    return StretchDriver_Result_Retry; 
    } 
    
    if(pfn == NULL_PFN) {  

	/* Don't need to page anything in => try the fast path option:
	   grab a free frame and map it under our faulting virtual address. */

	res = fast_map((PhysicalDriver_st *)st, vpn, MAP(pte), pwidth);
	if((res != StretchDriver_Result_Retry) || !VP$ActivationMode(st->vpp))
	    return res;
    }
    
    st->inthd = True;

    /* 
    ** If get here, then got a retry, and we've activations on. 
    ** => need to do some disky things.
    */

    /* First ensure that we *have* a backing store */
    if(!st->fio && !bind_swap(st))
	return StretchDriver_Result_Failure;

    /* If we get this far, we have a valid st->fio; now select a victim */
    victim = find_replacement(st, &vpte); 
    vpfn   = PFN_PTE(vpte);
    vdty   = IS_DIRTY_PTE(vpte); 

    /* 'Lock' the victim page by unmapping it */
    vpte = UNMAP(vpte); 
    if(!do_map(victim->vpn, vpfn, vpte)) 
	ntsc_dbgstop();

    /*
    ** SMH: we do an 'artificial' test called WTEST in which we 
    ** write dirty pages to disk, but never need to re-reread them 
    ** in. This enumlates the case where pre-paging is optimal. 
    */
// #define WTEST
    

    if(vdty) {   /* victim dirty => write to disk */

	/* Map the pfn under our 'IO' stretch */
	LOCK(st); 
	if((word_t)st->iobase & 1) {
	    eprintf("recursive disk thing!\n"); 
	    ntsc_dbgstop();
	}
	(word_t)st->iobase |= 1; 
	UNLOCK(st); 
	if(!do_map(VPN(st->iobase), vpfn, VALID(vpte)))
	    ntsc_dbgstop();

	/* If we've never been on disk before, allocate a new 'block' */
	if(!IS_OND(victim)) {
	    victim->blokno = ALLOC_BLOK(st); 
	    SET_OND(victim); 
	}


#ifdef WTEST
	if(False) {
#else
	if(pfn != NULL_PFN) {  /* We need to page in from disk */
#endif
	    TRC(eprintf("DTY Victim+COD Faulter: "
			"exchange vpn %x & vpn %x [pfn %x]\n", 
			victim->vpn, vpn, vpfn)); 
	    XchgBloks(st, victim->blokno, pfn);
	    
	    LOCK(st); 
	    if(!((word_t)st->iobase & 1)) {
		eprintf("strangeness!!!\n"); 
		ntsc_dbgstop();
	    }
	    (word_t)st->iobase &= ~1U; 
	    UNLOCK(st); 

	    
	} else { 
	    TRC(eprintf("DTY Victim: "
			"Flush vpn %x [from vpn %x pfn %x] to disk\n", 
			victim->vpn, VPN(st->iobase), vpfn)); 
	    WriteBlok(st, victim->blokno); 

	    LOCK(st); 
	    if(!((word_t)st->iobase & 1)) {
		eprintf("strangeness!!!\n"); 
		ntsc_dbgstop();
	    }
	    (word_t)st->iobase &= ~1U; 
	    UNLOCK(st); 

	}

	/* Unmap the io base */
	if(!do_map(VPN(st->iobase), 0x0, UNMAP(vpte)))
	    ntsc_dbgstop();
	
	/* Record the victims blokno in its pte */
	if(!do_map(victim->vpn, victim->blokno, UNMAP(vpte)))
	    ntsc_dbgstop();
	/* 
	** Free victim entry XXX SMH possibly should just update
	** the entry with new info since we're about to create a 
	** new one below; but not yet since have async IO to sort.
	*/
	Heap$Free(st->heap, victim); 

    } else {

	/* Clean => don't need to flush victim page out to disk. */
	TRC(eprintf("Got victim [vpn %x] which is clean.\n", victim->vpn));

#ifdef WTEST
	if(False) {
#else
	if(pfn != NULL_PFN) {  /* We need to page this in from disk */
#endif
	    /* Need to page in from disk. */
	    TRC(eprintf("COD Faulter: load vpn %x [into pfn %x] from disk\n", 
			vpn, vpfn)); 
	    
	    LOCK(st); 
	    if((word_t)st->iobase & 1) {
		eprintf("recursive disk thing!\n"); 
		ntsc_dbgstop();
	    }
	    (word_t)st->iobase |= 1; 
	    UNLOCK(st); 

	    /* Map the pfn under our 'IO' stretch */
	    if(!do_map(VPN(st->iobase), vpfn, VALID(vpte)))
		ntsc_dbgstop();

	    TRC(eprintf("Blokno [from faulting pte] is %x\n", pfn));
	    ReadBlok(st, pfn);

	    /* Unmap the io base */
	    if(!do_map(VPN(st->iobase), 0x0, UNMAP(vpte)))
		ntsc_dbgstop();

	    LOCK(st); 
	    if(!((word_t)st->iobase & 1)) {
		eprintf("strangeness!!!\n"); 
		ntsc_dbgstop();
	    }
	    (word_t)st->iobase &= ~1U; 
	    UNLOCK(st); 
	    
	}
	
	

	/* 
	** If victim was on disk, record its blokno in the pte; else 
	** (unusual in real-life, but happens in my tests) mark it 
	** as a virgin. Then free its entry [should just update
	** it with new info, since we are about to recreate one below]*/
	if(!do_map(victim->vpn, (IS_OND(victim)) ? victim->blokno : 
		   NULL_PFN, UNMAP(vpte))) 
	    ntsc_dbgstop();
	Heap$Free(st->heap, victim); 
    }

    /* Now we have a frame to map into (vpfn), and we've got the 
       information (if any) in there already. Now just need to 
       update the TLBs, etc. */
    if(!do_map(vpn, vpfn, MAP(pte))) 
	return StretchDriver_Result_Failure; 

    if(!paged_add_mapping(st, vpn, vpfn, (pfn != NULL_PFN))) 
	return StretchDriver_Result_Failure; 

    st->inthd = False;
    return StretchDriver_Result_Success;
}




static const char *const reasons[] = {
    "Translation Not Valid", 
    "Unaligned Access", 
    "Access Violation", 
    "Instruction Fault", 
    "FOE", 
    "FOW", 
    "FOR", 
    "Page Fault"
    "Breakpoint", 
};



static StretchDriver_Result Fault_m(StretchDriver_cl *self, Stretch_clp str, 
				    addr_t vaddr, Mem_Fault reason)
{
    NULLDriver_st *st = (NULLDriver_st *) self->st;
    dcb_rw_t *rwp = RW(st->vpp);
    dcb_ro_t *rop = rwp->dcbro;
    bool_t fhres; 

    TRC(eprintf("StretchDriver$Fault: self=%p, str=%p, vaddr=%p, reason=%s\n", 
		self, str, vaddr, reasons[reason]));


    if(st->overrides[reason]) {
	fhres = FaultHandler$Handle(st->overrides[reason], 
				    str, vaddr, reason); 
#ifndef CHAINING_HANDLER
	return ( fhres ? StretchDriver_Result_Success : 
		 StretchDriver_Result_Failure); 
#else
	if (fhres) return StretchDriver_Result_Success;
#endif
    }
    
    /* Determine if the fault is recoverable or not */
    switch(reason) {

    case Mem_Fault_TNV:
    case Mem_Fault_ACV:
    case Mem_Fault_FOE:
    case Mem_Fault_UNA:
	eprintf("*** StretchDriver$Fault (did=%qx): va=%p\n", 
		rop->id, rwp->mm_va);
	eprintf("Unrecoverable fault '%s'\n", reasons[rwp->mm_code]);
	eprintf("Terminating domain %lx for now\n", rop->id);
	eprintf("[Should simply kill offending thread really]\n");
	ntsc_dbgstop();
	return StretchDriver_Result_Failure; 
	break;

    case Mem_Fault_PAGE:
	/* Page fault? Here? We should have called 'Map' in that case. 
	   Sounds like things are rather shafted... oops. */
	eprintf("*** StretchDriver$Fault (did=%qx): va=%p\n", 
		rop->id, rwp->mm_va);
	eprintf("Got a *page* fault [should have called Map] => screwed.\n");
	ntsc_dbgstop();
	return StretchDriver_Result_Failure; 
	break;
	
    case Mem_Fault_FOR:
	/* FOR is nominally recoverable, but we don't expect to get any 
	   of them on any architecture any more (PALcode now handles 'em
	   on alpha, and no-one else generates them). */
	eprintf("Domain %qx: Fault on Read from address %lx.\n", 
		rop->id, rwp->mm_va);
	ntsc_dbgstop();
	return StretchDriver_Result_Failure; 
	break;

    case Mem_Fault_FOW:
	/* FOW is the same as FOR - nominally recoverable, but no longer
	   expected; it is considered an error if one arrives. */
	eprintf("Domain %qx: Fault on Write to address %lx.\n", 
		rop->id, rwp->mm_va);
	ntsc_dbgstop();
	return StretchDriver_Result_Failure; 
	break;
    
    default:
	eprintf("*** StretchDriver$Fault (did=%x): va=%p\n", 
		rop->id, rwp->mm_va);
	eprintf("Domain %qx: bad memory fault reason %d.\n", rop->id, reason);
	ntsc_dbgstop();
	return StretchDriver_Result_Failure; 
	break;
	
    }

    return StretchDriver_Result_Success;
}



static FaultHandler_clp  AddHandler_m(StretchDriver_cl *self, 
				      Mem_Fault reason, 
				      FaultHandler_cl  *handler)
{
    NULLDriver_st   *st = (NULLDriver_st *) self->st;
    FaultHandler_cl *result; 

    if(reason > MAX_HDLRS) {
	eprintf("StretchDriver$AddHandler: bogus reason %d\n", reason); 
	return NULL; 
    }
    
    result = st->overrides[reason]; 
    st->overrides[reason] = handler; 
    return result; 

}
static StretchDriver_Result Lock_m(StretchDriver_cl *self, 
				   Stretch_clp str, 
				   addr_t vaddr) 
{
    eprintf("StretchDriver$Lock: not implemented.\n");
    return StretchDriver_Result_Failure; 
}

static StretchDriver_Result Unlock_m(StretchDriver_cl *self, 
				   Stretch_clp str, 
				   addr_t vaddr)
{
    eprintf("StretchDriver$Unlock: not implemented.\n");
    return StretchDriver_Result_Failure; 
}

static uint32_t Revoke_m(StretchDriver_cl *self, uint32_t maxf)
{
    eprintf("StretchDriver$Revoke: not implemented.\n");
    return 0;
}
