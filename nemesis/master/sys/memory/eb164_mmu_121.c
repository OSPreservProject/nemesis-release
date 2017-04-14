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
** 
**      This is the alpha specific code to update PALcode datastructures.
**      This incarnation of the MMU (and associated PALcode) is for 
**      MEMSYS_STD => it maps virtual addresses using a linear array.
**      As such, the amount of the virtual address space mapped is 
**      limited [we don't take native mode faults]. 
**      Any addresses higher than MAXVA are simply mapped 121 by the 
**      TLB miss handlers.  
** 
** ENVIRONMENT: 
**
**      This is called while we're still running 121 with no protection.
**      so VA = PA (handy) and we can scribble ANYWHERE!
** 
** ID : 	$Id: eb164_mmu_121.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <ntsc.h>
#include <exceptions.h>
#include <VPMacros.h>
#include <pal/nempal.h>
#include <stdio.h>
#include <pip.h>

#include <RamTab.ih>
#include <MMUMod.ih>
#include <Stretch.ih>

#include "mmu.h"


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




/* Information about protection domains */
typedef struct _pdom_st {
    uint16_t      refcnt;  /* reference count on this pdom    */
    uint16_t      asn;     /* ASN associated with this pdom   */
    uint32_t      gen;     /* Current generation of this pdom */
    Stretch_clp   stretch; /* handle on stretch (for destroy) */ 
} pdom_st; 



#ifdef DEBUG
#define TRC(x) x
#else 
#define TRC(x)
#endif

typedef struct _mmu_st {

    MMU_cl                mmucl;
    RamTab_cl             rtcl; 

    word_t                next_pdidx;          /* Next free pdom idx (hint)  */
    word_t                next_asn;            /* Next free ASN to issue     */
    pdom_t               *pdom_tbl[PDIDX_MAX]; /* Map pdom idx to pdom_t's   */
    pdom_st               pdominfo[PDIDX_MAX]; /* Map pdom idx to pdom_st's  */

    Frames_cl            *frames;           
    Heap_cl              *heap;
    StretchAllocator_cl  *sysalloc;         /* 'special' salloc [nailed]     */

    word_t                pptbr;            /* physical ptab base register   */
    word_t                vptbr;            /* virtual version of above      */
    hwpte_t              *ptbl;             /* synonym for vptbr (free cast) */
    word_t                maxidx;           /* bounds on the linear table    */

    RamTable_t           *ramtab;           /* base of ram table             */
    word_t                maxpfn;           /* size of above table           */

    /* The linear page table actually follows after this state record 
       (at the start of the next page) */

} MMU_st;


#define IN_REGION(_pmem, _pa) (						      \
  ((_pa) >= (word_t)(_pmem).start_addr) &&				      \
  ((_pa) < (word_t)(_pmem).start_addr+((_pmem).nframes<<(_pmem).frame_width)) \
    )


#define Kb(_b) ((_b) << 10)
#define Mb(_b) ((_b) << 20)

#define VPN(_vaddr) ((word_t)(_vaddr) >> PAGE_WIDTH)




static INLINE bool_t subsetof(Stretch_Rights sub, Stretch_Rights super)
{
    if(SET_IN(sub,Stretch_Right_Execute)&&!SET_IN(super,Stretch_Right_Execute))
	return False;
    if(SET_IN(sub,Stretch_Right_Write)&&!SET_IN(super,Stretch_Right_Write))
	return False;
    if(SET_IN(sub,Stretch_Right_Read)&&!SET_IN(super,Stretch_Right_Read))
	return False;
    return True;
}



uint16_t control_bits(Stretch_st *sst, Stretch_Rights rights, bool_t valid)
{
    uint16_t ctl     = 0;

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

    sst->global = rights; 
    return ctl;
}



/* 
 * XXX Note that OSF/1 PALcode uses the 4 modes as follows:
 *
 * U: Unused
 * S: Unused
 * E: User mode 
 * K: Kernel mode
 */
#define U '?'
#define S '?'
#define E 'U'
#define K 'K'

void dump_pte(hwpte_t *pte)
{
  eprintf( "|%08x|%04x|%c%c%c%c|%c%c%c%c|0%d%d%c|%c%c%c%c| (%04x)\n", 
	  pte->pfn,
	  pte->sid,
	  
	  (pte->ctl.flags.c_uwe ? U : '-'),
	  (pte->ctl.flags.c_swe ? S : '-'),
	  (pte->ctl.flags.c_ewe ? E : '-'),
	  (pte->ctl.flags.c_kwe ? K : '-'),
	  
	  (pte->ctl.flags.c_ure ? U : '-'),
	  (pte->ctl.flags.c_sre ? S : '-'),
	  (pte->ctl.flags.c_ere ? E : '-'),
	  (pte->ctl.flags.c_kre ? K : '-'),
	  
	  pte->ctl.flags.c_gh >> 1,
	  pte->ctl.flags.c_gh & 1,
	  
	  (pte->ctl.flags.c_asm ? 'A' : '-'),
	  (pte->ctl.flags.c_foe ? 'E' : '-'),
	  (pte->ctl.flags.c_fow ? 'W' : '-'),
	  (pte->ctl.flags.c_for ? 'R' : '-'),
	  
	  (pte->ctl.flags.c_valid ? 'V' : '-'),
	  
	  pte->ctl.all);
}


INLINE uint32_t alloc_pdidx(MMU_st *st)
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
    return 0xdead;
}



static bool_t add8k_pages(MMU_st *st, word_t vpn, hwpte_t *newpte, uint32_t n)
{
    hwpte_t *cur; 
    int i;
    
    if((vpn + n) > st->maxidx) {
	eprintf("add8k_pages: too many pages [%d from va %lx].\n", 
		n, vpn << PAGE_WIDTH);
	return False;
    }

    cur = &(st->ptbl[vpn]);
    
    for (i = 0; i < n; i++) {

	cur->pfn     = newpte->pfn;
	cur->sid     = newpte->sid;
	cur->ctl.all = newpte->ctl.all | PTE_M_KRE | PTE_M_KWE;

	if(newpte->ctl.flags.c_valid) {
	    if(cur->pfn != (vpn + i)) 
		ntsc_dbgstop();
	}

	cur++;
	
	/* Invalidate Both TB entries for this VA */
	ntsc_tbi(3, (addr_t)((vpn+i) << PAGE_WIDTH));
    }
    
    return True;
}


/*
** We only allow the updating of SIDs and control bits; i.e. 
** one may not use the below to change mapping information. 
*/
static bool_t update8k_pages(MMU_st *st, word_t vpn, 
			     hwpte_t *newpte, uint32_t n)
{
    hwpte_t *cur;
    int i;
    
    if((vpn + n) > st->maxidx) {
	eprintf("update8k_pages: too many pages [%d from va %lx].\n", 
		n, vpn << PAGE_WIDTH);
	return False;
    }

    
    cur = &(st->ptbl[vpn]);
    
    for (i = 0; i < n; i++) {

	cur->sid     = newpte->sid;
	cur->ctl.all = newpte->ctl.all | PTE_M_KRE | PTE_M_KWE;
	
	cur++;
	
	/* Invalidate Both TB entries for this VA */
	ntsc_tbi(3, (addr_t)((vpn+i) << PAGE_WIDTH));
    }
    
    return True;
}


static bool_t free8k_pages(MMU_st *st, word_t vpn, uint32_t n)
{
    uint32_t curpfn, fw, state, owner = OWNER_NONE; 
    hwpte_t *cur;
    int i;
    
    if((vpn + n) > st->maxidx) {
	eprintf("free8k_pages: too many pages [%d from va %lx].\n", 
		n, vpn << PAGE_WIDTH);
	return False;
    }

    
    cur    = &(st->ptbl[vpn]);
    curpfn = vpn;                  /* XXX assumes 121 memory mappings */
    
    for (i = 0; i < n; i++) {

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

	cur->pfn     = 0xDEADBEEF;
	cur->sid     = SID_NULL;
	cur->ctl.all = 0;
	
	cur++;
	curpfn++;

	/* Invalidate Both TB entries for this VA */
	ntsc_tbi(3, (addr_t)((vpn+i) << PAGE_WIDTH));
    }
    
    return True;
}


/*
** Before we switch over to our own page table, we need to ensure that
** we have all the mappings already established in our table too. 
*/
static void enter_mappings(MMU_st *const st, Mem_PMem pmem, Mem_Map mmap)
{
    word_t   curva, curpa;
    uint32_t curnf, fsize, psize;
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

	if(fwidth != WIDTH_8K) 
	    /* In this memsys, ignore any odd regions */
	    continue; 

	gh    = 0;
	psize = Kb(8);
	
	newpte.ctl.all |= gh; 
	fsize = psize; 
	    
	while(curnf) {
	    
	    newpte.pfn = curpa >> FRAME_WIDTH;

	    if(!add8k_pages(st, VPN(curva), &newpte, 1)) {
		eprintf("enter_mappings: failed to add pages to ptab?!?\n");
		ntsc_halt();
	    }

	    curva += psize; 
	    curpa += fsize; 
	    curnf--;
	}
    }

}



/*--------------------------------------------- MMUMod Entry Points ----*/


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




/*
** Initalise the "MMU" datastructures in physical memory 
 */
static MMU_clp New_m(MMUMod_cl *self, Mem_PMem pmem, Mem_Map init, 
		     word_t size, RamTab_clp *ramtab, addr_t *free)
{
    word_t stsz, ptsz, rtsz, totsz, maxpfn; 
    MMU_st *st; 
    int npages;
    hwpte_t *pte;
    int i;

    TRC(eprintf("MMUMod$New entered, size is %x\n", size));

    stsz = ROUNDUP(sizeof(*st), FRAME_WIDTH);

    /* Compute the amount of space reqd for the ramtab; 1 entry per PFN */
    rtsz = ROUNDUP(rtab_reqd(pmem, &maxpfn), FRAME_WIDTH);

    /* 
    ** Compute how much space is required initially for page tables.
    ** This file implements a simple linear page table to map all 
    ** virtual addresses up to the maximum *physical* address in memory.
    ** We've just counted the number of pfns, so for now we just use
    ** this value as the upper bound on our page table too. Any virtual 
    ** addresses "higher up" will be mapped 121 by the PALcode. 
    */
    npages = maxpfn; 
    ptsz   = ROUNDUP((npages * sizeof(struct hwpte)), FRAME_WIDTH);

    totsz = stsz + ptsz + rtsz + size; 


    st        = (MMU_st *)alloc_state(pmem, init, totsz);
    TRC(eprintf("MMUMod$New got %d bytes at (va) %p\n", totsz, st));

    st->vptbr = ROUNDUP(st + 1, FRAME_WIDTH);
    st->pptbr = st->vptbr;              /* 121 mapping here */

    st->ptbl   = (hwpte_t *)st->vptbr; 
    TRC(eprintf("MMUMod$New: got ptbr at %p\n", st->ptbl));

    TRC(eprintf("MMUMod$New: setting max idx to %p\n", npages));
    st->maxidx = npages; 
    
    /* Init the table to fault EVERYTHING */
    TRC(eprintf("+--------+----+----+----+----+----+\n"));
    TRC(eprintf("|  PFN   |SID | RD | WR |RGHA|EWRV|\n"));
    TRC(eprintf("+--------+----+----+----+----+----+\n"));
    pte = st->ptbl; 
    for (i = 0; i < npages; i++) {
	pte->pfn = i;
	pte->sid = SID_NULL;	/* Not assigned yet */
	
	/* Initially we fault EVERYTHING! */
	pte->ctl.all = PTE_M_FOE|PTE_M_FOW|PTE_M_FOR;
	
	TRC(if (i < 8) dump_pte(pte));
	pte++;
    }
    TRC(eprintf(":        :    :    :    : :   :  :\n"));


    /* Add mappings for the existing memory covered. */
    TRC(eprintf("Entering mappings for existing memory.\n"));
    enter_mappings(st, pmem, init);


    CL_INIT(st->mmucl, &mmu_ms, st);
    CL_INIT(st->rtcl, &rtab_ms, st);
    
    st->ramtab = (RamTable_t *)((void *)st->vptbr + ptsz);
    TRC(eprintf("MMUMod$New: ram table at %p\n", st->ramtab));
    st->maxpfn = maxpfn;

    *ramtab = &st->rtcl;

    /* Initialise the protection domain tables &c */
    st->next_pdidx = 0; 
    st->next_asn   = 0; 
    for(i = 0; i < PDIDX_MAX; i++) {
	st->pdom_tbl[i] = NULL;
	st->pdominfo[i].refcnt  = 0;
	st->pdominfo[i].asn     = 0;
	st->pdominfo[i].gen     = 0;
	st->pdominfo[i].stretch = (Stretch_cl *)NULL;
    }

    /* And store a pointer to the pdom_tbl in the info page */
    INFO_PAGE.pd_tab = &(st->pdom_tbl); 


    TRC(eprintf("MMUMod$New: writing PTBL addr %p\n", st->ptbl));
    ntsc_wrptbr(st->ptbl, &st->pdom_tbl[0] /* XXX 121 mapping here */);
    ntsc_tbi(-2, 0);


    *free   = ROUNDUP(st->ramtab + rtsz, FRAME_WIDTH);
    TRC(eprintf("MMUMod$New: returning free memory at %p\n", *free));
    return &st->mmucl;
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
    pdom_t  *base; 
    word_t   asn; 
    uint32_t idx = PDIDX(pdid); 
 
    if( (idx >= PDIDX_MAX) || ((base = st->pdom_tbl[idx]) == NULL) ) {
	eprintf("MMU$Engage: bogus pdid %x\n", pdid); 
	ntsc_dbgstop();
	return;
    }

    asn = st->pdominfo[idx].asn; 
    
    TRC(eprintf("MMU$Engage: pdid is %x => base is %p, asn is %x\n", 
		pdid, base, asn));
    
    /* Paranoia */
    if(asn != RO(Pvs(vp))->asn) 
	eprintf("MMU$Engage: warning - pdom asn == %x != %x == VP asn\n", 
		asn, RO(Pvs(vp))->asn); 
    
    ntsc_wrpdom(base, asn);    /* set the protection domain */
    ntsc_tbi(-2, 0);           /* invalidate all TB entries */
    return;
}

static void AddRange_m(MMU_cl *self, Stretch_cl *str, 
		       const Mem_VMemDesc *range, 
		       Stretch_Rights global)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = str->st; 
    hwpte_t     pte; 
    word_t      vpn;

    if(range->npages == 0) 
	return; 

    vpn = VPN((word_t)range->start_addr);
    if(vpn >= st->maxidx) {
	eprintf("MMU$AddRange: cannot add [%p,%p) unmapped in 121 MEMSYS.\n", 
		range->start_addr, range->start_addr + 
		(range->npages << range->page_width));
	return; 
    }


    pte.sid     = sst->sid;
    pte.pfn     = 0xFADEDBEE;
    pte.ctl.all = control_bits(sst, global, False);

    if(range->page_width != WIDTH_8K) {
	eprintf("MMU$AddRange: cannot deal with page width %d.\n", 
		range->page_width);
	ntsc_dbgstop();
    }

    if(!add8k_pages(st, vpn, &pte, range->npages)) {
	eprintf("MMU$AddRange: failed to add pages to ptab?!?\n");
	ntsc_dbgstop();
    }
    
    TRC(eprintf("MMU$AddRange: added va range [%p-%p), sid=%04x\n", 
	    range->start_addr, 
	    range->start_addr + (range->npages << PAGE_WIDTH), 
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
    uint32_t    curnp, fw, state, owner = OWNER_NONE; 
    word_t      curvpn, curpfn;
    hwpte_t     pte; 

    if((curnp = range->npages) == 0) 
	return; 

    if(curnp > pmem->nframes) {
	/* assume page width == frame width (coz won't work otherwise) */
	eprintf("MMU$AddMappedRange: too few frames in pmem to map vmem.\n");
	return;
    }

    curvpn = VPN((word_t)range->start_addr); 
    if(curvpn >= st->maxidx) {
	/* XXX SMH: currently this MMU code only handles virtual addresses 
	   upto maxva; anything above that is mapped 121 by the pal code. 
	   Hence if we have anything above maxva, we just need to sanity
	   check that we've a 121 mapping. */
	if( (pmem->start_addr  != range->start_addr) ||
	    (pmem->frame_width != range->page_width) || 
	    (pmem->nframes     != range->npages) ) { 
	    eprintf("MMU$AddMappedRange: bad mapping in 121 MEMSYS (va %p)\n", 
		    range->start_addr);
	    return; /* things will probably go pear shaped shortly */
	} 

	/* Ok; seems sane; assume everything's ok. */
	return;
    }

    curpfn = PFN((word_t)pmem->start_addr); 

    pte.sid     = sst->sid;
    pte.pfn     = 0xFADEDBEE;
    pte.ctl.all = control_bits(sst, gaxs, True);

    if(range->page_width != WIDTH_8K) {
	eprintf("MMU$AddMappedRange: cannot deal with page width %d.\n", 
		range->page_width);
	ntsc_dbgstop();
    }

    TRC(eprintf("MMU$AddMappedRange: adding va range [%p-%p) => [%p-%p)"
		", sid=%04x\n", range->start_addr, 
		range->start_addr + (range->npages << PAGE_WIDTH),
		pmem->start_addr, 
		pmem->start_addr + (pmem->nframes << FRAME_WIDTH), 
		sst->sid));

    while(curnp) {

	pte.pfn = curpfn;

	if(curpfn < st->maxpfn) {
	    
	    /* Sanity check the RamTab */
	    owner = RamTab$Get(&st->rtcl, curpfn, &fw, &state);
	    
	    if(owner == OWNER_NONE) {
		eprintf("MMU$AddMappedRange: physical addr %p not owned.\n",
			curpfn << FRAME_WIDTH);
		ntsc_dbgstop();
	    }

	    if(state == RamTab_State_Nailed) {
		eprintf("MMU$AddMappedRange: physical addr %p is nailed.\n",
			curpfn << FRAME_WIDTH);
		ntsc_dbgstop();
	    }
	}

	if(!add8k_pages(st, curvpn, &pte, 1)) {
	    eprintf("MMU$AddMappedRange: failed to add pages to ptab?!?\n");
	    ntsc_halt();
	}

	if(curpfn < st->maxpfn) {
	    /* Update the RamTab */
	    RamTab$Put(&st->rtcl, curpfn, owner, fw, RamTab_State_Mapped);
	}

	curpfn++;
	curvpn++;
	curnp--;
    }
    
    TRC(eprintf("MMU$AddMappedRange: added va range [%p-%p) => [%p-%p)"
		", sid=%04x\n", range->start_addr, 
		range->start_addr + (range->npages << PAGE_WIDTH),
		pmem->start_addr, 
		pmem->start_addr + (pmem->nframes << FRAME_WIDTH), 
		sst->sid));
    return;
}

static void UpdateRange_m(MMU_cl *self, Stretch_cl *str, 
			  const Mem_VMemDesc *range, 
			  Stretch_Rights gaxs)
{
    MMU_st     *st  = self->st;
    Stretch_st *sst = str->st; 
    word_t      vpn;
    hwpte_t     pte; 

    if(range->npages == 0) 
	return; 

    vpn = VPN((word_t)range->start_addr); 
    if(vpn >= st->maxidx) {
	eprintf("MMU$UpdateRange: cannot update [%p,%p) in 121 MEMSYS.\n", 
		range->start_addr, range->start_addr + 
	    (range->npages << range->page_width));
	return; 
    }

    /* Currently can only update the SID and global bits [i.e. not PFN] */
    pte.sid     = sst->sid;
    pte.ctl.all = control_bits(sst, gaxs, True);

    if(range->page_width != WIDTH_8K) {
	eprintf("MMU$UpdateRange: cannot deal with page width %d.\n", 
		range->page_width);
	ntsc_dbgstop();
    }

    
    if(!update8k_pages(st, vpn, &pte, range->npages)) {
	eprintf("MMU$UpdateRange: failed to add pages to ptab?!?\n");
	ntsc_dbgstop();
    }
    
    TRC(eprintf("MMU$UpdateRange: done.\n"));
    return;
}

static void FreeRange_m(MMU_cl *self, const Mem_VMemDesc *range)
{
    MMU_st *st = self->st;
    uint32_t pwidth;
    word_t   vpn;

    if(range->npages == 0) 
	return; 

    vpn = VPN((word_t)range->start_addr);
    if(vpn >= st->maxidx)
	/* This is all the 'freeing' we can do here ;-) */
	return;

    pwidth = range->page_width; 

    if(!free8k_pages(st, vpn, range->npages)) {
	eprintf("MMU$FreeRange: failed to zap entries from ptab!?!\n");
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

    
    /* Add the new pdom to the pdom table */
    st->pdom_tbl[idx] = base; 

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

    if((idx >= PDIDX_MAX) || (st->pdom_tbl[idx] == NULL)) {
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
    if((idx >= PDIDX_MAX) || (st->pdom_tbl[idx] == NULL)) {
	eprintf("MMU$FreeDomain: bogus pdid %lx\n", pdid); 
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

    if( (idx >= PDIDX_MAX) || ((p = st->pdom_tbl[idx]) == NULL) ) {
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

    if( (idx >= PDIDX_MAX) || (st->pdom_tbl[idx] == NULL) ) {
	eprintf("MMU$SetProt: bogus pdid %lx\n", pdid); 
	ntsc_dbgstop();
    }

    return st->pdominfo[idx].asn; 
}

static Stretch_Rights QueryGlobal_m(MMU_cl *self, Stretch_clp str)
{
    MMU_st        *st  = self->st;
    Stretch_st    *sst = str->st; 
    hwpte_t       *cur; 
    word_t         vpn; 
    Stretch_Rights res; 
    uint16_t       ctl; 

    res = Stretch_Rights_None; 
    if(!sst->s) return res; 

    if((vpn = VPN(sst->a)) > st->maxidx) {
	/* In IO-Space => can't lookup */
	eprintf("MMU$QueryGlobal: got IO-space str %p to do ?!?\n", str);
	return res; 
    }

    cur = &(st->ptbl[vpn]);
    ctl = cur->ctl.all; 

    if(ctl & PTE_M_URE)    res |= SET_ELEM(Stretch_Right_Read); 
    if(ctl & PTE_M_UWE)    res |= SET_ELEM(Stretch_Right_Write); 
    if(!(ctl & PTE_M_FOE)) res |= SET_ELEM(Stretch_Right_Execute); 
    if(ctl & PTE_M_ASM)    res |= SET_ELEM(Stretch_Right_Global); 

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

	curp = st->pdom_tbl[idx];

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
