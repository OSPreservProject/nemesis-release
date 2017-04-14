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
**      Kbd driver interface
** 
** ENVIRONMENT: 
**
**      Window system server
** 
** ID : $Id: kbd.c 1.1 Tue, 13 Apr 1999 15:41:47 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>
#include <IOMacros.h>
#include <IDCMacros.h>
#include <io.h>
#include <Closure.ih>
#include <stdio.h>
#include "WSSvr_st.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
 
/* 
 * Prototypes
 */
void   KbdInit (WSSvr_st *wst);

static void KbdHandler_m (WSSvr_st *wst);

/*
 * State 
 */
void KbdInit (WSSvr_st *wst)
{
    IO_clp	 io = NULL;
    Type_Any     any;
    IDCOffer_clp offer;
    Kbd_clp      kbd = NULL;

    TRC(eprintf("In KbdInit, Pvs(thds)=%p, KbdHandler_m=%p wst=%p\n",Pvs(thds),
	       KbdHandler_m, wst));

    TRC(eprintf("WS: KbdInit: binding to >dev>kbd\n"));
    
    if (!Context$Get(Pvs(root),"dev>kbd",&any)) {
	fprintf(stderr,"WS: dev>kbd not present\n");
	return;
    }

    offer=NARROW(&any, IDCOffer_clp);
    
    kbd = IDC_BIND(offer, Kbd_clp);

    io = IO_BIND(Kbd$GetOffer(kbd));
    
    wst->kbd_st.kbd = kbd;
    wst->kbd_private_st = io;
    
    /* Prime the IO with some keyboard event descriptors */
    IO_PRIME(io, sizeof(Kbd_Event));

    /* Fork the keyboard handler thread */
    Threads$Fork(Pvs(thds), KbdHandler_m, wst, 0);

    TRC(printf ("WS: KbdInit: done\n"));
}

static void KbdHandler_m (WSSvr_st *wst)
{
    IO_clp    io;
    IO_Rec	rec;
    word_t	value;

    io = wst->kbd_private_st;
    
    while (True) {
	Kbd_Event     *ev;
	uint32_t slots;

	/* Await a kbd event from the driver */
	if (IO$GetPkt (io, 1, &rec, FOREVER, &slots, &value)) {
	    if(slots > 1) {
		RAISE ("KbdAwaitEventGotTooMany", NULL);
	    }
	
	    ev = (Kbd_Event *)rec.base;
	    TRC(printf("KbdHandler: %lx %d %d\n", 
		       ev->time, ev->state, ev->key));
	
	    HandleKbdEvent(wst, ev);
	
	    /* Ack the event */
	    /* 	IO$PutPkt (io, 1, &rec, 0, FOREVER); */
	    IO$PutPkt (io, 1, &rec, 0, (NOW() + SECONDS(2)));
	}
    }
    
}

/* End */
