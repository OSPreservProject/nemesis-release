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
**      Nemesis client rendering window system server.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State record
** 
** ENVIRONMENT: 
**
**      Internal to module sys/WS
** 
** ID : $Id: WSSvr_st.h 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
** 
**
*/

#ifndef _WSSVR_H_
#define _WSSVR_H_

#include <IO.ih>
#include <IOOffer.ih>
#include <IDCService.ih>
#include <IDCCallback.ih>
#include <WS.ih>
#include <WSF.ih>
#include <WM.ih>
#include <Mouse.ih>
#include <Kbd.ih>
#include <FB.ih>
#include <FBBlit.ih>
#include <Domain.ih>
#include <mutex.h>
#include <regionstr.h>

#define WSSVR_MAX_WINDOWS    256    /* XXX Temp */
#define NUM_MGR_PKTS  32
#define NUM_EVENT_RECS 32

#ifdef DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif
 
#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

typedef struct _WSSvr_st WSSvr_st;
typedef struct _WS_st WS_st;
typedef struct _WSSvr_Window WSSvr_Window;

typedef struct chain chain;

struct chain {
    struct chain   *next;		/* For chaining windows */
    struct chain   *prev;
};

struct _WS_st {
    chain                clients;    /* link in client chain (must be first) */
    WSSvr_st             *svr_st;
    IO_clp               evio;	     /* Event stream */
    struct chain         windows;    /* Windows owned by this client */
    IDCCallback_cl       iocb_cl;  
    WSF_cl               wsf_cl;     /* Real WS closure */
    WS_clp               ws_clp;     /* closure indirected through WM */
    Domain_ID            id;
    IDCServerBinding_clp binding;
    IDCService_clp       evio_service;
    IOOffer_clp          evio_offer;
};


struct _WSSvr_Window {
    WS_WindowID    wid;
    bool_t         free;
    
    WS_st         *owner;
    
    chain          client;	/* Chain to owner */
#define CLIENT2WINDOW(s)  \
    ((WSSvr_Window *)((addr_t)(s) - (word_t)&((WSSvr_Window *)0)->client))
	
    chain          stack;		/* Stacking order */
#define STACK2WINDOW(s)  \
	((WSSvr_Window *)((addr_t)(s) - (word_t)&((WSSvr_Window *)0)->stack))
	
	
    bool_t         mapped;
    int32_t        x, y;
    uint32_t       width, height;
    bool_t         update_needed;
    
    FB_WindowID    fbid;		/* FB window identifier */
    FB_StreamID    sid;		/* FB update stream id */
    
    Region_Rec     clip_region, visible_region;
    Region_T       fb_expose_region;
    WS_Rectangle   expose_rect;
    bool_t         expose_due;
    
    uint64_t       fg;
    uint64_t       bg;

};

/* Public state of devices */
typedef struct {
    int32_t   x;
    int32_t   y;
    uint32_t  buttons;
} Mouse_st;

typedef struct {
    Kbd_clp   kbd;  
    bool_t    shifted;
} Kbd_st;

typedef struct {
    FB_clp    fb;
    uint32_t  width;
    uint32_t  height;
    uint32_t  depth;
    uint32_t  pbits;
    uint32_t  cursor;
    word_t    xgrid;
    word_t    ygrid;
    word_t    xppwmask;
    word_t    yppwmask;
    uint64_t  protos;
} FB_st;

typedef struct {
    WSSvr_Window *w;
    uint32_t xdown;
    uint32_t ydown;
} WM_st;


struct _WSSvr_st {
    /* Mutex to protect state */

    mutex_t   mu;

    /* Client chain */
    chain     clients;
    
    /* Update stream for root window */
    IO_clp    io_root;
    FBBlit_clp blit_root;
    mutex_t    root_io_mu;
    
    /* Normal Windows */
#define ROOT_WINDOW  0
    WSSvr_Window window[WSSVR_MAX_WINDOWS];
    
    /* Event Delivery */
    WSSvr_Window  *focus;
    chain     stack;		/* Stacking order */
    
    /* Device state */
    Mouse_st  mouse_st;
    addr_t    mouse_private_st;
    
    Kbd_st    kbd_st;
    addr_t    kbd_private_st;
    
    FB_st     fb_st;
    addr_t    fb_private_st;

    /*
    ** If fill_bg == 0, then we have a ppm bitmap in st->backdrop, 
    ** which covers the entire screen, and is used to deal with 
    ** expose events on the root window.
    ** Otherwise (viz. fill_bg != 0), then we have a single fill
    ** colour instead, a line of which is stored in st->backdrop.
    */
    bool_t      fill_bg;
    Stretch_clp backdrop_stretch;
    uint16_t    *backdrop;

    IO_clp      mgr_io;

    IDCCallback_cl       callback_cl;
    
    Region_T  updates_region;
    bool_t     update_lock_count;
    WSSvr_Window *first_exposed;
    
    WM_cl const *wm;
    
    Region_clp region;

    /* The event queue */
    WS_Event        events[NUM_EVENT_RECS];
    Event_Count     valid_event_ec;
    Event_Count     free_event_ec;
    Event_Sequencer free_event_sq;
    Thread_clp      event_thread;

};

/* WS Internal interfaces */
extern void MouseInit ( WSSvr_st *st );
extern void KbdInit ( WSSvr_st *st );
extern void FBInit ( WSSvr_st *st );

/* Handlers for various external events */
extern void HandleMouseEvent(WSSvr_st *st, Mouse_Event *ev);
extern void HandleKbdEvent(WSSvr_st *st, Kbd_Event *ev);

extern void Refocus(WSSvr_st *st);
extern bool_t EventDeliver( WSSvr_st *st, WSSvr_Window *w, 
			    WS_Event *ev, bool_t block);
extern void HandleRegionExposures( WSSvr_st *st, 
				   Region_T r, 
				   WSSvr_Window *w);

extern void HandleWindowExposures( WSSvr_st*st, 
				   WSSvr_Window *rec_window,
				   WSSvr_Window *first_exposed);

extern void InitBackdrop(WSSvr_st *st);
extern void BackdropLoadThread(WSSvr_st *st);
extern void ExposeBackdrop(WSSvr_st* st, uint32_t x1, uint32_t y1, 
			   uint32_t x2, uint32_t y2);
extern void UpdateThread(WSSvr_st *st);
extern void ProcessUpdates(WSSvr_st *st);
extern void EventThread(WSSvr_st *st);

void Window_Raise(WSSvr_st *st, WSSvr_Window *w);
void Window_Lower(WSSvr_st *st, WSSvr_Window *w);
void Window_Move(WSSvr_st *st, WSSvr_Window *w, int32_t x, int32_t y);


#endif /* _WSSVR_H_ */
