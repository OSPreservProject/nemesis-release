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
**      dev/pci/mga
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Device driver for PMAGBBA framestore providing clipped
**      write-only window support.
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: main.c 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
** 
**
*/

#include <autoconf/callpriv.h>
#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

#include <IOMacros.h>
#include <IDCMacros.h>

#ifdef CONFIG_CALLPRIV
#include <IOCallPriv.ih>
#include <FBBlitMod.ih>
#endif

#include <Closure.ih>

#include <salloc.h>

#include <regionstr.h>

//#define DEBUG
#define TEST_CARD

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef TEST_CARD
void test_card16a( uint16_t *ptr );
#endif


/* Cursor stuff */
#include "cursor32.xpm.h"
#define CURSOR_IDX   0
#define CURSOR_BITS  ((char *)(cursor32_xpm))
#define CURSOR_INITX 300 
#define CURSOR_INITY 162

extern void MGALoadCursor(char *cardbase, int index, char *cursor);
extern void MGAMoveCursor(char *scr, int idx, int x, int y);
extern char * MGAInit(int Width, int Height, int Depth, int Mode);


/*****************************************************************/
/*
 * Module stuff : Domain Entry Point
 */

/*****************************************************************/
/* State */

#include "FB_st.h"

/*****************************************************************/
/*
 * FB Exported interface
 */

#include <FB.ih>

static FB_Info_fn               FB_Info_m;
static FB_MgrStream_fn          FB_MgrStream_m;
static FB_MaskStream_fn         FB_MaskStream_m;
static FB_CreateWindow_fn       FB_CreateWindow_m;
static FB_DestroyWindow_fn      FB_DestroyWindow_m;
static FB_MapWindow_fn          FB_MapWindow_m;
static FB_UnMapWindow_fn        FB_UnMapWindow_m;
static FB_UpdateWindows_fn      FB_UpdateWindows_m;
static FB_ExposeWindow_fn       FB_ExposeWindow_m;
static FB_MoveWindow_fn         FB_MoveWindow_m;
static FB_ResizeWindow_fn       FB_ResizeWindow_m;
static FB_UpdateStream_fn       FB_UpdateStream_m;
static FB_AdjustQoS_fn          FB_AdjustQoS_m;
static FB_LoadCursor_fn         FB_LoadCursor_m;
static FB_SetCursor_fn 		FB_SetCursor_m;
static FB_SetLUT_fn    		FB_SetLUT_m;

static FB_op fb_ms = {         
  FB_Info_m,
  FB_MgrStream_m,
  FB_MaskStream_m,
  FB_CreateWindow_m,
  FB_DestroyWindow_m,
  FB_MapWindow_m,
  FB_UnMapWindow_m,
  FB_UpdateWindows_m,
  FB_ExposeWindow_m,
  FB_MoveWindow_m,
  FB_ResizeWindow_m,
  FB_UpdateStream_m,
  FB_AdjustQoS_m,
  FB_LoadCursor_m,
  FB_SetCursor_m,
  FB_SetLUT_m
};

/*****************************************************************/

#define EX_DBG(x) 

#define UNIMPLEMENTED \
printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

/*****************************************************************/

FB_st *fb_stp;

typedef uint32_t Bits32;
typedef uint32_t Pixel32;

static void InitClipRAM(FB_clp self)
{
  FB_st *st = (FB_st *)self->st;

  /* All pixels belong to bogus window */
  memset(st->clipbase, 0xFF, FB_WIDTH * (FB_HEIGHT + 16));

}

void Main (Closure_clp self)
{
    FB_st *st;
    IOEntryMod_clp emod;

    int i;
    
    TRC(printf("FB/MGA: entered\n"));
 
    st = (FB_st *)Heap$Malloc(Pvs(heap), 
			      sizeof(FB_st));
    if (st == NULL) 
	RAISE_Heap$NoMemory();

    memset(st, 0, sizeof(FB_st));

    CL_INIT(st->cl, &fb_ms, st);


    /* Before we do anything, see if we can find a framebuffer device
       that we understand. */

    st->cardbase = (Pixel_t *) MGAInit(FB_WIDTH, FB_HEIGHT, FB_DEPTH, FB_MODE);

    TRC(printf("st->cardbase = %p\n", st->cardbase));

    if(!st->cardbase) {
	fprintf(stderr, "Failed to initialise MGA device\n");

	Heap$Free(Pvs(heap), st);

	return;
    }

#ifdef TEST_CARD
    test_card16a( (uint16_t *) st->cardbase );
#else
    memset((uchar_t *) st->cardbase, 0, 
	   FB_WIDTH * FB_HEIGHT * (FB_DEPTH/8) );
#endif	

    /* Init state */
    st->qosleft = 100;
    st->cursched = 0;
    st->schedndx = 0;
    for (i = 0; i < FB_SCHEDLEN; i++) {
	st->sched[0][i] = st->sched[1][i] = 0;
    }
    
    /* Initialise window DS */
    for (i = 0 ; i < FB_MAX_WINDOWS; i++) {
	FB_Window *w = &st->window[i];
	w->wid  = i;
	w->free = True;
	w->tagword = w->tag  = w->wid;

	/* extend the tag to be a full word (32 or 64 bits) wide */
	w->tagword |= (w->tagword<<8);
	w->tagword |= (w->tagword<<16);
	/* Generates harmless overflow warning on 32bit architectures */
	w->tagword |= (w->tagword<<32);

    }
    
    /* Initialise stream DS */
    for (i = 0 ; i < FB_MAX_STREAMS; i++) {
	FB_Stream *s = &st->stream[i];
	s->sid  = i;
	s->free = True;
	s->qos  = 0;
    }

    TRC(printf("About to create an entry...\n"));

    /* Create an entry*/
    emod = NAME_FIND ("modules>IOEntryMod", IOEntryMod_clp);
    st->fbentry = IOEntryMod$New (emod, Pvs(actf), Pvs(vp),
				  VP$NumChannels (Pvs(vp)));    
    

    TRC(printf("About to grab stretches for FB and CLIP\n"));

    /* Grab stretches for FB and CLIP */
    {

	Stretch_clp           str;
	Stretch_Size          size;

	/* Get a stretch for clip RAM. Allocate a spare line of tiles
           at the start and end in case we need to blit over the edges */

	str    = STR_NEW(sizeof(tag_t)*FB_WIDTH*(FB_HEIGHT+16));
	if (!str) {
	    fprintf(stderr,"Couldn't get stretch");
	}
	st->clipstr = str;
	st->clipbase = (tag_t *) STR_RANGE(str, &size);
	st->clip = st->clipbase + (FB_WIDTH * 8);

	if (!st->clip || size < (FB_WIDTH*(FB_HEIGHT+16))) {
	    fprintf(stderr,"Stretch bogus\n");
	}
	TRC(fprintf(stderr,"FB: Clip RAM at %p size %lx\n", st->clip, size));	
    }
    TRC(printf("About to init clip RAM\n"));

    /* Initialise the clip RAM */
    InitClipRAM(&st->cl);

    TRC(printf("About to add dev>fblite to namespace\n"));
    CX_ADD("dev>fblite", st->cardbase, addr_t);

    TRC(printf("About to init blitting code\n"));
    /* Init the blitting code */
    blit_init(st);

    /* Turn on cursor */
    TRC(printf("About to turn on cursor\n"));
    MGALoadCursor(st->cardbase, CURSOR_IDX, CURSOR_BITS);
    MGAMoveCursor(st->cardbase, CURSOR_IDX, CURSOR_INITX, CURSOR_INITY);

    /* Export management interface */
    TRC(printf("About to export management interface\n"));
    IDC_EXPORT ("dev>FB", FB_clp, &st->cl);
    /* Go into a loop to service all the clients */


    /* Launch WSSvr */

    TRC(printf("Trying to launch wssvr\n"));
    TRY {
	Closure_clp wssvr;

	wssvr = NAME_FIND("modules>WSSvr", Closure_clp);
	
	Threads$Fork(Pvs(thds), wssvr->op->Apply, wssvr->st, 0);

	st->wssvr_shares_domain = True;
    } CATCH_ALL {
	printf("Couldn't launch WSSvr - exception %s\n",
	       exn_ctx.cur_exception);
    } ENDTRY;

    TRC(printf("FB/MGA: Init Done. (%ix%i %ibpp mode %i)\n",
	       FB_WIDTH,FB_HEIGHT,FB_DEPTH,FB_MODE));
    blit_loop(st);
}

/*****************************************************************/

static uint32_t FB_Info_m (
        FB_cl   *self
        /* RETURNS */,
        uint32_t        *height,
	uint32_t        *xgrid,
	uint32_t        *ygrid,
	uint32_t        *psize,
	uint32_t        *pbits,
	uint64_t        *protos)
{
    *height = FB_HEIGHT;
    
    *xgrid = PIXEL_X_GRID;
    *ygrid = PIXEL_Y_GRID;
    
    *psize = FB_DEPTH;
    *pbits = 15;
    
    *protos = 
	SET_ELEM(FB_Protocol_DFS8) | SET_ELEM(FB_Protocol_DFS16) |
	SET_ELEM(FB_Protocol_Bitmap8) | SET_ELEM(FB_Protocol_Bitmap16);
    
    return FB_WIDTH;
}

void process_mgr_pkt(FB_st *st, uint32_t arg,
			    IO_Rec *rec, uint32_t nrecs, 
			    word_t padding1, dcb_ro_t *dcb)
{

    FB_MgrPkt *pkt;

    if(nrecs != 1) {
	eprintf("process_mgr_pkt : num packets invalid : %d\n", nrecs);
	return;
    }

    pkt = rec->base;

    switch (pkt->tag) {

    case FB_MgrPktType_Mouse:
	
	FB_SetCursor_m (&st->cl, pkt->u.Mouse.index,
			pkt->u.Mouse.x, pkt->u.Mouse.y);
    
	break;

    default:

	eprintf("Invalid mgr packet type %u\n", pkt->tag);
	break;


    }

}

static IOOffer_clp FB_MgrStream_m (FB_clp self) {
    
    FB_st *st = (FB_st *)self->st;

#ifdef CONFIG_CALLPRIV
    CallPriv_AllocCode rc;
    CallPriv_clp cp;
    IOCallPriv_Info  info;
#endif
    IDCTransport_clp idc;
    IDCService_clp service;
    IOOffer_clp o;
    Type_Any any;

    extern void  process_mgr_pkt_stub();

    if (st->wssvr_shares_domain) {
	/* We are in the same domain - force WSSvr to use procedural
           interface */
	return NULL;
    }

#ifdef CONFIG_CALLPRIV    
    if(st->mgr_cp) {
	fprintf(stderr, "Already have one manager stream opened!!\n");
	return NULL;
    }

    cp = IDC_OPEN("sys>CallPrivOffer", CallPriv_clp);

#ifdef EB164
    rc = CallPriv$Allocate(cp, process_mgr_pkt_stub, st, &st->mgr_cp);
#else
    rc = CallPriv$Allocate(cp, process_mgr_pkt, st, &st->mgr_cp);
#endif
    
    if (rc != CallPriv_AllocCode_Ok) {
	fprintf(stderr, "Error initialising callpriv!\n");
	return NULL;
    }
    
    info = Heap$Malloc(Pvs(heap), sizeof(*info));
    
    info->slots = 64; 
    info->callpriv = st->mgr_cp;
    info->arg = 0;
    
    idc = NAME_FIND("modules>IOCallPrivTransport", IDCTransport_clp);
    
    TRC(fprintf(stderr, "Creating IOCallPriv update stream offer\n"));
    
    ANY_INIT(&any, IOCallPriv_Info, info);
    o = IDCTransport$Offer(idc, &any, NULL, Pvs(heap), 
			   Pvs(gkpr), NULL, &service);
    return o;
#else
    return NULL;
#endif
}


static IOOffer_clp FB_MaskStream_m (
        FB_cl   *self,
        FB_Protocol p)
{
    FB_st *st = (FB_st *)self->st;
    UNIMPLEMENTED;
    return (IOOffer_clp) NULL;
}

static FB_WindowID FB_CreateWindow_m (
        FB_cl   	*self,
        int32_t 	x       /* IN */,
        int32_t 	y       /* IN */,
        uint32_t        width   /* IN */,
        uint32_t        height  /* IN */,
        bool_t  	clip    /* IN */ )
{
  FB_st *st = (FB_st *)self->st;
  FB_Window *w;
  FB_WindowID wid = 0;
  
  x &= ~(PIXEL_X_GRID - 1);
  y &= ~(PIXEL_Y_GRID - 1);

  TRC(fprintf(stderr,"FB_CreateWindow_m (%p, %d, %d, %d, %d, %d)\n",
	     self, x, y, width, height, clip));

  /* Find an unused window */
  ENTER();
  while (wid < FB_MAX_WINDOWS && !st->window[wid].free) wid++;
  if (wid < FB_MAX_WINDOWS) {
    w = &st->window[wid];
    w->free = False;
  } 
  LEAVE();
  
  if (wid == FB_MAX_WINDOWS) RAISE_FB$Failure();

  {
    FB_Window *w = &st->window[wid];
    
    w->mapped = True;		/* XXX */
    w->x      = x;
    w->y      = y;
    w->width  = width;
    w->height = height;
    w->stride = height;

    w->clipped = clip;		/* XXX */

    w->clip = st->clip + (y * FB_WIDTH) + x;

    w->pmag = st->cardbase+ (y * FB_WIDTH) + x;
  }


  return wid;
}

static void FB_DestroyWindow_m (
        FB_cl   *self,
        FB_WindowID     wid       /* IN */ )
{
    FB_st *st = (FB_st *)self->st;
    FB_Window *w = &st->window[wid];
    FB_StreamID  NOCLOBBER sid = 0;
    FB_Stream *NOCLOBBER s;
	
    FB_UnMapWindow_m(self, wid);
    
    /* Close any streams on this window */
    
    while(sid < FB_MAX_STREAMS) {
	s = &st->stream[sid];
	if((!s->free) && (s->w == w)) {
	    
	    s->w = 0;
	    s->free = True;
	    
	    /* XXX Ought to revoke IDCOffer and free FBBlit */	    
	}
	
	sid++;
    }
    
    /* Free the window */

    w->free = True;

}

static void FB_MapWindow_m (
        FB_cl   *self,
        FB_WindowID     wid     /* IN */ )
{
  FB_st *st = (FB_st *)self->st;
  FB_Window *w = &st->window[wid];

  w->mapped = True;
}

static void FB_UnMapWindow_m (
        FB_cl   *self,
        FB_WindowID     wid     /* IN */ )
{
  FB_st *st = (FB_st *)self->st;
  FB_Window *w = &st->window[wid];

  w->mapped = False;
}

static void FB_UpdateWindows_m(FB_clp self,
			       const FB_WinUpdatePkt *updates)
{

    FB_st *st = (FB_st *)self->st;
    int i;

    for(i = 0; i < SEQ_LEN(updates); i++) {
	
	FB_MgrWinData *data = &SEQ_ELEM(updates, i);
    
	FB_MoveWindow_m(self, data->wid, data->x, data->y);

	FB_ResizeWindow_m(self, data->wid, data->width, data->height);

	if(data->exposemask) 
	    FB_ExposeWindow_m(self, data->wid, data->exposemask);

    }

}

#define XTILE(x) (x>>3)
#define YTILE(y) (y>>3)
#define CLIP_STRIDE (YTILE(FB_WIDTH))
#define CLIP_TILE(st,x,y) (st->clip + (YTILE(y) * CLIP_STRIDE) + XTILE(x))

static void FB_ExposeWindow_m (
        FB_cl   *self,
        FB_WindowID     wid     /* IN */,
	Region_T region)
{
    Region_BoxPtr rptr;
    FB_st *st = (FB_st *)self->st;
    FB_Window *w = &st->window[wid];
    int i;

    tag_t tag = w->tag;
    word_t tagword = w->tagword;
    word_t tagcmp;
    tag_t   *cp;			/* clip RAM pointer */
    uint64_t x, y;		        /* current coord */
    uint64_t xmax, ymax;
    uint64_t row;

    TRC(fprintf(stderr,"FB_ExposeWindow_m (%p, %x)\n", self, wid));
    
    rptr = REGION_RECTS(region);

    for(i = 0; i<REGION_NUM_RECTS(region); i++)
    {
	y = MAX(0, rptr[i].y1);
	xmax = MIN(rptr[i].x2, FB_WIDTH);
	ymax = MIN(rptr[i].y2, FB_HEIGHT);
	
	for ( ; y < ymax; y++) {
	    EX_DBG(uint16_t *dodo);

	    x = MAX(0, rptr[i].x1);
	    
	    cp = st->clip + y*FB_WIDTH + x;
	    EX_DBG(dodo = st->cardbase + y * FB_WIDTH + x);

	    for(; (x&(sizeof(word_t) - 1)) && x < xmax; x++) {
		tag_t t = *cp;
		if(t != tag) {

		    *cp = tag;
		}
		cp++;

		EX_DBG(*dodo++ = 0xd0d0);
	    }
	    
	    for ( ; x < (xmax & ~(sizeof(word_t) - 1)); x+= sizeof(word_t)) {
		tagcmp = *((word_t *)cp);
		
		if(tagcmp != tagword) {
		    *((word_t *)cp) = tagword;
		}

		((word_t *)cp)++;
		EX_DBG(*((uint32_t *)dodo)++ = 0xd0d0d0d0);
		EX_DBG(*((uint32_t *)dodo)++ = 0xd0d0d0d0);
	    }

	    for( ; x < xmax; x++) {
		tag_t t = *cp;
		if(t != tag) {
		    *cp = tag;
		}
		cp++;
		EX_DBG(*dodo++ = 0xd0d0);
	    }
	    
	}
    }

}

static void FB_MoveWindow_m (
        FB_cl   *self,
        FB_WindowID     wid     /* IN */,
        int32_t x       	/* IN */,
        int32_t y       	/* IN */ )
{
  FB_st *st = (FB_st *)self->st;
  FB_Window *w = &st->window[wid];

  x &= ~ (PIXEL_X_GRID - 1);
  y &= ~ (PIXEL_Y_GRID - 1);

  TRC(fprintf(stderr,"FB_MoveWindow_m (%p, %x, %d, %d)\n",
	     self, wid, x, y));
  ENTER();
  w->x  = x;
  w->y  = y;
  w->clip = st->clip + y*FB_WIDTH + x;
  w->pmag = st->cardbase + y * FB_WIDTH + x;
  LEAVE();
}

static void FB_ResizeWindow_m (
        FB_cl   *self,
        FB_WindowID     wid     /* IN */,
        uint32_t width   	/* IN */,
        uint32_t height  	/* IN */ )
{
  FB_st *st = (FB_st *)self->st;
  FB_Window *w = &st->window[wid];

  TRC(fprintf(stderr,"FB_ResizeWindow_m (%p, %x, %d, %d)\n",
	     self, wid, width, height));
  ENTER();
  w->width = width;
  w->height = height;
  LEAVE();
}

static FB_StreamID FB_UpdateStream_m (
        FB_cl          *self,
        FB_WindowID     wid     /* IN */,
        FB_Protocol     p       /* IN */,
        FB_QoS          q       /* IN */,
        bool_t          clip    /* IN */
       /* RETURNS */,
        IOOffer_clp   *io,
	FBBlit_clp     *blit)
{
    FB_st *st = (FB_st *)self->st;
    FB_Window *w = &st->window[wid];
    FB_StreamID NOCLOBBER        sid = 0;
    FB_Stream *NOCLOBBER s;
    
    TRC(fprintf(stderr,"FB_UpdateStream_m (%p, %lx, %d, %d, %d)\n",
		self, wid, p ,q, clip));

    TRC(fprintf(stderr,"Finding an unused stream\n"));
    /* Find an unused stream */
    ENTER();
    while (sid < FB_MAX_STREAMS && !st->stream[sid].free) sid++;
    if (sid < FB_MAX_STREAMS) {
	s = &st->stream[sid];
	s->free = False;
    } 
    LEAVE();
    
    if (sid == FB_MAX_STREAMS) RAISE_FB$Failure();
    
    TRC(fprintf(stderr,"Setting up stream %d\n", sid));
    /* Set up an update stream offer */
    {
	IOTransport_clp	iot;
	IDCService_clp      service;
	IDCService_clp      sv;
	IDCOffer_clp        o;
	
	/* XXX NAS: Want to work out stream from IO_clp handed beck from a 
	 * rendezvous - slap it in handle until someone gives me a better
	 * idea.
	 */
	TRC(fprintf(stderr,"Digging up IO Transport\n"));  
	iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);
	TRC(fprintf(stderr,"Making IO transport OFfer\n"));
	
	o = IOTransport$Offer (iot, Pvs(heap), 128, IO_Mode_Tx, IO_Kind_Master,
			       NULL, Pvs(gkpr), NULL, st->fbentry, (word_t)s, 
			       &service);

	TRC(fprintf(stderr, "Made IOTransport offer\n"));

	/* Can potentially use IDCService to revoke the offer */
	s->service = service;
	s->offer   = o;
	
	s->w       = w;
	s->proto   = p;

	s->blit_m = st->blit_fns[p];

    } 

#ifdef CONFIG_CALLPRIV

    TRC(fprintf(stderr, "Making Callpriv blit offer\n"));
    TRY {
	*blit = FBBlitMod$New(NAME_FIND("modules>FBBlitMod", FBBlitMod_clp),
			      sid, st->blit_cps[p]);
    } CATCH_Context$NotFound(n) {
	fprintf(stderr, "Unable to locate '%s'\n", n);
	RAISE_FB$Failure();
    } ENDTRY;
#else

    *blit = NULL;

#endif

    *io = s->offer;
    TRC(fprintf(stderr,"FB_UpdateStream_m: Done, Offer:%x\n",*io ));
    
    return sid;
}

static void FB_AdjustQoS_m (
        FB_cl          *self,
        FB_StreamID     sid,			    
        FB_QoS          q )
{
    FB_st *st = (FB_st *) self->st;
    UNIMPLEMENTED;
}

static void FB_LoadCursor_m (
        FB_cl   	*self,
        uint32_t        index   /* IN */,
        uchar_t         *bits    /* IN */ )
{
    FB_st *st = (FB_st *) self->st;

    eprintf("FB$LoadCursor: idx=%x, bits at %p", index, bits);
    MGALoadCursor(st->cardbase, index, bits);
}


static void FB_SetLUT_m (
        FB_cl   	*self,
        uint32_t        index   /* IN */,
        uint32_t        num     /* IN */,
        FB_LUTEntry_ptr entries /* IN */ )
{
    FB_st *st = (FB_st *) self->st;
    UNIMPLEMENTED;
}


static void FB_SetCursor_m (
        FB_cl   	*self,
        uint32_t        index   /* IN */,
        uint32_t        x       /* IN */,
        uint32_t        y       /* IN */ )
{
    FB_st *st = (FB_st *) self->st;

    MGAMoveCursor(st->cardbase, index, x, y);
}

/* End */




#ifdef TEST_CARD

inline uint32_t RGB15( uint32_t r, uint32_t g, uint32_t b )
{
    return (r&0x1f) | ((g&0x1f)<<5) | ((b&0x1f)<<10) ;
}

#define WIDTH  FB_WIDTH
#define HEIGHT FB_HEIGHT

#define PITCH  (FB_WIDTH/2)  /* in int32 */

#define YSKIP  (FB_HEIGHT/4)

void test_card16a( uint16_t *ptr )
{
    int x,y,c;
    uint32_t *p = (uint32_t *)ptr;

    for(y=0;y<YSKIP;y++)
    {
	for(x=0;x<WIDTH/2;x++)
	{
	    c = RGB15( (x&0x1f), 0, 0 );
	    p[x] = c | (c<<16);
	}
	p+=PITCH;
    }

    for(y=YSKIP;y<2*YSKIP;y++)
    {
	for(x=0;x<WIDTH/2;x++)
	{
	    c = RGB15( 0, (x&0x1f), 0);
	    p[x] = c | (c<<16);
	}
	p+=PITCH;
    }

    for(y=YSKIP*2;y<3*YSKIP;y++)
    {
	for(x=0;x<WIDTH/2;x++)
	{
	    c = RGB15( 0, 0, (x&0x1f) );
	    p[x] = c | (c<<16);
	}
	p+=PITCH;
    }

    for(y=YSKIP*3;y<4*YSKIP;y++)
    {
	for(x=0;x<WIDTH/2;x++)
	{
	    c = RGB15( (x&0x1f), (x&0x1f), (x&0x1f) );
	    p[x] = c | (c<<16);
	}
	p+=PITCH;
    }

}

#endif
