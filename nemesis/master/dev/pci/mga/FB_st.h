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
**      User safe frame buffer device
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State record
** 
** ENVIRONMENT: 
**
**      Internal to module dev/pci/mga
** 
** ID : $Id: FB_st.h 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#ifndef _FB_H_
#define _FB_H_

#ifdef __LANGUAGE_C 

#include <dcb.h>
#include <mutex.h>

#include <FB.ih>
#include <IO.ih>
#include <CallPriv.ih>

#define m1024x768x15
//#define m1280x1024x15
//#define m1600x1200x15


/*****************************/
#ifdef   m1024x768x15
#define FB_WIDTH  1024
#define FB_HEIGHT  768
#define FB_DEPTH    16
#define FB_MODE      0
#endif

#ifdef m1280x1024x15
#define FB_WIDTH  1280
#define FB_HEIGHT 1024
#define FB_DEPTH    16
#define FB_MODE      1 
#endif

#ifdef m1600x1200x15
#define FB_WIDTH  1600
#define FB_HEIGHT 1200
#define FB_DEPTH    16
#define FB_MODE      2
#endif
/*****************************/


#ifdef __ALPHA__
#define PIXEL_X_GRID 8  /* must be powers of two */
#else
#define PIXEL_X_GRID 2
#endif
#define PIXEL_Y_GRID 1

typedef uint16_t  pixel16_t;
typedef pixel16_t row16_t[8];

typedef uint8_t  pixel8_t;
typedef pixel8_t row8_t[8];

typedef uint8_t  tag_t;

#if FB_DEPTH==16
typedef pixel16_t Pixel_t;
#endif

#if FB_DEPTH==8
typedef pixel8_t Pixel_t;
#endif





typedef union {
  pixel16_t pixel[8][8];
  row16_t  row[8]; 
} tile16_t;

typedef union {
  uint8_t pixel[8][8];
  row8_t  row[8]; 
} tile8_t;

typedef struct _FB_st     FB_st;
typedef struct _FB_Window FB_Window;
typedef struct _FB_Stream FB_Stream;

typedef uint32_t blit_fn(FB_st *st, dcb_ro_t *dcb, FB_StreamID sid,
		     FBBlit_Rec *rec);

struct _FB_Window {
    FB_WindowID    wid;		/* ID of this window */
    bool_t         free;
    
    dcb_ro_t      *owner;		/* DCB of owner */
    
    bool_t         mapped;
    bool_t         clipped;
    int32_t        x, y;
    uint32_t       width, height;
    uint32_t       stride;
    
    /* Rendering data - keep together for better cache performance */  
    Pixel_t        *pmag;	/* Pointer to window in PMAGBBA */
    tag_t          *clip;	/* Pointer to clip mask */
    tag_t       tag;
    word_t      tagword;
};

struct _FB_Stream {
  FB_StreamID    sid;		/* ID of this stream */
  bool_t         free;

  IDCOffer_clp   offer;		/* Client offer */
  IDCService_clp service;	/* For revocation of offer */
  FB_Window      *w;		/* Window */
  FB_Protocol    proto;		/* Protocol for IO connection */
  FB_QoS         qos;		/* Current QoS */


  IO_clp         io;		/* Client IO connection */
  blit_fn        *blit_m;	/* Blit method */

  /* Leaky Bucket Scheduling */
  int64_t        credits;	/* Credits in the bucket */
  uint64_t       creditspersec; 
  Time_ns        nspercredit;
  Time_ns        last;		/* Time of last credit */
  Time_ns        lastdbg;
};

#define FB_MAX_WINDOWS    64	/* XXX Temp */
#define FB_MAX_STREAMS    64	/* XXX Temp */

typedef void cpriv_fn(void);

struct _FB_st {
  FB_cl        cl;  /* FB closure */  

  Stretch_clp  clipstr;		/* Clip Mask RAM */
  tag_t       *clip, *clipbase;

  FB_Window window[FB_MAX_WINDOWS];
  FB_Stream stream[FB_MAX_STREAMS];

#define FB_SCHEDLEN 128
  Event_Count sched_event;
  uint64_t    cursched;
  FB_StreamID sched[2][FB_SCHEDLEN];
  uint64_t    schedndx;

  /* Queue of work to do */
  mutex_t     mu;
  FB_StreamID q[FB_MAX_STREAMS];
  
  FB_QoS      qosleft;
  IOEntry_clp fbentry;

  Pixel_t     *cardbase;
  blit_fn         *blit_fns[6];
  cpriv_fn        *blit_stub_fns[6];
  CallPriv_Vector blit_cps[6];  
  CallPriv_Vector mgr_cp;

  bool_t wssvr_shares_domain;  
};

/* Amount of blitting to do in a single callpriv */

#define MAX_PIXELS 128

/* Locking for state */
#define ENTER()  VP$ActivationsOff(Pvs(vp))
#define LEAVE()  VP$ActivationsOn(Pvs(vp))

/* Scheduling prototypes */
extern int  sched_update_qos(FB_st *st, FB_StreamID sid, FB_QoS q);
extern void sched_commit(FB_st *st);

/* Blitting code */
extern void blit_init(FB_st *st);
extern void blit(FB_st *st, FB_Stream *s);

/* Hardware poking functions */
extern void bt459_set_cursor_position(int32_t x, int32_t y);
extern void bt459_init_colour_map( void );
extern void bt459_reformat_cursor(char **xpm, uchar_t *bits);
extern void bt459_load_cursor( uchar_t *bits );
extern uint32_t *fb_init( );

#endif /* __LANGUAGE_C */

/* XXX Structure offsets for assember - nasty! */
#define FB_Q_BLIT_AVA    0x00
#define FB_Q_BLIT_DFS    0x08
#define FB_Q_BLIT_RASTER 0x10

/* XXX Blit CALLPRIV IDS */
#define FB_PRIV_BLIT_AVA    0x00
#define FB_PRIV_BLIT_DFS    0x01
#define FB_PRIV_BLIT_RASTER 0x02

#endif /* _FB_H_ */

