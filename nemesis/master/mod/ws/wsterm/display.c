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
**      Nemesis client rendering window system.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Window functions
** 
** ENVIRONMENT: 
**
**      Shared library.
** 
** ID : $Id: display.c 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
#include <time.h>
#include <ecs.h>

#include <IO.ih>
#include <IOMacros.h>
#include <IDCMacros.h>
#include <WS.ih>

#include <WS.h>

#include "WSprivate.h"

#define TRC(x) 

Display * 
WSOpenDisplay(string_t NOCLOBBER name)
{
    Display *d   = Heap$Malloc(Pvs(heap), sizeof(struct _Display));
    
    IDCOffer_clp NOCLOBBER wsoffer = NULL;
    IOOffer_clp evoffer;
    Type_Any any;
    
    uint32_t width, height, xgrid, ygrid, depth, dummy;
    set_t protos;
    
    if (!d) return NULL;
    
    if(!(name && name[0])) name = "svc>WS";
    
    TRC(printf ("WSOpenDisplay: binding to '%s'\n", name));
    while (!wsoffer) {
	TRY {
	    wsoffer = NAME_FIND (name, IDCOffer_clp);
	} CATCH_Context$NotFound (UNUSED n) {
	    TRC(printf ("WSOpenDisplay: offer not present, retrying..\n"));
	  PAUSE (SECONDS (5));
	} ENDTRY;
    }
    
    TRC(printf ("WSOpenDisplay: binding to WS\n"));
    d->binding = IDCOffer$Bind(wsoffer, Pvs(gkpr), &any);
    d->ws = NARROW(&any, WS_clp);
    
    TRC(printf ("WSOpenDisplay: asking for event stream\n"));
    evoffer = WS$EventStream(d->ws);
    
    TRC(printf ("WSOpenDisplay: binding to event stream\n"));
    
    d->evio = IO_BIND(evoffer); 
    d->ev   = NULL;
    
    TRC(printf ("WSOpenDisplay: priming event pipe\n"));
    IO_PRIME(d->evio, sizeof(WSEvent));
    
    width = WS$Info(d->ws, &height, &xgrid, &ygrid, &depth, &dummy, &protos);
    
    d->width  = width;
    d->height = height;
    
    return d;
}

void 
WSCloseDisplay(Display* d)
{
    
    IDCClientBinding$Destroy(d->binding);
    
    FREE(d);
}

uint32_t 
WSDisplayHeight(Display* d) 
{
    return d->height;
}

uint32_t 
WSDisplayWidth(Display* d) 
{
    return d->width;
}

