/* 

    TiMidity -- Experimental MIDI to WAVE converter
    Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    linux_audio.c

    Functions to play sound on the VoxWare audio driver (Linux or FreeBSD)

*/

#include "config.h"
#include "output.h"
#include "controls.h"
#include <IOMacros.h>
#include <IDCMacros.h>
#include <AuPlaybackWriter.ih>
#include <AuMod.ih>
#include <IO.ih>
#include <stdio.h>

#include <time.h>
#include <ecs.h>
#include <nemesis.h>
#include <salloc.h>
#define TXBUFSIZE    1024
#define NTXBUFS	     10


static int open_output(void); /* 0=success, 1=warning, -1=fatal error */
static void close_output(void);
static void output_data(int32 *buf, int32 count);
static void flush_output(void);
static void purge_output(void);

/* export the playback mode */

#define dpm nemesis_play_mode

PlayMode dpm = {
  DEFAULT_RATE, PE_16BIT|PE_SIGNED,
  -1,
  {0}, /* default: get all the buffer fragments you can */
  "nemesis audio", 'd',
  "dev>audio0",
  open_output,
  close_output,
  output_data,
  flush_output,
  purge_output  
};


/*************************************************************************/
/* We currently only honor the PE_MONO bit, the sample rate, and the
   number of buffer fragments. We try 16-bit signed data first, and
   then 8-bit unsigned if it fails. If you have a sound device that
   can't handle either, let me know. */

AuPlaybackWriter_clp writer;

static int open_output(void)
{
  int   i, warnings=0;

  Au_FormatDescription desc = { 44100, 2, 1, 16, 16, 32 };
  writer = AuMod$NewPlaybackWriter(NAME_FIND("modules>AuMod", AuMod_clp),
				   NULL,
				   &desc,
				   1024,
				   "timidity",
				   NULL);
  dpm.rate=44100;

  return warnings;
}
#define QUANT 32


/* count is number of stereo samples */
static void output_data(int32 *buf, int32 count) {
    IO_Rec   rec;
    int32_t *ptr;
    int32_t *endofbuf;
    word_t   valid_recs;
    int16_t *bptr;
    char    *endofbptr;
    uint32_t nrecs;
    int      i;
    int      j;
    int16_t intbuffer[count*2]; /* GCC extension */
    ptr = buf;
    bptr = intbuffer;
    for (j=0; j<count*2; j++) {
	int v;
	
	v = (*ptr++)>>(32-16-GUARD_BITS);
	if (v>32767) v=32767;
	if (v<-32768) v=-32768;
	*bptr++ = (int16) v;
    }
    AuPlaybackWriter$Put(writer, intbuffer, count, 1, 16384);
}

static void close_output(void)
{
    /* noop XXX */
}

static void flush_output(void)
{
    /* noop XXX */
}

static void purge_output(void)
{
    /* noop XXX */
}
