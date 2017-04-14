/*
*****************************************************************************
**                                                                          *
**  Copyright 1995, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	
**
** FUNCTIONAL DESCRIPTION:
**
**	
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: displaymod.c 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>
#include <IOMacros.h>
#include <IDCMacros.h>
#include <CRendDisplayMod.ih>
#include <WS.ih>

#include "CRendDisplayMod_st.h"
#include "CRendDisplay_st.h"

#ifdef DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif


/*
 * Module stuff
 */
static	CRendDisplayMod_OpenDisplay_fn 		CRendDisplayMod_OpenDisplay_m;

static	CRendDisplayMod_op	ms = {
  CRendDisplayMod_OpenDisplay_m
};

const static	CRendDisplayMod_cl	cl = { &ms, NULL };

CL_EXPORT (CRendDisplayMod, CRendDisplayMod, cl);

/*---------------------------------------------------- Entry Points ----*/

static CRendDisplay_clp 
CRendDisplayMod_OpenDisplay_m (
	CRendDisplayMod_cl	*self,
	string_t NOCLOBBER	name	/* IN */ )
{
    CRendDisplay_st * NOCLOBBER d = NULL;
    extern CRendDisplay_op CRendDisplay_ms;
    
    IDCOffer_clp NOCLOBBER wsoffer = NULL;
    uint64_t protos;
    Type_Any any;
    
    if(!(name && name[0])) name = "svc>WS";
    
    TRC(printf ("WSOpenDisplay: binding to '%s'\n", name));
    
    while (!wsoffer) {
	TRY {
	    wsoffer = NAME_FIND(name, IDCOffer_clp); 
	} CATCH_Context$NotFound (UNUSED cname) {
	    TRC(printf ("WSOpenDisplay: offer not present, retrying..\n"));
	    PAUSE (SECONDS (5));
	} ENDTRY;
    }
    
    /* We don't go through the object table - this allows multiple
       sections of a program to maintain their own connections to the
       display */

    TRC(printf("WSOpenDisplay: obtained offer %p\n", wsoffer));

    TRY {
	
	d = Heap$Malloc(Pvs(heap), sizeof(CRendDisplay_st));
	if(!d) RAISE_Heap$NoMemory();
	
	memset(d, 0, sizeof(*d));
	
	CL_INIT(d->cl, &CRendDisplay_ms, d);
	
	TRC(printf("WSOpenDisplay: Binding to wsoffer\n"));

	d->binding = IDCOffer$Bind(wsoffer, Pvs(gkpr), &any);
	
	d->ws = NARROW(&any, WS_clp);

	TRC(printf("WSOpenDisplay: Creating event stream\n"));

	d->evio = IO_BIND(WS$EventStream(d->ws));
	
	TRC(printf ("WSOpenDisplay: priming event pipe\n"));
	IO_PRIME(d->evio, sizeof(WSEvent));
	
	{
	    uint32_t dummy;
	    d->width = WS$Info(d->ws, &d->height, 
			       &d->xgrid, &d->ygrid, &d->depth, 
			       &dummy, &protos);
	}
	
	
    } CATCH_ALL {
	
	if(d->binding) IDCClientBinding$Destroy(d->binding);
	
	if(d) FREE(d);
	
	RERAISE;
    } ENDTRY;
    
    return &d->cl;
}

/*
 * End 
 */
