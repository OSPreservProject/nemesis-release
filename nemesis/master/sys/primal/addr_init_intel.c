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
 **      sys/primal
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      x86 specific memory intialisation. 
 ** 
 ** ENVIRONMENT: 
 **
 **      Bizarre limbo-land.
 ** 
 ** ID : $Id: addr_init_intel.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 ** 
 */
#include <nemesis.h>
#include <stdio.h>
#include <nexus.h>
#include <salloc.h>
#include <pip.h>

#include <FramesMod.ih>
#include <StretchAllocatorF.ih>

#include "addr_init.h"

#ifdef DEBUG
#define DEBUG_MEM
#define TRC(_x) _x
#define TRC_MEM(_x) _x
#else
#define TRC(_x)
#define TRC_MEM(_x) 
#endif 

#ifdef DEBUG_MEM
#define ASSERT_ADDRESS(str, addr)		\
{ addr_t _a;					\
  size_t _s;					\
  _a = STR_RANGE(str, &_s);			\
  if (_a != (addr_t)(addr)) {			\
    eprintf("CreateAddressSpace: Stretch address incorrect %p\n", _a); \
    ntsc_halt();				\
  }						\
}
#else
#define ASSERT_ADDRESS(str, addr)
#endif

#define VA_SIZE 0xFFFFFFFFUL

#define ETRC(_x) 

static AddrMap addr_map[MAP_SIZE];

FramesF_cl *InitPMem(FramesMod_cl *fmod, Mem_PMem allmem, Mem_Map memmap, 
		     RamTab_cl *rtab, addr_t free)
{
    Mem_PMemDesc used[32];
    FramesF_clp framesF;
    int i, j;

    TRC(eprintf("InitPMem: INTEL.\n"));

    /* Determine the used pmem from the mapping info */
    j = 0;
    for(i=0; memmap[i].nframes; i++) {
	used[i].start_addr  = memmap[i].paddr;
	used[i].nframes     = memmap[i].nframes;
	used[i].frame_width = memmap[i].frame_width; 
	used[i].attr      = 0;
    }

    /* now terminate the array */
    used[i].nframes = 0;

    framesF = FramesMod$New(fmod, allmem, used, rtab, free);
    return framesF;
}


#ifdef CONFIG_MEMSYS_EXPT

StretchAllocatorF_cl *InitVMem(SAllocMod_cl *smod, Mem_Map memmap, 
			       Heap_cl *h, MMU_cl *mmu)
{
    StretchAllocatorF_cl *salloc;
    Mem_VMemDesc allvm[2];
    Mem_VMemDesc used[32];
    int i;

    allvm[0].start_addr = 0;
    allvm[0].npages     = VA_SIZE >> PAGE_WIDTH;
    allvm[0].page_width = PAGE_WIDTH;
    allvm[0].attr       = 0;

    allvm[1].npages     = 0;  /* end of array marker */

    /* Determine the used vm from the mapping info */
    for(i=0; memmap[i].nframes; i++) {
	
	used[i].start_addr  = memmap[i].vaddr;
	used[i].npages      = memmap[i].nframes;
	used[i].page_width  = memmap[i].frame_width; 
	used[i].attr        = 0;

    }

    /* now terminate the array */
    used[i].npages = 0;

    salloc = SAllocMod$NewF(smod, h, mmu, allvm, used);
    return salloc;
}



ProtectionDomain_ID CreateAddressSpace(Frames_clp frames, MMU_clp mmu, 
					StretchAllocatorF_clp sallocF, 
					nexus_ptr_t nexus_ptr)
{
    nexus_ptr_t          nexus_end;
    struct nexus_ntsc   *ntsc  = NULL;
    struct nexus_module *mod;
    struct nexus_blob   *blob;
    struct nexus_prog   *prog;
    struct nexus_primal *primal;
    struct nexus_nexus  *nexus;
    struct nexus_name   *name;
    struct nexus_EOI    *EOI;
    ProtectionDomain_ID  pdom;
    Stretch_clp          str;
    int                  map_index;

    primal    = nexus_ptr.nu_primal;
    nexus_end = nexus_ptr.generic;
    nexus_end.nu_primal++;	/* At least one primal */
    nexus_end.nu_ntsc++;	/* At least one ntsc */
    nexus_end.nu_nexus++;	/* At least one nexus */
    nexus_end.nu_EOI++;		/* At least one EOI */

    ETRC(eprintf("Creating new protection domain (mmu at %p)\n", mmu));
    pdom = MMU$NewDomain(mmu);

    /* Intialise the pdom map to zero */
    for(map_index=0; map_index < MAP_SIZE; map_index++) { 
        addr_map[map_index].address= NULL;
	addr_map[map_index].str    = (Stretch_cl *)NULL;
    }
    map_index= 0;

    /* 
    ** Sort out memory before the boot address (platform specific)
    ** In MEMSYS_EXPT, this memory has already been marked as used 
    ** (both in virtual and physical address allocators) by the 
    ** initialisation code, so all we wish to do is to map some 
    ** stretches over certain parts of it. 
    ** Most of the relevant information is provided in the nexus, 
    ** although even before this we have the PIP. 
    */

    /* First we need to map the PIP globally read-only */
    str = StretchAllocatorF$NewOver(sallocF, PAGE_SIZE, AXS_GR, 
				    (addr_t)INFO_PAGE_ADDRESS, 
				    0, PAGE_WIDTH, NULL);
    ASSERT_ADDRESS(str, INFO_PAGE_ADDRESS);

    /* Map stretches over the boot image */
    TRC_MEM(eprintf("CreateAddressSpace (EXPT): Parsing NEXUS at %p\n", 
		nexus_ptr.generic));

    while(nexus_ptr.generic < nexus_end.generic)  {
	
	switch(*nexus_ptr.tag) {
	    
	  case nexus_primal:
	    primal = nexus_ptr.nu_primal++;
	    TRC_MEM(eprintf("PRIM: %lx\n", primal->lastaddr));
	    break;
	    
	  case nexus_ntsc:
	    ntsc = nexus_ptr.nu_ntsc++;
	    TRC_MEM(eprintf("NTSC: T=%06lx:%06lx D=%06lx:%06lx "
			    "B=%06lx:%06lx\n",
			    ntsc->taddr, ntsc->tsize,
			    ntsc->daddr, ntsc->dsize,
			    ntsc->baddr, ntsc->bsize));

	    /* Listen up, dudes: here's the drill:
	     *
	     *   |    bss     |  read/write perms
	     *   |------------|
	     *   |   data     |-------------
             *   |------------|  read/write & execute perms  (hack page)
	     *   |   text     |-------------   <- page boundary
	     *   |            |
	     *   |            |  read/execute perms
	     *
	     * Now, the text and data boundary of the NTSC is not
	     * necessarily page aligned, so there may or may not be a
	     * hack page overlapping it.
	     * The next few bits of code work out whether we need a
	     * hack page, and creates it.
	     */
	    if ((ntsc->daddr - ntsc->taddr) & (FRAME_SIZE-1))
	    {
		/* If NTSC text is over 1 page, need some text pages */
		if (ALIGN(ntsc->daddr - ntsc->taddr) - FRAME_SIZE != 0)
		{
		    str = StretchAllocatorF$NewOver(
			sallocF, ALIGN(ntsc->daddr - ntsc->taddr)-FRAME_SIZE, 
			AXS_GE, (addr_t)ntsc->taddr, 0, PAGE_WIDTH, NULL);
		    ASSERT_ADDRESS(str, ntsc->taddr);
		}

		/* create hack page */
		str = StretchAllocatorF$NewOver(
		    sallocF, FRAME_SIZE, AXS_NONE, 
		    (addr_t)(ALIGN(ntsc->daddr) - FRAME_SIZE), 0, 
		    PAGE_WIDTH, NULL);
		TRC_MEM(eprintf("       -- hack page at %06lx\n",
				ALIGN(ntsc->daddr) - FRAME_SIZE));
		ASSERT_ADDRESS(str, ALIGN(ntsc->daddr) - FRAME_SIZE);
		SALLOC_SETPROT(salloc, str, pdom,
					 SET_ELEM(Stretch_Right_Read) |
					 SET_ELEM(Stretch_Right_Write) |
					 SET_ELEM(Stretch_Right_Execute));
	    }
	    else
	    {
		/* no hack page needed */
		str = StretchAllocatorF$NewOver(sallocF, ntsc->tsize, AXS_GE,
						(addr_t)ntsc->taddr, 0, 
						PAGE_WIDTH, NULL);
		ASSERT_ADDRESS(str, ntsc->taddr);
	    }
	    break;
	    
	  case nexus_nexus:
	    nexus = nexus_ptr.nu_nexus++;
	    TRC_MEM(eprintf("NEX:  N=%06lx,%06lx IGNORING\n", 
			    nexus->addr, nexus->size));
	    nexus_end.generic = (addr_t)(nexus->addr + nexus->size);

	    /* XXX Subtlety - NEXUS tacked on the end of NTSC BSS */
	    /* 
	    ** XXX More subtlety; the NEXUS is always a page and `a bit', where
	    ** the bit is whatever's left from the end of the ntsc's bss upto
	    ** a page boundary, and the page is the one following that.
	    ** This is regardless of whether or not the nexus requires this 
	    ** space, and as such nexus->size can be misleading. Simplest 
            ** way to ensure we alloc enough mem for now is to simply 
	    ** use 1 page as a lower bound for the nexus size.
	    */
	    if ((ntsc->dsize + ntsc->bsize + MAX(nexus->size, FRAME_SIZE) -
		(ALIGN(ntsc->daddr) - ntsc->daddr)) > 0)
	    {
		str = StretchAllocatorF$NewOver(
		    sallocF, ntsc->dsize + ntsc->bsize + 
		    MAX(nexus->size, FRAME_SIZE) -
		    (ALIGN(ntsc->daddr) - ntsc->daddr) /* size */, 
		    AXS_NONE, (addr_t)ALIGN(ntsc->daddr), 0, PAGE_WIDTH, NULL);
		ASSERT_ADDRESS(str, ALIGN(ntsc->daddr));
		TRC_MEM(eprintf("Setting pdom prot on ntsc data (%p)\n",
				ALIGN(ntsc->daddr)));
		SALLOC_SETPROT(salloc, str, pdom, 
					 SET_ELEM(Stretch_Right_Read));
	    }

	    break;
	    
	  case nexus_module:
	    mod = nexus_ptr.nu_mod++;
	    TRC_MEM(eprintf("MOD:  T=%06lx:%06lx\n",
			    mod->addr, mod->size));
	    str = StretchAllocatorF$NewOver(sallocF, mod->size, AXS_GE, 
					    (addr_t)mod->addr, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, mod->addr);
	    break;
	    
	  case nexus_namespace:
	    name = nexus_ptr.nu_name++;
	    nexus_ptr.generic = (char *)nexus_ptr.generic + 
		name->nmods * sizeof(addr_t);
	    TRC_MEM(eprintf("NAME: N=%06lx:%06lx\n", 
			    name->naddr, name->nsize));
	    if (name->nsize == 0){

		/* XXX If we put an empty namespace in the nemesis.nbf
		   file, nembuild still reserves a page for it in the
		   nexus, so we need to make sure that at least a page is
		   requested from the stretch allocator, otherwise the
		   _next_ entry in the nexus will cause the ASSERT_ADDRESS
		   to fail. This probably needs to be fixed in
		   nembuild.
		   */

		TRC_MEM(eprintf(
		    "NAME: Allocating pad page for empty namespace\n"));

		name ->nsize = 1;
	    }
	    

	    str = StretchAllocatorF$NewOver(sallocF, name->nsize, 
					    AXS_GR, (addr_t)name->naddr, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, name->naddr);
	    break;
	    
	  case nexus_program:
	    prog= nexus_ptr.nu_prog++;
	    TRC_MEM(eprintf("PROG: T=%06lx:%06lx D=%06lx:%06lx "
			    "B=%06lx:%06lx  \"%s\"\n",
			    prog->taddr, prog->tsize,
			    prog->daddr, prog->dsize,
			    prog->baddr, prog->bsize,
			    prog->program_name));

	    str = StretchAllocatorF$NewOver(sallocF, prog->tsize, 
					    AXS_NONE, (addr_t)prog->taddr, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, prog->taddr);

	    /* Keep record of the stretch for later mapping into pdom */
	    addr_map[map_index].address= (addr_t)prog->taddr;
	    addr_map[map_index++].str  = str;

	    if (prog->dsize + prog->bsize) {
		str = StretchAllocatorF$NewOver(
		    sallocF, ROUNDUP((prog->dsize+prog->bsize), FRAME_WIDTH), 
		    AXS_NONE, (addr_t)prog->daddr, 0, PAGE_WIDTH, NULL);
		ASSERT_ADDRESS(str, prog->daddr);
		/* Keep record of the stretch for later mapping into pdom */
		addr_map[map_index].address= (addr_t)prog->daddr;
		addr_map[map_index++].str  = str;
	    }
	    
	    break;

	case nexus_blob:
	    blob = nexus_ptr.nu_blob++;
	    TRC_MEM(eprintf("BLOB: B=%06lx:%06lx\n",
			    blob->base, blob->len));

	    /* slap a stretch over it */
	    str = StretchAllocatorF$NewOver(sallocF, blob->len, 
					    AXS_GR, (addr_t)blob->base, 
					    0, PAGE_WIDTH, NULL);
	    ASSERT_ADDRESS(str, blob->base);
	    break;

	  case nexus_EOI:
	    EOI = nexus_ptr.nu_EOI++;
	    TRC_MEM(eprintf("EOI:  %lx\n", EOI->lastaddr));
	    break;
	    
	  default:
	    TRC_MEM(eprintf("Bogus NEXUS entry: %x\n", *nexus_ptr.tag));
	    ntsc_halt();
	    break;
	}
    }
    TRC_MEM(eprintf("CreateAddressSpace: Done\n"));
    return pdom;

}


/*
** At startup we create a physical heap; while this is fine, the idea
** of protection is closely tied to that of stretches. Hence this function
** maps a stretch over the existing heap.
** This allows us to map it read/write for us, and read-only to everyone else.
*/
void MapInitialHeap(HeapMod_clp hmod, Heap_clp heap, 
		    word_t heap_size, ProtectionDomain_ID pdom)
{
    Stretch_clp str;
    Heap_clp realheap; 
    addr_t a = (addr_t)((size_t)heap & ~(PAGE_SIZE-1));

    TRC(eprintf(" + Mapping stretch over heap: 0x%x bytes at %p\n", 
	    heap_size, a));
    str = StretchAllocatorF$NewOver((StretchAllocatorF_cl *)Pvs(salloc), 
				    heap_size, AXS_R, a, 0, 
				    PAGE_WIDTH, NULL);
    ASSERT_ADDRESS(str, a);
    TRC(eprintf(" + Done!\n"));

    realheap = HeapMod$Realize(hmod, heap, str);

    if(realheap != heap) 
	eprintf("WARNING: HeapMod$Realize(%p) => %p\n", 
		heap, realheap);


    /* Map our heap as local read/write */
    STR_SETPROT(str, pdom, (SET_ELEM(Stretch_Right_Read)|
			    SET_ELEM(Stretch_Right_Write)));
}


#else /* ! CONFIG_MEMSYS_EXPT */


StretchAllocatorF_cl *InitVMem(SAllocMod_cl *smod, HeapMod_cl *hmod, 
			       Frames_cl *frames, MMU_cl *mmu)
{
    StretchAllocatorF_cl *salloc;
    Mem_VMemDesc regions[2];

    regions[0].start_addr = 0;
    regions[0].npages     = VA_SIZE >> PAGE_WIDTH;
    regions[0].page_width = PAGE_WIDTH;
    regions[0].attr       = 0;

    regions[1].npages     = 0;  /* end of array marker */

    salloc = SAllocMod$NewF(smod, hmod, frames, mmu, regions);
    return salloc;
}


ProtectionDomain_ID CreateAddressSpace(Frames_clp frames, MMU_clp mmu, 
					StretchAllocatorF_clp sallocF, 
					nexus_ptr_t nexus_ptr)
{
    nexus_ptr_t  	       nexus_end;
    
    struct nexus_ntsc   *ntsc;
    struct nexus_module *mod;
    struct nexus_blob   *blob;
    struct nexus_prog   *prog;
    struct nexus_primal *primal;
    struct nexus_nexus  *nexus;
    struct nexus_name   *name;
    struct nexus_EOI    *EOI;
    ProtectionDomain_ID  pdom;
    StretchAllocator_cl *salloc= (StretchAllocator_cl *)sallocF;
    Stretch_clp  str;
    int map_index;

    /* Free the frames used by the boot image */
    primal    = nexus_ptr.nu_primal;
    nexus_end = nexus_ptr.generic;
    nexus_end.nu_primal++;	/* At least one primal */
    nexus_end.nu_ntsc++;        /* At least one ntsc */
    nexus_end.nu_nexus++;       /* At least one nexus */
    nexus_end.nu_EOI++;		/* At least one EOI */
    TRC_MEM(eprintf("nexus_end = %p\n", nexus_end.generic));

#define SORTED_OUT_ADDR_INIT
#ifdef SORTED_OUT_ADDR_INIT
    TRC_MEM(eprintf("CreateAddressSpace: Returning image memory (%lx-%lx)\n",
		    primal->firstaddr, (primal->lastaddr-primal->firstaddr)));
    TRC_MEM(eprintf(" + this is a lie...\n"));
    /* Return memory from 0--640K */
    Frames$Free(frames, 0, K(640));

    /* Return weird-ass memory from 640K--1M */
    Frames$Free(frames, (addr_t)K(640), K(384));

    /* Return memory from 1M--end of image */
    Frames$Free(frames, (addr_t)M(1), 
		(primal->lastaddr - primal->firstaddr-M(1)));
#else
    TRC_MEM(eprintf("CreateAddressSpace: Returning memory up to %lx\n",
		    primal->lastaddr));
    Frames$Free(frames, 0, primal->lastaddr); 
#endif
    
    /* Grotty memory allocation stuff that XXX assumes XXX  memory is
     * allocated in order from regions of type 'real memory'. This 
     * is all a lot cleaner in the EXPT memory system. */

    /* Sort out memory before the boot address (platform specific) */
    pdom = NULL_PDID;
    
    /* First page (4k) is supposed to cause faults */
    str = STR_NEW_SALLOC(salloc, 4096);
    
    /* Next we have the PIP */
    str = STR_NEW_GR(salloc, 4096);

    /* Next page is the kernel info page */
    /* (not currently used, but it's probably a good idea to keep it
       lying around for future use) */
    str = STR_NEW_GR(salloc, 4096);

    /* Create a pdom for Nemesis domain (incl. end of primal + ntsc) */
    pdom = MMU$NewDomain(mmu);
    TRC_MEM(eprintf("CreateAddressSpace: made nemesis pdom at %p\n", pdom));

    /* Now we have kernel space up to 640K */
    /* Then a couple of pages for the PDOM for NTSC and primal */
#define PDOM_SIZE (K(8))
    str = STR_NEW_W(salloc, K(640) - 3*4096 - PDOM_SIZE);

    /* Intialise the pdom map to zero */
    for(map_index=0; map_index < MAP_SIZE; map_index++) { 
        addr_map[map_index].address= NULL;
	addr_map[map_index].str    = (Stretch_cl *)NULL;
    }
    map_index= 0;

    /* Now alloc the 'non-memory' between 640K and 1M; this has been 
       (grossly!) marked as "standard memory" in memsys_std so that 
       the below will work. Soon, I hope, this initalisation stuff
       can be made to be the same as the (far nicer) expt version. */
    /* Urk! Even grottier --- all this is globally read. This is 
       since some drivers actually just bail at the address of 
       the bios info they want, without even entering a critical 
       section. Dreadful, but this is the simplest way to get 
       things working again. */
    str = STR_NEW_R(salloc, K(384));

    /* Map stretches over the boot image */
    TRC_MEM(eprintf("CreateAddressSpace: Parsing NEXUS at %p\n", 
		    nexus_ptr.generic));
    
    while(nexus_ptr.generic < nexus_end.generic)  {
	
	switch(*nexus_ptr.tag) {
	    
	  case nexus_primal:
	    primal = nexus_ptr.nu_primal++;
	    TRC_MEM(eprintf("PRIM: %lx\n", primal->lastaddr));
	    break;
	    
	  case nexus_ntsc:
	    ntsc = nexus_ptr.nu_ntsc++;
	    TRC_MEM(eprintf("NTSC: T=%06lx:%06lx D=%06lx:%06lx "
			    "B=%06lx:%06lx\n",
			    ntsc->taddr, ntsc->tsize,
			    ntsc->daddr, ntsc->dsize,
			    ntsc->baddr, ntsc->bsize));

	    /* Listen up, dudes: here's the drill:
	     *
	     *   |    bss     |  read/write perms
	     *   |------------|
	     *   |   data     |-------------
             *   |------------|  read/write & execute perms  (hack page)
	     *   |   text     |-------------   <- page boundary
	     *   |            |
	     *   |            |  read/execute perms
	     *
	     * Now, the text and data boundary of the NTSC is not
	     * necessarily page aligned, so there may or may not be a
	     * hack page overlapping it.
	     * The next few bits of code work out whether we need a
	     * hack page, and creates it.
	     */
	     
	    if ((ntsc->daddr - ntsc->taddr) & (FRAME_SIZE-1))
	    {
		/* If NTSC text is over 1 page, need some text pages */
		if (ALIGN(ntsc->daddr - ntsc->taddr) - FRAME_SIZE != 0)
		{
		    str = STR_NEW_GE(salloc,ALIGN(ntsc->daddr - ntsc->taddr)
				     - FRAME_SIZE);
		    ASSERT_ADDRESS(str, ntsc->taddr);
		}

		/* create hack page */
		str = STR_NEW_SALLOC(salloc, FRAME_SIZE);
		TRC_MEM(eprintf("       -- hack page at %06lx\n",
				ALIGN(ntsc->daddr) - FRAME_SIZE));
		ASSERT_ADDRESS(str, ALIGN(ntsc->daddr) - FRAME_SIZE);
		SALLOC_SETPROT(salloc, str, pdom,
					 SET_ELEM(Stretch_Right_Read) |
					 SET_ELEM(Stretch_Right_Write) |
					 SET_ELEM(Stretch_Right_Execute));
	    }
	    else
	    {
		/* no hack page needed */
		str = STR_NEW_GE(salloc, ntsc->tsize);
		ASSERT_ADDRESS(str, ntsc->taddr);
	    }

	    break;
	    
	  case nexus_nexus:
	    nexus = nexus_ptr.nu_nexus++;
	    TRC_MEM(eprintf("NEX:  N=%06lx,%06lx IGNORING\n", 
			    nexus->addr, nexus->size));
	    nexus_end.generic = (addr_t)(nexus->addr + nexus->size);
	    
	    /* XXX Subtlety - NEXUS tacked on the end of NTSC BSS */
	    /* 
	    ** XXX More subtlety; the NEXUS is always a page and `a bit', where
	    ** the bit is whatever's left from the end of the ntsc's bss upto
	    ** a page boundary, and the page is the one following that.
	    ** This is regardless of whether or not the nexus requires this 
	    ** space, and as such nexus->size can be misleading. Simplest 
            ** way to ensure we alloc enough mem for now is to simply 
	    ** use 1 page as a lower bound for the nexus size.
	    */
	    if ((ntsc->dsize + ntsc->bsize + MAX(nexus->size, FRAME_SIZE) -
		(ALIGN(ntsc->daddr) - ntsc->daddr)) > 0)
	    {
		str = STR_NEW_SALLOC(salloc, ntsc->dsize + ntsc->bsize + 
				 MAX(nexus->size, FRAME_SIZE) -
				 (ALIGN(ntsc->daddr) - ntsc->daddr));
		ASSERT_ADDRESS(str, ALIGN(ntsc->daddr));
		TRC_MEM(eprintf("Setting pdom prot on ntsc data (%p)\n",
				ALIGN(ntsc->daddr)));
		SALLOC_SETPROT(salloc, str, pdom, 
					 SET_ELEM(Stretch_Right_Read));
	    }

	    break;
	    
	  case nexus_module:
	    mod = nexus_ptr.nu_mod++;
	    TRC_MEM(eprintf("MOD:  T=%06lx:%06lx\n",
			    mod->addr, mod->size));
	    str = STR_NEW_GE(salloc, mod->size);
	    ASSERT_ADDRESS(str, mod->addr);
	    break;
	    
	  case nexus_namespace:
	    name = nexus_ptr.nu_name++;
	    nexus_ptr.generic = (char *)nexus_ptr.generic + 
		name->nmods * sizeof(addr_t);
	    TRC_MEM(eprintf("NAME: N=%06lx:%06lx\n", 
			    name->naddr, name->nsize));

	    if (name->nsize == 0){

		/* XXX If we put an empty namespace in the nemesis.nbf
		   file, nembuild still reserves a page for it in the
		   nexus, so we need to make sure that at least a page is
		   requested from the stretch allocator, otherwise the
		   _next_ entry in the nexus will cause the ASSERT_ADDRESS
		   to fail. This probably needs to be fixed in
		   nembuild.
		   */

		TRC_MEM(eprintf(
		    "NAME: Allocating pad page for empty namespace\n"));

		name ->nsize = 1;
	    }
	    

	    str = STR_NEW_GR(salloc, name->nsize);
	    ASSERT_ADDRESS(str, name->naddr);
	    break;
	    
	  case nexus_program:
	    prog= nexus_ptr.nu_prog++;
	    TRC_MEM(eprintf("PROG: T=%06lx:%06lx D=%06lx:%06lx "
			    "B=%06lx:%06lx  \"%s\"\n",
			    prog->taddr, prog->tsize,
			    prog->daddr, prog->dsize,
			    prog->baddr, prog->bsize,
			    prog->program_name));
	    str = STR_NEW_SALLOC(salloc, prog->tsize);
	    ASSERT_ADDRESS(str, prog->taddr);
	    /* Keep record of the stretch for later mapping into pdom */
	    addr_map[map_index].address= (addr_t)prog->taddr;
	    addr_map[map_index++].str  = str;
	    
	    if (prog->dsize + prog->bsize) {
		str = STR_NEW_SALLOC(salloc, ROUNDUP((prog->dsize+prog->bsize), 
						 FRAME_WIDTH));
		ASSERT_ADDRESS(str, prog->daddr);
		/* Keep record of the stretch for later mapping into pdom */
		addr_map[map_index].address= (addr_t)prog->daddr;
		addr_map[map_index++].str  = str;
	    }
	    
	    break;

	case nexus_blob:
	    blob = nexus_ptr.nu_blob++;
	    TRC_MEM(eprintf("BLOB: B=%06lx:%06lx\n",
			    blob->base, blob->len));

	    /* slap a stretch over it */
	    str = STR_NEW_SALLOC(salloc, blob->len);
	    ASSERT_ADDRESS(str, blob->base);
	    SALLOC_SETGLOBAL(salloc, str, SET_ELEM(Stretch_Right_Read));
	    break;

	  case nexus_EOI:
	    EOI = nexus_ptr.nu_EOI++;
	    TRC_MEM(eprintf("EOI:  %lx\n", EOI->lastaddr));
	    break;
	    
	  default:
	    eprintf("Bogus NEXUS entry: %x\n", *nexus_ptr.tag);
	    ntsc_halt();
	    break;
	}
    }
    TRC_MEM(eprintf("CreateAddressSpace: Done\n"));
    return pdom;
}


/*
** At startup we create a physical heap; while this is fine, the idea
** of protection is closely tied to that of stretches. Hence this function
**   (a) 'frees' the memory underneath the heap, and
**   (b) allocates it again through the stretch allocator.
** This allows us to map it read/write for us, and read-only to everyone else.
*/
void MapInitialHeap(HeapMod_clp hmod, Heap_clp heap, Frames_clp frames, 
		    word_t heap_size, ProtectionDomain_ID pdom)
{
    Stretch_clp str;
    Heap_cl *realheap; 
    addr_t a;

    a = (addr_t)((size_t)heap & ~(FRAME_SIZE-1));

    Frames$Free(frames, a, heap_size);
    str = STR_NEW_R(Pvs(salloc), heap_size);
    ASSERT_ADDRESS(str, a);

    realheap = HeapMod$Realize(hmod, heap, str);

    if(realheap != heap) 
	eprintf("WARNING: HeapMod$Realize(%p) => %p\n", 
		heap, realheap);

    
    /* Map our heap as local read/write */
    STR_SETPROT(str, pdom, (SET_ELEM(Stretch_Right_Read)|
			    SET_ELEM(Stretch_Right_Write)));
}

#endif



/*
** MapDomain maps the text and data parts of a domain into the given
** protection domain with 'appropriate' rights. The address map built
** up earlier by CreateAddressSpace() is used to determine the correct
** stretches from the (physical) addresses.
*/
void MapDomain(addr_t taddr, addr_t daddr, ProtectionDomain_ID pdom)
{
    int i;

    for(i=0; i < MAP_SIZE; i++) {
	if(addr_map[i].address==taddr) {
	    STR_SETPROT(addr_map[i].str, pdom,
			SET_ELEM(Stretch_Right_Read) | 
			SET_ELEM(Stretch_Right_Execute));
	} else if(addr_map[i].address==daddr) {
	    STR_SETPROT(addr_map[i].str, pdom,
			SET_ELEM(Stretch_Right_Read) | 
			SET_ELEM(Stretch_Right_Write));
	    break;
	}
    }
}
