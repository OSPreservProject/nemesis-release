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
**      Window functions.
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: windows.c 1.1 Tue, 13 Apr 1999 15:41:47 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <links.h>
#include <time.h>
#include <assert.h>
#include <IOMacros.h>
#include <stdio.h>
#include <regionstr.h>

#include <IO.ih>
#include <IOMacros.h>

#include "WSSvr_st.h"

/* LL = st->mu */ 

void ExposeRegion(WSSvr_st *st, WSSvr_Window *w, Region_T r) {

    Region_T new_fb = Region$New(st->region, NULL);

    Region$Copy(st->region, new_fb, r); 
    
    if(w->fb_expose_region != NULL) {
	fprintf(stderr, "****Non-NULL fb_expose_region in window %p****\n", w);
    }

    w->fb_expose_region = new_fb;

    w->update_needed = True;
    
}

/* LL = st->mu */

void HandleWindowExposures(WSSvr_st *st, WSSvr_Window *w, WSSvr_Window *first)
{

    HandleRegionExposures(st, &w->clip_region, first);

}

#define DB(x) TRC(printf(x)) 
#define PRINT_RGN(rgn) TRC(Region$Print(st->region, rgn))

/* LL = st->mu */

void HandleRegionExposures(WSSvr_st *st, Region_T r, WSSvr_Window *first)
{
    struct chain *current;

    /* just add to updates list */

    DB("Current updates_region:\n");
    PRINT_RGN(st->updates_region);
    DB("Adding region:\n");
    PRINT_RGN(r);

    Region$Union(st->region, st->updates_region, st->updates_region, r);
    
    DB("Result:\n");
    PRINT_RGN(st->updates_region);

    if(st->first_exposed) {
	
	current = &first->stack;
	
	while(current != &st->stack) {
	    
	    if(current == &st->first_exposed->stack) {
		
		/* The window being exposed is on top of the previous
		   window being exposed - make it the new exposed
		   window */
		st->first_exposed = first;
		
		break;
	    }
	    
	    current = current->next;
	    
	}
    } else {
	
	st->first_exposed = first;

    }

    if(!st->update_lock_count) {
	ProcessUpdates(st);
    }
}

static void DispatchUpdate(WSSvr_st *st, WSSvr_Window *w) {

    WS_Event wsev;
    bool_t event_needed;
    WS_Rectangle rect;
    Region_T r = w->fb_expose_region;

    if(w->fb_expose_region) {

	rect.x1 = r->extents.x1 - w->x;
	rect.y1 = r->extents.y1 - w->y;
	rect.x2 = r->extents.x2 - w->x;
	rect.y2 = r->extents.y2 - w->y;
	    
	Region$Free(st->region, w->fb_expose_region);
	w->fb_expose_region = NULL;

	/* Enter a critical section to avoid a race with the event
           thread */
	Threads$EnterCS(Pvs(thds), False);
	event_needed = !w->expose_due;
	if(w->expose_due) {
	    w->expose_rect.x1 = MIN(w->expose_rect.x1, rect.x1);
	    w->expose_rect.x2 = MAX(w->expose_rect.x2, rect.x2);
	    w->expose_rect.y1 = MIN(w->expose_rect.y1, rect.y1);
	    w->expose_rect.y2 = MAX(w->expose_rect.y2, rect.y2);
	} else {
	    w->expose_due = True;
	    w->expose_rect = rect;
	}
	Threads$LeaveCS(Pvs(thds));
	
	if(event_needed) {
	    wsev.d.tag = WS_EventType_Expose;
	    wsev.t = NOW();
	    
	    if(w->wid != ROOT_WINDOW) {
		if(!EventDeliver(st, w, &wsev, False)) {
		    /* We were unable to deliver the event - schedule
		       another update for this window later ... */
		
		    st->update_lock_count++;
		    HandleWindowExposures(st, w, w);
		    st->update_lock_count--;
		}
	    } else {
		/* This is an expose event for the root window - don't
		   bother sending it out on the usual event channel */
		wsev.d.u.Expose = w->expose_rect;
		w->expose_due = False;
		HandleRootWindowEvent(st, &wsev);
	    }
	}
    }
}

/* ProcessUpdates sends updates to the frame buffer, and causes the
   update_region of each window to reflect the oustanding update event
   due to it. */

/* LL = st->mu */

void ProcessUpdates(WSSvr_st *st) {
	
    struct chain *first_exposed, *current;
    Region_Rec  avail_reg, expose_reg, portion_reg, change_reg;
    
    WSSvr_Window *w;
    bool_t     exposing = False;
    FB_WinUpdatePkt upd;
    
    Region$InitRec(st->region, &avail_reg, NULL);
    Region$InitRec(st->region, &portion_reg, NULL);
    Region$InitRec(st->region, &change_reg, NULL);
    Region$InitRec(st->region, &expose_reg, NULL);
    
    TRC(printf("Doing updates\n"));

    current = st->stack.next;
    first_exposed = &st->first_exposed->stack;
    st->first_exposed = NULL;
    
    /* clip to screen area */

    DB("Updates region\n");
    PRINT_RGN(st->updates_region);    
    
    Region$SetXY(st->region, &avail_reg, 0, 0, 
		 st->fb_st.width, st->fb_st.height);
    Region$Intersect(st->region, &avail_reg, &avail_reg, 
		     st->updates_region);

    Region$MakeEmpty(st->region, st->updates_region);
    
    while((current != &st->stack) && (!REGION_NIL(&avail_reg))) {
	
	/* Don't bother sending expose events for windows that can't
           have changed */

	if(current == first_exposed) {
	    exposing = True;
	}
	
	w = STACK2WINDOW(current);
	
	if(w->mapped) {
	    
	    if(!REGION_NIL(&avail_reg)) {
		/* expose relevant portions */
		
		DB("Intersecting with window clip region\n");
		PRINT_RGN(&w->clip_region);
		
		Region$Intersect(st->region, &expose_reg, 
				 &avail_reg, &w->clip_region);
		
		DB("Intersection is\n");
		PRINT_RGN(&expose_reg);
		
		if(exposing && !REGION_NIL(&expose_reg)) {
		    
		    ExposeRegion(st, w, &expose_reg);
		}
	    }
	    
	    /* remove portions from available screen area */
	    
	    Region$Subtract(st->region, &avail_reg, 
			    &avail_reg, &w->clip_region);
	    
	    DB("Subtraction result is\n");
	    PRINT_RGN(&avail_reg);
			    
	}
	
	current = current->next;
    }
    
    /* Give anything left to the root window */
    
    
    if(!REGION_NIL(&avail_reg)) {
	DB("Exposing root window\n");
	
	ExposeRegion(st, &st->window[ROOT_WINDOW], &avail_reg);
    } else {
	DB("Available region empty before reaching root window\n");
    }
    
    /* Screen area in region is now portioned out to appropriate windows
       Next build update packet and call into frame buffer */
    
    
    /* The state for the world is now safe for other threads to
       call Handle{Region|Window}Exposures() */
    
    
    current = st->stack.next;
    
    SEQ_INIT(&upd, 0, Pvs(heap));
    
    while(current != &st->stack) {
	
	w = STACK2WINDOW(current);
	
	if(w->update_needed) {
	    
	    FB_MgrWinData data;
	    
	    data.wid = w->fbid;
	    data.x = w->x;
	    data.y = w->y;
	    data.width = w->width;
	    data.height = w->height;
	    data.exposemask = w->fb_expose_region;
	    
	    if(data.exposemask && REGION_NIL(data.exposemask)) {
		data.exposemask = NULL;
	    }
	    
	    SEQ_ADDH(&upd, data);
	    w->update_needed = False;
	    
	}
	
	current = current->next;
	
    }
    
    w = &st->window[ROOT_WINDOW];
    if(w->update_needed) {
	
	FB_MgrWinData data;
	
	data.wid = w->fbid;
	data.x = w->x;
	data.y = w->y;
	data.width = w->width;
	data.height = w->height;
	data.exposemask = w->fb_expose_region;
	
	SEQ_ADDH(&upd, data);
	w->update_needed = False;
    }
    
    FB$UpdateWindows(st->fb_st.fb, &upd);
    
    SEQ_FREE_DATA(&upd);
    
    /* Now that the frame buffer is set up, send out expose events */
    
    current = st->stack.next;
    
    while(current != &st->stack) {
	
	w = STACK2WINDOW(current);
	
	DispatchUpdate(st, w);
	
	current = current->next;
    }
    
    DispatchUpdate(st, &st->window[ROOT_WINDOW]);

    Region$DoneRec(st->region, &expose_reg);
    Region$DoneRec(st->region, &avail_reg);
    Region$DoneRec(st->region, &portion_reg);
    Region$DoneRec(st->region, &change_reg);
    
}

void Window_Raise(WSSvr_st *st, WSSvr_Window *w)
{

    MU_LOCK(&st->mu);
    
    if(w->stack.prev != &st->stack) {
	/* Move to top of stacking list */
	LINK_REMOVE(&w->stack);
	LINK_ADD_TO_HEAD(&st->stack, &w->stack);
	
	if(w->mapped) {
	    HandleWindowExposures(st, w, w);
	    
	}
    }
    
    Refocus(st);

    MU_RELEASE(&st->mu);
    
}

    

void Window_Lower(WSSvr_st *st, WSSvr_Window *w)
{
  struct chain *first_exposed;

  MU_LOCK(&st->mu);
      
  if(w->stack.next != &st->stack) {

      first_exposed = w->stack.next;
      /* Move to bottom of stacking list */
      LINK_REMOVE(&w->stack);
      LINK_ADD_TO_TAIL(&st->stack, &w->stack);
      
      if(w->mapped) {

	  HandleWindowExposures(st, w, STACK2WINDOW(first_exposed));

      }
  }

  Refocus(st);

  MU_RELEASE(&st->mu);
  
}

void Window_Move(WSSvr_st *st, WSSvr_Window *w, int32_t x, int32_t y)
{

    int32_t x1, x2, y1, y2;
    Region_Rec union_reg;

    x &= st->fb_st.xppwmask;
    y &= st->fb_st.yppwmask;

    x1 = MIN(w->x, x);
    y1 = MIN(w->y, y);
    x2 = MAX(w->x, x) + w->width;
    y2 = MAX(w->y, y) + w->height;

    Region$InitRec(st->region, &union_reg, NULL);

    MU_LOCK(&st->mu);
    
    Region$Copy(st->region, &union_reg, &w->clip_region);
    Region$Translate(st->region, &w->clip_region, (x - w->x), (y - w->y));
    Region$Union(st->region, &union_reg, &union_reg, &w->clip_region);
    
    w->x = x;
    w->y = y;

    if(w->mapped) {
	w->update_needed = True;
	
	HandleRegionExposures(st, &union_reg, w);
	
    }

    Refocus(st);

    MU_RELEASE(&st->mu);

    Region$DoneRec(st->region, &union_reg);
    
}

