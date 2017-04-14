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
**      buffer
** 
** FUNCTIONAL DESCRIPTION:
** 
**      A buffer between a reader and a writer
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: pipe.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <string.h>
#include <mutex.h>
#include <exceptions.h>
#include <Wr.ih>
#include <Rd.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static  Wr_PutC_fn              Wr_PutC_m;
static  Wr_PutStr_fn            Wr_PutStr_m;
static  Wr_PutChars_fn          Wr_PutChars_m;
static  Wr_Seek_fn              Wr_Seek_m;
static  Wr_Flush_fn             Wr_Flush_m;
static  Wr_Close_fn             Wr_Close_m;
static  Wr_Length_fn            Wr_Length_m;
static  Wr_Index_fn             Wr_Index_m;
static  Wr_Seekable_fn          Wr_Seekable_m;
static  Wr_Buffered_fn          Wr_Buffered_m;
static  Wr_Lock_fn              Wr_Lock_m;
static  Wr_Unlock_fn            Wr_Unlock_m;
static  Wr_LPutC_fn             Wr_LPutC_m;
static  Wr_LPutStr_fn           Wr_LPutStr_m;
static  Wr_LPutChars_fn         Wr_LPutChars_m;
static  Wr_LFlush_fn            Wr_LFlush_m;

static  Rd_GetC_fn              Rd_GetC_m;
static  Rd_EOF_fn               Rd_EOF_m;
static  Rd_UnGetC_fn            Rd_UnGetC_m;
static  Rd_CharsReady_fn        Rd_CharsReady_m;
static  Rd_GetChars_fn          Rd_GetChars_m;
static  Rd_GetLine_fn           Rd_GetLine_m;
static  Rd_Seek_fn              Rd_Seek_m;
static  Rd_Close_fn             Rd_Close_m;
static  Rd_Length_fn            Rd_Length_m;
static  Rd_Index_fn             Rd_Index_m;
static  Rd_Intermittent_fn      Rd_Intermittent_m;
static  Rd_Seekable_fn          Rd_Seekable_m;
static  Rd_Lock_fn              Rd_Lock_m;
static  Rd_Unlock_fn            Rd_Unlock_m;
static  Rd_LGetC_fn             Rd_LGetC_m;
static  Rd_LEOF_fn              Rd_LEOF_m;

static  Wr_op   wr_ms = {
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
    Wr_LFlush_m
};

static  Rd_op   rd_ms = {
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
    Rd_LEOF_m
};

typedef struct {
    Rd_cl rd;
    Wr_cl wr;
    char *buffer;
    uint32_t bufsize;
    uint32_t trigger;

    mutex_t mu;
    condition_t cv;
    uint32_t wrp;
    uint32_t rdp;
    bool_t waiting; /* Someone is waiting to be signalled */
    bool_t rd_open, wr_open;

    mutex_t rd_mu;
    uint32_t lastc; /* Last character read from reader, or -1 if none */
    bool_t ungetc; /* Set if last character should be output next */

    mutex_t wr_mu;
} Pipe_st;


/* Free the Pipe_st structure */
static void cleanup(Pipe_st *st)
{
    MU_FREE(&st->mu);
    MU_FREE(&st->wr_mu);
    MU_FREE(&st->rd_mu);
    CV_FREE(&st->cv);
    FREE(st->buffer);
    FREE(st);
}

/*------------------------------------------------- Rd Entry Points ----*/

/* Call with the reader mutex and buffer mutex locked. */
static uint32_t rd_ready(Pipe_st *st)
{
    uint32_t r;

    if (st->rdp<=st->wrp) {
	/* We're chasing the writer pointer, or the buffer is empty */
	r=st->wrp-st->rdp;
    } else {
	/* The writer pointer has wrapped around, but we haven't */
	r=(st->bufsize-st->rdp)+st->wrp;
    }

    return r;
}

/* Call with the reader mutex and buffer mutex locked. Returns the
   number of characters put into "b"; if this is zero then we are at
   end-of-file. If "line" is True then we stop after a '\n'. */
static uint64_t rd_get(Pipe_st *st, char *b, uint64_t nb, bool_t line)
{
    uint64_t count=0;
    char *wp=b;
    uint32_t todo;

    while (nb>0) {
	/* Work out how much we can do in one go; either up to the write
	   pointer or the end of the buffer */
	if (st->rdp>st->wrp) {
	    todo=st->bufsize-st->wrp;
	} else {
	    todo=st->wrp-st->rdp;
	}

	if (todo>nb) todo=nb;

	if (todo==0) {
	    if (!st->wr_open) {
		/* The writer is closed and there's nothing left for us
		   to do. End of file! */
		break;
	    }
	    /* We have to wait for the writer to put more into the buffer */
	    st->waiting=True;
	    WAIT(&st->mu, &st->cv);
	    continue;
	}

	if (line) {
	    /* Scan for a newline, and truncate todo if we find one */
	    char *eol;
	    eol=memchr(st->buffer+st->rdp, '\n', todo);
	    if (eol) nb=todo=eol-(st->buffer+st->rdp)+1;
	}

	/* Now we know what we have to do */
	memcpy(wp, st->buffer+st->rdp, todo);
	st->rdp+=todo;
	wp+=todo;
	count+=todo;
	nb-=todo;
	/* ASSERT st->rdp<=bufsize */
	if (st->rdp>=st->bufsize) st->rdp=0;
    }
    /* If the writer is waiting for some space to be freed in the buffer,
       and there's more than "trigger" amount of space then kick it */
    if (st->waiting && (st->bufsize-rd_ready(st))>=st->trigger) {
	st->waiting=False;
	SIGNAL(&st->cv);
    }

    return count;
}

static bool_t rd_eof(Pipe_st *st)
{
    return ((st->wrp==st->rdp) && !st->wr_open);
}

static void rd_leave(Pipe_st *st)
{
    MU_RELEASE(&st->mu);
    MU_RELEASE(&st->rd_mu);
}

static void rd_enter(Pipe_st *st)
{
    MU_LOCK(&st->rd_mu);
    MU_LOCK(&st->mu);
    if (!st->rd_open) {
	rd_leave(st);
	RAISE_Rd$Failure(0);
    }
}

static int8_t Rd_GetC_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
    uint8_t c;
    uint64_t nb;

    rd_enter(st);

    if (st->ungetc) {
	c=st->lastc;
	st->ungetc=False;
	nb=1;
    } else {
	nb=rd_get(st, &c, 1, False);
    }

    MU_RELEASE(&st->mu);
    if (nb==0) {
	MU_RELEASE(&st->rd_mu);
	RAISE_Rd$EndOfFile();
    }
    st->lastc=c;
    MU_RELEASE(&st->rd_mu);
    return c;
}

static bool_t Rd_EOF_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
    bool_t eof;

    rd_enter(st);

    if (st->ungetc) {
	/* Obviously not EOF because we have at least one character
	   left */
	eof=False;
    } else {
	eof=rd_eof(st);
    }
    rd_leave(st);

    return eof;
}

static void Rd_UnGetC_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
    bool_t ok;

    rd_enter(st);

    if (st->ungetc || st->lastc==-1) {
	ok=False; /* Already done an UnGetC, or no last character */
    } else {
	st->ungetc=True;
	ok=True;
    }

    rd_leave(st);
    if (!ok) RAISE_Rd$Failure(3); /* Can't unget */
}

static uint64_t Rd_CharsReady_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
    uint64_t ready=0;

    rd_enter(st);

    if (st->ungetc) ready++; /* The ungetted character counts for one */

    ready+=rd_ready(st);

    if (ready==0 && rd_eof(st)) ready=1; /* EOF counts as a character */

    rd_leave(st);
    return ready;
}

static uint64_t Rd_GetChars_m (
        Rd_cl   *self,
        Rd_Buffer       buf     /* IN */,
        uint64_t        nb      /* IN */ )
{
    Pipe_st *st = self->st;
    Rd_Buffer b=buf;
    uint64_t count=0;

    rd_enter(st);

    if (st->ungetc && nb>0) {
	*b++=st->lastc;
	count++;
	nb--;
	st->ungetc=False;
    }

    if (nb>0) {
	count+=rd_get(st, b, nb, False);
    }
    if (count>0) st->lastc=buf[count-1];

    rd_leave(st);
    return count;
}

static uint64_t Rd_GetLine_m (
        Rd_cl   *self,
        Rd_Buffer       buf     /* IN */,
        uint64_t        nb      /* IN */ )
{
    Pipe_st *st = self->st;
    Rd_Buffer b=buf;
    uint64_t count=0;

    rd_enter(st);

    if (st->ungetc && nb>0) {
	*b++=st->lastc;
	count++;
	nb--;
	st->ungetc=False;
	if (st->lastc=='\n') nb=0; /* Terminate read here - it's a newline */
    }

    if (nb>0) {
	count+=rd_get(st, b, nb, True);
    }
    if (count>0) st->lastc=buf[count-1];

    rd_leave(st);
    return count;
}

static void Rd_Seek_m (
        Rd_cl   *self,
        uint64_t        nb      /* IN */ )
{
    RAISE_Rd$Failure(1); /* Unseekable */
}

static void Rd_Close_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;

    rd_enter(st);

    st->rd_open=False;
    /* If the writer is waiting for any reason at all, kick it so that it
       notices we've gone. If there is no writer it shouldn't be waiting... */
    if (st->waiting) {
	st->waiting=False;
	SIGNAL(&st->cv);
    }

    rd_leave(st);
    if (!st->wr_open) cleanup(st); /* Free everything if both ends are
				      closed */
}

static uint64_t Rd_Length_m (
        Rd_cl   *self )
{
    RAISE_Rd$Failure(4); /* Length unknown */
    return 0;
}

static uint64_t Rd_Index_m (
        Rd_cl   *self )
{
    RAISE_Rd$Failure(1); /* Unseekable */
    return 0;
}

static bool_t Rd_Intermittent_m (
        Rd_cl   *self )
{
    return True; /* We don't known when data's turning up */
}

static bool_t Rd_Seekable_m (
        Rd_cl   *self )
{
    return False; /* Not seekable */
}

static void Rd_Lock_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;

    MU_LOCK(&st->rd_mu);
    if (!st->rd_open) {
	MU_RELEASE(&st->rd_mu);
	RAISE_Rd$Failure(0); /* Technically can't be raised, but seems like
				the only sensible thing to do. */
    }
}

static void Rd_Unlock_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
  
    MU_RELEASE(&st->rd_mu);
}

static int8_t Rd_LGetC_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
    uint8_t c;
    uint64_t nb;

    if (st->ungetc) {
	c=st->lastc;
	st->ungetc=False;
	nb=1;
    } else {
	MU_LOCK(&st->mu);
	nb=rd_get(st, &c, 1, False);
	MU_RELEASE(&st->mu);
    }

    if (nb==0) {
	RAISE_Rd$EndOfFile();
    }
    st->lastc=c;
    return c;
}

static bool_t Rd_LEOF_m (
        Rd_cl   *self )
{
    Pipe_st *st = self->st;
    bool_t eof;

    if (st->ungetc) {
	/* Obviously not EOF because we have at least one character
	   left */
	eof=False;
    } else {
	MU_LOCK(&st->mu);
	eof=rd_eof(st);
	MU_RELEASE(&st->mu);
    }
    return eof;
}

/*------------------------------------------------- Wr Entry Points ----*/

/* Call only when writer mutex and buffer mutex are locked */
static bool_t wr_put(Pipe_st *st, Wr_Buffer s, uint64_t nb)
{
    uint32_t todo;

    while (nb>0) {
	if (!st->rd_open) {
	    /* The reader side of the pipe has been closed. We can't write
	       any more data, so we should raise Wr$Failure(0) on this
	       side. */
	    return False; /* Reader is closed -> failure */
	}
	/* Write up to the end of the buffer, the position of the reader,
	   or the end of the input data */
	if (st->wrp<st->rdp) {
	    /* Free space bounded by reader pointer */
	    todo=st->rdp-st->wrp-1;
	} else {
	    /* Free space bounded by end of buffer, unless rdp is zero */
	    todo=st->bufsize-st->wrp-(st->rdp==0);
	}

	/* If there's more free space than bytes to write, we're limited
	   by the available data */
	if (todo>nb) todo=nb;

	if (todo==0) {
	    /* The reader will have to read some data before we can put
	       any more in; if they are blocked then signal them. */
	    if (st->waiting) {
		st->waiting=False;
		SIGNAL(&st->cv);
	    }
	    /* Wait for there to be some free space */
	    st->waiting=True;
	    WAIT(&st->mu, &st->cv);
	    /* We should be signalled when the reader has cleared some
	       data from the buffer, or when it is closed. */
	    continue;
	}
	/* Copy the data into the buffer */
	memcpy(st->buffer+st->wrp, s, todo);
	s+=todo;
	st->wrp+=todo;
	nb-=todo;
	/* ASSERT st->wrp<=st->bufsize */
	if (st->wrp>=st->bufsize) st->wrp=0; /* Wrap around to start */
    }
    /* If the reader was waiting, signal them - we have put some data in */
    if (st->waiting && rd_ready(st)>st->trigger) {
	st->waiting=False;
	SIGNAL(&st->cv);
    }
    return True;
}

/* Call only when writer mutex and buffer mutex are locked */
static bool_t wr_flush(Pipe_st *st)
{
    bool_t empty;

    do {
	empty = (st->wrp==st->rdp);
	if (!empty) {
	    if (!st->rd_open) {
		return False; /* Reader closed, not all data flushed */
	    }
	    /* The reader might be stalled; if it is, give it a kick */
	    if (st->waiting) {
		st->waiting=False;
		SIGNAL(&st->cv);
	    }
	    /* Now we wait to see what happens. */
	    st->waiting=True;
	    WAIT(&st->mu, &st->cv);
	}
    } while (!empty);
    return True;
}

static void wr_leave(Pipe_st *st)
{
    MU_RELEASE(&st->mu);
    MU_RELEASE(&st->wr_mu);
}

static void wr_enter(Pipe_st *st)
{
    MU_LOCK(&st->wr_mu);
    MU_LOCK(&st->mu);
    if (!st->wr_open) {
	wr_leave(st);
	RAISE_Wr$Failure(0); /* Closed */
    }
}

static void Wr_PutC_m (
        Wr_cl   *self,
        int8_t  ch      /* IN */ )
{
    Pipe_st *st = self->st;
    bool_t ok;

    wr_enter(st);

    ok=wr_put(st, &ch, 1);

    wr_leave(st);

    if (!ok) {
	RAISE_Wr$Failure(0); /* Reader closed */
    }
}

static void Wr_PutStr_m (
        Wr_cl   *self,
        string_t        s       /* IN */ )
{
    Pipe_st *st = self->st;
    bool_t ok=True;
    uint64_t len;

    wr_enter(st);

    len=strlen(s);

    if (len>0) {
	ok=wr_put(st, s, len);
    }

    wr_leave(st);

    if (!ok) {
	RAISE_Wr$Failure(0); /* Reader closed */
    }
}

static void Wr_PutChars_m (
        Wr_cl   *self,
        Wr_Buffer       s       /* IN */,
        uint64_t        nb      /* IN */ )
{
    Pipe_st *st = self->st;
    bool_t ok;

    wr_enter(st);

    ok=wr_put(st, s, nb);

    wr_leave(st);

    if (!ok) {
	RAISE_Wr$Failure(0);
    }
}

static void Wr_Seek_m (
        Wr_cl   *self,
        uint64_t        n       /* IN */ )
{
    RAISE_Wr$Failure(1);
    return; /* Can't seek on a pipe */
}

static void Wr_Flush_m (
        Wr_cl   *self )
{
    Pipe_st *st = self->st;
    bool_t ok;

    wr_enter(st);

    ok=wr_flush(st);

    wr_leave(st);

    if (!ok) {
	RAISE_Wr$Failure(0);
    }
}

static void Wr_Close_m (
        Wr_cl   *self )
{
    Pipe_st *st = self->st;
    bool_t ok;

    wr_enter(st);

    ok=wr_flush(st);
    st->wr_open=False;

    /* If the reader is waiting for input, we need to signal it to let
       it know we are closed */
    if (st->waiting) {
	st->waiting=False;
	SIGNAL(&st->cv);
    }

    wr_leave(st);

    if (!st->rd_open) cleanup(st); /* Free everything */

    if (!ok) RAISE_Wr$Failure(0); /* Not all data was read by the reader */
}

static uint64_t Wr_Length_m (
        Wr_cl   *self )
{
    RAISE_Wr$Failure(1);
    return 0;
}

static uint64_t Wr_Index_m (
        Wr_cl   *self )
{
    RAISE_Wr$Failure(1);
    return 0;
}

static bool_t Wr_Seekable_m (
        Wr_cl   *self )
{
    return False;
}

static bool_t Wr_Buffered_m (
        Wr_cl   *self )
{
    return True;
}

static void Wr_Lock_m (
        Wr_cl   *self )
{
    Pipe_st *st = self->st;

    MU_LOCK(&st->wr_mu);
    if (!st->wr_open) {
	MU_RELEASE(&st->wr_mu);
	RAISE_Wr$Failure(0); /* Technically we're not supposed to do this,
				but it seems like the only sensible thing
				to do. */
    }
}

static void Wr_Unlock_m (
        Wr_cl   *self )
{
    Pipe_st *st = self->st;

    MU_RELEASE(&st->wr_mu);
}

static void Wr_LPutC_m (
        Wr_cl   *self,
        int8_t  ch      /* IN */ )
{
    Pipe_st *st = self->st;
    bool_t ok;

    MU_LOCK(&st->mu);
    ok=wr_put(st, &ch, 1);
    MU_RELEASE(&st->mu);
    if (!ok) RAISE_Wr$Failure(0);
}

static void Wr_LPutStr_m (
        Wr_cl   *self,
        string_t        s       /* IN */ )
{
    Pipe_st *st = self->st;
    uint64_t len;
    bool_t ok;

    len=strlen(s);
    if (len>0) {
	MU_LOCK(&st->mu);
	ok=wr_put(st, s, len);
	MU_RELEASE(&st->mu);
	if (!ok) RAISE_Wr$Failure(0);
    }
}

static void Wr_LPutChars_m (
        Wr_cl   *self,
        Wr_Buffer       s       /* IN */,
        uint64_t        nb      /* IN */ )
{
    Pipe_st *st = self->st;
    bool_t ok;

    MU_LOCK(&st->mu);
    ok=wr_put(st, s, nb);
    MU_RELEASE(&st->mu);
    if (!ok) RAISE_Wr$Failure(0);
}

static void Wr_LFlush_m (
        Wr_cl   *self )
{
    Pipe_st *st = self->st;
    bool_t ok;

    MU_LOCK(&st->mu);
    ok=wr_flush(st);
    MU_RELEASE(&st->mu);
    if (!ok) RAISE_Wr$Failure(0);
}

/*---------------------------------------- Constructor Entry Points ----*/

Rd_clp CreatePipe(Heap_clp heap, uint32_t bufsize, uint32_t trigger,
		  Wr_clp *wr)
{
    Pipe_st *st;

    st=Heap$Malloc(heap, sizeof(*st));
    if (!st) {
	RAISE_Heap$NoMemory();
    }

    st->buffer=Heap$Malloc(heap, bufsize);
    if (!st->buffer) {
	FREE(st);
	RAISE_Heap$NoMemory();
    }

    CL_INIT(st->rd, &rd_ms, st);
    CL_INIT(st->wr, &wr_ms, st);

    if (bufsize<2) bufsize=2;
    if (trigger>=bufsize) trigger=(bufsize>>1); /* If the trigger value
						   is invalid then fix it */

    st->bufsize=bufsize;
    st->trigger=trigger;
    st->wrp=0;
    st->rdp=0;

    st->ungetc=False;
    st->lastc=-1; /* No character is ungetted */
    MU_INIT(&st->mu);
    CV_INIT(&st->cv);
    MU_INIT(&st->wr_mu);
    MU_INIT(&st->rd_mu);

    st->rd_open=True;
    st->wr_open=True;

    *wr=&st->wr;
    return &st->rd;
}
