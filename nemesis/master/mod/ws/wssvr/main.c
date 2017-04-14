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
**      Server initialisation
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: main.c 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <time.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <IOMacros.h>
#include <stdio.h>

#include <FB.ih>
#include <IO.ih>
#include <IOMacros.h>

/*****************************************************************/
/*
 * WS interface
 */

#include <WS.ih>
#include <WSF.ih>
#include <WMMod.ih>
#include <WM.ih>

#include <environment.h>

static  WS_EventStream_fn       WS_EventStream_m;
static  WS_CreateWindow_fn      WS_CreateWindow_m;
static  WS_UpdateStream_fn      WS_UpdateStream_m;
static  WS_DestroyWindow_fn     WS_DestroyWindow_m;
static  WS_MapWindow_fn         WS_MapWindow_m;
static  WS_UnMapWindow_fn       WS_UnMapWindow_m;
static  WS_MoveWindow_fn        WS_MoveWindow_m;
static  WS_ResizeWindow_fn      WS_ResizeWindow_m;
static  WS_RaiseWindow_fn       WS_RaiseWindow_m;
static  WS_LowerWindow_fn       WS_LowerWindow_m;
static  WS_AdjustClip_fn        WS_AdjustClip_m;
static  WS_SetTitle_fn          WS_SetTitle_m;
static  WS_Info_fn              WS_Info_m;
static  WS_Close_fn             WS_Close_m;
static  WS_WinInfo_fn           WS_WinInfo_m;
static  WSF_DeliverEvent_fn     WSF_DeliverEvent_m;
static  WSF_LockUpdates_fn      WSF_LockUpdates_m;
static  WSF_UnlockUpdates_fn    WSF_UnlockUpdates_m;

static WSF_op wsf_ms = {         
    WS_EventStream_m,
    WS_CreateWindow_m,
    WS_DestroyWindow_m,
    WS_UpdateStream_m,
    WS_MapWindow_m,
    WS_UnMapWindow_m,
    WS_MoveWindow_m,
    WS_ResizeWindow_m,
    WS_RaiseWindow_m,
    WS_LowerWindow_m,
    WS_AdjustClip_m,
    WS_SetTitle_m,
    WS_Info_m,
    WS_Close_m,
    WS_WinInfo_m,
    WSF_DeliverEvent_m,
    WSF_LockUpdates_m,
    WSF_UnlockUpdates_m
};

static	IDCCallback_Request_fn 		IO_Request_m;
static	IDCCallback_Bound_fn 		IO_Bound_m;
static	IDCCallback_Closed_fn 		IO_Closed_m;

static	IDCCallback_op	iocb_ms = {
    IO_Request_m,
    IO_Bound_m,
    IO_Closed_m
};

static IDCCallback_Request_fn WS_IDCRequest_m;
static IDCCallback_Bound_fn WS_IDCBound_m;
static IDCCallback_Closed_fn WS_IDCClosed_m;

static IDCCallback_op WS_callback_ms = { 
    WS_IDCRequest_m, 
    WS_IDCBound_m, 
    WS_IDCClosed_m 
};


/*****************************************************************/
/* Server State */

#include <WSSvr_st.h>

/*****************************************************************/
/*
 * WSSvr interface
 */

MAIN_RUNES(WSSvr);

/* Null Window Manager */

static  WM_NewClient_fn                 NullWM_NewClient_m;
static  WM_HandleEvent_fn               NullWM_HandleEvent_m;

static  WM_op   nullwm_ms = {
    NullWM_NewClient_m,
    NullWM_HandleEvent_m
};

const static WM_cl nullwm_cl = { &nullwm_ms, NULL };

/*****************************************************************/

/*****************************************************************/

void
Main (Closure_clp self)
{
    WSSvr_st       *st;
    IOOffer_clp    mgr_offer;
    FB_MgrPkt      *mgr_pkt;
    IO_Rec          rec;
    int             i;
    
    TRC(printf("WSSvr: entered\n"));
    
    st = Heap$Malloc(Pvs(heap), sizeof(WSSvr_st));
    
    TRC(printf("WSSvr_st = %p\n", st));
    
    if (st == NULL) {
	fprintf(stderr,"WSSvr: no memory for state\n");
	RAISE_Heap$NoMemory();
    }
    
    memset(st, 0, sizeof(WSSvr_st));
    
    CL_INIT(st->callback_cl, &WS_callback_ms, st);
    
    /* We need the Region library */
    st->region = NAME_FIND("modules>Region", Region_clp);

    /* Init the lock on server state */
    
    MU_INIT(&st->mu);
    
    st->free_event_ec = EC_NEW();
    st->valid_event_ec = EC_NEW();
    st->free_event_sq = SQ_NEW();
    EC_ADVANCE(st->free_event_ec, NUM_EVENT_RECS - 1);

    /* Initialise the window datastructures */
    for (i = 0 ; i < WSSVR_MAX_WINDOWS; i++) {
	WSSvr_Window *w = &st->window[i];
	w->wid  = i;
	w->free = True;
    }
    
    /* Initialise the window stacking order list */
    LINK_INITIALISE(&st->stack);
    st->focus = NULL;
    
    /* Initialise client list */
    LINK_INITIALISE(&st->clients);

    /* Initialise the framebuffer device */
    FBInit(st);
    
    st->mouse_st.x = st->fb_st.width >> 1;
    st->mouse_st.y = st->fb_st.height >> 1;
    st->mouse_st.buttons = 0;

    /* Continue setting up the framebuffer */
    
    mgr_offer = FB$MgrStream(st->fb_st.fb);
    
    TRC(printf("mgr_offer = %p\n", mgr_offer));
    
    if (mgr_offer) {
	st->mgr_io = IO_BIND(mgr_offer);
	
	/* Preload the mgr stream with some dummy mouse packets */
	
	mgr_pkt = Heap$Malloc(Pvs(heap), sizeof(FB_MgrPkt) * NUM_MGR_PKTS);
	
	for (i = 0; i < NUM_MGR_PKTS; i++) {
	    mgr_pkt[i].tag = FB_MgrPktType_Mouse;
	    mgr_pkt[i].u.Mouse.index = st->fb_st.cursor;
	    mgr_pkt[i].u.Mouse.x = st->mouse_st.x;
	    mgr_pkt[i].u.Mouse.y = st->mouse_st.y;
	    
	    rec.base = &mgr_pkt[i];
	    rec.len = sizeof(FB_MgrPkt);
	    IO$PutPkt(st->mgr_io, 1, &rec, 1, FOREVER);
	}
    } else {
	st->mgr_io = NULL;
    }
    
    st->update_lock_count = 0;
    st->updates_region = Region$New(st->region, NULL);
    st->first_exposed = NULL;
    
    MU_INIT(&st->root_io_mu);
    
    /* Create the root window */
    {
	WSSvr_Window *w = &st->window[ROOT_WINDOW];
	uint32_t width = st->fb_st.width;
	uint32_t height = st->fb_st.height;
	IOOffer_clp    offer;
	Region_T  reg;
	Region_Box box;
	FB_Protocol NOCLOBBER proto = 0;
	
	if(st->fb_st.depth == 8) {
	    proto = FB_Protocol_Bitmap8;
	} else if(st->fb_st.depth == 16) {
	    proto = FB_Protocol_Bitmap16;
	} else {
	    TRC(printf("WSSvr: Unknown screen depth\n"));
	    RAISE_WS$Failure();
	}
	
	TRC(printf("Creating root window\n"));
	
	TRY {
	    w->fbid = FB$CreateWindow(st->fb_st.fb, 
				      0, 0, 
				      width, 
				      height, 
				      False);
	} CATCH_FB$Failure() {
	    RAISE_WS$Failure();
	} ENDTRY;
	
	FB$MapWindow(st->fb_st.fb, w->fbid);

	box.x1 = 0;
	box.y1 = 0;
	box.x2 = width;
	box.y2 = height;
	
	reg = Region$New(st->region, &box);
	FB$ExposeWindow(st->fb_st.fb, w->fbid, reg);
	Region$Free(st->region, reg);
	
	TRY {
	    w->sid = FB$UpdateStream(st->fb_st.fb, w->fbid, 
				     proto, 0, 
				     False, &offer, &st->blit_root);
	    
	    TRY {
		st->io_root = IO_BIND(offer);
	    } CATCH_TypeSystem$Incompatible() {
		st->io_root = NULL;
	    } ENDTRY;
	    
	} CATCH_ALL {
	    FB$DestroyWindow(st->fb_st.fb, w->fbid);
	} ENDTRY;
    
	/* Update window struct */
	w->free   = False;
	w->owner  = NULL;
	w->x      = 0;
	w->y      = 0;
	w->width  = width;
	w->height = height;
	w->wid    = ROOT_WINDOW;
	w->fb_expose_region = NULL;
	w->expose_due = False;
	
	st->focus = w;		/* Focussed on root window */
    }
    
    /* Initialise the backdrop */
    InitBackdrop(st);
    
    /* Initialise window manager */
    
    TRC(printf("Initialising window manager\n"));
    
    st->wm = NULL;
    
    TRY {
	WS_st *ws_st;
	WMMod_clp NOCLOBBER wmmod = NULL;
	
	ws_st = Heap$Malloc(Pvs(heap), sizeof(WS_st));
	if(!ws_st) {
	    RAISE_Heap$NoMemory();
	}
	memset(ws_st, 0, sizeof(*ws_st));
	CL_INIT(ws_st->wsf_cl, &wsf_ms, ws_st);
	ws_st->svr_st = st;
	ws_st->evio   = NULL;
	
	LINK_INITIALISE(&ws_st->windows);
	
	TRY {
	    wmmod = NAME_FIND("modules>WMMod", WMMod_clp);
	} CATCH_Context$NotFound(UNUSED c) {
	    fprintf(stderr, 
		    "WSSvr: Can't find window manager - using NullWM\n");
	} ENDTRY;
	
	if(wmmod != NULL) {
	    st->wm = WMMod$New(wmmod, &ws_st->wsf_cl);
	}
	
    } CATCH_ALL { 
	fprintf(stderr, 
		"WSSvr: Exception %s starting window manager - using NullWM\n",
		exn_ctx.cur_exception);
    } ENDTRY;
    
    /* Use NullWM if window manager not initialised */
    
    if(!st->wm) {
	st->wm = &nullwm_cl;
    }
    
    /* Mouse and keyboard, if present */
    TRC(printf("Initialising mouse\n"));
    MouseInit(st);
    
    TRC(printf("Initialising keyboard\n"));
    KbdInit(st);
    
    /* Start the event thread */
    Threads$Fork(Pvs(thds), EventThread, st, 0);
    
    /* Export the WS server interface */
    TRC(printf("WSSvr: Exporting svc>WS...\n"));
    
    {
	IDCTransport_clp	shmt;			
	Type_Any       	any, offer_any;		
	IDCOffer_clp   	offer;			
	IDCService_clp 	service;		
	
	shmt = NAME_FIND ("modules>ShmTransport", IDCTransport_clp); 
	ANY_INIT(&any, WS_clp, NULL);		
	offer = IDCTransport$Offer (shmt, &any, &st->callback_cl, 
				    Pvs(heap), Pvs(gkpr), 
				    Pvs(entry), &service); 
	
	ANY_INIT(&offer_any,IDCOffer_clp,offer);	
	Context$Add (Pvs(root), "svc>WS", &offer_any);	
	
    }
    
    TRC(printf("WSSvr: Exported svc>WS\n"));
    
}

/*****************************************************************/
/* IDCCallback method for IO */

static bool_t IO_Request_m (IDCCallback_cl *self, IDCOffer_clp offer,
			    Domain_ID dom, ProtectionDomain_ID pdom,
			    const Binder_Cookies *clt_cks, 
			    Binder_Cookies *svr_cks)
{
    return True;
}

static bool_t IO_Bound_m (IDCCallback_cl *self, IDCOffer_clp offer,
			  IDCServerBinding_clp binding, Domain_ID dom,
			  ProtectionDomain_ID pdom, 
			  const Binder_Cookies *clt_cks, Type_Any *server)    
{
    WS_st  *st = self->st;
    IO_cl    *io = NARROW(server, IO_clp);

    st->evio = io;

    return True;
}

static void IO_Closed_m (IDCCallback_cl *self, IDCOffer_clp offer,
			 IDCServerBinding_clp binding, const Type_Any *server)
{
    return;
}

/*****************************************************************/
/* WS methods */
IOOffer_clp WS_EventStream_m (WS_cl *self) 
{
    WS_st         	*st = self->st;
    IOTransport_clp      iot;
    int slots = 20;
    int bytes = slots * sizeof(WS_Event);
	
    IOData_Area *area;
    IOData_Shm *shm;

    shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
    area = &SEQ_ELEM(shm, 0);

    area->buf.base = NULL;
    area->buf.len = bytes;
    area->param.attrs = 0;
    area->param.awidth = PAGE_WIDTH;
    area->param.pwidth = PAGE_WIDTH;

    TRC(printf("WS_EventStream_m(%p)\n", self));
    
    /* Set up an event stream */
    
    if(st->evio_offer) {
	return st->evio_offer;
    }
    
    iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);    
    
    st->evio_offer = IOTransport$Offer (iot, 
					Pvs(heap), slots, 
					IO_Mode_Rx,
					IO_Kind_Slave,
					shm,
					Pvs(gkpr), &st->iocb_cl, NULL,
					0, &st->evio_service);
    
    return st->evio_offer;
}

WS_WindowID WS_CreateWindow_m (WS_cl   *self,
			       int32_t  x,
			       int32_t  y,
			       uint32_t width,
			       uint32_t height)
{
    WS_st                  *st = self->st;
    WSSvr_st           *svr_st = st->svr_st;
    WS_WindowID  NOCLOBBER wid = 0;
    Region_Box             brec;
    
    TRC(printf("WS_CreateWindow_m(%p, %d, %d, %d, %d)\n",
	       self, x, y, width, height));
    
    /* Find an unused window */

    MU_LOCK(&svr_st->mu);
    while (wid < WSSVR_MAX_WINDOWS && !svr_st->window[wid].free) wid++;
    MU_RELEASE(&svr_st->mu);

    if (wid == WSSVR_MAX_WINDOWS) RAISE_WS$Failure();
    
    {
	WSSvr_Window *w = &svr_st->window[wid];
	
	TRY {
	    w->fbid = FB$CreateWindow(svr_st->fb_st.fb, x, y, 
				      width, height, True);
	} CATCH_FB$Failure() {
	    RAISE_WS$Failure();
	} ENDTRY;

	TRC(printf("WS_CreateWindow_m: FBID %x\n", w->fbid));
	
	/* Update window struct */
	w->free   = False;
	w->x      = x;
	w->y      = y;
	w->width  = width;
	w->height = height;
	w->mapped = False;
	
	brec.x1 = x;
	brec.y1 = y;
	brec.x2 = x + width;
	brec.y2 = y + height;

	/* Initialise with a full clip region and an empty visible region */
	
	Region$InitRec(svr_st->region, &w->clip_region, &brec);
	Region$InitRec(svr_st->region, &w->visible_region, NULL);
	
	w->fb_expose_region = NULL;
	w->expose_due = False;

	/* Chain onto client window list */
	LINK_ADD_TO_HEAD(&st->windows, &w->client);
	w->owner  = st;

	MU_LOCK(&svr_st->mu);
	
	/* Place at top of stack */
	LINK_ADD_TO_HEAD(&svr_st->stack, &w->stack);
	
	MU_RELEASE(&svr_st->mu);
	
    }
    return wid;
}

void WS_DestroyWindow_m (WS_cl *self,
			 const WS_WindowID wid)
{
    WS_st            *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window      *w = &svr_st->window[wid];
    
    TRC(printf("WS_DestroyWindow_m(%p, %d)\n", self, (uint32_t) wid));
    if (w->owner != st) {
	TRC(printf("WS_DestroyWindow_m: Client is not owner\n"));
	RAISE_WS$BadWindow();
    }
    
    if(w->mapped) WS_UnMapWindow_m(self, wid);
	
    TRY {
	
	FB$DestroyWindow(svr_st->fb_st.fb, w->fbid);
    } CATCH_FB$BadWindow() {
	fprintf(stderr, "WSSvr: Fatal Error: Bad FB_WindowID\n");
    } ENDTRY;
	
    MU_LOCK(&svr_st->mu);
    
    LINK_REMOVE(&w->client);
    LINK_REMOVE(&w->stack);
    
    Refocus(svr_st);

    MU_RELEASE(&svr_st->mu);
        
    Region$DoneRec(svr_st->region, &w->clip_region);
    Region$DoneRec(svr_st->region, &w->visible_region);
    
    w->owner = NULL;
    w->free = True;

    
}

FB_StreamID WS_UpdateStream_m (WS_cl            *self,
			       const WS_WindowID wid,
			       FB_Protocol       p,
			       FB_QoS            q,
			       bool_t            clip,
			       IOOffer_clp      *offer /* OUT */,
			       FBBlit_clp       *blit  /* OUT */)
{
    WS_st            *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window      *w = &svr_st->window[wid];
    FB_StreamID       sid;
    
    TRC(printf("WS_UpdateStream_m(%p, %d, %d, %d, %d)\n", 
	       self, (int) wid, p, q, clip));
    
    if (w->owner != st) RAISE_WS$BadWindow();
    
    /* XXX Do something sensible with clip argument */
    sid = FB$UpdateStream(svr_st->fb_st.fb, w->fbid, 
			  p, q, clip, offer, blit);
    TRC(printf("WS$Update Stream offer %p\n", *offer));
    
    return sid;
}  

void WS_MapWindow_m (WS_cl *self,
		     const WS_WindowID wid)
{
    WS_st            *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window      *w = &svr_st->window[wid];
    FB_clp            fb = svr_st->fb_st.fb;
    
    TRC(printf("WS_MapWindow_m(%p, %d)\n", self, (int) wid));
    if (w->owner != st) RAISE_WS$BadWindow();
    
    TRY {
	FB$MapWindow(fb, w->fbid);
    } CATCH_FB$BadWindow() {
	fprintf(stderr, "WSSvr: Fatal Error: Bad FB_WindowID\n");
    } ENDTRY;

    w->mapped = True;
	
    MU_LOCK(&svr_st->mu);
    
    HandleWindowExposures(svr_st, w, w);
    
    Refocus(svr_st);

    MU_RELEASE(&svr_st->mu);
    
    TRC(printf("WS_MapWindow_m done\n"));
}

void WS_UnMapWindow_m (WS_cl   *self,
		       const WS_WindowID wid)
{
    WS_st *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window *w = &svr_st->window[wid];
    FB_clp       fb = svr_st->fb_st.fb;
    
    TRC(printf("WS_UnMapWindow_m(%p, %d)\n", self, (int) wid));
    if (w->owner != st) RAISE_WS$BadWindow();
    
    if(w->mapped) {
	TRY {
	    
	    FB$UnMapWindow(fb, w->fbid);
	    w->mapped = False;
	    
	    MU_LOCK(&svr_st->mu);
	    
	    Region$MakeEmpty(svr_st->region, &w->visible_region);
	    
	    /* If we're about the unmap the highest window with
               updates due, reset the first_exposed field. The
               HandleWindowExposures() call will set it to the new
               appropriate window */
	    
	    if(svr_st->first_exposed == w) svr_st->first_exposed = NULL;

	    HandleWindowExposures(svr_st, w, STACK2WINDOW(w->stack.next));
	    
	    MU_RELEASE(&svr_st->mu);
	    
	} CATCH_FB$BadWindow() {
	    fprintf(stderr, "WSSvr: Fatal Error: Bad FB_WindowID\n");
	}  ENDTRY;
    }
}

void WS_RaiseWindow_m (WS_cl   *self,
		       const WS_WindowID wid)
{
    WS_st *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window *w = &svr_st->window[wid];
    
    TRC(printf("WS_RaiseWindow_m(%p, %d)\n", self, (int) wid));
    if (w->owner != st) RAISE_WS$BadWindow();
    
    Window_Raise(svr_st, w);

}

void WS_LowerWindow_m (WS_cl   *self,
		       const WS_WindowID wid)
{
    WS_st *st    = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window *w = &svr_st->window[wid];
    
    TRC(printf("WS_LowerWindow_m(%p, %d)\n", self, (int) wid));
    if (w->owner != st) RAISE_WS$BadWindow();
    
    Window_Lower(svr_st, w);

}

void WS_MoveWindow_m (WS_cl        *self,
		      WS_WindowID   wid,
		      int32_t       x,
		      int32_t       y)
{
    WS_st *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window *w = &svr_st->window[wid];
    
    TRC(printf("WS_MoveWindow_m(%p, %d, %d, %d)\n", self, (int) wid, x, y));
    if (w->owner != st) RAISE_WS$BadWindow();
    
    Window_Move(svr_st, w, x, y);
  
}

void WS_ResizeWindow_m (WS_cl         *self,
			WS_WindowID    wid,
			int32_t        width,
			int32_t        height)
{
    WS_st *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window *w = &svr_st->window[wid];
    
    Region_Box  brec;
    Region_Rec expose_region;

    TRC(printf("WS_ResizeWindow_m(%p, %d, %d, %d)\n", self, 
	       (int) wid, width, height));

    if (w->owner != st) RAISE_WS$BadWindow();
    
    /* Update state */
    
    w->width  = width;
    w->height = height;
    w->update_needed = True;
    
    /* Build region that needs exposing */
    
    brec.x1 = w->x;
    brec.y1 = w->y;
    brec.x2 = w->x + width;
    brec.y2 = w->y + height;
    
    Region$InitRec(svr_st->region, &expose_region, NULL);

    MU_LOCK(&svr_st->mu);

    Region$Copy(svr_st->region, &expose_region, &w->clip_region);
    
    Region$SetBox(svr_st->region, &w->clip_region, &brec);
    
    Region$Union(svr_st->region, &expose_region, 
		 &w->clip_region, &expose_region);
    
    /* Deal with frame buffer and update notifications */

        
    HandleRegionExposures(svr_st, &expose_region, w);
    
    Refocus(svr_st);

    MU_RELEASE(&svr_st->mu);
    
    Region$DoneRec(svr_st->region, &expose_region);
    
}

static int32_t WS_WinInfo_m (WS_cl       *self,
			     WS_WindowID  wid,
			     /* RETURNS */
			     int32_t     *y,
			     uint32_t    *width,
			     uint32_t    *height)
{
    int x;

    WS_st        *st = (WS_st *) self->st;
    WSSvr_st     *svr_st = st->svr_st;
    WSSvr_Window *w = &svr_st->window[wid];

    x = w -> x;
    (*y) = w -> y;
    (*width) = w -> width;
    (*height) = w -> height;

    return x;
}

void WS_AdjustClip_m (WS_cl *self,
		      WS_WindowID wid,
		      Region_T region,
		      WS_ClipOp op) 
{
  WS_st            *st = self->st;
  WSSvr_st     *svr_st = st->svr_st;
  WSSvr_Window      *w = &svr_st->window[wid];

  Region_Rec bounds;
  Region_Box brec;

  TRC(printf("WS_AdjustClip_m(%p, %d, %p, %d)\n", 
	     self, (int) wid, region, op));

  if (w->owner != st) RAISE_WS$BadWindow();

  brec.x1 = w->x;
  brec.y1 = w->y;
  brec.x2 = w->x + w->width;
  brec.y2 = w->y + w->height;

  /* Clip to window bounds */

  Region$Translate(svr_st->region, region, w->x, w->y);

  Region$InitRec(svr_st->region, &bounds, &brec);
  Region$Intersect(svr_st->region, region, region, &bounds);

  MU_LOCK(&svr_st->mu);

  switch (op) {

  case WS_ClipOp_Set:
      
      Region$Copy(svr_st->region, &w->clip_region, region);
      break;

  case WS_ClipOp_Add:
      Region$Union(svr_st->region, &w->clip_region, &w->clip_region, region);
      break;

  case WS_ClipOp_Subtract:
      Region$Subtract(svr_st->region, &w->clip_region, &w->clip_region, region);
      break;

  }
  
  if(w->mapped) {
      HandleRegionExposures(svr_st, (op == WS_ClipOp_Set)?&bounds:region, w);

  }
  
  Refocus(svr_st);

  MU_RELEASE(&svr_st->mu);
  
  Region$DoneRec(svr_st->region, &bounds);

}

void WS_SetTitle_m (WS_cl *self,
		    WS_WindowID w,
		    string_t title) 
{

    /* WSSvr isn't bothered about the window title */

}

static uint32_t WS_Info_m (
        WS_cl   *self
        /* RETURNS */,
        uint32_t        *height,
	uint32_t        *xgrid,
	uint32_t        *ygrid,
	uint32_t        *psize,
	uint32_t        *pbits,
	uint64_t        *protos)
{
    WS_st         *st = self->st;
    WSSvr_st  *svr_st = st->svr_st;
    FB_st       *fbst = &svr_st->fb_st;

    *height = fbst->height;
    *xgrid = fbst->xgrid;
    *ygrid = fbst->ygrid;
    *psize = fbst->depth;
    *pbits = fbst->pbits;
    *protos = fbst->protos;

    return fbst->width;
}

void WS_Close_m (WS_cl *self)
{
    WS_st *st = self->st;
    IDCServerBinding_clp binding;
    
    TRC(printf("WS$Close(%p)\n", self));
    
    while(!LINK_EMPTY(&st->windows)) {
	
	WSSvr_Window *w = CLIENT2WINDOW(st->windows.next);
	
	WS_DestroyWindow_m(self, w->wid);
    }

    /* If we have an open event stream, close it */
    if(st->evio) {
	IO_clp io = st->evio;
	st->evio = NULL;
	IO$Dispose(io);
    }

    /* If the binding hasn't already been cleared up, close it now */
    if(st->binding) {
	TRC(printf("WSSvr: Closing binding\n"));
	binding = st->binding;
	st->binding = NULL;
	IDCServerBinding$Destroy(binding);
    } else {
	TRC(printf("WSSvr: Binding already closed\n"));
    }

    LINK_REMOVE(&st->clients);

    FREE(st);
}

static void WSF_DeliverEvent_m (WSF_cl  *self, const WS_Event  *ev) 
{
    WS_st *st = self->st;
    IO_clp io;
    IO_Rec rec;
    word_t value;
    uint32_t nrecs;
    
    TRC(printf("WSF_DeliverEvent_m(%p, %p)\n", self, ev));
    
    io = st->evio;
    if (!io) { 
	return;
    }

    if (IO$GetPkt(io, 1, &rec, 0, &nrecs, &value)) {

	*(WS_Event *)rec.base = *ev;
	rec.len = sizeof(*ev);
	
	IO$PutPkt(io, 1, &rec, 0, 0);
    }
}

static void WSF_LockUpdates_m (WSF_clp self) {
    WS_st            *st = self->st;
    WSSvr_st     *svr_st = st->svr_st;

    TRC(printf("WSF_LockUpdates_m(%p)\n", self));
    svr_st->update_lock_count++;
}

static void WSF_UnlockUpdates_m (WSF_clp self) {
    WS_st         *st = self->st;
    WSSvr_st  *svr_st = st->svr_st;
    
    TRC(printf("WSF_UnlockUpdates_m(%p)\n", self));
    
    MU_LOCK(&svr_st->mu);
    if(svr_st->update_lock_count) {
	svr_st->update_lock_count--;
	
	if ((!svr_st->update_lock_count) && (svr_st->first_exposed)) {
	    
	    ProcessUpdates(svr_st);
	    
	}
    } else {
	fprintf(stderr, "Attempted to unlock WSSvr when already unlocked!\n");
    }
    MU_RELEASE(&svr_st->mu);
}

/* Null Window Manager methods */

static WS_clp NullWM_NewClient_m (
    WM_cl          *self,
    WSF_clp         w       /* IN */,
    Domain_ID       dom     /* IN */)
{
    return (WS_clp) w;
}

static void NullWM_HandleEvent_m (
    WM_cl   *self,
    WS_Event  *ev             /* IN */,
    WSF_clp defclient         /* IN */)
{

    if(defclient) WSF$DeliverEvent(defclient, ev);

}

/* ------------------------------------------------------------ */

static bool_t WS_IDCRequest_m (
    IDCCallback_clp self,
    IDCOffer_clp offer,
    Domain_ID id,
    ProtectionDomain_ID pdom,
    const Binder_Cookies *clt_cks,
    Binder_Cookies *svr_cks) {

    TRC(printf("WSSvr: WS Bind from client %02x\n", (uint32_t) id));

    return True;

}

static bool_t WS_IDCBound_m (
    IDCCallback_clp self,
    IDCOffer_clp offer,
    IDCServerBinding_clp binding,
    Domain_ID client,
    ProtectionDomain_ID pdom,
    const Binder_Cookies *clt_cks,
    Type_Any *any) {

    WS_st          *ws_st;
    WS_clp    NOCLOBBER ws = NULL;
    WSSvr_st       *svr_st = self->st;

    ws_st  = (WS_st *)Heap$Malloc(Pvs(heap), sizeof(WS_st));
    
    if (!ws_st) {
	return False;
    }
    
    memset(ws_st, 0, sizeof(*ws_st));

    CL_INIT(ws_st->wsf_cl, &wsf_ms, ws_st);
    CL_INIT(ws_st->iocb_cl, &iocb_ms, ws_st);
    LINK_INITIALISE(&ws_st->windows);
            
    if (svr_st->wm) {
	/* Redirect client through window manager */
	TRY {
	    ws = WM$NewClient(svr_st->wm, &ws_st->wsf_cl, client);
	} CATCH_WM$Failure() {
	    TRC(printf("WSSvr: Caught WM$Failure when adding client\n"));
	    FREE(ws_st);
	    return False;
	} ENDTRY;
    } else {
	/* No window manager - pass straight back */
	ws = (WS_clp) &ws_st->wsf_cl;
    }
    
	
    ws_st->svr_st  = svr_st;
    ws_st->evio    = NULL;
    ws_st->ws_clp  = ws;
    ws_st->id      = client;
    ws_st->binding = binding;

    LINK_ADD_TO_HEAD(&svr_st->clients, &ws_st->clients);
    any->val = (pointerval_t) ws;

    return True;
}


static void WS_IDCClosed_m(
    IDCCallback_clp self,
    IDCOffer_clp offer,
    IDCServerBinding_clp binding,
    const Type_Any *any)
{
    WSSvr_st *svr_st = self->st;
    WS_clp ws = NARROW(any, WS_clp);
    WS_st *ws_st;

    chain *c;

    c = svr_st->clients.next;

    while(c != &svr_st->clients) {
	
	ws_st = (WS_st *)c;
	if(ws_st->ws_clp == ws) {

	    TRC(printf("WSSvr: client %02x (state %p) closed connection\n", 
		   (uint32_t)ws_st->id, ws_st));

	    /* IDCCallback$Closed() calls WS$Close() and vice versa -
	       reset the binding field to ensure we don't recurse. */

	    if(ws_st->binding) {
		TRC(printf("WSSvr: Closing WS connection\n"));
		ws_st->binding = NULL;
		WS$Close(ws);
	    } else {
		TRC(printf("WSSvr: WS Connection already closed\n"));
	    }

	    return;
	}
	c = c->next;
    }

    fprintf(stderr, "WSSvr IDCCallback$Closed: Unknown client\n");
}

/* End */
