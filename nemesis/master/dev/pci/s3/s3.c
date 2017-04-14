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
**      dev/pci/s3
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
** ID : $Id: s3.c 1.2 Wed, 28 Apr 1999 11:42:49 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>

#include <IDCMacros.h>
#include <IOMacros.h>
#include <IOCallPriv.ih>
#include <FBBlitMod.ih>
#include <Closure.ih>

#include <salloc.h>
#include <regionstr.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

int isti3026(void);
int isibmrgb(void);

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

static const unsigned char asin[256] = {
  0,   0,   1,   1,   2,   3,   3,   4,
  5,   5,   6,   7,   7,   8,   8,   9,
  10,  10,  11,  12,  12,  13,  14,  14,
  15,  16,  16,  17,  17,  18,  19,  19,
  20,  21,  21,  22,  23,  23,  24,  25,
  25,  26,  26,  27,  28,  28,  29,  30,
  30,  31,  32,  32,  33,  34,  34,  35,
  36,  36,  37,  38,  38,  39,  40,  40,
  41,  42,  42,  43,  43,  44,  45,  45,
  46,  47,  47,  48,  49,  49,  50,  51,
  52,  52,  53,  54,  54,  55,  56,  56,
  57,  58,  58,  59,  60,  60,  61,  62,
  62,  63,  64,  64,  65,  66,  67,  67,
  68,  69,  69,  70,  71,  71,  72,  73,
  74,  74,  75,  76,  76,  77,  78,  79,
  79,  80,  81,  82,  82,  83,  84,  84,
  85,  86,  87,  87,  88,  89,  90,  90,
  91,  92,  93,  93,  94,  95,  96,  97,
  97,  98,  99, 100, 100, 101, 102, 103,
  104, 104, 105, 106, 107, 108, 108, 109,
  110, 111, 112, 113, 113, 114, 115, 116,
  117, 118, 118, 119, 120, 121, 122, 123,
  124, 125, 125, 126, 127, 128, 129, 130,
  131, 132, 133, 134, 135, 136, 137, 137,
  138, 139, 140, 141, 142, 143, 144, 145,
  146, 147, 149, 150, 151, 152, 153, 154,
  155, 156, 157, 158, 159, 161, 162, 163,
  164, 165, 167, 168, 169, 170, 172, 173,
  174, 176, 177, 178, 180, 181, 183, 184,
  186, 187, 189, 191, 192, 194, 196, 197,
  199, 201, 203, 205, 207, 210, 212, 215,
  217, 220, 223, 227, 230, 235, 241, 255
};

/*****************************************************************/

static const unsigned char cos[256] = {
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 254,
  254, 254, 254, 254, 254, 253, 253, 253,
  253, 252, 252, 252, 252, 251, 251, 251,
  251, 250, 250, 250, 249, 249, 249, 248,
  248, 247, 247, 247, 246, 246, 245, 245,
  244, 244, 243, 243, 242, 242, 241, 241,
  240, 240, 239, 239, 238, 238, 237, 236,
  236, 235, 235, 234, 233, 233, 232, 231,
  231, 230, 229, 229, 228, 227, 227, 226,
  225, 224, 224, 223, 222, 221, 220, 220,
  219, 218, 217, 216, 215, 215, 214, 213,
  212, 211, 210, 209, 208, 208, 207, 206,
  205, 204, 203, 202, 201, 200, 199, 198,
  197, 196, 195, 194, 193, 192, 191, 190,
  189, 188, 187, 185, 184, 183, 182, 181,
  180, 179, 178, 177, 175, 174, 173, 172,
  171, 170, 168, 167, 166, 165, 164, 162,
  161, 160, 159, 158, 156, 155, 154, 153,
  151, 150, 149, 147, 146, 145, 144, 142,
  141, 140, 138, 137, 136, 134, 133, 132,
  130, 129, 127, 126, 125, 123, 122, 121,
  119, 118, 116, 115, 114, 112, 111, 109,
  108, 106, 105, 104, 102, 101,  99,  98,
  96,  95,  93,  92,  91,  89,  88,  86,
  85,  83,  82,  80,  79,  77,  76,  74,
  73,  71,  70,  68,  67,  65,  63,  62,
  60,  59,  57,  56,  54,  53,  51,  50,
  48,  47,  45,  43,  42,  40,  39,  37,
  36,  34,  33,  31,  29,  28,  26,  25,
  23,  22,  20,  18,  17,  15,  14,  12,
  11,   9,   7,   6,   4,   3,   1,   0
};

/*****************************************************************/

FB_st *fb_stp;


#define R1 128
#define R2 64
#define CENTRE_X 320
#define CENTRE_Y 720

typedef uint32_t Bits32;
typedef uint32_t Pixel32;

static void InitClipRAM(FB_clp self)
{
  FB_st *st = (FB_st *)self->st;

  /* All pixels belong to bogus window */
  memset(st->clipbase, 0xFF, FB_WIDTH * (FB_HEIGHT + 16));

#if 0
  /* Make window 0 anular */
  for (i = 0; i <= R1; i++) {
    unsigned int a1, a2;
    unsigned int r1ca, r2ca;

    a1  = asin[(255*i) / R1];
    r1ca = (R1 * (uint32_t)cos[a1]) / 255;

    if (i >= R2) {
      
      /* Big circle radius R1 */
      FB_ExposeWindow_m(self, 0, CENTRE_X-r1ca, CENTRE_Y-i, 2*r1ca, 1);
      FB_ExposeWindow_m(self, 0, CENTRE_X-r1ca, CENTRE_Y+i, 2*r1ca, 1); 

    } else {

      /* Subtract smaller circle radius R2 */
      a2  = asin[(255*i) / R2];
      r2ca = (R2 * cos[a2]) / 255;

      /* Left */
      FB_ExposeWindow_m(self, 0, CENTRE_X-r1ca, CENTRE_Y-i, r1ca - r2ca, 1);
      FB_ExposeWindow_m(self, 0, CENTRE_X-r1ca, CENTRE_Y+i, r1ca - r2ca, 1);

      /* Right */
      FB_ExposeWindow_m(self, 0, CENTRE_X+r2ca, CENTRE_Y-i, r1ca - r2ca, 1);
      FB_ExposeWindow_m(self, 0, CENTRE_X+r2ca, CENTRE_Y+i, r1ca - r2ca, 1);
    }
  }
#endif /* 0 */
}


void Main (Closure_clp self)
{
    FB_cl *fbclp;
    FB_st *st;
    IOEntryMod_clp emod;

    int i;
    
    TRC(printf("FB/S3: entered\n"));
    
    st = (FB_st *)Heap$Malloc(Pvs(heap), 
			      sizeof(FB_st));
    if (st == NULL) 
	RAISE_Heap$NoMemory();

    memset(st, 0, sizeof(FB_st));

    fbclp = &st->cl;
    CLP_INIT(fbclp, &fb_ms, st);


    /* Before we do anything, see if we can find a framebuffer device
       that we understand. */

    st->cardbase = (pixel16_t *) fb_init();

    TRC(printf("st->cardbase = %p\n", st->cardbase));

    if(!st->cardbase) {
        fprintf(stderr, "FB/S3: Failed to initialise device\n");
        //ntsc_halt();

        Heap$Free(Pvs(heap), st);

        return;
    }

#if 0
    video_engine_init(st); /* Initialise Video Engine stuff */
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

    /* Create an entry*/
    emod= NAME_FIND ("modules>IOEntryMod", IOEntryMod_clp);
    st->fbentry = IOEntryMod$New (emod, Pvs(actf), Pvs(vp),
				  VP$NumChannels (Pvs(vp)));    
    

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
	st->clipbase = (tag_t *)STR_RANGE(str, &size);
	st->clip = st->clipbase + (FB_WIDTH * 8);

	if (!st->clip || size < (FB_WIDTH*(FB_HEIGHT+16))) {
	    fprintf(stderr,"Stretch bogus\n");
	}
	STR_SETPROT(str, VP$ProtDomID(Pvs(vp)), 
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) | 
		    SET_ELEM(Stretch_Right_Meta));
	TRC(fprintf(stderr,"FB: Clip RAM at %p size %lx\n", st->clip, size));	
    }
    /* Initialise the clip RAM */
    InitClipRAM(fbclp);

    /* Init the blitting code */

    CX_ADD("dev>fblite", st->cardbase, addr_t);
    blit_init(st);

    /* Turn on cursor */
    if(isti3026())
    {
	s3Ti3026CursorOn();
	s3Ti3026LoadCursor(0,0,0,0);
    }
    if(isibmrgb())
    {
	s3IBMRGBCursorOn();
	s3IBMRGBLoadCursor(0,0,0,0);
    }

    /* Export management interface */
    IDC_EXPORT ("dev>FB", FB_clp, fbclp);
    /* Go into a loop to service all the clients */

    /* Launch WSSvr */

    TRY {
	Closure_clp wssvr;

	wssvr = NAME_FIND("modules>WSSvr", Closure_clp);
	
	Threads$Fork(Pvs(thds), wssvr->op->Apply, wssvr->st, 0);

	st->wssvr_shares_domain = True;
    } CATCH_ALL {
	TRC(printf("Couldn't launch WSSvr - exception %s\n",
		   exn_ctx.cur_exception));
    } ENDTRY;

    printf("FB/S3: Init Done.\n");

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
  FB_st *st = (FB_st *)self->st;

  *height = FB_HEIGHT;

  *xgrid = PIXEL_X_GRID;
  *ygrid = PIXEL_Y_GRID;

  *psize = 16;
  *pbits = 15;

  *protos = 
      SET_ELEM(FB_Protocol_DFS8) | SET_ELEM(FB_Protocol_DFS16) |
      SET_ELEM(FB_Protocol_Bitmap8) | SET_ELEM(FB_Protocol_Bitmap16) |
      SET_ELEM(FB_Protocol_Video16);

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

    CallPriv_AllocCode rc;
    CallPriv_clp cp;

    IDCTransport_clp idc;
    IOCallPriv_Info  info;
    IDCService_clp service;
    IOOffer_clp o;
    Type_Any any;

    extern void  process_mgr_pkt_stub();

    if(st->wssvr_shares_domain) {
	/* We are in the same domain - force WSSvr to use procedural
           interface */
	return NULL;
    }

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
    
    if(rc != CallPriv_AllocCode_Ok) {
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
	    
	    if(s->io) {
		IO$Dispose(s->io);
		IDCService$Destroy(s->service);
	    }
	    
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
    FB_st        *st = (FB_st *)self->st;
    FB_Window    *w = &st->window[wid];
    int           i;

    tag_t         tag = w->tag;
    word_t        tagword = w->tagword;
    word_t        tagcmp;
    tag_t        *cp;			/* clip RAM pointer */
    uint64_t      x, y;		        /* current coord    */
    uint64_t      xmax, ymax;
    uint64_t      row;

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
        IOOffer_clp   *offer,
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
    
    s->io = NULL;
    s->offer = NULL;
    s->service = NULL;

    TRC(fprintf(stderr,"Setting up stream %d\n", sid));
    /* Set up an update stream offer */
    {
	IOTransport_clp	iot;
	IDCService_clp      service;
	IDCService_clp      sv;
	IDCOffer_clp        o;
	
	/* XXX NAS want to work out stream from IO_clp handed back from a 
	 * rendezvous - slap it in handle until someone gives me a better
	 * idea.
	 */
	TRC(fprintf(stderr,"Digging up IO Transport\n"));  
	iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);
	TRC(fprintf(stderr,"Making IO transport OFfer\n"));
	
	o = IOTransport$Offer (iot, Pvs(heap), 128, IO_Mode_Tx, IO_Kind_Master,
			       NULL, Pvs(gkpr), NULL, st->fbentry, (word_t)s, 
			       &service);
	
	/* Can potentially use IDCService to revoke the offer */
	s->service = service;
	s->offer   = o;
	s->io      = NULL;
	
	s->w       = w;
	s->proto   = p;

	s->blit_m  = st->blit_fns[p];
    
    } 

    TRY {
	*blit = FBBlitMod$New(NAME_FIND("modules>FBBlitMod", FBBlitMod_clp),
			      sid, st->blit_cps[p]);
    } CATCH_Context$NotFound(n) {
	fprintf(stderr, "Unable to locate '%s'\n", n);
	RAISE_FB$Failure();
    } ENDTRY;

    *offer = s->offer;
    TRC(fprintf(stderr,"FB_UpdateStream_m: Done, Offer:%x\n",*offer ));
    
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
  UNIMPLEMENTED;

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

  if(isti3026())
      s3Ti3026MoveCursor(0, x, y);
  if(isibmrgb())
      s3IBMRGBMoveCursor(0, x, y);

}

/* XXX ; this is horrible; xrip.h dragged in specially */
#include "xrip/h/xrip.h"

int isti3026(void ){
    return DAC_IS_TI3026;
}

int isibmrgb(void) {
    return DAC_IS_IBMRGB;
}
