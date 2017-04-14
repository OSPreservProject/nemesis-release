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
**      app/WSterm
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Window System IO 
** 
** ENVIRONMENT: 
**
**      Unpriv domain
** 
** ID : $Id: wsterm.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>
#include <stdio.h>
#include <ntsc.h>

#include <keysym.h>

#include <WS.h>
#include "WSprivate.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define DB(_x) _x

/*****************************************************************/
/* State record */
typedef struct {
    Display *d;
    Window   w;
    uint8_t  fg;
    uint8_t  bg;
    Rd_clp   rd;
    Wr_clp   wr;
} WSterm_st;

static Closure_Apply_fn Init; 
static Closure_op ms = { Init }; 
static const Closure_cl cl = {&ms, NULL}; 
CL_EXPORT(Closure, WSterm, cl); 

/*****************************************************************/

#define BUF_INIT(buf)      ((buf)->head = (buf)->tail = 0)
#define BUF_IS_EMPTY(buf)  ((buf)->head == (buf)->tail)
#define BUF_IS_FULL(buf)   (!(((buf)->head+1 - (buf)->tail) & (BUF_SIZE-1)))
#define BUF_PUTC(buf, c) 				\
{  							\
  if (!BUF_IS_FULL(buf)) {	                        \
    (buf)->data[(buf)->head++] = c;			\
    if ((buf)->head == BUF_SIZE)			\
      (buf)->head = 0;					\
  }							\
}
#define BUF_GETC(buf)    			\
({						\
  uchar_t c = (buf)->data[(buf)->tail++];	\
  if ((buf)->tail == BUF_SIZE)		       	\
    (buf)->tail = 0;				\
  c;                                            \
})

typedef struct {
  uint32_t      head;
  uint32_t      tail;
#define BUF_SIZE      256
  uchar_t       data[BUF_SIZE];
} buf_t;

/*****************************************************************/
/*
 * Reader closure
 */
#include <Rd.ih>

static Rd_GetC_fn       GetC_m;
static Rd_EOF_fn        EOF_m;
static Rd_UnGetC_fn     UnGetC_m;
static Rd_CharsReady_fn CharsReady_m;
static Rd_GetChars_fn   GetChars_m;
static Rd_GetLine_fn    GetLine_m;
static Rd_Seek_fn       Seek_m;
static Rd_Close_fn      Close_m;
static Rd_Length_fn     Length_m;
static Rd_Index_fn      Index_m;
static Rd_Intermittent_fn Intermittent_m;
static Rd_Seekable_fn   Seekable_m;
static Rd_Lock_fn       Lock_m;
static Rd_Unlock_fn     Unlock_m;
static Rd_LGetC_fn      LGetC_m;
static Rd_LEOF_fn       LEOF_m;
static Rd_op rd_ms = {
    GetC_m,
    EOF_m,
    UnGetC_m,
    CharsReady_m,
    GetChars_m,
    GetLine_m,
    Seek_m,
    Close_m,
    Length_m,
    Index_m,
    Intermittent_m,
    Seekable_m,
    Lock_m,
    Unlock_m,
    LGetC_m,
    LEOF_m,
};

typedef struct _WSterm_Rd_st WSterm_Rd_st;
struct _WSterm_Rd_st {
    SRCThread_Condition   cv;     /* Kicked by a put into the buffer. */
    SRCThread_Mutex       mu;     /* Lock on clients.                 */
    buf_t                 buf;	/* Main buffer */
#define UNGETC_NOCHAR (1000)
    int32_t               ungetted;       /* one-char backup buffer   */
    int32_t               lastread;       /* used for ungetc          */
    Rd_cl                 cl;
};



/*----------------Reader methods-------------------------------------*/

/*
 * GetC checks the unget character, and if there is none, does that
 * standard condition-variable thang on the buffer.
 */
static int8_t GetC_m ( Rd_cl *self )
{
    WSterm_Rd_st *st = self->st;
    NOCLOBBER int8_t res =0;

    USING (MUTEX, &(st->mu),
	   res = LGetC_m (self);
	);
    return res;
}

/*
 * EOF is always false for the serial line. Stubs may override this
 * method for local bindings.
 */
static bool_t EOF_m ( Rd_cl *self )
{
    WSterm_Rd_st *st = self->st;
    NOCLOBBER bool_t res = False;
  
    USING(MUTEX, &(st->mu),
	  res = LEOF_m(self);
        );
    return res;
}

/*
 * UnGetC pushes the last char read into the buffer.
 */
static void UnGetC_m ( Rd_cl    *self )
{
    WSterm_Rd_st *st = self->st;

    USING (MUTEX, &(st->mu),
         
	   if ( st->ungetted == UNGETC_NOCHAR ) {
	       RAISE_Rd$Failure( 3 ); /* 3 = can't UnGetC */
	   }
         
	   st->ungetted = st->lastread;
	);
}

/*
 * Simple read of the buffer size. 
 */
static uint64_t CharsReady_m ( Rd_cl    *self )
{
#if 0
    WSterm_Rd_st *st = self->st;
    return st->length - st->point;
#else /* 0 */
    return 0;
#endif /* 0 */
}

/*
 * Get a number of characters into the buffer. 
 * Under the lock we still maintain the invariants concerning UnGetC.
 * We also only return characters up to the end of the current line.
 */
static uint64_t GetChars_m (Rd_cl       *self,
			    Rd_Buffer    buf     /* IN */,
			    uint64_t     nb      /* IN */ )
{
    WSterm_Rd_st *st = self->st;

    NOCLOBBER uint64_t i = 0;

    USING (MUTEX, &(st->mu),

	   for( i = 0; i < nb; i++ ) {
	       if ( st->ungetted != UNGETC_NOCHAR )  {
		   buf[i] = st->ungetted;
		   st->ungetted = UNGETC_NOCHAR;
	       } else {
		   while( BUF_IS_EMPTY( &(st->buf ) )) {
		       WAIT( &(st->mu), &(st->cv) );
		   }
		   buf[i] = BUF_GETC( &(st->buf) ); 
	       }
	       st->lastread = buf[i];
	   }
	);
    return i;
}

/*
 * GetLine is very similar to GetChars, since we stop after a line anyway.
 */
static uint64_t GetLine_m (Rd_cl          *self,
			   Rd_Buffer       buf     /* IN */,
			   uint64_t        nb      /* IN */ )
{
    WSterm_Rd_st *st = self->st;
    NOCLOBBER bool_t nocr = True;
    uint64_t i;

    USING (MUTEX, &(st->mu),
	 
	   for( i = 0; i < nb && nocr; i++ ) {
	       if ( st->ungetted != UNGETC_NOCHAR )  {
		   buf[i] = st->ungetted;
		   st->ungetted = UNGETC_NOCHAR;
	       } else {
		   while( BUF_IS_EMPTY( &(st->buf ) )) {
		       WAIT( &(st->mu), &(st->cv) );
		   }
		   buf[i] = BUF_GETC( &(st->buf) );
	       }
	       st->lastread = buf[i];
	       if ( buf[i] == '\n' ) nocr = False;
	   }
	);
    return nb;
}

/*
 * We are not seekable.
 */
static void Seek_m ( Rd_cl      *self,
		     uint64_t        nb      /* IN */ )
{
    RAISE_Rd$Failure(1); /* 1 == not seekable */
}

/*
 * Simply send close downstream. 
 */
static void Close_m ( Rd_cl     *self )
{
/*  Rd_Close( st->rd ); */
}

/* 
 * Length is unknown (potentially infinite).
 */
static uint64_t Length_m ( Rd_cl *self )
{
    RAISE_Rd$Failure(4);
    return 0;
}

/*
 * Index is probably not correctly implemented, but returns 0.
 */
static uint64_t Index_m ( Rd_cl *self )
{
    return 0;
}

/*
 * We are intermittent.
 */
static bool_t Intermittent_m ( Rd_cl    *self )
{
    return True;
}

/*
 * We are not seekable
 */
static bool_t Seekable_m ( Rd_cl        *self )
{
    return False;
}

/*
 * Acquire the lock
 */
static void Lock_m ( Rd_cl      *self )
{
    WSterm_Rd_st *st = self->st;
    MU_LOCK(&(st->mu));
}

static void Unlock_m ( Rd_cl    *self )
{
    WSterm_Rd_st *st = self->st;
    MU_RELEASE(&(st->mu));
}

static int8_t LGetC_m ( Rd_cl   *self )
{
    WSterm_Rd_st *st = self->st;
    int8_t res;
  
    if ( st->ungetted != UNGETC_NOCHAR )  {
	res = st->ungetted;
	st->ungetted = UNGETC_NOCHAR;
    } else {
	while( BUF_IS_EMPTY( &(st->buf ) )) {
	    WAIT( &(st->mu), &(st->cv) );
	}
	res = BUF_GETC( &(st->buf) ); 
    }
    st->lastread = res;
    return res;
}

static bool_t LEOF_m ( Rd_cl    *self )
{
    WSterm_Rd_st *st = self->st;

    st->ungetted = st->lastread; /* Nobble further UnGetCs */
    return False;
}

/*****************************************************************/

#include <Wr.ih>
/*
 * Writer for output to the WSterm window 
 */
static Wr_PutC_fn      WSterm_PutC_m;
static Wr_PutStr_fn    WSterm_PutStr_m;
static Wr_PutChars_fn  WSterm_PutChars_m;
static Wr_Seek_fn      WSterm_Seek_m;
static Wr_Flush_fn     WSterm_Flush_m;
static Wr_Close_fn     WSterm_Close_m;
static Wr_Length_fn    WSterm_LengthOrIndex_m;
static Wr_Index_fn     WSterm_LengthOrIndex_m;
static Wr_Seekable_fn  WSterm_Seekable_m;
static Wr_Buffered_fn  WSterm_Buffered_m;
static Wr_Lock_fn      WSterm_Lock_m;
static Wr_Unlock_fn    WSterm_Unlock_m;
static Wr_LPutC_fn     WSterm_LPutC_m;
static Wr_LPutStr_fn   WSterm_LPutStr_m;
static Wr_LPutChars_fn WSterm_LPutChars_m;
static Wr_LFlush_fn    WSterm_LFlush_m;

static Wr_op wr_ms = {
    WSterm_PutC_m,
    WSterm_PutStr_m,
    WSterm_PutChars_m,
    WSterm_Seek_m,
    WSterm_Flush_m,
    WSterm_Close_m,
    WSterm_LengthOrIndex_m,
    WSterm_LengthOrIndex_m,
    WSterm_Seekable_m,
    WSterm_Buffered_m,
    WSterm_Lock_m,
    WSterm_Unlock_m,
    WSterm_LPutC_m,
    WSterm_LPutStr_m,
    WSterm_LPutChars_m,
    WSterm_LFlush_m
};
typedef struct _WSterm_Wr_st WSterm_Wr_st;
struct _WSterm_Wr_st {
    SRCThread_Mutex       mu;     /* Lock on clients.                 */
    Window       w;		/* Window to send output to */
    uint8_t      fg, bg;
    uint32_t     x;		/* Coords */
    uint32_t     y;
    buf_t        buf;		/* Output buffer */
    Wr_cl        cl;
};

/*----------------Writer methods---------------------------------*/
/*
 * Strictly, these should all RAISE_Wr_Failure (0) if self is closed,
 * but I can't be bothered just now.
 */
static INLINE void InnerLPutC (WSterm_Wr_st *st, int8_t c);
static INLINE void InnerFlush (WSterm_Wr_st *st);

static void
WSterm_PutC_m (Wr_clp self, int8_t c)
{
    WSterm_Wr_st *st = self->st;
    USING (MUTEX, &st->mu, InnerLPutC (st, c););
}

static void
WSterm_PutStr_m (Wr_clp self, string_t s)
{
    WSterm_Wr_st *st = self->st;
    USING (MUTEX, &st->mu, WSterm_LPutStr_m (self, s););
}

static void
WSterm_PutChars_m (Wr_clp self, Wr_Buffer s, uint64_t nb)
{
    WSterm_Wr_st *st = self->st;
    USING (MUTEX, &st->mu, WSterm_LPutChars_m (self, s, nb););
}

static void
WSterm_Seek_m (Wr_clp self, uint64_t n)
{
    RAISE_Wr$Failure (1);
}

static void
WSterm_Flush_m (Wr_clp self)
{
    WSterm_Wr_st *st = self->st;
    USING (MUTEX, &st->mu, InnerFlush (st););
}

static void
WSterm_Close_m (Wr_clp self)
{
    /* XXX - use Client_t cntrl_cl to close down */
}

static uint64_t
WSterm_LengthOrIndex_m (Wr_clp self)
{
    WSterm_Wr_st *st = self->st;
    NOCLOBBER uint32_t res = 0;

    USING (MUTEX, &st->mu, res = st->buf.head;);  

    if (res <= BUF_SIZE)
	RAISE_Wr$Failure (1);

    return res - BUF_SIZE;
}

static bool_t
WSterm_Seekable_m (Wr_clp self)
{
    return False;
}

static bool_t
WSterm_Buffered_m (Wr_clp self)
{
    return True;
}

/* 
 * Unlocked operations
 */

/* LL < st->mu */
static void
WSterm_Lock_m (Wr_clp self)
{
    WSterm_Wr_st *st = self->st;
    MU_LOCK(&st->mu);
}

/* LL = st->mu */
static void
WSterm_Unlock_m (Wr_clp self)
{
    WSterm_Wr_st *st = self->st;
    MU_RELEASE(&st->mu);
}

/* LL = st->mu */
static void
WSterm_LPutC_m (Wr_clp self, int8_t c)
{
    WSterm_Wr_st *st = self->st;
    InnerLPutC (st, c);
}

/* LL = st->mu */
static void
WSterm_LPutStr_m (Wr_clp self, string_t s)
{
    WSterm_Wr_st *st = self->st;
  
    while (*s)
	InnerLPutC (st, *s++);

    InnerFlush (st);
}

/* LL = st->mu */
static void
WSterm_LPutChars_m (Wr_clp self, Wr_Buffer s, uint64_t nb)
{
    WSterm_Wr_st *st = self->st;
  
    for (; nb; nb--)
	InnerLPutC (st, *s++);

    InnerFlush (st);
}

/* LL = st->mu */
static void
WSterm_LFlush_m (Wr_clp self)
{
    WSterm_Wr_st *st = self->st;
    InnerFlush (st);
}

#define UNSENT(st) (st->buf.head)

/* LL = st->mu */
static INLINE void
InnerLPutC (WSterm_Wr_st *st, int8_t c)
{
    if(BUF_IS_FULL(&st->buf))
	InnerFlush(st);

    BUF_PUTC(&st->buf, c);
}

/* LL = st->mu */

static INLINE void
InnerFlush (WSterm_Wr_st *st)
{
    static void WSputchar(WSterm_Wr_st *st, uint32_t c);

    while(!(BUF_IS_EMPTY(&st->buf)))
	WSputchar(st, BUF_GETC(&st->buf));
    WSFlush(st->w); 
}

/*****************************************************************/
/* Constants... */
#define BLACK 0x00
#define WHITE 0xFF

#define WSTERM_WIDTH 80
#define WSTERM_HEIGHT 25
/*****************************************************************/

static void WSputchar(WSterm_Wr_st *st, uint8_t c)
{
    char buf[2];
    int i;

    c&= 0xff;

    if (c == '\r')
	return;

    if (c == '\a') {
	/* bell */

	/* blink cursor */
	for ( i = 0; i < 16; i++) {
	    WSSetForeground(st->w, 0xf);
	    WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);
	    WSFlush(st->w);
	    WSSetForeground(st->w, st->bg);
	    WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);
	    WSFlush(st->w);
	}
	WSSetForeground(st->w, st->fg);
	WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);
	WSFlush(st->w);
	
	return;
    }

    if (c == '\n' || st->x == WSTERM_WIDTH) {

	/* Remove cursor */
	WSSetForeground(st->w, st->bg);
	WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);
	
	/* Move to next line */
	st->x = 0; st->y++; 

	if (st->y == WSTERM_HEIGHT) st->y = 0;
	st->w->yroll = (WSTERM_HEIGHT - st->y -1)*16;
	/* Blank the line */
	WSFillRectangle(st->w, 0, (st->y)<<4, (WSTERM_WIDTH*8)-1, 15);

	/* Draw cursor */
	WSSetForeground(st->w, st->fg);
	WSFillRectangle(st->w, 0, (st->y)<<4, 7, 15);

	/* WSFlush(st->w); */

	return;
    }

/*    if ((c == 0x7F) && (st->x > 0)) { */
    if ((c == '\b') && (st->x > 0)) { 

	/* Remove cursor */
	WSSetForeground(st->w, st->bg);
	WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);

	/* Backspace */
	st->x--;

	/* Draw cursor */
	WSSetForeground(st->w, st->fg);
	WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);
	return;
    }

    if ((c < ' ') || (c > '~'))
	return;

    buf[0] = c;
    buf[1] = 0;
    
    if (st->x >= WSTERM_WIDTH) {
	fprintf(stderr, "shafted.\n");
	ntsc_dbgstop();
    }

    /* Output char */
    WSSetForeground(st->w, st->fg);
    WSDrawString(st->w, (st->x)<<3, (st->y)<<4, buf);

    /* Move cursor */
    if ( ++(st->x) < WSTERM_WIDTH) {

	/* Draw cursor */
	WSSetForeground(st->w, st->fg);
	WSFillRectangle(st->w, (st->x)<<3, (st->y)<<4, 7, 15);
    }    

}

/* Put a character into the reader's buffer from behind */
static void Rd_Putchar (Rd_clp self, uchar_t c)
{
    WSterm_Rd_st *st = self->st;

    USING(MUTEX, &(st->mu),
	  BUF_PUTC(&st->buf, c);
	);
    SIGNAL(&(st->cv));
}

static void EventLoop (WSterm_st *st)
{
    Display *d = st->d;
    Window w = st->w;
    Rd_clp rd = st->rd;
    int my1, my2;
    while (True) {
	WSEvent  ev;
	uint32_t key;
#ifdef DEBUG 
	char buf[32];
#endif

	WSNextEvent(d, &ev);

	switch(ev.d.tag) {
	case WS_EventType_Expose:
	    my1 = (ev.d.u.Expose.y1 - w->yroll ) % w->height;
	    my2 = (ev.d.u.Expose.y2 - w->yroll ) % w->height;
	    my1 = 0;
	    my2 = (w->height);
	       
	    if (my2 < my1) {
		WSExposeRectangle(w, 
				  ev.d.u.Expose.x1, my2,
			      ev.d.u.Expose.x2 - ev.d.u.Expose.x1,
			      my1-my2);
	    } else {
		WSExposeRectangle(w, 
				  ev.d.u.Expose.x1, my1,
			      ev.d.u.Expose.x2 - ev.d.u.Expose.x1,
			      my2-my1);
	    }
	    break;
	case WS_EventType_Mouse:
#ifdef DEBUG
	    sprintf(buf, "Mouse: %c%c%c (%d,%d)    ", 
		    ev.d.u.Mouse.buttons & 1<<WS_Button_Left   ? 'L' : '-',
		    ev.d.u.Mouse.buttons & 1<<WS_Button_Middle ? 'M' : '-',
		    ev.d.u.Mouse.buttons & 1<<WS_Button_Right  ? 'R' : '-',
		    ev.d.u.Mouse.x, ev.d.u.Mouse.y);

	    WSSetForeground(w, st->fg);
	    WSDrawString(w, 0, 0, buf);
	    WSFlush(w); 
#endif 
	    break;
	case WS_EventType_KeyPress:
	    key = ev.d.u.KeyPress;
#ifdef DEBUG
	    sprintf(buf, "Down:  %04x       ", key);
	    WSSetForeground(w, st->fg);
	    WSDrawString(w, 0, 32, buf);
#endif 
	    if (key <= XK_asciitilde) 
		Rd_Putchar(rd, key);
	    else if (key == XK_Return) 
		Rd_Putchar(rd, '\n');
	    else if (key == XK_Delete)
		Rd_Putchar(rd, 0x7F);
#if 0
	    else 
		fprintf(stderr, "<%d>\n", key);
#endif 0

	    break;
	case WS_EventType_KeyRelease:
	    key = ev.d.u.KeyPress;
#ifdef DEBUG
	    sprintf(buf, "Up:    %04x       ", key);
	    WSSetForeground(w, st->fg);
	    WSDrawString(w, 0, 16, buf);
#endif
	    break;
	    
	default:
	    break;
	}
	WSFlush(w); 
    }
}


static Rd_clp NewRd() 
{
    WSterm_Rd_st *st;
    st = Heap$Malloc(Pvs(heap), sizeof (*st));

    /* Init reader */
    MU_INIT(&st->mu);
    CV_INIT(&st->cv);
    BUF_INIT(&st->buf);
    st->ungetted = UNGETC_NOCHAR;
    st->lastread = UNGETC_NOCHAR;
    CL_INIT(st->cl, &rd_ms, st);
    return &st->cl;
}
static Wr_clp NewWr(Window w, uint8_t fg, uint8_t bg) 
{
    WSterm_Wr_st *st;
    st = Heap$Malloc(Pvs(heap), sizeof (*st));

    /* Init writer */
    MU_INIT(&st->mu);
    BUF_INIT(&st->buf);
    st->w = w;
    st->fg = fg;
    st->bg = bg;
    st->x = 0;
    st->y = (WSTERM_HEIGHT-1);
    CL_INIT(st->cl, &wr_ms, st);

    return &st->cl;
}


void Init (Closure_clp self)
{
    WSterm_st *st;

    TRC(printf ("WSterm: entered.\n"));
    st = Heap$Malloc(Pvs(heap), sizeof (*st));

    st->d = WSOpenDisplay(""); 
    TRC(printf("wsterm : opened display at %p\n", st->d));
    st->w = WSCreateWindow (st->d, 64, 192, WSTERM_WIDTH*8, WSTERM_HEIGHT*16); 
    TRC(printf("wsterm : created window at %p\n", st->w));
    st->w->yroll = (WSTERM_HEIGHT-1)*16;
    st->fg = WHITE;
    st->bg = BLACK;

    WSMapWindow(st->d, st->w); 
    TRC(printf("wsterm : mapped window.\n"));

    st->rd = NewRd();
    st->wr = NewWr(st->w, st->fg, st->bg);

    WSSetForeground(st->w, st->bg);
    TRC(printf("wsterm : set fground.\n"));
    WSFillRectangle(st->w, 0, 0,  WSTERM_WIDTH*8, WSTERM_HEIGHT*16);
    TRC(printf("wsterm : filled rect...\n"));
    WSFlush(st->w); 

    WSSetForeground(st->w, st->fg);
    WSSetBackground(st->w, st->bg);

    Pvs(in)  = st->rd;
    Pvs(out) = st->wr;
    TRC(printf("wsterm entered, domain %lx, dcb at %lx\n", VP$DomainID(Pvs(vp)), 
	   RO(Pvs(vp))));

    (void)Threads$Fork(Pvs(thds), EventLoop, st, 0 );  
}


