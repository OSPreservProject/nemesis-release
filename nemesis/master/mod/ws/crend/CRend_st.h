#ifndef CREND_ST
#define CREND_ST

#ifndef CRENDISPLAY_ST
#include "CRendDisplay_st.h"
#endif
#include <CRendDisplay.ih>
#include <Stretch.ih>
#include <IO.ih>
#include <WS.ih>
#include <FB.ih>
#include <FBBlit.ih>

/* Currently SRC IT uses 8 bit pixels, EB164 and IX86 use 16 bit
   pixels. This ought to be more flexible XXX */

#ifdef SRCIT
#define PIXEL_SIZE 8
#else
#define PIXEL_SIZE 16
#endif

#if (PIXEL_SIZE == 16)

typedef uint16_t pixel_t;

#endif

#if (PIXEL_SIZE == 8)

typedef uint8_t pixel_t;

#endif

typedef struct {
    word_t left;
    word_t right;
} dirty_range;

#define MARK_DIRTY(x, y)\
{\
    if(st->dirty) { \
	if(st->dirty[y].left > x) st->dirty[y].left = x;\
	if(st->dirty[y].right < x) st->dirty[y].right = x;\
    }\
}

#define MARK_DIRTY_RANGE(x1, x2, y)\
{\
    if(st->dirty) {\
        if(st->dirty[y].left > x1) st->dirty[y].left = x1;\
        if(st->dirty[y].right < x2) st->dirty[y].right = x2;\
    }\
}

#define MARK_ALL_DIRTY(_st)\
{\
    int y;\
    for(y = 0; y < _st->height; y++) {\
        _st->dirty[y].left = 0;\
        _st->dirty[y].right = st->width - 1;\
    }\
}

#define MARK_CLEAN(_st, y)\
{\
    _st->dirty[y].left = _st->width;\
    _st->dirty[y].right = 0;\
}


typedef struct {
    CRendDisplay_st   *display;	        /* Display */
    CRend_cl          cl;
    WS_WindowID       id;		/* WS Window ID */
    FB_Protocol       proto;		/* Protocol for FB connection */
    FB_StreamID       sid;		/* FB Stream ID */
    FBBlit_clp        blit;

    IO_clp            fbio;		/* Connection to FB */
    word_t            num_ioslots;

    uint32_t          sent;		/* Depth of pipe */
    uint32_t          x;		/* x offset on fb */
    uint32_t          y;		/* y offset on fb */
    
    Stretch_clp       pixstr;		/* Buffer stretch */

    pixel_t          *pixels;		/* Local pixmap if required */
    dirty_range      *dirty;
    uint32_t          width;
    uint32_t          height;
    uint32_t          stride;		/* pixelmap stride in pixels */
    pixel_t           fg;
    pixel_t           bg;
    word_t            wordfg;
    word_t            wordbg;
    CRend_PlotMode    plot_mode;
    
    int            ownpixdata;        /* 1 if we own the pixel data and stretch 
					    and so should destroy it
					 0 if we don't own the pixel data
                                         2 if we should just free it not kill
					    the stretch */

    uint32_t          xgrid, ygrid;
} CRend_st;

#endif
