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
**      Mouse driver interface
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: mouse.c 1.1 Tue, 13 Apr 1999 15:41:47 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>
#include <IOMacros.h>
#include <IDCMacros.h>

#include <stdio.h>
#include <WS.ih>

#include "WSSvr_st.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
 
/* 
 * Prototypes
 */
void MouseInit (WSSvr_st *wst);

static void MouseHandler_m (WSSvr_st *wst);

void MouseInit (WSSvr_st *wst)
{
    IOOffer_clp offer;
    IO_clp      io = NULL;
    Type_Any    mouse_any;
    
    TRC(printf ("WS: MouseHandler: binding to >dev>mouse_{serial,ps2}\n"));

    if (Context$Get(Pvs(root), "dev>mouse_serial", &mouse_any)) {
	TRC(printf("WS: going for serial mouse\n"));
	offer=NARROW(&mouse_any, IOOffer_clp);
	io = IO_BIND(offer);
#if 0
	CX_ADD_IN_CX(Pvs(root), "svc>wsmouse", 
	"dev>mouse_serial", string_t);
#endif /* 0 */	
    } else if (Context$Get(Pvs(root), "dev>mouse_ps2", &mouse_any)) {
	TRC(printf("WS: going for ps2 mouse\n"));
	offer=NARROW(&mouse_any, IOOffer_clp);
	io = IO_BIND(offer);
#if 0
	CX_ADD_IN_CX(Pvs(root), "svc>wsmouse", 
	"dev>mouse_ps2", string_t);
#endif /* 0 */
    } else {
	printf("WS: No mouse.\n");
    }

    if (!io) {
	return;
    }
    
    TRC(printf ("WS: MouseHandler: done\n"));
    
    /* Prime the IO with some mouse event descriptors */
    IO_PRIME(io, sizeof(Mouse_Event));

    wst->mouse_private_st = io;

    /* Fork the mouse handler thread */
    Threads$Fork(Pvs(thds), MouseHandler_m, wst, 0);

}

static void MouseHandler_m (WSSvr_st *wst)
{
    IO_clp io = wst->mouse_private_st;
    IO_Rec	rec;
    word_t	value;
    uint32_t	nrecs;
    Mouse_Event ev, *next_ev;

    while (True) {
	/* Await a mouse event from the driver */
	if((!IO$GetPkt (io, 1, &rec, FOREVER, &nrecs, &value)) ||
	   (nrecs != 1)) {
	    fprintf(stderr, "WSSvr: Mouse Handler failed to get rec!\n");
	    return;
	}
	
	if (rec.len != sizeof(Mouse_Event)) {
	    fprintf(stderr, 
		    "WSSvr: Mouse Handler got bogus length rec (%u)!\n",
		    rec.len);
	    return;
	}
	
	ev = *(Mouse_Event *) rec.base;
	IO$PutPkt(io, 1, &rec, 1, FOREVER);
	
	while (IO$GetPkt(io, 1, &rec, 0, &nrecs, &value)) {

	    if(nrecs != 1) {
		fprintf(stderr, "WSSvr: Mouse Handler failed to get rec!\n");
		return;
	    }

	    if (rec.len != sizeof(Mouse_Event)) {
		fprintf(stderr, 
			"WSSvr: Mouse Handler got bogus length rec (%u)!\n",
			rec.len);
		return;
	    }
		
	    next_ev = (Mouse_Event *) rec.base;
	    
	    if (next_ev->buttons == ev.buttons) {
		
		/* Merge these events */
		
		ev.dx += next_ev->dx;
		ev.dy += next_ev->dy;
		ev.time = next_ev->time;
		
		IO$PutPkt(io, 1, &rec, 1, FOREVER);
		
	    } else {

		/* Mouse buttons have changed */
		HandleMouseEvent(wst, &ev);
		
		ev = *next_ev;
		IO$PutPkt(io, 1, &rec, 1, FOREVER);
		
	    }
	}
	
	/* Handle the mouse report */
	HandleMouseEvent(wst, &ev);
	
    }
    
}

/* End */
