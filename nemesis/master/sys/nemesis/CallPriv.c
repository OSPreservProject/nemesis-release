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
**      CallPriv allocator
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of CallPriv.if
** 
** ENVIRONMENT: 
**
**      Nemesis domain. Privileged to make interrupt allocate syscalls.
** 
** ID : $Id: CallPriv.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <autoconf/callpriv.h>

#ifdef CONFIG_CALLPRIV

#include <nemesis.h>
#include <kernel_st.h>
#include <stdio.h>
#include <ntsc.h>

#include <CallPriv.ih>
#include <Heap.ih>
#include <Domain.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static CallPriv_Allocate_fn  Allocate_m;
static CallPriv_Free_fn      Free_m;

static CallPriv_op callpriv_ops = { Allocate_m, Free_m };

typedef struct {
    callpriv_desc *callpriv_dispatch;
    CallPriv_Section callpriv_unused_vector;
    Domain_ID  owner[N_CALLPRIVS];
    /* id=0 -> no owner
       id=1 -> kernel owned */
    CallPriv_cl cl;
} callpriv_st;

CallPriv_clp CallPrivInit(addr_t kst)
{
    callpriv_st *st;
    int i;

    st = Heap$Malloc(Pvs(heap), sizeof(callpriv_st));
    if(!st) {
	fprintf(stderr,"CallPrivInit: no memory\n");
	return NULL;
    }

    st->cl.op = &callpriv_ops;
    st->cl.st = st;
    st->callpriv_dispatch = ((kernel_st *)kst)->cptable;

    for(i = 0; i<N_CALLPRIVS; i++) {
	st->owner[i] = 0;
    }

    /* At initialisation, the kernel will have filled in the callpriv
       dispatch table with references to a function trapping unused
       vectors. Take a copy of the pointer for later use */

    st->callpriv_unused_vector = st->callpriv_dispatch[0].cpfun;

    return &st->cl;
}

static CallPriv_AllocCode Allocate_m(CallPriv_clp self,
			      CallPriv_Section section,
			      CallPriv_StateT state,
			      CallPriv_Vector* vector /* OUT */) 
{
    callpriv_st *st = self->st;
    word_t ipl;
    Domain_ID clientid = 2;
    unsigned int i;
    
    ipl = ntsc_ipl_high();

    for (i = 0; i<N_CALLPRIVS; i++) {
	if (st->owner[i] == 0) break;
    }

    if (i == N_CALLPRIVS) {
	/* Run out of callpriv vectors */
	ntsc_swpipl(ipl);
	return CallPriv_AllocCode_NoFreeVectors;
    }

    st->owner[i] = clientid;
    st->callpriv_dispatch[i].cpfun = section;
    st->callpriv_dispatch[i].state = state;

    ntsc_swpipl(ipl);
#ifdef DEBUG
    fprintf(stderr,"CallPriv: Allocated vector %d for section %p, state %p\n", 
	    i, section, state);
#endif
    *vector = i;

    return CallPriv_AllocCode_Ok;
}
    
static CallPriv_FreeCode Free_m(CallPriv_cl *self, CallPriv_Vector vector) 
{
    callpriv_st *st = self->st;
    int ipl;
    Domain_ID clientid = 2;

    if (vector < 0 || vector >= N_CALLPRIVS)
	return CallPriv_FreeCode_InvalidVector;

    ipl = ntsc_ipl_high();
    
    if (st->owner[vector] == 0) {
	ntsc_swpipl(ipl);
	return CallPriv_FreeCode_NoStub;
    }

    if (st->owner[vector] != clientid) {
	ntsc_swpipl(ipl);
	return CallPriv_FreeCode_NotOwner;
    }

    st->callpriv_dispatch[vector].cpfun = st->callpriv_unused_vector;
    st->callpriv_dispatch[vector].state = (void *)vector;
    st->owner[vector] = 0;

    ntsc_swpipl(ipl);
#ifdef DEBUG
    fprintf(stderr,"CallPriv: Released vector %d\n", vector);
#endif
    return CallPriv_FreeCode_Ok;
    
}

#endif /* CONFIG_CALLPRIV */
