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
** ID : $Id: strings.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <WS.h>

#include "WSprivate.h"
#include "font.h"

#ifdef TILED

/* XXX TILE alignment for now! */
bool_t WSDrawString(Window		w,
		  int32_t		x,
		  int32_t		y,
		  string_t	        s)
{
  uchar_t	*font;
  tile_t	*tp;
  uchar_t        ch;

  word_t tilex;
  word_t tiley;

  int j;

#if 0  
  static const uint32_t spread[16] = {
      0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
      0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
      0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
      0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF,
  };
#else
  static const uint32_t spread[16] = {
      0x00000000, 0xFF000000, 0x00FF0000, 0xFFFF0000, 
      0x0000FF00, 0xFF00FF00, 0x00FFFF00, 0xFFFFFF00, 
      0x000000FF, 0xFF0000FF, 0x00FF00FF, 0xFFFF00FF, 
      0x0000FFFF, 0xFF00FFFF, 0x00FFFFFF, 0xFFFFFFFF,
  };
#endif
  
  tilex = x>>3; 

  while ((ch = *s)) {

      tiley = y>>3;
      
      if (x >= 0 && ((x+FONT_WIDTH) <= w->width) &&
	  y >= 0 && ((y+FONT_HEIGHT) <= w->height)) {
	  
	  tp = (tile_t *)w->pixels + tiley * (w->stride>>3) + tilex;
	  font = &FONT[ (int)ch * FONT_HEIGHT];
	  
	  MARKDIRTY(w, tilex, tiley, 0x01);
	  
	  for (j = 0; j < FONT_HEIGHT; j++) {
	      uchar_t	row = *font++;	
	      uint32_t mask;
	      uint64_t pixels;
	      
	      mask = spread[ (row>>4) & 0xf ];
	      pixels = (w->fg & mask) | (w->bg & ~mask);
	      mask = spread[ row & 0xf ];
	      pixels |= ((w->fg & mask) | (w->bg & ~mask))<<32;
	      
	      tp->row[j&7] = pixels;
	      
	      if ((j&7) == 7) {	/* Next tile down? */
		  tp += w->stride>>3;
		  tiley++;
		  MARKDIRTY(w, tilex, tiley, 0x01);
	      }
	  }
	  s++; tilex++;
	  x+= FONT_WIDTH;
      } else {
	  return False;
      }
  }
  return True;
}

#else /* TILED */

bool_t WSDrawString(Window		w,
		    int32_t		x,
		    int32_t		y,
		    string_t	        s)
{
    uchar_t	*font;
    uchar_t	*pp;
    uchar_t      ch;
    
    int j;
    
    
#if 0  
    static const uint32_t spread[16] = {
	0x00000000, 0x000000FF, 0x0000FF00, 0x0000FFFF,
	0x00FF0000, 0x00FF00FF, 0x00FFFF00, 0x00FFFFFF,
	0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,
	0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF,
    };
#else
    static const uint32_t spread[16] = {
	0x00000000, 0xFF000000, 0x00FF0000, 0xFFFF0000, 
	0x0000FF00, 0xFF00FF00, 0x00FFFF00, 0xFFFFFF00, 
	0x000000FF, 0xFF0000FF, 0x00FF00FF, 0xFFFF00FF, 
	0x0000FFFF, 0xFF00FFFF, 0x00FFFFFF, 0xFFFFFFFF,
    };
#endif
  
    while (ch = *s) {
	
	if (x >= 0 && ((x+FONT_WIDTH) < w->width) &&
	    y >= 0 && ((y+FONT_HEIGHT) < w->height)) {
	    
	    pp = w->pixels + (y * w->stride) + (x & (~3));
	    font = &FONT[ (int)ch * FONT_HEIGHT];
	    
	    for (j = 0; j < FONT_HEIGHT; j++) {
		uchar_t	row = *font++;
		uint32_t mask;
		
		mask = spread[ (row>>4) & 0xf ];
		((uint32_t*)pp)[0] = (w->fg & mask) | (w->bg & ~mask);
		mask = spread[ row & 0xf ];
		((uint32_t*)pp)[1] = (w->fg & mask) | (w->bg & ~mask);
		pp += w->stride;
		MARKDIRTY(w, x>>3, (y+j)>>3, 0x03);
	    }
	    s++;
	    x+= FONT_WIDTH;
	} else {
	    return False;
	}
    }
    return True;
}

#endif /* TILED */

