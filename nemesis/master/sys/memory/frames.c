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
** Implementation of FramesMod / FramesF / Frames interfaces
** 
** FUNCTIONAL DESCRIPTION:
** 
**  Creates a Frames interface to manage a given set of regions.
**
** ENVIRONMENT: 
**
**  Anywhere except PAL code.
** 
** ID : $Id: frames.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
** Throughout this file we have two sorts of 'frames': 
** 
**    - physical frames
**    - logical frames
** 
** A *physical* frame is a piece of physical memory of size 
** FRAME_SIZE [which is more or less guaranteed to be the same 
** as PAGE_SIZE]. Equivalently one may consider the size of a physical
** frame to be 2^{FRAME_WIDTH}, which is the normal manner in which 
** it is expressed in this file as it happens to be more convenient.
** In summary: a physical frame is the analog of a normal "page" in 
** the virtual address space. 
** 
** A *logical* frame, on the other hand, is a naturally aligned 
** piece of physical memory of size 2^{FRAME_WIDTH + k}, where k >= 0.
** It is the "real" (i.e. internal) unit of allocation within this
** file, and different regions may have different logical frame sizes. 
** In particular, IO space tends to be allocated (and accounted) at
** a far coarser granularity than FRAME_SIZE as this would waste a 
** large amount of space. 
** Additionally, a client may request (via Alloc/AllocRange) a number 
** of bytes with a given "frame width" --- this is used to constrain
** alignment and rounding ('bytes' will be rounded up). It also means
** that the allocated memory will be accounted [internally] as a 
** logical frame of the appropriate width. Similarly, when freeing 
** at a particular start address, the 'bytes' will be rounded up 
** to the frame width with which they were allocated. Note that it 
** is possible for an allocation logical frame width (ALFW) to be
** greater than the region logical frame width (RLFW). This can 
** be somewhat confusing, but it all works really. Trust me. 
** In summary: a logical frame is (roughly) the analog of "superpage"
** in the virtual address space. 
**
*/

#include <nemesis.h>
#include <frames.h>
#include <links.h>
#include <ntsc.h>
#include <dcb.h>

#include <FramesMod.ih>

#ifdef FRAMES_TRACE
#define ETRC(_x) _x
#define TRC(_x) _x
#define DEBUG
#else
#define TRC(_x)
#define ETRC(_x)
#endif
    
#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif
    
#define MTRC(_x)
    
/*
 * Module stuff 
 */
static const FramesMod_Reqd_fn	        Reqd_m;
static const FramesMod_New_fn	        New_m;
static const FramesMod_Done_fn	        Done_m;
static FramesMod_op FramesMod_ms = { 
    Reqd_m,
    New_m,
    Done_m,
};

static const FramesMod_cl fmod_cl = { &FramesMod_ms, (addr_t)0xabbadeadUL };
CL_EXPORT(FramesMod, FramesMod, fmod_cl);
CL_EXPORT_TO_PRIMAL(FramesMod, FramesModCl, fmod_cl);    
    
static Frames_Alloc_fn      Alloc_m;
static Frames_AllocRange_fn AllocRange_m;
static Frames_Query_fn      Query_m;
static Frames_Free_fn       Free_m;
static Frames_Destroy_fn    Destroy_m;

static Frames_op frames_ms = { 
    Alloc_m, 
    AllocRange_m, 
    Query_m, 
    Free_m,
    Destroy_m,
};


static FramesF_NewClient_fn NewClient_m;
static FramesF_AddFrames_fn AddFrames_m;
static FramesF_op framesF_ms = { 
    Alloc_m, 
    AllocRange_m, 
    Query_m, 
    Free_m,
    Destroy_m,
    NewClient_m, 
    AddFrames_m,
};

/* 
 * Data Structures
 */

typedef struct _FramesClient_st FramesClient_st; /* per-client state       */
typedef struct _FramesMod_st    FramesMod_st;    /* shared state (regions) */
typedef struct _Frame           Frame;           /* per frame alloc info   */


struct _FramesClient_st {
    Frames_cl     cl;           /* The closure itself (may be a FramesF one)*/

    dcb_ro_t     *rop;          /* Virtual address of client DCBRO          */

    flink_t      *headp;        /* List of frames allocated to this client  */
    uint32_t      npf;          /* No. of physical frames allocated         */

    uint32_t      owner;        /* Either phys or virt addr of client dcb   */
    uint32_t      gtdf;         /* High water mark on guaranteed frames     */
    uint32_t      xtraf;        /*   "" on xtra frames; xtraf >= gtdf       */

    Heap_cl      *heap;         /* Heap for allocating things with          */
    FramesMod_st *fst;          /* Back pointer to shared state             */
};

/*
** The Frames Mod state is a little tricky due to the fact that we
** don't have a heap around when we create the first one. Hence we
** need to be a bit careful with our memory layout.
** The layout is roughly as follows:
**
**   ---------------------
**   |    Client State   |   Special 'client state' for the System.
**   ---------------------
**   |    Start Ptr      |   First frame of first region (region 0)
**   +-------------------+   
**   |  Number of Frames |   The number of logical frames in this region.
**   +-------------------+   
**   |    Frame Size     |   Size of frames in this region (expressed as width)
**   +-------------------+   
**   | Frame Attributes  |   Any special attrs of frames in this region.
**   +-------------------+   
**   |   Ram Table       |   Pointer to the RamTab iface, or NULL. 
**   +-------------------+
**   |     Next Ptr      |   Pointer to next region information.
**   +-------------------+  
**   |                   |
**   .                   .  
**   .                   .   A bunch of Frame structures for this region.
**   .                   .  
**   |                   |
**   +-------------------+
**   |   Start Ptr       |   First frame of second region (region 1)
**   +-------------------+
**   |                   |   ... etc  (as for first region)
**
**
** Regions of non-standard memory (e.g. IO space regions, or ROM ones)
** have a NULL pointer to the frame table since it is not used for 
** their ownership information.
** 
** The last region has a 'Next Ptr' field of NULL.
*/

struct _FramesMod_st {
    addr_t        start;  /* pointer to first frame of this region (aligned) */
    word_t        nlf;    /* number of logical frames in this region         */
    uint32_t      fwidth; /* width (in bits) of the frames in this region    */
    uint32_t      attrs;  /* attributes (memory, DMAable, ROM, etc.)         */
    RamTab_cl    *ramtab; /* pointer to the frame table, or NULL             */
    FramesMod_st *next;   /* pointer to next part of state, or NULL          */
    Frame        *frames; /* frame structures for this region                */
};


/* 
** Frame structure; this has evolved over the years and now contains 
** only a single field. On most architectures/compilers this will 
** not be stupidly padded I hope, so the space overhead is the same
** while having slightly nicer syntactic looks.
*/
struct _Frame {
    uint32_t   free;   /* 0= used, 1= free, 2=free & next free, etc */
};
    

/* 
** Macros
*/

#define UNALIGNED(_x)            ((word_t)_x & 1)
#define RIDX(_st)                ((word_t)(_st)->start >> FRAME_WIDTH)
#define FADDR(_st,_idx)          (((_st)->start + ((_idx) << (_st)->fwidth)))
#define IS_RAM(_attr)            (!SET_IN(_attr, Mem_Attr_ReadOnly) &&	\
				  !SET_IN(_attr, Mem_Attr_NonMemory)) 

#undef FILL_MEM /* define if want mem filled with 'bogus' pointers */

/*
** Utility functions for:
**   - determining which region a particular address falls within, 
**   - allocating 'n' physical frames at an arbitrary (physical) address,
**   - allocating 'n' physical frames at a particular address.
**   - updating the list of physical memory regions owned by a client
**
*/


/*
** get_region(): a utility function which returns a pointer to
** the 'state' of whatever region the address addr is in, or NULL
** if addr is not in any of the regions which we manage.
*/
static FramesMod_st *get_region(FramesMod_st *st, addr_t addr)
{
    FramesMod_st *res= st; 

    while(res) {
	if((addr >= res->start) && 
	   addr < (res->start + (res->nlf << res->fwidth)))
	    break;
	res = res->next;
    }
    
    return res;
}


/* 
** PM_AllocAny(): 
**   - allocates 'npf' contiguous physical frames (i.e frames of 
**     size FRAME_SIZE) aligned to (at least) the width in 'align'.
**     NB: we expect 'align' to be >= FRAME_WIDTH.
** Returns 'flf', the first *logical* frame allocated in the
** region, and 'nlf' the number of *logical* frames allocated.
*/
static FramesMod_st *PM_AllocAny(Frames_cl *self, uint32_t npf, 
				 uint32_t align, word_t *flf, word_t *nlf)
{
    FramesClient_st *cst = (FramesClient_st *) self->st;
    register FramesMod_st *st = cst->fst;
    register FramesMod_st *cur_st = st;
    
    TRC(eprintf("PM_AllocAny: %d frames requested.\n", npf));
    while(cur_st) {

	if(!cur_st->attrs) {
	    
	    /* Translate from npf = number of frames of size FRAME_SIZE reqd
	       into *nlf = number of frames of size 1 << cur_st->fwidth */
	    *nlf = npf >> (cur_st->fwidth - FRAME_WIDTH);

	    /* 
	    ** XXX SMH: don't handle non standard frame width within 
	    ** region currently. Should consider the:
	    **    FRAME_WIDTH <-> align <-> cur_st->fwidth 
	    ** relationship, and try to come up with something 
	    ** sensible. 
	    */
	    if(*nlf != npf) {
		eprintf("URK! region with non std frame width\n");
		ntsc_dbgstop();
	    }
	    
	    /* 
	    ** Find a large enough place in frames: we need "npf"
	    ** contiguous physical frames starting aligned to "align".
	    */
	    for (*flf=0; *flf < cur_st->nlf; (*flf)++) {
		if(cur_st->frames[*flf].free  >= *nlf &&
		   ALIGNED(FADDR(cur_st,*flf),align))
		    return cur_st;
	    }
	} 
	
	/* No luck in this region; move to next one */
	cur_st = cur_st->next;
    }
    
    /* Oh dear --- out of physical memory. */
    return NULL;
}




/* 
** PM_AllocRange(): allocates a (contiguous) set of frames.
**     -- 'npf' is the number of physical frames (i.e. frames 
**         of size FRAME_SIZE) which are requested, 
**     -- 'start' describes the required start address; we are 
**         guaranteed that it is aligned correctly. 
** Returns 'flf', the first *logical* frame allocated in this
** region, and 'nlf', the number of *logical* frames allocated.
*/
static FramesMod_st *PM_AllocRange(Frames_cl *self, uint32_t npf, 
				   addr_t start, word_t *flf, word_t *nlf)
{
    FramesClient_st *cst = (FramesClient_st *) self->st;
    register FramesMod_st *st = cst->fst;
    register FramesMod_st *cur_st = st;
    uint32_t fshift; 

    /* have requested start address, so find appropriate region */
    ETRC(eprintf("PM_AllocRange: start address of %p requested.\n", start));
    
    if((cur_st = get_region(st, start)) == NULL) {
	/* requested address not in any of our regions */
	eprintf("PM_AllocRange: urk! we don't own req. address %p.\n", 
		start);
	ntsc_halt();	
    }

    /* Translate from npf = number of physical frames (i.e. frames
       of size FRAME_SIZE) required into *nlf = number of logical frames 
       required (i.e. frames of size (1 << cur_st->fwidth) ) */
    fshift = cur_st->fwidth - FRAME_WIDTH; /* >= 0 */
    *nlf   = ROUNDUP(npf, fshift) >> fshift;
    *flf   = BYTES_TO_LFRAMES(start - cur_st->start, cur_st->fwidth);
    
    if(cur_st->frames[*flf].free < *nlf) {
	/* not enough space at requested address: give as much as possible */
	MTRC(eprintf("PM_AllocRange: not %d bytes free @ req. address %p.\n", 
		     (*nlf << cur_st->fwidth), start));
	*nlf = cur_st->frames[*flf].free;
	MTRC(eprintf("Returning suggested (max) allocation of %d frames\n", 
		     *nlf));
	return cur_st;
    } 
    
    ETRC(eprintf("PM_AllocRange: allocated %lx bytes at %p.\n", 
		 (*nlf << cur_st->fwidth), start));
    return cur_st;
}

static bool_t add_range(FramesClient_st *cst, addr_t base, 
			uint32_t npf, uint32_t frame_width)
{
    flink_t *link;
    flist_t *cur, *new, *next; 
    word_t end, cbase, cend, nbase, nend; 
    bool_t mergelhs, mergerhs; 

    if(!cst->heap || !cst->headp || !npf) 
	return True;   /* we're happy enough */

    ETRC(eprintf("add_range: base=%p, npf=%d, fwidth=%d\n", base, npf, 
	    frame_width));

    /* Compute the end address of the new region */
    end  = (word_t)base + (npf << FRAME_WIDTH);

    /* Try to find the correct place to insert it */
    for(link = cst->headp->next; link != cst->headp; link = link->next) {

	cur = (flist_t *)link; 

	cbase = (word_t)cur->base; 
	cend  = cbase + (cur->npf << FRAME_WIDTH);
	nbase = (word_t)((flist_t *)link->next)->base; 
	if(((word_t)base >= cend) && (end <= nbase))
	    break;
    }

    if(link == cst->headp) { 

	/* We wrapped => no elem prior to us. Check if we can merge on rhs */
	next  = (flist_t *)cst->headp->next; 
	cbase = (word_t)next->base; 

	if(end == cbase) {

	    /* Ok: can merge onto the current head of the list */
	    next->npf += npf; 
	    next->base = base; 
	    return True; 

	} else {
	    
	    /* Otherwise, need new element at head */
	    if((new = Heap$Malloc(cst->heap, sizeof(*new))) == NULL)
		return False;
	
	    new->base    = base; 
	    new->npf     = npf; 
	    new->fwidth  = frame_width; 
	
	    LINK_ADD_TO_HEAD(cst->headp, &new->link);
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
    mergelhs = ((cur->fwidth == frame_width) && ((word_t)base == cend));
    
    /* Now check if we could merge on the rhs too */
    if(link->next != cst->headp) {
	next      = (flist_t *)link->next; 
	nbase     = (word_t)next->base; 
	mergerhs  = ((next->fwidth == frame_width) && (nbase == end));
    } else {
	/* would wrap, so cannot merge on rhs*/
	next  = (flist_t *)NULL;    /* stop GDB complaining at us */
	nbase = 0xf00d;             /*  "" */
	mergerhs = False;
    }

    if(!mergelhs && !mergerhs) {
	/* Cannot merge => need a new list element (after 'link'=='cur') */
	
	if((new = Heap$Malloc(cst->heap, sizeof(*new))) == NULL)
	    return False;
	
	new->base    = base; 
	new->npf     = npf; 
	new->fwidth  = frame_width; 
	
	LINK_INSERT_AFTER(link, &new->link);
	return True;
    }


    if(mergelhs) {
	MTRC(eprintf("add_range: coalesce current [%p-%p) & new [%p-%p)\n", 
		     cbase, cend, base, end));
	cur->npf += npf; 
	cend      = cbase + (cur->npf << FRAME_WIDTH);
	MTRC(eprintf("        i.e region is now at [%p-%p)\n", 
		     cbase, cend));
	

	if(mergerhs) {
	    MTRC(eprintf("add_range: plus can merge on rhs..\n"));
	    cur->npf += next->npf; 
	    cend      = cbase + (cur->npf << FRAME_WIDTH);
	    MTRC(eprintf("        i.e region is now at [%p-%p)\n", 
			 cbase, cend));

	    /* Now free 'next' */
	    LINK_REMOVE(link->next);
	    Heap$Free(cst->heap, next);
	} 
	return True;
    } 

    /* If get here, we can merge on rhs, but *not* of lhs */
    nend = nbase + (next->npf << FRAME_WIDTH);
    MTRC(eprintf("add_range: coalesce new [%p-%p) and next [%p-%p)\n", 
		 base, end, nbase, nend));
    next->npf += npf; 
    next->base = base; 
    nbase      = (word_t)next->base;
    nend       = nbase + (next->npf << FRAME_WIDTH);
    MTRC(eprintf("        i.e region is now at [%p-%p)\n", 
		 nbase, nend));
    return True;
}

static bool_t del_range(FramesClient_st *cst, addr_t base, 
			uint32_t npf, uint32_t frame_width)
{
    flink_t *link;
    flist_t *cur, *new;
    word_t end, cbase, cend;

    if(!cst->heap || !cst->headp || !npf) 
	return True;   /* we're happy enough */
    
    /* Compute the end address of the new region */
    end  = (word_t)base + (npf << FRAME_WIDTH);
    ETRC(eprintf("del_range: base=%p, end=%p npf=%d, fwidth=%d\n", 
	    base, end, npf, frame_width));


    /* 
    ** Find the region containing addresses [base,end) 
    ** There should be exactly one of these, since we have coalesced
    ** adjacent regions in add_range above. 
    */
    for(link = cst->headp->next; link != cst->headp; link = link->next) {

	cur = (flist_t *)link; 

	cbase = (word_t)cur->base; 
	cend  = cbase + (cur->npf << FRAME_WIDTH);

	if((cbase <= (word_t)base) && (end <= cend)) 
	    break;
    }

    if(link == cst->headp)  /* Urk! Didn't find it. In trouble now ... */
	return False; 

    /* Check if our region is exactly 'cur' */
    if(((word_t)base == cbase) && (end == cend)) {
	LINK_REMOVE(link); 
	Heap$Free(cst->heap, cur);
	return True;
    }

    /* If we touch either side of 'cur', just update it */
    if((word_t)base == cbase) {
	cur->base += (npf << FRAME_WIDTH);
	cur->npf  -= npf; 
	return True;
    } 

    if(end == cend) {
	cur->npf  -= npf; 
	return True;
    } 

    /* Otherwise, we need to split 'cur' down the middle. */
    if((new = Heap$Malloc(cst->heap, sizeof(*new))) == NULL)
	return False;

    new->base    = (addr_t)end;
    new->fwidth  = frame_width; 
    new->npf     = (cend - end) >> FRAME_WIDTH;
	
    cur->npf     = ((word_t)base - cbase) >> FRAME_WIDTH;

    LINK_INSERT_AFTER(link, &new->link);
    return True;
}
/*----------------------------------------- FramesMod methods -------------*/

/* Reqd_m() determines the amount of state New_m [below] will require */
static word_t Reqd_m(FramesMod_cl *self, Mem_PMem allmem)
{
    word_t nreg, nlf, res;

    /* Scan through the set of mem desc and count the number of
       logical frames they contain in total. */
    nlf = 0;
    for(nreg = 0; allmem[nreg].nframes != 0; nreg++) {

	ETRC(eprintf(" ++ region %d: start=%lx, end=%lx, fsize=%lx, nf=%lx\n", 
		     nreg, allmem[nreg].start_addr, 
		     allmem[nreg].start_addr + 
		     (allmem[nreg].nframes << (allmem[nreg].frame_width)) - 1,
		     (1UL<<(allmem[nreg].frame_width)), 
		     allmem[nreg].nframes));
	nlf += allmem[nreg].nframes;
    }
    
    ETRC(eprintf("FramesMod$Reqd: total of %d frames in %d regions\n", 
		 nlf, nreg));
    
    res = sizeof(Frames_cl) + sizeof(FramesClient_st) + 
	(nreg * sizeof(struct _FramesMod_st)) +
	(nlf * sizeof(struct _Frame));

    
    /* And round it up to a physical frame boundary */
    res = ROUNDUP(res, FRAME_WIDTH); 

    ETRC(eprintf("FramesMod$Reqd: hence need total of 0x%x (%d) bytes\n", 
		 res, res));
    return res;
}


/* 
** FramesMod_New: create the initial FramesF closure and initialise all 
** shared state (for the regions, etc).
*/
static FramesF_clp New_m(FramesMod_cl *self, Mem_PMem allmem, Mem_PMem used, 
			 RamTab_cl *ramtab, addr_t state)
{
    FramesF_cl      *res;
    FramesMod_st    *fst, *rst;
    FramesClient_st *cst;
    uint32_t nreg, fshift; 
    uint32_t i, j, k;
    word_t   npf, flf, nlf;
    addr_t   start; 

    ETRC(eprintf("Entered FramesMod$New\n"));
    ETRC(eprintf(" + self is %p, state is %p\n", self, state));

    for(nreg = 0; allmem[nreg].nframes != 0; nreg++)
	;

    cst  = (FramesClient_st *)state; 
    res  = (FramesF_cl *)&cst->cl;
    fst  = (FramesMod_st *)(cst+1);
    MTRC(eprintf(" + frames_cl at %p, cst at %p, fst at %p\n", res, cst, fst));
    
    /* Initialise the per-client state; we're a bit special */
    cst->rop   = NULL;         /* no domains around yet */
    cst->owner = OWNER_SYSTEM; /* 'magic' owner since have no DCB yet */      
    cst->headp = NULL;         /* don't bother with a list for system */
    cst->npf   = 0;            /* No physical frames allocated yet    */ 
    cst->gtdf  = -1;           /* unlimited    */
    cst->xtraf = -1;           /* unlimited    */
    cst->heap  = NULL;         /* get on of these later on */
    cst->fst   = fst;          /* back pointer */

    /* Initialise all regions as free first */
    rst = fst; 
    for (i = 0; i < nreg; ) {

	rst->start  = allmem[i].start_addr;
	rst->nlf    = allmem[i].nframes;
	rst->fwidth = allmem[i].frame_width;
	rst->attrs  = allmem[i].attr;
	rst->ramtab = IS_RAM(rst->attrs) ? ramtab : NULL;
	rst->frames = (Frame *)(rst + 1);

	MTRC(eprintf("Region %d: [%p-%p), fwidth=%x, nlf=%x\n", 
		     i, rst->start, rst->start + (rst->nlf << rst->fwidth), 
		     rst->fwidth, rst->nlf));
	
	for(j=0 ; j < rst->nlf; j++) 
	    rst->frames[j].free  = rst->nlf - j;
	
	if(++i < nreg) {
	    /* get to the next bit */
	    rst->next = &(rst->frames[rst->nlf]);
	    rst       = (FramesMod_st *)rst->next; 
	} else  {
	    /* cauterise the final one */
	    rst->next = NULL;
	}
    }
    
    
    /* Setup the closure for return */
    res->op = &framesF_ms;
    res->st = (addr_t)cst;
    
    /* 
    ** Now we need to mark as free all used ones.
    ** XXX SMH: could do this within the above loop if we knew 
    ** anything about the relative ordering of the descriptors.
    */
    for(i = 0; used[i].nframes; i++) {

#ifdef TRC_USED
	eprintf(" ++ used region %d: start=%lx, end=%lx, fsize=%lx, nf=%lx\n", 
		i, used[i].start_addr, used[i].start_addr + 
		(used[i].nframes << (used[i].frame_width)),
		(1UL<<(used[i].frame_width)), used[i].nframes);
#endif

	/* Alloc it! */
	fshift = used[i].frame_width - FRAME_WIDTH; /* may be zero */	
	npf    = used[i].nframes << fshift; 
	start  = used[i].start_addr;
	
	if(!ALIGNED(start,used[i].frame_width)) {
	    eprintf("FramesMod$New: used region %d has mis-aligned start.\n", 
		    i);
	    eprintf("But ignoring for now...\n");
	}

	while(npf) {

	    rst = PM_AllocRange((Frames_cl *)res, npf, start, &flf, &nlf); 

	    if(nlf == 0) {
		eprintf("Urk! Non alloc here according to ret val. Death\n");
		ntsc_halt();
	    }

	    /*
	    ** Ok, got the first loigcal frame index in flf. 
	    ** Now update our allocation information (viz. `free' fields)
	    ** and, if appropriate, the ram table. 
	    */
	    if(rst->ramtab) {
		uint32_t ridx = RIDX(rst);
		for (j=flf; j < (flf + nlf); j++) {
		    rst->frames[j].free = 0;	
		    for(k=0; k < (1UL << fshift); k++) {
			RamTab$Put(rst->ramtab, ridx + (j << fshift) + k, 
				   cst->owner, rst->fwidth, 0);
		    }
		}
	    } else {
		for (j=flf; j < (flf+nlf); j++) 
		    rst->frames[j].free  = 0;	
	    }

	    cst->npf += (nlf << fshift); /* update our npf */

	    /* Okay; update our loop variables and continue... */
	    npf   -= (nlf << fshift);
	    start += (nlf << rst->fwidth); 
	}
    }

    
    TRC(eprintf("Returning res at %p\n", res));
    return res;
}


static void Done_m(FramesMod_cl *self, FramesF_cl *framesF, Heap_cl *heap)
{
    FramesClient_st *cst = (FramesClient_st *)framesF->st;

    if(cst->heap != NULL) {
	eprintf("FramesMod$Done: called with heap=%p, but already have %p\n", 
		heap, cst->heap);
	ntsc_dbgstop();
    }

    cst->heap = heap;

    /* XXX do we want to have a list of regions for system??? */
#if 0
    LINK_INITIALISE(&rop->finfo);
    /* go through ramtab and create appropriate flist_t's, adding as we go */
#endif

    return;
}

/*-------------------------------------------- Frames methods -------------*/


/* 
** Alloc(): allocates a (contiguous) set of frames if available.
** Default type of frame is 'Memory' (i.e. !NonMemory).
*/
static addr_t Alloc_m(Frames_cl *self, uint32_t bytes, uint32_t fw)
{
    return AllocRange_m(self, bytes, fw, ANY_ADDRESS, 0);
}


/* 
** AllocRange(): allocates a (contiguous) specified set of frames.
**          -- 'bytes' defines how many bytes are requested, 
**          -- 'fw' specifies the requested allocation logical frame width.
**          -- 'start' describes the required start address; 
**              If start is unaligned (e.g. -1), then any address is ok
**              else start will be used (assuming it is aligned to 'fw')
**          -- 'attrs' specifies the required attributes, if any.
*/
static addr_t AllocRange_m(Frames_cl *self, uint32_t bytes, uint32_t fw,
			   addr_t start, Mem_Attrs attrs)
{
    FramesClient_st *cst = (FramesClient_st *) self->st;
    register FramesMod_st *cur_st;
    word_t npf, flf, nlf, start_free;
    uint32_t i, j, aw; 
    addr_t base;
	
    ETRC(eprintf("Frames$AllocRange: %x bytes fw %d\n", bytes, fw));

    if(bytes == 0) {
	eprintf("Frames$Alloc warning: got request for zero bytes.\n");
	return NO_ADDRESS;
    }


    /* XXX attributes are ignored for the moment */
    

    /* Convert bytes into physical frames (rounded by width) */
    fw  = MAX(fw, FRAME_WIDTH);
    npf = ROUNDUP(bytes, fw) >> FRAME_WIDTH; 

    if((cst->npf + npf) > cst->gtdf) {
	eprintf("Frames$Alloc: client [dcbro %p, id %qx] exceeding quota.\n", 
		cst->rop, cst->rop ? cst->rop->id : 0x666);
	/* continue for now */
    }

    /* Set the alignment width to be the same as the frame width, i.e. 
       we want 'natural' alignment. */
    aw = fw;  

    if(UNALIGNED(start)) {

	cur_st = PM_AllocAny(self, npf, aw, &flf, &nlf);
	if(cur_st == NULL) {
	    eprintf("Frames$Alloc: failed to alloc %x bytes\n", bytes);
	    ntsc_dbgstop();  /* XXX SMH: temp */
	    return NO_ADDRESS;
	}

    } else {

	/* For PM_AllocRange() we check alignment externally */
	if(!ALIGNED(start,aw)) {
	    eprintf("Frames$Alloc: start %p not aligned to width %d\n", 
		    start, aw);
	    return NO_ADDRESS;
	}
	cur_st = PM_AllocRange(self, npf, start, &flf, &nlf);
	if(cur_st == NULL) {
	    eprintf("Frames$AllocRange: failed to alloc %x bytes at %p\n", 
		    bytes, start);
	    ntsc_dbgstop();  /* XXX SMH: temp */
	    return NO_ADDRESS;
	}

    }


    /* 
    ** Check if our requested frame width has been rounded up; 
    ** this can only happen if we've allocated from a non-standard
    ** region with a default logical frame width greater than our
    ** requested one. 
    */
    if(cur_st->fwidth > fw) {
	fw  = cur_st->fwidth; 
	npf = ROUNDUP(bytes, fw) >> FRAME_WIDTH; 
    }

    base = cur_st->start + LFRAMES_TO_BYTES(flf, cur_st->fwidth);
    
    
    ETRC(eprintf("Frames$Alloc: got first frame idx=%lx, num frames=%x\n", 
	    flf, nlf));

    /* Got the first frame index in flf; update any predecessors */
    start_free = cur_st->frames[flf].free;
    for(i = flf; i != 0; ) {
	i--; 
	if(cur_st->frames[i].free == 0) 
	    break;
	cur_st->frames[i].free -= start_free; 
    }

    /* Mark this allocation as used and, if appropriate, the RAM table. */
    if(cur_st->ramtab) {
	uint32_t ridx   = RIDX(cur_st);
	uint32_t fshift = (cur_st->fwidth - FRAME_WIDTH); /* may be zero */
	for (i = flf; i < (flf+nlf); i++) {
	    cur_st->frames[i].free = 0;	
	    for(j = 0; j < (1 << fshift); j++) {
		RamTab$Put(cur_st->ramtab, ridx + (i << fshift) + j, 
			   cst->owner, fw, 0);
	    }
	}

    } else {
	for (i=flf; i < (flf+nlf); i++) 
	    cur_st->frames[i].free  = 0;	
    }

    /* Update the number of frames we've allocated on this interface */
    cst->npf += npf; 

    /* Add the info about this newly allocated region to our list */
    if(!add_range(cst, base, npf, fw)) {
	eprintf("Frames$Alloc: something's wrong.\n");
	ntsc_dbgstop();
    }

    ETRC(eprintf("  allocated %p\n", base));
    return base;
}

static uint32_t Query_m(Frames_cl *self, addr_t base, Mem_Attrs *attrs)
{
    FramesClient_st *cst = (FramesClient_st *) self->st;
    register FramesMod_st *st = cst->fst;
    register FramesMod_st *cur_st = st;

    if((cur_st = get_region(st, base)) == NULL) {
	/* requested address not in any of our regions */
	eprintf("Frames$Query: address %p is non-existant\n", base);
	return 0; 
    }

    *attrs = cur_st->attrs; 
    return cur_st->fwidth; 
}

/*
** Free(): frees 'bytes' bytes (rounded up to the logical frame width with 
** which it was allocated) starting at (or before) address 'base'.
*/
static void Free_m(Frames_cl *self, addr_t base, uint32_t bytes)
{
    FramesClient_st *cst = (FramesClient_st *) self->st;
    register FramesMod_st *cur_st = get_region(cst->fst, base);
    word_t npf, newnpf, startrlf, endrlf, end_free;
    uint32_t rlfw, alfw, fw, state, owner; 
    addr_t end; 
    int32_t i;

    ETRC(eprintf("Frames$Free: %x bytes starting at addr %p\n", bytes, base));

    if(cur_st == NULL) {
	/* base address not in any of our regions => don't exist. */
	eprintf("Frames$Free: cannot handle non-existant addr %p.\n", base);
	ntsc_halt();	
	return; 
    }

    /* 
    ** Ok: we have a current region in our hands, and have potentially 
    ** two different logical frame widths to cope with:
    ** 
    **    - region logical frame width, RLFW == cur_st->fwidth
    **    - allocation logical frame width, ALFW. 
    **
    ** We use RLFW for handling the indices within the region structure, 
    ** while we use ALFW for rounding down/up the region to be freed. 
    ** 
    ** If the region is not within RAM, then we know RLFW == ALFW, 
    ** and hence there is no problem. 
    ** If the region *is* within RAM, we need to lookup the ALFW in 
    ** the RamTab; we do this by looking up the *physical* frame 
    ** number of [base] and checking its frame width. 
    */

    rlfw = cur_st->fwidth; 
    if(cur_st->ramtab) {
	MTRC(eprintf("Frames$Free: checking ALFW (from pfn 0x%x)\n", 
		     PFN(base)));
	owner = RamTab$Get(cur_st->ramtab, PFN(base), &alfw, &state);
    } else alfw = rlfw; 

    MTRC(eprintf("Frames$Free: have rlfw = %d, alfw = %d\n", rlfw, alfw));

    /* Now we need to round "base+bytes" up, and "base" down to alfw. */
    end  = (addr_t)ROUNDUP(base + bytes, alfw);
    base = (addr_t)(((word_t)base >> alfw) << alfw);
    npf  = PFN(end-base);
    MTRC(eprintf("Frames$Free: aligned region [%p-%p) [npf = %d]\n", 
	    base, end, npf));
    
    /* Check that we actually own these frames, and they're not in use */
    if(cur_st->ramtab) {
	uint32_t pfn   = PFN(base);
	for (i = 0; i < npf; i++) {
	    owner = RamTab$Get(cur_st->ramtab, pfn + i, &fw, &state);
	    if(owner != cst->owner) {
		eprintf("Frames$Free: urk! don't own frame at %p\n", 
			(pfn + i) << FRAME_WIDTH);
		ntsc_dbgstop();
	    }
	    if(fw != alfw) { 
		eprintf("Frames$Free: frame %p fw is %d, should be %d\n", 
			(pfn + i) << FRAME_WIDTH, fw, alfw);
		ntsc_dbgstop();
	    }
	    if((state==RamTab_State_Mapped)||(state==RamTab_State_Nailed)) { 
		eprintf("Frames$Free: frame at %p is %s.\n", 
			(pfn + i) << FRAME_WIDTH, 
			(state == RamTab_State_Mapped) ? "mapped" : "nailed");
		ntsc_dbgstop();
	    }
	}
    }

    /* Now get the frame indices in the region (i.e. lframes of width RLFW) */
    startrlf = BYTES_TO_LFRAMES(base - cur_st->start, rlfw);
    endrlf   = BYTES_TO_LFRAMES(end - cur_st->start,  rlfw);

    if (endrlf > cur_st->nlf) {
	eprintf("Frames$Free: urk! not all addresses are in same region\n");
	ntsc_halt();
    } 

    /* 
    ** First sort out the frames we're freeing.
    ** We do this from the back so can keep `free` counts consistent.
    */
    end_free = (endrlf == cur_st->nlf) ? 0 : cur_st->frames[endrlf+1].free;
    ETRC(eprintf("Frames$Free: startrlf is %x endrlf is %x end_free is %x\n", 
		 startrlf, endrlf, end_free)); 
    
    for (i = endrlf-1; i >= (int32_t) startrlf; i--) {
	cur_st->frames[i].free = ++end_free;
    }

    ETRC(eprintf("--- updated frames to be freed [i is %x]\n", i));

    /* 
    ** Now need to update all empty frames immediately before the first
    ** frame we've freed; hopefully this will not be too many since we 
    ** alloc first fit.
    ** [Note: i points to frame before startrlf already]
    */
    while( i>=0 && cur_st->frames[i].free) { /* go 'til we hit an empty one */
	cur_st->frames[i].free= ++end_free;
	i--;
    }
    
    /* Now update the ramtab (if appropriate) */
    if(cur_st->ramtab) {
	uint32_t ridx   = RIDX(cur_st);
	uint32_t fshift = (cur_st->fwidth - FRAME_WIDTH); /* may be zero */
	for (i = startrlf; i < endrlf; i++) {
	    RamTab$Put(cur_st->ramtab, ridx + (i << fshift), 
		       0, cur_st->fwidth, 0);
	}
    }
    
    /* Finally, update cst->npf, and our linked list of regions */
    newnpf    = cst->npf - npf;  /* XXX SMH paranoia: stop wrap */
    if(newnpf > cst->npf) {
	eprintf("Frames$Free: freeing more frames than I own (ignoring)\n");
	newnpf = 0; 
    }
    cst->npf  = newnpf;

    if(!del_range(cst, base, npf, alfw)) {
	eprintf("Frames$Free: something's wrong.\n");
	ntsc_dbgstop();
    }
    
    ETRC(eprintf("--- updated predecessor frames [i is %x]\n", i));
    return;
}

static void Destroy_m(Frames_cl *self)
{
    eprintf("Frames$Destroy called: not implemented.\n");
    ntsc_dbgstop();
}

/*-------------------------------------------- FramesF methods ------------*/

static Frames_cl *NewClient_m(FramesF_cl *self, addr_t dcbva, 
			      uint32_t dcbpa, uint32_t gtdf, 
			      uint32_t xtraf, uint32_t npf)
{
    FramesClient_st *cst = (FramesClient_st *) self->st;
    FramesMod_st    *fst = cst->fst;
    FramesClient_st *new_cst;
    FramesMod_st    *cur_st; 
    dcb_ro_t *rop = (dcb_ro_t *)dcbva; 
    word_t flf, nlf, start_free;
    uint32_t i, j, aw; 
    addr_t base;

    ETRC(eprintf("FramesF$NewClient: dcbva,pa %p,%lx gf %d xf %d nf %d\n", 
		 dcbva, dcbpa, gtdf, xtraf, npf));

    if(!cst->heap) {
	eprintf("FramesF$NewClient: called before initialisation complete.\n");
	return (Frames_cl *)NULL;
    }


    /* Sanity check args */
    if(npf > gtdf) {
	eprintf("FramesF$NewClient: truncating nf to %d [was %d]\n", 
		gtdf, npf);
	npf = gtdf;
    }

    if(xtraf < gtdf) {
	eprintf("FramesF$NewClient: upping xtraf to %d [was %d]\n", 
		gtdf, xtraf);
	xtraf = gtdf;
    }

    /* Ok: now have xtraf >= gtdf >= npf    */
    /* XXX Admission control on frames ??? */

    if(!(new_cst = Heap$Malloc(cst->heap, sizeof(*new_cst)))) {
	eprintf("FramesF$NewClient: out of memory death.\n");
	ntsc_dbgstop();
    }

    rop->minpfn    = 0; 
    rop->maxpfn    = RamTab$Size(fst->ramtab);
    rop->ramtab    = RamTab$Base(fst->ramtab);

    LINK_INITIALISE(&rop->finfo);

    new_cst->rop   = rop;
    new_cst->headp = &rop->finfo;
    new_cst->npf    = npf; 

    /* 
    ** We want owner to be a cardinal => on alpha we use the 
    ** physical address of the DCBRO (on other machines we can 
    ** just use the virtual address, which is more useful too). 
    */
#ifdef __ALPHA__
    new_cst->owner = dcbpa;
#else
    new_cst->owner = (uint32_t)dcbva;
#endif
    new_cst->gtdf  = gtdf;
    new_cst->xtraf = xtraf;

    new_cst->heap  = cst->heap; 
    new_cst->fst   = cst->fst; 

    /* Alloc some frames now; we have no special alignment constraints */
    aw     = FRAME_WIDTH; 
    cur_st = PM_AllocAny((Frames_cl *)self, npf, aw, &flf, &nlf);
    
    if(cur_st == NULL) {
	eprintf("FramesF$NewClient: failed to alloc %d frames.\n", npf);
	ntsc_dbgstop();
    }

    if(npf != nlf) {
	eprintf("FramesF$NewClient: nlf==%d != %d==npf. Help!\n", nlf, npf);
	ntsc_dbgstop();
    }
	
    base = cur_st->start + LFRAMES_TO_BYTES(flf, cur_st->fwidth);

    ETRC(eprintf("FramesF$NewClient: allocated %d physical frames at %p\n", 
		 npf, base));

    /* Got the first frame index in flf; update any predecessors */
    start_free = cur_st->frames[flf].free;
    for(i = flf; i != 0; ) {
	i--; 
	if(cur_st->frames[i].free == 0) 
	    break;
	MTRC(eprintf("."));
	cur_st->frames[i].free -= start_free; 
    }

    /* Mark this allocation as used and, if appropriate, the RAM table. */
    if(cur_st->ramtab) {
	uint32_t ridx = RIDX(cur_st);
	uint32_t fshift = (cur_st->fwidth - FRAME_WIDTH); /* may be zero */
	for (i=flf; i < (flf+nlf); i++) {
	    cur_st->frames[i].free = 0;	
	    for(j = 0; j < (1 << fshift); j++) {
		RamTab$Put(cur_st->ramtab, ridx + (i << fshift) + j, 
			   new_cst->owner, FRAME_WIDTH, 0);
	    }
	}
    } else {
	for (i=flf; i < (flf+nlf); i++) 
	    cur_st->frames[i].free  = 0;	
    }

    /* Update the number of frames we've allocated on this interface */
    cst->npf += npf; 


    /* Add the info about this newly allocated region to our list */
    if(!add_range(new_cst, base, npf, FRAME_WIDTH)) {
	eprintf("FramesF$NewClient: something's wrong.\n");
	ntsc_dbgstop();
    }

    /* And that is it */
    CL_INIT(new_cst->cl, &frames_ms, new_cst);
    return &new_cst->cl;
}

static bool_t AddFrames_m(FramesF_cl *self, Mem_PMemDesc *pmem)
{
    FramesClient_st *cst = (FramesClient_st *)self->st;
    FramesMod_st *last_st, *new_st; 
    uint32_t i, reqd; 


    ETRC(eprintf("FramesF$AddFrames: region start=%p, nf=0x%x, "
		 "fw=0x%x, attrs=0x%x\n", pmem->start_addr, pmem->nframes, 
		 pmem->frame_width, pmem->attr));

    /* Allocate space for the bookkeeping of the new region */
    reqd = sizeof(struct _FramesMod_st) + 
	(pmem->nframes * sizeof(struct _Frame));

    if(!(new_st = (FramesMod_st *)Heap$Malloc(cst->heap, reqd))) {
	eprintf("FramesF$AddFrames: cannot alloc new framesmod state!\n");
	return False; 
    }

    new_st->start  = pmem->start_addr; 
    new_st->nlf    = pmem->nframes; 
    new_st->fwidth = pmem->frame_width;
    new_st->attrs  = pmem->attr;
    new_st->ramtab = NULL;  /* XXX Assume new regions *cannot* be RAM */
    new_st->next   = NULL; 
    new_st->frames = (Frame *)(new_st + 1);

    /* Initialise the frame structures; all free initially */
    for(i = 0; i < new_st->nlf; i++) 
	new_st->frames[i].free = new_st->nlf - i; 

    /* Find the last region */
    for(last_st = cst->fst; last_st->next; last_st = last_st->next)
	;

    /* And link it in! */
    last_st->next = new_st; 

    ETRC(eprintf("FramesF$AddFrames: success.\n"));
    return True;
}
