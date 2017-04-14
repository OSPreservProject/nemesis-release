/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Command line module.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      A reader filter which implements simple command line editing
**      in ASCII. This reader has the property that it only operates a
**	line at a time, and so CharsReady is always False if we have
**	just read a line. This enables the interpreter upstream of us
**	to issue a prompt for each line, if it wished.
** 
** ENVIRONMENT: 
**
**      User application
** 
** ID : $Id: cline.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>
#include <ctype.h>
#include <stdio.h>

#include <CLine.ih>
#include <CLineCtl.ih>


#include <Rd.ih>
#include <Wr.ih>


#ifdef CLINE_TRACE
#define TRC(_x) _x
#else 
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x)  _x
#else
#define DB(_x)
#endif

#define LINE_SIZE (512)
#define HIST_SIZE (32)
#define UNGETC_NOCHAR (256)
#define PC(_c) Wr$PutC(st->wr,_c)

/*****************************************************************/
/*
 * Reader Methods
 */

static Rd_GetC_fn	GetC_m;
static Rd_EOF_fn 	EOF_m;
static Rd_UnGetC_fn 	UnGetC_m;
static Rd_CharsReady_fn CharsReady_m;
static Rd_GetChars_fn	GetChars_m;
static Rd_GetLine_fn 	GetLine_m;
static Rd_Seek_fn 	Seek_m;
static Rd_Close_fn 	Close_m;
static Rd_Length_fn 	Length_m;
static Rd_Index_fn 	Index_m;
static Rd_Intermittent_fn Intermittent_m;
static Rd_Seekable_fn 	Seekable_m;
static Rd_Lock_fn	Lock_m;
static Rd_Unlock_fn 	Unlock_m;
static Rd_LGetC_fn 	LGetC_m;
static Rd_LEOF_fn 	LEOF_m;
static Rd_op rd_op = {
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

/*---State-record-----------------------------------------------*/

typedef struct EB_t EB_t;
struct EB_t {
    struct _Rd_cl cl;
    Rd_clp        rd;
    Wr_clp        wr;
    uint32_t      ungetted;
    uint32_t      lastread;
    uint32_t      point;
    uint32_t      length;
    uint32_t      size;
    mutex_t       mu;
    int8_t        ch[LINE_SIZE];   /* current line */
    int8_t        lastch;          /* for CR-LF / CR-NUL / VT-100 processing */
    CLineCtl_cl   ctl;
    bool_t        echo;
};

/*---Misc-statics-----------------------------------------------*/

static void EB_Reset( EB_t *st );
static void EB_Printable( EB_t *st, int8_t c );
static void EB_DeleteBack( EB_t*st );
static void EB_MoveBack( EB_t *st);
static void EB_Weirdo( EB_t *st, int8_t c );
static void GetNewLine( EB_t *st );

/*---Creator-Function------------------------------------------*/

/*
 * Module stuff
 */

static	CLine_New_fn 	       CLine_New_m;
static	CLine_op	ms = {CLine_New_m};
static	const CLine_cl	cl = { &ms, NULL };
CL_EXPORT (CLine, CLine, cl);

static	CLineCtl_Echo_fn           CLineCtl_Echo_m;
static	CLineCtl_op	ctl_ms = {CLineCtl_Echo_m};


/*---------------------------------------------------- Entry Points ----*/

static Rd_clp CLine_New_m (CLine_cl	*self,
			   Rd_clp	rawrd	/* IN */,
			   Wr_clp	rawwr	/* IN */
			   /* RETURNS */,
			   Wr_clp	*wr,
			   CLineCtl_clp *ctl)
{
    struct EB_t *st = Heap$Malloc(Pvs(heap),sizeof(*st ));
    
    if ( !st ) RAISE_Heap$NoMemory();
    
    CL_INIT(st->cl, &rd_op, st );
    st->rd = rawrd;
    st->wr = rawwr;
    st->size = LINE_SIZE;
    MU_INIT(&st->mu);
    EB_Reset( st );
    st->lastch = 0;

    CL_INIT(st->ctl, &ctl_ms, st );
    st->echo = True;

    *wr  = rawwr;
    *ctl = &st->ctl;
    return &st->cl;
}

/*---------------------------------------------------- Entry Points ----*/

static void CLineCtl_Echo_m (CLineCtl_cl *self, bool_t mode)
{
    EB_t *st = self->st;
    
    st->echo = mode;
}

/*---Editing-actions----------------------------------------*/

static void EB_Reset( EB_t *st )
{
    st->ungetted = UNGETC_NOCHAR;
    st->lastread = UNGETC_NOCHAR;
    st->point  = 0;
    st->length = 0;
    st->ch[0]  = '\0';
}

static void EB_Printable( EB_t *st, int8_t c )
{
    int i;
    /* We insert a char into the buffer. */
    
    if ( st->point == st->size - 2 ) {
	PC('\a'); /* ring the bell */
    } else {
	/* make space */
	for (i = st->length; i > st->point; i-- ) {
	    st->ch[i] = st->ch[i-1];
	}
	st->ch[ st->point++ ] = c;
	st->length++;
	st->ch[ st->length ] = '\0';
	/* This could be the echo? */
	if (st->echo) {
	  Wr$PutStr(st->wr,&(st->ch[ st->point - 1 ]) );
	  for( i = st->point; i < st->length; i++ ) {
	    PC('\b');
	  }   
	}
    }
    Wr$Flush(st->wr);
}
static void EB_DeleteBack( EB_t*st )
{
    int i;
    
    if ( st->point == 0 ) {
	PC('\a');
    } else {
	PC('\b');
	--(st->length);
	--(st->point);
	for( i = st->point; i < st->length; i++ ) {
	    st->ch[i] = st->ch[ i+1 ];
	}
	st->ch[ st->length ] = '\0';
	if (st->echo) {
	  Wr$PutStr(st->wr,&(st->ch[ st->point ]) );
	  PC(' '); 
	  for( i = st->point; i <= st->length; i++ ) {
	    PC('\b');
	  }
	}
    }
    Wr$Flush(st->wr);
}

static void EB_MoveBack( EB_t *st)
{
    if ( st->point == 0 ) {
	PC('\a');
    } else {
	PC('\b');
	--(st->point);
    }
    Wr$Flush(st->wr);
}

static void EB_MoveForward( EB_t *st )
{
    if ( st->point >= st->length ) {
	PC('\a');
    } else {
	PC(st->ch[st->point]); /* in the absence of a better way */
	++(st->point);
    }
    Wr$Flush(st->wr);
}

static void EB_MoveStart ( EB_t *st )
{
    while (st->point != 0) {
	PC('\b');
	--(st->point);
    }
    Wr$Flush(st->wr);
}

static void EB_MoveEnd ( EB_t *st )
{
    while (st->point < st->length) {
        /* in the absence of a better way */
	PC(st->ch[st->point]); 
	++(st->point);
    }
    Wr$Flush(st->wr);
}

static void EB_MoveDown ( EB_t *st )
{
    /* For now just beep */
    PC('\a');
    Wr$Flush(st->wr);
}

static void EB_MoveUp ( EB_t *st )
{
    PC('\a');
    Wr$Flush(st->wr);
}

static void EB_Weirdo( EB_t *st, int8_t c )
{
    /* For now, just 'beep' */
/*    PC('\a'); */
    Wr$Flush(st->wr);
}

static void EB_EndOfLine( EB_t *st, int8_t c )
{
    st->ch[ (st->length)++ ] = '\n';
    st->point = 0;

    Wr$PutC(st->wr,'\n');
    Wr$PutC(st->wr,'\r');
    Wr$Flush(st->wr);
};

/*---Get-new-line-of-text--------------------------------------*/

#define CTRL(c) ( (c) - '@' )

#define CURSOR_ESC 0x1b  /* == 'real' <esc>, which prefixes cursor keys    */

static void GetNewLine( EB_t *st )
{
    EB_Reset( st );
    
    TRC(fprintf(stderr, "\nGetNewLine: called.\n"));
    for(;;) {
	bool_t   was_cr = (st->lastch == '\r');
	bool_t   was_esc= (st->lastch == CURSOR_ESC);
	uint32_t c= Rd$GetC(st->rd) & 0xff;
	st->lastch = c;
	
	if ( isprint( c )) {
	    
	    if(was_esc && c == '[') {
		c= Rd$GetC(st->rd) & 0xff;
		switch(c) {

		case 'A':
		    EB_MoveUp( st );
		    break;

		case 'B':
		    EB_MoveDown( st );
		    break;
		    
		case 'C':
		    EB_MoveForward( st );
		    break;

		case 'D':
		    EB_MoveBack( st );
		    break;

		default:
		    EB_Weirdo( st, c );
		    break;
		}
	    } else {

		EB_Printable( st, c );

	    }
	    
	} else if (was_cr && (c == '\n' || c == 0)) {
	    /* ignore the second char in a CR-LF or
	       telnet-style CR-NUL sequence */
	    
	} else if ( c == '\n' || c == '\r' ) {
	    
	    EB_EndOfLine( st, c );
	    return;
	    
	} else if ( c == '\x7f' ) {
	    
	    EB_DeleteBack( st );
	    
	} else if ( c == CTRL('B') ) {
	    
	    EB_MoveBack( st );
	    
	} else if ( c == CTRL('F') ) {
	    
	    EB_MoveForward( st );
	    
	} else if ( c == CTRL('A') ) {
	    
	    EB_MoveStart( st );
	    
	} else if ( c == CTRL('E') ) {
	    
	    EB_MoveEnd( st );

	} else if ( c == CTRL('P') ) {
	    
	    EB_MoveUp( st );
	    
	} else if ( c == CTRL('N') ) {
	    
	    EB_MoveDown( st );
	    
	} else if ( c == CURSOR_ESC ) { 

	    st->lastch = c;

	} else {
	    
	    EB_Weirdo( st, c );
	}
	
    }
}

/*----------------Reader methods-------------------------------------*/

/*
 * GetC checks the unget character, and if there is none, does that
 * standard condition-variable thang on the buffer.
 */
static int8_t GetC_m ( Rd_cl *self )
{
    EB_t *st = self->st;
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
    EB_t *st = self->st;
    NOCLOBBER bool_t res = False;
    
    USING(MUTEX, &(st->mu),
	  res = LEOF_m(self);
	);
    return res;
}

/*
 * UnGetC pushes the last char read into the buffer.
 */
static void UnGetC_m ( Rd_cl	*self )
{
  EB_t *st = self->st;

  USING (MUTEX, &(st->mu),
	 
	 if ( st->ungetted != UNGETC_NOCHAR ) {
	     RAISE_Rd$Failure( 3 ); /* 3 = can't UnGetC */
	 }
	 
	 st->ungetted = st->lastread;
      );
}

/*
 * Simple read of the buffer size. 
 */
static uint64_t CharsReady_m ( Rd_cl	*self )
{
    EB_t *st = self->st;
    
    return st->length - st->point;
}

/*
 * Get a number of characters into the buffer. 
 * Under the lock we still maintain the invariants concerning UnGetC.
 * We also only return characters up to the end of the current line.
 *
 *
 *
 */
static uint64_t GetChars_m (Rd_cl *self,
			    Rd_Buffer NOCLOBBER buf	/* IN */,
			    uint64_t nb	/* IN */ )
{
    EB_t *st = self->st;
    NOCLOBBER uint32_t res = 0;
    uint64_t num = 0;
    int i = 0;

    if(!buf) {
	printf("cline - GetChars: buf is NULL, returning.\n");
	return res;
    }

    USING (MUTEX, &(st->mu),

	   /* first take care of new lines */
	   if ( st->point == st->length ) {
	       GetNewLine( st );
	   }
	   
	   /* Work out how many chars to pull out of the buffer */
	   res = MIN( nb, st->length - st->point );
	   num = res;

	   /* Next fudge ungetted characters */
	   if ( st->ungetted != UNGETC_NOCHAR ) {
	       buf[0] = st->ungetted;
	       st->lastread = st->ungetted;
	       st->ungetted = UNGETC_NOCHAR;
	       buf++;
	       num --;	
	   };


	   for( i = 0; i < num; i++ ) {
	       *buf++ = st->ch[ (st->point)++ ];
	       st->lastread = buf[i];
	   }
	);
    return res;
}

/*
 * GetLine is very similar to GetChars, since we stop after a line anyway.
 */
static uint64_t GetLine_m ( Rd_cl	*self,
	Rd_Buffer	buf	/* IN */,
	uint64_t	nb	/* IN */ )
{
    EB_t *st = self->st;
    return GetChars_m( self, buf, MIN( nb, st->size ));
}

/*
 * We are not seekable.
 */
static void Seek_m ( Rd_cl	*self,
	uint64_t	nb	/* IN */ )
{
    RAISE_Rd$Failure(1) /* 1 == not seekable */
}

/*
 * Simply send close downstream. 
 * XXX but what about our state?
 */
static void Close_m ( Rd_cl	*self )
{
    EB_t *st = self->st;
    Rd$Close( st->rd );
}

/* 
 * The length is unknown (potentially infinite).
 */
static uint64_t Length_m ( Rd_cl	*self )
{
    RAISE_Rd$Failure(4);

    return 0;
}

/*
 * Index is probably not correctly implemented, but returns 0.
 */
static uint64_t Index_m ( Rd_cl	*self )
{
    return 0;
}

/*
 * We are intermittent.
 */
static bool_t Intermittent_m ( Rd_cl	*self )
{
    return True;
}

/*
 * We are not seekable
 */
static bool_t Seekable_m ( Rd_cl	*self )
{
    return False;
}

/*
 * Acquire the lock
 */
static void Lock_m ( Rd_cl	*self )
{
    EB_t *st = self->st;
    MU_LOCK(&(st->mu));
}

static void Unlock_m ( Rd_cl	*self )
{
    EB_t *st = self->st;
    MU_RELEASE(&(st->mu));
}

static int8_t LGetC_m ( Rd_cl	*self )
{
    EB_t *st = self->st;
    
    if ( st->ungetted != UNGETC_NOCHAR )  { 
	st->lastread = st->ungetted;
	st->ungetted = UNGETC_NOCHAR;
	return st->lastread;
    } 
    
    if ( st->point == st->length ) {
	GetNewLine( st );
    }
    
    st->lastread = st->ch[ (st->point)++ ];
    return st->lastread;
}

static bool_t LEOF_m ( Rd_cl	*self )
{
    EB_t *st = self->st;
    
    st->ungetted = st->lastread; /* Nobble further UnGetCs */
#undef EOF /* XXX - HACK */
    return ( st->point == st->length && Rd$EOF( st->rd ) );
}
