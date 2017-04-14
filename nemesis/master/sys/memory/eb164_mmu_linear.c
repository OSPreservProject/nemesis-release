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
**      sys/memory
**
** FUNCTIONAL DESCRIPTION:
** 
**      This is the C part of the alpha linear page table implementation. 
**      This works only with memsys EXPT, and is not quite complete as 
**      yet (some functions don't work, but basic ones are ok). 
** 
**
*/

#include <nemesis.h>
#include <bstring.h>
#include <stdio.h>
#include <frames.h>
#include <exceptions.h>
#include <VPMacros.h>

#include <pip.h>

#include <RamTab.ih>
#include <MMUMod.ih>
#include <ProtectionDomain.ih>
#include <StretchAllocatorF.ih>


#include "mmu.h"

// #define DEBUG
#ifdef DEBUG
#define TRC(x) x
#define INITTRC(_x) _x
#else 
#define TRC(x)
#define INITTRC(_x)     /* trace initialisation code                     */
#endif

#define HTRC(_x)        /* trace heuristics                              */
#define STRC(_x)        /* trace splitting of guards                     */
#define XTRC(_x)        /* xtra tracing; everything else (very verbose!) */

#undef  DUMP_TABLES     /* dump all page tables on init. Very verbose.   */
#undef  TRACE_SYSALLOC  /* turns on STRC/XTRC for 'special' region only  */
#define SANITY_CHECK    /* lots of sanity checks; leave in for now       */
#undef  XTRA_CHECK      /* even more checking of translations; disable   */


#ifdef TRACE_SYSALLOC
#undef STRC(_x)
#define STRC(_x) 						\
{ 								\
    if(((word_t)va & 0x12300000000UL) == (0x12300000000UL))	\
	_x; 							\
}

#undef XTRC(_x)
#define XTRC(_x) 						\
{ 								\
    if(((word_t)va & 0x12300000000UL) == (0x12300000000UL))	\
	_x; 							\
}
#endif


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


/* For raw access to page table memory */
#define SUPERPAGE(_a) \
    (addr_t)((((word_t)_a)&0x3FFFFFFFFUL)|(0xFFFFFCUL<<40))

#define IN_REGION(_pmem, _pa) (						      \
  ((_pa) >= (word_t)(_pmem).start_addr) &&				      \
  ((_pa) < (word_t)(_pmem).start_addr+((_pmem).nframes<<(_pmem).frame_width)) \
    )

/* Useful macros for sizey size */
#define Kb(_b) ((unsigned long)(_b) << 10)
#define Mb(_b) ((unsigned long)(_b) << 20)

#define VPN(_vaddr) ((word_t)(_vaddr) >> PAGE_WIDTH)

/* Information about protection domains */
typedef struct _pdom_st {
    uint16_t      refcnt;  /* reference count on this pdom    */
    uint16_t      asn;     /* ASN associated with this pdom   */
    uint32_t      gen;     /* Current generation of this pdom */
    Stretch_clp   stretch; /* handle on stretch (for destroy) */ 
} pdom_st; 


/*
** We keep track of free chunks of page table memory in a type of 
** linked list (as defined below). 
** Each 'chunk' can hold up to 64 pages; if it holds less, we simply
** pre-allocate however many pages from the end. 
*/

typedef struct _ptmem {
    word_t         bitmap; /* Bitmap of free ptabs within the 'chunk' */
    addr_t         base;   /* Virt/Phys address of this chunk (121) */
    struct _ptmem *next;   /* Pointer to next structure, or NULL    */
} ptmem_t;

#define PTMEM_FULL     ((word_t)-1)
#define PTMEM_SIZE     WORD_LEN



/* We have an 8 Gbyte array of PTEs which we index into with the vpn. 
   Should probably be configurable, but for now, we simply define 
   it to start at 0x10.0000.0000ul */
#define PTAB_BASE 0x1000000000ul
#define PTAB_SIZE  0x200000000ul


/*
** We allocate a level 1 table with N_L1ENTS GPTEs within our state. 
*/
#define N_L1ENTS     (1024)
#define LOG2_NL1ENTS (10) 

typedef struct _mmu_st {

    /* First part of our state holds the level 1 system page table */
    struct hwpte         l1tab[N_L1ENTS];      /* Aligned to page boundary */

    bool_t               enabled;              /* If have mapped linear ptab */

    word_t               next_pdidx;           /* Next free pdom idx (hint)  */
    word_t               next_asn;             /* Next free ASN to issue     */
    pdom_t              *pdom_vtbl[PDIDX_MAX]; /* Map pdidx to pdom_t (va)   */
    pdom_t              *pdom_ptbl[PDIDX_MAX]; /* Map pdidx to pdom_t (pa)   */
    pdom_st              pdominfo[PDIDX_MAX];  /* Maps pdom IDs to pdom_st's */

    /* List of page table memory in 'chunks': head [here] holds info */
    ptmem_t              ptmem; 

    /* And now the `standard' state fields */
    MMU_cl               cl;
    RamTab_cl            rtcl; 

    Frames_cl           *frames;           
    Heap_cl             *heap;
    StretchAllocator_cl *sysalloc;         /* 'special' salloc [nailed]      */

    word_t               psptbr;           /* physical sptab base register   */
    word_t               vsptbr;           /* virtual version of above       */

    hwpte_t             *lptbr;            /* 'linear' page table base [virt]*/

    RamTable_t          *ramtab;           /* base of ram table              */
    word_t               maxpfn;           /* size of above table            */

} MMU_st;

#undef NULL_PFN
#define NULL_PFN 0xfadedbee

/* 
** SET_PTE(): macro for setting/updating a pte in a (leaf) GPTE.
** If we have a valid new PFN, then we set/update protection and
** translation (PFN) in the old pte.
** Otherwise we just set/update protection.
*/
#define SET_PTE(_old,_new) {			\
    if((_new).pfn != NULL_PFN)			\
	(_old)  = (_new);			\
    else {					\
	(_old).sid     = (_new).sid;		\
	(_old).ctl.all = (_new).ctl.all;	\
    }						\
}



/*
** -------- Utility Calls --------------------------------------------- **
*/


INLINE uint32_t alloc_pdidx(MMU_st *st)
{
    int i;

    i    = st->next_pdidx; 
    do {
	if(st->pdom_vtbl[i] == NULL) {
	    st->next_pdidx = (i + 1) % PDIDX_MAX;
	    return i; 
	}
	i = (i + 1) % PDIDX_MAX;
    } while(i != st->next_pdidx); 

    eprintf("alloc_pdidx: out of identifiers!\n");
    ntsc_dbgstop();
    return 0xdead;
}


/*
** -------- Page Table Alloc & Free ------------------------------------- **
*/


/*
** pt_alloc: gets an frame of physical memory for a page table
** (either system stuff, or the virtual page table). 
*/
static word_t pt_alloc(MMU_st *st) 
{
    word_t   i, j; 
    ptmem_t *cur;
    hwpte_t *tmp; 

    for(cur = &st->ptmem; cur; cur = cur->next) {
	if(cur->bitmap != PTMEM_FULL) {

	    /* Ok: some free mem in current ptmem */

	    for(i = 0; i < PTMEM_SIZE; i++) {
		if(!(cur->bitmap & (1 << i))) 
		    break;
	    }
	    cur->bitmap |= (1 << i); 
	    tmp = (hwpte_t *)(cur->base + (i << PAGE_WIDTH)); 
	    for(j = 0; j < 1024; j++) {
		tmp[j].sid      = SID_NULL;
		tmp[j].ctl.all  = PTE_M_FOE|PTE_M_FOW|PTE_M_FOR;
		tmp[j].pfn      = NULL_PFN;
	    }
	    return (word_t)tmp; 
	} 
    }
    eprintf("pt_alloc: out of memory. halting system.\n");
    ntsc_dbgstop();
    return 0xded;
}


static void pt_free(MMU_st *st, word_t start)
{
    eprintf("pt_free: unimplemented.\n");
    ntsc_dbgstop();
    return;

}

#define L1IDX(_va) (((word_t)(_va) >> 23) & 0x3FF)
#define L2IDX(_va) (((word_t)(_va) >> 13) & 0x3FF)



static hwpte_t *lookup_va(MMU_st *st, word_t va) 
{
    hwpte_t *l1, *l2, *lpte; 
    word_t   lpte_va, offset; 

    lpte = &(st->lptbr[VPN(va)]);
	
    if(!st->enabled) {
	/* Need to 'translate' lpte internally via system ptab */
	lpte_va = (word_t)lpte - PTAB_BASE; 
    
	/* Get its L1 idx & its L2 one */
	l1 = &(st->l1tab[L1IDX(lpte_va)]); 
	if(!l1->ctl.flags.c_valid) {
	    /* L1 not valid => TNV! */
	    eprintf("lookup_va (va=%p): L1 lpte entry [%lx] not valid!\n", 
		    va, *((word_t *)l1)); 
	    return NULL;
	}
	l2 = ((hwpte_t *)((word_t)l1->pfn << FRAME_WIDTH)) + L2IDX(lpte_va); 
	if(!l2->ctl.flags.c_valid) {
	    eprintf("lookup_va (va=%p): L2 lpte entry [%lx] not valid!\n", 
		    va, *((word_t *)l2)); 
	    /* L2 not valid => TNV! */
	    return NULL;
	}

	offset = (lpte_va & 0x1FFF) >> 3;
	lpte   = &((hwpte_t *)((word_t)l2->pfn << FRAME_WIDTH))[offset];
    } 

    return lpte; 
}



static bool_t add8k_pages(MMU_st *st, word_t va, hwpte_t *newpte, uint32_t n);

/*
** Ensure that there is a L2 system page table to map the LPTE 
** at "lpte_va"; return the *system* address of the L2 PTE for 
** "lpte_va" --- this address is both a virtual and a physical 
** address [i.e. it is mapped 1-2-1]). 
*/
static hwpte_t *ensure_l2(MMU_st *st, word_t lpte_va) 
{
    hwpte_t *l1, *l2;
    bool_t  res = False;

    if(lpte_va < PTAB_BASE || lpte_va >= (PTAB_BASE + PTAB_SIZE)) {
	eprintf("ensure_l2: got bogus starting lpte va %p.\n", lpte_va); 
	ntsc_dbgstop();
	return False;
    }
    
    /* Normalise it w.r.t. base (this allows us to use a 2-level lookup) */
    lpte_va = lpte_va - PTAB_BASE;

    /* Get its L1 idx & its L2 one */
    l1 = &(st->l1tab[L1IDX(lpte_va)]); 
    if(!l1->ctl.flags.c_valid) {

	TRC(eprintf("ensure_l2: L1 not valid => need to allocate a L2: \n"));
	if(!(l2 = (hwpte_t *)pt_alloc(st))) {
	    eprintf("Failed to allocate L2!\n");
	    ntsc_dbgstop();
	    return False;
	}

	TRC(eprintf(" + got new L2 at [phys] %p\n", l2)); 
	l1->pfn     = ((word_t)l2 >> FRAME_WIDTH);
	l1->sid     = SID_NULL;
	l1->ctl.all =  PTE_M_KWE | PTE_M_KRE | PTE_M_URE | 
	    PTE_M_ASM | PTE_M_VALID; 

	res = True; 
    }

    l2 = ((hwpte_t *)((word_t)l1->pfn << FRAME_WIDTH)) + L2IDX(lpte_va); 
    return l2;
}

/*
** Arrange that there is 'space' in the linear page table 
** for '*n' hwptes mapping the virtual page "vpn". 
** Return the virtual address for accessing the linear page table, 
** the *physical* address which can be used to access it [if we
** have yet to engage the ptab], and the number of ptes for which 
** the physical address is viable [since may not be contiguous]. 
** 
** Note: currently we only ever allocate one L2 table, and one
**       page for the linear page table, at a time --- multiple 
**       calls are required if we want to handle more ptes. 
*/
hwpte_t *ensure_lptes(MMU_st *st, word_t vpn, /* IN OUT */ uint32_t *n, 
		      /* OUT */ hwpte_t **res_pa) 
{
    hwpte_t *l2, *lpte_pa, *lpte_va;
    word_t   offset; 

    /* The virtual address of the LPTE for the va is simply arrived 
       at by indexing its vpn off the LPtbr */
    lpte_va = &st->lptbr[vpn]; 
    offset  = ((word_t)lpte_va & (PAGE_SIZE-1)) >> 3; 

    /* We may not be able to access the LPT directly yet, so only 
       do first LPTE */
    l2 = ensure_l2(st, (word_t)lpte_va); 
    if(!l2->ctl.flags.c_valid) {

	/* Need to allocate a new page of page table, and map it 
	   via the l2, and also via lpte_va */
	
	if(!(lpte_pa = (hwpte_t *)pt_alloc(st))) {
	    eprintf("Failed to allocate LPTE page!\n");
	    ntsc_dbgstop();
	    return False;
	}
	
	l2->pfn     = ((word_t)lpte_pa >> FRAME_WIDTH);
	l2->sid     = SID_NULL;
	l2->ctl.all =  PTE_M_KWE | PTE_M_KRE | PTE_M_URE | 
	    PTE_M_ASM | PTE_M_VALID; 
	
	if(!add8k_pages(st, (word_t)lpte_va, l2, 1)) {
	    eprintf("Recursive add failed!\n");
	    ntsc_dbgstop();
	}
    }
    
    /* Ok - we have a page of LPTEs => sorted */
    *res_pa = ((hwpte_t *)((word_t)l2->pfn << FRAME_WIDTH)) + offset; 
    
    /* Ok: we've sorted out LPTEs for our vpn; setup *n and return */
    *n = MIN(*n, (1024 - offset));   /* nasty constant */
    return lpte_va; 
}

static void do_add8k(MMU_st *st, hwpte_t *lpte, hwpte_t *newpte, 
		     word_t start_vpn, uint32_t nvpns)
{
    hwpte_t *chk_pte;
    int i; 

    for (i = 0; i < nvpns; i++) {

	if(newpte->ctl.all & PTE_M_ASM) {
	    if(!(newpte->ctl.all & PTE_M_VALID)) {
		eprintf("do_add8k: adding invalid+global va=%p!!!\n",
			(start_vpn + i) << PAGE_WIDTH);
		ntsc_dbgstop();
	    }
	}
	
	lpte->pfn     = newpte->pfn;
	lpte->sid     = newpte->sid;
	lpte->ctl.all = newpte->ctl.all | PTE_M_KRE | PTE_M_KWE;
	lpte++;
	
	chk_pte = lookup_va(st, ((start_vpn+i) << PAGE_WIDTH));
	if(!chk_pte) {
	    eprintf("chk_pte is %lx: newpte->pfn is %x\n", chk_pte, 
		    newpte->pfn); 
	    ntsc_dbgstop();
	}

	if(chk_pte->pfn != newpte->pfn) {
	    eprintf("chk_pte->pfn is %x: newpte->pfn is %x\n", 
		    chk_pte->pfn, newpte->pfn); 
	    ntsc_dbgstop();
	}

	/* Invalidate Both TB entries for this VA */
	ntsc_tbi(3, (addr_t)((start_vpn+i) << PAGE_WIDTH));
    }
}


static bool_t add8k_pages(MMU_st *st, word_t va, hwpte_t *newpte, uint32_t n)
{
    hwpte_t  *cur_va, *cur_pa; 
    word_t    vpn, pte; 
    uint32_t  curn;

    STRC(eprintf("add8k_pages: %d starting at va %lx.\n", n, va));

    /* Need to determine if the base LPTE is already mapped by our 
       system page tables; if not, need to allocate a new page of 
       LPTE (and perhaps an L2 system entry to map it). */

    cur_va = NULL;
    cur_pa = NULL;

    vpn    = VPN(va); 
    curn   = n; 

    while(n) {
	
	cur_va = ensure_lptes(st, vpn, &curn, &cur_pa); 
	if(cur_va == NULL) {
	    eprintf("add8k_pages: failed to allocate lptes [va=%p, n=%lx].\n",
		    va, n);
	    ntsc_dbgstop();
	}

	/* Currently ensure lptes only does at most one LPTE page
	   at each go, so may have to iterate. Also, at start-of-day
	   the actual lpte's themselves may not be contiguous 
	   virtually. Hence iterate through 'curn' of 'em each time. */
	
	STRC(eprintf("OK: enough of linear ptab[at %p] to do %d of %d ptes\n", 
		     cur_va, curn, n)); 
	
	do_add8k(st, cur_pa, newpte, vpn, curn);

	pte = lookup_va(st, (vpn << PAGE_WIDTH));
	STRC(eprintf("After add; lookup of va %p gives pte %lx\n", 
		     (vpn << PAGE_WIDTH), pte));
	vpn += curn; 
	n   -= curn; 
	curn = n; 
    }

    return True;
}





static bool_t update8k_pages(MMU_st *st, word_t va, 
			     hwpte_t *newpte, uint32_t n)
{
    hwpte_t  *cur; 
    word_t    vpn;
    int i;

    vpn  = VPN(va);
    cur  = &(st->lptbr[vpn]);

    /* XXX Should check that all region is mapped, but for now, ignored */
    
    if(!st->enabled) {
	eprintf("update8k_pages: %d LPTEs starting at %p\n", n, cur); 
	eprintf("Case where MMU not enabled not supported!\n");
	ntsc_dbgstop();
    }

    for (i = 0; i < n; i++) {
	
	cur->sid     = newpte->sid;
	cur->ctl.all = newpte->ctl.all | PTE_M_KRE | PTE_M_KWE;
	cur++;
	
	/* Invalidate Both TB entries for this VA */
	ntsc_tbi(3, (addr_t)((vpn+i) << PAGE_WIDTH));
    }

    return i; 
}

static bool_t free8k_pages(MMU_st *st, word_t va, uint32_t n)
{
    uint32_t curpfn, fw, state, owner = OWNER_NONE; 
    hwpte_t *cur;
    word_t   vpn; 
    int i;

    vpn  = VPN(va);
    cur  = &(st->lptbr[vpn]);

    /* XXX Should check that all region is mapped, but for now, ignored */
    
    if(!st->enabled) {
	eprintf("free8k_pages: %d LPTEs starting at %p\n", n, cur); 
	eprintf("Case where MMU not enabled not supported!\n");
	ntsc_dbgstop();
    }

    for (i = 0; i < n; i++) {

	if(cur->ctl.flags.c_valid) {

	    curpfn = cur->pfn; 

	    if(curpfn < st->maxpfn) {

		/* Sanity check the RamTab */
		owner = RamTab$Get(&st->rtcl, curpfn, &fw, &state);
		
		if(owner == OWNER_NONE) {
		    eprintf("free8k_pages:: PFN %x (addr %p) not owned.\n",
			    curpfn, curpfn << FRAME_WIDTH);
		    ntsc_dbgstop();
		    return False;	
		}
		
		if(state == RamTab_State_Nailed) {
		    eprintf("free8k_pages:: PFN %x (addr %p) is nailed.\n",
			    curpfn, curpfn << FRAME_WIDTH);
		    ntsc_dbgstop();
		    return False;
		}
		
		/* Update the pfn in the RamTab as ummapped. */
		RamTab$Put(&st->rtcl, curpfn, owner, fw, 0);
	    }
	}

	cur->pfn     = 0xDEADBEEF;
	cur->sid     = SID_NULL;
	cur->ctl.all = 0;
	
	cur++;

	/* Invalidate Both TB entries for this VA */
	ntsc_tbi(3, (addr_t)((vpn+i) << PAGE_WIDTH));
    }
    
    return True;
}


/*
** We support 8Mb 'superpages' via special L2 entries in the system 
** page table; when the linear lookup misses (as it will), we fake
** out the relevant PTE in the double miss handler and increment 
** past the virtual load. 
** 
** Hence here we need to: 
**   a) ensure there is a L2 system page table installed. 
**   b) add entries for each 8Mb page in it. 
** 
** For simplicity, we don't yet handle adding > 1024 8Mb pages at 
** a time [or, more generally, any number of pages which would 
** require more than one L2 page table]. 
*/
static bool_t add8Mb_pages(MMU_st *st, word_t va, hwpte_t *newpte, uint32_t n)
{
    word_t   lpte_va, offset;
    hwpte_t *l1, *l2;
    int i; 

    /* Need to 'translate' lpte internally via system ptab */
    lpte_va = (word_t )(&(st->lptbr[VPN(va)])) - PTAB_BASE;

    /* Get its L1 idx & lookup the L1 PTE */
    l1 = &(st->l1tab[L1IDX(lpte_va)]); 
    if(!l1->ctl.flags.c_valid) {
	/* L1 not valid => need to allocate a new L2 */
	TRC(eprintf("add8Mb_pages (va=%p): L1 lpte entry [%lx] not valid\n", 
		    va, *((word_t *)l1))); 

	if(!(l2 = (hwpte_t *)pt_alloc(st))) {
	    eprintf("Failed to allocate L2!\n");
	    ntsc_dbgstop();
	}

	l1->pfn     = ((word_t)l2 >> FRAME_WIDTH);
	l1->sid     = SID_NULL;
	l1->ctl.all =  PTE_M_KWE | PTE_M_KRE | PTE_M_URE | 
	    PTE_M_ASM | PTE_M_VALID; 
    }

    l2 = ((hwpte_t *)((word_t)l1->pfn << FRAME_WIDTH)) + L2IDX(lpte_va); 

    offset = (lpte_va & 0x1FFF) >> 3; 
    if((offset + n) > 1024) {
	eprintf("add8Mb_pages: too many pages to add!\n"); 
	ntsc_dbgstop();
    }

    for(i = 0; i < n; i++) {
	l2[offset+i].pfn     = newpte->pfn; 
	l2[offset+i].sid     = newpte->sid; 
	l2[offset+i].ctl.all = newpte->ctl.all | PTE_M_NLD;
    }

    return True;
}



/*
** We support 8Gb 'superpages' via special L1 entries in the system 
** page table; when the linear lookup misses (as it will), we fake
** out the relevant PTE in the double miss handler and increment 
** past the virtual load. 
** 
** For simplicity, we only handle adding at most 1 of these at a 
** time (and since multiple of these are unlikely to be useful). 
*/
static bool_t add8Gb_pages(MMU_st *st, word_t va, hwpte_t *newpte, uint32_t n)
{
    word_t   lpte_va;
    hwpte_t *l1;

    if(n > 1) {
	eprintf("Cannot allocate more than 1 8Gb page at a time ;-(\n");
	ntsc_dbgstop();
    }

    /* Need to 'translate' lpte internally via system ptab */
    lpte_va = (word_t )(&(st->lptbr[VPN(va)])) - PTAB_BASE;

    /* Get its L1 idx & lookup the L1 PTE */
    l1 = &(st->l1tab[L1IDX(lpte_va)]); 
    if(l1->ctl.flags.c_valid) {
	/* L1 already valid => cannot add 8Gbytes here! */
	eprintf("add8Gb_pages (va=%p): L1 lpte entry [%lx] already valid!!\n", 
		va, *((word_t *)l1)); 
	ntsc_dbgstop();
    }

    l1->pfn     = newpte->pfn; 
    l1->sid     = newpte->sid; 
    l1->ctl.all = newpte->ctl.all | PTE_M_NLD;

    return True;

}

static bool_t add_pages(MMU_st *st, uint16_t pwidth, word_t va, 
			uint32_t npages, hwpte_t *pte)
{
    bool_t res = False; 

    switch(pwidth) {
    case WIDTH_8K:
	res = add8k_pages(st, va, pte, npages);
	break;
	
    case WIDTH_8MB:
	res = add8Mb_pages(st, va, pte, npages);
	break;

    case WIDTH_8GB:
	res = add8Gb_pages(st, va, pte, npages);
	break;
	
    default:
	eprintf("add_pages: unsupported page width %d\n", pwidth);
	break;
    }
    
    return res; 
}

static uint32_t update_pages(MMU_st *st, uint16_t pwidth, word_t va, 
			     uint32_t npages, hwpte_t *pte)
{
    uint32_t res = 0; 

    switch(pwidth) {
    case WIDTH_8K:
	res = update8k_pages(st, va, pte, npages);
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
    case WIDTH_8K:
	res = free8k_pages(st, va, npages);
	break;

    default:
	eprintf("free_pages: unsupported page width %d\n", pw);
	break;
    }

    return res; 
}



/*
** -------- Initialisation Stuff ---------------------------------------- **
*/

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
** Compute how much space is required initially for page tables.
*/

static word_t ptmem_reqd(Mem_Map mmap)
{
    word_t res; 
    int i;


    for(i = 0; mmap[i].nframes; i++) {
	INITTRC(eprintf("ptmem_reqd: got memmap [%08x,%08x) -> [%08x,%08x), "
			"%d frames\n", 
			mmap[i].vaddr, mmap[i].vaddr + 
			(mmap[i].nframes << mmap[i].frame_width), 
			mmap[i].paddr, mmap[i].paddr + 
			(mmap[i].nframes << mmap[i].frame_width), 
			mmap[i].nframes));
    }
    
    /* XXX SMH: for now, hardwire the number of initial page tables to 32 */
    res = (PAGE_SIZE * 32);
    return res;
}
    

/*
** Initialise the page table memory; since we have not yet allocated
** any page tables (apart from the L1 in our state), all of our pages
** go onto the free list for now.
*/
void init_ptabs(MMU_st *st, void *ptbase, word_t ptsize)
{
    int i, npages;
    word_t free, bit; 

    /* First off, bzero the entire piece of memory */
    for(i= 0; i < (ptsize / sizeof(word_t)); i++)
	((word_t *)SUPERPAGE(ptbase))[i]= (word_t)0; 

    INITTRC(eprintf("Zeroed page table memory at %lx\n", ptbase));

    /* Now mark all the ptabs as free */
    /* We maintain a linked list of 'chunks' of page table memory:
       each 'chunk' is up to 64 page tables (contiguously), and 
       we use a bitmap to note whether they are free or not. */
    npages = ptsize >> PAGE_WIDTH; 
    if(npages > PTMEM_SIZE) {
	eprintf("init_ptabs: only handle up to %d pages of page table.\n", 
		PTMEM_SIZE);
	ntsc_dbgstop();
    }

    st->ptmem.base   = ptbase; 
    st->ptmem.next   = NULL;

    free = (PTMEM_SIZE - npages); 
    bit  = 1UL << (PTMEM_SIZE - 1); 
    while(free) {
	st->ptmem.bitmap |= bit; 
	bit >>= 1UL; 
	free--; 
    }

    /* And that's it */
    return;
}



/*
** Before we switch over to our own page table, we need to ensure that
** we have all the mappings already established in our table too. 
*/
static void enter_mappings(MMU_st *const st, Mem_PMem pmem, Mem_Map mmap)
{
    word_t   curva, curpa, fsize, psize;
    uint32_t curnf;
    uint16_t gh, fwidth;
    hwpte_t  newpte;
    int      i; 

    for(i=0; mmap[i].nframes; i++) {

	curnf = mmap[i].nframes; 
	curva = (word_t)mmap[i].vaddr;
	curpa = (word_t)mmap[i].paddr;

	newpte.ctl.all  = PTE_M_KWE | PTE_M_KRE | PTE_M_ASM | PTE_M_VALID; 
	newpte.sid      = SID_NULL;   /* since no stretches yet */
	fwidth          = mmap[i].frame_width;

	switch(fwidth) {
	case WIDTH_8GB:
	    gh    = (3 << PTE_V_GH);
	    psize = 1U<<fwidth;
	    /* XXX SMH: temp HACK. Give global read/write on io space */
 	    eprintf("HACK [eb164]: global read/write for IO region %d\n", i);
	    newpte.ctl.all |= (PTE_M_UWE | PTE_M_URE); 
	    break;

	case WIDTH_8MB:
	    gh    = (3 << PTE_V_GH);
	    psize = 1U<<fwidth;
	    /* XXX SMH: temp HACK. Give global read/write on io space */
 	    eprintf("HACK [eb164]: global read/write for IO region %d\n", i);
	    newpte.ctl.all |= (PTE_M_UWE | PTE_M_URE); 
	    break;

	case WIDTH_4MB:
	    gh    = (3 << PTE_V_GH);
	    psize = Mb(4);
	    break;
		
	case WIDTH_512K:
	    gh    = (2 << PTE_V_GH);
	    psize = Kb(512);
	    break;
	    
	case WIDTH_64K:
	    gh    = (1 << PTE_V_GH);
	    psize = Kb(64);
	    break;
	    
	case WIDTH_8K:
	    gh    = 0;
	    psize = Kb(8);
	    break;
	    
	default:
 	    eprintf("enter mappings: reg %d using bad frame width %d\n", 
		    i, fwidth);
	    ntsc_halt();
	}
	
	newpte.ctl.all |= gh; 
	fsize = psize; 
	    
	while(curnf) {
	    
	    newpte.pfn = curpa >> FRAME_WIDTH;

	    if(!add_pages(st, fwidth, curva, 1, &newpte)) {
		eprintf("enter_mappings: failed to add pages to ptab?!?\n");
		ntsc_halt();
	    }


	    curva += psize; 
	    curpa += fsize; 
	    curnf--;
	}
    }

}


/* 
** alloc_state() allocates some space at the end of the image. 
** We've not even got a FramesMod yet, so need to be a bit 
** cunning; essentially we look for a physical memory region 
** in the physical memory descriptors which, according to the 
** init memory map, has enough space remaining. 
** We then update the initial memory map to extend the final 
** used piece to include the memory we're now allocating.
*/
static word_t alloc_state(Mem_PMem pmem, Mem_Map init, word_t size)
{
    int i, j, the_reg = -1; 
    word_t last_paddr, first_free, last_used; 

    /* First find a physical memory region */
    for(i=0; pmem[i].nframes; i++) {

	if(!(pmem[i].attr & Mem_Attr_NonMemory)) {

	    /* Keep track of the last address in this physical region   */
	    last_paddr = (word_t)pmem[i].start_addr + 
		(pmem[i].nframes << pmem[i].frame_width);

	    /* And keep track of the first free address in this region  */
	    first_free = (word_t)pmem[i].start_addr;

	    /* Now check the initial memmap for mem in this region */
	    for(j=0; init[j].nframes; j++) {

		/* Get the last used address in this entry */
		last_used = (word_t)init[j].paddr + 
		    (init[j].nframes << init[j].frame_width);

		/* Is it in our candidate physical region ? */
		if(IN_REGION(pmem[i], last_used) && last_used > first_free) {
		    first_free = last_used; 
		    the_reg = j;
		}
	    }

	    /* Ok, now check if there's enough slack at the end */
	    if(last_paddr - first_free >= size) {
		/* Patch up region info */
		init[the_reg].nframes += size >> init[the_reg].frame_width; 
		return first_free; 
	    }
	} 
    }
    eprintf("alloc_state: could not allocate %d bytes of real memory.", size);
    eprintf("Fatal: halting system\n");
    ntsc_halt();

}  


/*--------------------------------------------- MMUMod Entry Points ----*/


/*
 * Initalise the "MMU" datastructures in physical memory 
 */
static MMU_clp New_m(MMUMod_cl *self, Mem_PMem pmem, Mem_Map init, 
		     word_t size, RamTab_clp *ramtab, addr_t *free)
{
    MMU_st     *st;
    word_t      ptsz, rtsz, maxpfn, stsz, totsz; 
    word_t      rtbase, st_va, st_pa; 
    addr_t      vpdtbl_va, ppdtbl_pa; 
    int         i;

    size = ROUNDUP(size, FRAME_WIDTH);
    INITTRC(eprintf("MMUMod$New called, size is %x.\n", size));

    rtsz = rtab_reqd(pmem, &maxpfn);
    stsz = ROUNDUP(sizeof(*st), FRAME_WIDTH);
    ptsz = ptmem_reqd(init);
    ptsz = ROUNDUP(ptsz, FRAME_WIDTH);
    INITTRC(eprintf("MMUMod$New: require %x bytes for ramtab, %x bytes for "
		    "state and %x for ptabs\n", rtsz, stsz, ptsz));
    totsz = ROUNDUP((rtsz + stsz + ptsz + size), FRAME_WIDTH);
    INITTRC(eprintf("         => require %x bytes total.\n", totsz));

    /* The ramtab comes first in our chunk of memory */
    rtbase  = alloc_state(pmem, init, totsz);
    bzero((char *)rtbase, rtsz);

    INITTRC(eprintf("MMUMod$New: got ramtab at [%p, %p)\n", 
		    rtbase, rtbase + rtsz));
    
    /* The state record (such as it is) follows immediate afterwards. */
    st_va   = rtbase + rtsz; 
    st_pa   = st_va;          /* 121 mapping of mmu state, etc.  */

    INITTRC(eprintf("MMUMod$New got %d bytes at (va) %p\n", totsz, 
		    st_va));

    st = (MMU_st *)st_va;
    INITTRC(eprintf("MMUMod: got state at va=%p, pa=%p\n", st_va, st_pa));

    bzero(st, stsz);

    /* Intialise the ram table state & setup the return parameter */ 
    st->maxpfn = maxpfn;
    st->ramtab = (RamTable_t *)rtbase; 
    CL_INIT(st->rtcl, &rtab_ms, st);
    *ramtab = &st->rtcl;

    /* 
    ** Initialise the protection domain tables &c; we have three of these, 
    ** indexed by protection domain id (pdid): 
    ** 
    **  - pdom_vtbl: this maps pdid to the virtual address of the base 
    **               of the protection domain's sid->rights array.
    **               Used by us, and (via PIP) by any user-level code.
    ** 
    **  - pdom_ptbl: this maps pdid to the physical address of the base 
    **               of the protection domain's sid->rights array.
    **               Used by PALcode via the k_st.pdom_tbl pointer. 
    **
    **  - pdominfo : maps pdid to misc other protection domain info (viz. 
    **               ASN, ref count & stretch)
    */
    st->next_pdidx = 0; 
    st->next_asn   = 0; 
    for(i = 0; i < PDIDX_MAX; i++) {
	st->pdom_vtbl[i]        = NULL;  
	st->pdom_ptbl[i]        = NULL;
	st->pdominfo[i].refcnt  = 0;
	st->pdominfo[i].asn     = 0;
	st->pdominfo[i].gen     = 0;
	st->pdominfo[i].stretch = (Stretch_cl *)NULL;
    }

    /* Get the va of the pdom_vtbl, and the pa of the pdom_ptbl */
    vpdtbl_va = &(st->pdom_vtbl[0]); 
    ppdtbl_pa = &(st->pdom_ptbl[0]);  /* XXX pa == va at this point */

    /* store va of pdom_vtbl in PIP for user-level query, etc. */
    INFO_PAGE.pd_tab = vpdtbl_va; 

    /* We have a level one page table which is accessed *physically* 
       by PALcode; */

    /* Sort out the level one page table; it contains N_L1ENTS entries */
    for(i=0; i < N_L1ENTS; i++) {
	st->l1tab[i].sid      = SID_NULL;
	st->l1tab[i].ctl.all  = PTE_M_FOE|PTE_M_FOW|PTE_M_FOR;
	st->l1tab[i].pfn      = NULL_PFN;
    }

    /* We can't yet access the linear page tables directly (in VAS) */
    st->enabled = False; 

    /* Setup the phys & virt pointers to the level one page table */
    st->psptbr  = (word_t)&(st->l1tab[0]);
    st->vsptbr  = st->psptbr;  /* XXX pa == va at this point */
  
    INITTRC(eprintf("Setting up virtual linear page table at %qx\n", 
		    PTAB_BASE)); 
    st->lptbr    = (hwpte_t *)PTAB_BASE; 

    /* 
    ** Initialise the page table memory (which is in pages).
    ** It follows immediately after the state record.
    */
    init_ptabs(st, (word_t *)ROUNDUP((st + 1), FRAME_WIDTH), ptsz);


    /* Add mappings for the existing memory covered. */
    INITTRC(eprintf("Entering mappings for existing memory.\n"));
    enter_mappings(st, pmem, init);

    /* Swap over to our new page table; we also pass the pa of the pdom_ptbl */
    INITTRC(eprintf("MMUMod: setting new sptbr to va=%p, pa=%p\n", 
		    st->vsptbr, st->psptbr));
    INITTRC(eprintf("     +  setting new [im]vptbr to va=%p\n", 
		    st->lptbr));
    ntsc_wrptbr((addr_t)st->psptbr, (addr_t)st->lptbr, ppdtbl_pa);
    ntsc_tbi(-2, 0);
    INITTRC(eprintf("+++ done new ptbr.\n"));

    /* Set ourselves as 'enabled' => can now access the LPT directly. */
    INITTRC(eprintf("+++ setting st->enabled to 'True'\n")); 
    st->enabled = True;

    /* Sort out pointer to free space for caller */
    *free = (addr_t)ROUNDUP(((void *)st + stsz + ptsz), FRAME_WIDTH);
    INITTRC(eprintf("MMUMod: got free space at va=[%p-%p)\n", 
		    *free, (word_t)*free+size));


    CL_INIT(st->cl, &mmu_ms, st);
    INITTRC(eprintf("MMUMod: returning closure at %p\n", &st->cl));
    return &st->cl;
}
 


static void Done_m(MMUMod_cl *self, MMU_cl *mmu, Frames_cl *frames, 
		   Heap_cl *heap, StretchAllocator_cl *salloc)
{
    MMU_st *st = mmu->st;

    TRC(eprintf("MMUMod$Done: frames=%p, heap=%p, salloc=%p\n", frames, 
		heap, salloc));

    /* We don't require much here; just squirrel away the closures */
    st->frames   = frames;
    st->heap     = heap;
    st->sysalloc = salloc;

    return;
}


/*------------------------------------------------ MMU Entry Points ----*/

static void Engage_m(MMU_cl *self, ProtectionDomain_ID pdid)
{
    MMU_st  *st = self->st;
    pdom_t  *pbase; 
    word_t   asn; 
    uint32_t idx = PDIDX(pdid); 
 
    if( (idx >= PDIDX_MAX) || ((pbase = st->pdom_ptbl[idx]) == NULL) ) {
	eprintf("MMU$Engage: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
	return;
    }
    
    asn = st->pdominfo[idx].asn; 
    
    TRC(eprintf("MMU$Engage: pdid is %x => base is %p, asn is %x\n", 
		pdid, pbase, asn));
    
    /* Paranoia */
    if(asn != RO(Pvs(vp))->asn) 
	eprintf("MMU$Engage: warning - pdom asn == %x != %x == VP asn\n", 
		asn, RO(Pvs(vp))->asn); 
    
    ntsc_wrpdom(pbase, asn);    /* set the protection domain */
    ntsc_tbi(-2, 0);            /* invalidate all TB entries */
    return;
}

INLINE uint16_t control_bits(Stretch_Rights rights, bool_t valid)
{
    uint16_t ctl = 0;

    ctl  = PTE_M_KWE | PTE_M_KRE;
    if(valid) ctl |= PTE_M_VALID;

    if(SET_IN(rights, Stretch_Right_Read))
	ctl |= PTE_M_URE;
    if(SET_IN(rights, Stretch_Right_Write))
	ctl |= PTE_M_UWE;
    if(!SET_IN(rights, Stretch_Right_Execute))
	ctl |= PTE_M_FOE;
    if(SET_IN(rights, Stretch_Right_Global))
	ctl |= PTE_M_ASM;

    return ctl;
}

static void AddRange_m(MMU_cl *self, Stretch_cl *str, 
		       const Mem_VMemDesc *range, 
		       Stretch_Rights global)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = str->st; 
    uint32_t    curnp, psize;
    uint16_t    gh, pwidth; 
    hwpte_t     pte; 
    word_t      curva;

    curnp = range->npages; 
    curva = (word_t)range->start_addr; 
    
    pte.sid     = sst->sid;
    pte.pfn     = NULL_PFN;
    pte.ctl.all = control_bits(global, False);

    switch((pwidth = range->page_width)) {
    case WIDTH_8GB:
	gh    = (3 << PTE_V_GH);
	psize = 1U<<pwidth;      
	break;
	
    case WIDTH_4MB:
	gh    = (3 << PTE_V_GH);
	psize = Mb(4);
	break;
	
    case WIDTH_512K:
	gh    = (2 << PTE_V_GH);
	psize = Kb(512);
	break;
	
    case WIDTH_64K:
	gh    = (1 << PTE_V_GH);
	psize = Kb(64);
	break;
	
    case WIDTH_8K:
	gh    = 0;
	psize = Kb(8);
	break;
	
    default:
	eprintf("MMU$AddRange: bad page width %d\n", pwidth);
	ntsc_halt();
    }
    
    pte.ctl.all |= gh; 

    if(!add_pages(st, pwidth, curva, curnp, &pte)) {
	eprintf("MMU$AddRange: failed to add pages to ptab?!?\n");
	ntsc_dbgstop();
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
    uint32_t    fw, state, owner = OWNER_NONE;
    uint32_t    curnf, curnp, pfn, fsize, psize;
    uint16_t    gh, fwidth, pwidth; 
    hwpte_t     pte; 
    word_t      curva, curpa;


    /* Check have valid page/frame widths */
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
	pwidth = fwidth; 
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

    pte.sid     = sst->sid;
    pte.pfn     = 0xFADEDBEE;
    pte.ctl.all = control_bits(gaxs, True);

    psize       = 1UL << pwidth; 

    switch(pwidth) {
    case WIDTH_8GB:
	gh    = (GH_4MB << PTE_V_GH);
	break;
	
    case WIDTH_4MB:
	gh    = (GH_4MB << PTE_V_GH);
	break;
	
    case WIDTH_512K:
	gh    = (GH_512K << PTE_V_GH);
	break;
	
    case WIDTH_64K:
	gh    = (GH_64K << PTE_V_GH);
	break;
	
    case WIDTH_8K:
	gh    = (GH_8K << PTE_V_GH);
	break;
	
    default:
	eprintf("MMU$AddMappedRange: bad page width %d\n", pwidth);
	ntsc_halt();
    }
    
    pte.ctl.all |= gh; 
    fsize  = psize; 

    while(curnp) {

	pte.pfn = pfn = PFN(curpa);

	if(pfn < st->maxpfn) {

	    /* Sanity check the RamtTab */
	    owner = RamTab$Get(&st->rtcl, PFN(curpa), &fw, &state);

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


	if(!add_pages(st, pwidth, curva, 1, &pte)) {
	    eprintf("MMU$AddMappedRange: failed to add pages to ptab?!?\n");
	    ntsc_dbgstop();
	}

	if(pfn < st->maxpfn) {
	    /* Update the RamTab */
	    RamTab$Put(&st->rtcl, PFN(curpa), owner, fw, RamTab_State_Mapped);
	
	}
	curva += psize; 
	curpa += fsize;
	curnp--;
    }
    
    TRC(eprintf("MMU$AddMappedRange: added va range [%p-%p) => [%p-%p), "
		"sid=%04x\n", range->start_addr, range->start_addr + 
		(range->npages << pwidth), pmem->start_addr, 
		pmem->start_addr + (pmem->nframes << fwidth), sst->sid));
    return;
}

static void UpdateRange_m(MMU_cl *self, Stretch_cl *str, 
			  const Mem_VMemDesc *range, 
			  Stretch_Rights gaxs)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = str->st; 
    uint32_t    curnp, psize, updated;
    uint16_t    gh, pwidth; 
    hwpte_t     pte; 
    word_t      curva;

    curnp = range->npages; 
    curva = (word_t)range->start_addr; 

    /* Currently can only update the SID and global bits [i.e. not PFN] */
    pte.sid     = sst->sid;
    pte.ctl.all = control_bits(gaxs, True);

    pwidth = range->page_width;
    if(!VALID_WIDTH(pwidth)) {
	eprintf("MMU$UpdateRange: unsupported page width %d\n", pwidth);
	return; 
    }

    psize = 1UL << pwidth; 

    switch((pwidth = range->page_width)) {
    case WIDTH_8GB:
	gh    = (GH_4MB << PTE_V_GH);
	break;

    case WIDTH_4MB:
	gh    = (GH_4MB << PTE_V_GH);
	break;
	
    case WIDTH_512K:
	gh    = (GH_512K << PTE_V_GH);
	break;
	
    case WIDTH_64K:
	gh    = (GH_64K << PTE_V_GH);
	break;
	
    case WIDTH_8K:
	gh    = (GH_8K << PTE_V_GH);
	break;
	
    default:
	eprintf("MMU$UpdateRange: bad page width %d\n", pwidth);
	ntsc_halt();
    }
    
    pte.ctl.all |= gh; 

    while(curnp) {
	updated = update_pages(st, pwidth, curva, curnp, &pte);
	if(!updated) {
	    eprintf("MMU$UpdateRange: failed to all entries for "
		    "virtual range [%08x-%08x]\n", curva, 
		    curva + (curnp << pwidth));
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

    if(!free_pages(st, range->page_width, range->start_addr, range->npages)) {
	eprintf("FreeRange: free failed!\n");
	ntsc_dbgstop();
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
    uint32_t idx; 
    Stretch_Size sz;
    pdom_t  *base; 

    TRC(eprintf("MMU$NewDomain: called.\n"));
    idx = alloc_pdidx(st); 

    /* Intialise our 'state' */
    st->pdominfo[idx].refcnt  = 0; 
    st->pdominfo[idx].asn     = st->next_asn++;   /* XXX very noddy */
    st->pdominfo[idx].stretch = StretchAllocator$New(st->sysalloc, 
						     sizeof(pdom_t), 
						     Stretch_Rights_None); 
    base  = (pdom_t *)Stretch$Range(st->pdominfo[idx].stretch, &sz);
    bzero(base, sizeof(pdom_t)); 

    /* Add the virt + phys addresses of the pdom base to our pdom tables */
    st->pdom_vtbl[idx] = (pdom_t *)base; 
    st->pdom_ptbl[idx] = (pdom_t *)((word_t)base & 0xFFFFFFFF); /* XXX vtop */ 

    /* Construct the pdid from the generation and the index */
    res   = ++st->pdominfo[idx].gen;
    res <<= 32;
    res  |= idx; 

    /* And return it */
    TRC(eprintf("MMU$NewDomain: returning %lx\n", res));
    return res;
}



static void IncDomain_m(MMU_cl *self, ProtectionDomain_ID pdid)
{
    MMU_st  *st  = self->st;
    uint32_t idx = PDIDX(pdid); 

    if((idx >= PDIDX_MAX) || (st->pdom_vtbl[idx] == NULL)) {
	eprintf("MMU$IncDomain: bogus pdid %lx\n", pdid); 
	ntsc_dbgstop();
    }
    
    st->pdominfo[idx].refcnt++;
    TRC(eprintf("MMU$IncDomain: pdid %lx - refcnt now %x\n", pdid,
		st->pdominfo[idx].refcnt));
    return;
}



static void FreeDomain_m(MMU_cl *self, ProtectionDomain_ID pdid)
{
    MMU_st  *st  = self->st;
    uint32_t idx = PDIDX(pdid); 
    
    TRC(eprintf("MMU$FreeDomain: pdid %lx\n", pdid));
    if((idx >= PDIDX_MAX) || (st->pdom_vtbl[idx] == NULL)) {
	eprintf("MMU$FreeDomain: bogus pdid %lx\n", pdid); 
	ntsc_dbgstop();
    }

    if(st->pdominfo[idx].refcnt) st->pdominfo[idx].refcnt--;

    if(st->pdominfo[idx].refcnt == 0) {
	StretchAllocator$DestroyStretch(st->sysalloc, 
					st->pdominfo[idx].stretch); 
	st->pdom_vtbl[idx] = NULL;
	st->next_pdidx     = idx; 
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

    if( (idx >= PDIDX_MAX) || ((p = st->pdom_vtbl[idx]) == NULL) ) {
	eprintf("MMU$SetProt: bogus pdid %lx\n", pdid); 
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
    
    /* Invalidate all non-global TB entries */
    ntsc_tbi(-1, 0); 
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

    if( (idx >= PDIDX_MAX) || ((p = st->pdom_vtbl[idx]) == NULL) ) {
	eprintf("MMU$SetProt: bogus pdid %lx\n", pdid); 
	ntsc_dbgstop();
	return Stretch_Rights_None;
    }
    
    s = ((Stretch_st *)(str->st))->sid; 
    
    mask = s & 1 ? 0xF0 : 0x0F;
    res  = p->rights[s>>1] & mask;
    if (s & 1) res >>= 4;
    
    return res;
}

/* Return the ASN associated with the protection domain "pd" */
static word_t QueryASN_m(MMU_cl *self, ProtectionDomain_ID pdid) 
{
    MMU_st   *st  = self->st;
    uint32_t  idx = PDIDX(pdid); 

    if( (idx >= PDIDX_MAX) || (st->pdom_vtbl[idx] == NULL) ) {
	eprintf("MMU$SetProt: bogus pdid %lx\n", pdid); 
	ntsc_dbgstop();
    }

    return st->pdominfo[idx].asn; 
}

static Stretch_Rights QueryGlobal_m(MMU_cl *self, Stretch_clp str)
{
    MMU_st        *st  = self->st;
    Stretch_st    *sst = str->st; 
    Stretch_Rights res; 
    hwpte_t       *pte; 
    uint16_t       ctl;

    res = Stretch_Rights_None; 
    if(!sst->s) return res; 

    if(!(pte = lookup_va(st, sst->a))) {
	eprintf("MMU$QueryGlobal: invalid str %p [TNV]\n", str);
	return res;
    }
    
    ctl = pte->ctl.all;
    
    if(ctl & PTE_M_URE)    res |= SET_ELEM(Stretch_Right_Read); 
    if(ctl & PTE_M_UWE)    res |= SET_ELEM(Stretch_Right_Write); 
    if(!(ctl & PTE_M_FOE)) res |= SET_ELEM(Stretch_Right_Execute); 
    if(ctl & PTE_M_ASM)    res |= SET_ELEM(Stretch_Right_Global); 

    TRC(eprintf("MMU$QueryGlobal (str=%p): returning %x\n", str, res));
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
    
    /* Get the sid & template sid, and their masks */
    sid   =  ((Stretch_st *)(str->st))->sid; 
    mask  = sid & 1 ? 0xF0 : 0x0F;
    tsid  = ((Stretch_st *)(tmpl->st))->sid; 
    tmask = tsid & 1 ? 0xF0 : 0x0F;

    /* Iterate through all pdoms, copying rights across */
    for(idx = 0; idx < PDIDX_MAX; idx++) {

	if( (curp = st->pdom_vtbl[idx]) != NULL) {

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
