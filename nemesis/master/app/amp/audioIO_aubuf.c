#include <nemesis.h>
#include <stdio.h>
#include <IDCMacros.h>
#include <Au.ih>
#include <AuMod.ih>
#include <time.h>
#include <ecs.h>
#define TRC(_x)

const int AUDIO_BUFFER_SIZE = 64;
AuPlaybackWriter_clp writer = 0;
extern unsigned char nem_vol;
int indicated_frequency;
int32_t conversion[2048];

void audioOpen(int frequency, int stereo, int volume) {
    Au_FormatDescription desc = { 44100, 2, 1, 16, 16, 32 };
    Thread_clp thread;
    if (writer) return;

    indicated_frequency = frequency;
#ifdef ASYNC
    writer = AuMod$NewAsyncPlaybackWriter(NAME_FIND("modules>AuMod", AuMod_clp),
				     NULL,
				     &desc,
				     256,
				     65536,
				     "amp",
					  &thread);
#else
    writer = AuMod$NewPlaybackWriter(NAME_FIND("modules>AuMod", AuMod_clp),
				     NULL,
				     &desc,
				     256,
				     "amp",
				     NULL
				     );
#endif
}

void audioWrite(int32_t *inbuffer, int count) {
    if (indicated_frequency == 22050) {
	int i;
	int32_t *ptr;

	ptr = conversion;
	
	for (i=0; i<count/4; i++) {
	    int32_t x;
	    x = *inbuffer++;
	    *ptr++ = x;
	    *ptr++ = x;
	}
	AuPlaybackWriter$Put(writer, conversion, count/2, 1, 16384);
    } else {
	AuPlaybackWriter$Put(writer, inbuffer, count/4, 1, 16384);
    }
}

void audioClose(void) {
}

/* do what BeOS did to avoid buffering */


int
audioBufferOpen(int frequency, int stereo, int volume)
{
	audioOpen(frequency, stereo, volume);
	return 0; /* XXX */
}


inline void
audioBufferWrite(char *buf,int bytes)
{
	audioWrite((int32_t *) buf, bytes);
}


void
audioBufferClose()
{
	audioClose();
}

void audioFlush(void) {
}

void audioBufferFlush()
{
    audioFlush();
}



#include "audio.h"
#include "transform.h"
/* but take printout from buffer.c because we don't need the BeOS special */

void printout(void)
{
int j;

        if (nch==2)
                j=32 * 18 * 2;
        else
                j=32 * 18;

        if (A_WRITE_TO_FILE) {
#ifndef NO_BYTE_SWAPPING
        short *ptr = (short*) sample_buffer;
	int i = j;

                for (;i>=0;--i)
                        ptr[i] = ptr[i] << 8 | ptr[i] >> 8;
#endif

                fwrite(sample_buffer, sizeof(short), j, out_file);
        }

        if (A_AUDIO_PLAY) {
#ifdef LINUX_REALTIME
                rt_printout((short*) sample_buffer, j * sizeof(short));
#else /* LINUX_REALTIME */
#ifdef NEMESIS
		audioWrite((int32_t*)sample_buffer, j * sizeof(short));
#else
                if (AUDIO_BUFFER_SIZE==0)
                        audioWrite((char*)sample_buffer, j * sizeof(short));
                else
                        audioBufferWrite((char*)sample_buffer, j * sizeof(short))
#endif
#endif /* LINUX_REALTIME */
        }

}
