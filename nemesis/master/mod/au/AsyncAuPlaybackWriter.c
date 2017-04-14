/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
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
** ID : $Id: AsyncAuPlaybackWriter.c 1.1 Thu, 18 Feb 1999 14:19:27 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <ecs.h>
#include <assert.h>
#include <exceptions.h>
#include <IDCMacros.h>
#include <AuPlaybackWriter.ih>
#include <aumod_internals.h>

#define LOGSIZE 65536
#define TRC(_x)
#ifdef DOLOGGING
#define LOG(_x) _x
#else
#define LOG(_x)
#endif

AuPlaybackWriter_clp AuPlaybackWriter_New(const Au_Rec *au, int64_t latency, string_t streamname, int32_t *queue_length);

#define WRITERPOS(st) (st->writer_index % st->len)
#define READERPOS(st) (st->reader_index % st->len)
#define QUEUELENGTH(st) (st->writer_index - st->reader_index)
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED", __FUNCTION__);

uint32_t aubufloc(const Au_Rec *au);

#define AUBUFLOC(_x)  aubufloc(_x)


static	AuPlaybackWriter_Put_fn 		Async_AuPlaybackWriter_Put_m;
static  AuPlaybackWriter_Destroy_fn             Async_AuPlaybackWriter_Destroy_m;
static  AuPlaybackWriter_QueryQueueLength_fn             Async_AuPlaybackWriter_QueryQueueLength_m;

static  AuPlaybackWriter_op	auplaybackwriter_ms = {
  Async_AuPlaybackWriter_Put_m,
  Async_AuPlaybackWriter_Destroy_m,
  Async_AuPlaybackWriter_QueryQueueLength_m
};

/*---------------------------------------------------- Entry Points ----*/

#define QUANTUM 16384

void aubufthread(AsyncAuplaybackWriter_st *st) {
    uint64_t last;
    int played;
    fprintf(stderr,"playback: starting\n");
    while (1) {
	int32_t writer;
	int32_t writer_index;
	if (st->reader_index >= st->writer_index) {
	    fprintf(stderr,"playback blocking\n");
	    last = EC_READ(st->ec_writer);
    	    EC_AWAIT(st->ec_writer, last+1);
	}
	writer_index = st->writer_index;
	writer = writer_index % st->len;
	TRC(printf("aubufthread: writer %d reader %d\n", st->writer_index, st->reader_index));
	while (writer_index - st->reader_index > QUANTUM) {
	    int out;

	    out = QUANTUM;

	    if (READERPOS(st) + QUANTUM > st->len)
		out = st->len - READERPOS(st);

	    TRC(fprintf(stderr,"aubuf playing quantum\n"));
	    played = AuPlaybackWriter$Put(st->sync, st->buffer + READERPOS(st), out, 0, st->volume);
	    st->reader_index += played;
#ifdef BACKPRESSURE
	    EC_ADVANCE(st->ec_reader, 1);
#endif
	    if (played < QUANTUM && QUEUELENGTH(st)>((st->len * 7) / 8)) {
		PAUSE(SECONDS(1)/1000);
		break;
	    }

	    TRC(fprintf(stderr,"reader ec %qx\n", (uint64_t) EC_READ(st->ec_reader)));
	}
	
    }
}

AuPlaybackWriter_clp Async_AuPlaybackWriter_New(const Au_Rec *au, 
						int64_t minimum_latency, 
						uint32_t maximum_length,
						string_t streamname,
						Thread_clp *thread) {
    AsyncAuplaybackWriter_st *newst;
    newst = malloc(sizeof(*newst));
    newst -> closure.st = newst;
    newst -> closure.op = &auplaybackwriter_ms;
    newst -> sync = AuPlaybackWriter_New(au, 2048, streamname, NULL);
    if (!newst->sync) return;
    newst -> buffer = malloc(maximum_length * 4);
    newst -> writer_index = 0;
    newst -> reader_index = 0;
    newst -> ec_reader = Events$New(Pvs(evs));
    newst -> ec_writer = Events$New(Pvs(evs));
    newst -> len = maximum_length;
    newst -> start = NOW();
    newst -> volume = 16384;
    *thread = Threads$Fork(Pvs(thds), aubufthread, newst, 0);
    
    return &(newst->closure);
}

void delay(void) {
    PAUSE(SECONDS(1)/1000);
}

static uint32_t Async_AuPlaybackWriter_Put_m (
	AuPlaybackWriter_cl	*self,
	addr_t	samples	/* IN */,
	uint32_t	lengthin	/* IN */,
	bool_t	block	/* IN */,
	int32_t	volume	/* IN */ )
{
    AsyncAuplaybackWriter_st	*st = self->st;
    int32_t reader;
    int32_t spare;
    int32_t togo;
    int32_t *ptr;
    uint32_t length = lengthin;
    uint64_t i;
    st->volume = volume;
    spare = 0;

    while (length>0) {
	i = 0;
	while (spare < length) {
	    spare = st->len - (st->writer_index - st->reader_index); /* the spare is the amount of space in the circular buffer */
	    if (spare > length) break;
	    if (!block) return lengthin - length;
#ifndef BACKPRESSURE
	    delay();

#else
	    TRC(printf("secondary buffer full reader %d writer %d length %d block %d\n",
		       st->reader_index, st->writer_index, st->len, block));

	    i = EC_READ(st->ec_reader);	    
	    TRC(printf("await till %d\n", i+1));
	    EC_AWAIT(st->ec_reader, i+1);

	    
	    
	    TRC(printf("main thread woken up again\n"));
#endif	    
	}
	
	spare = st->len - (st->writer_index - st->reader_index);
	togo = length;
	if (spare < length) {	/* if there is not enough spare space */
	    togo = spare;
	}
	ptr = samples;
	TRC(printf("%d in buffer, %d to add, length %d\n", st->writer_index - st->reader_index, togo, st->len));
	if (WRITERPOS(st) + togo > st->len) {
	    int fragment;
	    int toend;
	    
	    toend = st->len - WRITERPOS(st);
	    fragment = togo - toend;
	    memcpy(st->buffer + WRITERPOS(st), ptr,toend*4);
	    ptr += toend;
	    memcpy(st->buffer, ptr, fragment * 4);
	    TRC(printf("S"));
	} else {
	    memcpy(st->buffer + WRITERPOS(st), ptr, togo*4);
	    TRC(printf("."));
	}
	
	st->writer_index += togo;
	
	TRC(printf("played %d writer %d reader %d\n", togo, st->writer_index, st->reader_index));
	EC_ADVANCE(st->ec_writer, 1);
	
	length -= togo;
	
    }
    return lengthin;
}

void Async_AuPlaybackWriter_Destroy_m(AuPlaybackWriter_clp self) {
  AsyncAuplaybackWriter_st	*st = self->st;

  AuPlaybackWriter$Destroy(st->sync);
  free(st);
}

int32_t Async_AuPlaybackWriter_QueryQueueLength_m(AuPlaybackWriter_clp self) {
    AsyncAuplaybackWriter_st *st = self->st;

    return (st->writer_index - st->reader_index) + AuPlaybackWriter$QueryQueueLength(st->sync);
}
/*
 * End 
 */
