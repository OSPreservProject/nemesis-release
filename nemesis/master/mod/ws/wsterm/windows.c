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
** ID : $Id: windows.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <ntsc.h>
#include <stdio.h>
#include <salloc.h>
#include <time.h>

#include <WS.h>

#include <IO.ih>
#include <IOMacros.h>

#define TRC(x) 

#include "WSprivate.h"

#define PACK      8
#define NTXBUFS   32
#define TXPKTSIZE 288
#define TXBUFSIZE 512

#ifdef TILED
void WSFlush_Callpriv_DFS(Window w);
void WSFlush_IO_DFS_8(Window w);
void WSFlush_IO_AVA(Window w);
#else /* TILED */
void WSFlush_Callpriv_Raster(Window w);
void WSFlush_IO_Raster(Window w);
void WSFlush_IO_Raster_8(Window w);
#endif /* TILED */

Window WSCreateWindow(Display *d,
	       int32_t	_x,
	       int32_t	y,
	       uint32_t	width,
	       uint32_t	height)
{
  int32_t NOCLOBBER x = _x;
  IOOffer_clp offer;		/* FB IDC channel offer */
  Window       w;
  StretchAllocator_clp  salloc;

  TRC(printf("WSCreateWindow(%#lx, %d, %d, %d, %d)\n", 
	     d, x, y, width, height));

  salloc = Pvs(salloc);

  /* Alignment/size constraints XXX should get from WSSvr */
  x &= (~2);

  /* Create a window record */
  w = Heap$Malloc(Pvs(heap), sizeof(struct _Window));
  if (!w) RAISE_Heap$NoMemory();

  /* Try to create a window */
  TRY 
    w->id = WS$CreateWindow(d->ws, x, y, width, height);
  CATCH_WS$Failure()
    RERAISE;
  ENDTRY;
  TRC(printf("WSCreateWindow: WS_WindowID %x\n", w->id));

  /* Ask for an update stream */
  TRY 
#ifdef TILED
    w->sid = WS$UpdateStream(d->ws, w->id, FB_Protocol_DFS8, 0, True, &offer,
			     &w->blit);
#else
    w->sid = WS$UpdateStream(d->ws, w->id, FB_Protocol_Bitmap8, 0, True, &offer);
#endif
  CATCH_WS$Failure()
    RERAISE;
  ENDTRY;
  TRC(printf("WSCreateWindow Stream offer %p\n", offer));

  /* Try to connect to framestore */
  w->fb = IO_BIND(offer);  
  #ifdef TILED
  w->flush_m = WSFlush_Callpriv_DFS;
  #else
  w->flush_m = WSFlush_IO_Raster_8;
  #endif
  TRC(printf("WSCreateWindow: Bound to update stream\n"));

  TRC(printf("w->fb = %p\n", w->fb));

  /* Fill in */
  w->display = d;
  w->x       = x;
  w->y       = y;
  w->width   = width;
  w->height  = height;
  w->stride  = width;	
  w->fg      = 0xFFFFFFFFFFFFFFFFULL;
  w->bg      = 0x0000000000000000UL;

  /* Create a local pixel buffer */
  {
    Stretch_Size          size;
    
    w->pixstr  = STR_NEW_SALLOC(salloc, (w->height * w->stride));
    if (!w->pixstr)
    {
	printf("Broken\n");
	ntsc_halt();
    }
    w->pixels  = STR_RANGE(w->pixstr, &size);
    if (!w->pixels || size < (w->height * w->stride))
    {
	printf("Stretch bogus");
	ntsc_halt();
    }
    SALLOC_SETGLOBAL(salloc, w->pixstr, SET_ELEM(Stretch_Right_Read) );
    TRC(printf("WSCreateWindow: Pixels at %p size %lx\n", w->pixels, size));
  }

  /* Malloc a diffs array (one bit per tile, 8x8 tile square per uint64_t) */
  { 
    int xtiles = w->stride >> 3;
    int ytiles = w->height >> 3;
    int nwords;

    /* Round width sizes up to a sensible amount */
    xtiles = (xtiles + 7) & ~7; 
    ytiles = (ytiles + 7) & ~7; 
    
    /* How many entries in diffs array */
    nwords = (xtiles>>3) * (ytiles>>3);
    TRC(printf("xtiles %d ytiles %d nwords %d\n", 
	   xtiles, ytiles, nwords));

    w->diffstride = xtiles>>3;
    w->diffsize = nwords * sizeof(uint64_t);
    w->diffs  = (uint64_t *) Heap$Malloc (Pvs(heap), 
					  w->diffsize);
    if (!w->diffs)
    {
	printf("Malloc failed\n");
	ntsc_halt();
    }
    memset(w->diffs, 0, w->diffsize);
    TRC(printf("%d: Diffs %d bytes at %p (stride %d)\n",
	   w->id, w->diffsize, w->diffs, w->diffstride));
  }

  if (1) {
    Stretch_Size          size;
    
    /* Get a stretch for TX buffers. */
    w->bufstr = STR_NEW_SALLOC(salloc, (NTXBUFS * TXBUFSIZE));
    if (!w->bufstr) 
    {
	printf("Couldn't get stretch");
	ntsc_halt();
    }
    w->buf  = (uint64_t *) STR_RANGE(w->bufstr, &size);
    if (!w->buf || size < (NTXBUFS * TXBUFSIZE)) 
    {
	printf("Stretch bogus");
	ntsc_halt();
    }
    SALLOC_SETGLOBAL(salloc, w->bufstr, SET_ELEM(Stretch_Right_Read) );
    TRC(printf("WSCreateWindow: Buffer at %p size %lx\n", w->buf, size));
    w->sent = 0;
  }

  return w;
}
		
void WSDestroyWindow(Window	w)
{
    WS$DestroyWindow(w->display->ws, w->id);
    FREE(w);
}

/*
 * Move/resize - Should be done by WM exposure
 */
void WSMapWindow(Display  *d,
		 Window    w)
{
    WS$MapWindow(d->ws, w->id);
}

void WSUnMapWindow(Display  *d,
		   Window    w)
{
    WS$UnMapWindow(d->ws, w->id);
}

void WSMoveWindow(Display  *d,
		  Window    w,
		  int32_t   x,
		  int32_t   y)
{
    WS$MoveWindow(d->ws, w->id, x, y);
    w->x = x;
    w->y = y;
}

void WSResizeWindow(Display  *d,
		    Window    w,
		    uint32_t  width,
		    uint32_t  height)
{
    WS$ResizeWindow(d->ws, w->id, width, height);
    w->width  = width;
    w->height = height;
}


/*
 * FG/BG colour for rendering
 */
void WSSetForeground(Window	 w, 
		     uint32_t fg)
{
    uint64_t p = fg & 0xFF;
    p |= p<<8;
    p |= p<<16;
    p |= p<<32;
    w->fg =p;
}		

void WSSetBackground(Window	 w,
		     uint32_t bg)
{
    uint64_t p = bg & 0xFF;
    p |= p<<8;
    p |= p<<16;
    p |= p<<32;
    w->bg =p;
}		

FB_StreamID
WSUpdateStream(Window w, FB_Protocol p, FB_QoS q, bool_t clip, 
	       IOOffer_clp *offer, FBBlit_clp *blit)
{
  FB_StreamID NOCLOBBER sid;
  
  /* Ask for an update stream */
  TRY {
      sid = WS$UpdateStream(w->display->ws, w->id, p, q, clip, offer, blit);
  } CATCH_WS$Failure() {
      return (FB_StreamID) -1;
  } ENDTRY;
  
  return sid;
}

struct raster_hdr {
  uint64_t x;
  uint64_t y;
  uint64_t ntiles;
  uint64_t dataptr;
  uint64_t datastride;
};

#define DFS_MAGIC (~0xBadCe11)	
struct dfs_hdr {
  uint32_t magic;		/* Protect against cell loss */
  uint8_t  x;
  uint8_t  y;
  uint8_t  xtiles;
  uint8_t  ytiles;
};

/* XXX Blit CALLPRIV IDS */
#define FB_PRIV_BLIT_AVA    0x00
#define FB_PRIV_BLIT_DFS    0x01
#define FB_PRIV_BLIT_RASTER 0x02

#ifdef TILED

/*
 * Callpriv DFS flush method 
 */
void WSFlush_Callpriv_DFS(Window w)
{
    uint64_t x, y;

    uint64_t bits = 0;
    uint64_t pdus = 0;
    
    FBBlit_Rec rec;
    
    for (y = 0; y < w->height>>3; y++) {
	for (x = 0; x < w->width>>3; x+= PACK) {
	    if ((DIRTY(w, x, y) & ((1<<PACK)-1)) || (w->yroll != w->yrollat)) { /* XXX PACK <= 8 */
		rec.sourcewidth = PACK << 3;
		rec.destwidth = PACK << 3;
		rec.sourceheight = 1 << 3;
		rec.destheight = 1 << 3;
		rec.x = x<<3;
		rec.y = ((y<<3)+w->yroll)%(w->height);
		rec.data = (tile_t *)w->pixels + (y * (w->width>>3)) + x;
		
		while(FBBlit$Blit(w->blit, &rec));
		
		bits += (TXPKTSIZE*8); pdus++;
	    }
	}
    }
    w->yrollat = w->yroll;
    memset(w->diffs, 0, w->diffsize);
}

#else /* TILED */
#warning untiled
/*
 * Callpriv Raster flush method 
 */
static void WSFlush_Callpriv_Raster(Window w)
{
  IO_Rec   rec;
  uint64_t x, y;

  uint64_t bits = 0;
  uint64_t pdus = 0;
  Time_ns start = NOW();

  static int times = 0;
  struct raster_hdr hdr;
  rec.base = &hdr;
  hdr.datastride = w->stride>>3;
  hdr.ntiles     = PACK;

  for (y = 0; y < w->height>>3; y++) {
    for (x = 0; x < w->width>>3; x+= PACK) {
      if (DIRTY(w, x, y) & ((1<<PACK)-1)) { /* XXX PACK <= 8 */
	hdr.x = x;
	hdr.y = y;
	hdr.dataptr = w->pixels + (y << 3) * w->stride + (x<<3);
	bits += (TXPKTSIZE*8); pdus++;
      }
    }
  }
  {
    if(++times == 1000) {
      Time_ns end = NOW();
      TRC(printf("%d: TX at %ld bps %ld pdu/s\n", 
	     w->id,
	     (bits * SECONDS(1)) / (1+end-start),
	     (pdus * SECONDS(1)) / (1+end-start)));
      times = 0;
    }
  }
  memset(w->diffs, 0, w->diffsize);
}

#endif /* TILED */

#ifdef TILED 

/*
 * Tiled IO DFS flush method 
 */
void WSFlush_IO_DFS_8(Window w)
{
  IO_Rec   rec[2];
  uint32_t nrecs;
  word_t   value;

  word_t x, y;

  struct dfs_hdr *hdr;

  for (y = 0; y < w->height>>3; y++) {
    for (x = 0; x < w->width>>3; x+= PACK) {
      if (DIRTY(w, x, y) & ((1<<PACK)-1)) { /* XXX PACK <= 8 */

	if (w->sent < NTXBUFS) {
	  rec[0].base = w->buf + w->sent*sizeof(*hdr);
	  w->sent++;
	} else {
	    IO$GetPkt (w->fb, 2, rec, FOREVER, &nrecs, &value);
	}

	rec[0].len  = sizeof(*hdr);
	hdr = rec[0].base;
	hdr->magic  = DFS_MAGIC;
	hdr->xtiles = PACK;
	hdr->ytiles = 1;
	hdr->x      = x;
	hdr->y      = y;

	rec[1].base = (tile_t *)w->pixels + (y * (w->width>>3)) + x;
	rec[1].len = PACK * (8*8);

	IO$PutPkt (w->fb, 2, rec, 0, FOREVER);
      }
    }
  }
  memset(w->diffs, 0, w->diffsize);
}

/*
 * IO AVA flush method 
 */
void WSFlush_IO_AVA(Window w)
{
  IO_Rec   rec[1];
  uint32_t nrecs;
  word_t   value;
  uint64_t x, y;

  for (y = 0; y < w->height>>3; y++) {
      for (x = 0; x < w->width>>3; x+= PACK) {
	  if (DIRTY(w, x, y) & ((1<<PACK)-1)) { /* XXX PACK <= 8 */
	      
	      
	      if (w->sent < NTXBUFS) {
		  rec[0].base = w->buf + w->sent*TXBUFSIZE;
		  w->sent++;
	      } else {
		  IO$GetPkt (w->fb, 1, rec, FOREVER, &nrecs, &value);
	      }
	      
	      rec[0].len  = TXPKTSIZE;
	      
	      /* XXX Yuk - fix this to use multiple recs */
	      memcpy(rec[0].base, 
		     (tile_t *)w->pixels + (y * (w->width>>3)) + x,
		     (PACK<<6));
	      
	      ((uchar_t *)rec[0].base)[rec[0].len-15] = y;
	      ((uchar_t *)rec[0].base)[rec[0].len-16] = x;
	      
	      /* Set up AAL5 len */
	      ((uchar_t *)rec[0].base)[rec[0].len-6] = TXPKTSIZE >> 8;
	      ((uchar_t *)rec[0].base)[rec[0].len-5] = TXPKTSIZE & 0xff;
	      
	      IO$PutPkt (w->fb, 1, rec, 0, FOREVER);
	  }
      }
  }

  memset(w->diffs, 0, w->diffsize);
}

#else /* TILED */
struct raster8_hdr {
  uint64_t x;
  uint64_t y;
  uint64_t xtiles;
  uint64_t ylines;
  uint64_t dataptr;
  uint64_t datastride;
};

struct raster8_hdr hdr;
IO_Rec rec;

static void WSFlush_IO_Raster_8(Window w)
{
  word_t value;
  uint32_t nrecs;
  IO_Rec rec2;

  hdr.x = 0;
  hdr.y = 0;
  hdr.xtiles = w->width>>3;
  hdr.ylines = w->height;
  hdr.datastride = w->width;
  hdr.dataptr = w->pixels;

  rec.base = &hdr;
  rec.len = sizeof(struct raster8_hdr);

  if (w->sent < NTXBUFS) {
      w->sent++;
  } else {
      IO$GetPkt (w->fb, 1, &rec2, FOREVER, &nrecs, &value);
  }
  
  IO$PutPkt (w->fb, 1, &rec, 0, FOREVER);
}


/*
 * Raster IO flush method 
 */
static void WSFlush_IO_Raster(Window w)
{
  IO_Rec rec;
  word_t value;
  uint32_t nrecs;
  int x, y, i;

  uint64_t bits = 0;
  uint64_t pdus = 0;
  Time_ns start = NOW();

  for (y = 0; y < w->height>>3; y++) {
    for (x = 0; x < w->width>>3; x+= PACK) {
      uchar_t  *pp = w->pixels + (y << 3) * w->stride + (x<<3);
      uint64_t *pkt;
      uint32_t nrecs;

      if (w->sent < NTXBUFS) {
	rec.base = w->buf + w->sent*TXBUFSIZE;
	w->sent++;
      } else {
	IO$GetPkt (w->fb, 1, &rec, FOREVER, &nrecs, &value);
      }

      pkt = (uint64_t *)rec.base;
      rec.len  = TXPKTSIZE;

      for (i = 0; i < (PACK<<3); i+=8) {
	pkt[i + 0] = *((uint64_t *)(pp + w->stride * 0));
	pkt[i + 1] = *((uint64_t *)(pp + w->stride * 1));
	pkt[i + 2] = *((uint64_t *)(pp + w->stride * 2));
	pkt[i + 3] = *((uint64_t *)(pp + w->stride * 3));
	pkt[i + 4] = *((uint64_t *)(pp + w->stride * 4));
	pkt[i + 5] = *((uint64_t *)(pp + w->stride * 5));
	pkt[i + 6] = *((uint64_t *)(pp + w->stride * 6));
	pkt[i + 7] = *((uint64_t *)(pp + w->stride * 7));
	pp+=8;
      }

      ((uchar_t *)rec.base)[rec.len-15] = y;
      ((uchar_t *)rec.base)[rec.len-16] = x;

      /* Set up AAL5 len */
      ((uchar_t *)rec.base)[rec.len-6] = TXPKTSIZE >> 8;
      ((uchar_t *)rec.base)[rec.len-5] = TXPKTSIZE & 0xff;

      IO$PutPkt (w->fb, 1, &rec, 0, FOREVER);
      bits += (TXPKTSIZE*8); pdus++;
      
   }
  }
}

#endif /* TILED */

void WSFlush(Window w) 
{
  w->flush_m(w);
}
