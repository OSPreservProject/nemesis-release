/* $XConsortium: regionstr.h,v 1.5 89/07/09 15:34:24 rws Exp $ */
/***********************************************************
Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.

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
#ifndef REGIONSTRUCT_H
#define REGIONSTRUCT_H

#include <Region.ih>


#define REGION_NIL(reg) ((reg)->data && !(reg)->data->numRects)
#define REGION_NUM_RECTS(reg) ((reg)->data ? (reg)->data->numRects : 1)
#define REGION_SIZE(reg) ((reg)->data ? (reg)->data->size : 0)
#define REGION_RECTS(reg) ((reg)->data ? (Region_BoxPtr)((reg)->data + 1) \
			               : &(reg)->extents)
#define REGION_BOXPTR(reg) ((Region_BoxPtr)((reg)->data + 1))
#define REGION_BOX(reg,i) (&REGION_BOXPTR(reg)[i])
#define REGION_TOP(reg) REGION_BOX(reg, (reg)->data->numRects)
#define REGION_END(reg) REGION_BOX(reg, (reg)->data->numRects - 1)
#define REGION_SZOF(n) (sizeof(Region_Data) + ((n) * sizeof(Region_Box)))

#if 0
extern Region_T miRegionCreate(Region_BoxPtr /*rect*/, int /*size*/);

extern void miRegionInit(
    Region_T /*pReg*/,
    Region_BoxPtr /*rect*/,
    int /*size*/
);

extern void miRegionDestroy(
    Region_T /*pReg*/
);

extern void miPrintRegion(Region_T pReg);

extern void miRegionUninit(
    Region_T /*pReg*/
);

extern Bool miRegionCopy(
    Region_T /*dst*/,
    Region_T /*src*/
);

extern Bool miIntersect(
    Region_T /*newReg*/,
    Region_T /*reg1*/,
    Region_T /*reg2*/
);

extern Bool miUnion(
    Region_T /*newReg*/,
    Region_T /*reg1*/,
    Region_T /*reg2*/
);

extern Bool miRegionAppend(
    Region_T /*dstrgn*/,
    Region_T /*rgn*/
);

extern Bool miRegionValidate(
    Region_T /*badreg*/,
    Bool * /*pOverlap*/
);

#if 0
extern Region_T miRectsToRegion(
    int /*nrects*/,
    xRectanglePtr /*prect*/,
    int /*ctype*/
);
#endif

extern Bool miSubtract(
    Region_T /*regD*/,
    Region_T /*regM*/,
    Region_T /*regS*/
);

extern Bool miInverse(
    Region_T /*newReg*/,
    Region_T /*reg1*/,
    Region_BoxPtr /*invRect*/
);

extern int miRectIn(
    Region_T /*region*/,
    Region_BoxPtr /*prect*/
);

extern void miTranslateRegion(
    Region_T /*pReg*/,
    int /*x*/,
    int /*y*/
);

extern void miRegionReset(
    Region_T /*pReg*/,
    Region_BoxPtr /*pBox*/
);

extern Bool miPointInRegion(
    Region_T /*pReg*/,
    int /*x*/,
    int /*y*/,
    Region_BoxPtr /*box*/
);

extern Bool miRegionNotEmpty(
    Region_T /*pReg*/
);

extern void miRegionEmpty(
    Region_T /*pReg*/
);

extern Region_BoxPtr miRegionExtents(
    Region_T /*pReg*/
);

#endif
#endif /* REGIONSTRUCT_H */
