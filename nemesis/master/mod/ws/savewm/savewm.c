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
** ID : $Id: savewm.c 1.4 Tue, 08 Jun 1999 14:02:38 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <regionstr.h>
#include <exceptions.h>
#include <links.h>
#include <time.h>
#include <ecs.h>


#include <IDCMacros.h>

#include <WS.ih>
#include <WM.ih>
#include <WMMod.ih>
#include <FB.ih>
#include <FBBlit.ih>
#include <Mouse.ih>
#include <LongCardTbl.ih>
#include <LongCardTblMod.ih>
#include <CRend.ih>
#include <CRendPixmap.ih>
#include <IO.ih>
#include <IOMacros.h>
#include <DomainMgr.ih>
#include <stdlib.h>
#include <keysym.h>

#ifdef DEBUG
#define TRC(x) x
#define TRC_SaveWM(_x) _x
#else
#define TRC(x)
#define TRC_SaveWM(_x) 
#endif

#define SAVER_TIMEOUT SECONDS(900)
#define SAVER_CHECK_RATE SECONDS(60)

/*
 * Module stuff
 */
static	WS_EventStream_fn 		SaveWM_EventStream_m;
static	WS_CreateWindow_fn 		SaveWM_CreateWindow_m;
static	WS_DestroyWindow_fn 		SaveWM_DestroyWindow_m;
static	WS_UpdateStream_fn 		SaveWM_UpdateStream_m;
static	WS_MapWindow_fn 		SaveWM_MapWindow_m;
static	WS_UnMapWindow_fn 		SaveWM_UnMapWindow_m;
static	WS_MoveWindow_fn 		SaveWM_MoveWindow_m;
static	WS_ResizeWindow_fn 		SaveWM_ResizeWindow_m;
static  WS_RaiseWindow_fn               SaveWM_RaiseWindow_m;
static  WS_LowerWindow_fn               SaveWM_LowerWindow_m;
static	WS_AdjustClip_fn 		SaveWM_AdjustClip_m;
static  WS_SetTitle_fn                  SaveWM_SetTitle_m;
static  WS_Info_fn                      SaveWM_Info_m;
static	WS_Close_fn 		        SaveWM_Close_m;
static  WS_WinInfo_fn                   SaveWM_WinInfo_m;

static	WS_op	ws_ms = {
    SaveWM_EventStream_m,
    SaveWM_CreateWindow_m,
    SaveWM_DestroyWindow_m,
    SaveWM_UpdateStream_m,
    SaveWM_MapWindow_m,
    SaveWM_UnMapWindow_m,
    SaveWM_MoveWindow_m,
    SaveWM_ResizeWindow_m,
    SaveWM_RaiseWindow_m,
    SaveWM_LowerWindow_m,
    SaveWM_AdjustClip_m,
    SaveWM_SetTitle_m,
    SaveWM_Info_m,
    SaveWM_Close_m,
    SaveWM_WinInfo_m,
};

static  WM_NewClient_fn                 SaveWM_NewClient_m;
static  WM_HandleEvent_fn               SaveWM_HandleEvent_m;

static  WM_op   wm_ms = {
    SaveWM_NewClient_m,
    SaveWM_HandleEvent_m
};

static  WMMod_New_fn            WMMod_New_m;

static  WMMod_op        wmmod_ms = {
    WMMod_New_m
};

const static  WMMod_cl        wmmod_cl = { &wmmod_ms, NULL };

CL_EXPORT (WMMod, WMMod, wmmod_cl);

typedef struct _chain chain;

struct _chain {
    chain *next;
    chain *prev;
};

#define CLIENT(c) \
((WS_st *)(((addr_t)(c)) - OFFSETOF(WS_st, cchain)))

#define WINDOW(c) \
    ((WM_Window *)(((addr_t)(c)) - OFFSETOF(WM_Window, wchain)))

#define LIGHT_GREY 0xFFFF
#define MID_GREY   0x6318
#define DARK_GREY  0x4210
#define MID_BLUE   0x631F
#define DARK_BLUE  0x000F

typedef struct _WS_st WS_st;
typedef struct _WM_st WM_st;
typedef struct _WM_Window WM_Window;

struct _WS_st {
    WM_st *wmst;
    WSF_clp wsf;
    WS_cl ws_cl;
    chain  wchain;
    chain  cchain;
    Domain_ID id;
    char *name;
};

struct _WM_st { 
    WSF_clp wsf;
    WM_cl   wm_cl;
    chain cchain;
    LongCardTbl_clp tbl;
    uint32_t xgrid, ygrid;
    uint32_t xppwmask, yppwmask;
    uint32_t width, height;
    FB_Protocol proto;

    uint32_t depth; /* == log2 of ((screen depth in bits)/8) */
    uint32_t xedge, topedge, bottomedge;
    void    *leftblit, *rightblit;
    void    *focusleftblit, *focusrightblit;

    WM_Window *moving;
    WM_Window *clipping;
    bool_t clip_add;

    bool_t menuVisible;
    uint32_t move_origin_x, move_origin_y;

    WS_MouseData mouse, oldmouse;
    CRendPixmap_clp pixmap;
    WM_Window *focus;
    Region_clp region;
    Time_ns  lastevent;
    int   screen_saved; /* 0 means unsaved, 1 means saving, 2 means saved, 3 means waking up */
    WS_WindowID screen_saver_window;
    void *fblite;
    CRend_clp screen_saver_crend;

    bool_t clipenabled;
};

struct _WM_Window {
    WS_st *owner;
    WS_WindowID wid;
    WS_WindowID dec_wid;
    FBBlit_clp  dec_blit;
    IO_clp      dec_io;
    FB_StreamID dec_strid;
    chain wchain;
    int32_t x, y, width, height;
    bool_t client_mapped, wm_mapped, wm_mapped_before_save;

    void *topblit, *bottomblit, *rightblit, *focusrightblit;
    int decwidth, decstride;
    WS_MouseData mouse;
    CRend_clp crend;
    char *title;
    bool_t focus;

    /* XXX Control over decoration is a hack used for the java Swing
     * demo.  It expects pop up menus, tool-tips etc to appear without
     * decoration.  It signals this by setting an empty (ie "", not NULL)
     * title with WS$SetTitle.  
     *
     * Clients should probably be able to specify other details too --
     * eg iconifiable, resiable etc.
     */

    bool_t decorated;

};

/* Useful functions */

/* RAISES WS.BadWindow */
static WM_Window *FindWindow(WS_st *st, WS_WindowID wid); 
static WS_st *FindClient(WM_st *st, WSF_clp wsf);
static bool_t HandleGlobalEvent(WM_st *st, WS_Event *ev, WSF_clp wsf);
static void HandleLocalEvent(WM_st *st, WS_Event *ev); 
static void HandleClientEvent(WM_st *st, WS_Event *ev, WSF_clp wsf);
static void SetWindowDecorations(WM_st *st, WM_Window *w, bool_t resize);
static void DrawWindowDecorations(WM_st *st, WM_Window *w,
				  word_t x1, word_t y1,
				  word_t x2, word_t y2);


static void DestroyWindow(WS_st *st, WM_Window *w);
static void UnMapWindow(WS_st *st, WM_Window *w);

struct killer_st { 
    VP_clp vp;
    DomainMgr_clp dommgr;
    Domain_ID id;
};


static void start_saver(WM_st *st) {
    WS$MapWindow((WS_clp)st->wsf, st->screen_saver_window);
    WS$RaiseWindow((WS_clp)st->wsf, st->screen_saver_window);

    fprintf(stderr,"Started screen saver\n");
}		


static void stop_saver(WM_st *st) {
    st->screen_saved = 3;
    while (st->screen_saved == 3) {
	Threads$Yield(Pvs(thds));
    }
    WS$UnMapWindow((WS_clp)st->wsf, st->screen_saver_window);
    fprintf(stderr,"Stopped screen saver\n");
}		




   
void killer_thread(struct killer_st *st) {
    /* XXX be a litte  paranoid  */
    PAUSE(SECONDS(1)/10);
    DomainMgr$Destroy(st->dommgr, st->id);
    free(st);
}


static void kill(Domain_ID id) {
    char buf[80];
    VP_clp vp;
    DomainMgr_clp dommgr;
    char *name;
    struct killer_st *killer;


    sprintf(buf,"proc>domains>%qx>vp", id);
    name =strdup(buf);
    vp = NAME_FIND(name,VP_clp);
    dommgr = IDC_OPEN("sys>DomainMgrOffer", DomainMgr_clp);

    killer = malloc(sizeof(struct killer_st));
    killer-> vp = vp;
    killer->dommgr = dommgr;
    killer->id = id;

    /* XXX superstition wrt domain death */
    Threads$Fork(Pvs(thds), killer_thread, killer, 0);

}
 
/*---------------------------------------------------- Entry Points ----*/

static IOOffer_clp SaveWM_EventStream_m (
    WS_cl	*self )
{
    WS_st	*st = (WS_st *) self->st;

    return WS$EventStream((WS_clp)st->wsf);

}

static WS_WindowID SaveWM_CreateWindow_m (
    WS_cl	*self,
    int32_t	_x	/* IN */,
    int32_t	_y	/* IN */,
    uint32_t	_width	/* IN */,
    uint32_t	_height	/* IN */ )
{
    int32_t NOCLOBBER x = _x;
    int32_t NOCLOBBER y = _y;
    uint32_t NOCLOBBER width = _width;
    uint32_t NOCLOBBER height = _height;

    WS_st *st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;
    WM_Window *w;
    IOOffer_clp offer;

    if (wmst->screen_saved) stop_saver(wmst);

    w = Heap$Malloc(Pvs(heap), sizeof(*w));

    if(!w) RAISE_WS$Failure();

    memset(w, 0, sizeof(*w));
    TRC_SaveWM(printf("SaveWM: allocated & zero'ed WM_Window 'w' at %p\n", w));

    w->wid = 0;
    w->dec_wid = 0;
    w->dec_blit = NULL;
    w->dec_io  = NULL;
    w->dec_strid = 0;

    x &= wmst->xppwmask;
    y &= wmst->yppwmask;

    if(x<wmst->xedge) {
	x += wmst->xedge;
    }

    if(y<wmst->topedge) {
	y += wmst->topedge;
    }
  
    width &= wmst->xppwmask;
    height &= wmst->yppwmask;

    TRY {
	w->dec_wid = WS$CreateWindow((WS_clp)wmst->wsf, 
				     x - wmst->xedge, 
				     y - wmst->topedge, 
				     width + 2*wmst->xedge, 
				     height + wmst->topedge + 
				     wmst->bottomedge);

	w->wid = WS$CreateWindow((WS_clp)st->wsf, x, y, width, height);

	w->dec_strid = WS$UpdateStream((WS_clp)wmst->wsf, w->dec_wid,
				       wmst->proto,
				       0, True, &offer, &w->dec_blit);

	w->dec_io = IO_BIND(offer);
	
    } CATCH_WS$Failure() {

	if(w->wid) WS$DestroyWindow((WS_clp)st->wsf, w->wid);

	if(w->dec_wid) WS$DestroyWindow((WS_clp)wmst->wsf, w->dec_wid);
	FREE(w);
	RERAISE;
    } ENDTRY;

    TRC_SaveWM(printf("SaveWM: now got wid & dec_wid [%p and %p resp]\n", w->wid, 
		   w->dec_wid));
    LongCardTbl$Put(wmst->tbl, w->dec_wid, w);

    w->owner = st;
    w->x = x;
    w->y = y;
    w->width = width;
    w->height = height;
    w->client_mapped = False;
    w->wm_mapped = True;
    w->decorated = True;

    TRC_SaveWM(fprintf(stderr, "SaveWM: just before SetWinDec: w->wid is %p\n", 
		    w->wid));

    SetWindowDecorations(wmst, w, True);
    LINK_ADD_TO_HEAD(&st->wchain, &w->wchain);

    return w->wid;

}

static void DestroyWindow(WS_st *st, WM_Window *w) {

    WM_Window *tmp;

    UnMapWindow(st, w);

    WS$DestroyWindow((WS_clp)st->wmst->wsf, w->dec_wid);
    LongCardTbl$Delete(st->wmst->tbl, w->dec_wid, (addr_t)&tmp);
    WS$DestroyWindow((WS_clp)st->wsf, w->wid);

    LINK_REMOVE(&w->wchain);
    FREE(w);
}
    

static void SaveWM_DestroyWindow_m (
    WS_cl	*self,
    WS_WindowID	wid	/* IN */ )
{
    WS_st	*st = (WS_st *) self->st;
    WM_Window *w;

    w = FindWindow(st, wid);

    WSF$LockUpdates(st->wsf);
    
    TRY {
	DestroyWindow(st, w);
    } FINALLY {
	WSF$UnlockUpdates(st->wsf);
    } ENDTRY;

}

static FB_StreamID SaveWM_UpdateStream_m (
    WS_cl	*self,
    WS_WindowID	w	/* IN */,
    FB_Protocol	p	/* IN */,
    FB_QoS	q	/* IN */,
    bool_t	clip	/* IN */
    /* RETURNS */,
    IOOffer_clp	*offer,
    FBBlit_clp	*blit )
{
    WS_st	*st = (WS_st *) self->st;

    return WS$UpdateStream((WS_clp)st->wsf, w, p, q, clip, offer, blit);

}

static void SaveWM_MapWindow_m (
    WS_cl	*self,
    WS_WindowID	wid	/* IN */ )
{
    WS_st	*st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;

    WM_Window *w = FindWindow(st, wid);

    if(!w->client_mapped) {

	w->client_mapped = True;

	if(w->wm_mapped) {
	  
	    WSF$LockUpdates(wmst->wsf);
	    WS$MapWindow((WS_clp)st->wsf, wid);
	    if (w->decorated) {
		WS$MapWindow((WS_clp)wmst->wsf, w->dec_wid);
	    }
	    WSF$UnlockUpdates(wmst->wsf);
	}
    }

}

static void UnMapWindow(WS_st *st, WM_Window *w) {

    WM_st *wmst = st->wmst;

    if(w->client_mapped) {
	
	w->client_mapped = False;
	
	if(w->wm_mapped) {
	    
	    if(w == wmst->focus) {
		wmst->focus = NULL;
	    }

	    if(w == wmst->moving) {
		wmst->moving = NULL;
	    }

	    if(w == wmst->clipping) {
		wmst->clipping = NULL;
	    }
	    
	    WSF$LockUpdates(st->wmst->wsf);
	    WS$UnMapWindow((WS_clp)st->wsf, w->wid);
	    if (w -> decorated)
	    {
		WS$UnMapWindow((WS_clp)st->wmst->wsf, w->dec_wid);
	    }
	    WSF$UnlockUpdates(st->wmst->wsf);
	    
	    if(w->topblit) {
		FREE(w->topblit);
		w->topblit = NULL;
	    }

	    if(w->crend) {
		CRend$Close(w->crend);
		w->crend = NULL;
	    }
	}
	
    }
    
}

static void SaveWM_UnMapWindow_m (
    WS_cl	*self,
    WS_WindowID	wid	/* IN */ )
{
    WS_st	*st = (WS_st *) self->st;
    WM_Window *w = FindWindow(st, wid);

    WSF$LockUpdates(st->wsf);

    TRY {
	UnMapWindow(st, w);
    } FINALLY {
	WSF$UnlockUpdates(st->wsf);
    } ENDTRY;
}

static void SaveWM_MoveWindow_m (
    WS_cl	*self,
    WS_WindowID	wid	/* IN */,
    int32_t	x	/* IN */,
    int32_t	y	/* IN */ )
{
    WS_st	*st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;

    WM_Window *w = FindWindow(st, wid);

    w->x = x;
    w->y = y;

    WSF$LockUpdates(wmst->wsf);
    WS$MoveWindow((WS_clp)st->wsf, wid, x, y);
    WS$MoveWindow((WS_clp)wmst->wsf, w->dec_wid, 
		  x - wmst->xedge, 
		  y - wmst->topedge);
    WSF$UnlockUpdates(wmst->wsf);
}

static void SaveWM_ResizeWindow_m (
    WS_cl	*self,
    WS_WindowID	wid	/* IN */,
    int32_t	width	/* IN */,
    int32_t	height	/* IN */ )
{
    WS_st	*st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;

    WM_Window *w = FindWindow(st, wid);
  
    w->width = width;
    w->height = height;

    WSF$LockUpdates(wmst->wsf);
    WS$ResizeWindow((WS_clp)st->wsf, wid, width, height);
    WS$ResizeWindow((WS_clp)wmst->wsf, w->dec_wid, 
		    width + 2*wmst->xedge, 
		    height + wmst->topedge + wmst->bottomedge);

    SetWindowDecorations(wmst, w, True);  

    WSF$UnlockUpdates(wmst->wsf);

}

static void SaveWM_RaiseWindow_m (
    WS_cl *self,
    WS_WindowID wid)
{
    WS_st	*st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;
    
    WM_Window *w = FindWindow(st, wid);
    
    WSF$LockUpdates(wmst->wsf);
    WS$RaiseWindow((WS_clp)wmst->wsf, w->dec_wid);
    WS$RaiseWindow((WS_clp)st->wsf, wid);
    WSF$UnlockUpdates(wmst->wsf);
}

static void SaveWM_LowerWindow_m (
    WS_cl *self,
    WS_WindowID wid)
{
    WS_st	*st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;
    
    WM_Window *w = FindWindow(st, wid);
    
    WSF$LockUpdates(wmst->wsf);
    WS$LowerWindow((WS_clp)wmst->wsf, w->dec_wid);
    WS$LowerWindow((WS_clp)st->wsf, wid);
    WSF$UnlockUpdates(wmst->wsf);
}



static void SaveWM_AdjustClip_m (
    WS_cl	*self,
    WS_WindowID	w	/* IN */,
    Region_T 	region	/* IN */,
    WS_ClipOp op	/* IN */ )
{
    WS_st	*st = (WS_st *) self->st;
    
    WS$AdjustClip((WS_clp)st->wsf, w, region, op);
    
}

static void SaveWM_SetTitle_m (
    WS_cl       *self,
    WS_WindowID wid,
    string_t    title)
{
    WS_st       *st = (WS_st *) self->st;
    WM_st *wmst = st->wmst;
    WM_Window *w = FindWindow(st, wid);

    if(w->title) FREE(w->title);

    w->title = strdup(title);

    /*
     * XXX Client signals that a window should not be decorated
     * XXX by setting its title to "".
     */

    if (strlen(title) == 0)
    {
	if (w -> decorated)
	{
	    WS$UnMapWindow((WS_clp)st->wmst->wsf, w->dec_wid);
	}

	w->decorated = False;
    }
    else
    {
	if ((!(w -> decorated)) &&
	    (w -> client_mapped) && 
	    (w -> wm_mapped))
	{
	    WS$MapWindow((WS_clp)st->wmst->wsf, w->dec_wid);
	}
	w->decorated = True;
    }

    SetWindowDecorations(wmst, w, False);

    DrawWindowDecorations(wmst, w, 0, 0, w->decwidth, wmst->topedge);
}
    
static int32_t SaveWM_WinInfo_m  (
    WS_cl       *self,
    WS_WindowID  wid,
    /* RETURNS */
    int32_t     *y,
    uint32_t    *width,
    uint32_t    *height)
{
    int x;

    WS_st     *st = (WS_st *) self->st;
    WM_Window *w = FindWindow(st, wid);

    x = w -> x;
    (*y) = w -> y;
    (*width) = w -> width;
    (*height) = w -> height;

    return x;
}

static uint32_t SaveWM_Info_m (
    WS_cl   *self
    /* RETURNS */,
    uint32_t        *height,
    uint32_t        *xgrid,
    uint32_t        *ygrid,
    uint32_t        *psize,
    uint32_t        *pbits,
    uint64_t        *protos)
{
    WS_st *st = (WS_st *)self->st;

    return WS$Info((WS_clp)st->wsf, height, xgrid, ygrid, psize, pbits, protos);
}

static void SaveWM_Close_m (
    WS_cl	*self )
{
    WS_st    *st = (WS_st *) self->st;
    
    /* WS$Close will deal with closing binding and any open windows */
 
    TRC(printf("SaveWM: Client %d ['%s'] disconnected\n", (uint32_t) st->id,
	       st->name));
   
    WSF$LockUpdates(st->wsf);

    TRY {

	while(!LINK_EMPTY(&st->wchain)) {
	    
	    WM_Window *w = WINDOW(st->wchain.next);
	    
	    TRC(printf("SaveWM: Destroying border window %d\n",
		       (uint32_t)w->dec_wid));
	    
	    DestroyWindow(st, w);
	    
	}

	LINK_REMOVE(&st->cchain);

    } FINALLY {

	WSF$UnlockUpdates(st->wsf);

    } ENDTRY;
    
    WS$Close((WS_clp)st->wsf);
    
    TRC(printf("SaveWM: Cleared up after client\n"));
    
    FREE(st);
    
}

/* WM operations */

static WS_clp SaveWM_NewClient_m (
    WM_cl       *self,
    WSF_clp      w       /* IN */,
    Domain_ID    dom     /* IN */)
{
    WM_st *st = (WM_st *) self->st;
    WS_st *ws_st;

    char context[256];
    string_t name;

    ws_st = Heap$Malloc(Pvs(heap), sizeof(*ws_st));

    if(!ws_st) RAISE_WM$Failure();

    CL_INIT(ws_st->ws_cl, &ws_ms, ws_st);

    ws_st->wsf = w;
    ws_st->wmst = st;

    sprintf(context, "proc>domains>%qx>name", dom);
  
    name = NAME_FIND(context, string_t);

    ws_st->id = dom;
    ws_st->name = strdup(name);
    
    LINK_INITIALISE(&ws_st->wchain);

    LINK_ADD_TO_HEAD(&st->cchain, &ws_st->cchain);

    TRC(printf("WM_NewClient : Connection from %s [%u]\n", 
	       name, (uint32_t) dom));

    return &ws_st->ws_cl;

}

static void SaveWM_HandleEvent_m (
    WM_cl   *self,
    WS_Event  *ev     /* IN */,
    WSF_clp  defclient)
{
    WM_st *st = (WM_st *) self->st;
    st->lastevent = NOW();
    /* stop the saver only if this is only an expose/obscure event */
    if (st->screen_saved && 
	(ev->d.tag == WS_EventType_Mouse 
	|| ev->d.tag == WS_EventType_KeyPress
	|| ev->d.tag == WS_EventType_KeyRelease)) {
	fprintf(stderr,"Stopping screen saver\n");
	stop_saver(st);
    }
    if (ev->w == st->screen_saver_window) return;

    if(HandleGlobalEvent(st, ev, defclient)) {
      
	if(defclient == st->wsf) {
	    HandleLocalEvent(st, ev);
	} else {
	    HandleClientEvent(st, ev, defclient);
	}
    }
}

static void SaverThread(void *st_v) {
    WM_st *st = (WM_st *)st_v;

    while (1) {
	Time_ns now = NOW();
	if ((now - st->lastevent)>SAVER_TIMEOUT && (!st->screen_saved)) {
	    fprintf(stderr,"now %qx lastevent %qx timeout %qx\n", now, st->lastevent, SAVER_TIMEOUT);
	    fprintf(stderr,"Screen saving\n"); 
	    start_saver(st);
	    st->screen_saved = 1;
	    {
		uint32_t *ptr, *limit;
		int i;
		int countdown;

		countdown = 13 * 32;
		limit = ((uint32_t *) (st->fblite) + st->width * st->height / 2);
		
		ptr = st->fblite;
		while (st->screen_saved == 1) {
		    if (countdown) {
			/* fade out the screen */
			for (i=0; i<1000; i++) {
			    uint32_t value;
			    
			    value= *ptr & 0x7fffffff;
			    
			    if (value & 0x1f) value -= 1;
			    if (value & 0x3e0) value -= 0x20;
			    if (value & 0x7c00) value -= 0x400;
			    if (value & 0x1f0000) value -= 1 * 0x10000;
			    if (value & 0x3e00000) value -= 0x20 * 0x10000;
			    if (value & 0x7c000000) value -= 0x400 * 0x10000;
			    
			    *ptr = value;
			    
			    ptr += 13;
			    if (ptr >= limit) {
				ptr -=  st->width * st->height / 2;
				countdown --;
			    }
			}
		    } else {
			PAUSE(SECONDS(1)/1000);
			/* add a post-fade screen saver here if you want */
		    }
		    
		}
		st->screen_saved = 0;
	    }
	} else {
	    /* screen saver not active; wait for a while */
	    PAUSE(SAVER_CHECK_RATE);
	}
    }
}

/* WMMod operations */

static WM_clp WMMod_New_m (
    WMMod_cl        *self,
    WSF_clp         wsf)
{

    WM_st *st;
    LongCardTblMod_clp tblmod;
    LongCardTbl_clp tbl;
    uint64_t protos;
    uint32_t depth, xedgestride, dummy;
    CRend_clp crend;

    tblmod = NAME_FIND("modules>LongCardTblMod", LongCardTblMod_clp);

    tbl = LongCardTblMod$New(tblmod, Pvs(heap));

    st = Heap$Malloc(Pvs(heap), sizeof(*st));
    if(!st) RAISE_Heap$NoMemory();

    memset(st, 0, sizeof(*st));

    CL_INIT(st->wm_cl, &wm_ms, st);

    st->wsf = wsf;
    st->tbl = tbl;
    LINK_INITIALISE(&st->cchain);

    st->pixmap = NAME_FIND("modules>CRendPixmap", CRendPixmap_clp);
    st->region = NAME_FIND("modules>Region", Region_clp);

    st->width = WS$Info((WS_clp)wsf, 
			&st->height, &st->xgrid, 
			&st->ygrid, &depth, &dummy, &protos);

    switch(depth) {

    case 8:
	st->depth = 0;
	st->proto = FB_Protocol_Bitmap8;
	break;

    case 16:
	st->depth = 1;
	st->proto = FB_Protocol_Bitmap16;
	break;

    default:
	fprintf(stderr, "WMMod_New: Unknown screen depth\n");
	RAISE_WMMod$Failure();
	break;

    }

    st->xppwmask = ~(st->xgrid - 1);
    st->yppwmask = ~(st->ygrid - 1);

    /* Make sure all the decoration sizes are aligned on the display grid */

    for(st->xedge = 4; st->xedge % st->xgrid; st->xedge++);
    for(st->topedge = 18; st->topedge % st->ygrid; st->topedge++);
    for(st->bottomedge = 4; st->bottomedge % st->ygrid; st->bottomedge++);

    xedgestride = (st->xedge + st->xgrid - 1) & st->xppwmask;

    st->leftblit = Heap$Malloc(Pvs(heap), (4 * xedgestride) << st->depth);
    st->rightblit = st->leftblit + (xedgestride << st->depth);
    st->focusleftblit = st->leftblit + 2 *(xedgestride << st->depth);
    st->focusrightblit = st->leftblit + 3 * (xedgestride << st->depth);

    st->focus = NULL;

    crend = CRendPixmap$New(st->pixmap, st->leftblit, 
			    2 * xedgestride, 2, 2 * xedgestride);
    CRend$SetForeground(crend, LIGHT_GREY);
    CRend$FillRectangle(crend, 0, 0, 2, 2);

    CRend$SetForeground(crend, DARK_GREY);
    CRend$FillRectangle(crend, ( 2 * xedgestride) - 2, 0, 2, 2);
    
    CRend$SetForeground(crend, MID_GREY);
    CRend$FillRectangle(crend, 2, 0, (2 * xedgestride) - 4, 1);
    
    CRend$SetForeground(crend, MID_BLUE);
    CRend$FillRectangle(crend, 2, 1, (2 * xedgestride) - 4, 1);

    st->moving = NULL;
    st->clipping = NULL;
    st->lastevent = NOW();
    st->screen_saver_window = WS$CreateWindow((WS_clp)st->wsf, 0,0, st->width, st->height);
    st->fblite = NAME_FIND("dev>fblite", addr_t);
    st->screen_saver_crend = CRendPixmap$New(st->pixmap, st->fblite, st->width, st->height, st->width);
    fprintf(stderr,"Screen saver window ID is %d\n", st->screen_saver_window);
    WS$UnMapWindow((WS_clp)st->wsf, st->screen_saver_window);
    fprintf(stderr,"Unmapped Screen saver window ID is %d\n", st->screen_saver_window);
    Threads$Fork(Pvs(thds), SaverThread, st, 0);
    st->clipenabled = False;

    return &st->wm_cl;
}

/* Useful functions */

static WM_Window *FindWindow(WS_st *st, WS_WindowID wid) {

    chain *c;
    WM_Window *w = BADPTR;
    c = st->wchain.next;
    
    while(c != &st->wchain) {
	
	w = WINDOW(c);
	
	if(w->wid == wid) break;
	
	c = c->next;
    }
    
    if(c == &st->wchain) RAISE_WS$BadWindow();

    return w;
}

static WS_st *FindClient(WM_st *st, WSF_clp wsf) {

    chain *c;
    WS_st *ws = BADPTR;
    
    c = st->cchain.next;

    while(c != &st->cchain) {

	ws = CLIENT(c);

	if(ws->wsf == wsf) break;

	c = c->next;
    }

    if(c == &st->cchain) {
	fprintf(stderr, "Inconsistent client chain in SaveWM!\n");
	PAUSE(SECONDS(10));
    }
    return ws;
}

static void HandleClientEvent(WM_st *st, WS_Event *ev, WSF_clp wsf) 
{

    WM_Window *w;
    WS_st *ws_st;

    if(wsf) {
 
	ws_st = FindClient(st, wsf);
	
	w = FindWindow(ws_st, ev->w);

	switch(ev->d.tag) {
	case WS_EventType_Mouse:
	{
	    
	    const WS_MouseData *m = &ev->d.u.Mouse;
	    
	    Mouse_Buttons pressed, released;
	    
	    pressed  = ~w->mouse.buttons & m->buttons;
	    released = w->mouse.buttons & ~m->buttons;

	    w->mouse = *m;
	    
	    if(!(st->clipping || st->moving || st->menuVisible)) {

		if(pressed & SET_ELEM(Mouse_Button_Left)) {
		    st->clipping = w;
		    st->clip_add = True;
		} else if(pressed & SET_ELEM(Mouse_Button_Right)) {
		    st->clipping = w;
		    st->clip_add = False;
		}
	    }
	    break;
	    
	}

	case WS_EventType_Expose: {
	  /*
	   * Check if the window is moving ie. the user is dragging
	   * it around. If it is then we need to set the reason
	   * for the exposure so that clients can distinguish between
	   * *real* exposures and moves.
	   */
	  if (st->moving) ev->r = WS_EventReason_Move;
	  
	  break;
	}

	
	default:
	    break;
	}
	
	
	WSF$DeliverEvent(wsf, ev);
    } else {

	/* Event for backdrop window */

	switch(ev->d.tag) {
	case WS_EventType_Mouse:
	{
	    
	    int x, y;
	    const WS_MouseData *m = &ev->d.u.Mouse;
	    
	    Mouse_Buttons pressed, released;
	    
	    pressed  = ~st->oldmouse.buttons & m->buttons;
	    released = st->oldmouse.buttons & ~m->buttons;

	    if(!(st->clipping || st->moving || st->menuVisible)) {

		if(pressed & SET_ELEM(Mouse_Button_Left)) {

		    /* Show the menu of invisible windows */
		    chain *cc, *wc;

		    st->menuVisible = True;
		    
		    x = (m->absx - 10) & st->xppwmask;
		    y = (m->absy + 10) & st->yppwmask;

		    WSF$LockUpdates(st->wsf);

		    cc = st->cchain.next; 

		    while(cc != &st->cchain) {

			ws_st = CLIENT(cc);

			wc = ws_st->wchain.next;

			while(wc != &ws_st->wchain) {

			    w = WINDOW(wc);

			    if(w->client_mapped && (!w->wm_mapped)) {

				/* Magically map window title bar into menu */

				WS$MoveWindow((WS_clp)st->wsf, w->dec_wid,
					      x, y);

				WS$ResizeWindow((WS_clp)st->wsf, w->dec_wid, 
						w->width, 
						st->topedge);
					     
				WS$RaiseWindow((WS_clp)st->wsf, w->dec_wid);

				WS$MapWindow((WS_clp)st->wsf, w->dec_wid);

				y += st->topedge;

			    }

			    wc = wc->next;

			}

			cc = cc->next;

		    }
    
		    WSF$UnlockUpdates(st->wsf);

		}

	    }
	    break;

	}

	default:
	    break;

	}
    }
}

static void HandleLocalEvent(WM_st *st, WS_Event *ev) 
{

    WM_Window *w;

    if(!LongCardTbl$Get(st->tbl, ev->w, (addr_t)&w)) {
	fprintf(stderr, "HandleLocalEvent : Invalid window ID\n");
	return;
    }

    switch(ev->d.tag) {

    case WS_EventType_Expose: 
    {
	const WS_Rectangle *r = &ev->d.u.Expose;
	
	DrawWindowDecorations(st, w, r->x1, r->y1, r->x2, r->y2);
	
	break;
    }

    case WS_EventType_Mouse:
    {

	const WS_MouseData *m = &ev->d.u.Mouse;

	Mouse_Buttons pressed, released;

	pressed  = ~w->mouse.buttons & m->buttons;
	released = w->mouse.buttons & ~m->buttons;

	w->mouse = *m;

	if(!(st->clipping || st->moving || st->menuVisible)) {

	    if(pressed & SET_ELEM(Mouse_Button_Left)) {
		
		int decwidth = w->width + 2 * st->xedge;
 		if (m->x > decwidth - 16 && m->x < decwidth - 4 && 
		    m->y >= 4 && m->y < 16) {

		    /* Close button was pressed - kill client */
		    kill(w->owner->id);
		} else {
		    /* Bring window to front */
		    WSF$LockUpdates(st->wsf);
		    WS$RaiseWindow((WS_clp)st->wsf, w->dec_wid);
		    WS$RaiseWindow((WS_clp)w->owner->wsf, w->wid);
		    WSF$UnlockUpdates(st->wsf);
		}
	    }
	    
	    if(pressed & SET_ELEM(Mouse_Button_Right)) {

		/* Make window disappear */
		w->wm_mapped = False;
		WSF$LockUpdates(st->wsf);
		WS$UnMapWindow((WS_clp)w->owner->wsf, w->wid);
		WS$UnMapWindow((WS_clp)st->wsf, w->dec_wid);
		WSF$UnlockUpdates(st->wsf);
	    }
	    
	    if(pressed & SET_ELEM(Mouse_Button_Middle)) {

		/* Start moving window */
		st->moving = w;
		st->move_origin_x = m->absx;
		st->move_origin_y = m->absy;
	    }
	}
	
	
	break;

    }

    default:
	break;
    }

}

static bool_t HandleGlobalEvent(WM_st *st, WS_Event *ev, WSF_clp wsf) {

    switch(ev->d.tag) {

    case WS_EventType_Mouse:
    {

	const WS_MouseData *m = &ev->d.u.Mouse;
	int32_t deltax, deltay;

	Mouse_Buttons pressed, released;

	pressed  = ~st->mouse.buttons & m->buttons;
	released = st->mouse.buttons & ~m->buttons;

	st->oldmouse = st->mouse;
	st->mouse = *m;

	if(st->menuVisible) {

	    if(released & SET_ELEM(Mouse_Button_Left)) {

		/* Hide menu */

		WM_Window *w, *focus = st->focus;
		WS_st *ws_st;
		chain *cc, *wc;

		st->menuVisible = False;
		
		WSF$LockUpdates(st->wsf);
		
		cc = st->cchain.next; 
		
		while(cc != &st->cchain) {
		    
		    ws_st = CLIENT(cc);
		    
		    wc = ws_st->wchain.next;
		    
		    while(wc != &ws_st->wchain) {
			
			w = WINDOW(wc);
			
			if(w->client_mapped && (!w->wm_mapped)) {
			    
			    /* Magically map window title bar into menu */
			    
			    WS$MoveWindow((WS_clp) st->wsf, w->dec_wid,
					  w->x - st->xedge, 
					  w->y - st->topedge);
			    
			    WS$ResizeWindow((WS_clp) st->wsf, w->dec_wid, 
					    w->width + 2 * st->xedge, 
					    w->height + st->topedge + 
					    st->bottomedge);
			    
			    WS$UnMapWindow((WS_clp) st->wsf, w->dec_wid);
			    
			}
			
			wc = wc->next;
			
		    }
		    
		    cc = cc->next;
		    
		}
		
		if(focus && focus->client_mapped && (!focus->wm_mapped)) {

		    focus->wm_mapped = True;

		    {

			WM_Window *w = focus;
			Region_Box rect;
			Region_Rec clip;
			int decwidth = w->width + 2 * st->xedge;
			
			rect.x1 = st->xedge;
			rect.y1 = st->topedge;
			rect.x2 = st->xedge + w->width;
			rect.y2 = st->topedge + w->height;
			
			Region$InitRec(st->region, &clip, &rect);
			
			rect.x1 = 0;
			rect.y1 = 0;
			rect.x2 = decwidth;
			rect.y2 = st->topedge + w->height + st->bottomedge;
			
			Region$Inverse(st->region, &clip, &clip, &rect);
			
			WS$AdjustClip((WS_clp)st->wsf, w->dec_wid, 
				      &clip, WS_ClipOp_Set);
			
			Region$DoneRec(st->region, &clip);
			
		    }		    

		    
		    if (focus -> decorated)
		    {
			WS$MapWindow((WS_clp) st->wsf, focus->dec_wid);
		    }
		    WS$RaiseWindow((WS_clp) st->wsf, focus->dec_wid);
		    WS$MapWindow((WS_clp) focus->owner->wsf, focus->wid);
		    WS$RaiseWindow((WS_clp) focus->owner->wsf, focus->wid);

		    
		}
		
		WSF$UnlockUpdates(st->wsf);
		
	    }

	    return False;
	}

	if(st->moving) {

	    if(released & SET_ELEM(Mouse_Button_Middle)) {
		st->moving = NULL;
	    } else if((m->x != st->move_origin_x )||
		      (m->y != st->move_origin_y)) {
		deltax = (m->absx - (int32_t) st->move_origin_x);
		deltax &= /* st->xppwmask */ ~7;
		
		deltay = (m->absy - (int32_t) st->move_origin_y);
		deltay &= /* st->yppwmask */ ~7;
		
		if(deltax || deltay) {
		    
		    WM_Window *mov = st->moving;
		    
		    mov->x += deltax;
		    mov->y += deltay;
		    
		    WSF$LockUpdates(st->wsf);
		    WS$MoveWindow((WS_clp)mov->owner->wsf, 
				  mov->wid, mov->x, mov->y);
		    
		    WS$MoveWindow((WS_clp)st->wsf, mov->dec_wid, 
				  mov->x - st->xedge, mov->y - st->topedge);
		    WSF$UnlockUpdates(st->wsf);
		    
		    st->move_origin_x += deltax;
		    st->move_origin_y += deltay;
		    
		}

		return False;
	    }
	    
	}

	if(st->clipping) {

	    Region_Box  brec;
	    Region_Rec  reg;

	    if(st->clip_add && (released & SET_ELEM(Mouse_Button_Left))) {
		st->clipping = NULL;
	    } else if((!st->clip_add) && 
		      (released & SET_ELEM(Mouse_Button_Right))) {
		st->clipping = NULL;
	    } else {
		
		brec.x1 = m->absx - 16 - st->clipping->x;
		brec.x2 = m->absx + 16 - st->clipping->x;
		brec.y1 = m->absy - 12 - st->clipping->y;
		brec.y2 = m->absy + 12 - st->clipping->y;

		/* 
		 * XXX MJH: must now press LAlt with mouse button to
		 * clip. Changed this because the clipping stuff
		 * messes up button drags in the X stuff 
		 */
		if (st->clipenabled) {
		  
		  Region$InitRec(st->region, &reg, &brec);
		  
		  WS$AdjustClip((WS_clp) st->clipping->owner->wsf, 
				st->clipping->wid, &reg, 
				st->clip_add?WS_ClipOp_Add:WS_ClipOp_Subtract);
		
		  Region$DoneRec(st->region, &reg);

		}
	    }
	}
	
	
	    
	break;
    }

    case WS_EventType_EnterNotify:
    {
	WM_Window *new, *old;

	
	old = st->focus;

	if(wsf == st->wsf) {
	    
	    /* This is a decoration window */
	    if(!LongCardTbl$Get(st->tbl, ev->w, (addr_t)&new)) {
		fprintf(stderr, "HandleGlobalEvent : Invalid window ID\n");
		PAUSE(SECONDS(10));
	    }

	} else if(wsf) {
	    
	    /* This is a real window */
	    
	    WS_st *ws_st;
	    
	    ws_st = FindClient(st, wsf);
	    
	    new = FindWindow(ws_st, ev->w);

	} else {
	    
	    /* This isn't anything to do with us */
	    
	    new = NULL;
	}

	if(new != old) {

	    if(old) {
		old->focus = False;
		SetWindowDecorations(st, old, False);
		DrawWindowDecorations(
		    st, old, 0, 0, old->decwidth, 
		    st->topedge + old->height + st->bottomedge);
	    }

	    st->focus = new;

	    if(new) {
		new->focus = True;
		SetWindowDecorations(st, new, False);
		DrawWindowDecorations(
		    st, new, 0, 0, new->decwidth, 
		    st->topedge + new->height + st->bottomedge);
	    }
	}
	break;
    }

    /* XXX MJH */
    case WS_EventType_KeyPress:
      
      if (ev->d.u.KeyPress == XK_Alt_L)
	st->clipenabled = True;
      
      break;

    case WS_EventType_KeyRelease:
      
      if (ev->d.u.KeyPress == XK_Alt_L)
	st->clipenabled = False;
      
      break;

	    
    default:
      break;
    }

    return True;
}

static void SetWindowDecorations(WM_st *st, WM_Window *w, bool_t resize) {
    int decwidth = w->width + 2 * st->xedge;
    int decheight = st->topedge + st->bottomedge;
    int decstride = (decwidth + st->xgrid - 1) & st->xppwmask;
    int blitsize, rightalignpad, rightblitsize;
    char title[96];
    Region_Box rect;
    Region_Rec clip;

    w->decwidth = decwidth;
    w->decstride = decstride;

    if(w->topblit && resize) { 
	FREE(w->topblit);
	w->topblit = NULL;
	w->bottomblit = NULL;
	w->rightblit = NULL;
	w->focusrightblit = NULL;
    }

    if(w->crend && resize) { 
	CRend$Close(w->crend);
	w->crend = NULL;
    }

    if(!w->topblit) {

	/* Blit memory for each window is laid out as:

	   top border graphics
	   bottom border graphics
	   normal right hand border graphics 
	   focus right hand border graphics

	   The top and bottom graphics are aligned on grid boundaries.
	   The right hand graphics are aligned with the positions in
	   which they will be drawn. */

	/* How much does the right edge blit data need to be padded to
           be properly aligned ? */

	rightalignpad = (w->width + st->xedge) & ~st->xppwmask;
	
	rightblitsize = 
	    ((rightalignpad + st->xedge) + st->xgrid - 1) & st->xppwmask;
	
	blitsize = ((decheight * decstride) + 2 * rightblitsize) << st->depth;

	w->topblit = Heap$Malloc(Pvs(heap), blitsize);
    
	w->bottomblit = w->topblit + ((st->topedge * decstride) << st->depth);

	w->rightblit = 
	    w->bottomblit + ((st->bottomedge * decstride) << st->depth);

	w->focusrightblit = w->rightblit + (rightblitsize << st->depth);

	/* Right hand blit data is already drawn - we just need to copy it
	   to the right alignment */
	
	memcpy(w->rightblit + (rightalignpad << st->depth), 
	       st->rightblit, 
	       st->xedge << st->depth);
	
	memcpy(w->focusrightblit + (rightalignpad << st->depth),
	       st->focusrightblit,
	       st->xedge << st->depth);

    }

    if(!w->crend) {
	w->crend = CRendPixmap$New(st->pixmap, w->topblit,
				   decwidth, decheight,
				   decstride);
    }

    /* Window decorations assume light is coming from top left */

    /* Draw (most of) top/left border */
    CRend$SetForeground(w->crend, LIGHT_GREY);
    CRend$FillRectangle(w->crend, 0, 0, decwidth - 2, 2);
    CRend$FillRectangle(w->crend, 0, 2, 2, decheight - 2);

    /* Draw bottom/right border */
    CRend$SetForeground(w->crend, DARK_GREY);
    CRend$FillRectangle(w->crend, decwidth - 2, 0, 2, decheight);
    CRend$FillRectangle(w->crend, 2, decheight - 2, decwidth - 2, 2);
    CRend$DrawPoint(w->crend, 1, decheight - 1);

    /* Draw last pixel in top border */
    CRend$SetForeground(w->crend, LIGHT_GREY);
    CRend$DrawPoint(w->crend, decwidth - 2, 0);
    
    /* Fill in flat portion (lower central portion of title bar and
       upper central portion of bottom decoration) */

    if(w->focus) {
	CRend$SetForeground(w->crend, MID_BLUE);
    } else {
	CRend$SetForeground(w->crend, MID_GREY);
    }
    CRend$FillRectangle(w->crend, 2, 2, decwidth - 4, decheight - 4);

    {
	/* Draw the X button */
	int boxsize;
	int i;
	boxsize = 12;

	CRend$SetForeground(w->crend, w->focus ? DARK_BLUE : DARK_GREY);
	CRend$FillRectangle(w->crend, decwidth - 4 - boxsize, 4, boxsize, boxsize);
	CRend$SetForeground(w->crend, LIGHT_GREY);
	CRend$FillRectangle(w->crend, decwidth - 4 - boxsize, 4, boxsize-1, boxsize-1);
	CRend$SetForeground(w->crend, w->focus ? MID_BLUE : MID_GREY);
	CRend$FillRectangle(w->crend, decwidth - 4 - boxsize + 1, 5, boxsize - 2, boxsize - 2);
	CRend$SetForeground(w->crend, 0);
	
	for (i=0; i<2; i++) {
	    CRend$DrawLine(w->crend, decwidth - 4 - boxsize + 2 + i, decwidth - 4 - 3 + i , 4 + 2 , 4 + boxsize - 3 + 1 );
	    CRend$DrawLine(w->crend, decwidth - 4 - boxsize + 2 + i, decwidth - 4 - 3 + i , 4 + boxsize - 3, 4 + 2 -1 );
	}
    }
	

    /* Draw title string */
    if(w->focus) {
	CRend$SetForeground(w->crend, 0);
	CRend$SetBackground(w->crend, MID_BLUE);
    } else {
	CRend$SetBackground(w->crend, MID_GREY);
	CRend$SetForeground(w->crend, 0);
    }

    if(w->title) {
	sprintf(title, "[%d] %.32s - %.32s", (uint32_t)w->owner->id, 
		w->owner->name, w->title);
    } else {
	sprintf(title, "[%d] %.32s", (uint32_t) w->owner->id, w->owner->name);
    }
    
    CRend$DrawString(w->crend, st->xedge+4, 4, title);

    /* Set clip region */

    if(resize) {

	rect.x1 = st->xedge;
	rect.y1 = st->topedge;
	rect.x2 = st->xedge + w->width;
	rect.y2 = st->topedge + w->height;
	
	Region$InitRec(st->region, &clip, &rect);
	
	rect.x1 = 0;
	rect.y1 = 0;
	rect.x2 = decwidth;
	rect.y2 = st->topedge + w->height + st->bottomedge;
	
	Region$Inverse(st->region, &clip, &clip, &rect);

	WS$AdjustClip((WS_clp)st->wsf, w->dec_wid, &clip, WS_ClipOp_Set);

	Region$DoneRec(st->region, &clip);

    }
}

static void DrawWindowDecorations(WM_st *st, WM_Window *w,
				  NOCLOBBER word_t x1, NOCLOBBER word_t y1,
				  NOCLOBBER word_t x2, NOCLOBBER word_t y2) 
{

    FBBlit_Rec rec;

    void DoBlit(void) {
	if(w->dec_blit) {
	    TRY {
		while(FBBlit$Blit(w->dec_blit, &rec));
	    } CATCH_FBBlit$BadBlitParams() {
		fprintf(stderr, "Bad blit param\n");
#if 0
		ntsc_dbgstop();
#endif /* 0 */
	    } ENDTRY;
	} else {
	    IO_Rec iorec;
	    word_t value;
	    uint32_t nrecs; 

	    iorec.base = &rec;
	    iorec.len = sizeof(rec);
	    IO$PutPkt(w->dec_io, 1, &iorec, 0, FOREVER);
	    IO$GetPkt(w->dec_io, 1, &iorec, FOREVER, &nrecs, &value);
	}
    }	    
    
    if(!(w->dec_blit || w->dec_io)) return;
    
    x1 &= st->xppwmask;
    y1 &= st->yppwmask;

    x2 = (x2 + st->xgrid - 1) & st->xppwmask;
    y2 = (y2 + st->ygrid - 1) & st->yppwmask;
    
    if(y1 <= st->topedge) {
	
	rec.x = x1;
	rec.y = y1;
	
	rec.data = w->topblit + 
	    ((rec.x + rec.y * w->decstride) << st->depth);
	rec.stride = w->decstride << st->depth;
	
	rec.sourceheight = rec.destheight = 
	    MIN(st->topedge, y2) - rec.y;
	rec.sourcewidth = rec.destwidth = 
	    MIN(w->decwidth, x2) - rec.x;
	
	DoBlit();
	
	}
	
    if((y1 <= w->height + st->topedge) && (y2 >= st->topedge)) {
	
	if(x1 < st->xedge) {
	    
	    rec.stride = 0;
	    if(w->focus) {
		rec.data = st->focusleftblit;
	    } else {
		rec.data = st->leftblit;
	    }
	    
	    rec.x = 0;
	    rec.y = MAX(y1, st->topedge);
	    
	    rec.sourcewidth = rec.destwidth = st->xedge;
	    rec.sourceheight = rec.destheight = 
		MIN(y2, w->height + st->topedge) - rec.y;
	    
	    DoBlit();

	}
	
	if(x2 >= w->width + st->xedge) {
	    
	    rec.stride = 0;
	    if(w->focus) {
		rec.data = w->focusrightblit;
	    } else {
		rec.data = w->rightblit;
	    }	   
	    
	    
	    rec.x = (w->width + st->xedge) & st->xppwmask;
	    rec.y = MAX(y1, st->topedge);
	    
	    rec.sourcewidth = rec.destwidth = w->decwidth - rec.x;
	    rec.sourceheight = rec.destheight =
		MIN(y2, w->height + st->topedge) - rec.y;
	
	    DoBlit();
	    
	}
    }
	
    if(y2 >= w->height + st->topedge) {
	
	int ystart = w->height + st->topedge;
	
	rec.stride = w->decstride << st->depth;
	
	rec.x = x1;
	rec.y = MAX(y1, ystart);
	
	rec.data = w->bottomblit + 
	    ((rec.x + (rec.y - ystart) * w->decstride) << st->depth);
	
	rec.sourceheight = rec.destheight = 
	    MIN(w->height + st->topedge + st->bottomedge, y2) - rec.y;
	rec.sourcewidth = rec.destwidth = 
	    MIN(w->width + 2 * st->xedge, x2) - rec.x;
	
	DoBlit();
	
    }
    
}

/*
 * End 
 */
