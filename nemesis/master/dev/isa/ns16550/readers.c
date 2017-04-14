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
**      Nemesis Device Driver
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Driver for UART devices: serial readers and writers
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: readers.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#undef EOF
#include <string.h>
#include <mutex.h>
#include <exceptions.h>

#include "Serial_st.h"


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
Rd_op rd_op = {
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

static Wr_PutC_fn Wr_PutC_m;
static Wr_PutStr_fn Wr_PutStr_m;
static Wr_PutChars_fn Wr_PutChars_m;
static Wr_Seek_fn Wr_Seek_m;
static Wr_Flush_fn Wr_Flush_m;
static Wr_Close_fn Wr_Close_m;
static Wr_Length_fn Wr_Length_m;
static Wr_Index_fn Wr_Index_m;
static Wr_Seekable_fn Wr_Seekable_m;
static Wr_Buffered_fn Wr_Buffered_m;
static Wr_Lock_fn Wr_Lock_m;
static Wr_Unlock_fn Wr_Unlock_m;
static Wr_LPutC_fn Wr_LPutC_m;
static Wr_LPutStr_fn Wr_LPutStr_m;
static Wr_LPutChars_fn Wr_LPutChars_m;
static Wr_LFlush_fn Wr_LFlush_m;
Wr_op wr_op = {
    Wr_PutC_m,
    Wr_PutStr_m,
    Wr_PutChars_m,
    Wr_Seek_m,
    Wr_Flush_m,
    Wr_Close_m,
    Wr_Length_m,
    Wr_Index_m,
    Wr_Seekable_m,
    Wr_Buffered_m,
    Wr_Lock_m,
    Wr_Unlock_m,
    Wr_LPutC_m,
    Wr_LPutStr_m,
    Wr_LPutChars_m,
    Wr_LFlush_m,
};

/*****************************************************************/

#ifdef KDEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif

#ifdef TRACE_SERIAL_RD
#define TRC(_x) _x
#else
#define TRC(_x) 
#endif

#define UNGETC_NOCHAR (1000)

/*
 * Initialise the reader section of the serial structure
 */
void SerialInitRd( Serial_st *st )
{
    TRC(pr("SerialRdInit: called.\n"));
    MU_INIT (&(st->rd.mu));
    CV_INIT (&(st->rd.cv));

    st->rd.lastread = UNGETC_NOCHAR;
    st->rd.ungetted = UNGETC_NOCHAR;
    st->rd.current_client=0;
}

/*
 * Initialise the writer section of the serial structure
 */
void SerialInitWr(Serial_st *st)
{
    TRC(pr("SerialWrInit: called.\n"));
    MU_INIT (&(st->wr.mu));
    CV_INIT (&(st->wr.cv));
}


/*----------------Reader methods-------------------------------------*/

void RdSrvThread(addr_t data)
{
    Closure$Apply((Closure_clp) data);
    fprintf(stderr,"serial: Reader server thread dying.\n");
}

/*
** The reader has a callback to enable it to stop multiple people from
** trying to read at the same time. 
*/
static IDCCallback_Request_fn  Request_m;
static IDCCallback_Bound_fn    Bound_m;
static IDCCallback_Closed_fn   Closed_m;

IDCCallback_op rd_cb_op = {
    Request_m,
    Bound_m,
    Closed_m
};

static bool_t Request_m(IDCCallback_cl *self, IDCOffer_cl *offer, 
			Domain_ID client, ProtectionDomain_ID pdid,
			const Binder_Cookies *clt_cks, 
			/* OUT */ Binder_Cookies *svr_cks)
{
    Serial_st *st = self->st;

    /* We get called here if someone has wishes to bind to our offer */

    if (st->rd.current_client) {
        eprintf("Serial: domain %qx already bound => bad luck.\n",
		st->rd.current_client);
	return False;
    }
    
    return True;
}

static bool_t Bound_m(IDCCallback_cl *self, IDCOffer_cl *offer, 
		    IDCServerBinding_cl *binding, Domain_ID client,
		    ProtectionDomain_ID pdid, 
		    const Binder_Cookies *clt_cks, Type_Any *server)
{
    Serial_st *st = self->st;

    /* We get called here if someone has successfully bound to our offer */

    TRC(eprintf("Serial: successful bind to reader by domain %qx\n", client));
    st->rd.current_client = client;
    return True;

}

static void Closed_m(IDCCallback_cl *self, IDCOffer_cl *offer, 
		     IDCServerBinding_cl *binding, 
		     const Type_Any *any)
{
    Serial_st *st = self->st;
    st->rd.current_client = 0;
    return;
}

/*
 * GetC checks the unget character, and if there is none, does that
 * standard condition-variable thang on the buffer.
 */
static int8_t Rd_GetC_m ( Rd_cl	*self )
{
    Serial_st *st = self->st;
    int8_t res =0;

    MU_LOCK(&(st->rd.mu));

    if ( st->rd.ungetted != UNGETC_NOCHAR )  {
	res = st->rd.ungetted;
	st->rd.ungetted = UNGETC_NOCHAR;
    } else {
	while( BUF_IS_EMPTY( &(st->rxbuf ) )) {
	    WAIT( &(st->rd.mu), &(st->rd.cv) );
	}
	res = SerialGetcBuffer( &(st->rxbuf) );
    }
    st->rd.lastread = res;

    MU_RELEASE(&(st->rd.mu));

    return res;
}

/*
 * EOF is always false for the serial line. Stubs may override this
 * method for local bindings.
 */
static bool_t Rd_EOF_m ( Rd_cl *self )
{
#ifdef WHY
    Serial_st *st = self->st;

    USING(MUTEX, &(st->rd.mu),
	  st->rd.ungetted = st->rd.lastread; /* Nobble further UnGetCs */
	);
#endif 

    return False;
}

/*
 * UnGetC pushes the last char read into the buffer.
 */
static void Rd_UnGetC_m ( Rd_cl	*self )
{
    Serial_st *st = self->st;

    USING (MUTEX, &(st->rd.mu),
	 
	   if ( st->rd.ungetted == UNGETC_NOCHAR ) {
	       RAISE_Rd$Failure( 3 ); /* 3 = can't UnGetC */
	   }
	 
	   st->rd.ungetted = st->rd.lastread;
	);
}

/*
 * Simple read of the buffer size. 
 */
static uint64_t Rd_CharsReady_m ( Rd_cl	*self )
{
    Serial_st *st = self->st;

    return st->rxbuf.size - st->rxbuf.free;
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
    Serial_st *st = self->st;
    int i = 0;

    MU_LOCK(&(st->rd.mu));
	 
    for( i = 0; i < nb; i++ ) {
	if ( st->rd.ungetted != UNGETC_NOCHAR )  {
	    buf[i] = st->rd.ungetted;
	    st->rd.ungetted = UNGETC_NOCHAR;
	} else {
	    while( BUF_IS_EMPTY( &(st->rxbuf ) )) {
		WAIT( &(st->rd.mu), &(st->rd.cv) );
	    }
	    buf[i] = SerialGetcBuffer( &(st->rxbuf) );
	}
	st->rd.lastread = buf[i];
    }
    MU_RELEASE(&(st->rd.mu));
    return i;
}

/*
 * GetLine is very similar to GetChars.
 */
static uint64_t Rd_GetLine_m ( Rd_cl	*self,
			       Rd_Buffer buf	/* IN */,
			       uint64_t	nb	/* IN */ )
{
    Serial_st *st = self->st;
    bool_t nocr = True;
    int i;

    MU_LOCK(&(st->rd.mu));
    for( i = 0; i < nb && nocr; i++ ) {
	if ( st->rd.ungetted != UNGETC_NOCHAR )  {
	    buf[i] = st->rd.ungetted;
	    st->rd.ungetted = UNGETC_NOCHAR;
	} else {
	    while( BUF_IS_EMPTY( &(st->rxbuf ) )) {
		WAIT( &(st->rd.mu), &(st->rd.cv) );
	    }
	    buf[i] = SerialGetcBuffer( &(st->rxbuf) );
	}
	st->rd.lastread = buf[i];
	if ( buf[i] == '\n' ) nocr = False;
    }
    MU_RELEASE(&(st->rd.mu));
    return i;
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
 * We can't close this either. XXX
 */
static void Rd_Close_m ( Rd_cl	*self )
{
}

/* 
 * Length raises Failure(4) since the length is unknown (potentially
 * infinite).
 */
static uint64_t Rd_Length_m ( Rd_cl	*self )
{
    RAISE_Rd$Failure(4);

    return 0;
}

/*
 * Index is probably not correctly implemented, but returns -1.
 * XXX maybe should return count of characters read since reader was opened?
 */
static uint64_t Rd_Index_m ( Rd_cl	*self )
{
    return -1;
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
    Serial_st *st = self->st;
    MU_LOCK(&(st->rd.mu));
}

static void Rd_Unlock_m ( Rd_cl	*self )
{
    Serial_st *st = self->st;
    MU_RELEASE(&(st->rd.mu));
}

static int8_t Rd_LGetC_m ( Rd_cl	*self )
{
    Serial_st *st = self->st;
    int8_t res;
  
    if ( st->rd.ungetted != UNGETC_NOCHAR )  {
	res = st->rd.ungetted;
	st->rd.ungetted = UNGETC_NOCHAR;
    } else {
	while( BUF_IS_EMPTY( &(st->rxbuf ) )) {
	    WAIT( &(st->rd.mu), &(st->rd.cv) );
	}
	res = SerialGetcBuffer( &(st->rxbuf) );
    }
    st->rd.lastread = res;
    return res;
}

static bool_t Rd_LEOF_m ( Rd_cl	*self )
{
#ifdef WHY
    Serial_st *st = self->st;

    st->rd.ungetted = st->rd.lastread; /* Nobble further UnGetCs */
#endif
    return False;
}

/*----------------Writer methods-------------------------------------*/

static void Wr_PutC_m(Wr_cl *self, int8_t ch)
{
    Serial_st *st = self->st;

    USING(MUTEX, &(st->wr.mu),
	  Wr_LPutC_m(self, ch);
	);
}

static void Wr_PutStr_m(Wr_cl *self, string_t string)
{
    Serial_st *st = self->st;

    USING(MUTEX, &(st->wr.mu),
	  Wr_LPutStr_m(self, string);
	);
}

static void Wr_PutChars_m(Wr_cl *self, Wr_Buffer s, uint64_t nb)
{
    Serial_st *st = self->st;

    USING(MUTEX, &(st->wr.mu),
	  Wr_LPutChars_m(self, s, nb);
	);
}

static void Wr_Seek_m(Wr_cl *self, uint64_t n)
{
    RAISE_Wr$Failure(1);
}

static void Wr_Flush_m(Wr_cl *self)
{
    Serial_st *st = self->st;

    /* Push for transmission of the buffer contents. Wait until the
       buffer is empty. */
    USING(MUTEX, &(st->wr.mu),
	  Wr_LFlush_m(self);
	);
}

static void Wr_Close_m(Wr_cl *self)
{
    /* We don't do anything */
}

/* XXX should probably return count of characters written so far */
static uint64_t Wr_Length_m(Wr_cl *self)
{
    return 0;
}

/* Same as Length? */
static uint64_t Wr_Index_m(Wr_cl *self)
{
    return 0;
}

static bool_t Wr_Seekable_m(Wr_cl *self)
{
    return False;
}

static bool_t Wr_Buffered_m(Wr_cl *self)
{
    return True;
}

static void Wr_Lock_m(Wr_cl *self)
{
    Serial_st *st = self->st;

    MU_LOCK(&(st->wr.mu));
}

static void Wr_Unlock_m(Wr_cl *self)
{
    Serial_st *st = self->st;

    MU_RELEASE(&(st->wr.mu));
}

static void Wr_LPutC_m(Wr_cl *self, int8_t ch)
{
    Wr_LPutChars_m(self, &ch, 1);
}

static void Wr_LPutStr_m(Wr_cl *self, string_t s)
{
    Wr_LPutChars_m(self, s, strlen(s));
}

static void Wr_LPutChars_m(Wr_cl *self, Wr_Buffer s, uint64_t nb)
{
    Serial_st *st = self->st;

    Serial_PutChars(st, (uchar_t *)s, nb);
}

static void Wr_LFlush_m(Wr_cl *self)
{
    Serial_st *st = self->st;

    if (!BUF_IS_EMPTY(&(st->txbuf)))
	WAIT(&(st->wr.mu), &(st->wr.cv));
}
