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
** ID : $Id: display.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <salloc.h>
#include <IOMacros.h>
#include <CRendDisplay.ih>
#include <stdio.h>
#include "CRendDisplay_st.h"
#include "CRend_st.h"
#define TRC(_x)


/*
 * Module stuff
 */
static  CRendDisplay_GetDisplayHeight_fn  CRendDisplay_GetDisplayHeight_m;
static  CRendDisplay_GetDisplayWidth_fn   CRendDisplay_GetDisplayWidth_m;
static  CRendDisplay_GetWS_fn             CRendDisplay_GetWS_m;
static  CRendDisplay_Close_fn             CRendDisplay_Close_m;
static  CRendDisplay_CreateWindow_fn      CRendDisplay_CreateWindow_m;
static  CRendDisplay_EventsPending_fn     CRendDisplay_EventsPending_m;
static  CRendDisplay_NextEvent_fn         CRendDisplay_NextEvent_m;

CRendDisplay_op CRendDisplay_ms = {
  CRendDisplay_GetDisplayHeight_m,
  CRendDisplay_GetDisplayWidth_m,
  CRendDisplay_GetWS_m,
  CRendDisplay_Close_m,
  CRendDisplay_CreateWindow_m,
  CRendDisplay_EventsPending_m,
  CRendDisplay_NextEvent_m
};



/*---------------------------------------------------- Entry Points ----*/

static uint32_t 
CRendDisplay_GetDisplayHeight_m (
	CRendDisplay_cl	*self )
{
  CRendDisplay_st	*st = (CRendDisplay_st *) self->st;
  return st->height;
}

static uint32_t 
CRendDisplay_GetDisplayWidth_m (
	CRendDisplay_cl	*self )
{
  CRendDisplay_st	*st = (CRendDisplay_st *) self->st;
  return st->width;
}

static WS_clp CRendDisplay_GetWS_m( CRendDisplay_clp self) {
    
    CRendDisplay_st	*st = (CRendDisplay_st *) self->st;
    return st->ws;
}
    

static void 
CRendDisplay_Close_m (
	CRendDisplay_cl	*self )
{
    CRendDisplay_st	*st = (CRendDisplay_st *) self->st;
    IDCClientBinding$Destroy(st->binding);
    FREE(st);
}

static CRend_clp 
CRendDisplay_CreateWindow_m (
	CRendDisplay_cl	*self,
	uint32_t	_x	/* IN */,
	uint32_t	_y	/* IN */,
	uint32_t	_width	/* IN */,
	uint32_t	_height	/* IN */ )
{
    uint32_t NOCLOBBER x = _x;
    uint32_t NOCLOBBER y = _y;
    uint32_t NOCLOBBER width = _width;
    uint32_t NOCLOBBER height = _height;
    
    CRendDisplay_st	*st = (CRendDisplay_st *) self->st;
    IOOffer_clp offer;		/* FB IDC channel offer */
    CRend_st *     NOCLOBBER  w = NULL;

    extern CRend_op CRend_ms;
    StretchAllocator_clp  salloc;
    FB_Protocol NOCLOBBER proto = 0;
    bool_t NOCLOBBER success = False;
    
    TRC(fprintf(stderr,"WSCreateWindow(%#lx, %d, %d, %d, %d)\n", 
		st, x, y, width, height));
    
    salloc = Pvs(salloc);
    
    /* Alignment/size constraints */
    
    x &= ~(st->xgrid);
    width = (width+st->xgrid) & (~st->xgrid);		
    y &= (~st->ygrid);
    height = ((height+st->ygrid) & (~st->ygrid));		

    TRY {
    
	/* Create a window record */
	w = Heap$Malloc(Pvs(heap), sizeof(*w));
	if (!w) RAISE_CRend$NoResources();
	
	memset(w, 0, sizeof(*w));
	CL_INIT(w->cl, &CRend_ms, w);

	/* Fill in */
	w->display = st;
	w->x       = x;
	w->y       = y;
	w->width   = width;
	w->height  = height;
	w->stride  = (width + st->xgrid - 1) & ~(st->xgrid - 1);	
	w->ownpixdata = 1;
	w->xgrid   = st->xgrid;
	w->ygrid   = st->ygrid;
	w->plot_mode = CRend_PlotMode_Normal;
	

	
	/* Create a local pixel buffer */
	{
	    Stretch_Size          req_size, size;
	    int i;

	    req_size = w->height * w->stride * sizeof(pixel_t);
	    TRC(fprintf(stderr,"Doing salloc for %d\n", w->height*w->stride));
	    w->pixstr  = STR_NEW_SALLOC(salloc, req_size);

	    if (!w->pixstr)
	    {
		fprintf(stderr, "Can't allocate pixel stretch\n");
		RAISE_CRend$NoResources();
	    }
	
	    w->pixels  = (pixel_t *) STR_RANGE(w->pixstr, &size);
	    if (!w->pixels || size < req_size)
	    {
		fprintf(stderr,"Stretch bogus");

		RAISE_CRend$NoResources();
	    }

	    STR_SETGLOBAL(w->pixstr, SET_ELEM(Stretch_Right_Read) );
	    
	    w->dirty = Heap$Malloc(Pvs(heap), sizeof(dirty_range) * w->height);
	    if(!w->dirty) {
		fprintf(stderr, "Can't allocate dirty block\n");
		RAISE_CRend$NoResources();
	    }
	    
	    for(i = 0; i<w->height; i++) {
		MARK_CLEAN(w, i);
	    }
	    
	    TRC(fprintf(stderr,"WSCreateWindow: Pixels at %p size %lx\n", 
			w->pixels, size));
	    
	    memset(w->pixels, 0, size);

	}
	
    
	/* Try to create a window */
	w->id = WS$CreateWindow(st->ws, x, y, width, height);


	TRC(fprintf(stderr,"WSCreateWindow: WS_WindowID %x\n", w->id));

	switch(st->depth) {
	    
	case 8:
	    if(PIXEL_SIZE != 8) {
		fprintf(stderr, 
			"CreateWindow:"
			" CRend compiled for wrong pixel size\n");
		RAISE_CRend$NoResources();
	    }
	    proto = FB_Protocol_Bitmap8;
	    break;

	case 16:
	    if(PIXEL_SIZE != 16) {
		fprintf(stderr, 
			"CreateWindow:"
			" CRend compiled for wrong pixel size\n");
		RAISE_CRend$NoResources();
	    }
	    proto = FB_Protocol_Bitmap16;
	    break;

	default:
	    fprintf(stderr,
		    "CreateWindow: Unknown pixel size\n");
	    RAISE_CRend$NoResources();
	    break;
	}
	    
	/* Ask for an update stream */
	w->sid = WS$UpdateStream(st->ws, w->id, proto, 
				 0, True, &offer, &w->blit);
	
	TRC(fprintf(stderr,"WSCreateWindow Stream offer %p\n", offer));
	
	/* Try to connect to framestore */
	w->fbio = NULL;

	if(offer) {
	    TRY {
		w->fbio = IO_BIND(offer);  
		
		w->num_ioslots = IO$QueryDepth(w->fbio) / 2;
	    } CATCH_TypeSystem$Incompatible() {
	    } ENDTRY;
	}

	if(!(w->fbio || w->blit)) {
	    /* Oh dear, we can't bind to either of our fb offers */

	    RAISE_WS$Failure();
	}
	TRC(fprintf(stderr,"WSCreateWindow: Bound to update stream\n"));
	
	TRC(fprintf(stderr,"w->fbio = %x\n", w->fbio));

	CRend$SetForeground(&w->cl, 0xffffffff);
	CRend$SetBackground(&w->cl, 0);

	success = True;
	
    } FINALLY {
	
	if(!success) {
	    if(w) {
		if(w->dirty) FREE(w->dirty);
		if(w->pixstr) {
		    STR_DESTROY_SALLOC(salloc, w->pixstr);
		}
		if(w->fbio) {
		    IO$Dispose(w->fbio);
		}

		if(w->id) {
		    WS$DestroyWindow(st->ws, w->id);
		}
	    }
	}
    } ENDTRY;

    
    TRC(fprintf(stderr,"Done\n"));
    return &w->cl;
}


static bool_t 
CRendDisplay_EventsPending_m (
        CRendDisplay_cl *self )
{
  CRendDisplay_st       *st = (CRendDisplay_st *) self->st;
  IO_Rec rec;
  word_t value;
  uint32_t nrecs;

   /* Already have an event? */
  if (st->ev) 
    return True;
  
  /* Can we get one? */
  if (IO$GetPkt(st->evio, 1, &rec, 0, &nrecs, &value)) {
    st->ev = (WSEvent *)rec.base;
    return True;
  }

  return False;
}

static void 
CRendDisplay_NextEvent_m (
        CRendDisplay_cl *self,
        CRendDisplay_Event_ptr  ev      /* IN */ )
{
  CRendDisplay_st       *st = (CRendDisplay_st *) self->st;
  IO_Rec   rec;
  word_t value;
  uint32_t nrecs; 

  /* If we have an event already then return it and send ack */
  if (st->ev) {

    *ev = *(st->ev);
    rec.base = st->ev;
    st->ev = NULL;
    rec.len  = sizeof(WSEvent);
    IO$PutPkt(st->evio, 1, &rec, 0, FOREVER);

  } else {   
    /* Await the next event */
    IO$GetPkt(st->evio, 1, &rec, FOREVER, &nrecs, &value);
    *ev = *(WSEvent *)rec.base;
    IO$PutPkt(st->evio, 1, &rec, 0, FOREVER);
  }
}


/*
 * End 
 */
