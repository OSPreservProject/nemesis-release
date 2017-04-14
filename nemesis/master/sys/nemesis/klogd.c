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
**      klogd.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Reads console output from the buffer in the ntsc
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: klogd.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include "klogd.h"
#include "time.h"
#include "ecs.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

struct cd_init {
    kernel_st *kst;
    Wr_clp output;
};

static void entry(void *data)
{
    struct cd_init *init = data;

    kernel_st *kst = init->kst;
    Wr_clp output = init->output;
    
    uint8_t c;
    uint8_t linebuf[512];
    uint32_t index;

    Heap$Free(Pvs(heap),init);

    index=0;
    for (;;) {
	/* Wait for some data */
	while(kst->console.head == kst->console.tail) {
	    PAUSE(MILLISECS(500));
	}
	/* Fetch characters */
	while(kst->console.head != kst->console.tail) {
	    c=kst->console.buf[kst->console.tail++];
	    if (kst->console.tail==CONSOLEBUF_SIZE) kst->console.tail=0;
	    if (c=='\n') {
		linebuf[index++]='\n';
		linebuf[index++]=0;

		Wr$PutStr(output,linebuf);

		index=0;
	    } else {
		linebuf[index++]=c;
	    }
	}
    }
    /* NOTREACHED */
}

void start_console_daemon(kernel_st *kst, Wr_clp output)
{
    struct cd_init *init;

    init=Heap$Malloc(Pvs(heap), sizeof(*init));
    init->kst=kst;
    init->output=output;

    Threads$Fork(Pvs(thds), entry, init, 0);
}
