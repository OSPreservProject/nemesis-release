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
**      sys/memory_expt
**
** FUNCTIONAL DESCRIPTION:
** 
**      This is the C part of the alpha guarded page table implementation. 
**      This works only with memsys EXPT. 
** 
** ID : $Id: eb164_mmu_gpt.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
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
    if(((word_t)vaddr & 0x12300000000UL) == (0x12300000000UL))	\
	_x; 							\
}

#undef XTRC(_x)
#define XTRC(_x) 						\
{ 								\
    if(((word_t)vaddr & 0x12300000000UL) == (0x12300000000UL))	\
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


/* Information about protection domains */
typedef struct _pdom_st {
    uint16_t      refcnt;  /* reference count on this pdom    */
    uint16_t      asn;     /* ASN associated with this pdom   */
    uint32_t      gen;     /* Current generation of this pdom */
    Stretch_clp   stretch; /* handle on stretch (for destroy) */ 
} pdom_st; 



/* Number of bytes in a chunk of page table memory */
#define CHUNK_SIZE   (PT_SZ_MAX * sizeof(struct gpte))

/*
** We keep track of free chunks of memory of various sizes to allocate
** new page tables from (and of course we also may free page tables).
** We use the below type of linked list for this.
** Note that *every* chunk of memory is of size CHUNK_SIZE which is 
** (above defined to be) PT_SZ_MAX * sizeof(gpte_t). Hence each 
** chunk contains 1, 2, 4, 8, etc page tables depending on the 
** page table size.
*/

#define NBMAPS 4    /* allows up to 64*4 = 256 page tables per chunk */

typedef struct _ptmem {
    word_t         bitmap[NBMAPS]; /* Bitmap of free ptabs within the chunk */
    addr_t         base;           /* Virt/Phys address of this chunk (121) */
    struct _ptmem *next;           /* Pointer to next structure, or NULL    */
} ptmem_t;


/*
** The below two macros are used to manage the bitmap array above; 
** for a given size of page table SZ, we have NP = (CHUNK_SIZE / SZ)
** page tables per chunk. Then: 
**    -- BIX_MAX(NP) gives the upper bound on the number of elements
**       of the bitmap array we need to consider, and 
**    -- ALLBITS(NP) gives the bitmask representing 'all full' for 
**       the chunk (since we only use powers of two, the 'all full' 
**       bitmap is the same for all words [0...BIX_MAX). 
*/

#define BIX_MAX(_np) (((_np) + (WORD_LEN-1)) >> 6)
#define ALLBITS(_np) ((!((_np)%WORD_LEN)?0:(1UL<<((((_np)-1)&0x3F)+1)))-1UL)



/* 
** We hold free chunks from size (MAX(PT_SZ_MIN,2) * sizeof(gpte_t)) upto and 
** including chunks of (PT_SZ_MAX * sizeof(gpte_t)).
*/
#define PT_ARRAY_SZ  (LOG2_PT_SZ_MAX-1)




/*
** We allocate a level 1 table with N_L1ENTS GPTEs within our state. 
*/
#define N_L1ENTS     (1024)
#define LOG2_NL1ENTS (10) 

typedef struct _mmu_st {

    /* First part of our state holds the level 1 page table */
    struct gpte          l1tab[N_L1ENTS];  /* Aligned to page boundary */

    /* 
    ** Following this we have a page worth of 'ptmem_t's, st->cdesc[].
    ** In general, the first st->cdidx of these structures will each
    ** describe a 'chunk' of memory we've allocated. 
    ** st->chunks is *not* typically navigated itself; instead each of 
    ** the structure st->chunks[0], .. st->chunks[st->chunkidx - 1] 
    ** will be on either (a) the free list of chunks, st->free, or 
    ** (b) on the page table lists, st->ptabs[0,...n]. 
    ** 
    ** NB: cdidx is monotonically increasing => we currently never 
    ** release any physical memory allocated for page tables. It may, 
    ** of course, be reused for later page tables. 
    */
    ptmem_t              cdesc[PAGE_SIZE / sizeof(ptmem_t)];   
    word_t               cdidx;            /* high watermark for st->cdesc */


    word_t               next_pdidx;           /* Next free pdom idx (hint)  */
    word_t               next_asn;             /* Next free ASN to issue     */
    pdom_t              *pdom_vtbl[PDIDX_MAX]; /* Map pdidx to pdom_t (va)   */
    pdom_t              *pdom_ptbl[PDIDX_MAX]; /* Map pdidx to pdom_t (pa)   */
    pdom_st              pdominfo[PDIDX_MAX];  /* Maps pdom IDs to pdom_st's */


    /* And now the `standard' state fields */
    MMU_cl               cl;
    RamTab_cl            rtcl; 

    Frames_cl           *frames;           
    Heap_cl             *heap;
    StretchAllocator_cl *sysalloc;         /* 'special' salloc [nailed]    */

    word_t               pptbr;            /* physical ptab base register  */
    word_t               vptbr;            /* virtual version of above     */

    RamTable_t          *ramtab;           /* base of ram table            */
    word_t               maxpfn;           /* size of above table          */

    /* 
    ** As noted in the comment re: st->cdesc/st->cdidx above, the first 
    ** st->cdidx members of st->cdesc[] are all on one of the below lists: 
    ** 
    **   - if the chunk described by the cdesc is unused, then is is on
    **     the st->free list. 
    ** +
    **   - if the chunk described by the desc contains one or more page
    **     tables, then it is on the st->pts[log2_n] list, where all 
    **     the page tables it contains are of size 2^(log2_n + 1).
    */    
    ptmem_t              *free;             /* list of unused chunks        */
    ptmem_t              *pts[PT_ARRAY_SZ]; /* lists of used chunks         */
} MMU_st;


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
** -------- Debugging Calls --------------------------------------------- **
*/

void dump_gpte(struct gpte *gpte, char *pfx, int idx, bool_t banner)
{

    if(IS_NULL_GPTE(gpte)) {
	if(banner) 
	    eprintf("%s+-----------------+", pfx);
	eprintf("%s>>> NULL GPTE <<<<\n", pfx);
	return;
    }

    if(GRD_LEAF(gpte->guard)) {
	if(banner) {
	    eprintf("%s+----+------------------+---+---+----+--------+----+"
		    "----+----+----+----+\n", pfx);
	    eprintf("%s|Indx|      Guard       | L | S | SZ |  PFN   |"
		    "SID | RD | WR |NGHA|EWRV|\n", pfx);
	    eprintf("%s+----+------------------+---+---+----+--------+----+"
		    "----+----+----+----+\n", pfx);
	}

	eprintf("%s|%04x|%p| %s | %s | %02x |", pfx, idx, 
		GRD_VAL(gpte->guard),
		GRD_LEAF(gpte->guard) ?"L":"-",
		GRD_SPAGE(gpte->guard)?"S":"-",
		PTR_SIZE(gpte->guard));
	
	/* if we have a valid leaf, then dump the actual PTE info */
	eprintf("%08x|%04x|%c%c%c%c|%c%c%c%c|%c%d%d%c|%c%c%c%c|\n", 
		 gpte->pointer.hwpte.pfn,
		 gpte->pointer.hwpte.sid,
		 
		 (gpte->pointer.hwpte.ctl.flags.c_uwe ? '?' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_swe ? '?' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_ewe ? 'U' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_kwe ? 'K' : '-'),
		 
		 (gpte->pointer.hwpte.ctl.flags.c_ure ? '?' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_sre ? '?' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_ere ? 'U' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_kre ? 'K' : '-'),
		 
		 (gpte->pointer.hwpte.ctl.flags.c_nld ? 'N' : '-'),

		 gpte->pointer.hwpte.ctl.flags.c_gh >> 1,
		 gpte->pointer.hwpte.ctl.flags.c_gh & 1,
		 
		 (gpte->pointer.hwpte.ctl.flags.c_asm ? 'A' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_foe ? 'E' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_fow ? 'W' : '-'),
		 (gpte->pointer.hwpte.ctl.flags.c_for ? 'R' : '-'),
		 
		 (gpte->pointer.hwpte.ctl.flags.c_valid ? 'V' : '-'));
    } else {
	if(banner) {
	    eprintf("%s+----+------------------+---+---+----+"
		    "------------------+----+\n", pfx);
	    eprintf("%s|Indx|      Guard       | L | S | SZ |"
		    "      Pointer     | SZ |\n", pfx);
	    eprintf("%s+----+------------------+---+---+----+"
		    "------------------+----+\n", pfx);
	}
	eprintf("%s|%04x|%p| %s | %s | %02x |", pfx, idx, 
		GRD_VAL(gpte->guard),
		GRD_LEAF(gpte->guard) ?"L":"-",
		GRD_SPAGE(gpte->guard)?"V":"-",
		PTR_SIZE(gpte->guard));
	
	eprintf("%p| %02x |\n",
		PTR_VAL(gpte->pointer.ptr),
		PTR_SIZE(gpte->pointer.ptr));
    }
}




#ifdef DUMP_TABLES
/*
** dump_tables() is a utility function to (recursively) dump a tree
** of page tables, terminating whenever it hits a leaf. 
** The "mbz" parameter describes how many leading bits we know already
** to be zero when we get here; the "level" field just carries the 
** current depth, and is used only for pretty printing.
*/
void dump_tables(word_t ptr, int mbz, int level)
{
    gpte_t *cur;
    int i, table_size;
    char buf[32], *bufptr;

    table_size = 1UL<<(WORD_LEN-(PTR_SIZE(ptr)+mbz));
    cur = (gpte_t *)SUPERPAGE(PTR_VAL(ptr));

    /* sort out the prefix for printing */
    bufptr= buf;
    for(i=0; i < level; i++)
	*bufptr++ = ' ';
    *bufptr = '\0';

    eprintf("%s ~~~~~ Level %d: table at %p, with %d entries. ~~~~~\n",
	    buf, level, PTR_VAL(ptr), table_size);
	
    for(i=0; i < table_size; i++) {
	if(!IS_NULL_GPTE(&cur[i])) {
	    dump_gpte(&cur[i], buf, i, !i);
	    if(!GRD_LEAF(cur[i].guard)) {
		dump_tables(cur[i].pointer.ptr, 
			    WORD_LEN - PTR_SIZE(cur[i].guard), 
			    level+1);
	    }
	}
    }
    
    eprintf("%s ~~~~~ Level %d: table at %p, done. ~~~~~\n",
	    buf, level, PTR_VAL(ptr));
}
#endif

#ifdef XTRA_CHECKS
void check_va(word_t va)
{
    word_t pte; 
    uint32_t vpn   = (va >> PAGE_WIDTH);

    /* Now look up the translation of the vaddr & check its ok */
    if( (pte = ntsc_trans(vpn)) == NULL_PTE)
	eprintf(" XXX vaddr %p [vpn %x] translation not valid! urk!\n", 
		va, vpn);
    else eprintf(" vaddr %p [vpn %x] maps to PTE = %p\n", 
		 va, vpn, pte);

}

void dbl_check_va(MMU_cl *self, word_t va)
{
    MMU_st *st = self->st; 
    gpte_t *entry; 

    if(!lookup_va(st->pptbr, va, &entry, 24)) {
	eprintf("lookup_va failed on %p\n");
	eprintf("why?\n");
	ntsc_halt();
    }
    check_va(va);
}
#endif

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
** We will deal with 'chunks' of page table memory, where each 
** chunk is CHUNK_SIZE = (PT_SZ_MAX * sizeof(struct gpte)) = 16384 bytes.
** Hence the maximum size page table will require an entire chunk, 
** while smaller sizes will require 1/2, 1/4, etc. of a chunk.
*/

static word_t ptmem_reqd(Mem_Map mmap)
{
    word_t res; 
    int i;


    for(i = 0; mmap[i].nframes; i++) {
	XTRC(eprintf("ptmem_reqd: got memmap [%08x,%08x) -> [%08x,%08x), "
			"%d frames\n", 
			mmap[i].vaddr, mmap[i].vaddr + 
			(mmap[i].nframes << mmap[i].frame_width), 
			mmap[i].paddr, mmap[i].paddr + 
			(mmap[i].nframes << mmap[i].frame_width), 
			mmap[i].nframes));
    }
    
    /* XXX SMH: for now, just hardwire the number of chunks to 16 */
    res = (CHUNK_SIZE * 16);
    return res;
}
    


/* 
** The below simply picks the next chunk descriptor, increments the 
** index points, and returns the address of the chosen descriptor. 
** XXX Failure is *not* considered at present (nor is freeing ;-)
*/
INLINE ptmem_t *cdesc_alloc(MMU_st *st)
{
    return (ptmem_t *)&(st->cdesc[st->cdidx++]);
}


/*
** Initialise the page table memory; since we have not yet allocated
** any page tables (apart from the L1 in our state), all of our chunks 
** go onto the free list for now; as and when we require page tables 
** of various sizes, we'll move them onto the relevant page table lists.
*/
void init_ptabs(MMU_st *st, void *ptbase, word_t ptsize, bool_t sysinit)
{
    ptmem_t **cur;
    word_t used; 
    int i;

    /* First off, bzero the entire piece of memory */
    for(i= 0; i < (ptsize / sizeof(word_t)); i++)
	((word_t *)SUPERPAGE(ptbase))[i]= (word_t)0; 

    INITTRC(eprintf("Zeroed page table memory at %lx\n", ptbase));

    /* Now fill the free list with all the memory we have */
    used = 0; 
    cur = &st->free; 
    while(used < ptsize) {
	*cur         = cdesc_alloc(st);      /* get a new 'node' */
	(*cur)->base = ptbase + used; 
	for(i = 0; i < NBMAPS; i++) 
	    (*cur)->bitmap[i] = 0;           /* all memory free at start */
	(*cur)->next = NULL; 

	cur   = &((*cur)->next);
	used += CHUNK_SIZE;
    }
    *cur = NULL;

    if(sysinit) {
	/* Finally, mark all the ptab arrays as empty */
	for(i = 0; i < PT_ARRAY_SZ; i++) 
	    st->pts[i] = NULL; 
    }

    /* And that's it */
    return;
}


static void alloc_chunks(MMU_st *st, uint32_t nchunks) 
{
    void *base; 
    size_t size; 

    if(!st->frames) {
	eprintf("eb164_mmu: out of ptab memory and cannot alloc more.\n");
	eprintf("(PANIC).\n");
	ntsc_dbgstop();
    }

    size = nchunks * CHUNK_SIZE; 
    base = Frames$Alloc(st->frames, size, PAGE_WIDTH);

    init_ptabs(st, SUPERPAGE(base), size, False); 
    return;
}

/*
** -------- Page Table Alloc & Free ------------------------------------- **
*/


/*
** pt_alloc: gets an aligned piece of physical memory large enough to hold 
** 'n' == '2^log2_n' guarded page table entries, and initialises all entries
** to GPTE_NULL.
** Note that since the "pointer" part of a GPTE requires bits [5:0] to 
** describe the size of the table, all page tables \emph{must} be aligned
** to at least 64 bytes. This means that 2 entry tables (which only require
** 32 bytes) are actually allocated as 4 entry tables.
*/
static word_t pt_alloc(MMU_st *st, int log2_n)
{
    ptmem_t *res, *oldhd;
    uint32_t num_entries, ptab_size, npts;
    gpte_t  *cur;
    word_t   bit;
    int      i, j, k, l; 
    
    if(log2_n == 1) {
	STRC(eprintf("Urk! log2_n is too small => get tiny ptabs. up it.\n"));
	log2_n++;
    }

    i = log2_n-LOG2_PT_SZ_MIN;
    STRC(eprintf("pt_alloc: st=%p, log2_n=%x, index=%x\n", st, log2_n, i));

    num_entries = 1 << log2_n;
    ptab_size   = num_entries * sizeof(gpte_t);

    if(i < 0 || i >= PT_ARRAY_SZ) {
	eprintf("pt_alloc: cannot get page table for %d entries.\n",
		num_entries);
	ntsc_halt();
    }
    STRC(eprintf("pt_alloc: want table for %d entries\n", num_entries));


    if(i < PT_ARRAY_SZ) {  
	/* ok, plausible: check list starting at st->pts[i] */

	res  = st->pts[i];
	npts = CHUNK_SIZE / ptab_size; /* no. of ptabs per chunk in list */

	while(res) {

	    /* Check bitmap to see if this ptmem_t has a free ptab  */
	    for(j = 0; j < BIX_MAX(npts); j++) {

		/* XXX NB: below requires npts to be a power of two */
		if(res->bitmap[j] < ALLBITS(npts)) {

		    /* Got one in this desc! Cool. Update bitmap */
		    k = 0; bit = 1; 
		    while(res->bitmap[j] & bit) {
			k++; 
			bit <<= 1UL; 
		    }
		    res->bitmap[j] |= bit;
		    
		    /* k is the index relative to this bitmap area; 
		       update it to be happy for j >= 1 */
		    k += (j * WORD_LEN); 
		    
		    STRC(eprintf("pt_alloc: got %d%s ptab in chunk@%p\n", 
				 k, k==1?"st":(k==2?"nd":(k==3?"rd":"th")), 
				 res->base));

		    STRC(eprintf("BTW: res->bitmap[%d] is now %p\n", 
				 j, res->bitmap[j]));
		    cur = (gpte_t *)(res->base + (k * ptab_size));
		    for(l = 0; l < num_entries; l++) {
			cur[l].guard = 0x0UL;
			cur[l].pointer.ptr = 0x0UL;
			cur[l].pointer.hwpte.pfn = NULL_PFN;
		    }
		    
		    STRC(eprintf("pt_alloc: %d entries at PHYS [%x -- %x]\n", 
				 num_entries, (word_t)res->base + 
				 (k*ptab_size), (word_t)res->base + 
				 ((k+1)*ptab_size)));
		    return (word_t)(res->base + (k * ptab_size));
		} /* else bitmap[j] is full; try the next one */
	    }
	    res = res->next; 
	}

	/* No free ptabs on our list; grab a new one from the free list */

	if(!st->free)
	    /* None on the free list either; alloc some more from frames */
	    alloc_chunks(st, 2 /* XXX hardwired to two chunks for now */); 

	res        = st->free;    /* grab the head of the free list      */
	st->free   = res->next;   /* link around the chunk we've removed */
	oldhd      = st->pts[i];  /* store the old head of our ptab list */
	st->pts[i] = res;         /* coz the new head is our new chunk!  */
	res->next  = oldhd;       /* and link through to the old head    */

	/* 
	** Ok: now have an 'empty' chunk on the head of our current page 
	** list. Just need to grab the first page table in that chunk, and
	** then we're done. 
	*/
#ifdef SANITY_CHECK
	for(j=0; j < NBMAPS; j++) 
	    if(res->bitmap[j] != 0) 
		ntsc_dbgstop();
#endif
	res->bitmap[0] |= 1; 
	cur = (gpte_t *)res->base;
	for(j = 0; j < num_entries; j++) {
	    cur[j].guard = 0x0UL;
	    cur[j].pointer.ptr = 0x0UL;
	    cur[j].pointer.hwpte.pfn = NULL_PFN;
	}
	
	return (word_t)res->base; 
    }

    /* Oops. No ptabs around. We're doomed (coalesce, or alloc more mem) */
    eprintf("pt_alloc: panic -> no chunks of size %x\n", ptab_size);
    ntsc_dbgstop();
    return (word_t)0xabbadead;    /* keep gcc happy */
}


static void pt_free(MMU_st *st, word_t start, int log2_n)
{
    int i;

    i = log2_n-LOG2_PT_SZ_MIN;
    if(i < 0 || i >= PT_ARRAY_SZ) {
	eprintf("pt_free: cannot use %x bytes starting at %lx.\n",
		((1 << log2_n) * sizeof(gpte_t)), start);
	return;
    }

    eprintf("pt_free: start=%p, log2_n=%x, index=%x.\n", start, log2_n, i);
    eprintf("Not yet implemented.\n");
    ntsc_dbgstop();
    return;

}


/*
** Lookup Functions -------------------------------------------- **
*/

/*
** SMH: utility function to lookup a virtual address in the GPTs.
** If successful, returns True, and (*res) holds a pointer to the
** GPTE of the relevant address.
** Otherwise returns False, and (*res) holds a pointer to the GPTE
** where we hit the guard fault.
*/
static bool_t lookup_va(word_t ptr, word_t vaddr, gpte_t **res, int max_lvls) 
{
    word_t idx;

    if(max_lvls == 0) {
	eprintf("lookup_va: ptr=%lx, vaddr=%lx: too many levels.\n", 
		ptr, vaddr);
	return False;
    }

    idx  = (vaddr >> PTR_SIZE(ptr));
    *res = (gpte_t *)SUPERPAGE(PTR_VAL(ptr)) + idx;

    XTRC(eprintf("lookup_va: ptr=%lx, vaddr=%lx, idx=%d\n", ptr, vaddr, idx));

    /* Now xor the vaddr with the guard */
    vaddr ^= (*res)->guard;
    if(vaddr >> PTR_SIZE((*res)->guard) != 0) /* guard fault */
	return False;
    
    if(GRD_LEAF((*res)->guard))
	return True;

    return(lookup_va((*res)->pointer.ptr, vaddr, res, max_lvls-1));
}


/*
** heuristic(): called when we have a guard fault and want to make
** a split; in parameters are:
**   "sz"     -  current table size, encoded as a shift right value
**   "glen"   -  current guard length, encoded as above. glen < sz.
**   "pwidth" -  the width of the new page, whose addition caused this split.
**   "npages" -  a hint about the number of contiguous pages likely
**               to be added soon. 
**   "gva"    -  the guarded va which faulted, i.e. va XOR guard.
** From these we compute the size of the new next-level page table, 
** and the length of this levels modified (shortened) guard. 
*/
static uint32_t heuristic(uint32_t sz, uint32_t glen, 
			  uint32_t pwidth, uint32_t npages, 
			  word_t gva, uint32_t *mod_glen)
				
{
    uint32_t i, x, y, z;
    uint32_t xlen, ylen, zlen;
    uint32_t new_idx, new_sz, ubound;

    /*
    ** Now consider the the values x, y & z in "gva" (i.e. the vaddr 
    ** after we've applied the guard) as illustrated below:
    ** 
    **                    <-----------------    SZ  ------->
    **                   |---- MISGRD ----|  <-- GLEN ------->
    **  +------------+---+-----+----+-----+-------+-+-+----+
    **  |000......000|0.0|0...0|DIFF|0...0|00...00|L|V|GLEN|
    **  +------------+---+-----+----+-----+-------+-+-+----+
    **  <--- MBZ --->     <-x->  y   <-z->
    **
    ** The bits in MISDGRD are split into three parts: x, the prefix 
    ** part which matched the guard, y, the part which failed to match
    ** the guard, and z, the suffix which matched the guard. 
    ** Both |x| and |z| may be zero, but |y| must be >= 1 (or we would 
    ** not have faulted). 
    **
    ** When we split, we will create a new page table below the current
    ** one. We require that the new virtual address will match in there, 
    ** as well as any virtual addresses which would have matched on this
    ** current guard. To perform the split, we need to derive 3 parts 
    ** out of MISGRD:
    ** 
    **   1. NEWGRD - this will be the new guard used in place of the 
    **               one which caused this fault. 
    **               It will be a prefix (possibly NULL) of the old
    **               guard which also matches the new address. 
    **               i.e. 0 <= |NEWGRD| <= |x|
    **  
    **   2. IDX'   - this will be the index part of GPTE in the soon-
    **               to-be-created next-level page table. 
    **               The size of this new page table, in fact, will 
    **               be determined by IDX', since its size will end
    **               up being 2^{|IDX'|}. 
    **               This field must suffice to distinguish between 
    **               the old and the new (currently faulting) virtual 
    **               addresses, and so must contain at least some of 
    **               y. It might contain all of x+y+z either. 
    **               Additionally, LOG2_PT_SZ_MIN <= |IDX'| <= LOG2_PT_SZ_MAX
    ** 
    **   3. GRD'   - this will be the guard part in the next-level GPTE
    **               which will map the virtual addresses currently 
    **               handled by this faulting entry.  If IDX' does not
    **               extend to the end of MISGRD, then GRD' will need
    **               to contain the remaining bits. 
    **               
    ** In addition to any individual constraints above, we also have
    ** the contraint that any guard should be >= LOG2_PT_SZ_MIN. This 
    ** arises out of the observation that if we ever need to split 
    ** a GPTE:
    **    we need   |IDX'| >= LOG2_PT_SZ_MIN  
    **    we know   |IDX'| <= |x|+|y|+|z|
    **
    ** => need |x|+|y|+|z| >= LOG2_PT_SZ_MIN
    **    i.e.    |MISGRD| >= LOG2_PT_SZ_MIN.
    ** 
    ** The only time we may have a guard with less bits is when we are
    ** certain that we will never get a guard fault - which is only 
    ** when we have a guard of zero length. 
    ** So we have the following requirements:
    **
    **             |NEWGRD| = 0 or |NEWGRD| >= LOG2_PT_SZ_MIN
    **         and                   |IDX'| >= LOG2_PT_SZ_MIN
    **         and                   |IDX'| <= LOG2_PT_SZ_MAX
    **         and   |GRD'| = 0 or   |GRD'| >= LOG2_PT_SZ_MIN
    ** 
    ** Assuming there are multiple solutions to this, we also need 
    ** to choose |IDX'| subject to the above constraints. Do we 
    ** try to maximise |IDX'| (best locally-computed depth) or minimise
    ** it (best space)? 
    ** 
    ** For now we will simply choose new_idx which comes closest to 
    ** representing an "ideal" new page-table size. This "ideal" 
    ** value is chosen based on the hint 'npages' passed into this 
    ** routine - the number of contiguous pages after this one 
    ** likely to be added 'soon'. We want new_idx such that more 
    ** than half the page table will be (hopefully) filled. 
    **
    **  i.e. ideal s.t 
    **                 npages >= (2^{ideal} / 2) + 1
    **           or  2*npages >   2^{ideal}
    ** 
    ** After choosing such an ideal (subject to the max/min requirements
    ** mentioned above), we select locations by 
    ** trying to use the existing (x,y,z) partition if possible; if we 
    ** need to shrink y, we use the trailing bits of y in the guard, 
    ** repeating it in all page table entries. 
    ** If we need to grow y, we first try to add bits from z, and iff
    ** we exhaust these, do we add bits from the rhs (least significant)
    ** of x. This is based on the intuition that higher (more left) guard 
    ** bits are less likely to fault than lower ones (possion 
    ** distribution hazarded). 
    */

    /* 
    ** Compute our 'ideal' new page table size based on the number
    ** of pages to follow, as given in the "npages" hint parameter.
    ** The maximum size is the entire faulting guard, which contains
    ** (sz - glen) bits, so we first check this is large enough. 
    */
#ifdef SANITY_CHECK
    if((sz-glen) < LOG2_PT_SZ_MIN) {
	eprintf("heuristic: guard is too small (only %d bits)!\n", sz-glen);
	eprintf("gva is %p, sz=%x, glen=%x\n", gva, sz, glen);
	ntsc_dbgstop();
    }
#endif

    new_idx = LOG2_PT_SZ_MIN;
    ubound  = MIN((sz-glen), LOG2_PT_SZ_MAX); 
    while((npages << 1) > (1 << new_idx)) {
	if(new_idx >= ubound) 
	    break;
	new_idx++;
    }

    HTRC(eprintf("Considering npages=%d, new_idx=%d (new_ptab size is %d)\n", 
	    npages, new_idx, 1<<new_idx));

    /* 
    ** Now we want to select which bits within the guard to take for
    ** the index field of the next level. We start by trying to use 
    ** the bits in y. If |y| is > new_idx, we shrink the new idx from 
    ** the rhs. If |y| is < new_idx, we expand by taking bits from 
    ** the lhs of z. If |y| + |z| is < new_idx, we expand further
    ** by taking bits from the rhs of x. 
    ** Once this has been done, we may need to change our answer a 
    ** bit in order to satisfy the guard conditions (i.e. length of 0
    ** or length >= LOG2_PT_SZ_MIN).
    */

    /* First, get |x|, |y| and |z| */
    for(i = sz-1; (i >= glen) && ((gva >> i) == 0); i--) 
	;
    xlen = sz - 1 - i;
    for(zlen = 0;  ((gva >> (glen + zlen)) & 1) == 0; zlen++)
	;
    ylen = (sz - glen) - xlen - zlen; 
    HTRC(eprintf("xlen=%d, ylen=%d, zlen=%d\n", xlen, ylen, zlen));

    z = glen + zlen; 
    y = z + ylen;
    x = sz; 
    
    HTRC(eprintf("x=%d, y=%d, z=%d\n", x, y, z));


    if(y - glen >= new_idx) {
	/* do idx and guard totally on the rhs; x is happy */
	if(y-z == new_idx) { 
	    /* Cool: we're done. Just use y as idx, and rest is ok */
	    *mod_glen = y; 
	    return new_idx; 
	}
	if(y-z > new_idx) { 
	    /* Try to increase new_idx to fit y, up to maximum size */
	    while(++new_idx < LOG2_PT_SZ_MAX) {
		if(y-z == new_idx) { 
		    /* Cool! Extended y successfully */
		    *mod_glen = y; 
		    return new_idx; 
		}
	    }

	    /* Here we have |y| > max table size. */
	    eprintf("heuristic: y > max table size. Not implemented!\n");
	    eprintf("gva is %p, sz=%x, glen=%x\n", gva, sz, glen);
	    eprintf("npages=%d, new_idx=%d (new_ptab size is %d)\n", 
		    npages, new_idx, 1<<new_idx);
	    eprintf("xlen=%d, ylen=%d, zlen=%d\n", xlen, ylen, zlen);
	    eprintf("x=%d, y=%d, z=%d\n", x, y, z);
	    eprintf("XXX impl this part of heuristic!\n");
	    ntsc_halt();
	} else { /* need all y + some (or all) z */
	    HTRC(eprintf("new_idx > ylen => need all y + some/all z\n"));
	    /* grow new_idx rightwards from start of z */
	    while(--z >= glen) {
		if(y-z == new_idx) {
		    /* seems ok; now check z and x are valid */
		    if((z-glen) && (z-glen < LOG2_PT_SZ_MIN)) {
			eprintf("After exending idx, |z| is too small\n");
			eprintf("gva is %p, sz=%d, glen=%d\n", gva, sz, glen);
			eprintf("npages=%d, new_idx=%d (new_ptab sz is %d)\n", 
				npages, new_idx, 1<<new_idx);
			eprintf("xlen=%d, ylen=%d, zlen=%d\n", 
				xlen, ylen, zlen);
			eprintf("x=%d, y=%d, z=%d\n", x, y, z);
			ntsc_halt();
		    }

		    if((x-y) && (x-y < LOG2_PT_SZ_MIN)) {
			eprintf("Modified guard=%x is too small!\n", x-y);
			eprintf("gva is %p, sz=%x, glen=%x\n", gva, sz, glen);
			ntsc_halt();
		    } 
		    new_sz = y - new_idx; 
		    if(new_sz - pwidth < LOG2_PT_SZ_MIN) {
			HTRC(eprintf("Trouble: trailing guard only %d bits\n", 
				     new_sz - pwidth));

			/* Try to grow it first */
			if(y - pwidth <= LOG2_PT_SZ_MAX) {
			    eprintf("hueristic: grow new_idx rightwards.\n");
			    eprintf("not yet implemented.\n");
			    ntsc_halt();
			} 

			/* Try to shrink it */
			if(new_idx > LOG2_PT_SZ_MIN) {
			    while(--new_idx >= LOG2_PT_SZ_MIN) {
				if(++new_sz - pwidth >= LOG2_PT_SZ_MIN) {
				    *mod_glen = y; 
				    return new_idx; 
				}
			    }
			}
			
			eprintf("GPT heuristic() -  impossible situation.");
			eprintF("Dying.\n");
			ntsc_halt();
		    }
		    *mod_glen = y; 
		    return new_idx; 
		}
	    }
	    eprintf("GPT heuristic() -  impossible situation. Dying.\n");
	    ntsc_halt();
	}
	ntsc_halt();
    } else {
	HTRC(eprintf("Need to grow past y + z; stealing bits from x!\n"));
	while(++y <= x) {
	    if(y == (glen + new_idx)) { /* ok, cool */
		HTRC(eprintf("Cool: got enough bits, y now %d\n", y));
		/* check if |x| is >= LOG2_PT_SZ_MIN */
		if(x-y < LOG2_PT_SZ_MIN) 
		    goto idx_is_all; 
		*mod_glen = y;
		return new_idx; 
	    }
	} 
idx_is_all:
	new_idx = x - glen; 
	if(new_idx < LOG2_PT_SZ_MIN || new_idx > LOG2_PT_SZ_MAX) {
	    eprintf("heuristic: fatal! cannot sort new_idx growth.\n");
	    ntsc_halt();
	}
	*mod_glen = sz; /* no guard in the current level */
	return new_idx; 
    }
}



/*
** Recursive auxillary function used to add a new entry into the 
** guarded page table.
**
**  "ptr"     is a pointer to the current page table 
**  "vaddr"   is the (remaining) virtual address to be added
**  "pwidth"  is the page width of this region (currently always PAGE_WIDTH)
**  "npages"  is a hint about the likely number of consequtive pages to be 
**            added; we currently only add one at a time, but may extend 
**            this to do more later. For now, we use npages as part of 
**            the heuristic which chooses new page-table sizes on a split.
**  "new_pte" is the new pte. Tricky, huh.
**  "mbz"     is the number of bits (from lhs) we know to be zero in 
**            any guards at this level. 
*/
static gpte_t *add_aux(MMU_st *st, word_t ptr, word_t vaddr, 
		       uint32_t pwidth, uint32_t npages, 
		       hwpte_t *new_pte, int mbz)
{
    gpte_t *cur, *new_tab;
    word_t gvaddr, new_ptr, new_grd;
    uint32_t glen, new_idx, mod_glen;
    uint32_t i, idx, sz, new_sz;

    XTRC(eprintf("Entered add_aux: ptr is %x, vaddr is %p, mbz is %d.\n", 
	    ptr, (addr_t)vaddr, mbz));
    
    sz   = PTR_SIZE(ptr);
    idx  = (vaddr >> sz);
    cur  = (gpte_t *)SUPERPAGE(PTR_VAL(ptr)) + idx;
    glen = PTR_SIZE(cur->guard);

#ifdef SANITY_CHECK
    {
	bool_t die= False;

	/* 
	** Two quick sanity checks; 
	**   i)  the leftmost mbz bits of the guard must be zero.
	**  ii)  the guard must cover at least mbz + log2(table_size) bits.
	*/

	if(mbz && ((cur->guard >> (WORD_LEN - mbz)) != 0)) {
	    eprintf("add_aux: guard doesn't have mbz=%d zero leading bits.\n", 
		    mbz);
	    die = True;
	}

	if(glen > sz) {
	    eprintf("add_aux: guard len %d too short (should be >= %d)\n", 
		    WORD_LEN-glen, WORD_LEN-sz);
	    eprintf(" -- mbz is %d, log2 table_size is %d.\n", 
		    mbz, WORD_LEN-sz-mbz);
	    die= True; 
	}

	if(die) {
	    eprintf("add_aux error: ptr is %x, vaddr is %p, mbz is %d.\n", 
		    ptr, (addr_t)vaddr, mbz);
	    eprintf("Dumping bad GPTE and dying....\n");
	    dump_gpte(cur, " BAD GRD: ", idx, True);
	    ntsc_halt();
	}
    }
#endif /* SANITY_CHECK */


    if(IS_NULL_GPTE(cur)) {

	/* We're in luck! An empty spot just where we want it */

	{
	    word_t ipl = ntsc_swpipl(IPL_K_HIGH);
	    /* 
	    ** Need to have a guard to match however many bits are left. 
	    ** We use the passed in page width so that we can deal with
	    ** 'large' pages (viz. from ghints).
	    */
	    cur->guard  = vaddr & ~((1UL << pwidth) - 1UL);
	    cur->guard |= pwidth;
	    cur->guard |= GRD_M_LEAF; 
	    if(pwidth == WIDTH_4GB)  /* use 'software' superpage (4gigs) */
		cur->guard |= GRD_M_SPAGE; 
	    SET_PTE(cur->pointer.hwpte, *new_pte);
	    ntsc_swpipl(ipl);
	}

	return cur;
    } 

    /* If we get here, we've not hit an empty spot, so we have a valid 
       GPTE pointed to by 'cur'. We need to "guard" the virtual address. */
    gvaddr = vaddr ^ cur->guard;
    if((gvaddr >> glen) == 0) { 

	XTRC(eprintf("Matched guard: gvaddr is %lx\n", gvaddr));
	
	if(GRD_LEAF(cur->guard)) { /* We've found an existing one */
	    /* 
	    ** XXX SMH: add_aux/add_entry are only used when dealing 
	    ** with non-existing mappings. Hence if we get here, 
	    ** we've got an error. 
	    */
	    eprintf("add_aux: hit existing entry. death.\n");
	    ntsc_halt();
	} else 
	    return(add_aux(st, cur->pointer.ptr, gvaddr, pwidth, npages, 
			   new_pte, WORD_LEN-glen));
    }


    /* 
    ** If we get here, we've hit a guard fault. This is the tricky case, 
    ** For now, we simply add a new page table below, but there are 
    ** probably (I intuit) better ways of doing this. 
    */

    STRC(eprintf("add_aux: grd fault, have %d 'effective' bits to use.\n",
	    sz - PTR_SIZE(cur->guard)));
    STRC(dump_gpte(cur, " FAULT : ", idx, True));

    /* 
    ** The guard we faulted on (cur->guard) looks something like this:
    **
    **                        <-----          SZ    ----->
    **                              <---     GLEN     --->
    **  +----------------+---+-----+------------+-+-+----+
    **  |000..........000|IDX|GBITS|000......000|L|V|GLEN|
    **  +----------------+---+-----+------------+-+-+----+
    **  <---   MBZ   ---> 
    **
    ** IDX is m bits wide, where m is log2 of the current page table size.
    ** It is a 'must-match' part of the guard.
    ** GBITS is at least 1 bit wide, and could be much more. We know that
    ** |GBITS] >= 1, since if it were not, we'd not be able to fault.
    **
    ** We wish to split the guard into two parts; one on this level, 
    ** and one on the next level. Since the next level guard will 
    ** have to look rather similar, we need to consider IDX', i.e.
    ** the must-match part of the next level.
    ** This is an important quantity since |GBITS| >= |IDX'| == new_idx,
    ** where new_idx is the log2 of the size of the new page table.
    **
    ** Choosing a good one of these is potentially hard, so we 
    ** encapsulate it out into a heuristic() function. This tells
    ** us not only new_idx, but also mod_glen, the length of the
    ** shortened guard we will place in cur->guard.
    */

    new_idx = heuristic(sz, glen, pwidth, npages, gvaddr, &mod_glen); 
    new_sz  = mod_glen - new_idx; 
#ifdef SANITY_CHECK
    if(new_sz < glen) {
	eprintf("add_aux: new table will decode more bits than now.\n");
	eprintf("i.e. new_sz is %d, glen is (was) %d\n", new_sz, glen);
	ntsc_halt();
    }
#endif

    /* Get a new page table of the relevant size */
    new_ptr = (word_t)pt_alloc(st, new_idx);
    new_tab = (gpte_t *)SUPERPAGE(new_ptr);


    /* Critical section below. */
    {
	word_t ipl = ntsc_swpipl(IPL_K_HIGH);

	/* 
	** Sort out the part of the current entry in the new table; it will 
	** have a guard length of "glen", and the same pointer/pte field as
	** is here. However it will have more mbz bits cleared on the lhs. 
	*/
	new_grd    = PTR_VAL(cur->guard);  /* preserve 'L' and 'V' bits */
	cur->guard = GRD_VAL(cur->guard);  /* zap 'L' and 'V' bits      */


	/* Clean the original guard, and update its guard length */
	for(i = glen; i < mod_glen; i++)
	    cur->guard &= ~(1UL<<i);
	cur->guard |= mod_glen;
	
	/* Sort out the next level guard; zap from mbz to new_sz */
	for(i = (WORD_LEN - mbz); i >= mod_glen; i--)
	    new_grd &= ~(1UL<<i);
	new_grd |= glen;
	
	
	/* insert the new guard */
	i= new_grd >> new_sz; 
	new_tab[i].guard       = new_grd;
	new_tab[i].pointer.ptr = cur->pointer.ptr;
	STRC(eprintf("add_aux: after split; moved entry (index %d) is:\n", i));
	STRC(dump_gpte(&(new_tab[i]), " MOVED ", i, True));
	
	/* insert the new entry */
	vaddr ^= cur->guard;
	i = vaddr >> new_sz;
	new_tab[i].guard  = (vaddr & ~((1UL<<pwidth) - 1UL)) | pwidth;
	new_tab[i].guard |= GRD_M_LEAF; 
	if(pwidth == WIDTH_4GB) 
	    new_tab[i].guard |= GRD_M_SPAGE;
	
	SET_PTE(new_tab[i].pointer.hwpte, *new_pte);
	STRC(eprintf("add_aux: after split; ~new~ entry (index %d) is:\n", i));
	STRC(dump_gpte(&(new_tab[i]), " ~NEW~ ", i, True));
    
	/* insert a pointer to this new table */
	cur->pointer.ptr = new_ptr | new_sz; 

	ntsc_swpipl(ipl);
    }

    return (&(new_tab[i]));
}





static gpte_t *add_entry(MMU_st *st, uint16_t pwidth, word_t vaddr, 
			 uint32_t n, hwpte_t *new_pte)
{
    gpte_t *retval= (gpte_t *)NULL;

    XTRC(eprintf("add_entry: vpn=%x (addr=%p), n=%x\n", 
	    vaddr >> PAGE_WIDTH, vaddr, n));

    /* SMH: it would be better to be able to add multiple in one go */
    
    retval= add_aux(st, st->vptbr, vaddr, pwidth, n, new_pte, 0);

    return retval;
}


/*
** Recursive auxillary function used to modify a number of  entries in the 
** guarded page table.
**
**  "ptr"     is a pointer to the current page table 
**  "vaddr"   is the (remaining) virtual address to be added
**  "pwidth"  is the page width of this region (currently always PAGE_WIDTH)
**  "npages"  is the number of pages consequtively after this one to be added.
**  "new_pte" is the new pte. Tricky, huh.
**  "mbz"     is the number of bits (from lhs) we know to be zero in 
**            any guards at this level. 
** Returns the number of pages successfully modified, or zero in the case
** of any error.
*/
static uint32_t update_aux(MMU_st *st, word_t ptr, word_t vaddr, 
			   uint32_t pwidth, uint32_t npages, 
			   hwpte_t *new_pte, int mbz)
{
    gpte_t *cur;
    word_t gvaddr;
    uint32_t glen;
    uint32_t i, idx, sz;
    uint32_t cents, max_idx;

    sz   = PTR_SIZE(ptr);
    idx  = (vaddr >> sz);
    cur  = (gpte_t *)SUPERPAGE(PTR_VAL(ptr)) + idx;
    glen = PTR_SIZE(cur->guard);

    if(IS_NULL_GPTE(cur)) {
	/* An empty spot => not present. Death */
	eprintf("update_aux: empty spot in table [TNV]. Death.\n");
	eprintf("BTW: cur at %p\n", cur);
	return 0;
    } 

    /* If we get here, we've not hit an empty spot, so we have a valid 
       GPTE pointed to by 'cur'. We need to "guard" the virtual address. */
    gvaddr = vaddr ^ cur->guard;

    if((gvaddr >> glen) == 0) { 

	XTRC(eprintf("Matched guard: gvaddr is %lx\n", gvaddr));
	
	if(GRD_LEAF(cur->guard)) { /* We've found it */

	    /* 
	    ** SMH: we want to try to update as many GPTEs as possible 
	    ** in this call; we should be able to deal with MIN(x,npages)
	    ** pages, where x is the number of consecutive leaf entries
	    ** in the current table following the current entry. 
	    */

	    /* 
	    ** Get the mnumber of entries remaining in this table; max_idx
	    ** is simply the upper bound on table indices relative to cur.
	    */
	    max_idx = (1UL<<(WORD_LEN-(PTR_SIZE(ptr)+mbz))) - idx;  
	    
	    /* Check how many consecutive entries are also leaves */
	    for(cents = 1; (cents < max_idx) && (cents < npages) && 
		    GRD_LEAF(cur[cents].guard); cents++) 
		;

	    /* 
	    ** cents holds the number of consecutive entries we can 
	    ** update this time. Note that cents is always >= 1 here.
	    */
	    TRC(eprintf("update_aux: doing %d consec pages [tot todo=%d]\n", 
			cents, npages));
	    {
		word_t ipl = ntsc_swpipl(IPL_K_HIGH);
		for(i=0; i < cents; i++) {
		    cur[i].pointer.hwpte.sid     = new_pte->sid; 
		    cur[i].pointer.hwpte.ctl.all = new_pte->ctl.all; 
		}
		ntsc_swpipl(ipl);
	    }

	    return cents;
	} else 
	    return(update_aux(st, cur->pointer.ptr, gvaddr, pwidth, npages, 
			   new_pte, WORD_LEN-glen));
    }


    /* If we get here, we've hit a guard fault. */
    eprintf("update_aux: guard fault [TNV]. Death.\n");
    eprintf("BTW: cur at %p\n", cur);
    return 0;
}

/*
** update_range() is used to modify a number of existing entries in 
** the guarded page table. Currently this update can only involve 
** the non-PFN part of a PTE - i.e. mappings cannot be changed with
** this routine [use ntsc_map instead]. 
** Will fix this [and add_entry -> add_range] by means of a function 
** pointer passed into aux function to determine the pte for a given 
** virtual address. 
**
** Returns the number of pages successfully modified, or zero in the case
** of any error.
*/
static uint32_t update_range(MMU_st *st, uint16_t pwidth, word_t vaddr, 
			     uint32_t n, hwpte_t *new_pte)
{
    XTRC(eprintf("update_range: vpn=%x (addr=%p), n=%x\n", 
	    vaddr >> PAGE_WIDTH, vaddr, n));

    if(n < 1) {
	eprintf("update_range: bad number of pages %d\n", n);
	return 0;
    }
    return update_aux(st, st->vptbr, vaddr, pwidth, n, new_pte, 0);
}

static bool_t free_entry(MMU_st *st, uint16_t pwidth, word_t vaddr)
{
    gpte_t *entry;

    if(!lookup_va(st->vptbr, vaddr, &entry, 32)) {
	eprintf("free_entry: failed to lookup vaddr=%p\n", vaddr);
	return False;
    }

    /* 
    ** XXX SMH: should actually 'free' the entry if possible; i.e. should 
    ** do a free_pt() if an entire page table is emptied. 
    */
    entry->pointer.hwpte.pfn     = NULL_PFN;
    entry->pointer.hwpte.sid     = SID_NULL;
    entry->pointer.hwpte.ctl.all = 0;

    return True;
}


/*
** Before we switch over to our own page table, we need to ensure that
** we have all the mappings already established in our table too. 
*/
static void enter_mappings(MMU_st *const st, Mem_PMem pmem, Mem_Map mmap)
{
    word_t   curva, curpa, chkpa, fsize, psize;
    uint32_t curnf;
    uint16_t gh, fwidth;
    hwpte_t  newpte;
    gpte_t  *entry, *chk; 
    int      i; 

    for(i=0; mmap[i].nframes; i++) {

	curnf = mmap[i].nframes; 
	curva = (word_t)mmap[i].vaddr;
	curpa = (word_t)mmap[i].paddr;

	newpte.ctl.all  = PTE_M_KWE | PTE_M_KRE | PTE_M_ASM | PTE_M_VALID; 
	newpte.sid      = SID_NULL;   /* since no stretches yet */
	fwidth          = mmap[i].frame_width;

	switch(fwidth) {
	case WIDTH_4GB:
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
	    if(!(entry = add_entry(st, fwidth, curva, curnf, &newpte))) {
		eprintf("enter_mappings: failed to add mapping %p->%p\n", 
			curva, curpa);
		ntsc_dbgstop();
	    }

#ifdef SANITY_CHECK	    
	    if(!GRD_LEAF(entry->guard)) {
		eprintf("Urk! entry for curva=%p is not even a leaf!\n", 
			curva);
		ntsc_halt();
	    }

	    chkpa = (word_t)entry->pointer.hwpte.pfn << FRAME_WIDTH;
	    if(chkpa != curva) {
		eprintf("urk! bogus translation curva=%p -> pa=%p\n", 
			curva, chkpa);
		eprintf("BTW: hwpte is: PFN=%lx, SID=%04x, flags=%04x\n", 
			entry->pointer.hwpte.pfn, entry->pointer.hwpte.sid, 
			entry->pointer.hwpte.ctl.all);
		ntsc_halt();
	    }

	    if(!lookup_va(st->vptbr, curva, &chk, 32)) {
		eprintf("Urk! Failed to lookup curva=%p\n", curva);
		ntsc_halt();
	    }

	    if(chk != entry) {
		eprintf("Urk! lookup of curva=%p returned wrong gpte\n", 
			curva);
		ntsc_halt();
	    }
#endif

	    curva += psize; 
	    curpa += fsize; 
	    curnf--;
	}
    }

#ifdef SANITY_CHECK
    for(i=0; mmap[i].nframes; i++) {

	curnf  = mmap[i].nframes; 
	curva  = (word_t)mmap[i].vaddr;
	curpa  = (word_t)mmap[i].paddr;
	fwidth = mmap[i].frame_width;

	switch(fwidth) {
	case WIDTH_4GB:
	    psize = Mb(4096);
	    break;

	case WIDTH_4MB:
	    psize = Mb(4);
	    break;
		
	case WIDTH_512K:
	    psize = Kb(512);
	    break;
	    
	case WIDTH_64K:
	    psize = Kb(64);
	    break;
	    
	case WIDTH_8K:
	    psize = Kb(8);
	    break;
	    
	default:
	    /* XXX SMH: this is *bad* so we just die for now. */
	    eprintf("enter_mappings, sanity check: bad frame width %d\n", 
		    fwidth);
	    ntsc_halt();
	    break;
	}
	
	fsize = psize; 

	while(curnf) {
	    
	    if(!lookup_va(st->vptbr, curva, &chk, 32)) {
		eprintf("Failed to lookup curva=%p, faulted at:\n", curva);
		dump_gpte(chk, " FLT ", 0, True);
		ntsc_halt();
	    }

	    if(!GRD_LEAF(chk->guard)) {
		eprintf("Urk! entry for curva=%p is not even a leaf!\n", 
			curva);
		ntsc_halt();
	    }

	    chkpa = (word_t)chk->pointer.hwpte.pfn << FRAME_WIDTH;
	    if(chkpa != curva) {
		eprintf("urk! bogus translation curva=%p -> pa=%p\n", 
			curva, chkpa);
		eprintf("BTW: hwpte is: PFN=%lx, SID=%04x, flags=%04x\n", 
			chk->pointer.hwpte.pfn, chk->pointer.hwpte.sid, 
			chk->pointer.hwpte.ctl.all);
		eprintf("and  guard is: %lx\n", chk->guard);
		ntsc_halt();
	    }

	    curva += psize; 
	    curpa += fsize; 
	    curnf--;
	}
    }
#endif
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

    /* Sort out the level one page table; it contains N_L1ENTS entries */
    for(i=0; i < N_L1ENTS; i++) {
	st->l1tab[i].guard             = 0x0UL;
	st->l1tab[i].pointer.ptr       = 0x0UL;
	st->l1tab[i].pointer.hwpte.pfn = NULL_PFN;
    }

    /* 
    ** Guarded page table "pointers" are encoded as: 
    **   [63:6] - physical address of ptab 
    **    [5:0] - page table size, "sz". 
    ** Since "sz" is only 6 bits, it represents the size 
    ** of the page table pointed to in the form 2^{64-sz}. 
    ** So for our l1 table, we want "sz" to be 64-log2(N_L1ENTS). 
    */
    st->pptbr  = (word_t)&(st->l1tab[0]) | (64 - LOG2_NL1ENTS);
    st->vptbr  = st->pptbr;  /* XXX pa == va at this point */
    

    /* 
    ** Initialise the page table memory (which is in chunks).
    ** It follows immediately after the state record.
    */

    init_ptabs(st, (word_t *)ROUNDUP((st + 1), FRAME_WIDTH), ptsz, True);


    /* Add mappings for the existing memory covered. */
    INITTRC(eprintf("Entering mappings for existing memory.\n"));
    enter_mappings(st, pmem, init);

#ifdef DUMP_TABLES
    dump_tables(st->vptbr, 0, 0);
#endif

    /* Swap over to our new page table; we also pass the pa of the pdom_ptbl */
    INITTRC(eprintf("MMUMod: setting new ptbr to va=%p, pa=%p\n", 
	    st->vptbr, st->pptbr));
    ntsc_wrptbr((addr_t)st->pptbr, ppdtbl_pa);
    ntsc_tbi(-2, 0);
    INITTRC(eprintf("+++ done new ptbr.\n"));

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
    ntsc_tbi(-2, 0);           /* invalidate all TB entries */
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
    gpte_t     *entry; 
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
    case WIDTH_4GB:
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

    while(curnp) {
	if(!(entry = add_entry(st, pwidth, curva, curnp, &pte))) {
	    eprintf("MMU$AddRange: failed to add entry for va %08x\n", curva);
	    ntsc_halt();
	    return; 
	}
	curva += psize; 
	curnp--;
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
    gpte_t     *entry; 
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
    case WIDTH_4GB:
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

	if(!(entry = add_entry(st, pwidth, curva, curnp, &pte))) {
	    eprintf("MMU$AddMappedRange: failed on va %08x -> pa %08x\n", 
		    curva, curpa);
	    ntsc_halt();
	    return; 
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
    case WIDTH_4GB:
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
	updated = update_range(st, pwidth, curva, curnp, &pte);
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
    uint32_t curnp, pwidth;
    word_t   curva;

    curnp  = range->npages; 
    pwidth = range->page_width; 
    curva  = (word_t)range->start_addr; 

    while(curnp--) 
	if(!free_entry(st, pwidth, curva))
	    eprintf("FreeRange: failed to free entry for va %08x\n", curva);
	else curva += pwidth; 
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
    gpte_t        *entry; 
    Stretch_Rights res; 
    uint16_t       ctl;

    res = Stretch_Rights_None; 
    if(!sst->s) return res; 

    if(!lookup_va(st->vptbr, sst->a, &entry, 32)) {
	eprintf("MMU$QueryGlobal: invalid str %p [TNV]\n", str);
	return res;
    }
    
    ctl = entry->pointer.hwpte.ctl.all;
    
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
