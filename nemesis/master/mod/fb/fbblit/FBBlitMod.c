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
**      dev/FB/FBBlitMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Veneer over callpriv blitting
** 
** ENVIRONMENT: 
**
**      shared library
** 
** ID : $Id: FBBlitMod.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/
#include <nemesis.h>
#include <exceptions.h>

#include <FB.ih>
#include <FBBlit.ih>
#include <FBBlitMod.ih>
#include <autoconf/callpriv.h>

static FBBlitMod_New_fn New_m;
static FBBlitMod_op blitmod_ms = { New_m };
static const FBBlitMod_cl cl = {&blitmod_ms, NULL};

CL_EXPORT(FBBlitMod, FBBlitMod, cl);

static FBBlit_Blit_fn Blit_m;
static FBBlit_op blit_ms = { Blit_m };

typedef struct _blit_st {
    FBBlit_cl cl;
    FB_StreamID sid;
    CallPriv_Vector callpriv;
} blit_st;

static FBBlit_clp New_m(FBBlitMod_clp self, FB_StreamID sid,
			CallPriv_Vector vec) {

    blit_st *st = Heap$Malloc(Pvs(heap), sizeof(blit_st));

    CL_INIT(st->cl, &blit_ms, st);

    st->sid = sid;
    st->callpriv = vec;

    return &st->cl;

}

static uint32_t Blit_m (FBBlit_clp self, FBBlit_Rec *rec) {

    blit_st *st = self->st;
    uint32_t rc;
#ifdef CONFIG_CALLPRIV
    rc = ntsc_callpriv(st->callpriv, st->sid, rec, 0, 0); 
#else
    printf("FBBlit$Blit(): No callpriv support!\n");
    rc = (uint32_t) -1;
#endif

    if(rc == (uint32_t) -1) {

	RAISE_FBBlit$BadStream();

    }

    if(rc == (uint32_t) -2) {

	RAISE_FBBlit$BadBlitParams();

    }

    return rc;
}
