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
**      Event delivery mechanism.
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: events.c 1.1 Tue, 13 Apr 1999 15:41:47 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <time.h>
#include <ecs.h>
#include <IOMacros.h>
#include <stdio.h>

#include <IO.ih>
#include <IOMacros.h>

#include "WSSvr_st.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
 
void HandleRootWindowEvent(WSSvr_st *st, WS_Event *ev);

/* 
 * Handle keyboard events 
 */
void HandleKbdEvent( WSSvr_st *st, Kbd_Event *ev )
{
    WS_Event      wsev;

    /* Simply sent a WSEvent */
    wsev.t       = ev->time;
    wsev.d.tag   = ((ev->state == Kbd_State_Down) ? 
		    WS_EventType_KeyPress :
		    WS_EventType_KeyRelease);
    wsev.d.u.KeyPress = ev->key;
    
    MU_LOCK(&st->mu);
    EventDeliver(st, st->focus, &wsev, True);
    MU_RELEASE(&st->mu);
}


/* 
 * Handle mouse events.  Keep track of x, y and button state 
 */
void HandleMouseEvent( WSSvr_st *st, Mouse_Event *ev )
{
    Mouse_st *mst = &st->mouse_st;
    /* the volatile on the next line is to work around a bug in the
     * arm backend of gcc in version 2.7.2 - it is fixed in 2.7.90 */
    volatile int32_t newx, newy;

    /*
     * Handle mouse motion
     */
    if (ev->dx || ev->dy) {

	FB_MgrPkt *pkt;
	IO_Rec    rec;
	word_t value;
	
	newx = mst->x + ev->dx;
	if( newx < 0 ) {
	    newx = 0;
	}
	if( newx >= st->fb_st.width ) {
	    newx = st->fb_st.width-1;
	}
	
	newy = mst->y - ev->dy;
	if( newy < 0 ) newy = 0;
	if( newy >= st->fb_st.height )
	    newy = st->fb_st.height-1;

	mst->x = newx;
	mst->y = newy;

	if(st->mgr_io) {
	    uint32_t slots;
	    IO$GetPkt(st->mgr_io, 1, &rec, FOREVER, &slots, &value);
	    
	    pkt = (FB_MgrPkt *)rec.base;
	    
	    pkt->tag = FB_MgrPktType_Mouse;
	    pkt->u.Mouse.index = st->fb_st.cursor;
	    pkt->u.Mouse.x = st->mouse_st.x;
	    pkt->u.Mouse.y = st->mouse_st.y;
	    
	    rec.base = pkt;
	    rec.len = sizeof(FB_MgrPkt);
	    
	    /* This won't block as long as the FB has implemented an
               IOCallPriv */
	    
	    IO$PutPkt(st->mgr_io, 1, &rec, 1, FOREVER);
	} else {

	    /* The FB has not provided a manager stream for some reason */
	    FB$SetCursor(st->fb_st.fb, st->fb_st.cursor, st->mouse_st.x,
			 st->mouse_st.y); 
	}
	
	MU_LOCK(&st->mu);

	Refocus(st);

	MU_RELEASE(&st->mu);

    }

    /*
     * Handle buttons
     */
    
    mst->buttons = ev->buttons;
    
    /* Send a WSEvent */
    {
	WS_Event      wsev;
	WS_MouseData  *md = &wsev.d.u.Mouse;
	
	wsev.d.tag  = WS_EventType_Mouse;
	wsev.t      = ev->time;
	md->buttons = mst->buttons;
	md->x       = mst->x - st->focus->x;
	md->y       = mst->y - st->focus->y;
	md->absx    = mst->x;
	md->absy    = mst->y;
	
	MU_LOCK(&st->mu);
	EventDeliver(st, st->focus, &wsev, True);
	MU_RELEASE(&st->mu);
    }    

}

/* Work out which window is in focus */

/* LL = server st->mu */

void Refocus(WSSvr_st *st)
{
    WSSvr_Window  *newfocus = &st->window[ROOT_WINDOW];
    WSSvr_Window  *wtmp;
    struct chain *headp = &st->stack;
    struct chain *wcp;
    uint32_t x;
    uint32_t y;
    
    x = st->mouse_st.x;
    y = st->mouse_st.y;
    
    for (wcp = headp->next; wcp != headp; wcp = wcp->next) {
	wtmp = STACK2WINDOW(wcp);
	
	if(wtmp->mapped && Region$TestPoint(st->region, &wtmp->clip_region, 
					    x, y))
	{ 
	    newfocus = wtmp; 
	    break; 
	}
    }

    if (newfocus != st->focus) {
	WS_Event wsev;
	
	TRC(printf("<<<<LEAVE window %d\n", st->focus->wid));
	
	wsev.t = NOW();

	wsev.d.tag = WS_EventType_LeaveNotify;
	
	if(EventDeliver(st, st->focus, &wsev, False)) {
	    st->focus = &st->window[ROOT_WINDOW];
	
	    wsev.d.tag = WS_EventType_EnterNotify;

	    if(EventDeliver(st, newfocus, &wsev, False)) {

		st->focus = newfocus;

	    }
	}

    }
}

/* LL = st->mu */

bool_t EventDeliver( WSSvr_st *st, WSSvr_Window *w, WS_Event *ev, bool_t block) 
{
    Event_Val val;

    if(w->wid == ROOT_WINDOW) {
	HandleRootWindowEvent(st, ev);
    }
 
    ev->w = w->wid;

    if((!block) || (Pvs(thd) == st->event_thread)) {
	/* We can't block here, so optionally throw this event away */
	Threads$EnterCS(Pvs(thds), False);
	val = SQ_READ(st->free_event_sq);

	if(EC_READ(st->free_event_ec) < val) {

	    /* We would have to block - refuse the event */
	    Threads$LeaveCS(Pvs(thds));
	    return False;
	} else {
	    val = SQ_TICKET(st->free_event_sq);
	    Threads$LeaveCS(Pvs(thds));
	}
    } else {

	val = SQ_TICKET(st->free_event_sq);
    }

    EC_AWAIT(st->free_event_ec, val);
    
    val = val % NUM_EVENT_RECS;
    st->events[val] = *ev;
    
    EC_ADVANCE(st->valid_event_ec, 1);
    
    return True;
}

void EventThread(WSSvr_st *st) {

    Event_Val val = 0;
    
    WS_Event *ev;
    
    st->event_thread = Pvs(thd);
    while(1) {

	WSSvr_Window *w;

	EC_AWAIT(st->valid_event_ec, val + 1);
	ev = &st->events[val % NUM_EVENT_RECS];

	w = &st->window[ev->w];

	if(ev->d.tag == WS_EventType_Expose) {
	    /* Get the exposure rectangle */
	    Threads$EnterCS(Pvs(thds), False);
	    ev->d.u.Expose = w->expose_rect;
	    w->expose_due = False;
	    Threads$LeaveCS(Pvs(thds));
	}

	if(ev->d.tag == WS_EventType_Mouse) {

	    /* Try coalescing mouse events */
	    
	    while(EC_READ(st->valid_event_ec) > (val+1)) {
		WS_Event *ev_next;
		val++;
		
		ev_next = &st->events[val % NUM_EVENT_RECS];
		
		/* See if this is a mouse event in the same window */
		if((ev_next->d.tag == WS_EventType_Mouse) && 
		   (ev_next->d.u.Mouse.buttons == ev->d.u.Mouse.buttons)) {
		    
		    /* Free old event */
		    EC_ADVANCE(st->free_event_ec, 1);
		    
		    /* Point at new event */
		    ev = ev_next;
		} else {
		    /* Push this event back */
		    val--;
		    break;
		}
	    }
	}

	WM$HandleEvent(st->wm, ev, w->owner ? (&w->owner->wsf_cl) : NULL);
	
	/* Free event */
	EC_ADVANCE(st->free_event_ec, 1);
	
	val++;
#if 0
	if(st->need_refocus) {
	    MU_LOCK(&st->mu);
	    Refocus(st);
	    MU_RELEASE(&st->mu);
	}
#endif

    }
	
}

void HandleRootWindowEvent(WSSvr_st *st, WS_Event *ev) 
{
    
    switch(ev->d.tag) {
    case WS_EventType_Expose: 
    {
	WS_Rectangle *r = &ev->d.u.Expose;

	ExposeBackdrop(st, r->x1, r->y1, r->x2, r->y2);
	break;
    }
#if 0
    case WS_EventType_Mouse: 
    {
	
	MouseEvent(st, ev);
	break;
#endif
    default:
	TRC(printf("Got event, tag is %d\n", ev->d.tag));
    }

}
    



