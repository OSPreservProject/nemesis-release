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
 **      Stretch allocation for the 'STD' memory system -- i.e. allocates
 **      physical memory under virtual by default. 
 ** 
 ** ENVIRONMENT: 
 **
 **      Nemesis Domain. Requires Frames.
 ** 
 ** ID : $Id: salloc_std.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <exceptions.h>
#include <kernel.h>
#include <ntsc.h>
#include <links.h>
#include <pip.h>
#include <stdio.h>

#include <autoconf/memctxts.h>
#ifdef CONFIG_MEMCTXTS
#include <contexts.h>
#endif 

#include <MMU.ih>
#include <Stretch.ih>
#include <SAllocMod.ih>
#include <StretchAllocator.ih>
#include <StretchAllocatorF.ih>

#include "mmu.h"

#define ETRC(_x) 

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

    /*We hang all of the allocated stretches here */
    struct link_t       stretches;
    Context_cl         *clnt_mem; 
}; 


/* Shared state */
struct SA_st {
    Frames_cl 	  *f;                  
    Heap_cl	  *h;
    MMU_cl        *mmu;
    uint32_t      *sids;               /* Pointer to table of SIDs in use */
    Stretch_clp   *ss_tab;             /* SID -> Stretch_clp mapping */
    Context_cl    *mem;
    Context_cl    *sysmemcx;
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

    /* SMH check for meta rights in done in ntsc/palcode */
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

    /* SMH check for meta rights in done in ntsc/palcode */
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

    TRC(eprintf("Stretch$Query(%p, %x) -> va=[%p,%p) [%c%c%c%c]\n", 
		self, pdid, st->a, st->a+st->s, 
		SET_IN(res, Stretch_Right_Meta)    ? 'M' : '-',
		SET_IN(res, Stretch_Right_Execute) ? 'E' : '-',
		SET_IN(res, Stretch_Right_Write)   ? 'W' : '-',
		SET_IN(res, Stretch_Right_Read)    ? 'R' : '-'));
    
    return res;
}


static StretchAllocator_New_fn            NewStretch_m;
static StretchAllocator_NewList_fn        NewList_m;
static StretchAllocator_NewAt_fn          NewAt_m;
static StretchAllocator_Clone_fn          Clone_m;
static StretchAllocator_Destroy_fn        DestroyAllocator_m;
static StretchAllocator_DestroyStretch_fn DestroyStretch_m;
static StretchAllocator_op sa_op = { 
    NewStretch_m, 
    NewList_m, 
    NewAt_m,
    Clone_m,
    DestroyStretch_m,
    DestroyAllocator_m
};

/*
 * StretchAllocatorF Method Suite
 */
static StretchAllocatorF_NewClient_fn 	NewClient_m;
static StretchAllocatorF_op saf_op = { 
    NewStretch_m, 
    NewList_m, 
    NewAt_m,
    Clone_m,
    DestroyStretch_m,
    DestroyAllocator_m,
    NewClient_m,
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




#define BYTES_TO_PAGES(_x,_pw) (ROUNDUP((word_t)(_x), (_pw)) >> (_pw))

/*
** Util: generic stretch creatie wobbly. 
*/
static Stretch_st *CreateStretch(SA_st *st, addr_t base, 
				 word_t size, Stretch_Rights gaxs)
{
    Stretch_st *res;
    Mem_VMemDesc vmem;
    Mem_PMemDesc pmem;

    res = Heap$Malloc(st->h, sizeof(*res) );
    
    if(!res) {
      DB(eprintf("CreateStretch: Malloc failed.\n"));
      return (Stretch_st *)NULL;
    }

    /* Initalise the closure */
    CL_INIT(res->cl, &s_op, res);

    /* Setup the MMU related fields */
    res->global  = Stretch_Rights_None;
    res->sid     = AllocSID(st);
    res->mmu     = st->mmu;

    RegisterSID(st, res->sid, &res->cl);

    /* Setup the virtual memory descriptor; assume PAGE_WIDTH for now */
    vmem.start_addr = base; 
    vmem.npages     = BYTES_TO_PAGES(size, PAGE_WIDTH); 
    vmem.page_width = PAGE_WIDTH;
    vmem.attr       = 0;

    if(size) {
	/* If have non-zero size, then add a mapped (121!) range */

	/* Determine the physical memory properties */
	pmem.start_addr  = vmem.start_addr;
	pmem.frame_width = Frames$Query(st->f, pmem.start_addr, &pmem.attr);
	pmem.nframes     = BYTES_TO_LFRAMES(size, pmem.frame_width);

	/* Set page width/npages according to the physical memory fields */
	vmem.npages     = pmem.nframes; 
	vmem.page_width = pmem.frame_width;

	/* And go and add the range */
	MMU$AddMappedRange(st->mmu, &res->cl, &vmem, &pmem, gaxs);

    } else 
	/* Otherwise, just add an unmapped range */ 
	MMU$AddRange(st->mmu, &res->cl, &vmem, gaxs);
    

    /* Fill in the rest of the fields */
    res->a  = base; 
    res->s  = vmem.npages << vmem.page_width; 

    return res;
}

/*--StretchAllocator methods-----------------------------------------------*/


/*
 * Create a single stretch
 */
static Stretch_clp NewStretch_m(StretchAllocator_cl *self, Stretch_Size size,
				Stretch_Rights global)
{
    Client_st *cst = (Client_st *)(self->st); 
    SA_st *st = cst->sa_st;
    uint32_t mode = 0;
    Stretch_st *s;
    addr_t base; 

    ETRC(eprintf("StretchAllocator$New: VP=%p called with size %x\n",
		 cst->vp, size));
    
    if(size) 
	base = Frames$Alloc(st->f, size, FRAME_WIDTH);
    else 
	base = NO_ADDRESS; 

    if(!(s = CreateStretch(st, base, size, global))) {
	eprintf("StretchAllocator$New: CreateStretch failed.\n");
	ntsc_halt();
    }

#ifdef CONFIG_MEMCTXTS
    if(cst->vp) {
	char sidname[8];

	sprintf(sidname, "%04x", s->sid); 
	CX_ADD_IN_CX(st->mem, sidname, (IDC_Buffer *)&s->a, IDC_Buffer);
	CX_ADD_IN_CX(cst->clnt_mem, sidname, (IDC_Buffer *)&s->a, IDC_Buffer);
    }
#endif    

    s->sa = self;
    ETRC(eprintf("StretchAllocator$New: allocated [%lx-%lx], SID=%x\n", 
		 s->a, s->a + s->s, s->sid));

    /* Default access rights are user r/w/m, parent m */

    if(cst->pdid != NULL_PDID) {
	/* pdid will be NULL during some parts of the startup phase */
	ETRC(eprintf("StretchAllocator$New: setting prot RW-M pdid %x.\n", 
		     cst->pdid));
	MMU$SetProt(st->mmu, cst->pdid, &s->cl,
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) | 
		    SET_ELEM(Stretch_Right_Meta));
	if(cst->parent != NULL_PDID) {
	    ETRC(eprintf("+++ setting parent prot ---M pdid %x.\n", 
			 cst->parent));
	    MMU$SetProt(st->mmu, cst->parent, &s->cl,
			SET_ELEM(Stretch_Right_Meta));
	}
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
    Stretch_Size *szp;
    uint32_t i, num, mode=0;
    uint64_t totsize; 
    uint8_t *chunk;

    /* First work out how many stretches we need. */
    num = SEQ_LEN (sizes);
    ETRC(eprintf("StretchAllocator$NewList: VP=%p called with %x sizes\n", 
		 cst->vp, num));


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
    
    totsize = 0;

    for (i=0; i<num; i++) {
	int orgsize, size;
	
	orgsize = size = SEQ_ELEM(sizes,i);
	size = ROUNDUP(size, FRAME_WIDTH);
	totsize += size;
    }
    chunk = Frames$Alloc(st->f, totsize, FRAME_WIDTH);
    if (!chunk) {
	eprintf("Out of memory in StretchP$NewList: "
		"wanted %x, didn't get it\n");
        RAISE_StretchAllocator$Failure();
    }
    totsize = 0; /* now, an index to the start of unallocated bytes in chunk */

    for(i=0, szp= SEQ_START(sizes); (i<num) && (szp < SEQ_END(sizes));
	szp++, i++) {
	
	s = sbase[i] = CreateStretch(st, chunk + totsize, *szp, global);

#ifdef CONFIG_MEMCTXTS
	if(cst->vp) {
	    char sidname[8];

	    sprintf(sidname, "%04x", s->sid); 
	    CX_ADD_IN_CX(st->mem, sidname, (IDC_Buffer *)&s->a, 
			 IDC_Buffer);
	    CX_ADD_IN_CX(cst->clnt_mem, sidname, (IDC_Buffer *)&s->a, 
			 IDC_Buffer);
	}
#endif    
	if(!s) {
	    eprintf("StretchAllocator$NewList: CreateStretch failed.\n");
	    ntsc_halt();
	}
	s->sa = self;
	ETRC(eprintf("StretchAllocator$NewList: allocd [%lx-%lx], SID=%x\n", 
		     s->a, s->a + s->s, s->sid));
	
	totsize += ROUNDUP(*szp, FRAME_WIDTH);
	
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
    Mem_VMemDesc myvmem;
    Mem_PMemDesc mypmem;
    uint32_t mode = 0;
    Stretch_st *s;

    ETRC(eprintf("StretchAllocator$NewAt called: VP=%p, start=%p, size=%lx\n", 
		 cst->vp, start, size));
    
    /* 
    ** XXX SMH: in this old memsys, we need to be a bit careful since 
    ** we can't handle non 121 mappings. Hence if we have a given 
    ** start address (not ANY_ADDRESS), we require that either pmem
    ** is NULL, or that pmem->start_addr == start. 
    */
    if(pmem != NULL) {

	/* We've been passed in some memory => don't need to alloc it */

	if(start != ANY_ADDRESS) {
	    /* Got both pmem *and* a start address: they need to be the same */
	    if(pmem->start_addr != start) {
		eprintf("StretchAllocator$NewAt: start address conflict.\n");
		eprintf("[121 memsys requested start va=%p, start pa=%p\n", 
			start, pmem->start_addr); 
		ntsc_halt();
	    }

	    /* Otherwise check pmem covers (exactly) the vm range */
	    size = ROUNDUP(size, pmem->frame_width);
	    if((pmem->nframes << pmem->frame_width) != size) {
		eprintf("StretchAllocator$NewAt: start => pmem not onto.\n");
		eprintf("size (rounded) is %lx; pmem 'extent' is %lx\n", 
			size, pmem->nframes << pmem->frame_width);
		ntsc_halt();
	    } 
	    ETRC(eprintf("StretchAllocator$NewAt: rounded up size to %lx\n", 
			 size));

	} else {

	    /* Only got pmem, so treat that as requested "start" address */
	    size = ROUNDUP(size, pmem->frame_width);
	}

    } else {
	/* No pmem given, so alloc 'size' bytes @ 'start', default fwidth */
	mypmem.attr        = 0;
	mypmem.nframes     = ROUNDUP(size, FRAME_WIDTH) >> FRAME_WIDTH; 
	mypmem.frame_width = FRAME_WIDTH; 
	mypmem.start_addr  = Frames$AllocRange(st->f, size, FRAME_WIDTH, 
					       start, attr);
	pmem = &mypmem; 
    }

    /* Setup virtual memory descriptor from physical one */
    myvmem.start_addr = pmem->start_addr; 
    myvmem.npages     = pmem->nframes; 
    myvmem.page_width = pmem->frame_width; 
    myvmem.attr       = pmem->attr;
    
    if(!(s = Heap$Malloc(st->h, sizeof(*s)))) {
	eprintf("StretchAllocator$NewAt: failed to alloc stretch state.\n");
	ntsc_halt();
    }

    /* Initalise the closure */
    CL_INIT(s->cl, &s_op, s);

    /* Fill in the the fields */
    s->a       = myvmem.start_addr; 
    s->s       = size;
    s->global  = Stretch_Rights_None;
    s->sid     = AllocSID(st);
    s->mmu     = st->mmu;

    RegisterSID(st, s->sid, &s->cl);

    MMU$AddMappedRange(st->mmu, &s->cl, &myvmem, pmem, global);

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
    ETRC(eprintf("StetchAllocator$NewAt: returning stretch at %p\n", &s->cl));
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
    Stretch_st *tmpl_st, *s;
    Stretch_Rights gaxs; 
    addr_t base; 

    tmpl_st = (Stretch_st *)template->st;
    if(size == 0)  size = tmpl_st->s; 

    ETRC(eprintf("StretchAllocator$Clone: VP=%p called with size %x\n",
		 cst->vp, size));

    /* Grab some physical memory */
    base = Frames$Alloc(st->f, size, FRAME_WIDTH);

    /* Get the global rights from the template & use them for new stretch */
    gaxs = MMU$QueryGlobal(st->mmu, template); 
    if(!(s = CreateStretch(st, base, size, gaxs))) {
	eprintf("StretchAllocator$Clone: CreateStretch failed.\n");
	ntsc_halt();
    }

#ifdef CONFIG_MEMCTXTS
    if(cst->vp) {
	char sidname[8];

	sprintf(sidname, "%04x", s->sid); 

	CX_ADD_IN_CX(st->mem, sidname, (IDC_Buffer *)&s->a, IDC_Buffer);
	CX_ADD_IN_CX(cst->clnt_mem, sidname, (IDC_Buffer *)&s->a, IDC_Buffer);
    }
#endif    

    s->sa = self;
    ETRC(eprintf("StretchAllocator$Clone: allocated [%lx-%lx], SID=%x\n", 
		 s->a, s->a + s->s, s->sid));

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


static void DestroyAllocator_m(StretchAllocator_cl *self )
{
    Client_st *st = (Client_st *)(self->st);
    SA_st *sa_st  = st->sa_st;

    TRC(eprintf("StretchAllocator$DestroyAllocator\n"));
    /* XXX Race condition on the dequeueing - we can't use an ENTER
       around the whole lot, since DestroyStretch, when doing a
       Context$Remove for the memory context stuff, may need to do a
       MU_LOCK if we're using a locked heap, and that would cause an
       assertion failure in the paranoia code in the events
       module. Just hope that no-one tries to allocate while the
       allocator's being destroyed. This is pretty unlikely, since the
       owning domain should have been toasted by now. */
 
    /* Destroy all stretches */

    while (!LINK_EMPTY(&st->stretches)) {
      Stretch_st *str = (Stretch_st *)(st->stretches.next);
      StretchAllocator$DestroyStretch(self, &str->cl);
    }

    /* XXX SMH: if this salloc has any children, need to remove its 
       protection domain from their cst's. */

    /* Free state */
    Heap$Free(sa_st->h, st);
}

 
static void DestroyStretch_m(StretchAllocator_cl *self, Stretch_cl *str)
{
    Stretch_st *st = (Stretch_st *)(str->st);
    Client_st *cst = (Client_st *)(st->sa->st);
    SA_st   *sa_st = cst->sa_st;
    Mem_VMemDesc vmem;
    uint32_t NOCLOBBER mode = 0;

    TRC(eprintf("StretchAllocator$DestroyStretch [%qx]: str=%p\n", 
	    RO(cst->vp)->id, str));

#ifdef STRETCH_DEADBEEF
    {
	word_t *p = st->a;
	eprintf("%p %p\n", st->a, st->s);
	eprintf("DEADBEEF from %p to %p\n", st->a, st->a + st->s);
	while (p < (st->a + st->s)) *p++ = 0xdeadbeef;
    }
#endif

#ifdef CONFIG_MEMCTXTS

    /* Remove the stretch from the memctxt _before_ we destroy it, or
       someone else might get the same StretchID, and then we have a
       nasty race condition on the memory context.

       Also, if this is the data segment of a program in the image, it
       will have been registered in system memctx, rather than the
       client's memory context. Thus the Context$Remove might fail for
       cst->clnt_mem context, in which case it should be in
       sa_st->sysmemcx */

    if(cst->vp) {
	char sidname[8];

	sprintf(sidname, "%04x", st->sid); 
	TRY {
	    Context$Remove(cst->clnt_mem, sidname); 
	} CATCH_Context$NotFound(UNUSED name) {
	    Context$Remove(sa_st->sysmemcx, sidname);
	} ENDTRY;
	Context$Remove(sa_st->mem, sidname); 
    }
#endif    

    /* SMH: lightweight synch.- requires no IDC between ENTER/LEAVE */
    ENTER(Pvs(vp), mode);

    /* Remove it from our list */
    LINK_REMOVE(&st->link);

    /* Invalidate the vm range */
    vmem.start_addr = st->a; 
    vmem.npages     = BYTES_TO_PAGES(st->s, PAGE_WIDTH); 
    vmem.page_width = PAGE_WIDTH; 
    vmem.attr       = 0;
    MMU$FreeRange(st->mmu, &vmem);

    /* And free up the memory underneath */
    if(st->a != NO_ADDRESS) Frames$Free(sa_st->f, st->a, st->s );

    /* Now free the misc other per-stretch data */
    FreeSID(sa_st, st->sid);

    LEAVE(Pvs(vp), mode);

    /* And, finally, free the state */
    Heap$Free(sa_st->h, st);

    return;
}






/*--StretchAllocatorF methods ----------------------------------------------*/


static StretchAllocator_clp 
NewClient_m (StretchAllocatorF_clp self,  /* IN */
	     ProtectionDomain_ID   pdid,
	     VP_clp                vp,
	     ProtectionDomain_ID   parent_pdid)
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
	new->parent = (parent_pdid == pdid) ? NULL_PDID: parent_pdid;
    }

    LINK_INITIALISE (&new->stretches);

#ifdef CONFIG_MEMCTXTS
    {
	Context_cl *domains, *domcx; 
	char domname[32];

	sprintf(domname, "%qx", VP$DomainID(vp));
	domains = NAME_FIND("proc>domains", Context_clp); 
	domcx   = NAME_LOOKUP(domname, domains, Context_clp); 
	new->clnt_mem = CX_NEW_IN_CX(domcx, "mem");
    }
#endif


    CL_INIT( new->cl, &sa_op, new );

    sa = &new->cl;
    ETRC(eprintf("StretchAllocator$NewClient: returning %p\n", sa));
    return sa;
}


/*--SAllocMod methods ----------------------------------------------------*/

static StretchAllocatorF_cl *NewF_m(SAllocMod_cl   *self,
				    HeapMod_clp     hmod,
				    Frames_clp      frames,
				    MMU_clp         mmu,
				    Mem_VMem        regions
				    /* RETURNS */ )
{
    StretchAllocatorF_clp sa;
    Mem_VMemDesc vmem;
    Mem_PMemDesc pmem;
    Stretch_st heapst; 
    Client_st *cst;
    Heap_clp heap; 
    addr_t hbase; 
    word_t hsize; 
    SA_st *st;
    int i;

    ETRC(eprintf("SAllocMod$NewF: called.\n"));

    /* First of all we need to create ourselves a heap */
    /* XXX SMH: just hardwire 256K for now. Should be a param? or config? */
    hsize = 256 << 10;  
    hbase = Frames$Alloc(frames, hsize, PAGE_WIDTH);

    /* Setup the virtual memory descriptor */
    vmem.start_addr = hbase; 
    vmem.attr       = 0;

    /* Determine the physical memory properties */
    pmem.start_addr  = vmem.start_addr;
    pmem.frame_width = Frames$Query(frames, pmem.start_addr, &pmem.attr);
    pmem.nframes     = BYTES_TO_LFRAMES(hsize, pmem.frame_width);

    /* Set page width/npages according to the physical memory fields */
    vmem.npages     = pmem.nframes; 
    vmem.page_width = pmem.frame_width;
	
    /* Fake out a stretch state to keep the MMU happy */

    /* Initalise the closure */
    CL_INIT(heapst.cl, &s_op, &heapst);

    /* Setup the MMU related fields */
    heapst.global  = SET_ELEM(Stretch_Right_Read);
    heapst.sid     = SID_NULL;

    MMU$AddMappedRange(mmu, &heapst.cl, &vmem, &pmem, 
		       SET_ELEM(Stretch_Right_Read)); 
    heap  = HeapMod$NewRaw(hmod, hbase, hsize); 

    /* Ok; got our heap, now we can get our state sorted. */
    cst = (Client_st *)Heap$Malloc(heap, sizeof(Client_st) + sizeof(SA_st));
  
    if (!cst ) {
	DB(eprintf("StretchAllocatorF$New: Malloc failed.\n"));
	RAISE_Heap$NoMemory();
	return (StretchAllocatorF_clp)0;
    }

    /* Fill in the server state */
    st = (SA_st *)(cst + 1);
    st->f   = frames;
    st->h   = heap;
    st->f   = frames;
    st->mmu = mmu;
    LINK_INITIALISE(&st->clients);

    /* Allocate space for the sid allocation table */
    if(!(st->sids = Heap$Malloc(heap, SID_ARRAY_SZ * sizeof(uint32_t)))) {
	eprintf("SAllocMod$NewF: failed to alloc space for sidtab.\n");
	ntsc_halt();
    }

    /* Intialise the sid allocation table */
    for(i=0; i<SID_ARRAY_SZ; i++) 
	st->sids[i] = 0;

    /* Allocate space for the sid->stretch mapping */
    if(!(st->ss_tab = Heap$Malloc(heap, SID_MAX * sizeof(Stretch_cl *)))) {
	eprintf("SAllocMod$NewF: failed to alloc space for ss_tab.\n");
	ntsc_halt();
    }

    /* Initialise the table to NULL */
    for(i=0; i<SID_MAX; i++) 
	st->ss_tab[i] = NULL;

    /* Poke it info the INFO_PAGE */
    INFO_PAGE.ss_tab = st->ss_tab; 

    /* Client state */
    CL_INIT( cst->cl, (StretchAllocator_op *)&saf_op, cst );
    cst->sa_st  = st;
    cst->vp     = NULL;
    cst->pdid   = NULL_PDID;
    cst->parent = NULL_PDID;

    LINK_INITIALISE (&cst->stretches);


    /* Keep a record of this 'client' */
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
#ifdef CONFIG_MEMCTXTS
    SA_st   *sa_st = cst->sa_st;  
    Context_cl *proc, *domains, *domcx;
    Stretch_st *cur_st; 
    struct link_t *cur, *headp; 
    char domname[32];
    char sidname[8];
    word_t rsvd; 
#endif

    ETRC(eprintf("SAllocMod$Done: vp=%p, pdid=%p\n", vp, pdid));
    if(cst->vp != NULL)
	eprintf("Urk! SAllocMod$Done(): vp was non NULL (was %p)\n", cst->vp);
    
    cst->vp   = vp;
    cst->pdid = pdid;

#ifdef CONFIG_MEMCTXTS
    sprintf(domname, "%qx", VP$DomainID(vp));
    proc          = NAME_FIND("proc", Context_clp); 
    sa_st->mem    = CX_NEW_IN_CX(proc, "meminfo"); 
    sa_st->sysmemcx      = CX_NEW_IN_CX(sa_st->mem, "rsvd"); 
    domains       = NAME_LOOKUP("domains", proc, Context_clp); 
    domcx         = NAME_LOOKUP(domname, domains, Context_clp); 
    cst->clnt_mem = CX_NEW_IN_CX(domcx, "mem");

    rsvd  = 0;
    headp = &cst->stretches; 
    for(cur = headp->next; cur != headp; cur = cur->next) {
	cur_st = (Stretch_st *)cur; 
	sprintf(sidname, "%04x", cur_st->sid); 
	CX_ADD_IN_CX(sa_st->mem, sidname, (IDC_Buffer *)&cur_st->a, 
		     IDC_Buffer);
	CX_ADD_IN_CX(sa_st->sysmemcx, sidname, (IDC_Buffer *)&cur_st->a, 
		     IDC_Buffer);
	rsvd += cur_st->s; 
    }
    CX_ADD_IN_CX(sa_st->sysmemcx, "total", rsvd, word_t); 
#endif    
    
    return;
}
