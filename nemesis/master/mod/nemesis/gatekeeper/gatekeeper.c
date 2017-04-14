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
**      gatekeeper.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements Gatekeepers,
** 
** ENVIRONMENT: 
**
**      User space. Pervasive Exceptions required.
** 
** ID : $Id: gatekeeper.c 1.2 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
** 
**
*/


#include <nemesis.h>
#include <stdio.h>
#include <contexts.h>
#include <frames.h>
#include <mutex.h>
#include <exceptions.h>
#include <salloc.h>

#include <VPMacros.h>           /* SMH: for ProtDom only */

#include <GatekeeperMod.ih>
#include <LongCardTblMod.ih>

#include <autoconf/memsys.h>
#include <autoconf/heap_paranoia.h>

/* XXX Turning on tracing in the gatekeeper is not for mortals. 
   It is part of the IDC mechanism which normal printf relies upon. 
   eprintf requires kernel privileges to work before IDC is running */
/*#define GATEKEEPER_TRACE*/
#ifdef GATEKEEPER_TRACE
#define DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif

#if CONFIG_HEAP_PARANOIA_VALUE > 0
#define SET_BLOCK_RA(_node) ((((addr_t *)(_node))[-2]) = RA())
#else
#define SET_BLOCK_RA(_node)
#endif

/*
 * Module stuff 
 */
static GatekeeperMod_New_fn		New_m;
static GatekeeperMod_NewPrivate_fn	NewPrivate_m;
static GatekeeperMod_NewGlobal_fn	NewGlobal_m;
static GatekeeperMod_NewSimple_fn	NewSimple_m;
static GatekeeperMod_op gm_op = {
    New_m,
    NewPrivate_m, 
    NewGlobal_m, 
    NewSimple_m 
};
static const GatekeeperMod_cl gm_cl = { &gm_op, (addr_t)0 };
CL_EXPORT(GatekeeperMod,GatekeeperMod,gm_cl);



/*
 * Gatekeeper (simple) methods
 */
static Gatekeeper_GetHeap_fn 		SimpleGetHeap_m;
static Gatekeeper_GetStretch_fn         SimpleGetStretch_m;
static Gatekeeper_op simple_gk_op = { 
    SimpleGetHeap_m, 
    SimpleGetStretch_m 
};

/*
 * Reference counted heap methods
 */

static  Heap_Malloc_fn          GkHeap_Malloc_m;
static  Heap_Free_fn            GkHeap_Free_m;
static  Heap_Calloc_fn          GkHeap_Calloc_m;
static  Heap_Realloc_fn         GkHeap_Realloc_m;
static  Heap_Purge_fn           GkHeap_Purge_m;
static  Heap_Destroy_fn         GkHeap_Destroy_m;
static  Heap_Check_fn           GkHeap_Check_m;
static  Heap_Query_fn           GkHeap_Query_m;

static  Heap_op GkHeap_ms = {
    GkHeap_Malloc_m,
    GkHeap_Free_m,
    GkHeap_Calloc_m,
    GkHeap_Realloc_m,
    GkHeap_Purge_m,
    GkHeap_Destroy_m,
    GkHeap_Check_m,
    GkHeap_Query_m
};

/*
 * State record for real gatekeeper
 */
typedef struct real_gk_st real_gk_st;
struct real_gk_st {
    struct _Gatekeeper_cl cl;	  /* Closure */
    StretchAllocator_clp  salloc; /* How this pdom/dom can get stretches  */
    HeapMod_clp           hmod;   /* The thing to use to make our heaps   */
    LongCardTbl_clp       lctbl;  /* Used to map pdoms to heaps           */
    Frames_clp            frames;
    mutex_t               mu;
};

/*
 * State record for reference-counted heap
 */

typedef struct _GkHeap_st {

    /* Used by simple and real gkprs */
    Heap_cl              cl;
    bool_t               destroyable;
    Heap_clp             heap;
    mutex_t              mu;
    
    /* Used by real gkpr */
    real_gk_st          *gk_st;
    int                  refCount;
    Stretch_clp          str;
    Stretch_Size         size; 
    Stretch_Rights       access; 
    ProtectionDomain_ID  pdid;
} GkHeap_st;

/*
 * State record for simple gatekeeper
 */
typedef struct simple_gk_st simple_gk_st;
struct simple_gk_st {
    Gatekeeper_cl          cl;	        /* Closure 			*/
    Heap_clp               theheap;	/* Heap to return		*/
    Heap_clp               myheap;	/* Heap state comes from	*/
    Stretch_clp            stretch;	/* Stretch used for heap	*/
    ProtectionDomain_ID    pdid;	
    GkHeap_st              heap_st;     /* Simple heap state wrapper    */
};


/*
** `Real' Gatekeeper methods 
*/
static Gatekeeper_GetHeap_fn 		RealGetHeap_m;
static Gatekeeper_GetStretch_fn 	RealGetStretch_m;
static Gatekeeper_op real_gk_op = { 
    RealGetHeap_m, 
    RealGetStretch_m 
};


/*---GatekeeperMod-methods-----------------------------------------------*/

/*
 * Create a new, general Gatekeeper
 */
static Gatekeeper_clp New_m(GatekeeperMod_cl 	*self,
			    StretchAllocator_clp sa,
			    HeapMod_clp		 hm,
			    Heap_clp		 h,
			    Frames_clp           f)
{
    LongCardTblMod_clp lctmod;
    real_gk_st *st;

    TRC(eprintf("GatekeeperMod$New: called.\n"));

    /* Alloc our own state on the heap passed in */
    if (!(st=Heap$Malloc(h, sizeof(*st)))) {
	eprintf("GatekeeperMod$New: failed to malloc state\n");
	RAISE_Heap$NoMemory();
    }

    CL_INIT (st->cl, &real_gk_op, st);
    st->salloc= sa;
    st->hmod  = hm;
    
    /* We use the heap passed in to alloc space for our lctable too */
    lctmod= NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);
    st->lctbl= LongCardTblMod$New(lctmod, h);
    st->frames=f;

    MU_INIT(&st->mu);

    /* Is that it? */
    
    TRC(eprintf("GatekeeperMod$New: done (gk at %p).\n", &st->cl));
    return &st->cl;
}

/*
 * Return a gatekeeper for the specified pdom using the stretch provided.
 */
static Gatekeeper_clp NewPrivate_m(GatekeeperMod_cl	*self,
				   Stretch_clp	s,
				   ProtectionDomain_ID  pdid,
				   HeapMod_clp	hm,
				   Heap_clp	h )
{
#ifdef GATEKEEPER_TRACE
    Stretch_Size size; addr_t a;
#endif
    Heap_clp 	 newheap;
    simple_gk_st *st;
    
    TRC(a = STR_RANGE(s,&size);
	eprintf("GatekeeperMod$NewGlobal: stretch at %p + %lx\n",
		a, size));
    
    /* Create a new heap within the stretch. */
    TRC(eprintf("GatekeeperMod$NewGlobal: creating heap.\n"));
    newheap = HeapMod$New(hm,s);
    
    /* Create some state */
    if (!(st=Heap$Malloc(h,sizeof(*st)))) {
	
	/* We don't need to clean up the newheap since it's state is */
	/* entirely within the stretch. */
	DB(eprintf("GatekeeperMod$NewGlobal: failed to malloc state\n"));
	RAISE_Heap$NoMemory();
    }
    
    st->theheap = newheap;
    st->stretch = s;
    st->pdid    = pdid;
    st->myheap  = h;
    CL_INIT (st->cl,&simple_gk_op,st);
    
    CL_INIT (st->heap_st.cl, &GkHeap_ms, &st->heap_st);
    st->heap_st.heap = newheap;
    st->heap_st.destroyable = False;
    MU_INIT(&st->heap_st.mu);

    TRC(eprintf("GatekeeperMod$NewGlobal: done.\n"));
    return &st->cl;
}

/*
 * Return a gatekeeper to run in a single stretch.
 */
static Gatekeeper_clp NewGlobal_m(GatekeeperMod_cl	*self,
				  Stretch_clp	s,
				  HeapMod_clp	hm,
				  Heap_clp	h )
{
#ifdef GATEKEEPER_TRACE
    Stretch_Size size; addr_t a;
#endif
    Heap_clp 	 newheap;
    simple_gk_st *st;
    
    TRC(a = STR_RANGE(s,&size);
	eprintf("GatekeeperMod$NewGlobal: stretch at %p + %lx\n",
		a, size));
    
    /* Try to map the stretch globally read-only */
    TRC(eprintf("GatekeeperMod$NewGlobal: mapping stretch.\n"));
    STR_SETGLOBAL(s, SET_ELEM(Stretch_Right_Read) );
    
    /* Create a new heap within the stretch. */
    TRC(eprintf("GatekeeperMod$NewGlobal: creating heap.\n"));
    newheap = HeapMod$New(hm,s);
    
    /* Create some state */
    if (!(st=Heap$Malloc(h,sizeof(*st)))) {
	
	/* We don't need to clean up the newheap since it's state is */
	/* entirely within the stretch. */
	DB(eprintf("GatekeeperMod$NewGlobal: failed to malloc state\n"));
	RAISE_Heap$NoMemory();
    }
    
    st->theheap = newheap;
    st->stretch = s;
    st->pdid    = NULL_PDID;	/* Use this to mean global */
    st->myheap  = h;
    CL_INIT (st->cl,&simple_gk_op,st);
    
    CL_INIT (st->heap_st.cl, &GkHeap_ms, &st->heap_st);
    st->heap_st.heap = newheap;
    st->heap_st.destroyable = False;
    MU_INIT(&st->heap_st.mu);

    TRC(eprintf("GatekeeperMod$NewGlobal: done.\n"));
    return &st->cl;
}

/*
 * Run a trivial gatekeeper 
 */
static Gatekeeper_clp NewSimple_m(GatekeeperMod_cl *self,
				  Heap_clp	h )
{
    simple_gk_st *st;
    
    TRC(eprintf("GatekeeperMod$NewSimple: called.\n"));
    
    /* Create some state */
    if (!(st=Heap$Malloc(h,sizeof(*st)))) {
	
	/* We don't need to clean up the newheap since it's state is */
	/* entirely within the stretch. */
	DB(eprintf("GatekeeperMod$NewGlobal: failed to malloc state\n"));
	RAISE_Heap$NoMemory();
    }
    
    st->theheap = h;
    st->stretch = NULL;
    st->pdid    = NULL_PDID;	/* Use this to mean global */
    st->myheap  = h;
    CL_INIT(st->cl,&simple_gk_op,st);
    
    CL_INIT (st->heap_st.cl, &GkHeap_ms, &st->heap_st);
    st->heap_st.heap = h;
    st->heap_st.destroyable = False;
    MU_INIT(&st->heap_st.mu);

    /* done. */
    TRC(eprintf("GatekeeperMod$NewGlobal: done.\n"));
    return &st->cl;
}

				  
/*---Gatekeeper (simple) methods------------------------------------------*/

/*
** Get a heap readable and/or writable by the given protection domain. 
** In this implementation (simple) we have exactly one heap, and hence
** cannot handle certain options. 
*/
static Heap_clp SimpleGetHeap_m (Gatekeeper_cl *self, 
				 ProtectionDomain_ID pdid, 
				 Stretch_Size size, 
				 Stretch_Rights access, 
				 bool_t cache)
{
    simple_gk_st *st = self->st;
    
    TRC(eprintf("Gatekeeper$GetHeap: acquire heap for pdid %x\n", pdid));

    if(!cache) {
	/* Cannot create a new heap; we only have the one. So die. */
	RAISE_Gatekeeper$Failure();
    }
    
    if (st->pdid != NULL_PDID && (st->pdid != pdid)) {
	/* Cannot create a new heap for pdom "pdid"; So die. */
	RAISE_Gatekeeper$Failure();
    }

    if(access != SET_ELEM(Stretch_Right_Read)) {
	/* Can't chmod the heap either; its read only, so die. */
	RAISE_Gatekeeper$Failure();
    }

    return &st->heap_st.cl;
}

/*
 * XXX Unimplemented; shared stretch from the simple gatekeeper.
 */
static Stretch_clp 
SimpleGetStretch_m(Gatekeeper_cl *self, ProtectionDomain_ID pdid,
		   Stretch_Size size, Stretch_Rights access, 
		   uint32_t awidth, uint32_t pwidth)
{
    eprintf("Urk! Trying to call GetStretch() on a simple gatekeeper! die!\n");

    RAISE_Gatekeeper$Failure();

    return (Stretch_cl *)NULL;  /* Keep gcc happy. */
}


/*---Gatekeeper (real) methods---------------------------------------------*/

#define DEF_GKHEAP_SIZE 16384 /* default heap size */
#define DEF_GKSTR_SIZE  65536 /* default stretch size */


/* 
** GetHeap(): 
**   returns a heap readable and/or writable  by the given protection domain.
**   If "cache" is True, this \emph{may} be a 'used one' (i.e. one we 
**   created in an earlier call). It is expected that such second-hand 
**   heaps will be empty (or practically empty) if returned.
**   Otherwise a new heap will be created, and mapped with the appropriate
**   rights into "pdid". 
*/   
static Heap_clp RealGetHeap_m(Gatekeeper_cl *self, ProtectionDomain_ID pdid, 
			      Stretch_Size size, Stretch_Rights access, 
			      bool_t cache)
{
    real_gk_st *st= self->st;
    GkHeap_st *heap_st;
    Stretch_Size sz; 

    TRC(eprintf("Gatekeeper$GetHeap(%p)(real): acquire heap for pdid %qx\n",
		self,pdid));

    if(cache) {

	/* check if we have a heap already for this pdid */

	MU_LOCK(&st->mu);
	if(LongCardTbl$Get(st->lctbl, (word_t)pdid, (addr_t *)&heap_st)) {

	    /*
	    ** XXX SMH: we can currently only cache exactly one heap per
	    ** pdom; hence if the size or the access rights are different
	    ** from one we created earlier, we lose. 
	    */
	    if((!size || size == heap_st->size) && access == heap_st->access) {
		TRC(eprintf("Gatekeeper$GetHeap(%p): returning old heap "
			    "at %p\n", self, &heap_st->cl));
		
		heap_st->refCount++;
		MU_RELEASE(&st->mu);
		return &heap_st->cl;

	    } else 
		/* We can't cache the new heap coz only one per pdom */
		cache = False; 
	}
	MU_RELEASE(&st->mu);

    }

    /* If here, need to create a new heap & map it accordingly */
    if(!(heap_st = Heap$Malloc(HEAP_OF(st), sizeof(*heap_st)))) {
	eprintf("Gatekeeper: couldn't malloc\n");
	RAISE_Gatekeeper$Failure();
    }

    sz = (size == 0) ? DEF_GKHEAP_SIZE : size; 
    heap_st->str = STR_NEW_SALLOC(st->salloc, sz);

    /* 
    ** If the pdom requiring "access" is not ours (as will be the
    ** case almost all the time), then grant the appropriate access.  
    ** If the pdom _is_ ours, then we already have RWM access since we 
    ** created the stretch. 
    */
    if (pdid != VP$ProtDomID(Pvs(vp))) {
	SALLOC_SETPROT(st->salloc, heap_st->str, pdid, access);
    }

    /* Create the new heap */
    CL_INIT(heap_st->cl, &GkHeap_ms, heap_st);
    heap_st->heap        = HeapMod$New(st->hmod, heap_st->str);
    heap_st->refCount    = 1;
    heap_st->size        = sz; 
    heap_st->access      = access; 
    heap_st->pdid        = pdid;
    heap_st->gk_st       = st;
    heap_st->destroyable = True;
    MU_INIT(&heap_st->mu);


    if(cache) {
	/* Store the heap for future reference. */
	MU_LOCK(&st->mu);
	LongCardTbl$Put(st->lctbl, (word_t)pdid, (addr_t)heap_st);
	MU_RELEASE(&st->mu);
    }

    /* Return the new heap */
    TRC(eprintf("Gatekeeper$GetHeap(%p)(real): returning new heap at %p\n", 
		self, &heap_st->cl));
    return &heap_st->cl;
}


/* 
** GetStretch(): 
**   returns a stretch of the desired "size" (or larger) which has the 
**   access rights "access" rights for the protection domain "pdid". 
**   The base of the stretch will be aligned to "awidth", and it 
**   should be mapped contiguously according to "pwidth". 
**   Such areas of memory may be used as shared buffers for, 
**   e.g., IO channels between domains.
*/   
static Stretch_clp 
RealGetStretch_m(Gatekeeper_cl *self, ProtectionDomain_ID pdid,
		   Stretch_Size size, Stretch_Rights access, 
		   uint32_t awidth, uint32_t pwidth)
{
    real_gk_st *st= self->st;
#ifdef CONFIG_MEMSYS_EXPT
    StretchDriver_cl *sdriver;
    Mem_PMemDesc pmem; 
#endif
    Stretch_clp str;
    Stretch_Size sz;

    TRC(eprintf("Gatekeeper$GetStretch: acquire stretch for pdid %x, sz %d\n", 
	       pdid, size));
    sz = size ? size : DEF_GKSTR_SIZE;

    /* Create a new stretch & map it accordingly */
#ifdef CONFIG_MEMSYS_EXPT
    
    /* XXX SMH: Use special shared sdriver here */
    sdriver = Pvs(sdriver); 

    /* Get some physical memory */
    if((pmem.start_addr = Frames$Alloc(st->frames, sz, awidth)) == NO_ADDRESS)
	RAISE_Gatekeeper$Failure();
    TRC(eprintf("Gatekeeper$GetStretch: got pmem (%x bytes at %p)\n", sz, 
		pmem.start_addr));
    pmem.frame_width = Frames$Query(st->frames, pmem.start_addr, &pmem.attr);
    pmem.nframes     = BYTES_TO_LFRAMES(sz, pmem.frame_width);

    str = StretchAllocator$NewAt(st->salloc, sz, 0, ANY_ADDRESS, 0, &pmem); 
    StretchDriver$Bind(sdriver, str, pwidth);

#else

    /* Just ignore awidth and pwidth */
    str = STR_NEW_SALLOC(st->salloc, sz);

#endif


    /* 
    ** If the pdom requiring "access" is not ours (as will be the
    ** case almost all the time), then grant the appropriate access.  
    ** If the pdom _is_ ours, then we already have RWM access since we 
    ** created the stretch. 
    */
    if (pdid != VP$ProtDomID(Pvs(vp))) 
	SALLOC_SETPROT(st->salloc, str, pdid, access);


    /* Return the new stretch */
    TRC(eprintf("Gatekeeper$GetStretch[real]: returning new str at %p\n", str));
    return str;
}

static Heap_Ptr GkHeap_Malloc_m (
        Heap_cl *self,
        Heap_Size       size    /* IN */ )
{
    GkHeap_st       *st = self->st;
    Heap_Ptr p;
    MU_LOCK(&st->mu);
    p = Heap$Malloc(st->heap, size);
    if(p) {
	HEAP_OF(p) = self;
	SET_BLOCK_RA(p);
    }
    MU_RELEASE(&st->mu);
    return p;
}

static void GkHeap_Free_m (
        Heap_cl *self,
        Heap_Ptr        p       /* IN */ )
{
    GkHeap_st *st = self->st;
    MU_LOCK(&st->mu);
    if(p) HEAP_OF(p) = st->heap;
    Heap$Free(st->heap, p);
    MU_RELEASE(&st->mu);
}

static Heap_Ptr GkHeap_Calloc_m (
        Heap_cl *self,
        Heap_Size       nmemb   /* IN */,
        Heap_Size       size    /* IN */ )
{
    GkHeap_st       *st = self->st;
    Heap_Ptr p =  Heap$Calloc(st->heap, nmemb, size);
    if(p) {
	HEAP_OF(p) = self;
	SET_BLOCK_RA(p);
    }
    return p;
}

static Heap_Ptr GkHeap_Realloc_m (
        Heap_cl *self,
        Heap_Ptr        p       /* IN */,
        Heap_Size       size    /* IN */ )
{
    GkHeap_st       *st = self->st;
    
    MU_LOCK(&st->mu);
    if(p) HEAP_OF(p) = st->heap;
    p = Heap$Realloc(st->heap, p, size);
    if(p) HEAP_OF(p) = self;
    MU_RELEASE(&st->mu);
    return p;
}

static Heap_clp GkHeap_Purge_m (
        Heap_cl *self )
{
    GkHeap_st       *st = self->st;
    fprintf(stderr, "GkHeap(%p) - Purge requested! (Ignoring)\n", self);
    return self;
}

static void GkHeap_Destroy_m (
        Heap_cl *self )
{
    GkHeap_st       *st = self->st;
    addr_t        tmp;

    if(st->destroyable) {
	st->refCount--;
	if(!st->refCount) {
	    TRC(eprintf("Gkpr: Destroying heap\n"));
	    MU_LOCK(&st->gk_st->mu);
	    LongCardTbl$Delete(st->gk_st->lctbl, (word_t) st->pdid, &tmp);
	    Heap$Destroy(st->heap);
	    MU_RELEASE(&st->gk_st->mu);
	    FREE(st);
	} else {
//	    eprintf("Gkpr: Not destroying stretch\n");
	}
    } 

}

static void GkHeap_Check_m (
    Heap_cl *self, bool_t checkFreeBlocks )
{
    GkHeap_st       *st = self->st;
    Heap$Check(st->heap, checkFreeBlocks);
}

static bool_t GkHeap_Query_m (
    Heap_cl *self, Heap_Statistic kind, /* RETURNS */ uint64_t *value) {
    return False;
}
