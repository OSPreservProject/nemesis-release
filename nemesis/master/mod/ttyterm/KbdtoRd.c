/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Keyboard driver translation module
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Translates from Kbd to Rd interfaces
** 
** ENVIRONMENT: 
**
**      User space module
** 
** ID : $Id: KbdtoRd.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>
#include <time.h>
#include <stdio.h>
#include <IOMacros.h>
#include <keysym.h>
#include <SRCThread.ih>

#include <Kbd.ih>
#include <Rd.ih>

typedef struct _rd_st {
    IO_clp io;
    uint32_t lastread;
    uint32_t ungetted;
    bool_t shifted;
    Kbd_clp kbd;
    SRCThread_Mutex mu;
} Rd_st;

/*****************************************************************/
/*
 * Module stuff : Reader closure
 */
static Rd_GetC_fn	Rd_GetC_m;
static Rd_EOF_fn 	Rd_EOF_m;
static Rd_UnGetC_fn 	Rd_UnGetC_m;
static Rd_CharsReady_fn Rd_CharsReady_m;
static Rd_GetChars_fn	Rd_GetChars_m;
static Rd_GetLine_fn 	Rd_GetLine_m;
static Rd_Seek_fn 	Rd_Seek_m;
static Rd_Close_fn 	Rd_Close_m;
static Rd_Length_fn 	Rd_Length_m;
static Rd_Index_fn 	Rd_Index_m;
static Rd_Intermittent_fn Rd_Intermittent_m;
static Rd_Seekable_fn 	Rd_Seekable_m;
static Rd_Lock_fn	Rd_Lock_m;
static Rd_Unlock_fn 	Rd_Unlock_m;
static Rd_LGetC_fn 	Rd_LGetC_m;
static Rd_LEOF_fn 	Rd_LEOF_m;
static Rd_op rd_op = {
    Rd_GetC_m,
    Rd_EOF_m,
    Rd_UnGetC_m,
    Rd_CharsReady_m,
    Rd_GetChars_m,
    Rd_GetLine_m,
    Rd_Seek_m,
    Rd_Close_m,
    Rd_Length_m,
    Rd_Index_m,
    Rd_Intermittent_m,
    Rd_Seekable_m,
    Rd_Lock_m,
    Rd_Unlock_m,
    Rd_LGetC_m,
    Rd_LEOF_m,
};

#define UNGETC_NOCHAR (1000)

/*----------------Reader methods-------------------------------------*/


#define CTRL(c) ( (c) - '@' )

static uint8_t fetch(Rd_st *st)
{
    uint32_t res;
    IO_Rec rec;
    Kbd_Event *ev;
    uint32_t val, nrecs;

    res=0;
    while (res==0) {
	IO$GetPkt(st->io, 1, &rec, FOREVER, &nrecs, &val);
	/* Ignore key up events */
	ev=(void *)rec.base;
	if (ev->state == Kbd_State_Down)
	    res = ev->key;
	IO$PutPkt(st->io, 1, &rec, 1, FOREVER);
    }
    switch (res) {
    case XK_Delete:
	res = 127;
	break;
    }

    return res;
}

/*
 * GetC checks the unget character, and if there is none, does that
 * standard condition-variable thang on the buffer.
 */
static int8_t Rd_GetC_m ( Rd_cl	*self )
{
    Rd_st *st = self->st;
    int8_t res =0;

    MU_LOCK(&(st->mu));

    if ( st->ungetted != UNGETC_NOCHAR )  {
	res = st->ungetted;
	st->ungetted = UNGETC_NOCHAR;
    } else {
	res=fetch(st);
    }
    st->lastread = res;

    MU_RELEASE(&(st->mu));

    return res;
}

/*
 * EOF is always false for the serial line. Stubs may override this
 * method for local bindings.
 */
static bool_t Rd_EOF_m ( Rd_cl *self )
{
    Rd_st *st = self->st;

    USING(MUTEX, &(st->mu),
	  st->ungetted = st->lastread; /* Nobble further UnGetCs */
	);

    return False;
}

/*
 * UnGetC pushes the last char read into the buffer.
 */
static void Rd_UnGetC_m ( Rd_cl	*self )
{
    Rd_st *st = self->st;

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
static uint64_t Rd_CharsReady_m ( Rd_cl	*self )
{
    return 0;
}

/*
 * Get a number of characters into the buffer. 
 * Under the lock we still maintain the invariants concerning UnGetC,
 * since it is possible someone else could come in when we do the WAIT.
 *
 */
static uint64_t Rd_GetChars_m ( Rd_cl	*self,
				Rd_Buffer	buf	/* IN */,
				uint64_t	nb	/* IN */ )
{
    Rd_st *st = self->st;
    uint64_t i = 0;

    MU_LOCK(&(st->mu));
	 
    for( i = 0; i < nb; i++ ) {
	if ( st->ungetted != UNGETC_NOCHAR )  {
	    buf[i] = st->ungetted;
	    st->ungetted = UNGETC_NOCHAR;
	} else {
	    buf[i] = fetch(st);
	}
	st->lastread = buf[i];
    }
    MU_RELEASE(&(st->mu));
    return i + 1;
}

/*
 * GetLine is very similar to GetChars.
 */
static uint64_t Rd_GetLine_m ( Rd_cl	*self,
			       Rd_Buffer	buf	/* IN */,
			       uint64_t	nb	/* IN */ )
{
    Rd_st *st = self->st;
    bool_t nocr = True;
    uint64_t i;

    MU_LOCK(&(st->mu));
    for( i = 0; i < nb && nocr; i++ ) {
	if ( st->ungetted != UNGETC_NOCHAR )  {
	    buf[i] = st->ungetted;
	    st->ungetted = UNGETC_NOCHAR;
	} else {
	    buf[i] = fetch(st);
	}
	st->lastread = buf[i];
	if ( buf[i] == '\n' ) nocr = False;
    }
    MU_RELEASE(&(st->mu));
    return nb;
}

/*
 * We are not seekable.
 */
static void Rd_Seek_m ( Rd_cl	*self,
			uint64_t	nb	/* IN */ )
{
    RAISE_Rd$Failure(1) /* 1 == not seekable */
	}

/*
 * We can't close this either. 
 * XXX why not? Somebody implement this sometime.
 */
static void Rd_Close_m ( Rd_cl	*self )
{

}

/* 
 * The length is unknown (potentially infinite).
 */
static uint64_t Rd_Length_m ( Rd_cl	*self )
{
    RAISE_Rd$Failure(4);

    return 0;
}

/*
 * Index is probably not correctly implemented, but returns 0.
 */
static uint64_t Rd_Index_m ( Rd_cl	*self )
{
    return 0;
}

/*
 * We are intermittent.
 */
static bool_t Rd_Intermittent_m ( Rd_cl	*self )
{
    return True;
}

/*
 * We are not seekable
 */
static bool_t Rd_Seekable_m ( Rd_cl	*self )
{
    return False;
}

/*
 * Acquire the lock
 */
static void Rd_Lock_m ( Rd_cl	*self )
{
    Rd_st *st = self->st;
    MU_LOCK(&(st->mu));
}

static void Rd_Unlock_m ( Rd_cl	*self )
{
    Rd_st *st = self->st;
    MU_RELEASE(&(st->mu));
}

static int8_t Rd_LGetC_m ( Rd_cl	*self )
{
    Rd_st *st = self->st;
    int8_t res;
  
    if ( st->ungetted != UNGETC_NOCHAR )  {
	res = st->ungetted;
	st->ungetted = UNGETC_NOCHAR;
    } else {
	res = fetch(st);
    }
    st->lastread = res;
    return res;
}

static bool_t Rd_LEOF_m ( Rd_cl	*self )
{
    Rd_st *st = self->st;

    st->ungetted = st->lastread; /* Nobble further UnGetCs */
    return False;
}

/* ------------------------------------------------------------------- */

Rd_clp KbdReader_New(Kbd_clp kbd)
{
    Rd_st *st;
    Rd_clp rd;
    IO_Rec rec;
    IOData_Shm *shm;
    IOOffer_clp offer;
    uint8_t *base;
    int i;

    /* Allocate some state */
    rd     = Heap$Malloc(Pvs(heap), sizeof(*st)+sizeof(*rd));
    rd->op = &rd_op;
    rd->st = rd+1;
    st     = rd->st; 

    MU_INIT (&(st->mu));
    st->lastread = UNGETC_NOCHAR;
    st->ungetted = UNGETC_NOCHAR;
    st->shifted  = False;
    st->kbd      = kbd;

    offer  = Kbd$GetOffer(kbd);
    st->io = IO_BIND(offer);

    /* Stuff some recs down there */
    shm  = IO$QueryShm(st->io);
    base = SEQ_ELEM(shm, 0).buf.base; 
      
    for (i=0; i<4; i++) {
	rec.base = base + i*sizeof(Kbd_Event);
	rec.len  = sizeof(Kbd_Event);
	IO$PutPkt(st->io, 1, &rec, 1, FOREVER);
    }

    return rd;
}
