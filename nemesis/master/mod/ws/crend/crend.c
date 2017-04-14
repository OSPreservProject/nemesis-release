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
** ID : $Id: crend.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
**
**
*/



#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <salloc.h>

#include <CRend.ih>
#include <stdio.h>
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#include "CRend_st.h"


#define GREEN 0x3e0
#define RED 0x7c00
#define BLUE 0x1f
#define REDSHIFT 10
#define BLUESHIFT 0
#define GREENSHIFT 5


#define swap(x, y) { int temp = x; x = y; y = temp; }


/*
 * Module stuff
 */
static	CRend_SetBackground_fn 		CRend_SetBackground_m;
static	CRend_SetForeground_fn 		CRend_SetForeground_m;
static  CRend_SetPlotMode_fn            CRend_SetPlotMode_m;
static	CRend_DrawPoint_fn 		CRend_DrawPoint_m;
static	CRend_DrawPoints_fn 		CRend_DrawPoints_m;
static	CRend_DrawLine_fn 		CRend_DrawLine_m;
static	CRend_DrawLines_fn 		CRend_DrawLines_m;
static	CRend_DrawRectangle_fn 		CRend_DrawRectangle_m;
static	CRend_DrawRectangles_fn 	CRend_DrawRectangles_m;
static	CRend_FillRectangle_fn 		CRend_FillRectangle_m;
static	CRend_FillRectangles_fn 	CRend_FillRectangles_m;
static  CRend_DrawArc_fn                CRend_DrawArc_m;
static  CRend_DrawArcs_fn               CRend_DrawArcs_m;
static	CRend_DrawSegments_fn 		CRend_DrawSegments_m;
static	CRend_DrawString_fn 		CRend_DrawString_m;
static  CRend_ExposeRectangle_fn        CRend_ExposeRectangle_m;
static	CRend_Flush_fn 		CRend_Flush_m;
static	CRend_Close_fn 		CRend_Close_m;
static  CRend_Map_fn            CRend_Map_m;
static  CRend_UnMap_fn          CRend_UnMap_m;
static  CRend_ResizeWindow_fn   CRend_ResizeWindow_m;
static  CRend_MoveWindow_fn     CRend_MoveWindow_m;
static  CRend_GetID_fn          CRend_GetID_m;
static  CRend_ScrollRectangle_fn   CRend_ScrollRectangle_m;
static  CRend_GetPixelData_fn           CRend_GetPixelData_m;
static  CRend_GetWidth_fn CRend_GetWidth_m;
static  CRend_GetHeight_fn CRend_GetHeight_m;

static  CRend_DrawPixmap_fn     CRend_DrawPixmap_m;

CRend_op CRend_ms = {
  CRend_SetBackground_m,
  CRend_SetForeground_m,
  CRend_SetPlotMode_m,
  CRend_DrawPoint_m,
  CRend_DrawPoints_m,
  CRend_DrawLine_m,
  CRend_DrawLines_m,
  CRend_DrawRectangle_m,
  CRend_DrawRectangles_m,
  CRend_FillRectangle_m,
  CRend_FillRectangles_m,
  CRend_DrawArc_m,
  CRend_DrawArcs_m,
  CRend_DrawSegments_m,
  CRend_DrawString_m,
  CRend_DrawPixmap_m,
  CRend_GetPixelData_m,
  CRend_GetWidth_m,
  CRend_GetHeight_m,
  CRend_Flush_m,
  CRend_ExposeRectangle_m,
  CRend_Close_m,
  CRend_Map_m,
  CRend_UnMap_m,
  CRend_ResizeWindow_m,
  CRend_MoveWindow_m,
  CRend_ScrollRectangle_m,
  CRend_GetID_m
};

#define UNIMPLEMENTED \
printf("%s: UNIMPLEMENTED\n", __FUNCTION__);


INLINE int sign(x) {
    if (!x) return 0;
    return x>0 ? 1:-1;
}

/*
 * sin_table[x] gives sin of x degrees in fixpoint with 16-bit fraction.
 * eg 65536 corresponds to 1. 
 */

static const int32_t sin_table[360] = {
    0,
    1143,
    2287,
    3429,
    4571,
    5711,
    6850,
    7986,
    9120,
    10252,
    11380,
    12504,
    13625,
    14742,
    15854,
    16961,
    18064,
    19160,
    20251,
    21336,
    22414,
    23486,
    24550,
    25606,
    26655,
    27696,
    28729,
    29752,
    30767,
    31772,
    32767,
    33753,
    34728,
    35693,
    36647,
    37589,
    38521,
    39440,
    40347,
    41243,
    42125,
    42995,
    43852,
    44695,
    45525,
    46340,
    47142,
    47929,
    48702,
    49460,
    50203,
    50931,
    51643,
    52339,
    53019,
    53683,
    54331,
    54963,
    55577,
    56175,
    56755,
    57319,
    57864,
    58393,
    58903,
    59395,
    59870,
    60326,
    60763,
    61183,
    61583,
    61965,
    62328,
    62672,
    62997,
    63302,
    63589,
    63856,
    64103,
    64331,
    64540,
    64729,
    64898,
    65047,
    65176,
    65286,
    65376,
    65446,
    65496,
    65526,
    65536,
    65526,
    65496,
    65446,
    65376,
    65286,
    65176,
    65047,
    64898,
    64729,
    64540,
    64331,
    64103,
    63856,
    63589,
    63302,
    62997,
    62672,
    62328,
    61965,
    61583,
    61183,
    60763,
    60326,
    59870,
    59395,
    58903,
    58393,
    57864,
    57319,
    56755,
    56175,
    55577,
    54963,
    54331,
    53683,
    53019,
    52339,
    51643,
    50931,
    50203,
    49460,
    48702,
    47929,
    47142,
    46340,
    45525,
    44695,
    43852,
    42995,
    42125,
    41243,
    40347,
    39440,
    38521,
    37589,
    36647,
    35693,
    34728,
    33753,
    32767,
    31772,
    30767,
    29752,
    28729,
    27696,
    26655,
    25606,
    24550,
    23486,
    22414,
    21336,
    20251,
    19160,
    18064,
    16961,
    15854,
    14742,
    13625,
    12504,
    11380,
    10252,
    9120,
    7986,
    6850,
    5711,
    4571,
    3429,
    2287,
    1143,
    0,
    -1143,
    -2287,
    -3429,
    -4571,
    -5711,
    -6850,
    -7986,
    -9120,
    -10252,
    -11380,
    -12504,
    -13625,
    -14742,
    -15854,
    -16961,
    -18064,
    -19160,
    -20251,
    -21336,
    -22414,
    -23486,
    -24550,
    -25606,
    -26655,
    -27696,
    -28729,
    -29752,
    -30767,
    -31772,
    -32768,
    -33753,
    -34728,
    -35693,
    -36647,
    -37589,
    -38521,
    -39440,
    -40347,
    -41243,
    -42125,
    -42995,
    -43852,
    -44695,
    -45525,
    -46340,
    -47142,
    -47929,
    -48702,
    -49460,
    -50203,
    -50931,
    -51643,
    -52339,
    -53019,
    -53683,
    -54331,
    -54963,
    -55577,
    -56175,
    -56755,
    -57319,
    -57864,
    -58393,
    -58903,
    -59395,
    -59870,
    -60326,
    -60763,
    -61183,
    -61583,
    -61965,
    -62328,
    -62672,
    -62997,
    -63302,
    -63589,
    -63856,
    -64103,
    -64331,
    -64540,
    -64729,
    -64898,
    -65047,
    -65176,
    -65286,
    -65376,
    -65446,
    -65496,
    -65526,
    -65536,
    -65526,
    -65496,
    -65446,
    -65376,
    -65286,
    -65176,
    -65047,
    -64898,
    -64729,
    -64540,
    -64331,
    -64103,
    -63856,
    -63589,
    -63302,
    -62997,
    -62672,
    -62328,
    -61965,
    -61583,
    -61183,
    -60763,
    -60326,
    -59870,
    -59395,
    -58903,
    -58393,
    -57864,
    -57319,
    -56755,
    -56175,
    -55577,
    -54963,
    -54331,
    -53683,
    -53019,
    -52339,
    -51643,
    -50931,
    -50203,
    -49460,
    -48702,
    -47929,
    -47142,
    -46340,
    -45525,
    -44695,
    -43852,
    -42995,
    -42125,
    -41243,
    -40347,
    -39440,
    -38521,
    -37589,
    -36647,
    -35693,
    -34728,
    -33753,
    -32768,
    -31772,
    -30767,
    -29752,
    -28729,
    -27696,
    -26655,
    -25606,
    -24550,
    -23486,
    -22414,
    -21336,
    -20251,
    -19160,
    -18064,
    -16961,
    -15854,
    -14742,
    -13625,
    -12504,
    -11380,
    -10252,
    -9120,
    -7986,
    -6850,
    -5711,
    -4571,
    -3429,
    -2287,
    -1143,
};

/*---------------------------------------------------- Entry Points ----*/

const word_t threshold5to3[] = {
    1,
    4,
    8,
    14,
    20,
    25,
    30,
    32
};

const word_t threshold5to2[] = {
    3,
    16,
    24,
    32
};

pixel_t ConvColour(uint32_t colour) {
    
    word_t volatile col = colour;
    word_t old_red = (col >> 10) & 31;
    word_t old_green = (col >> 5) & 31;
    word_t old_blue = col & 31;

    word_t new_red, new_green, new_blue, new_col;

    new_red = 0; 
    while(old_red >= threshold5to3[new_red]) {
	new_red++;
    }

    new_green = 0; 
    while(old_green >= threshold5to3[new_green]) {
	new_green++;
    }

    new_blue = 0; 
    while(old_blue >= threshold5to2[new_blue]) {
	new_blue++;
    }


    new_col = (new_red << 5) | (new_green << 2) | new_blue;
    return new_col;
}
	


static void 
CRend_SetBackground_m (
	CRend_cl	*self,
	uint32_t	pixel	/* IN */ )
{
  CRend_st	*st = (CRend_st *) self->st;
  
  if(sizeof(pixel_t) == 1) {
      /* If we're using 8 bit colour, map to 3:3:2 XXX */
      
      st->bg = ConvColour(pixel);

  } else {
      st->bg = (pixel_t) pixel;
  }

  /* extend the pixel to a full word (may harmlessly overflow ) */
  st->wordbg = st->bg;
  st->wordbg |= (st->wordbg << sizeof(pixel_t) * 8);
  st->wordbg |= (st->wordbg << sizeof(pixel_t) * 16);
  st->wordbg |= (st->wordbg << sizeof(pixel_t) * 32);
}





static void 
CRend_SetForeground_m (
	CRend_cl	*self,
	uint32_t	pixel	/* IN */ )
{
  CRend_st	*st = (CRend_st *) self->st;

  if(sizeof(pixel_t) == 1) {
      /* If we're using 8 bit colour, map to 3:3:2 XXX */

      st->fg = ConvColour(pixel);

  } else {
      st->fg = (pixel_t) pixel;
  }

  /* extend the pixel to a full word (may harmlessly overflow ) */
  st->wordfg = st->fg;
  st->wordfg |= (st->wordfg << (sizeof(pixel_t) * 8));
  st->wordfg |= (st->wordfg << (sizeof(pixel_t) * 16));
  st->wordfg |= (st->wordfg << (sizeof(pixel_t) * 32));

}


static void
CRend_SetPlotMode_m (
	CRend_cl	*self,
	CRend_PlotMode	 mode	/* IN */ )
{
  CRend_st	*st = (CRend_st *) self->st;

  st -> plot_mode = mode;
}


static void
CRend_DrawPoint_m (
    CRend_clp          self,
    int32_t            x,
    int32_t            y) {
    
    CRend_Point point;

    point.x = x;
    point.y = y;

    CRend_DrawPoints_m(self, &point, 1, CRend_CoordMode_Origin);
}

static void 
CRend_DrawPoints_m (
	CRend_cl	*self,
	CRend_Point     *points,
	uint32_t         npoints,
	CRend_CoordMode  mode) {
    
    CRend_st	*st = (CRend_st *) self->st;
    CRend_PlotMode plot_mode = st -> plot_mode;
    
    int32_t x = 0, y = 0;
    pixel_t fg = st->fg;
    
    while(npoints --) {
	if(mode == CRend_CoordMode_Origin) {
	    x = points->x;
	    y = points->y;
	} else {
	    x += points->x;
	    y += points->y;
	}
	
	points++;
	
	{
	    
	    pixel_t *const pixel = st->pixels + y*st->stride + x;
	    
	    /* Most tests are expected to succeed, so use bitwise tests
	       for speed */
	    
	    if ((x>=0) & (y>=0) & (x<st->width) & (y<st->height)) {
		
		
		TRC(printf("(%d,%d):  %x -> %x\n)", 
			   x, y, *pixel, (uint32_t)(pixel_t)fg));
		
		if (plot_mode == CRend_PlotMode_Normal)
		{
		    *pixel = fg;
		}
		else
		{
		    *pixel ^= fg;
		}
		
		MARK_DIRTY(x, y);
	    }
	}
	
	
    }
}



#if defined(__ALPHA__)
static INLINE int32_t muldiv64(int32_t m1, int32_t m2, int32_t d)
{
    return (uint32_t) ((int64_t) m1 *(int64_t) m2 / (int64_t) d);
}

#endif

#if defined(__ARM__)
static INLINE int32_t muldiv64(int32_t m1, int32_t m2, int32_t d)
{
    return (uint32_t) ((int64_t) m1 *(int64_t) m2 / (int64_t) d);
}
#endif
#if defined(__IX86__)
/* We use the 32-bit to 64-bit multiply and 64-bit to 32-bit divide of the */
/* 386 (which gcc doesn't know well enough) to efficiently perform integer */
/* scaling without having to worry about overflows. */
static INLINE int32_t muldiv64(int32_t m1, int32_t m2, int32_t d)
{
/* int32 * int32 -> int64 / int32 -> int32 */
    int result;
    __asm__(
	       "imull %%edx\n\t"
	       "idivl %3\n\t"
  :	       "=a"(result)	/* out */
  :	       "a"(m1), "d"(m2), "g"(d)		/* in */
  :	       "ax", "dx"	/* mod */
	);
    return result;
}

#endif				/* !__alpha__ */

#if defined(__ALPHA__)
static INLINE int32_t regioncode(int32_t x, int32_t y, 
				 int32_t maxx, int32_t maxy)
{
    int32_t result = 0;
    if (x < 0)
	result |= 1;
    else if (x > maxx)
	result |= 2;
    if (y < 0)
	result |= 4;
    else if (y > maxy)
	result |= 8;
    return result;
}
#endif

#if defined(__ARM__)
static INLINE int32_t regioncode(int32_t x, int32_t y, 
				 int32_t maxx, int32_t maxy)
{
    int32_t result = 0;
    if (x < 0)
	result |= 1;
    else if (x > maxx)
	result |= 2;
    if (y < 0)
	result |= 4;
    else if (y > maxy)
	result |= 8;
    return result;
}
#endif

#if defined(__IX86__)
#define INC_IF_NEG(y, result)				\
{							\
	__asm__("btl $31,%1\n\t"			\
		"adcl $0,%0"				\
		: "=r" ((int) result)			\
		: "rm" ((int) (y)), "0" ((int) result)	\
		);					\
}

static INLINE int regioncode(int x, int y, int maxx, int maxy)
{
    int dx1, dx2, dy1, dy2;
    int result;
    result = 0;
    dy2 = maxy - y;
    INC_IF_NEG(dy2, result);
    result <<= 1;
    dy1 = y;
    INC_IF_NEG(dy1, result);
    result <<= 1;
    dx2 = maxx - x;
    INC_IF_NEG(dx2, result);
    result <<= 1;
    dx1 = x;
    INC_IF_NEG(dx1, result);
    return result;
}

#endif				/* __alpha__ */

static void 
CRend_DrawLine_m (
	CRend_cl	*self,
	int32_t	x1	/* IN */,
	int32_t	x2	/* IN */,
	int32_t	y1	/* IN */,
	int32_t	y2	/* IN */ )
{
#define LEFT 1
#define RIGHT 2
#define BOTTOM 4
#define TOP 8
    CRend_st	*st = (CRend_st *) self->st;
    CRend_PlotMode plot_mode = st -> plot_mode;
    int dx, dy, ax, ay, sx, sy, x, y;
    int yinc; 
    pixel_t *pp;

    /* Cohen & Sutherland algorithm */
    for (;;) {
	int r1 = regioncode(x1, y1, st->width-1, st->height-1);
	int r2 = regioncode(x2, y2, st->width-1, st->height-1);
	if (!(r1 | r2))
	    break;		/* completely inside */
	if (r1 & r2)
	    return;		/* completely outside */
	if (r1 == 0) {
	    swap(x1, x2);	/* make sure first */
	    swap(y1, y2);	/* point is outside */
	    r1 = r2;
	}
	if (r1 & 1) {	/* left */
	    y1 += muldiv64(-x1, y2 - y1, x2 - x1);
	    x1 = 0;
	} else if (r1 & 2) {	/* right */
	    y1 += muldiv64(st->width -1 - x1, y2 - y1, x2 - x1);
	    x1 = st->width-1;
	} else if (r1 & 4) {	/* top */
	    x1 += muldiv64(-y1, x2 - x1, y2 - y1);
	    y1 = 0;
	} else if (r1 & 8) {	/* bottom */
	    x1 += muldiv64(st->height -1 - y1, x2 - x1, y2 - y1);
	    y1 = st->height-1;
	}
    }

    dx = x2 - x1;
    dy = y2 - y1;
    ax = abs(dx) << 1;
    ay = abs(dy) << 1;
    sx = (dx >= 0) ? 1 : -1;
    sy = (dy >= 0) ? 1 : -1;
    x = x1;
    y = y1;

    pp = st->pixels + (st->stride * y) + x;
    yinc = sy * ((int) st->stride);

    if (ax > ay) {
	int d = ay - (ax >> 1);
	while (1) {

	    MARK_DIRTY(x,y);
	    if (plot_mode == CRend_PlotMode_Normal)
	    {
		*pp = st->fg;
	    }
	    else
	    {
		*pp ^= st->fg;
	    }

	    if (d > 0 || (d == 0 && sx == 1)) {
		y += sy;
		d -= ax;
		pp+= yinc;
	    }
	    if (x == x2)
	    {
		break;
	    }
	    x += sx;
	    pp+= sx;
	    d += ay;
	}
    } else {
	int sy = (dy >= 0) ? 1 : -1;
	int d = ax - (ay >> 1);
	while (1) {

	    MARK_DIRTY(x,y);
	    if (plot_mode == CRend_PlotMode_Normal)
	    {
		*pp = st->fg;
	    }
	    else
	    {
		*pp ^= st->fg;
	    }

	    if (d > 0 || (d == 0 && sy == 1)) {
		x += sx;
		pp+= sx;
		d -= ay;
	    }
	    if (y == y2)
	    {
		break;
	    }

	    y += sy;
	    pp+= yinc;
	    d += ax;
	}
    }
}



static void 
CRend_DrawLines_m (
	CRend_cl	*self,
	CRend_Point_ptr	points	/* IN */,
	uint32_t	npoints	/* IN */,
	CRend_CoordMode	mode	/* IN */ )
{
    
    while(npoints--) {
	CRend_DrawLine_m(self, 
			 points[0].x, points[1].x, 
			 points[0].y, points[1].y);
	
	points++;
    }
}

static void 
CRend_DrawRectangle_m (
    CRend_cl	*self,
    int32_t	x	/* IN */,
    int32_t	y	/* IN */,
    int32_t	width	/* IN */,
    int32_t	height	/* IN */ )
{
    CRend_DrawLine_m(self, x, x + width - 1, y, y);
    CRend_DrawLine_m(self, x+width -1, x + width - 1, y, y + height - 1);
    CRend_DrawLine_m(self, x, x + width - 1, y + height - 1, y + height - 1);
    CRend_DrawLine_m(self, x, x, y, y + height - 1);
}

static void 
CRend_DrawRectangles_m (
    CRend_cl	*self,
    CRend_Rectangle_ptr	rectangles	/* IN */,
    uint32_t	nrectangles	/* IN */ )
{
    while(nrectangles--) {
	CRend$DrawRectangle(self, rectangles->x, rectangles->y, 
			    rectangles->width, rectangles->height);
	rectangles++;
    }
    
}

static void 
CRend_FillRectangle_m (
	CRend_cl	*self,
	int32_t	x	/* IN */,
	int32_t	y	/* IN */,
	int32_t	width	/* IN */,
	int32_t	height	/* IN */ )
{
    CRend_st	*st = (CRend_st *) self->st;
    CRend_PlotMode plot_mode = st -> plot_mode;
    word_t togo;
    uint32_t bottom, right;
    int yp;
    pixel_t *pp;
    word_t *ppword;
    word_t pixel_skip;
    pixel_t fg = st->fg;
    TRC(fprintf(stderr,"WSFillRectangle(%#lx, %d, %d, %d)\n", 
		x, y, width, height));
    
    right = MIN(x + width, st->width);
    bottom = MIN(y + height, st->height);
    
    x = MAX(x, 0);
    y = MAX(y, 0);
    
    width = right - x;
    height = bottom - y;
    
    if (width <=0) return;
    if (height <=0) return;
    
    pixel_skip = st->stride - width;
    pp = st->pixels + x + (st->stride * y);
    
    for (yp= 0; yp<height; yp++) {
	togo = width;
	
	MARK_DIRTY_RANGE(x, x+width, y+yp);
	
	while(togo && (((word_t)pp) & (sizeof(word_t) - 1))) {
	    *pp++ = fg;
	    togo--;
	}

	ppword = (word_t *)pp;
	
	while (togo >= (sizeof(word_t) / sizeof(pixel_t) )) {
	    /* draw a word at a time */
	    *ppword++ = st->wordfg;
	    togo -= sizeof(word_t) / sizeof(pixel_t);
	}
	
	pp = (pixel_t *)ppword;    
	if (plot_mode == CRend_PlotMode_Normal)
	{
	    while(togo--) {
		*pp++ = fg;
	    }
	}
	else
	{
	    while(togo--) {
		*pp++ ^= fg;
	    }
	}

	pp += pixel_skip;
	
    }
    
}

static void 
CRend_FillRectangles_m (
    CRend_cl	*self,
    CRend_Rectangle_ptr	rectangles	/* IN */,
    uint32_t	nrectangles	/* IN */ )
{
    while(nrectangles--) {
	CRend$FillRectangle(self, rectangles->x, rectangles->y, 
			    rectangles->width, rectangles->height);
	rectangles++;
    }
}


static void
CRend_DrawArc_m (
    CRend_cl     *self,
    int32_t       x           /* IN */,
    int32_t       y           /* IN */,
    uint32_t      width       /* IN */,
    uint32_t      height      /* IN */,
    uint32_t      startAngle  /* IN */,
    uint32_t      extentAngle /* IN */)
{
    uint32_t  angle;
    int32_t   cur_x;
    int32_t   cur_y;
    int32_t   x_rad;
    int32_t   y_rad;

    x_rad = width / 2;
    y_rad = height / 2;

    startAngle = startAngle / 64;
    extentAngle = extentAngle / 64;

    cur_x = x + ((x_rad * sin_table[(startAngle + 90) % 360]) / 65536);
    cur_y = y - ((y_rad * sin_table[startAngle % 360]) / 65536);

    for (angle = startAngle; angle <= startAngle + extentAngle; angle ++)
    {
	int32_t new_x = x + ((x_rad * sin_table[(angle + 90) % 360]) / 65536);
	int32_t new_y = y - ((y_rad * sin_table[angle % 360]) / 65536);

	CRend_DrawLine_m (self, cur_x, new_x, cur_y, new_y);

	cur_x = new_x;
	cur_y = new_y;
    }
}


static void
CRend_DrawArcs_m (
    CRend_cl      *self,
    CRend_Arc_ptr  arcs       /* IN */,
    uint32_t       narcs      /* IN */)
{
    while (narcs--)
    {
	CRend_DrawArc_m (self,
			 arcs -> x,
			 arcs -> y,
			 arcs -> width,
			 arcs -> height,
			 arcs -> startAngle,
			 arcs -> extentAngle);
	arcs ++;
    }
}


static void 
CRend_DrawSegments_m (
    CRend_cl	*self,
    CRend_Segment_ptr	segments	/* IN */,
    uint32_t	nsegments	/* IN */ )
{
    
    while(nsegments--) {
	CRend_DrawLine_m(self, segments->x1, segments->x2, 
			 segments->y1, segments->y2);
	segments++;
    }
}

static INLINE int PrintChar(char ch, pixel_t *pixbase, int stride, 
			    uint32_t *colour, int *height_out, int transparent,
			    CRend_PlotMode plot_mode) {

	pixel_t *base, *scrptr;
	const uchar_t *font, *fontx;
	int i,j;
	int width, height;	
	int k1,c;
	extern const char font_data[];
	extern const int font_index[];
	const int masks[3] = {RED, BLUE, GREEN};
	int fg[3];
			
	width = 0;
	base = pixbase;

	if (transparent)
	{
	    for (c = 0; c < 3; c ++)
	    {
		fg[c] = ((colour[31]) & masks[c]);
	    }
	}

	if (ch >= ' ') {
	    width = font_index [ 3*(ch - ' ')];
	    height = font_index [ 1 + (3*(ch- ' '))];
	    
	    font = font_data + font_index [ 2+ (3*(ch - ' '))];
	    for (i=0; i<height; i++) {
		scrptr = base;
		fontx = font;
		if (transparent) {
		    for (j=0; j<width; j++) {
			pixel_t col;
			
			k1 = (*fontx++);

			/*
			 * In transparent mode we merge (*scrptr) and colour[31]
			 * according to k1.
			 */

			col = 0;
			for (c = 0; c < 3; c ++)
			{
			    pixel_t bg = ((*scrptr) & masks[c]);

			    col += (((32 - k1) * bg) / 32) & masks[c];
			    col += ((k1 * fg[c]) / 32) & masks[c];
			}

			if (plot_mode == CRend_PlotMode_Normal)
			{
			    *scrptr = col;
			}
			else
			{
			    *scrptr ^= col;
			}

			scrptr++;
		    }
		} else {
		    if (plot_mode == CRend_PlotMode_Normal)
		    {
			for (j=0; j<width; j++) {
			    
			    k1 = (*fontx++);
			    *scrptr = colour[k1]; /* XXX */
			    scrptr++;
			}
		    }
		    else
		    {
			for (j=0; j<width; j++) {
			    
			    k1 = (*fontx++);
			    *scrptr ^= colour[k1]; /* XXX */
			    scrptr++;
			}
		    }
		}
		base+=stride;
		font += width;
	    }
	} else {
	    height = 0;
	    width = 0;
	}
	
	*height_out = height;
	return width;
}

static void 
CRend_DrawString_m (
	CRend_cl	*self,
	int32_t	x	/* IN */,
	int32_t	y	/* IN */,
	string_t	s	/* IN */ )
{
    CRend_st	*st = (CRend_st *) self->st;
    CRend_PlotMode plot_mode = st -> plot_mode;
    uint32_t colour[32];
    int  delta[3], alph[3], beta[3], comps[3];
    const int shifts[3] = {REDSHIFT, BLUESHIFT, GREENSHIFT};
    const int masks[3] = {RED, BLUE, GREEN};
    
    int i,j;
    char *ptr=s;
    pixel_t *pixptr;
    int width;
    int maxheight = 0, stringwidth = 0;
    
    for (i=0; i<3; i++) {
	alph[i] = ( (st->bg & 0x8000 ? 0 : st->bg) & masks[i])>>shifts[i];
	beta[i] = (st->fg & masks[i])>>shifts[i];
	delta[i] = beta[i] - alph[i];
    }
    TRC(fprintf(stderr,"From (%d,%d,%d) to (%d,%d,%d)\n", alph[0], alph[1], alph[2], beta[0], beta[1], beta[2]));
    for (i=0; i<32; i++ ) {
	colour[i] = 0;
	
	for (j=0; j<3; j++ ) {
	    comps[j] = (alph[j] + ((i * delta[j])>>5) );
	    colour[i] += ((comps[j] & 0x1f)<<shifts[j]);
	}
	TRC(fprintf(stderr,"%d: (%d,%d,%d) -> %x\n", i,comps[0],comps[1],comps[2], colour[i]));
    }
    
    
    pixptr = st->pixels + x + (y * st->stride);
    while (*ptr) {
	int height;
	width = PrintChar(*ptr, pixptr, st->stride, colour, &height, (st->bg & 0x8000), plot_mode);
	maxheight = MAX(height, maxheight);
	stringwidth += width;
	pixptr += width;
	
	ptr++;
    }
    
    
    for(i = y; i< y + maxheight; i++) {
	MARK_DIRTY_RANGE(x, x+stringwidth, i);
    }
    
}

static void 
CRend_Flush_m (
	CRend_cl	*self )
{
    CRend_st	*st = (CRend_st *) self->st;
    FBBlit_Rec  blitrec;
    word_t value;
    uint32_t nrecs; 
    int y;

    if (!st->display) {
	fprintf(stderr,"CRend$Flush called on CRendPixmap\n");
	return;
    }
    
    
    if(st->blit) {
	for(y = 0; y < st->height; y++) {
	    
	    if(st->dirty[y].left <= st->dirty[y].right) {
		int x = st->dirty[y].left & (~7);
		
		blitrec.x = x;
		blitrec.y = y;
		blitrec.sourcewidth = (st->dirty[y].right + 1) - x;
		blitrec.sourceheight = 1;
		blitrec.stride = st->stride * sizeof(pixel_t);
		blitrec.data = st->pixels + (y * st->stride) + x;
		
		MARK_CLEAN(st, y);
		while (FBBlit$Blit(st->blit, &blitrec));
	    }
	}
    } else if(st->fbio) {
	
	IO_Rec iorec;
	blitrec.x = 0;
	blitrec.y = 0;
	blitrec.sourcewidth = st->width;
	blitrec.sourceheight = st->height;
	blitrec.stride = st->stride * sizeof(pixel_t);
	blitrec.data = st->pixels;

	iorec.base = &blitrec;
	iorec.len = sizeof(FBBlit_Rec);

	    
	IO$PutPkt(st->fbio, 1, &iorec, 0, FOREVER);
	IO$GetPkt(st->fbio, 1, &iorec, FOREVER, &nrecs, &value);
    } else {
	
	fprintf(stderr, "Can't flush CRend\n");
	
    }
}

static void CRend_ExposeRectangle_m (
    CRend_clp self,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height) {

    CRend_st *st = self->st;

    int z;
    int xmax = x + width - 1;

    for(z = y; z < y + height; z++) {
	MARK_DIRTY_RANGE(x, xmax, z);
    }
}
	

static void 
CRend_Close_m (CRend_cl	*self )
{
    CRend_st	*st = (CRend_st *) self->st;
    
    
    if (st->ownpixdata == 1) {
	STR_DESTROY(st->pixstr);
    }
    if (st->ownpixdata == 2) {
	free(st->pixels);
    }
    if (st->display) {
	WS$DestroyWindow(st->display->ws, st->id);
    }

    if(st->dirty) FREE(st->dirty);
    if(st->fbio) IO$Dispose(st->fbio);
    FREE(st);
}


static void 
CRend_Map_m (
        CRend_cl        *self )
{
  CRend_st      *st = (CRend_st *) self->st;
  TRC(fprintf(stderr,"Mapping window %d with server at %p\n", st->id, st->display->ws));
  if (!st->display) {
      fprintf(stderr,"CRend$Map called on CRendPixmap\n");
      return;
  }

  WS$MapWindow(st->display->ws, st->id);
}

static void 
CRend_UnMap_m (
        CRend_cl        *self )
{
  CRend_st      *st = (CRend_st *) self->st;
    if (!st->display) {
	fprintf(stderr,"CRend$UnMap called on CRendPixmap\n");
	return;
    }

  WS$UnMapWindow(st->display->ws, st->id);
}

static void 
CRend_ResizeWindow_m (
        CRend_cl        *self,
        uint32_t _newwidth   /* IN */,
        uint32_t _newheight  /* IN */ )
{
    uint32_t NOCLOBBER newwidth = _newwidth;
    uint32_t NOCLOBBER newheight = _newheight;
    CRend_st      *st = (CRend_st *) self->st;
    const int oldwidth = st->width, oldheight = st->height;
    const pixel_t *oldpixels = st->pixels;
    Stretch_Size size, reqsize;
    pixel_t *newpixels;
    const int oldstride = st->stride;
    uint32_t NOCLOBBER newstride = newwidth;
    Stretch_clp NOCLOBBER newstr;
    Stretch_clp oldstr=st->pixstr;
    int copywidth, copyheight;
    int y;
    dirty_range * NOCLOBBER newdirty;
    dirty_range *olddirty = st->dirty;
    bool_t NOCLOBBER success = False;

    if (!st->display) {
	fprintf(stderr,"CRend$Resizewindow called on CRendPixmap\n");
	return;
    }

    TRY {

	newstride = (newwidth + st->xgrid - 1) & ~(st->xgrid - 1);
	reqsize = newheight * newstride * sizeof(pixel_t);

	newstr = STR_NEW(reqsize);

	if (!newstr) {
	    fprintf(stderr,"Stretch Allocator returned zero\n");
	    RAISE_CRend$NoResources();
	}
    
	newpixels = STR_RANGE(newstr, &size);
	if (size < reqsize) {
	    fprintf(stderr,"Can't resize; no new stretch available. "
		    "Size is %x (viz %x)\n", size, reqsize );
	    
	    RAISE_CRend$NoResources();
	}
	
	if (!newpixels) {
	    fprintf(stderr,"Stretch in silly place\n");
	    RAISE_CRend$NoResources();
	}

	newdirty = Heap$Malloc(Pvs(heap), sizeof(dirty_range) * newheight);
	
	if(!newdirty) {
	    fprintf(stderr, "Couldn't allocate dirty scanline structure\n");
	    RAISE_CRend$NoResources();
	}
	
	memset(newpixels, 0, size);
	
	copywidth = MIN(oldwidth, newwidth);
	copyheight = MIN(oldheight, newheight);
	TRC(fprintf(stderr,
		    "Blat from %p stride %p to %p stride %u area %x by %x\n", 
		    oldpixels, oldstride, newpixels, newstride, 
		    copyheight, copywidth));
	
	for (y=0; y<copyheight; y++) {
	    memcpy(newpixels + y * newstride, 
		   oldpixels + y * oldstride, 
		   copywidth * sizeof(pixel_t));
	}
	
	WS$ResizeWindow(st->display->ws,st->id, newwidth, newheight);
	
	st->stride = newstride;
	st->width = newwidth;
	st->height = newheight;
	st->pixstr = newstr;
	st->pixels = newpixels;
	st->dirty = newdirty;

	success = True;
	
	if(oldstr) {
	    STR_DESTROY(oldstr);
	}

	if(olddirty) {
	    FREE(olddirty);
	}
	
	MARK_ALL_DIRTY(st);

    } FINALLY {
	if(!success) {
	    if(newstr) {
		STR_DESTROY(newstr);
	    }
	    
	    if(newdirty) {
		FREE(newdirty);
	    }
	}
    } ENDTRY;
		

}

static void 
CRend_MoveWindow_m (
        CRend_cl        *self,
        int32_t x       /* IN */,
        int32_t y       /* IN */ )
{
  CRend_st      *st = (CRend_st *) self->st;
  if (!st->display) {
      fprintf(stderr,"CRend$Movewindow called on CRendPixmap\n");
      return;
  }

  WS$MoveWindow(st->display->ws, st->id, x, y);
}

static void CRend_ScrollRectangle_m(CRend_clp self,
				 uint32_t  xsrc, 
				 uint32_t  ysrc, 
				 uint32_t  width, 
				 uint32_t  height,
				 int32_t   xoffset,
				 int32_t   yoffset) {

    CRend_st   *st = self->st;

    int x0, x1, y0, y1;
    int ys;
    int xlen;
    pixel_t *srcpixels = st->pixels + ysrc * st->stride + xsrc;
    
    int xdest = ((int32_t) xsrc) + xoffset;
    int ydest = ((int32_t) ysrc) + yoffset;

    if((xsrc > st->width) | (ysrc > st->height) | (xsrc+width > st->width) |
       (ysrc + height > st->height)) {
	return;
    }

    /* calculate the starting x and y offsets in the source data */
    x0 = MAX(0, xdest)-xdest;
    y0 = MAX(0, ydest)-ydest;
    
    x1 = MIN(st->width,xdest+width)-xdest;
    y1 = MIN(st->height,ydest+height)-ydest;
    
    xlen = x1  - x0;
    
    if (x0>=x1 || y0>=y1) {
	return; /* no work to do */
    }
    
    if(yoffset < 0) {
	for (ys = y0; ys<y1; ys++) {
	    MARK_DIRTY_RANGE(xdest + x0, xdest + x0 + xlen, ydest + ys);
	    memcpy( st->pixels + xdest +x0 + (ydest+ys) * st->stride, 
		    srcpixels + x0 + ys*st->stride, 
		    xlen * sizeof(pixel_t));
	}
    } else {
	for (ys = y1 - 1; ys >= y0; ys--) {
	    MARK_DIRTY_RANGE(xdest + x0, xdest + x0 + xlen, ydest + ys);
	    memcpy( st->pixels + xdest +x0 + (ydest+ys) * st->stride, 
		    srcpixels + x0 + ys*st->stride, 
		    xlen * sizeof(pixel_t));
	}

    }

}

		       

static WS_WindowID 
CRend_GetID_m (
        CRend_cl        *self )
{
  CRend_st      *st = (CRend_st *) self->st;
  return st->id;
}

static addr_t 
CRend_GetPixelData_m (
        CRend_cl        *self,
   /* RETURNS */
	uint32_t        *stride)
{
  CRend_st      *st = (CRend_st *) self->st;
  TRC(fprintf(stderr,"Writing stride to %x\n", stride));
  *stride = st->stride;
  return st->pixels;
}


static uint32_t CRend_GetWidth_m(CRend_cl *self) { 
    return ((CRend_st*) self->st)->width; 
}

static uint32_t CRend_GetHeight_m(CRend_cl *self) { 
    return ((CRend_st*) self->st)->height; 
}

static void CRend_DrawPixmap_m( 
    CRend_cl *self,
    int32_t x, int32_t y, 
    addr_t data,
    uint32_t width, uint32_t height, uint32_t stride ) 
{
    CRend_st      *st = (CRend_st *) self->st;
    CRend_PlotMode plot_mode = st -> plot_mode;
    int x0, x1, y0, y1;
    int ys;
    int xlen;
    pixel_t *pixels = data;
    
    x0 = MAX(0,x)-x;
    y0 = MAX(0,y)-y;
    
    x1 = MIN(st->width,x+width)-x;
    y1 = MIN(st->height,y+height)-y;
    
    xlen = x1  - x0;
    
    if (x0>=x1 || y0>=y1) {
	return; /* no work to do */
    }
    

    if (plot_mode == CRend_PlotMode_Normal)
    {
	for (ys = y0; ys<y1; ys++) {
	    MARK_DIRTY_RANGE(x + x0, x + x0 + xlen, y + ys);
	    memcpy( st->pixels + x+x0 + (y+ys) * st->stride, 
		    pixels + x0 + ys*stride, 
		    xlen * sizeof(pixel_t));
	}
    }
    else
    {
	pixel_t *dest;
	pixel_t *src;
	int i;

	for (ys = y0; ys<y1; ys++) {
	    MARK_DIRTY_RANGE(x + x0, x + x0 + xlen, y + ys);

	    dest = st->pixels + x+x0 + (y+ys) * st->stride;
	    src = pixels + x0 + ys*stride;

	    for (i = 0; i < xlen; i ++)
	    {
		*dest ^= (*src);
		dest ++;
		src ++;
	    }
	}
    }
}
         
  
  

/*
 * End 
 */
