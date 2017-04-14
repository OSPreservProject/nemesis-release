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
** ID : $Id: lines.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <WS.h>

#include "WSprivate.h"

#define TRC(x) 

#define sign(x) ((x > 0) ? 1 : (x < 0 ? -1 : 0))
#define abs(x)  ((x > 0) ? x : -x)

void WSDrawLine(Window   w, 
		int32_t  x1, 
		int32_t  x2,
		int32_t  y1,
		int32_t  y2)
{
    Segment s;
    
    s.x1 = x1; 
    s.x2 = x2;
    s.y1 = y1;
    s.y2 = y2;
    
    return WSDrawSegments(w, &s, 1);
}

void WSDrawLines(Window   w,
	    Point    *points,
	    int32_t  npoints,
	    int32_t  mode)
{
    UNIMPLEMENTED;
}

#ifdef TILED

void WSDrawSegments(Window    w,
		    Segment  *segments,
		    int32_t   nsegments)
{
  sword_t dx, dy;
  word_t adx, ady;
  sword_t signdx, signdy;
  word_t len;
  word_t du, dv;

  sword_t e, e1, e2;		/* Bresenham errors */

  tile_t *tp;			/* Pointer to tile */

  word_t tilex;			/* Current tile */
  word_t tiley;
  sword_t x, y;			/* current position within tile */

  TRC(kprintf("WSDrawSegments(%#lx, %#lx, %d)\n", 
	      w, segments, nsegments));

  while (nsegments--)  {
    Point pt1, pt2;

    pt1.x = segments->x1;
    pt1.y = segments->y1;
    pt2.x = segments->x2;
    pt2.y = segments->y2;

    TRC(kprintf("(%d,%d)->(%d,%d)\n", 
		pt1.x, pt1.y,
		pt2.x, pt2.y));

    dx = pt2.x - pt1.x;
    dy = pt2.y - pt1.y;

    adx = abs(dx);
    ady = abs(dy);

    signdx = sign(dx);
    signdy = sign(dy);
    
    if (adx > ady) {
      du = adx;
      dv = ady;
      len = adx;
    } else  {
      du = ady;
      dv = adx;
      len = ady;
    }

    e1 = dv * 2;
    e2 = e1 - 2*du;
    e = e1 - du;
    
    x = pt1.x; 
    y = pt1.y;

    tilex = x>>3;
    tiley = y>>3;
    MARKDIRTY(w, tilex, tiley, 0x01);

    tp = (tile_t *)w->pixels + ((w->stride>>3) * tiley) + tilex;
    
    if (adx > ady) {
      
      while (len--) {                   /* X axis */
	
        if (((signdx > 0) && (e < 0)) ||
            ((signdx <=0) && (e <=0))) {
            e += e1;
        } else {
          y += signdy;
          e += e2;
        }

        x += signdx;

        if ((x >= 0) && (x < w->width) &&
            (y >= 0) && (y < w->height)) {

	  if ((x>>3) != tilex || (y>>3) != tiley) { /* XXX Slow */
	    tilex = x>>3; tiley = y>>3;
	    tp = (tile_t *)w->pixels + ((w->stride>>3) * tiley) + tilex; 
	    MARKDIRTY(w, tilex, tiley, 0x01);
	  }
          tp->pixel[y&7][x&7] = w->fg;
        }
      }

    } else {

      while (len--) {                   /* Y axis */
	
        if (((signdx > 0) && (e < 0)) ||
            ((signdx <=0) && (e <=0))) {
          e += e1;
        } else  {
          x += signdx;
          e += e2;
	}
	
        y += signdy;
	
        if ((x >= 0) && (x < w->width) &&
            (y >= 0) && (y < w->height)) {
	  if ((x>>3) != tilex || (y>>3) != tiley) { /* XXX Slow */
	    tilex = x>>3; tiley = y>>3;
	    tp = (tile_t *)w->pixels + ((w->stride>>3) * tiley) + tilex;
	    MARKDIRTY(w, tilex, tiley, 0x01);
	  }
          tp->pixel[y&7][x&7] = w->fg;
        }
      }
      
    }
    segments++;
  }
}

#else

void WSDrawSegments(Window    w,
		    Segment  *segments,
		    int32_t   nsegments)
{
  sword_t dx, dy;
  word_t adx, ady;
  sword_t signdx, signdy;
  word_t len;
  word_t du, dv;

  sword_t e, e1, e2;		/* Bresenham errors */
  sword_t x, y;			/* current point on the line */

  uchar_t *pp;			/* pixel pointer */

  word_t tilex;
  word_t tiley;

  TRC(kprintf("WSDrawSegments(%#lx, %#lx, %d)\n", 
	      w, segments, nsegments));

  while (nsegments--)  {
    Point pt1, pt2;

    pt1.x = segments->x1;
    pt1.y = segments->y1;
    pt2.x = segments->x2;
    pt2.y = segments->y2;

    TRC(kprintf("(%d,%d)->(%d,%d)\n", 
		pt1.x, pt1.y,
		pt2.x, pt2.y));

    dx = pt2.x - pt1.x;
    dy = pt2.y - pt1.y;

    adx = abs(dx);
    ady = abs(dy);

    signdx = sign(dx);
    signdy = sign(dy);
    
    if (adx > ady) {
      du = adx;
      dv = ady;
      len = adx;
    } else  {
      du = ady;
      dv = adx;
      len = ady;
    }

    e1 = dv * 2;
    e2 = e1 - 2*du;
    e = e1 - du;
    
    x = pt1.x;
    y = pt1.y;
    tilex = x>>3;
    tiley = y>>3;

    pp = w->pixels + (w->stride * y) + x;
    
    if (adx > ady) {
      
      while (len--) {                   /* X AXIS */
	
        if (((signdx > 0) && (e < 0)) ||
            ((signdx <=0) && (e <=0))) {
            e += e1;
        } else {
          y += signdy;
          e += e2;
          pp += (signdy * (int32_t)w->stride);
        }

        x += signdx;
        pp += signdx;

        if ((x >= 0) && (x < w->width) &&
            (y >= 0) && (y < w->height)) {
          *pp = w->fg;
	  if ((x>>3) != tilex || (y>>3) != tiley) {
	    tilex = x>>3; tiley = y>>3;
	    MARKDIRTY(w, tilex, tiley, 0x01);
	  }
        }
      }

    } else {

      while (len--) {                   /* Y_AXIS */
	
        if (((signdx > 0) && (e < 0)) ||
            ((signdx <=0) && (e <=0))) {
          e +=e1;
        } else  {
          x += signdx;
          e += e2;
          pp += signdx;
	}
	
        y += signdy;
        pp += (signdy * (int32_t)w->stride);
	
        if ((x >= 0) && (x < w->width) &&
            (y >= 0) && (y < w->height)) {
          *pp = w->fg;
	  if ((x>>3) != tilex || (y>>3) != tiley) {
	    tilex = x>>3; tiley = y>>3;
	    MARKDIRTY(w, tilex, tiley, 0x01);
	  }
        }
      }
      
    }
    segments++;
  }
}

#endif
