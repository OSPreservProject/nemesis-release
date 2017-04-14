/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      sys/memory
**
** FUNCTIONAL DESCRIPTION:
** 
**      Stuff shared between salloc and (arch-specific) mmu code.
** 
** ENVIRONMENT: 
**
**      Internal to sys/memory.
** 
*/

#ifndef _MMU_H_
#define _MMU_H_

#include <MMU.ih>
#include <Stretch.ih>
#include <mmu_tgt.h>       /* pdom, sid and pte info for mc/arch */
#include <frames.h>

struct link_t {
    struct link_t *next;
    struct link_t *prev;
};


/*
** State record for a stretch
** XXX SMH: we expose this here for the benefit of the mmu code. 
** Would prefer not to, but reckon it's better than exposing SIDs
** in MIDDL.... 
*/
struct Stretch_st {
    struct link_t        link;
    StretchAllocator_clp sa;
    sid_t                sid;

    /* XXX A pointer to these two fields is put in the memory context
       as an IDC.Buffer, so they need to be kept together */
    addr_t 	 	 a;
    Stretch_Size 	 s;

    Stretch_Rights       global;
    Stretch_cl           cl;
    MMU_clp              mmu;
};

typedef struct Stretch_st Stretch_st;

/* Just for syntactic sugar, define the below */
#define Stretch_Rights_None ((Stretch_Rights)0U)

#endif /* _MMU_H_ */
