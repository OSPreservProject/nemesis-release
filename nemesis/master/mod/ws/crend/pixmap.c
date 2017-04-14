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
** ID : $Id: pixmap.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>

#include <CRendPixmap.ih>
#include <stdio.h>
#include <stdlib.h>

#define TRC(x)

#include "CRend_st.h"

/*
 * Module stuff
 */
static	CRendPixmap_New_fn 		CRendPixmap_New_m;
static  CRendPixmap_NewPPM_fn           CRendPixmap_NewPPM_m;

static	CRendPixmap_op	ms = {
  CRendPixmap_New_m,
  CRendPixmap_NewPPM_m,
};

const static	CRendPixmap_cl	cl = { &ms, NULL };

CL_EXPORT (CRendPixmap, CRendPixmap, cl);


/*---------------------------------------------------- Entry Points ----*/

static CRend_clp
CRendPixmap_New_m (
	CRendPixmap_cl	*self,
	addr_t          data /* IN */,
	uint32_t	width	/* IN */,
	uint32_t	height	/* IN */,
	uint32_t	stride	/* IN */ )
{
    CRend_st *       w;
    extern CRend_op CRend_ms;
    /* Create a window record */
    w = Heap$Malloc(Pvs(heap), sizeof(*w));
    if (!w) RAISE_CRend$NoResources();

    memset(w, 0, sizeof(*w));

    CL_INIT(w->cl, &CRend_ms, w);

    w->display = 0;
    w->width = width;
    w->height = height;
    w->stride = stride;
    w->pixels = data;
    w->dirty = NULL;
    w->ownpixdata = 0;
    w->plot_mode = CRend_PlotMode_Normal;

    CRend$SetForeground(&w->cl, 0xffffffff);
    CRend$SetBackground(&w->cl, 0);
    return &w->cl;
}


static CRend_clp
CRendPixmap_NewPPM_m (
    CRendPixmap_cl *self,
    Rd_clp rd) {
    char buf[128];
    int sx,sy;
    uint8_t *databuffer;
    int px,py;
    void *memory;
    CRend_st *st;
    CRend_clp crend;
    Rd$GetLine(rd, buf, 128);
    
    if (!strcmp(buf, "P6")) {
	return NULL;
    }
    
    do {
	Rd$GetLine(rd, buf, 128);
    } while (buf[0] == '#');

    sscanf(buf, "%d %d", &sx, &sy);

    do {
	Rd$GetLine(rd, buf, 128); 
    } while (buf[0] == '#');

    memory = calloc(sx * sy, sizeof(int16_t));
    databuffer =calloc(sx*3,1);

    if (!memory || !databuffer) {
	RAISE_CRend$NoResources();
    }

    crend = CRendPixmap_New_m(self, memory, sx, sy, sx);
    st = (CRend_st *) crend->st;
    for (py = 0; py<sy; py++) {
	uint32_t *scrptr;
	uint32_t pack;
	char *bufptr;
	Rd$GetChars(rd, databuffer, sx*3);
		
	bufptr = databuffer;
	scrptr = (uint32_t *) (((int16_t *) memory) + (sx * py));

	for (px = 0; px<sx; px+=2) {
	    pack = 0;
		    
	    pack = ((*(bufptr++) & 248))<<7;     /* 10 - 3      */
	    pack += ((*(bufptr++) & 248))<<2;    /* 5 - 3       */
	    pack += ((*(bufptr++) & 248))>>3;    /* 0 - 3       */
	    pack += ((*(bufptr++) & 248))<<(23); /* 16 + 10 - 3 */
	    pack += ((*(bufptr++) & 248))<<(18); /* 16 + 5 - 3  */
	    pack += ((*(bufptr++) & 248))<<(13); /* 16 + 0 - 3  */
	    
	    *scrptr = pack;
		    
	    scrptr++;
	}
    }

    free(databuffer);

    st->ownpixdata = 2;
    return crend;
}
/*
 * End 
 */
