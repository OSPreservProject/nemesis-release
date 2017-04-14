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
**      Prototypes
** 
** ENVIRONMENT: 
**
**      Shared library.
** 
** ID : $Id: WS.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _WS_H_
#define _WS_H_

#include <WS.ih>
#include <IO.ih>
#include <FB.ih>
#include <FBBlit.ih>

/*
 * Datatypes
 */

#ifndef SERVER_RENDERING

typedef struct _Display   Display;
typedef struct _Window   *Window;
typedef struct _Point     Point;
typedef struct _Segment   Segment;
typedef struct _Rectangle Rectangle;

typedef struct WS_Event   WSEvent;

struct _Point {
  int32_t x;
  int32_t y;
};

struct _Segment {
  int32_t x1;
  int32_t x2;
  int32_t y1;
  int32_t y2;
};

struct _Rectangle {
  int32_t x;
  int32_t y;
  uint32_t width;
  uint32_t height;
};

#else 

#include <WSRender.ih>

typedef struct _Display           Display;
typedef struct _Window           *Window;
typedef struct WSRender_Point     Point;
typedef struct WSRender_Segment   Segment;
typedef struct WSRender_Rectangle Rectangle;

#endif /* SERVER_RENDERING */

/* 
 * Function declarations.
 */

extern Display *WSOpenDisplay(
    const string_t		/* display_name */
);

extern uint32_t WSDisplayHeight(
    Display*			/* display */
);

extern uint32_t WSDisplayWidth(
    Display*			/* display */
);

extern void WSCloseDisplay(
    Display*			/* display */
);

/* 
 * Window creation/deletion 
 */
extern Window WSCreateWindow(
    Display*			/* display */,
    int32_t			/* x */,
    int32_t			/* y */,
    uint32_t			/* width */,
    uint32_t			/* height */
); 

extern void WSDestroyWindow(
    Window			/* w */
);

/*
 * Window update stream 
 */
extern FB_StreamID WSUpdateStream(
    Window                      /* w */,
    FB_Protocol                 /* protocol */,
    FB_QoS                      /* q */,
    bool_t                      /* clip */,				
    IOOffer_clp *              /* offer */,
    FBBlit_clp *                /* blit */
);

/*
 * Move/resize - Should be done by WM exposure
 */
extern void WSMapWindow(
    Display*			/* display */,
    Window			/* w */
);
extern void WSUnmapWindow(
    Display*			/* display */,
    Window			/* w */
);

extern void WSMoveWindow(
    Display*			/* display */,
    Window			/* w */,
    int32_t			/* x */,
    int32_t			/* y */
);

extern void WSResizeWindow(
    Display*			/* display */,
    Window			/* w */,
    uint32_t			/* width */,
    uint32_t			/* height */
);

/*
 * FG/BG colour for rendering
 */
extern void WSSetBackground(
    Window			/* w */,
    uint32_t			/* background */
);
extern void WSSetForeground(
    Window			/* w */,
    uint32_t			/* foreground */
);

/* 
 * Rendering ops 
 */
extern void WSDrawPoint(
    Window			/* w */,
    int32_t			/* x */,
    int32_t			/* y */
);

extern void WSDrawPoints(
    Window			/* w */,
    Point*			/* points */,
    int32_t			/* npoints */,
    int32_t			/* mode */
);

extern void WSDrawLine(
    Window			/* w */,
    int32_t			/* x1 */,
    int32_t			/* x2 */,
    int32_t			/* y1 */,
    int32_t			/* y2 */
);

extern void WSDrawLines(
    Window			/* w */,
    Point*			/* points */,
    int32_t			/* npoints */,
    int32_t			/* mode */
);

extern void WSDrawRectangle(
    Window			/* w */,
    int32_t			/* x */,
    int32_t			/* y */,
    uint32_t			/* width */,
    uint32_t			/* height */
);

extern void WSDrawRectangles(
    Window			/* w */,
    Rectangle*			/* rectangles */,
    int32_t			/* nrectangles */
);

extern void WSDrawSegments(
    Window			/* w */,
    Segment*			/* segments */,
    int32_t			/* nsegments */
);

extern bool_t WSDrawString(
    Window			/* w */,
    int32_t			/* x */,
    int32_t			/* y */,
    string_t			/* s */
);

extern void WSFillRectangle(
    Window			/* w */,
    int32_t			/* x */,
    int32_t			/* y */,
    uint32_t			/* width */,
    uint32_t			/* height */
);

extern void WSExposeRectangle(
    Window			/* w */,
    uint32_t			/* x */,
    uint32_t			/* y */,
    uint32_t			/* width */,
    uint32_t			/* height */
);

extern void WSFillRectangles(
    Window			/* w */,
    Rectangle*			/* rectangles */,
    int32_t			/* nrectangles */
);

extern void WSFlush(
    Window			/* w */
);

/*
 * Events 
 */
extern void WSNextEvent(
    Display*			/* d */,
    WSEvent*			/* event_return */
);

extern bool_t WSEventsPending(
    Display*			/* d */
);

#endif /* _WS_H_ */
