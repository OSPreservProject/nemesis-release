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
**      String rendering functions.
** 
** ENVIRONMENT: 
**
**      Shared library
** 
** ID : $Id: rectangles.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <WS.h>

#include "WSprivate.h"

#define TRC(x) 

void WSExposeRectangle(Window    w, 
		  uint32_t   x, 
		  uint32_t   y,
		  uint32_t  width,
		  uint32_t  height)
{
  word_t tilex, tiley;
  int32_t xt, yt, xmax, ymax;

  TRC(printf("WSExposeRectangle(%#lx, %u, %u, %u, %u)\n", 
	     w, x, y, width, height));

  x = (MIN(w->width, x)) & ~7;
  y = (MIN(w->height, y)) & ~7;
  xmax = (MIN(w->width, x + width) + 7) & ~7;
  ymax = (MIN(w->height, y + height) + 7) & ~7;

  yt = MAX(0, y);
  tiley = yt>>3;
  
  for ( ; yt <= ymax; yt+=8) {
    xt = MAX(0, x);
    tilex = x>>3;
    for ( ; xt <= xmax; xt += 8) {
      MARKDIRTY(w, tilex, tiley, 0x01);
      tilex++;
    }
    tiley++;
  }
}

void WSDrawRectangle(Window    w, 
		     int32_t   x, 
		     int32_t   y,
		     uint32_t  width,
		     uint32_t  height)
{
  Rectangle r;
  
  TRC(printf("WSDrawRectangle(%#lx, %d, %d, %d, %d)\n", 
	     w, x, y, width, height));
  r.x = x; 
  r.y = y;
  r.width  = width;
  r.height = height;

  WSDrawRectangles(w, &r, 1);
}

void WSDrawRectangles(Window      w,
		      Rectangle  *rectangles,
		      int32_t     nrectangles)
{
  Segment segment[4];

  while (nrectangles--)  {
    segment[0].x1 = segment[1].x1 = rectangles->x;
    segment[0].x2 = segment[1].x2 = rectangles->x + rectangles->width;

    segment[0].y1 = segment[0].y2 = rectangles->y;
    segment[1].y1 = segment[1].y2 = rectangles->y + rectangles->height;

    segment[2].x1 = segment[2].x2 = rectangles->x;
    segment[3].x1 = segment[3].x2 = rectangles->x + rectangles->width;

    segment[2].y1 = segment[3].y1 = rectangles->y;
    segment[2].y2 = segment[3].y2 = rectangles->y + rectangles->height;

    WSDrawSegments(w, segment, 4);
    rectangles++;
  }
}

void WSFillRectangle(Window    w, 
		     int32_t   x, 
		     int32_t   y,
		     uint32_t  width,
		     uint32_t  height)
{
  Rectangle r;
  
  TRC(printf("WSFillRectangle(%#lx, %d, %d, %d, %d)\n", 
	     w, x, y, width, height));
  r.x = x; 
  r.y = y;
  r.width  = width;
  r.height = height;

  WSFillRectangles(w, &r, 1);
}

#ifdef TILED
void WSFillRectangles(Window      w,
		      Rectangle  *rectangles,
		      int32_t     nrectangles)
{
  word_t x, y;			/* current point in rectangle */

  tile_t *tp;			/* tile pointer */
  word_t tilex, tiley;		/* tile coords; */

  while (nrectangles--)  {
    y = MAX(0, rectangles->y);
    tiley = y>>3;

    if (y & 7) {
      for ( ; (y & 7) && y < rectangles->y + rectangles->height; y++) {
	
	x = MAX(0, rectangles->x);
	tilex = x>>3;
	
	tp = (tile_t *)w->pixels + ((w->stride>>3) * tiley) + tilex; 
	
	if (x&7) {
	  MARKDIRTY(w, tilex, tiley, 0x01);
	  for ( ; (x & 7) && x < rectangles->x + rectangles->width; x++) 
	    tp->pixel[y&7][x&7] = w->fg;
	  tp++; tilex++;
	}
	for ( ; (x + 7) < rectangles->x + rectangles->width; x += 8) {
	  tp->row[y&7] = w->fg; 
	  MARKDIRTY(w, tilex, tiley, 0x01);
	  tilex++; tp++;
	}
	for ( ; x < rectangles->x + rectangles->width; x++) 
	  tp->pixel[y&7][x&7] = w->fg;
	MARKDIRTY(w, tilex, tiley, 0x01);
      }
      tiley++;
    }
    for ( ; (y + 7) < rectangles->y + rectangles->height; y+=8) {

      x = MAX(0, rectangles->x);
      tilex = x>>3;

      tp = (tile_t *)w->pixels + ((w->stride>>3) * tiley) + tilex; 

      if (x&7) {
	for ( ; (x & 7) && x < rectangles->x + rectangles->width; x++) {
	  tp->pixel[0][x&7] = w->fg;
	  tp->pixel[1][x&7] = w->fg;
	  tp->pixel[2][x&7] = w->fg;
	  tp->pixel[3][x&7] = w->fg;
	  tp->pixel[4][x&7] = w->fg;
	  tp->pixel[5][x&7] = w->fg;
	  tp->pixel[6][x&7] = w->fg;
	  tp->pixel[7][x&7] = w->fg;
	}
	MARKDIRTY(w, tilex, tiley, 0x01);
	tp++; tilex++;
      }
      for ( ; (x + 7) < rectangles->x + rectangles->width; x += 8) {
	tp->row[0] = w->fg;
	tp->row[1] = w->fg;
	tp->row[2] = w->fg;
	tp->row[3] = w->fg;
	tp->row[4] = w->fg;
	tp->row[5] = w->fg;
	tp->row[6] = w->fg;
	tp->row[7] = w->fg;
	MARKDIRTY(w, tilex, tiley, 0x01);
	tp++; tilex++;
      }
      for ( ; x < rectangles->x + rectangles->width; x++) {
	tp->pixel[0][x&7] = w->fg;
	tp->pixel[1][x&7] = w->fg;
	tp->pixel[2][x&7] = w->fg;
	tp->pixel[3][x&7] = w->fg;
	tp->pixel[4][x&7] = w->fg;
	tp->pixel[5][x&7] = w->fg;
	tp->pixel[6][x&7] = w->fg;
	tp->pixel[7][x&7] = w->fg;
      }
      MARKDIRTY(w, tilex, tiley, 0x01);

      tiley++;
    }
    for ( ; y < rectangles->y + rectangles->height; y++) {

      x = MAX(0, rectangles->x);
      tilex = x>>3;

      tp = (tile_t *)w->pixels + ((w->stride>>3) * tiley) + tilex; 

      if (x&7) {
	for ( ; (x & 7) && x < rectangles->x + rectangles->width; x++) 
	  tp->pixel[y&7][x&7] = w->fg;
	MARKDIRTY(w, tilex, tiley, 0x01);
	tp++; tilex++;
      }
      for ( ; (x + 7) < rectangles->x + rectangles->width; x += 8) {
	tp->row[y&7] = w->fg; 
	MARKDIRTY(w, tilex, tiley, 0x01);
	tilex++; tp++;
      }
      for ( ; x < rectangles->x + rectangles->width; x++) 
	tp->pixel[y&7][x&7] = w->fg;
      MARKDIRTY(w, tilex, tiley, 0x01);
    }

    rectangles++;
  }
}

#else /* TILED */

void WSFillRectangles(Window      w,
		      Rectangle  *rectangles,
		      int32_t     nrectangles)
{
  register int32_t x, y;	/* current point on the line */

  register uchar_t *pp;		/* pixel pointer */

  while (nrectangles--)  {
    y = MAX(0, rectangles->y);

    for ( ; y < rectangles->y + rectangles->height; y++) {

      x = MAX(0, rectangles->x);

      pp = w->pixels + (w->stride * y) + x;

      for ( ; (x & 3) && x < rectangles->x + rectangles->width; x++) {
	*pp++ = w->fg;
	MARKDIRTY(w, x>>3, y>>3, 0x01);
      }
      for ( ; (x + 3) < rectangles->x + rectangles->width; x += 4) {
	*(uint32_t *)pp = w->fg;
	MARKDIRTY(w, x>>3, y>>3, 0x01);
	pp += 4;
      }
      for ( ; x < rectangles->x + rectangles->width; x++) 
	*pp++ = w->fg;
	MARKDIRTY(w, x>>3, y>>3, 0x01);
    }
    rectangles++;
  }
}

#endif /* TILED */
