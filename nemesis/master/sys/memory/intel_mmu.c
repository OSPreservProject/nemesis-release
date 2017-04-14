/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      sys/memory
**
** FUNCTIONAL DESCRIPTION:
** 
**      This is the Intel specific code to update MMU data structures
**      It is used by both STD & EXPT memory systems.
** 
** ID : 	$Id: intel_mmu.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
#include <ntsc.h>

#include <RamTab.ih>
#include <MMUMod.ih>
#include <ProtectionDomain.ih>
#include <StretchAllocatorF.ih>

#include "mmu.h"

#include <contexts.h>

#ifdef DEBUG
#define TRC(_x) _x
#else 
#define TRC(_x)
#endif

#define MTRC(_x)


#define UNIMPLEMENTED  printf("%s: UNIMPLEMENTED\n", __FUNCTION__);


static const MMUMod_New_fn  New_m;
static const MMUMod_Done_fn Done_m;

static MMUMod_op mmumod_ms = {
    New_m,
    Done_m
};

static const MMUMod_cl mmumod_cl = { &mmumod_ms, NULL };
CL_EXPORT (MMUMod, MMUMod, mmumod_cl);
CL_EXPORT_TO_PRIMAL(MMUMod, MMUModCl, mmumod_cl);

static const MMU_Engage_fn         Engage_m;
static const MMU_AddRange_fn       AddRange_m;
static const MMU_AddMappedRange_fn AddMappedRange_m;
static const MMU_UpdateRange_fn    UpdateRange_m;
static const MMU_FreeRange_fn      FreeRange_m;
static const MMU_NewDomain_fn      NewDomain_m;
static const MMU_IncDomain_fn      IncDomain_m;
static const MMU_FreeDomain_fn     FreeDomain_m;
static const MMU_SetProt_fn        SetProt_m;
static const MMU_QueryProt_fn      QueryProt_m;
static const MMU_QueryASN_fn       QueryASN_m;
static const MMU_QueryGlobal_fn    QueryGlobal_m;
static const MMU_CloneProt_fn      CloneProt_m;

static MMU_op mmu_ms = {
    Engage_m,
    AddRange_m,
    AddMappedRange_m,
    UpdateRange_m,
    FreeRange_m,
    NewDomain_m,
    IncDomain_m,
    FreeDomain_m,
    SetProt_m,
    QueryProt_m,
    QueryASN_m,
    QueryGlobal_m,
    CloneProt_m,
};


static const RamTab_Size_fn RamTabSize_m; 
static const RamTab_Base_fn RamTabBase_m; 
static const RamTab_Put_fn  RamTabPut_m; 
static const RamTab_Get_fn  RamTabGet_m; 
static RamTab_op rtab_ms = {
    RamTabSize_m, 
    RamTabBase_m,
    RamTabPut_m, 
    RamTabGet_m
};


#define M(b) ((b)<<20)
#define K(b) ((b)<<10)

#define IN_REGION(_pmem, _pa) (						      \
  ((_pa) >= (word_t)(_pmem).start_addr) &&				      \
  ((_pa) < (word_t)(_pmem).start_addr+((_pmem).nframes<<(_pmem).frame_width)) \
    )

typedef struct _pdom_st {
    uint16_t      refcnt;  /* reference count on this pdom    */
    uint16_t      gen;     /* current generation of this pdom */
    Stretch_clp   stretch; /* handle on stretch (for destroy) */ 
} pdom_st; 


/* 
** The Pentium level 1 page table (or "page directory") has 1024 entries
** each of 32-bits each. These map to either a second level page table 
** (in the case of 4K pages), or directly to a 4Mb translation. 
*/
#define N_L1ENTS    1024
#define L1IDX(_va)  ((word_t)(_va) >> 22)


typedef uint8_t     l2_info;    /* free or used info for 1K L2 page tables */
#define L2FREE      (l2_info)0x12
#define L2USED      (l2_info)0x99

/* we alloc 4K for the second level pagetables, and 4K for their shadows */
#define N_L2ENTS      1024
#define L2SIZE        K(8)   
#define SHADOW(_va) ((char *)((void *)(_va) + K(4)))

typedef struct _mmu_st {

    hwpde_t               ptab[N_L1ENTS];  /* Level 1 page 'directory'    */
    swpte_t               stab[N_L1ENTS];  /* Level 1 shadows (4Mb pages) */
    hwpde_t               vtab[N_L1ENTS];  /* Virtual addresses of L2s    */

    MMU_cl                cl;
    RamTab_cl             rtcl; 

    word_t                next_pdidx;          /* Next free pdom idx (hint) */
    pdom_t               *pdom_tbl[PDIDX_MAX]; /* Map pdom idx to pdom_t's  */
    pdom_st               pdominfo[PDIDX_MAX]; /* Map pdom idx to pdom_st's */

    bool_t                use_pge;             /* Set iff we can use PGE    */

    Frames_cl            *frames;
    Heap_cl              *heap;
    StretchAllocator_cl  *sysalloc;

    uint32_t              nframes;

    addr_t	          va_l1;   /* virtual  address of l1 page table */
    addr_t	          pa_l1;   /* physical address of l1 page table */

    addr_t	          va_l2;   /* virtual  address of l2 array base */
    addr_t	          pa_l2;   /* physical address of l2 array base */

    addr_t	          vtab_va; /* virtual address of l2 PtoV table  */

    RamTable_t           *ramtab;  /* base of ram table            */
    word_t                maxpfn;  /* size of above table          */


    uint32_t              maxl2;   /* index of the last available chunk   */
    uint32_t              nextl2;  /* index of first potential free chunk */
    l2_info               info[1]; /* free/used l2 info; actually maxl2   */

} MMU_st;


#define BITMAP_BIT(_va) (((_va) >> 22) & 0x1F) /* Each bit in each word 
						  represents 4Mb => 5 bits 
						  at 22 bit offset */
#define BITMAP_IDX(_va) ((_va) >> 27)          /* Each array word represents 
						  32*4Mb = 128Mb => 27 bits */
#define N_EXTRA_L2S     32                     /* We require an additional 
						  margin of L2 ptabs */


INLINE uint16_t alloc_pdidx(MMU_st *st)
{
    int i;

    i    = st->next_pdidx; 
    do {
	if(st->pdom_tbl[i] == NULL) {
	    st->next_pdidx = (i + 1) % PDIDX_MAX;
	    return i; 
	}
	i = (i + 1) % PDIDX_MAX;
    } while(i != st->next_pdidx); 

    eprintf("alloc_pdidx: out of identifiers!\n");
    ntsc_dbgstop();
}

/* 
** Compute how much space is required for the ram table; this is a 
** system wide table which contains ownership information (and other 
** misc. stuff) for each frame of 'real' physical memory (viz. DRAM).
*/
static word_t rtab_reqd(Mem_PMem pmem, word_t *maxpfn) 
{
    int i;
    word_t lastf, maxf = 0; 


#define IS_RAM(_attr) (!SET_IN(_attr, Mem_Attr_ReadOnly) &&	\
		       !SET_IN(_attr, Mem_Attr_NonMemory)) 

    /* Calculate "maxf"; the index of the top frame of DRAM. */
    maxf = 0;
    for(i = 0; pmem[i].nframes != 0; i++) {
	if(IS_RAM(pmem[i].attr)) {
	    lastf = ((word_t)pmem[i].start_addr >> FRAME_WIDTH) + 
		pmem[i].nframes;
	    maxf = MAX(maxf, lastf);
	}
    }

    /* We return the max index into the table... */
    *maxpfn = maxf; 
    /* ... and the total size required for it. */
    return (maxf * sizeof(RamTable_t));
}



/* 
** Compute how much space is required initially for page tables: 
** We do this currently by cycling through all initial mappings
** and setting a bit in a 1024-bit bitmap if a particular 4Mb 
** chunk of the virtual address space requires a L2 page table. 
** We then add a fixed number to this to allow for L2 allocation 
** before we get frames and stretch allocators. 
*/

static word_t mem_reqd(Mem_Map mmap, int *nl2tables)
{
    word_t bitmap[32];
    word_t curva, res, nptabs; 
    int i, j;

    res = sizeof(MMU_st);   /* state includes the level 1 page table */
    for(i = 0; i < 32; i++) 
	bitmap[i] = 0;


    for(i = 0; mmap[i].nframes; i++) {
	MTRC(eprintf("mem_reqd: got memmap [%p,%p) -> [%p,%p), %d frames\n", 
		mmap[i].vaddr, 
		mmap[0].vaddr + (mmap[0].nframes << FRAME_WIDTH),
		mmap[i].paddr, 
		mmap[0].paddr + (mmap[0].nframes << FRAME_WIDTH),
		mmap[i].nframes));
	for(j=0; j < mmap[i].nframes; j++) {
	    curva = (word_t)(mmap[i].vaddr + (j << FRAME_WIDTH));
	    bitmap[BITMAP_IDX(curva)] |= 1 << BITMAP_BIT(curva);
	}
    }

    /* Now scan through the bitmap to determine the number of L2s reqd */
    nptabs = 0;
    for(i=0; i < 32; i++) {
	while(bitmap[i]) {
	    if(bitmap[i] & 1) 
		nptabs++;
	    bitmap[i] >>= 1; 
	}
    }

    MTRC(eprintf("Got ntpabs=%d, extra=%d => total is %d\n", 
	    nptabs, N_EXTRA_L2S, nptabs + N_EXTRA_L2S));
    nptabs    += N_EXTRA_L2S;
    *nl2tables = nptabs; 
    return (res + (nptabs * L2SIZE));
}


INLINE bool_t alloc_l2table(MMU_st *st, word_t *l2va, word_t *l2pa)
{
    int i; 

    for(i = st->nextl2; i < st->maxl2; i++)
	if(st->info[i] == L2FREE) 
	    break;

    if(i == st->maxl2) {
	/* XXX go get some more mem from frames/salloc */
	eprintf("alloc_l2table: out of memory for tables!\n");
	return False;
    }

    st->info[i] = L2USED;
    st->nextl2  = i+1; 

    *l2va = (word_t)st->va_l2 + (L2SIZE * i);
    bzero(*l2va, L2SIZE);
    *l2pa = (word_t)st->pa_l2 + (L2SIZE * i);
    MTRC(eprintf("alloc_l2table: new l2tab at va=%p, pa=%p, shad va=%p\n", 
		    *l2va, *l2pa, SHADOW(*l2va)));
    return True;
}


static bool_t add4k_page(MMU_st *st, word_t va, word_t pte, sid_t sid)
{
    uint32_t l1idx, l2idx;
    word_t   l2va, l2pa;
    swpte_t *swpte;

    l1idx  = L1IDX(va);

    if(!st->ptab[l1idx].ctl.c_valid) {
	MTRC(eprintf("mapping va=%08x requires new l2tab\n", va));
	if(!alloc_l2table(st, &l2va, &l2pa)) {
	    eprintf("intel_mmu:add4k_page - cannot alloc l2 table.\n");
	    return False;
	}
	st->ptab[l1idx].bits = (l2pa & PDE_M_PTA) | PDE_M_VALID | 
	    PDE_M_RW | PDE_M_US | PDE_M_WT;
	st->vtab[l1idx].bits = l2va & PDE_M_PTA;
    }

    if(st->ptab[l1idx].bits & PDE_M_PGSZ) {
	eprintf("URK! mapping va=%08x would use a 4MB page!\n", va);
	return False;      
    }

    l2pa = st->ptab[l1idx].bits & PDE_M_PTA;
    l2va = (word_t)st->va_l2 + (l2pa - (word_t)st->pa_l2);
    /* XXX PARANOIA */
    if(l2va != (st->vtab[l1idx].bits & PDE_M_PTA)) 
	eprintf("VTAB out of sync: l2va=%p, not %p\n", 
		l2va, st->vtab[l1idx].bits & PDE_M_PTA);
    
    /* Ok, once here, we have a pointer to our l2 table in "l2va" */
    l2idx = L2IDX(va);
    
    /* Set pte into real ptab */
    ((word_t *)l2va)[l2idx] = pte;  

    /* Setup shadow pte (holds sid + original global rights) */
    swpte          = (swpte_t *)SHADOW(l2va) + l2idx;
    swpte->sid     = sid;  /* store sid in shadow */
    swpte->ctl.all = ((hwpte_t *)&pte)->bits & 0xFF;
    return True;
}


/*
** update4k_pages is used to modify the information in the 
** page table about a particular contiguous range of pages. 
** It returns the number of pages (from 1 upto npages) 
** successfully updated, or zero on failure. 
*/
static uint32_t update4k_pages(MMU_st *st, word_t va, uint32_t npages, 
			       word_t pte, sid_t sid)
{
    uint32_t l1idx, l2idx;
    word_t   l2va, l2pa;
    swpte_t *swpte;
    hwpte_t *hwpte; 
    uint16_t ctl; 
    int i;

    l1idx  = L1IDX(va);

    if(!st->ptab[l1idx].ctl.c_valid)
	/* Urk! Not valid => cannot update. */
	return 0;

    if(st->ptab[l1idx].bits & PDE_M_PGSZ) {
	eprintf("update4k_pages: va=%08x is mapped using a 4MB page!\n", va);
	return 0;
    }

    l2pa = st->ptab[l1idx].bits & PDE_M_PTA;
    l2va = (word_t)st->va_l2 + (l2pa - (word_t)st->pa_l2);

    /* XXX PARANOIA */
    if(l2va != (st->vtab[l1idx].bits & PDE_M_PTA)) 
	eprintf("VTAB out of sync: l2va=%p, not %p\n", 
		l2va, st->vtab[l1idx].bits & PDE_M_PTA);

    
    /* Ok, once here, we have a pointer to our l2 table in "l2va" */
    l2idx = L2IDX(va);
    

    /* 
    ** We only allow the update of the control bits and sid here, 
    ** *not* of the mapping. Hence we need to isolate the control 
    ** bits of the pte we've been given to update with. 
    ** XXX SMH: really want to have a 'lamba function' we can 
    ** call to generate successive pte's; then can do remap here too. 
    */
    ctl   = pte & 0xFFF;   
    
    for(i = 0; (i < npages) && ((i + l2idx) < N_L2ENTS); i++) {

	/* Update pte in real ptab */
	hwpte = ((hwpte_t *)l2va + (l2idx + i)); 
	hwpte->bits  = (hwpte->bits & PTE_M_PFA) | ctl;

	/* Modify shadow pte (holds sid + original global rights) */
	swpte          = (swpte_t *)SHADOW(l2va) + (l2idx + i);
	swpte->sid     = sid;  /* store sid in shadow */
	swpte->ctl.all = ((hwpte_t *)&pte)->bits & 0xFF;
    }

    return i; 
}


static bool_t free4k_pages(MMU_st *st, word_t va, uint32_t npages)
{
    word_t   l2va, l2pa;
    uint32_t fwidth, state, owner;
    uint32_t l1idx, l2idx, pfn;
    swpte_t *swpte;
    hwpte_t *hwpte; 
    int i;

    l1idx  = L1IDX(va);

    if(!st->ptab[l1idx].ctl.c_valid)
	/* Urk! Not valid => cannot free. */
	return 0;

    if(st->ptab[l1idx].bits & PDE_M_PGSZ) {
	/* XXX SMH: Don't cope with frees of large pages yet. */
	eprintf("URK! freeing va=%08x would use a 4MB page!\n", va);
	return 0;
    }
    
    l2pa = st->ptab[l1idx].bits & PDE_M_PTA;
    l2va = (word_t)st->va_l2 + (l2pa - (word_t)st->pa_l2);

    /* XXX PARANOIA */
    if(l2va != (st->vtab[l1idx].bits & PDE_M_PTA)) 
	eprintf("VTAB out of sync: l2va=%p, not %p\n", 
		l2va, st->vtab[l1idx].bits & PDE_M_PTA);

    /* Ok, once here, we have a pointer to our l2 table in "l2va" */
    l2idx = L2IDX(va);
    

    /* 
    ** We try to free as many PTEs as possible here (i.e. from the 
    ** the position of the first va to the end of this L2 table). 
    ** If the entry is \emph{valid}, then we want to:
    **    - grab its PFN and, if appropriate, check the RamTab
    **    - mark entry as unmapped in the RamTab.
    ** Then we mark the PTE as invalid (and empty) and (maybe) check 
    ** if l2 table now empty and (maybe) recycle.
    ** XXX We don't recycle tables yet, since the way our allocators 
    ** work the mostly likely virtual range to be allocated next is 
    ** precisely the one we're freeing. 
    */

    for(i = 0; (i < npages) && ((i + l2idx) < N_L2ENTS); i++) {

	hwpte = ((hwpte_t *)l2va + (l2idx + i)); 

	if(hwpte->bits & PTE_M_VALID) {

	    pfn  = PFN_PTE(hwpte->bits);

	    if(pfn < st->maxpfn) {
		/* Sanity check the RamtTab */
		owner = RamTab$Get(&st->rtcl, pfn, &fwidth, &state);
		
		if(owner == OWNER_NONE) {
		    eprintf("free4k_pages: PFN %x (addr %p) not owned.\n", 
			    pfn, pfn << fwidth);
		    ntsc_dbgstop();
		    return 0;
		}

		if(state == RamTab_State_Nailed) {
		    eprintf("free4k_pages: PFN %x (addr %p) is nailed.\n", 
			    pfn, pfn << fwidth);
		    ntsc_dbgstop();
		    return 0;
		}

		/* Update the pfn in the RamTab as ummapped. */
		RamTab$Put(&st->rtcl, pfn, owner, fwidth, 0);
	    }
	}

	/* Update pte in real ptab */
	hwpte->bits  = 0; 

	/* Modify shadow pte (holds sid + original global rights) */
	swpte          = (swpte_t *)SHADOW(l2va) + (l2idx + i);
	swpte->sid     = SID_NULL;
	swpte->ctl.all = 0;
    }

    return i; 
}


static bool_t add4m_page(MMU_st *st, word_t va, word_t pte, sid_t sid)
{
    uint32_t l1idx;
    hwpde_t *hwpde; 
    swpte_t *swpte;

    l1idx = L1IDX(va);
    hwpde = (hwpde_t *)&st->ptab[l1idx];
    swpte = (swpte_t *)SHADOW(hwpde);
    TRC(eprintf("add4m_page: va = %p, pte = %lx, sid = %x\n", va, pte, sid));

    if(hwpde->ctl.c_valid) {
	eprintf("add4m_page: entry already in ptab for va=%p (idx %x)\n", 
		va, l1idx);
	ntsc_dbgstop();
	return False;
    }

    hwpde->bits    = pte | PDE_M_PGSZ;

    /* Setup shadow pte (holds sid + original global rights) */
    swpte->sid     = sid; 
    swpte->ctl.all = ((hwpte_t *)&pte)->bits & 0xFF;

    return True;
}

static uint32_t update4m_pages(MMU_st *st, word_t va, uint32_t npages, 
			       word_t pte, sid_t sid)
{
    uint32_t l1idx;
    hwpde_t *hwpde; 
    swpte_t *swpte;
    uint16_t ctl; 
    int i;

    l1idx = L1IDX(va);
    hwpde = (hwpde_t *)&st->ptab[l1idx];
    swpte = (swpte_t *)SHADOW(hwpde);

    if(!hwpde->ctl.c_valid) {
	eprintf("update4m_page: no entry in ptab for va=%p (idx %x)\n", 
		va, l1idx);
	ntsc_dbgstop();
	return False;
    }

    if(!hwpde->ctl.c_ps) {
	eprintf("update4m_page: not a 4M page for va=%p (idx %x)\n", 
		va, l1idx);
	ntsc_dbgstop();
	return False;
    }


    /* 
    ** We only allow the update of the control bits and sid here, 
    ** *not* of the mapping. Hence we need to isolate the control 
    ** bits of the pte we've been given to update with. 
    ** XXX SMH: really want to have a 'lamba function' we can 
    ** call to generate successive pte's; then can do remap here too. 
    */
    ctl   = pte & 0xFFF;   
    
    for(i = 0; (i < npages) && ((i + l1idx) < N_L1ENTS); i++) {

	/* Update pte in real ptab */
	hwpde[i].bits  = (hwpde[i].bits & PDE_M_PTA) | ctl;

	/* Modify shadow pte (holds sid + original global rights) */
	swpte[i].sid     = sid;  /* store sid in shadow */
	swpte[i].ctl.all = ((hwpte_t *)&pte)->bits & 0xFF;
    }

    return i; 
}

static bool_t free4m_pages(MMU_st *st, word_t va, uint32_t npages)
{
    eprintf("free4m_pages: not implemented.\n");
    return False;
}

static bool_t add_page(MMU_st *st, uint16_t pwidth, word_t va, 
		       word_t pte, sid_t sid)
{
    bool_t res = False; 

    switch(pwidth) {
    case WIDTH_4K:
	res = add4k_page(st, va, pte, sid);
	break;

    case WIDTH_4MB:
	res = add4m_page(st, va, pte, sid);
	break;

    default:
	eprintf("add_page: unsupported page width %d\n", pwidth);
	break;
    }

    return res; 
}

static uint32_t update_pages(MMU_st *st, uint16_t pwidth, word_t va, 
			     uint32_t npages, word_t pte, sid_t sid)
{
    uint32_t res = 0; 

    switch(pwidth) {
    case WIDTH_4K:
	res = update4k_pages(st, va, npages, pte, sid);
	break;

    case WIDTH_4MB:
	res = update4m_pages(st, va, npages, pte, sid);
	break;

    default:
	eprintf("update_page: unsupported page width %d\n", pwidth);
	break;
    }

    return res; 
}

static bool_t free_pages(MMU_st *st, uint16_t pw, word_t va, uint32_t npages)
{
    bool_t res = False; 

    switch(pw) {
    case WIDTH_4K:
	res = free4k_pages(st, va, npages);
	break;

    case WIDTH_4MB:
	res = free4m_pages(st, va, npages);
	break;

    default:
	eprintf("free_pages: unsupported page width %d\n", pw);
	break;
    }

    return res; 
}


/*
** Before we switch over to our own page table, we need to ensure that
** we have all the mappings already established in our table too. 
*/
static void enter_mappings(MMU_st *const st, Mem_PMem pmem, Mem_Map mmap)
{
    uint32_t curnf, pte;
    word_t   curva, curpa;
    word_t   startpa, lastpa; 
    int      i, j; 

    startpa = (word_t)st->pa_l1; 
    lastpa  = (word_t)st->pa_l2 + (st->maxl2 * L2SIZE);
    MTRC(eprintf("enter_mappings: start/end pa for ptabs are %p,%p\n", 
	    startpa, lastpa));

    for(i=0; mmap[i].nframes; i++) {

	curnf = mmap[i].nframes; 
	curva = (word_t)mmap[i].vaddr;
	curpa = (word_t)mmap[i].paddr;

	while(curnf) {


	    pte   = (curpa & PDE_M_PTA) | PDE_M_VALID;
	    pte  |= PDE_M_RW | PDE_M_US; 

	    /* We assume the frames used by the established mappings 
	       are part of the image unless otherwise specified */

	    /* 
	    ** Generally we want to cache things, but in the case 
	    ** of IO space we prefer not to.
	    */
	    for(j=0; pmem[j].nframes ; j++) {
		if(pmem[j].attr && (IN_REGION(pmem[j], curpa))) {
		    MTRC(eprintf("Disabling cache for va %x\n", curva));
		    pte  |= PDE_M_CD;
		    break; 
		}
	    }

	    /* We lock the L1 page table into the TLB */
	    if(st->use_pge && (curva == &(st->ptab))) 
		pte |= PTE_M_GBL;

	    if(!add4k_page(st, curva, pte, SID_NULL)) {
		eprintf("enter_mappings: failed to add mapping %p->%p\n", 
			curva, curpa);
		ntsc_dbgstop();
	    }

	    RamTab$Put(&st->rtcl, (curpa >> FRAME_WIDTH), OWNER_SYSTEM,
		       FRAME_WIDTH, RamTab_State_Mapped);

	    curva += FRAME_SIZE;
	    curpa += FRAME_SIZE;
	    curnf--;
	}
    }

    MTRC(eprintf("Enter mappings: required total of %d new l2 tables.\n", 
	    st->nextl2));
}




/*--------------------------------------------- MMUMod Entry Points ----*/

/*
 * Initalise the "MMU" datastructures in physical memory 
 */
static MMU_clp New_m(MMUMod_cl *self, Mem_PMem pmem, Mem_Map init, 
		     word_t size, RamTab_clp *ramtab, addr_t *free)
{
    MMU_st *st; 
    int i, nl2tables;
    addr_t st_va, st_pa;
    word_t rtsz, maxpfn; 
    word_t req; 
    
    req  = mem_reqd(init, &nl2tables);
    req  = ROUNDUP(req, FRAME_WIDTH);
    rtsz = rtab_reqd(pmem, &maxpfn);
    req += rtsz; 

    MTRC(eprintf("MMUMod$New: require %d bytes for %d page tables + state\n", 
		 req, nl2tables));
    MTRC(eprintf("         => require %d bytes total.\n", req + size));
    
    /* 
    ** Now 'alloc' some memory at the end of the image. We know that the 
    ** memmap is currently 1-to-1, and hence simply look for space of 
    ** at least (req+size) bytes somewhere 'low'. 
    ** XXX SMH: Should use the physical memory map to ensure we 
    **          allocate a valid and memory-like region.
    */
    for(i=0; init[i].nframes; i++) 
	;

    if(i) {
	init[i].vaddr   = init[i-1].vaddr + 
	    (init[i-1].nframes << init[i-1].frame_width);
    } else init[i].vaddr  = 0;
    init[i].paddr       = init[i].vaddr; 
    init[i].nframes     = (req+size + FRAME_SIZE-1) >> FRAME_WIDTH;
    init[i].mode        = 0;  /* XXX put something plausible here */ 
    init[i].frame_width = FRAME_WIDTH;
    TRC(eprintf("MMUMod$New got %d bytes at (va) %p\n", req + size, 
		init[i].vaddr));

    st_va = init[i].vaddr; 
    st_pa = init[i].paddr; 

    init[i+1].nframes   = 0;

    st = (MMU_st *)st_va;
    TRC(eprintf("MMUMod: got state at va=%p, pa=%p\n", st_va, st_pa));

    /* address of start of our state is the level1 page table */
    st->va_l1   = st_va;
    st->pa_l1   = st_pa;
    st->vtab_va = &(st->vtab);
    TRC(eprintf("MMUMod: got l1 ptable at va=%p, pa=%p, vtable at va=%p\n", 
		st->va_l1, st->pa_l1, st->vtab_va));

    /* Initialise the ptab to fault everything, & vtab to 'no trans' */
    for(i=0; i < N_L1ENTS; i++) {
	st->ptab[i].bits = 0;
	st->vtab[i].bits = 0;
	st->stab[i].sid      = SID_NULL;
	st->stab[i].ctl.all  = 0; 
    }


    /* Initialise the ram table; it follows the state record immediately. */
    st->maxpfn = maxpfn;
    st->ramtab = (RamTable_t *)ROUNDUP((st + 1), FRAME_WIDTH);
    bzero(st->ramtab, rtsz);
    TRC(eprintf("MMUMod: got ramtab at %p [0x%x entries]\n", 
	    st->ramtab, st->maxpfn));

    /* Setup the ramtable return parameter */
    CL_INIT(st->rtcl, &rtab_ms, st);
    *ramtab = &st->rtcl;

    /* Initialise the protection domain tables */
    st->next_pdidx = 0; 
    for(i = 0; i < PDIDX_MAX; i++) {
	st->pdom_tbl[i] = NULL;
	st->pdominfo[i].refcnt  = 0;
	st->pdominfo[i].gen     = 0;
	st->pdominfo[i].stretch = (Stretch_cl *)NULL;
    }

    /* And store a pointer to the pdom_tbl in the info page */
    INFO_PAGE.pd_tab = &(st->pdom_tbl); 

    /* XXX SMH: we can only use the 'GBL' bit if it is enabled - even 
       if we've not enabled it in CR4 (seems to cause reboot if we 
       use 'em - believe me, it's very very weird indeed).  */
    st->use_pge = (INFO_PAGE.processor.features & PROCESSOR_FEATURE_PGE); 

    /* Intialise our closures, etc to NULL for now */
    st->frames    = NULL;
    st->heap      = NULL;
    st->sysalloc  = NULL;

    st->va_l2  = (addr_t)ROUNDUP(((char *)st->ramtab + rtsz), FRAME_WIDTH);
    st->pa_l2  = st_pa + ((void *)st->va_l2 - (void *)st);
    st->maxl2  = nl2tables; 
    TRC(eprintf("MMUMod: got %d l2 tables at va=%p, pa=%p\n", 
	    st->maxl2, st->va_l2, st->pa_l2));

    st->nextl2 = 0;
    for(i=0; i < st->maxl2; i++)
	st->info[i] = L2FREE; 

    /* Enter mappings for all the existing translations */
    enter_mappings(st, pmem, init);

    /* Swap over to our new page table! */
    MTRC(eprintf("MMUMod: setting new ptbr to va=%p, pa=%p\n", 
	    st->va_l1, st->pa_l1));
    ntsc_wptbr(st->va_l1, st->pa_l1, st->vtab_va);
    MTRC(eprintf("+++ done new ptbr.\n"));

    /* And store some useful pointers in the PIP for user-level translation */
    INFO_PAGE.l1_va  = st->va_l1; 
    INFO_PAGE.l2tab  = st->vtab_va; 
    INFO_PAGE.mmu_ok = True; 

    /* Sort out pointer to free space for caller */
    *free = (addr_t)ROUNDUP(((void *)st + req), FRAME_WIDTH);
    TRC(eprintf("MMUMod: got free space at va=%p, pa=%p\n", 
	    *free, st_pa + (*free - st_va)));


    CL_INIT(st->cl, &mmu_ms, st);
    MTRC(eprintf("MMUMod: returning closure at %p\n", &st->cl));
    return &st->cl;
}



static void Done_m(MMUMod_cl *self, MMU_cl *mmu, Frames_cl *frames, 
		   Heap_cl *heap, StretchAllocator_cl *salloc)
{
    MMU_st *st = mmu->st;

    /* We don't require much here; just squirrel away the closures */
    st->frames   = frames;
    st->heap     = heap;
    st->sysalloc = salloc;

    return;
}


/*------------------------------------------------ MMU Entry Points ----*/

static void Engage_m(MMU_cl *self, ProtectionDomain_ID pdid)
{
    MMU_st   *st = self->st;
    pdom_t   *base; 
    uint16_t  idx = PDIDX(pdid); 

    if( (idx >= PDIDX_MAX) || ((base = st->pdom_tbl[idx]) == NULL) ) {
	eprintf("MMU$Engage: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
    }
    
    MTRC(eprintf("MMU$Engage: pdid is %x => base is %p\n", pdid, base));
    ntsc_flushtlb();
    ntsc_wrpdom(base);
    return;
}


INLINE uint16_t control_bits(MMU_st *st, Stretch_Rights rights, bool_t valid)
{
    uint16_t ctl = 0;

    if(valid) ctl |= PTE_M_VALID;

#ifdef CONFIG_NOPROT    
    ctl |= PTE_M_RW | PTE_M_US;  /* in NOPROT, everyone gets read and write */
#else
    if( SET_IN(rights, Stretch_Right_Read) ||    /* execute == read on Intel */
	SET_IN(rights, Stretch_Right_Execute))
	ctl |= PTE_M_US;
    
    if (SET_IN(rights, Stretch_Right_Write)) 
	ctl |= PTE_M_RW | PTE_M_US;              /* Write implies read */
#endif

    if(st->use_pge && (SET_IN(rights, Stretch_Right_Global)))
	ctl |= PTE_M_GBL;                       

    return ctl;
}


static void AddRange_m(MMU_cl *self, Stretch_cl *str, 
		       const Mem_VMemDesc *range, 
		       Stretch_Rights global)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = str->st; 
    uint32_t    curnp, pte, psize;
    word_t      curva;
    uint16_t    ctl, pwidth;

    curnp  = range->npages; 
    curva  = (word_t)range->start_addr; 
    ctl    = control_bits(st, global, False);
    pte    = (uint32_t)ctl; 
    pwidth = range->page_width; 
    if(!VALID_WIDTH(pwidth)) {
	eprintf("MMU$AddRange: unsupported page width %d\n", pwidth);
	return; 
    }

    psize = 1UL << pwidth; 

    while(curnp--) {
	if(!add_page(st, pwidth, curva, pte, sst->sid)) {
	    eprintf("MMU$AddRange: failed to add entry for va %08x\n", 
		    curva);
	    return; 
	}
	curva += psize; 
    }

    TRC(eprintf("MMU$AddRange: added va range [%p-%p), sid=%04x\n", 
		range->start_addr, 
		range->start_addr + (range->npages << pwidth), 
		sst->sid));
    return;
}

static void AddMappedRange_m(MMU_cl *self, Stretch_cl *str, 
			     const Mem_VMemDesc *range, 
			     const Mem_PMemDesc *pmem, 
			     Stretch_Rights gaxs)
{
    MMU_st     *st = self->st;
    Stretch_st *sst = str->st; 
    uint32_t    curnf, curnp, pte, pfn, psize;
    uint32_t    fwidth, state, owner = OWNER_NONE;
    word_t      curva, curpa;
    uint16_t    ctl, pwidth;

    /* Check have valid page/frame widths (aka 4Kb or 4Mb) */
    pwidth = range->page_width;
    if(!VALID_WIDTH(pwidth)) {
	eprintf("MMU$AddMappedRange: unsupported page width %d\n", pwidth);
	return; 
    }

    fwidth = pmem->frame_width;
    if(!VALID_WIDTH(fwidth)) {
	eprintf("MMU$AddMappedRange: unsupported frame width %d\n", fwidth);
	return; 
    }

    /* If have differing page/frame widths, need to try to homogenise */
    if(fwidth > pwidth) {
	TRC(eprintf("MMU$AddMappedRange: fw = %d > %d = pw.\n", 
		    fwidth, pwidth));
	pwidth = fwidth; 
	TRC(eprintf("range->npages is 0x%lx; pmem->nframes is 0x%lx\n", 
		    range->npages, pmem->nframes));
	curnp  = (range->npages << range->page_width) >> pwidth;
	curnf  = pmem->nframes;
    } else if (pwidth > fwidth) {
	curnp  = range->npages; 
	fwidth = pwidth; 
	curnf  = (pmem->nframes << pmem->frame_width) >> fwidth;
    } else {
	curnf  = pmem->nframes;
	curnp  = range->npages; 
    }

    if(curnf != curnp) {
	eprintf("MMU$AddMappedRange: have %d frames != %d pages\n", 
		curnf, curnp);
	ntsc_dbgstop();
	return;
    }

    curva = (word_t)range->start_addr; 
    curpa = (word_t)pmem->start_addr; 
    ctl   = control_bits(st, gaxs, True);
    /* 
    ** We use the combination of the bits 'NoCache' and 'NonMemory' to 
    ** determine our caching policy as follows: 
    **     if NoCache is set, then set the CD bit. 
    **     if NonMemory is set, but *not* NoCache, then set the WT bit. 
    ** This is a bit vile since the 'Attr' things are all a bit hotch
    ** potch. Essentially it just means that things like, e.g. frame 
    ** buffers, are updated correctly as long as their pmem attrs 
    ** include NonMemory (but not NoCache). 
    */
    if(SET_IN(pmem->attr, Mem_Attr_NoCache)) 
	ctl |= PDE_M_CD; 
    else if(SET_IN(pmem->attr, Mem_Attr_NonMemory))
	ctl |= PDE_M_WT;

    psize = 1UL << pwidth; 


    while(curnp--) {

	pfn    = PFN(curpa);
	pte    = (curpa & PDE_M_PTA) | ctl;

	if(pfn < st->maxpfn) {

	    /* Sanity check the RamtTab */
	    owner = RamTab$Get(&st->rtcl, pfn, &fwidth, &state);

	    if(owner == OWNER_NONE) {
		eprintf("MMU$AddMappedRange: physical addr %p not owned.\n",
			curpa);
		ntsc_dbgstop();
	    }

	    if(state == RamTab_State_Nailed) {
		eprintf("MMU$AddMappedRange: physical addr %p is nailed.\n",
			curpa);
		ntsc_dbgstop();
	    }

	}

	if(!add_page(st, pwidth, curva, pte, sst->sid))
	    eprintf("MMU$AddMappedRange: failed to add entry for va %08x\n", 
		    curva);

	if(pfn < st->maxpfn) {
	    /* Update the RamTab */
	    RamTab$Put(&st->rtcl, pfn, owner, fwidth, RamTab_State_Mapped);
	}

	curva += psize; 
	curpa += psize; 
    }

    TRC(eprintf("MMU$AddRange: added va range [%p-%p) => [%p-%p), sid=%04x\n", 
	    range->start_addr, 
	    range->start_addr + (range->npages << PAGE_WIDTH),
	    pmem->start_addr, 
	    pmem->start_addr + (pmem->nframes << FRAME_WIDTH), 
	    sst->sid));
    return;
}

/*
** Note: update cannot currently modify mappings, and expects
** that the virtual range contains valid PFNs already. 
*/
static void UpdateRange_m(MMU_cl *self, Stretch_cl *str, 
			  const Mem_VMemDesc *range, 
			  Stretch_Rights gaxs)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = str->st; 
    uint32_t    curnp, updated, psize, pte;
    uint16_t    ctl, pwidth;
    word_t      curva;

    curnp  = range->npages; 
    curva  = (word_t)range->start_addr; 
    ctl    = control_bits(st, gaxs, True);
    pte    = (uint32_t)ctl; 

    pwidth = range->page_width; 
    if(!VALID_WIDTH(pwidth)) {
	eprintf("MMU$UpdateRange: unsupported page width %d\n", pwidth);
	return; 
    }

    psize = 1UL << pwidth; 

    while(curnp) {

	updated = update_pages(st, pwidth, curva, curnp, pte, sst->sid);
	if(!updated) {
	    eprintf("MMU$UpdateRange: failed to do all entries for "
		    "virtual range [%08x-%08x]\n", curva, 
		    curva + (range->npages * psize));
	    eprintf("curva was %p, curnp was 0x%x\n", curva, curnp);
	    ntsc_halt();
	    return; 
	}
	
	curva += (updated * psize);
	curnp -= updated;
    }
    
    TRC(eprintf("MMU$UpdateRange: done.\n"));
    return;
}

static void FreeRange_m(MMU_cl *self, const Mem_VMemDesc *range)
{
    MMU_st *st = self->st;
    uint32_t curnp, freed, psize;
    uint16_t pwidth;
    word_t   curva;

    curnp = range->npages; 
    psize = ((word_t)1 << range->page_width);
    curva = (word_t)range->start_addr; 

    pwidth = range->page_width; 
    if(!VALID_WIDTH(pwidth)) {
	eprintf("MMU$UpdateRange: unsupported page width %d\n", pwidth);
	return; 
    }

    psize = 1UL << pwidth; 
    TRC(eprintf("MMU$FreeRange: %d pages starting at %p\n", curnp, curva));

    while(curnp) {

	freed = free_pages(st, pwidth, curva, curnp);
	if(!freed) {
	    eprintf("MMU$FreeRange: failed to free all entries for "
		    "virtual range [%08x-%08x]\n", curva, 
		    curva + (range->npages * psize));
	    eprintf("curva was %p, curnp was 0x%x\n", curva, curnp);
	    ntsc_halt();
	    return; 
	}
	
	curva += (freed * psize);
	curnp -= freed;
    }

    return;
}


/*
** Create a new protection domain. 
*/
static ProtectionDomain_ID NewDomain_m(MMU_cl *self)
{
    MMU_st *st = self->st;
    ProtectionDomain_ID res; 
    Stretch_Size sz;
    uint16_t idx; 
    pdom_t  *base; 

    TRC(eprintf("MMU$NewDomain: called.\n"));
    idx = alloc_pdidx(st); 

    /* Intialise our 'state' */
    st->pdominfo[idx].refcnt  = 0; 
    st->pdominfo[idx].stretch = StretchAllocator$New(st->sysalloc, 
						     sizeof(pdom_t), 
						     Stretch_Rights_None); 
    base  = (pdom_t *)Stretch$Range(st->pdominfo[idx].stretch, &sz);
    bzero(base, sizeof(pdom_t)); 

    /* Add the new pdom to the pdom table */
    st->pdom_tbl[idx] = base; 

    /* Construct the pdid from the generation and the index */
    res  = ((word_t)(++st->pdominfo[idx].gen)) << 16;
    res |= idx; 

    /* And return it */
    TRC(eprintf("MMU$NewDomain: returning %x\n", res));
    return res;
}


static void IncDomain_m(MMU_cl *self, ProtectionDomain_ID pdid)
{
    MMU_st  *st   = self->st;
    uint32_t idx = PDIDX(pdid); 

    if((idx >= PDIDX_MAX) || (st->pdom_tbl[idx] == NULL)) {
	eprintf("MMU$IncDomain: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
    }
    
    st->pdominfo[idx].refcnt++;
    return;
}



static void FreeDomain_m(MMU_cl *self, ProtectionDomain_ID pdid)
{
    MMU_st  *st  = self->st;
    uint32_t idx = PDIDX(pdid); 
    
    if((idx >= PDIDX_MAX) || (st->pdom_tbl[idx] == NULL)) {
	eprintf("MMU$FreeDomain: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
    }
    
    if(st->pdominfo[idx].refcnt) st->pdominfo[idx].refcnt--;
    
    if(st->pdominfo[idx].refcnt == 0) {
	StretchAllocator$DestroyStretch(st->sysalloc, 
					st->pdominfo[idx].stretch); 
	st->pdom_tbl[idx] = NULL;
	st->next_pdidx    = idx; 
    }

    return;
}

static void SetProt_m(MMU_cl *self, ProtectionDomain_ID pdid,
		      Stretch_cl *str, Stretch_Rights axs)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = (Stretch_st *)str->st;
    sid_t       sid = sst->sid; 
    uint32_t    idx = PDIDX(pdid); 
    pdom_t     *p; 
    uchar_t     mask;

    if( (idx >= PDIDX_MAX) || ((p = st->pdom_tbl[idx]) == NULL) ) {
	eprintf("MMU$SetProt: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
	return;
    }


    TRC(eprintf("MMU$SetProt(%p, %04x, [%c%c%c%c])\n", 
	    p, sid, 
	    SET_IN(axs,Stretch_Right_Meta)    ? 'M' : '-',
	    SET_IN(axs,Stretch_Right_Execute) ? 'E' : '-',
	    SET_IN(axs,Stretch_Right_Write)   ? 'W' : '-',
	    SET_IN(axs,Stretch_Right_Read)    ? 'R' : '-'));

    mask = sid & 1 ? 0xF0 : 0x0F;
    if (sid & 1) axs <<= 4;
    p->rights[sid>>1] &= ~mask;
    p->rights[sid>>1] |= axs;
    
    /* Want to invalidate all non-global TB entries, but we can't
    /* do that on Intel so just blow away the whole thing. */
#ifndef CONFIG_NOPROT
    ntsc_flushtlb();
#endif
}

/*
** Return the rights in pdom 'p' for the stretch identified by 's'
*/
static Stretch_Rights QueryProt_m(MMU_cl *self, ProtectionDomain_ID pdid,
				  Stretch_cl *str)
{
    MMU_st   *st  = self->st;
    uint32_t  idx = PDIDX(pdid); 
    pdom_t   *p; 
    sid_t     s;
    uchar_t   mask;
    Stretch_Rights res;

    if( (idx >= PDIDX_MAX) || ((p = st->pdom_tbl[idx]) == NULL) ) {
	eprintf("MMU$SetProt: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
	return Stretch_Rights_None;
    }
    
    s = ((Stretch_st *)(str->st))->sid; 
    
    mask = s & 1 ? 0xF0 : 0x0F;
    res  = p->rights[s>>1] & mask;
    if (s & 1) res >>= 4;
    
    return res;
}

static word_t QueryASN_m(MMU_cl *self, ProtectionDomain_ID pd) 
{
    /* No ASN on pcs */
    return 0x666; 
}

static Stretch_Rights QueryGlobal_m(MMU_cl *self, Stretch_clp str)
{
    MMU_st        *st  = self->st;
    Stretch_st    *sst = str->st; 
    uint32_t       l1idx, l2idx; 
    word_t         l2va, l2pa, bits;
    Stretch_Rights res; 

    res = Stretch_Rights_None; 

    TRC(eprintf("QueryGlobal called, str is %p\n", str));
    if(!sst->s) return res; 
	
    /* Lookup the first page of the stretch */
    l1idx = L1IDX(sst->a);
    if(!(st->ptab[l1idx].bits & PDE_M_VALID)) {
	eprintf("MMU$QueryGlobal: invalid str %p\n", str);
	return res; 
    }

    if(st->ptab[l1idx].bits & PDE_M_PGSZ) { /* Got a 4M page */

	bits = st->ptab[l1idx].bits & 0xFF; 
	if(bits & PDE_M_US) {
	    if(bits & PDE_M_RW) { 
		res |= SET_ELEM(Stretch_Right_Read) 
		    |  SET_ELEM(Stretch_Right_Write);
	    } else {
		res |= SET_ELEM(Stretch_Right_Read) 
		    |  SET_ELEM(Stretch_Right_Execute);
	    }
	}

    } else {                               /* Got a 4k page */

	l2idx = L2IDX(sst->a);
	l2pa  = st->ptab[l1idx].bits & PDE_M_PTA;
	l2va  = (word_t)st->va_l2 + (l2pa - (word_t)st->pa_l2);
	bits  = ((word_t *)l2va)[l2idx] & 0xFFF;
	if(bits & PTE_M_US) {
	    if(bits & PTE_M_RW) { 
		res |= SET_ELEM(Stretch_Right_Read) 
		    |  SET_ELEM(Stretch_Right_Write);
	    } else {
		res |= SET_ELEM(Stretch_Right_Read) 
		    |  SET_ELEM(Stretch_Right_Execute);
	    }
	    if(bits & PTE_M_GBL) res |= SET_ELEM(Stretch_Right_Global);
	}
    }

    TRC(eprintf("QueryGlobal: returning %x\n", res));
    return res; 
}

static void CloneProt_m(MMU_cl *self, Stretch_clp str, Stretch_clp tmpl)
{
    MMU_st *st = self->st;
    pdom_t *curp; 
    sid_t   sid, tsid;
    uchar_t mask, tmask;
    word_t  idx;
    Stretch_Rights axs;
    
    TRC(eprintf("CloneProt called, str is %p, templ is %p\n", str, tmpl));

    /* Get the sid & template sid, and their masks */
    sid   =  ((Stretch_st *)(str->st))->sid; 
    mask  = sid & 1 ? 0xF0 : 0x0F;
    tsid  = ((Stretch_st *)(tmpl->st))->sid; 
    tmask = tsid & 1 ? 0xF0 : 0x0F;

    /* Iterate through all pdoms, copying rights across */
    for(idx = 0; idx < PDIDX_MAX; idx++) {

	if( (curp = st->pdom_tbl[idx]) != NULL) {
	    
	    /* First get the template rights in the current pdom */
	    axs  = curp->rights[tsid>>1] & tmask;
	    if (tsid & 1) axs >>= 4;
	    
	    /* And now set those rights for the new sid */
	    if(sid & 1) axs <<= 4; 
	    curp->rights[sid>>1] &= ~mask;
	    curp->rights[sid>>1] |= axs;
	    
	}
    }

    return;
}

/*--------------------------------------------- RamTab Entry Points ----*/

static uint32_t RamTabSize_m(RamTab_cl *self) 
{
    MMU_st *st   = self->st;

    return st->maxpfn;
}


static addr_t RamTabBase_m(RamTab_cl *self) 
{
    MMU_st *st   = self->st;

    return (addr_t)st->ramtab; 
}


static void RamTabPut_m(RamTab_cl *self, uint32_t pfn, uint32_t owner,
			uint32_t fwidth, RamTab_State state)
{
    MMU_st     *st   = self->st;
    RamTable_t *rtab = st->ramtab; 

    if(pfn >= st->maxpfn) {
	eprintf("RamTab$Put: got out of range pfn %x (max is %x)\n", pfn);
	ntsc_dbgstop();
	return;
    }

    rtab[pfn].owner  = owner; 
    rtab[pfn].fwidth = fwidth; 
    rtab[pfn].state  = state; 
    return;
}

static uint32_t RamTabGet_m(RamTab_cl *self, uint32_t pfn, 
			    uint32_t *fwidth, RamTab_State *state)
{
    MMU_st     *st   = self->st;
    RamTable_t *rtab = st->ramtab; 

    if(pfn >= st->maxpfn) {
	eprintf("RamTab$Get: got out of range pfn %x (max is %x)\n", pfn);
	ntsc_dbgstop();
	return 0xABBADEAD;
    }

    *fwidth = rtab[pfn].fwidth;
    *state  = rtab[pfn].state; 

    return rtab[pfn].owner; 
}
