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
** ID : $Id: points.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <WS.h>

#include "WSprivate.h"

void WSDrawPoint(Window   w, 
	    int32_t  x, 
	    int32_t  y)
{
  Point p;
  
  p.x = x; 
  p.y = y;

  WSDrawPoints(w, &p, 1, 0);
}

void WSDrawPoints(Window   w,
	     Point    *points,
	     int32_t  npoints,
	     int32_t  mode)
{

  while (npoints--)  {
    if ((points->x >= 0) && (points->x < w->width) &&
	(points->y >= 0) && (points->y < w->height)) {
      register uchar_t *pp;		/* pixel pointer */

      pp = w->pixels + (w->stride * points->y) + points->x;
      
      *pp = w->fg;
    }
    points++;
  }
}



