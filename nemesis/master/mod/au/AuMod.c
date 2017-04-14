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
** ID : $Id: AuMod.c 1.1 Thu, 18 Feb 1999 14:19:27 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <AuMod.ih>
#include <ecs.h>
#include <exceptions.h>
#include <time.h>
#include <IDCMacros.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	AuMod_NewPlaybackWriter_fn 		AuMod_NewPlaybackWriter_m;
static  AuMod_NewAsyncPlaybackWriter_fn                 AuMod_NewAsyncPlaybackWriter_m;


static	AuMod_op	ms = {
  AuMod_NewPlaybackWriter_m,
  AuMod_NewAsyncPlaybackWriter_m

};

const static	AuMod_cl	cl = { &ms, NULL };

CL_EXPORT (AuMod, AuMod, cl);

#define OVERRIDE
int checkformat(const Au_FormatDescription *alpha, const Au_FormatDescription *beta) {
#ifdef OVERRIDE
    return 1; /* XXX */
#else
    if (alpha->target_rate != beta->target_rate ||
	alpha->channels != beta->channels ||
	alpha->little_endianness != beta->little_endianness ||
	alpha->bits_per_channel != beta->bits_per_channel ||
	alpha->channel_stride != beta->channel_stride ||
	alpha->sample_stride != beta->sample_stride) {
	return 0;
    }
    return 1;
#endif
}

Au_Rec *getaurec(const Au_Rec *in) {
    Au_Rec NOCLOBBER *myau;

    myau = in;
    while(!myau) {
	TRY {
	    myau = NAME_FIND("dev>audio0", Au_Rec);
	} CATCH_ALL {
	    PAUSE(SECONDS(1));
	} ENDTRY;
    }
    return myau;
}

/*---------------------------------------------------- Entry Points ----*/

AuPlaybackWriter_clp AuPlaybackWriter_New(const Au_Rec *au, 
					  int64_t latency, 
					  string_t streamname,
					  int32_t *queue_length
					  ); /* in AuPlaybackWriter.c */
AuPlaybackWriter_clp Async_AuPlaybackWriter_New(const Au_Rec *au, 
						int64_t minimum_latency, 
						uint32_t maximum_latency, 
						string_t streamname,
						Thread_clp *thread); /* in AuPlaybackWriter.c */

static AuPlaybackWriter_clp AuMod_NewPlaybackWriter_m (
	AuMod_cl	*self,
	const Au_Rec	*au	/* IN */,
	const Au_FormatDescription	*format,	/* IN */ 
	int64_t latency,
	string_t streamname,
	int32_t *queue_length) {
    Au_Rec *myau = getaurec(au);

    if (!checkformat(format, &myau->format)) {
	fprintf(stderr,"AuMod; formats incompatible!\n");
	return NULL;
    }
    return AuPlaybackWriter_New(myau, latency, streamname, queue_length);

}

static AuPlaybackWriter_clp AuMod_NewAsyncPlaybackWriter_m (
        AuMod_cl        *self,
        const Au_Rec    *au     /* IN */,
        const Au_FormatDescription      *format /* IN */,
        int64_t        minimum_latency /* IN */,
        uint32_t        maximum_length /* IN */,
        string_t        streamname      /* IN */
   /* RETURNS */,
        Thread_clp      *thread )
{
    Au_Rec *myau = getaurec(au);

    if (!checkformat(format, &myau->format)) {
	fprintf(stderr,"AuMod; formats incompatible!\n");
	return NULL;
    }

    return Async_AuPlaybackWriter_New(myau, minimum_latency, maximum_length, streamname, thread);
}

/*
 * End 
 */
