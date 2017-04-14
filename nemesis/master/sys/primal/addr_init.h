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
 **      Definitions and prototypes for the machine and architecture
 **      specific address space initialisation code.
 ** 
 ** ENVIRONMENT: 
 **
 **      Bizarre limbo-land.
 ** 
 ** ID : $Id: addr_init.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 */

#include <ntsc.h>
#include <frames.h>

#include <Mem.ih>
#include <MMU.ih>
#include <RamTab.ih>
#include <FramesMod.ih>
#include <HeapMod.ih>
#include <SAllocMod.ih>
#include <StretchAllocatorF.ih>

#define ALIGN(x) ROUNDUP((x), FRAME_WIDTH)

#define M(_b) ((_b)<<20)
#define K(_b) ((_b)<<10)


/* 
** When creating the address space, we make use of the "global" stretch 
** right which means 'will not have additional rights in any pdom' --- 
** i.e. this entry overrides all others. 
** This is implemented in some architecture dependent way such as the 
** alpha ASM bit, or the x86 'gbl' bit, etc.
** Here we simply defined a few short-cut definitions to make it easier
** for ourselves - global readable, global executable, & global writable. 
*/
#define AXS_NONE ((Stretch_Rights)0)

#define AXS_R (SET_ELEM(Stretch_Right_Read))

#define AXS_E (					\
    SET_ELEM(Stretch_Right_Read) |		\
    SET_ELEM(Stretch_Right_Execute))

#define AXS_W (					\
    SET_ELEM(Stretch_Right_Read) |		\
    SET_ELEM(Stretch_Right_Write))

#define AXS_GR (				\
    SET_ELEM(Stretch_Right_Read) |		\
    SET_ELEM(Stretch_Right_Global))

#define AXS_GE (				\
    SET_ELEM(Stretch_Right_Read) |		\
    SET_ELEM(Stretch_Right_Execute) |		\
    SET_ELEM(Stretch_Right_Global))

#define AXS_GW (				\
    SET_ELEM(Stretch_Right_Read) |		\
    SET_ELEM(Stretch_Right_Write) |		\
    SET_ELEM(Stretch_Right_Global))




/*
** CreateAddressSpace overlays memory with a number of stretches, and sets up
** the relevant access rights. In a number of cases, however, we want to map
** stretches as having certain rights in protection domains which don't yet 
** exist. For the moment, we solve this keeping track of stretches associated
** with various addresses so that we can add the protections later on.
*/
#define MAP_SIZE 0x60     /* number of entries in map; (NB: this is a guess) */
typedef struct {
    addr_t      address;
    Stretch_clp str;
} AddrMap;


/* 
** Below are a number of machine-dependent routines which must be provided
** by all targets in order to perform memory and address space initialisation.
*/

extern FramesF_cl *InitPMem(FramesMod_cl *, Mem_PMem, Mem_Map, 
			    RamTab_cl *, addr_t);

extern ProtectionDomain_ID CreateAddressSpace(Frames_cl *, MMU_cl *, 
					      StretchAllocatorF_cl *, 
					      nexus_ptr_t);

#include <autoconf/memsys.h>
#ifdef CONFIG_MEMSYS_EXPT
extern StretchAllocatorF_clp InitVMem(SAllocMod_cl *, Mem_Map, 
				      Heap_cl *, MMU_cl *);
extern void MapInitialHeap(HeapMod_cl *, Heap_cl *, word_t, 
			   ProtectionDomain_ID);
#else

/* We define a few macros which make use of the above access permissions */

#define STR_NEW_R(_salloc, _size)  				\
({								\
    StretchAllocator$New(_salloc, _size, AXS_R);		\
})
#define STR_NEW_E(_salloc, _size)  				\
({								\
    StretchAllocator$New(_salloc, _size, AXS_E);		\
})
#define STR_NEW_W(_salloc, _size)  				\
({								\
    StretchAllocator$New(_salloc, _size, AXS_W);		\
})
#define STR_NEW_GR(_salloc, _size)  				\
({								\
    StretchAllocator$New(_salloc, _size, AXS_GR);		\
})
#define STR_NEW_GE(_salloc, _size)  				\
({								\
    StretchAllocator$New(_salloc, _size, AXS_GE);		\
})
#define STR_NEW_GW(_salloc, _size)  				\
({								\
    StretchAllocator$New(_salloc, _size, AXS_GW);		\
})


extern Heap_cl *CreateInitialHeap(HeapMod_cl *, Frames_cl *, word_t);

extern StretchAllocatorF_clp InitVMem(SAllocMod_cl *, HeapMod_cl *, 
				      Frames_cl *, MMU_cl *);
extern void MapInitialHeap(HeapMod_cl *, Heap_cl *, Frames_cl *, 
			   word_t, ProtectionDomain_ID);
#endif

extern void MapDomain(addr_t, addr_t, ProtectionDomain_ID);
