/*
*****************************************************************************
**                                                                          *
**  Modifications for Nemesis                                               *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/WS/Region
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Portable region manipulation code
** 
** ENVIRONMENT: 
**
**      Shared library
** 
** ID : $Id: Region.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

/***********************************************************
Copyright 1987, 1988, 1989 by 
Digital Equipment Corporation, Maynard, Massachusetts, and
the Massachusetts Institute of Technology, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Digital or MIT not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#include <nemesis.h>
#include <stdio.h>
#include <exceptions.h>
#include <limits.h>
#include <bstring.h>

#include <regionstr.h>
#include <Region.ih>

#define xalloc(s) 	({ void *p = Heap$Malloc(Pvs(heap), (s)); if (!p) RAISE_Heap$NoMemory(); p; })
#define xrealloc(p, s) 	Heap$Realloc(Pvs(heap), (p), (s))
#define xfree(p)  	FREE(p)
#define min  		MIN
#define max		MAX
#define MAXSHORT	SHRT_MAX
#define MINSHORT	SHRT_MIN



/*
 * Module stuff
 */
static  Region_New_fn                   Region_New_m;
static  Region_Free_fn                  Region_Free_m;
static  Region_InitRec_fn               Region_InitRec_m;
static  Region_DoneRec_fn               Region_DoneRec_m;
static  Region_Equal_fn                 Region_Equal_m;
static  Region_SetBox_fn                Region_SetBox_m;
static  Region_SetXY_fn                 Region_SetXY_m;
static  Region_Print_fn                 Region_Print_m;
static  Region_Copy_fn                  Region_Copy_m;
static  Region_Intersect_fn             Region_Intersect_m;
static  Region_Union_fn                 Region_Union_m;
static  Region_Append_fn                Region_Append_m;
static  Region_MakeValid_fn             Region_MakeValid_m;
static  Region_Subtract_fn              Region_Subtract_m;
static  Region_Inverse_fn               Region_Inverse_m;
static  Region_TestRect_fn              Region_TestRect_m;
static  Region_TestPoint_fn             Region_TestPoint_m;
static  Region_Translate_fn             Region_Translate_m;
static  Region_IsEmpty_fn               Region_IsEmpty_m;
static  Region_MakeEmpty_fn             Region_MakeEmpty_m;
static  Region_Extents_fn               Region_Extents_m;

static  Region_op       ms = {
    Region_New_m,
    Region_Free_m,
    Region_InitRec_m,
    Region_DoneRec_m,
    Region_Equal_m,
    Region_SetBox_m,
    Region_SetXY_m,
    Region_Print_m,
    Region_Copy_m,
    Region_Intersect_m,
    Region_Union_m,
    Region_Append_m,
    Region_MakeValid_m,
    Region_Subtract_m,
    Region_Inverse_m,
    Region_TestRect_m,
    Region_TestPoint_m,
    Region_Translate_m,
    Region_IsEmpty_m,
    Region_MakeEmpty_m,
    Region_Extents_m,
};

static const Region_cl       cl = { &ms, NULL };

CL_EXPORT (Region, Region, cl);

/*
 * hack until callers of these functions can deal with out-of-memory
 */

bool_t miValidRegion(Region_T rgn);

#ifdef DEBUG

void death(void) {
    ntsc_halt();
}
#define assert(expr) {if (!(expr)) {\
		     ENTER_KERNEL_CRITICAL_SECTION();\
		   eprintf("Assertion failed file %s, line %d: " #expr "\n", \
					 __FILE__, __LINE__); \
		   death();\
			    LEAVE_KERNEL_CRITICAL_SECTION();\
								}\
		      }
#else
#define assert(expr)
#endif

#define good(reg) assert(miValidRegion(reg))

/*
 * The functions in this file implement the Region abstraction used extensively
 * throughout the X11 sample server. A Region is simply a set of disjoint
 * (non-overlapping) rectangles, plus an "extent" rectangle which is the
 * smallest single rectangle that contains all the non-overlapping rectangles.
 *
 * A Region is implemented as a "y-x-banded" array of rectangles.  This array
 * imposes two degrees of order.  First, all rectangles are sorted by top side
 * y coordinate first (y1), and then by left side x coordinate (x1).
 *
 * Furthermore, the rectangles are grouped into "bands".  Each rectangle in a
 * band has the same top y coordinate (y1), and each has the same bottom y
 * coordinate (y2).  Thus all rectangles in a band differ only in their left
 * and right side (x1 and x2).  Bands are implicit in the array of rectangles:
 * there is no separate list of band start pointers.
 *
 * The y-x band representation does not minimize rectangles.  In particular,
 * if a rectangle vertically crosses a band (the rectangle has scanlines in 
 * the y1 to y2 area spanned by the band), then the rectangle may be broken
 * down into two or more smaller rectangles stacked one atop the other. 
 *
 *  -----------				    -----------
 *  |         |				    |         |		    band 0
 *  |         |  --------		    -----------  --------
 *  |         |  |      |  in y-x banded    |         |  |      |   band 1
 *  |         |  |      |  form is	    |         |  |      |
 *  -----------  |      |		    -----------  --------
 *               |      |				 |      |   band 2
 *               --------				 --------
 *
 * An added constraint on the rectangles is that they must cover as much
 * horizontal area as possible: no two rectangles within a band are allowed
 * to touch.
 *
 * Whenever possible, bands will be merged together to cover a greater vertical
 * distance (and thus reduce the number of rectangles). Two bands can be merged
 * only if the bottom of one touches the top of the other and they have
 * rectangles in the same places (of the same width, of course).
 *
 * Adam de Boor wrote most of the original region code.  Joel McCormack
 * substantially modified or rewrote most of the core arithmetic routines,
 * and added miRegionValidate in order to support several speed improvements
 * to miValidateTree.  Bob Scheifler changed the representation to be more
 * compact when empty or a single rectangle, and did a bunch of gratuitous
 * reformatting.
 */

/*  True iff two Boxes overlap */
#define EXTENTCHECK(r1,r2) \
      (!( ((r1)->x2 <= (r2)->x1)  || \
          ((r1)->x1 >= (r2)->x2)  || \
          ((r1)->y2 <= (r2)->y1)  || \
          ((r1)->y1 >= (r2)->y2) ) )

/* True iff (x,y) is in Box */
#define INBOX(r,x,y) \
      ( ((r)->x2 >  x) && \
        ((r)->x1 <= x) && \
        ((r)->y2 >  y) && \
        ((r)->y1 <= y) )

/* True iff Box r1 contains Box r2 */
#define SUBSUMES(r1,r2) \
      ( ((r1)->x1 <= (r2)->x1) && \
        ((r1)->x2 >= (r2)->x2) && \
        ((r1)->y1 <= (r2)->y1) && \
        ((r1)->y2 >= (r2)->y2) )

#define xallocData(n) (Region_DataPtr)xalloc(REGION_SZOF(n))
#define xfreeData(reg) if ((reg)->data && (reg)->data->size) xfree((reg)->data)

#define RECTALLOC(pReg,n) \
if (!(pReg)->data || (((pReg)->data->numRects + (n)) > (pReg)->data->size)) \
    miRectAlloc(pReg, n)

#define ADDRECT(pNextRect,nx1,ny1,nx2,ny2)	\
{						\
    pNextRect->x1 = nx1;			\
    pNextRect->y1 = ny1;			\
    pNextRect->x2 = nx2;			\
    pNextRect->y2 = ny2;			\
    pNextRect++;				\
}

#define NEWRECT(pReg,pNextRect,nx1,ny1,nx2,ny2)			\
{									\
    if (!(pReg)->data || ((pReg)->data->numRects == (pReg)->data->size))\
    {									\
	miRectAlloc(pReg, 1);						\
	pNextRect = REGION_TOP(pReg);					\
    }									\
    ADDRECT(pNextRect,nx1,ny1,nx2,ny2);					\
    pReg->data->numRects++;						\
    assert(pReg->data->numRects<=pReg->data->size);			\
}


#define DOWNSIZE(reg,numRects)						 \
if (((numRects) < ((reg)->data->size >> 3)) && ((reg)->data->size > 200)) \
{									 \
    Region_DataPtr NewData;							 \
    NewData = (Region_DataPtr)xrealloc((reg)->data, REGION_SZOF(numRects));	 \
    if (NewData)							 \
    {									 \
	NewData->size = (numRects);					 \
	(reg)->data = NewData;						 \
    }									 \
}

#define MAKE_EMPTY(reg) \
{\
    if((reg)->data) {\
        if((reg)->data != &EmptyData) {\
	    (reg)->data->numRects = 0;\
        }\
    } else {\
        (reg)->data = (Region_Data *)&EmptyData;\
    }\
    (reg)->extents.x2 = (reg)->extents.x1;\
    (reg)->extents.y2 = (reg)->extents.y1;\
}


static const Region_Box EmptyBox = {0, 0, 0, 0};
static const Region_Data EmptyData = {0, 0};

static void Region_Print_m(Region_clp self, Region_T rgn)
{
    int num, size;
    register int i;
    Region_BoxPtr rects;

    good(rgn);

    num = REGION_NUM_RECTS(rgn);
    size = REGION_SIZE(rgn);
    rects = REGION_RECTS(rgn);
    printf("num: %d size: %d\n", num, size);
    printf("extents: %d %d %d %d\n",
	   rgn->extents.x1, rgn->extents.y1, rgn->extents.x2, rgn->extents.y2);
    for (i = 0; i < num; i++)
      printf("%d %d %d %d \n",
	     rects[i].x1, rects[i].y1, rects[i].x2, rects[i].y2);
    printf("\n");
}


static bool_t Region_Equal_m(Region_clp self, Region_T reg1, Region_T reg2)
{
    int i;
    Region_BoxPtr rects1, rects2;

    if (reg1->extents.x1 != reg2->extents.x1) return False;
    if (reg1->extents.x2 != reg2->extents.x2) return False;
    if (reg1->extents.y1 != reg2->extents.y1) return False;
    if (reg1->extents.y2 != reg2->extents.y2) return False;
    if (REGION_NUM_RECTS(reg1) != REGION_NUM_RECTS(reg2)) return False;
    
    rects1 = REGION_RECTS(reg1);
    rects2 = REGION_RECTS(reg2);
    for (i = 0; i != REGION_NUM_RECTS(reg1); i++) {
	if (rects1[i].x1 != rects2[i].x1) return False;
	if (rects1[i].x2 != rects2[i].x2) return False;
	if (rects1[i].y1 != rects2[i].y1) return False;
	if (rects1[i].y2 != rects2[i].y2) return False;
    }
    return True;
}

#ifdef DEBUG
#define INVALID(x) {ENTER_KERNEL_CRITICAL_SECTION(); x; LEAVE_KERNEL_CRITICAL_SECTION()}
#else
#define INVALID(x)
#endif

bool_t
miValidRegion(reg)
    Region_T reg;
{
    register int i, numRects;
    bool_t tmp;

    if ((reg->extents.x1 > reg->extents.x2) ||
	(reg->extents.y1 > reg->extents.y2))
    {
	INVALID(eprintf("Bad extents\n"));
	return False;
    }
    
    numRects = REGION_NUM_RECTS(reg);
    if (!numRects) {
	
	tmp = (reg->extents.x1 == reg->extents.x2) &&
	    (reg->extents.y1 == reg->extents.y2) &&
	    (reg->data->size || (reg->data == &EmptyData));

	INVALID(if (!tmp) eprintf("Bad empty region %p\n", reg));
	return tmp;
    } else if (numRects == 1) {
	tmp = !reg->data;
	INVALID(if (!tmp) eprintf("1 rectangle, but data = %p\n", reg->data));
	return tmp;
    } else {
	register Region_BoxPtr pboxP, pboxN;
	Region_Box box;

	pboxP = REGION_RECTS(reg);
	box = *pboxP;
	box.y2 = pboxP[numRects-1].y2;
	pboxN = pboxP + 1;
	for (i = numRects; --i > 0; pboxP++, pboxN++)
	{
	    if ((pboxN->x1 >= pboxN->x2) ||
		(pboxN->y1 >= pboxN->y2)) {
		INVALID(eprintf("Bad box %p\n", pboxN));
		return False;
	    }

	    if (pboxN->x1 < box.x1)
	        box.x1 = pboxN->x1;
	    if (pboxN->x2 > box.x2)
		box.x2 = pboxN->x2;
	    if ((pboxN->y1 < pboxP->y1) ||
		((pboxN->y1 == pboxP->y1) &&
		 ((pboxN->x1 < pboxP->x2) || (pboxN->y2 != pboxP->y2)))) {
		INVALID(eprintf("Out of order box %p\n", pboxN));
		return False;
	    }
	}
	
	tmp = (box.x1 == reg->extents.x1) &&
		(box.x2 == reg->extents.x2) &&
		(box.y1 == reg->extents.y1) &&
		(box.y2 == reg->extents.y2);

	INVALID(if(!tmp) eprintf("Region and extents not the same\n"));

	return tmp;
    }
}



/*****************************************************************
 *   RegionCreate(rect, size)
 *     This routine does a simple malloc to make a structure of
 *     REGION of "size" number of rectangles.
 *****************************************************************/

static Region_T Region_New_m(Region_clp self, Region_BoxPtr rect)
{
    register Region_T pReg;
    
    pReg = (Region_T)xalloc(sizeof(Region_Rec));
    if (rect)
    {
	pReg->extents = *rect;
	pReg->data = (Region_DataPtr)NULL;
    }
    else
    {
	pReg->extents = EmptyBox;
	pReg->data = (Region_Data *)&EmptyData;
    }
    return(pReg);
}

/*****************************************************************
 *   RegionInit(pReg, rect, size)
 *     Outer region rect is statically allocated.
 *****************************************************************/

static void Region_InitRec_m(Region_clp self, 
			     Region_Rec *pReg, 
			     Region_BoxPtr rect)
{
    if (rect)
    {
	pReg->extents = *rect;
	pReg->data = (Region_DataPtr)NULL;
    }
    else
    {
	pReg->extents = EmptyBox;
	pReg->data = (Region_Data *)&EmptyData;
    }
}

static void Region_Free_m(Region_clp self, Region_T pReg)
{
    good(pReg);
    xfreeData(pReg);
    xfree(pReg);
}

static void Region_DoneRec_m(Region_clp self, Region_T pReg)
{
    good(pReg);
    xfreeData(pReg);
}

static bool_t miRectAlloc(pRgn, n)
    register Region_T pRgn;
    int n;
{
    if (!pRgn->data)
    {
	n++;
	pRgn->data = xallocData(n);
	pRgn->data->numRects = 1;
	*REGION_BOXPTR(pRgn) = pRgn->extents;
    }
    else if (!pRgn->data->size)
    {
	pRgn->data = xallocData(n);
	pRgn->data->numRects = 0;
    }
    else
    {
	if (n == 1)
	{
	    n = pRgn->data->numRects;
	    if (n > 500) /* XXX pick numbers out of a hat */
		n = 250;
	}
	n += pRgn->data->numRects;
	pRgn->data = (Region_DataPtr)xrealloc(pRgn->data, REGION_SZOF(n));
    }
    pRgn->data->size = n;
    return True;
}

static void Region_Copy_m(Region_clp self, Region_T dst, Region_T src)
{
    good(dst);
    good(src);
    if (dst == src)
	return;
    dst->extents = src->extents;
    if (!src->data || !src->data->numRects)
    {
	xfreeData(dst);
	dst->data = src->data;
	return;
    }
    if (!dst->data || (dst->data->size < src->data->numRects))
    {
	xfreeData(dst);
	dst->data = xallocData(src->data->numRects);
	dst->data->size = src->data->numRects;
    }
    dst->data->numRects = src->data->numRects;
    bcopy((char *)REGION_BOXPTR(src), (char *)REGION_BOXPTR(dst),
	    dst->data->numRects * sizeof(Region_Box));
    return;
}


/*======================================================================
 *	    Generic Region Operator
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miCoalesce --
 *	Attempt to merge the boxes in the current band with those in the
 *	previous one.  We are guaranteed that the current band extends to
 *      the end of the rects array.  Used only by miRegionOp.
 *
 * Results:
 *	The new index for the previous band.
 *
 * Side Effects:
 *	If coalescing takes place:
 *	    - rectangles in the previous band will have their y2 fields
 *	      altered.
 *	    - pReg->data->numRects will be decreased.
 *
 *-----------------------------------------------------------------------
 */
INLINE static int
miCoalesce (pReg, prevStart, curStart)
    register Region_T	pReg;	    	/* Region to coalesce		     */
    int	    	  	prevStart;  	/* Index of start of previous band   */
    int	    	  	curStart;   	/* Index of start of current band    */
{
    register Region_BoxPtr	pPrevBox;   	/* Current box in previous band	     */
    register Region_BoxPtr	pCurBox;    	/* Current box in current band       */
    register int  	numRects;	/* Number rectangles in both bands   */
    register int	y2;		/* Bottom of current band	     */
    /*
     * Figure out how many rectangles are in the band.
     */
    numRects = curStart - prevStart;
    assert(numRects == pReg->data->numRects - curStart);

    if (!numRects) return curStart;

    /*
     * The bands may only be coalesced if the bottom of the previous
     * matches the top scanline of the current.
     */
    pPrevBox = REGION_BOX(pReg, prevStart);
    pCurBox = REGION_BOX(pReg, curStart);
    if (pPrevBox->y2 != pCurBox->y1) return curStart;

    /*
     * Make sure the bands have boxes in the same places. This
     * assumes that boxes have been added in such a way that they
     * cover the most area possible. I.e. two boxes in a band must
     * have some horizontal space between them.
     */
    y2 = pCurBox->y2;

    do {
	if ((pPrevBox->x1 != pCurBox->x1) || (pPrevBox->x2 != pCurBox->x2)) {
	    return (curStart);
	}
	pPrevBox++;
	pCurBox++;
	numRects--;
    } while (numRects);

    /*
     * The bands may be merged, so set the bottom y of each box
     * in the previous band to the bottom y of the current band.
     */
    numRects = curStart - prevStart;
    pReg->data->numRects -= numRects;
    do {
	pPrevBox--;
	pPrevBox->y2 = y2;
	numRects--;
    } while (numRects);
    return prevStart;
}


/* Quicky macro to avoid trivial reject procedure calls to miCoalesce */

#define Coalesce(newReg, prevBand, curBand)				\
    if (curBand - prevBand == newReg->data->numRects - curBand) {	\
	prevBand = miCoalesce(newReg, prevBand, curBand);		\
    } else {								\
	prevBand = curBand;						\
    }

/*-
 *-----------------------------------------------------------------------
 * miAppendNonO --
 *	Handle a non-overlapping band for the union and subtract operations.
 *      Just adds the (top/bottom-clipped) rectangles into the region.
 *      Doesn't have to check for subsumption or anything.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	pReg->data->numRects is incremented and the rectangles overwritten
 *	with the rectangles we're passed.
 *
 *-----------------------------------------------------------------------
 */

INLINE static bool_t
miAppendNonO (pReg, r, rEnd, y1, y2)
    register Region_T	pReg;
    register Region_BoxPtr	r;
    Region_BoxPtr  	  	rEnd;
    register int  	y1;
    register int  	y2;
{
    register Region_BoxPtr	pNextRect;
    register int	newRects;

    newRects = rEnd - r;

    assert(y1 < y2);
    assert(newRects != 0);

    /* Make sure we have enough space for all rectangles to be added */
    RECTALLOC(pReg, newRects);
    pNextRect = REGION_TOP(pReg);
    pReg->data->numRects += newRects;
    do {
	assert(r->x1 < r->x2);
	ADDRECT(pNextRect, r->x1, y1, r->x2, y2);
	r++;
    } while (r != rEnd);

    return True;
}

#define FindBand(r, rBandEnd, rEnd, ry1)		    \
{							    \
    ry1 = r->y1;					    \
    rBandEnd = r+1;					    \
    while ((rBandEnd != rEnd) && (rBandEnd->y1 == ry1)) {   \
	rBandEnd++;					    \
    }							    \
}

#define	AppendRegions(newReg, r, rEnd)					\
{									\
    int newRects;							\
    if ((newRects = rEnd - r)) {					\
	RECTALLOC(newReg, newRects);					\
	bcopy((char *)r, (char *)REGION_TOP(newReg),			\
              newRects * sizeof(Region_Box));				\
	newReg->data->numRects += newRects;				\
    }									\
}

/*-
 *-----------------------------------------------------------------------
 * miRegionOp --
 *	Apply an operation to two regions. Called by miUnion, miInverse,
 *	miSubtract, miIntersect....  Both regions MUST have at least one
 *      rectangle, and cannot be the same object.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *	The new region is overwritten.
 *	pOverlap set to True if overlapFunc ever returns True.
 *
 * Notes:
 *	The idea behind this function is to view the two regions as sets.
 *	Together they cover a rectangle of area that this function divides
 *	into horizontal bands where points are covered only by one region
 *	or by both. For the first case, the nonOverlapFunc is called with
 *	each the band and the band's upper and lower extents. For the
 *	second, the overlapFunc is called to process the entire band. It
 *	is responsible for clipping the rectangles in the band, though
 *	this function provides the boundaries.
 *	At the end of each band, the new region is coalesced, if possible,
 *	to reduce the number of rectangles in the region.
 *
 *-----------------------------------------------------------------------
 */
static bool_t
miRegionOp(newReg, reg1, reg2, overlapFunc, appendNon1, appendNon2, pOverlap)
    Region_T       newReg;		    /* Place to store result	     */
    Region_T       reg1;		    /* First region in operation     */
    Region_T       reg2;		    /* 2d region in operation        */
    bool_t	    (*overlapFunc)();       /* Function to call for over-
					     * lapping bands		     */
    bool_t	    appendNon1;		    /* Append non-overlapping bands  */
					    /* in region 1 ? */
    bool_t	    appendNon2;		    /* Append non-overlapping bands  */
					    /* in region 2 ? */
    bool_t	    *pOverlap;
{
    register Region_BoxPtr r1;			    /* Pointer into first region     */
    register Region_BoxPtr r2;			    /* Pointer into 2d region	     */
    Region_BoxPtr	    r1End;		    /* End of 1st region	     */
    Region_BoxPtr	    r2End;		    /* End of 2d region		     */
    short	    ybot;		    /* Bottom of intersection	     */
    short	    ytop;		    /* Top of intersection	     */
    Region_DataPtr	    oldData;		    /* Old data for newReg	     */
    int		    prevBand;		    /* Index of start of
					     * previous band in newReg       */
    int		    curBand;		    /* Index of start of current
					     * band in newReg		     */
    register Region_BoxPtr r1BandEnd;		    /* End of current band in r1     */
    register Region_BoxPtr r2BandEnd;		    /* End of current band in r2     */
    short	    top;		    /* Top of non-overlapping band   */
    short	    bot;		    /* Bottom of non-overlapping band*/
    register int    r1y1;		    /* Temps for r1->y1 and r2->y1   */
    register int    r2y1;
    int		    newSize;
    int		    numRects;

    /*
     * Initialization:
     *	set r1, r2, r1End and r2End appropriately, save the rectangles
     * of the destination region until the end in case it's one of
     * the two source regions, then mark the "new" region empty, allocating
     * another array of rectangles for it to use.
     */

    r1 = REGION_RECTS(reg1);
    newSize = REGION_NUM_RECTS(reg1);
    r1End = r1 + newSize;
    numRects = REGION_NUM_RECTS(reg2);
    r2 = REGION_RECTS(reg2);
    r2End = r2 + numRects;
    assert(r1 != r1End);
    assert(r2 != r2End);

    oldData = (Region_DataPtr)NULL;
    if (((newReg == reg1) && (newSize > 1)) ||
	((newReg == reg2) && (numRects > 1)))
    {
	oldData = newReg->data;
	newReg->data = (Region_Data *)&EmptyData;
    }
    /* guess at new size */
    if (numRects > newSize)
	newSize = numRects;
    newSize <<= 1;
    if (!newReg->data)
	newReg->data = (Region_Data *)&EmptyData;
    else if (newReg->data->size)
	newReg->data->numRects = 0;
    if (newSize > newReg->data->size)
	miRectAlloc(newReg, newSize);

    /*
     * Initialize ybot.
     * In the upcoming loop, ybot and ytop serve different functions depending
     * on whether the band being handled is an overlapping or non-overlapping
     * band.
     * 	In the case of a non-overlapping band (only one of the regions
     * has points in the band), ybot is the bottom of the most recent
     * intersection and thus clips the top of the rectangles in that band.
     * ytop is the top of the next intersection between the two regions and
     * serves to clip the bottom of the rectangles in the current band.
     *	For an overlapping band (where the two regions intersect), ytop clips
     * the top of the rectangles of both regions and ybot clips the bottoms.
     */

    ybot = min(r1->y1, r2->y1);
    
    /*
     * prevBand serves to mark the start of the previous band so rectangles
     * can be coalesced into larger rectangles. qv. miCoalesce, above.
     * In the beginning, there is no previous band, so prevBand == curBand
     * (curBand is set later on, of course, but the first band will always
     * start at index 0). prevBand and curBand must be indices because of
     * the possible expansion, and resultant moving, of the new region's
     * array of rectangles.
     */
    prevBand = 0;
    
    do {
	/*
	 * This algorithm proceeds one source-band (as opposed to a
	 * destination band, which is determined by where the two regions
	 * intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
	 * rectangle after the last one in the current band for their
	 * respective regions.
	 */
	assert(r1 != r1End);
	assert(r2 != r2End);
    
	FindBand(r1, r1BandEnd, r1End, r1y1);
	FindBand(r2, r2BandEnd, r2End, r2y1);

	/*
	 * First handle the band that doesn't intersect, if any.
	 *
	 * Note that attention is restricted to one band in the
	 * non-intersecting region at once, so if a region has n
	 * bands between the current position and the next place it overlaps
	 * the other, this entire loop will be passed through n times.
	 */
	if (r1y1 < r2y1) {
	    if (appendNon1) {
		top = max(r1y1, ybot);
		bot = min(r1->y2, r2y1);
		if (top != bot)	{
		    curBand = newReg->data->numRects;
		    miAppendNonO(newReg, r1, r1BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r2y1;
	} else if (r2y1 < r1y1) {
	    if (appendNon2) {
		top = max(r2y1, ybot);
		bot = min(r2->y2, r1y1);
		if (top != bot) {
		    curBand = newReg->data->numRects;
		    miAppendNonO(newReg, r2, r2BandEnd, top, bot);
		    Coalesce(newReg, prevBand, curBand);
		}
	    }
	    ytop = r1y1;
	} else {
	    ytop = r1y1;
	}

	/*
	 * Now see if we've hit an intersecting band. The two bands only
	 * intersect if ybot > ytop
	 */
	ybot = min(r1->y2, r2->y2);
	if (ybot > ytop) {
	    curBand = newReg->data->numRects;
	    (* overlapFunc)(newReg, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot,
			    pOverlap);
	    Coalesce(newReg, prevBand, curBand);
	}

	/*
	 * If we've finished with a band (y2 == ybot) we skip forward
	 * in the region to the next band.
	 */
	if (r1->y2 == ybot) r1 = r1BandEnd;
	if (r2->y2 == ybot) r2 = r2BandEnd;

    } while (r1 != r1End && r2 != r2End);

    /*
     * Deal with whichever region (if any) still has rectangles left.
     *
     * We only need to worry about banding and coalescing for the very first
     * band left.  After that, we can just group all remaining boxes,
     * regardless of how many bands, into one final append to the list.
     */

    if ((r1 != r1End) && appendNon1) {
	/* Do first nonOverlap1Func call, which may be able to coalesce */
	FindBand(r1, r1BandEnd, r1End, r1y1);
	curBand = newReg->data->numRects;
	miAppendNonO(newReg, r1, r1BandEnd, max(r1y1, ybot), r1->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Just append the rest of the boxes  */
	AppendRegions(newReg, r1BandEnd, r1End);

    } else if ((r2 != r2End) && appendNon2) {
	/* Do first nonOverlap2Func call, which may be able to coalesce */
	FindBand(r2, r2BandEnd, r2End, r2y1);
	curBand = newReg->data->numRects;
	miAppendNonO(newReg, r2, r2BandEnd, max(r2y1, ybot), r2->y2);
	Coalesce(newReg, prevBand, curBand);
	/* Append rest of boxes */
	AppendRegions(newReg, r2BandEnd, r2End);
    }

    if (oldData)
	xfree(oldData);

    if (!(numRects = newReg->data->numRects))
    {
	MAKE_EMPTY(newReg);
    }
    else if (numRects == 1)
    {
	newReg->extents = *REGION_BOXPTR(newReg);
	xfreeData(newReg);
	newReg->data = (Region_DataPtr)NULL;
    }
#if 0
    else
    {
	DOWNSIZE(newReg, numRects);
    }
#endif
    return True;
}

/*-
 *-----------------------------------------------------------------------
 * miSetExtents --
 *	Reset the extents of a region to what they should be. Called by
 *	miSubtract and miIntersect as they can't figure it out along the
 *	way or do so easily, as miUnion can.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The region's 'extents' structure is overwritten.
 *
 *-----------------------------------------------------------------------
 */
void
miSetExtents (pReg)
    register Region_T pReg;
{
    register Region_BoxPtr pBox, pBoxEnd;

    if (!pReg->data)
	return;
    if (!pReg->data->numRects)
    {
	pReg->extents.x2 = pReg->extents.x1;
	pReg->extents.y2 = pReg->extents.y1;
	return;
    }

    pBox = REGION_BOXPTR(pReg);
    pBoxEnd = REGION_END(pReg);

    /*
     * Since pBox is the first rectangle in the region, it must have the
     * smallest y1 and since pBoxEnd is the last rectangle in the region,
     * it must have the largest y2, because of banding. Initialize x1 and
     * x2 from  pBox and pBoxEnd, resp., as good things to initialize them
     * to...
     */
    pReg->extents.x1 = pBox->x1;
    pReg->extents.y1 = pBox->y1;
    pReg->extents.x2 = pBoxEnd->x2;
    pReg->extents.y2 = pBoxEnd->y2;

    assert(pReg->extents.y1 < pReg->extents.y2);
    while (pBox <= pBoxEnd) {
	if (pBox->x1 < pReg->extents.x1)
	    pReg->extents.x1 = pBox->x1;
	if (pBox->x2 > pReg->extents.x2)
	    pReg->extents.x2 = pBox->x2;
	pBox++;
    };

    assert(pReg->extents.x1 < pReg->extents.x2);
}

/*======================================================================
 *	    Region Intersection
 *====================================================================*/
/*-
 *-----------------------------------------------------------------------
 * miIntersectO --
 *	Handle an overlapping band for miIntersect.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *	Rectangles may be added to the region.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static bool_t
miIntersectO (pReg, r1, r1End, r2, r2End, y1, y2, pOverlap)
    register Region_T	pReg;
    register Region_BoxPtr	r1;
    Region_BoxPtr  	  	r1End;
    register Region_BoxPtr	r2;
    Region_BoxPtr  	  	r2End;
    short    	  	y1;
    short    	  	y2;
    bool_t		*pOverlap;
{
    register int  	x1;
    register int  	x2;
    register Region_BoxPtr	pNextRect;

    pNextRect = REGION_TOP(pReg);

    assert(y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    do {
	x1 = max(r1->x1, r2->x1);
	x2 = min(r1->x2, r2->x2);

	/*
	 * If there's any overlap between the two rectangles, add that
	 * overlap to the new region.
	 */
	if (x1 < x2)
	    NEWRECT(pReg, pNextRect, x1, y1, x2, y2);

	/*
	 * Advance the pointer(s) with the leftmost right side, since the next
	 * rectangle on that list may still overlap the other region's
	 * current rectangle.
	 */
	if (r1->x2 == x2) {
	    r1++;
	}
	if (r2->x2 == x2) {
	    r2++;
	}
    } while ((r1 != r1End) && (r2 != r2End));

    return True;
}


static void Region_Intersect_m(Region_clp self, Region_T newReg, 
			       Region_T reg1, Region_T reg2)
{
    good(reg1);
    good(reg2);
    good(newReg);
   /* check for trivial reject */
    if (REGION_NIL(reg1)  || REGION_NIL(reg2) ||
	!EXTENTCHECK(&reg1->extents, &reg2->extents))
    {
	/* Covers about 20% of all cases */
	
	MAKE_EMPTY(newReg);

    }
    else if (!reg1->data && !reg2->data)
    {
	/* Covers about 80% of cases that aren't trivially rejected */
	newReg->extents.x1 = max(reg1->extents.x1, reg2->extents.x1);
	newReg->extents.y1 = max(reg1->extents.y1, reg2->extents.y1);
	newReg->extents.x2 = min(reg1->extents.x2, reg2->extents.x2);
	newReg->extents.y2 = min(reg1->extents.y2, reg2->extents.y2);
	xfreeData(newReg);
	newReg->data = (Region_DataPtr)NULL;
    }
    else if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
	Region_Copy_m(NULL, newReg, reg1);
	return;
    }
    else if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
	Region_Copy_m(NULL, newReg, reg2);
	return;
    }
    else if (reg1 == reg2)
    {
	Region_Copy_m(NULL, newReg, reg1);
    }
    else
    {
	/* General purpose intersection */
	bool_t overlap; /* result ignored */
	if (!miRegionOp(newReg, reg1, reg2, miIntersectO, False, False,
			&overlap))
	    return;
	miSetExtents(newReg);
    }

    good(newReg);
    return;
}

#define MERGERECT(r)						\
{								\
    if (r->x1 <= x2) {						\
	/* Merge with current rectangle */			\
	if (r->x1 < x2) *pOverlap = True;				\
	if (x2 < r->x2) x2 = r->x2;				\
    } else {							\
	/* Add current rectangle, start new one */		\
	NEWRECT(pReg, pNextRect, x1, y1, x2, y2);		\
	x1 = r->x1;						\
	x2 = r->x2;						\
    }								\
    r++;							\
}

/*======================================================================
 *	    Region Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miUnionO --
 *	Handle an overlapping band for the union operation. Picks the
 *	left-most rectangle each time and merges it into the region.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *	pReg is overwritten.
 *	pOverlap is set to True if any boxes overlap.
 *
 *-----------------------------------------------------------------------
 */
static bool_t
miUnionO (pReg, r1, r1End, r2, r2End, y1, y2, pOverlap)
    register Region_T	pReg;
    register Region_BoxPtr	r1;
	     Region_BoxPtr  	r1End;
    register Region_BoxPtr	r2;
	     Region_BoxPtr  	r2End;
	     short	y1;
	     short	y2;
	     bool_t	*pOverlap;
{
    register Region_BoxPtr     pNextRect;
    register int        x1;     /* left and right side of current union */
    register int        x2;

    assert (y1 < y2);
    assert(r1 != r1End && r2 != r2End);

    pNextRect = REGION_TOP(pReg);

    /* Start off current rectangle */
    if (r1->x1 < r2->x1)
    {
	x1 = r1->x1;
	x2 = r1->x2;
	r1++;
    }
    else
    {
	x1 = r2->x1;
	x2 = r2->x2;
	r2++;
    }
    while (r1 != r1End && r2 != r2End)
    {
	if (r1->x1 < r2->x1) MERGERECT(r1) else MERGERECT(r2);
    }

    /* Finish off whoever (if any) is left */
    if (r1 != r1End)
    {
	do
	{
	    MERGERECT(r1);
	} while (r1 != r1End);
    }
    else if (r2 != r2End)
    {
	do
	{
	    MERGERECT(r2);
	} while (r2 != r2End);
    }
    
    /* Add current rectangle */
    NEWRECT(pReg, pNextRect, x1, y1, x2, y2);

    return True;
}

static void Region_Union_m(Region_clp self,
			   Region_T newReg, 
			   Region_T reg1, 
			   Region_T reg2)
{
    bool_t overlap; /* result ignored */

    good(reg1);
    good(reg2);
    good(newReg);
    /*  checks all the simple cases */

    /*
     * Region 1 and 2 are the same
     */
    if (reg1 == reg2)
    {
	Region_Copy_m(NULL, newReg, reg1);
	return;
    }

    /*
     * Region 1 is empty
     */
    if (REGION_NIL(reg1))
    {
        if (newReg != reg2)
	    Region_Copy_m(NULL, newReg, reg2);

	return;
    }

    /*
     * Region 2 is empty
     */
    if (REGION_NIL(reg2))
    {
        if (newReg != reg1)
	    Region_Copy_m(NULL, newReg, reg1);

        return;
    }

    /*
     * Region 1 completely subsumes region 2
     */
    if (!reg1->data && SUBSUMES(&reg1->extents, &reg2->extents))
    {
        if (newReg != reg1)
	    Region_Copy_m(NULL, newReg, reg1);

        return;
    }

    /*
     * Region 2 completely subsumes region 1
     */
    if (!reg2->data && SUBSUMES(&reg2->extents, &reg1->extents))
    {
        if (newReg != reg2)
	    Region_Copy_m(NULL, newReg, reg2);

        return;
    }

    if (!miRegionOp(newReg, reg1, reg2, miUnionO, True, True, &overlap))
	return;

    newReg->extents.x1 = min(reg1->extents.x1, reg2->extents.x1);
    newReg->extents.y1 = min(reg1->extents.y1, reg2->extents.y1);
    newReg->extents.x2 = max(reg1->extents.x2, reg2->extents.x2);
    newReg->extents.y2 = max(reg1->extents.y2, reg2->extents.y2);
    good(newReg);
    return;
}


/*======================================================================
 *	    Batch Rectangle Union
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miRegionAppend --
 * 
 *      "Append" the rgn rectangles onto the end of dstrgn, maintaining
 *      knowledge of YX-banding when it's easy.  Otherwise, dstrgn just
 *      becomes a non-y-x-banded random collection of rectangles, and not
 *      yet a True region.  After a sequence of appends, the caller must
 *      call miRegionValidate to ensure that a valid region is constructed.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *      dstrgn is modified if rgn has rectangles.
 *
 */
static void Region_Append_m(Region_clp self, Region_T dstrgn, Region_T rgn)
{
    int numRects, dnumRects, size;
    Region_BoxPtr new, old;
    bool_t prepend;

    if (!rgn->data && (dstrgn->data == (Region_Data *)&EmptyData))
    {
	dstrgn->extents = rgn->extents;
	dstrgn->data = (Region_DataPtr)NULL;
	return;
    }

    numRects = REGION_NUM_RECTS(rgn);
    if (!numRects)
	return;
    prepend = False;
    size = numRects;
    dnumRects = REGION_NUM_RECTS(dstrgn);
    if (!dnumRects && (size < 200))
	size = 200; /* XXX pick numbers out of a hat */
    RECTALLOC(dstrgn, size);
    old = REGION_RECTS(rgn);
    if (!dnumRects)
	dstrgn->extents = rgn->extents;
    else if (dstrgn->extents.x2 > dstrgn->extents.x1)
    {
	register Region_BoxPtr first, last;

	first = old;
	last = REGION_BOXPTR(dstrgn) + (dnumRects - 1);
	if ((first->y1 > last->y2) ||
	    ((first->y1 == last->y1) && (first->y2 == last->y2) &&
	     (first->x1 > last->x2)))
	{
	    if (rgn->extents.x1 < dstrgn->extents.x1)
		dstrgn->extents.x1 = rgn->extents.x1;
	    if (rgn->extents.x2 > dstrgn->extents.x2)
		dstrgn->extents.x2 = rgn->extents.x2;
	    dstrgn->extents.y2 = rgn->extents.y2;
	}
	else
	{
	    first = REGION_BOXPTR(dstrgn);
	    last = old + (numRects - 1);
	    if ((first->y1 > last->y2) ||
		((first->y1 == last->y1) && (first->y2 == last->y2) &&
		 (first->x1 > last->x2)))
	    {
		prepend = True;
		if (rgn->extents.x1 < dstrgn->extents.x1)
		    dstrgn->extents.x1 = rgn->extents.x1;
		if (rgn->extents.x2 > dstrgn->extents.x2)
		    dstrgn->extents.x2 = rgn->extents.x2;
		dstrgn->extents.y1 = rgn->extents.y1;
	    }
	    else
		dstrgn->extents.x2 = dstrgn->extents.x1;
	}
    }
    if (prepend)
    {
	new = REGION_BOX(dstrgn, numRects);
	if (dnumRects == 1)
	    *new = *REGION_BOXPTR(dstrgn);
	else
	    bcopy((char *)REGION_BOXPTR(dstrgn), (char *)new,
		  dnumRects * sizeof(Region_Box));
	new = REGION_BOXPTR(dstrgn);
    }
    else
	new = REGION_BOXPTR(dstrgn) + dnumRects;
    if (numRects == 1)
	*new = *old;
    else
	bcopy((char *)old, (char *)new, numRects * sizeof(Region_Box));
    dstrgn->data->numRects += numRects;
    return;
}

   
#define ExchangeRects(a, b) \
{			    \
    Region_Box     t;	    \
    t = rects[a];	    \
    rects[a] = rects[b];    \
    rects[b] = t;	    \
}

static void
QuickSortRects(rects, numRects)
    register Region_Box     rects[];
    register int        numRects;
{
    register int	y1;
    register int	x1;
    register int        i, j;
    register Region_BoxPtr     r;

    /* Always called with numRects > 1 */

    do
    {
	if (numRects == 2)
	{
	    if (rects[0].y1 > rects[1].y1 ||
		    (rects[0].y1 == rects[1].y1 && rects[0].x1 > rects[1].x1))
		ExchangeRects(0, 1);
	    return;
	}

	/* Choose partition element, stick in location 0 */
        ExchangeRects(0, numRects >> 1);
	y1 = rects[0].y1;
	x1 = rects[0].x1;

        /* Partition array */
        i = 0;
        j = numRects;
        do
	{
	    r = &(rects[i]);
	    do
	    {
		r++;
		i++;
            } while (i != numRects &&
		     (r->y1 < y1 || (r->y1 == y1 && r->x1 < x1)));
	    r = &(rects[j]);
	    do
	    {
		r--;
		j--;
            } while (y1 < r->y1 || (y1 == r->y1 && x1 < r->x1));
            if (i < j)
		ExchangeRects(i, j);
        } while (i < j);

        /* Move partition element back to middle */
        ExchangeRects(0, j);

	/* Recurse */
        if (numRects-j-1 > 1)
	    QuickSortRects(&rects[j+1], numRects-j-1);
        numRects = j;
    } while (numRects > 1);
}

/*-
 *-----------------------------------------------------------------------
 * miRegionValidate --
 * 
 *      Take a ``region'' which is a non-y-x-banded random collection of
 *      rectangles, and compute a nice region which is the union of all the
 *      rectangles.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *      The passed-in ``region'' may be modified.
 *	pOverlap set to True if any retangles overlapped, else False;
 *
 * Strategy:
 *      Step 1. Sort the rectangles into ascending order with primary key y1
 *		and secondary key x1.
 *
 *      Step 2. Split the rectangles into the minimum number of proper y-x
 *		banded regions.  This may require horizontally merging
 *		rectangles, and vertically coalescing bands.  With any luck,
 *		this step in an identity tranformation (ala the Box widget),
 *		or a coalescing into 1 box (ala Menus).
 *
 *	Step 3. Merge the separate regions down to a single region by calling
 *		miUnion.  Maximize the work each miUnion call does by using
 *		a binary merge.
 *
 *-----------------------------------------------------------------------
 */

static void Region_MakeValid_m(Region_clp self, 
				 Region_T badreg, 
				 bool_t *pOverlap)
{
    /* Descriptor for regions under construction  in Step 2. */
    typedef struct {
	Region_Rec   reg;
	int	    prevBand;
	int	    curBand;
    } RegionInfo;

	     int	numRects;   /* Original numRects for badreg	    */
	     RegionInfo *ri;	    /* Array of current regions		    */
    	     int	numRI;      /* Number of entries used in ri	    */
	     int	sizeRI;	    /* Number of entries available in ri    */
	     int	i;	    /* Index into rects			    */
    register int	j;	    /* Index into ri			    */
    register RegionInfo *rit;       /* &ri[j]				    */
    register Region_T  reg;        /* ri[j].reg			    */
    register Region_BoxPtr	box;	    /* Current box in rects		    */
    register Region_BoxPtr	riBox;      /* Last box in ri[j].reg		    */
    register Region_T  hreg;       /* ri[j_half].reg			    */

    *pOverlap = False;
    if (!badreg->data)
    {
	good(badreg);
	return;
    }
    numRects = badreg->data->numRects;
    if (!numRects)
    {
	good(badreg);
	return;
    }
    if (badreg->extents.x1 < badreg->extents.x2)
    {
	if ((numRects) == 1)
	{
	    xfreeData(badreg);
	    badreg->data = (Region_DataPtr) NULL;
	}
	else
	{
	    DOWNSIZE(badreg, numRects);
	}
	good(badreg);
	return;
    }

    /* Step 1: Sort the rects array into ascending (y1, x1) order */
    QuickSortRects(REGION_BOXPTR(badreg), numRects);

    /* Step 2: Scatter the sorted array into the minimum number of regions */

    /* Set up the first region to be the first rectangle in badreg */
    /* Note that step 2 code will never overflow the ri[0].reg rects array */
    ri = (RegionInfo *) xalloc(4 * sizeof(RegionInfo));
    sizeRI = 4;
    numRI = 1;
    ri[0].prevBand = 0;
    ri[0].curBand = 0;
    ri[0].reg = *badreg;
    box = REGION_BOXPTR(&ri[0].reg);
    ri[0].reg.extents = *box;
    ri[0].reg.data->numRects = 1;

    /* Now scatter rectangles into the minimum set of valid regions.  If the
       next rectangle to be added to a region would force an existing rectangle
       in the region to be split up in order to maintain y-x banding, just
       forget it.  Try the next region.  If it doesn't fit cleanly into any
       region, make a new one. */

    for (i = numRects; --i > 0;)
    {
	box++;
	/* Look for a region to append box to */
	for (j = numRI, rit = ri; --j >= 0; rit++)
	{
	    reg = &rit->reg;
	    riBox = REGION_END(reg);

	    if (box->y1 == riBox->y1 && box->y2 == riBox->y2)
	    {
		/* box is in same band as riBox.  Merge or append it */
		if (box->x1 <= riBox->x2)
		{
		    /* Merge it with riBox */
		    if (box->x1 < riBox->x2) *pOverlap = True;
		    if (box->x2 > riBox->x2) riBox->x2 = box->x2;
		}
		else
		{
		    RECTALLOC(reg, 1);
		    *REGION_TOP(reg) = *box;
		    reg->data->numRects++;
		}
		goto NextRect;   /* So sue me */
	    }
	    else if (box->y1 >= riBox->y2)
	    {
		/* Put box into new band */
		if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
		if (reg->extents.x1 > box->x1)   reg->extents.x1 = box->x1;
		Coalesce(reg, rit->prevBand, rit->curBand);
		rit->curBand = reg->data->numRects;
		RECTALLOC(reg, 1);
		*REGION_TOP(reg) = *box;
		reg->data->numRects++;
		goto NextRect;
	    }
	    /* Well, this region was inappropriate.  Try the next one. */
	} /* for j */

	/* Uh-oh.  No regions were appropriate.  Create a new one. */
	if (sizeRI == numRI)
	{
	    /* Oops, allocate space for new region information */
	    sizeRI <<= 1;
	    ri = (RegionInfo *) xrealloc(ri, sizeRI * sizeof(RegionInfo));
	    rit = &ri[numRI];
	}
	numRI++;
	rit->prevBand = 0;
	rit->curBand = 0;
	rit->reg.extents = *box;
	rit->reg.data = (Region_DataPtr)NULL;
	miRectAlloc(&rit->reg, (i+numRI) / numRI); /* MUST force allocation */
NextRect: ;
    } /* for i */

    /* Make a final pass over each region in order to Coalesce and set
       extents.x2 and extents.y2 */

    for (j = numRI, rit = ri; --j >= 0; rit++)
    {
	reg = &rit->reg;
	riBox = REGION_END(reg);
	reg->extents.y2 = riBox->y2;
	if (reg->extents.x2 < riBox->x2) reg->extents.x2 = riBox->x2;
	Coalesce(reg, rit->prevBand, rit->curBand);
	if (reg->data->numRects == 1) /* keep unions happy below */
	{
	    xfreeData(reg);
	    reg->data = (Region_DataPtr)NULL;
	}
    }

    /* Step 3: Union all regions into a single region */
    while (numRI > 1)
    {
	int half = numRI/2;
	for (j = numRI & 1; j < (half + (numRI & 1)); j++)
	{
	    reg = &ri[j].reg;
	    hreg = &ri[j+half].reg;
	    miRegionOp(reg, reg, hreg, miUnionO, True, True, pOverlap);
	    if (hreg->extents.x1 < reg->extents.x1)
		reg->extents.x1 = hreg->extents.x1;
	    if (hreg->extents.y1 < reg->extents.y1)
		reg->extents.y1 = hreg->extents.y1;
	    if (hreg->extents.x2 > reg->extents.x2)
		reg->extents.x2 = hreg->extents.x2;
	    if (hreg->extents.y2 > reg->extents.y2)
		reg->extents.y2 = hreg->extents.y2;
	    xfreeData(hreg);
	}
	numRI -= half;
    }
    *badreg = ri[0].reg;
    xfree(ri);
    good(badreg);
    return;
}


/*======================================================================
 * 	    	  Region Subtraction
 *====================================================================*/


/*-
 *-----------------------------------------------------------------------
 * miSubtractO --
 *	Overlapping band subtraction. x1 is the left-most point not yet
 *	checked.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *	pReg may have rectangles added to it.
 *
 *-----------------------------------------------------------------------
 */
/*ARGSUSED*/
static bool_t
miSubtractO (pReg, r1, r1End, r2, r2End, y1, y2, pOverlap)
    register Region_T	pReg;
    register Region_BoxPtr	r1;
    Region_BoxPtr  	  	r1End;
    register Region_BoxPtr	r2;
    Region_BoxPtr  	  	r2End;
    register int  	y1;
             int  	y2;
    bool_t		*pOverlap;
{
    register Region_BoxPtr	pNextRect;
    register int  	x1;

    x1 = r1->x1;
    
    assert(y1<y2);
    assert(r1 != r1End && r2 != r2End);

    pNextRect = REGION_TOP(pReg);

    do
    {
	if (r2->x2 <= x1)
	{
	    /*
	     * Subtrahend entirely to left of minuend: go to next subtrahend.
	     */
	    r2++;
	}
	else if (r2->x1 <= x1)
	{
	    /*
	     * Subtrahend preceeds minuend: nuke left edge of minuend.
	     */
	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend completely covered: advance to next minuend and
		 * reset left fence to edge of new minuend.
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend now used up since it doesn't extend beyond
		 * minuend
		 */
		r2++;
	    }
	}
	else if (r2->x1 < r1->x2)
	{
	    /*
	     * Left part of subtrahend covers part of minuend: add uncovered
	     * part of minuend to region and skip to next subtrahend.
	     */
	    assert(x1<r2->x1);
	    NEWRECT(pReg, pNextRect, x1, y1, r2->x1, y2);

	    x1 = r2->x2;
	    if (x1 >= r1->x2)
	    {
		/*
		 * Minuend used up: advance to new...
		 */
		r1++;
		if (r1 != r1End)
		    x1 = r1->x1;
	    }
	    else
	    {
		/*
		 * Subtrahend used up
		 */
		r2++;
	    }
	}
	else
	{
	    /*
	     * Minuend used up: add any remaining piece before advancing.
	     */
	    if (r1->x2 > x1)
		NEWRECT(pReg, pNextRect, x1, y1, r1->x2, y2);
	    r1++;
	    if (r1 != r1End)
		x1 = r1->x1;
	}
    } while ((r1 != r1End) && (r2 != r2End));


    /*
     * Add remaining minuend rectangles to region.
     */
    while (r1 != r1End)
    {
	assert(x1<r1->x2);
	NEWRECT(pReg, pNextRect, x1, y1, r1->x2, y2);
	r1++;
	if (r1 != r1End)
	    x1 = r1->x1;
    }
    return True;
}
	
/*-
 *-----------------------------------------------------------------------
 * miSubtract --
 *	Subtract regS from regM and leave the result in regD.
 *	S stands for subtrahend, M for minuend and D for difference.
 *
 * Results:
 *	True if successful.
 *
 * Side Effects:
 *	regD is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void Region_Subtract_m(Region_clp self, 
			      Region_T regD,
			      Region_T regM,
			      Region_T regS)
{
    bool_t overlap; /* result ignored */

    good(regM);
    good(regS);
    good(regD);
   /* check for trivial rejects */
    if (REGION_NIL(regM) || REGION_NIL(regS) ||
	!EXTENTCHECK(&regM->extents, &regS->extents))
    {
	Region_Copy_m(NULL, regD, regM);
	return;
    }
    else if (regM == regS)
    {

	MAKE_EMPTY(regD);
	return;
    }
 
    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    if (!miRegionOp(regD, regM, regS, miSubtractO, True, False, &overlap))
	return;

    /*
     * Can't alter RegD's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(regD);
    good(regD);
    return;
}

/*======================================================================
 *	    Region Inversion
 *====================================================================*/

/*-
 *-----------------------------------------------------------------------
 * miInverse --
 *	Take a region and a box and return a region that is everything
 *	in the box but not in the region. The careful reader will note
 *	that this is the same as subtracting the region from the box...
 *
 * Results:
 *	True.
 *
 * Side Effects:
 *	newReg is overwritten.
 *
 *-----------------------------------------------------------------------
 */
static void Region_Inverse_m(Region_clp self, Region_T newReg, 
			     Region_T reg1, Region_BoxPtr invRect)
{
    Region_Rec	  invReg;   	/* Quick and dirty region made from the
				 * bounding box */
    bool_t	  overlap;	/* result ignored */

    good(reg1);
    good(newReg);
   /* check for trivial rejects */
    if (REGION_NIL(reg1) || !EXTENTCHECK(invRect, &reg1->extents))
    {
	newReg->extents = *invRect;
	xfreeData(newReg);
	newReg->data = (Region_DataPtr)NULL;
        return;
    }

    /* Add those rectangles in region 1 that aren't in region 2,
       do yucky substraction for overlaps, and
       just throw away rectangles in region 2 that aren't in region 1 */
    invReg.extents = *invRect;
    invReg.data = (Region_DataPtr)NULL;
    if (!miRegionOp(newReg, &invReg, reg1, miSubtractO, True, False, &overlap))
	return;

    /*
     * Can't alter newReg's extents before we call miRegionOp because
     * it might be one of the source regions and miRegionOp depends
     * on the extents of those regions being unaltered. Besides, this
     * way there's no checking against rectangles that will be nuked
     * due to coalescing, so we have to examine fewer rectangles.
     */
    miSetExtents(newReg);
    good(newReg);
    return;
}

/*
 *   RectIn(region, rect)
 *   This routine takes a pointer to a region and a pointer to a box
 *   and determines if the box is outside/inside/partly inside the region.
 *
 *   The idea is to travel through the list of rectangles trying to cover the
 *   passed box with them. Anytime a piece of the rectangle isn't covered
 *   by a band of rectangles, partOut is set True. Any time a rectangle in
 *   the region covers part of the box, partIn is set True. The process ends
 *   when either the box has been completely covered (we reached a band that
 *   doesn't overlap the box, partIn is True and partOut is False), the
 *   box has been partially covered (partIn == partOut == True -- because of
 *   the banding, the first time this is True we know the box is only
 *   partially in the region) or is outside the region (we reached a band
 *   that doesn't overlap the box at all and partIn is False)
 */

static Region_Overlap Region_TestRect_m(Region_clp self, Region_T region,
					Region_BoxPtr prect)
{
    register int	x;
    register int	y;
    register Region_BoxPtr     pbox;
    register Region_BoxPtr     pboxEnd;
    int			partIn, partOut;
    int			numRects;

    good(region);
    numRects = REGION_NUM_RECTS(region);
    /* useful optimization */
    if (!numRects || !EXTENTCHECK(&region->extents, prect))
        return(Region_Overlap_Out);

    if (numRects == 1)
    {
	/* We know that it must be rgnIN or rgnPART */
	if (SUBSUMES(&region->extents, prect))
	    return(Region_Overlap_In);
	else
	    return(Region_Overlap_Partial);
    }

    partOut = False;
    partIn = False;

    /* (x,y) starts at upper left of rect, moving to the right and down */
    x = prect->x1;
    y = prect->y1;

    /* can stop when both partOut and partIn are True, or we reach prect->y2 */
    for (pbox = REGION_BOXPTR(region), pboxEnd = pbox + numRects;
         pbox != pboxEnd;
         pbox++)
    {

        if (pbox->y2 <= y)
           continue;    /* getting up to speed or skipping remainder of band */

        if (pbox->y1 > y)
        {
           partOut = True;      /* missed part of rectangle above */
           if (partIn || (pbox->y1 >= prect->y2))
              break;
           y = pbox->y1;        /* x guaranteed to be == prect->x1 */
        }

        if (pbox->x2 <= x)
           continue;            /* not far enough over yet */

        if (pbox->x1 > x)
        {
           partOut = True;      /* missed part of rectangle to left */
           if (partIn)
              break;
        }

        if (pbox->x1 < prect->x2)
        {
            partIn = True;      /* definitely overlap */
            if (partOut)
               break;
        }

        if (pbox->x2 >= prect->x2)
        {
           y = pbox->y2;        /* finished with this band */
           if (y >= prect->y2)
              break;
           x = prect->x1;       /* reset x out to left again */
        }
	else
	{
	    /*
	     * Because boxes in a band are maximal width, if the first box
	     * to overlap the rectangle doesn't completely cover it in that
	     * band, the rectangle must be partially out, since some of it
	     * will be uncovered in that band. partIn will have been set True
	     * by now...
	     */
	    partOut = True;
	    break;
	}
    }

    return(partIn ? ((y < prect->y2) ? Region_Overlap_Partial : 
		     Region_Overlap_In) : Region_Overlap_Out);
}

/* TranslateRegion(pReg, x, y)
   translates in place
*/

static void Region_Translate_m(Region_clp self, Region_T pReg, int x, int y)
{
    int x1, x2, y1, y2;
    register int nbox;
    register Region_BoxPtr pbox;

    good(pReg);
    pReg->extents.x1 = x1 = pReg->extents.x1 + x;
    pReg->extents.y1 = y1 = pReg->extents.y1 + y;
    pReg->extents.x2 = x2 = pReg->extents.x2 + x;
    pReg->extents.y2 = y2 = pReg->extents.y2 + y;
    if (((x1 - MINSHORT)|(y1 - MINSHORT)|(MAXSHORT - x2)|(MAXSHORT - y2)) >= 0)
    {
	if (pReg->data && (nbox = pReg->data->numRects))
	{
	    for (pbox = REGION_BOXPTR(pReg); nbox--; pbox++)
	    {
		pbox->x1 += x;
		pbox->y1 += y;
		pbox->x2 += x;
		pbox->y2 += y;
	    }
	}
	return;
    }
    if (((x2 - MINSHORT)|(y2 - MINSHORT)|(MAXSHORT - x1)|(MAXSHORT - y1)) <= 0)
    {
	pReg->extents.x2 = pReg->extents.x1;
	pReg->extents.y2 = pReg->extents.y1;
	xfreeData(pReg);
	pReg->data = (Region_Data *)&EmptyData;
	return;
    }
    if (x1 < MINSHORT)
	pReg->extents.x1 = MINSHORT;
    else if (x2 > MAXSHORT)
	pReg->extents.x2 = MAXSHORT;
    if (y1 < MINSHORT)
	pReg->extents.y1 = MINSHORT;
    else if (y2 > MAXSHORT)
	pReg->extents.y2 = MAXSHORT;
    if (pReg->data && (nbox = pReg->data->numRects))
    {
	register Region_BoxPtr pboxout;

	for (pboxout = pbox = REGION_BOXPTR(pReg); nbox--; pbox++)
	{
	    pboxout->x1 = x1 = pbox->x1 + x;
	    pboxout->y1 = y1 = pbox->y1 + y;
	    pboxout->x2 = x2 = pbox->x2 + x;
	    pboxout->y2 = y2 = pbox->y2 + y;
	    if (((x2 - MINSHORT)|(y2 - MINSHORT)|
		 (MAXSHORT - x1)|(MAXSHORT - y1)) <= 0)
	    {
		pReg->data->numRects--;
		continue;
	    }
	    if (x1 < MINSHORT)
		pboxout->x1 = MINSHORT;
	    else if (x2 > MAXSHORT)
		pboxout->x2 = MAXSHORT;
	    if (y1 < MINSHORT)
		pboxout->y1 = MINSHORT;
	    else if (y2 > MAXSHORT)
		pboxout->y2 = MAXSHORT;
	    pboxout++;
	}
	if (pboxout != pbox)
	{
	    if (pReg->data->numRects == 1)
	    {
		pReg->extents = *REGION_BOXPTR(pReg);
		xfreeData(pReg);
		pReg->data = (Region_DataPtr)NULL;
	    }
	    else
		miSetExtents(pReg);
	}
    }
}

static void Region_SetBox_m(Region_clp self, Region_T pReg, Region_BoxPtr pBox)
{
    good(pReg);
    assert(pBox->x1<=pBox->x2);
    assert(pBox->y1<=pBox->y2);
    pReg->extents = *pBox;
    xfreeData(pReg);
    pReg->data = (Region_DataPtr)NULL;
}

static void Region_SetXY_m(Region_clp self, Region_T pReg,
			   int x1, int y1, int x2, int y2) {
    good(pReg);
    assert(x1 <= x2);
    assert(y1 <= y2);
    pReg->extents.x1 = x1;
    pReg->extents.y1 = y1;
    pReg->extents.x2 = x2;
    pReg->extents.y2 = y2;
    xfreeData(pReg);
    pReg->data = (Region_DataPtr)NULL;
}

static bool_t Region_TestPoint_m(Region_clp self, Region_T pReg, 
				 int x, int y)
{
    register Region_BoxPtr pbox, pboxEnd;
    int numRects;

    good(pReg);
    numRects = REGION_NUM_RECTS(pReg);
    if (!numRects || !INBOX(&pReg->extents, x, y))
        return(False);
    if (numRects == 1)
    {
	return(True);
    }
    for (pbox = REGION_BOXPTR(pReg), pboxEnd = pbox + numRects;
	 pbox != pboxEnd;
	 pbox++)
    {
        if (y >= pbox->y2)
	   continue;		/* not there yet */
	if ((y < pbox->y1) || (x < pbox->x1))
	   break;		/* missed it */
	if (x >= pbox->x2)
	   continue;		/* not there yet */
	return(True);
    }
    return(False);
}

static bool_t Region_IsEmpty_m(Region_clp self, Region_T pReg)
{
    good(pReg);
    return(REGION_NIL(pReg));
}


static void Region_MakeEmpty_m(Region_clp self, Region_T pReg)
{
    good(pReg);
    
    MAKE_EMPTY(pReg);
}

static Region_BoxPtr Region_Extents_m(Region_clp self, Region_T pReg)
{
    good(pReg);
    return(&pReg->extents);
}

#define ExchangeSpans(a, b)				    \
{							    \
    DDXPointRec     tpt;				    \
    register int    tw;					    \
							    \
    tpt = spans[a]; spans[a] = spans[b]; spans[b] = tpt;    \
    tw = widths[a]; widths[a] = widths[b]; widths[b] = tw;  \
}


#define NextBand()						    \
{								    \
    clipy1 = pboxBandStart->y1;					    \
    clipy2 = pboxBandStart->y2;					    \
    pboxBandEnd = pboxBandStart + 1;				    \
    while (pboxBandEnd != pboxLast && pboxBandEnd->y1 == clipy1) {  \
	pboxBandEnd++;						    \
    }								    \
    for (; ppt != pptLast && ppt->y < clipy1; ppt++, pwidth++) {} \
}

/*
    Clip a list of scanlines to a region.  The caller has allocated the
    space.  FSorted is non-zero if the scanline origins are in ascending
    order.
    returns the number of new, clipped scanlines.
*/

