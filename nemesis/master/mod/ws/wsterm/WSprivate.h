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
**      lib/WS  (Nemesis client rendering window system).
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Private datastructures
** 
** ENVIRONMENT: 
**
**      Shared library.
** 
** ID : $Id: WSprivate.h 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#ifndef _WS_PRIVATE_H_
#define _WS_PRIVATE_H_

#include <FB.ih>

#define TILED   

#define UNIMPLEMENTED \
{ printf("%s: UNIMPLEMENTED\n", __FUNCTION__); return; }

typedef union {
  uint8_t  pixel[8][8];
  uint64_t  row[8];
} tile_t;

struct _Display {
  WS_clp   ws;			/* Connection to WS */
  IDCClientBinding_clp binding;
  IO_clp   evio;		/* Event channel */
  WSEvent  *ev;			/* Keep an event */
  char     *fb;			/* XXX Highly temporary! */
  uint32_t width;
  uint32_t height;
  uint32_t stride;
};

typedef void flush_fn(Window w);

struct _Window {
  Display      *display;	/* Display */
  WS_WindowID   id;		/* WS Window ID */
  FB_Protocol   proto;		/* Protocol for FB connection */
  FB_StreamID   sid;		/* FB Stream ID */
  IO_clp        fb;		/* Connection to FB */
  flush_fn      *flush_m;	/* Function to Flush updates to FB */
  FBBlit_clp    blit;
  Stretch_clp   bufstr;		/* Buffer stretch */
  addr_t        buf;		/* Address of buffers */
  uint32_t      sent;		/* Depth of pipe */
  uint32_t x;			/* x offset on fb */
  uint32_t y;			/* y offset on fb */

  Stretch_clp   pixstr;		/* Buffer stretch */
  char     *pixels;		/* Local pixmap if required */
  uint32_t width;
  uint32_t height;
  uint32_t stride;		/* XXX stride */
  uint64_t *diffs;		/* Local diffs */
  uint32_t diffstride;          /* stride of diffs array */
  uint32_t diffsize;            /* Size of diffs array */
  uint64_t fg;
  uint64_t bg;
    
    int32_t yrollat;
    int32_t yroll;   /* XXX y roll in pixels */
};

/* Mark tiles t starting at (x,y) as dirty */
#define MARKDIRTY(w, x, y, t)    			\
(w)->diffs[((y)>>3)*(w)->diffstride + ((x)>>3)] |= 	\
(uint64_t)(t) << ((((y)&7)<<3)  | ((x)&7))

/* Mark tiles t starting at (x,y) as clean */
#define MARKCLEAN(w, x, t) 				\
(w)->diffs[((y)>>3)*(w)->diffstride + ((x)>>3)] &= 	\
~((uint64_t)(t) << ((((y)&7) << 3) | ((x)&7))

/* Return the dirty mask entry for TILE (x,y) (and tiles to right) */
#define DIRTY(w, x, y)					\
((w)->diffs[((y)>>3)*(w)->diffstride + ((x)>>3)] >> ((((y)&7)<<3) | ((x)&7)))

#endif /* _WS_PRIVATE_H_ */
