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
** ID : $Id: AuPlaybackWriter.c 1.1 Thu, 18 Feb 1999 14:19:27 +0000 dr10009 $
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
#include <MixerRegister.ih>
#include <aumod_internals.h>
#ifdef DOLOGGING
#define LOG(_x) _x
#else
#define LOG(_x)
#endif

#define USEMIXER 1


#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);

uint32_t aubufloc(const Au_Rec *au) {
    uint32_t x;

    x = au->master_location;
    x += (NOW() - au->master_timestamp) / au->master_sample_period;
#if 0
    x -= au->master_granularity;
#endif
#if 0
    x += au->master_granularity;
#endif
    x -= 0;
    return x;
}

#define AUBUFLOC(_x)  aubufloc(_x)


static	AuPlaybackWriter_Put_fn 		AuPlaybackWriter_Put_m;
static  AuPlaybackWriter_Destroy_fn             AuPlaybackWriter_Destroy_m;
static  AuPlaybackWriter_QueryQueueLength_fn    AuPlaybackWriter_QueryQueueLength_m;

static  AuPlaybackWriter_op	auplaybackwriter_ms = {
  AuPlaybackWriter_Put_m,
  AuPlaybackWriter_Destroy_m,
  AuPlaybackWriter_QueryQueueLength_m
};

/* set up the buffer so that it is at the start position,
   ie (latency/period) samples ahead of where we calculate the
   sound card to be. 
   */


static void calibrate(AuPlaybackWriter_st *st) {
    int boost;
    int ptrlocb;
    st->ptrloc = AUBUFLOC(st->au);
    boost =  st->latency;
    st->ptrloc += boost;
    ptrlocb = st->ptrloc % st->au->length;
    TRC(printf("boost by %d\n", boost));
    st->ptr = ((int32_t *)(st->au->base)) + ptrlocb;
    st->toend = (st->au->length) - ptrlocb;
}

/*---------------------------------------------------- Entry Points ----*/



AuPlaybackWriter_clp AuPlaybackWriter_New(const Au_Rec *au, int64_t latency, string_t streamname, int32_t *queue_length) {
    AuPlaybackWriter_st *newst;
    AsyncAuplaybackWriter_st *placeholder;
    MixerRegister_clp NOCLOBBER mixer;
    
    mixer= 0;
#ifdef USEMIXER
    while (!mixer) {
	TRY {
	    mixer = IDC_OPEN("svc>aumixer", MixerRegister_clp);
	} CATCH_ALL {
	    fprintf(stderr,"Mixer not found yet\n");
	    PAUSE(SECONDS(1));
	} ENDTRY;
    }

    if (!MixerRegister$Register(mixer, 0, &newst, &placeholder)) {
	fprintf(stderr,"Mixer denied me!\n");
	return NULL;
    }
#else
    newst = malloc(sizeof(AuPlaybackWriter_st));
#endif
    newst -> closure.st = newst;
    newst -> closure.op = &auplaybackwriter_ms;
    newst->au = au;
    newst->absize = (uint32_t) au->length;
    newst->latency = latency + 1;
    newst->streamname = streamname;
    newst->queue_length = queue_length;
    LOG(newst->log = malloc(LOGSIZE));
    LOG(assert(newst->log));
    LOG(memset(newst->log, 0, LOGSIZE));
    LOG(newst->logptr = newst->log; newst->logcount = 0);
#ifdef DOLOGGING
    TRY {
	CX_ADD("svc>auplayback_log", newst->log, addr_t);
	CX_ADD("svc>auplayback_log_size", LOGSIZE, uint64_t);
    } CATCH_ALL {
	fprintf(stderr,"AuPlaybackWriter; couldn't export log information (is there a logging aubuf client already?)\n");
    } ENDTRY;
    fprintf(stderr,"Log at %x\n", newst->log);
#endif
    calibrate(newst);
#ifdef USEMIXER
    MixerRegister$ReadyToGo(mixer);
#endif
    return &(newst->closure);
}

static uint32_t AuPlaybackWriter_Put_m (
	AuPlaybackWriter_cl	*self,
	addr_t	samples	/* IN */,
	uint32_t	length	/* IN */,
	bool_t	block	/* IN */,
	int32_t	volume	/* IN */ )
{
  AuPlaybackWriter_st	*st = self->st;
  int i, toend;
  int32_t *inbuffer;		/* a pointer in to the data handed in */
  int32_t *ptr;			/* a copy of the pointer from the state record */
  uint32_t safezone = st->au->safe_zone;
				/* a copy of the safezone value */
  int32_t limit;		/* the number of samples to play */
  uint32_t togo;		/* a running total of th enumber of 
				samples still to be played */
  uint32_t aulength;		/* a copy of the length of the buffer, to avoid
				 many references */
#ifdef DOLOGGING
  void log_play_start(uint32_t loc, uint32_t num) {
      *(st->logptr++) = NOW();
      *(st->logptr++) = (((uint64_t) loc)<<32) + num;
  }
  void log_play_end(void) {
      *(st->logptr++) = NOW();
      st->logcount++;
      if (st->logcount >= (LOGSIZE / 24)) {
	  st->logcount = 0;
	  st->logptr = st->log;
      }
  }
#endif
  aulength = st->au->length;
  togo = length;
  inbuffer = samples;
  while (1) {			/* repeat every while we have more data to play */
      uint32_t curloc;
      int32_t delta;
      int volume_left, volume_right;
#ifdef USEMIXER
      volume_left = *st->volume_left;
      volume_right = *st->volume_right;
#else
      volume_left = volume_right = 10000;
#endif
      curloc = AUBUFLOC(st->au); /* work out where the sound card is */
      delta = st->ptrloc - curloc; /* work out how far ahead of it we currently are */

      if (delta < 0) {
	  /* we are behind! */
	  TRC(printf("behind by %d!\n", delta));
	  calibrate(st);
	  TRC(printf("now delta is %d\n", st->ptrloc - AUBUFLOC(st->au))) ;
      } else if (delta < safezone) {
	  void playsample(void) {
	      int32_t sample;
	      int16_t scaled_sample[2];
	      int16_t *addr_sample = (int16_t *) inbuffer;
	      scaled_sample[0] = 
		  /* do left side */
		  ((addr_sample[0] * volume_left)>>VOLMAX_SHIFT);
	      scaled_sample[1] = 
		  /* then the other side */
		  ((addr_sample[1] * volume_right) >> VOLMAX_SHIFT);

#ifdef SILENT
	      /* noop */
#else
	      /* then do a scaled add in to the buffer */
	      *ptr += *((int32_t *) scaled_sample);
#endif
	      ptr++; /* advance playout buffer pointer */
	      inbuffer++;  /* advance audio steam in pointer */
	      
	  }
	  if(st->queue_length) *st->queue_length = delta;
	  /* delta is now between 0 and safezone
	     => we can get some data in */
	  
	  limit = delta;
	  if (limit > togo) limit = togo; /* don't play more than we have */
	  if (delta + limit > safezone) limit -= delta+limit - safezone;
	  /* play them out */
	  {
	      toend = st->toend;
	      st->ptrloc += limit;
	      if (limit > toend) {

		  ptr=st->ptr;
		  LOG(log_play_start(st->ptr - ((int32_t*) st->au->base), toend));
		  for (i=0; i<toend; i++) playsample();

		  LOG(log_play_end());
		  limit -= toend;
		  togo -= toend;
		  ptr=((int32_t*) st->au->base);
		  LOG(log_play_start(0, limit));

		  for (i=0; i<limit; i++) playsample();

		  LOG(log_play_end());
		  togo -= limit;
		  st->ptr = ptr;
		  st->toend = st->absize - limit;
	      } else {
		  LOG(log_play_start(st->ptr - ((int32_t*) st->au->base), limit));
		  ptr = st->ptr;
		  for (i=0; i<limit; i++) playsample()

		  LOG(log_play_end());
		  st->ptr = ptr;

		  st->toend -= limit;
		  togo -= limit;
	      }
	      if (!toend) {
		  ptr = ((int32_t*) st->au->base);
		  toend = st->absize ;
	      }
	  }
      } else {
	  /* we are too far ahead; warn but pause */
	  TRC(printf("too far ahead by %d, (delta %d) %d to go; blocking\n", delta - safezone, delta, togo));
	  if (block) PAUSE(SECONDS(1)/10);
      }
	  
      /* there is now no more that we can do until the buffer empties itself */
      if (!togo) return length;	/* return if we are done */
      if (!block) return length - togo;	/* bail out if we have been told not to block */
      TRC(printf("block, %d togo\n", togo));
#if 0
      {
	  int i;
	  int j=0;
	  for (i=0; i<100; i++)  j++;
      }
#endif
  }
      
  
}

void AuPlaybackWriter_Destroy_m(AuPlaybackWriter_clp self) {
  AuPlaybackWriter_st	*st = self->st;

  free(st);
}

int32_t AuPlaybackWriter_QueryQueueLength_m(AuPlaybackWriter_clp self) {
    AuPlaybackWriter_st *st = self->st;

    return st->ptrloc - AUBUFLOC(st->au); /* work out how far ahead of it we currently are */
}

/*
 * End 
 */
