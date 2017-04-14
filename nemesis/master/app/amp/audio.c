/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/

/* audio.c	main amp source file 
 *
 * Created by: tomislav uzelac	Apr 1996 
 * Karl Anders Oygard added the IRIX code, 10 Mar 1997.
 * Ilkka Karvinen fixed /dev/dsp initialization, 11 Mar 1997.
 * Lutz Vieweg added the HP/UX code, 14 Mar 1997.
 * Dan Nelson added FreeBSD modifications, 23 Mar 1997.
 * Andrew Richards complete reorganisation, new features, 25 Mar 1997
 * Edouard Lafargue added sajber jukebox support, 12 May 1997
 */ 


#include "amp.h"

#define TRC(_x) _x
#include <nemesis.h>
#include <Rd.ih>

#include <exceptions.h>
#include <ecs.h>
#include <VPMacros.h>
#include <errno.h>
#include <stdio.h>
#ifndef NEMESIS
#ifndef __BEOS__
#include <sys/uio.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>


#include <fcntl.h>
#endif
#include <string.h>

#define AUDIO
#include "audio.h"
#include "formats.h"
#include "getbits.h"
#include "huffman.h"
#include "layer2.h"
#include "layer3.h"
#include "position.h"
#include "rtbuf.h"
#include "transform.h"
#include "controldata.h"
#include "guicontrol.h"

#ifndef __BEOS__
typedef int bool;
#endif

/*-------------------------------------------------------------------*/
/* Nemesis Stuff                                                     */
/*-------------------------------------------------------------------*/

/* Repeat the same track forever... */
#define NEM_REPEAT_FOREVER

#ifdef GUI
extern unsigned char       nem_play;
extern SRCThread_Mutex     nem_mu;      
extern SRCThread_Condition nem_go_again;
#endif

void statusDisplay(struct AUDIO_HEADER *header, int frameNo)
{
	int minutes,seconds;

	if ((A_SHOW_CNT || A_SHOW_TIME) && !(frameNo%10))
		msg("\r");
	if (A_SHOW_CNT && !(frameNo%10) ) {
		msg("{ %d } ",frameNo);
	}
	if (A_SHOW_TIME && !(frameNo%10)) {
		seconds=frameNo*1152/t_sampling_frequency[header->ID][header->sampling_frequency];
		minutes=seconds/60;
		seconds=seconds % 60;
		msg("[%d:%02d]",minutes,seconds);
	}
	if (A_SHOW_CNT || A_SHOW_TIME)
		fflush(stderr);
}

int decodeMPEG(void)
{
    int n;
struct AUDIO_HEADER header;
NOCLOBBER int cnt,g,snd_eof;

#ifdef LINUX_REALTIME
uid_t my_uid = getuid();

	set_realtime_priority();

	setreuid(my_uid, my_uid);

	prefetch_initial_fill();
#endif /* LINUX_REALTIME */  

	initialise_globals();

#ifndef LINUX_REALTIME
	if (A_FORMAT_WAVE) wav_begin();
#endif /* LINUX_REALTIME */
	
	if ((g=gethdr(&header))!=0) {
		report_header_error(g);
		return -1;
	}

	if (header.protection_bit==0) getcrc();

#ifdef LINUX_REALTIME
	if (setup_fancy_audio(&header)!=0) {
		warn("Cannot set up direct-to-DMA audio. Exiting\n");
		return -1;
	}
#else
	if (setup_audio(&header)!=0) {
		warn("Cannot set up audio. Exiting\n");
		return -1;
	}
#endif /* LINUX_REALTIME */

	
	show_header(&header);

	if (header.layer==1) {
		if (layer3_frame(&header,cnt)) {
			warn(" error. blip.\n");
			return -1;
		}
	} else if (header.layer==2)
		if (layer2_frame(&header,cnt)) {
			warn(" error. blip.\n");
			return -1;
		}

#ifdef LINUX_REALTIME
	if (start_fancy_audio(&header)!=0) {
		warn("Cannot start direct-to-DMA audio. Exiting\n");
		return -1;
	}
#endif /* LINUX_REALTIME */


	/*
	 * decoder loop **********************************
	 */
	snd_eof=0;
	cnt=0;
	n = 0;
	while (!snd_eof) {
/* Conditional play or pause */
		while (!snd_eof && ready_audio()) {
#ifdef GUI
		    if (n++==2) {DrawGui();n=0;}
		    if (nem_play) {
#endif


		    if ((g=gethdr(&header))!=0) {
			report_header_error(g);
#ifdef LINUX_REALTIME
			cleanup_fancy_audio();
#else
			if (g==GETHDR_EOF && A_FORMAT_WAVE) wav_end(&header);
#endif /* LINUX_REALTIME */
			snd_eof=1;
			break;
		    }
		    
		    if (header.protection_bit==0) getcrc();
		    
		    statusDisplay(&header,cnt);	
		    
		    if (header.layer==1) {
			if (layer3_frame(&header,cnt)) {
			    warn(" error. blip.\n");
			    return -1;
			}
		    } else if (header.layer==2)
			if (layer2_frame(&header,cnt)) {
			    warn(" error. blip.\n");
			    return -1;
			}
		    cnt++;
#ifdef GUI
		    }
		    
		    else
		    {
			/* STOP!!! */
			printf("should be stopped!\n");
			USING (MUTEX, &nem_mu,
			       if (!nem_play)
			       WAIT(&nem_mu, &nem_go_again);
		    );
		    }
#endif
		} 
#ifdef LINUX_REATIME
		if (block_fancy_audio(snd_eof)!=0) {
			warn("Problems with direct-to-DMA audio\n");
			return -1;
		}
#endif
	}
#ifdef LINUX_REALTIME
	if (stop_fancy_audio()!=0) {
		warn("Cannot stop direct-to-DMA audio. Exiting\n");
		return -1;
	}
#endif
	return 0;
}

int main(int argc,char **argv)
{
    int argPos;
    int i;

        TRC(printf("amp starting; argc %d argv at %x\n", argc, argv); PAUSE(SECONDS(1)/4));
	TRC({
	    for (i=0; i<argc; i++) 
		printf("Argument %d is %s\n", i, argv[i]);
	});
	TRC(printf("calling args\n"));
	argPos=args(argc,argv); /* process command line arguments */
	TRC(printf("calling initialise decoder\n"));
	initialise_decoder();	/* initialise decoder */
	TRC(printf("calling initialise decoder\n"));
	if (argc == 1) { 	/* Start amp as a GUI backend. */
		A_QUIET = TRUE;	/* TODO: change this to A_GUI_CONTROLLED */
#ifndef OS_SunOS
      		gui_control();
#endif
		printf("Need song!\n");
		exit(0);
    	} else {
	    	printf("displaying gui\n");

		if (A_AUDIO_PLAY) {	/* play each specified file */
		    TRC(printf("Audio play\n"));
			if (argPos<argc)
				for(;argPos<argc;argPos++) {
#ifdef LINUX_REALTIME
                                if (geteuid() != 0) 
                                        die("effective UID not root, cannot use realtime buffering\n");
				rt_play(argv[argPos]);
#else /* LINUX_REALTIME */
				TRC(printf("play %s\n", argv[argPos]));
#ifdef GUI
				nem_gui_init(argv[argPos]);
#endif
#ifdef NEM_REPEAT_FOREVER
				while(1)
				    play(argv[argPos],0);
#endif /* NEM_REPEAT_FOREVER */

#endif /* LINUX_REALTIME */
				}
			else
				displayUsage();
	 	} else {		/* convert the file to some format */
		    TRC(printf("Audio convert\n"));
			if ((argPos+2)==argc)
				play(argv[argPos],argv[argPos+1]);
			else {
				warn("Invalid number of parameters\n");
				displayUsage();
				die("");
			}
	}
		TRC(printf("inside exit reached\n"));
	msg("\nThank you for using amp!\n");
	exit(0);
    }
	TRC(printf("outside exit reached\n"));
  exit(0);
}

/* call this once at the beginning
 */
void initialise_decoder(void)
{
	premultiply();
	imdct_init();
	calculate_t43();
}

/* call this before each file is played
 */
void initialise_globals(void)
{
	append=data=nch=0; 
        f_bdirty=TRUE;
        bclean_bytes=0;

	memset(s,0,sizeof s);
	memset(res,0,sizeof res);
}

void report_header_error(int err)
{
	switch (err) {
		case GETHDR_ERR: die("error reading mpeg bitstream. exiting.\n");
					break;
		case GETHDR_NS : warn("this is a file in MPEG 2.5 format, which is not defined\n");
				 warn("by ISO/MPEG. It is \"a special Fraunhofer format\".\n");
				 warn("amp does not support this format. sorry.\n");
					break;
		case GETHDR_FL1: warn("ISO/MPEG layer 1 is not supported by amp (yet).\n");
					break;
		case GETHDR_FF : warn("free format bitstreams are not supported. sorry.\n");
					break;	
		case GETHDR_SYN: warn("oops, we're out of sync.\n");
					break;
		case GETHDR_EOF: 
		default: 		; /* some stupid compilers need the semicolon */
	}	
}

/* TODO: there must be a check here to see if the audio device has been opened
 * successfuly. This is a bitch because it requires all 6 or 7 OS-specific functions
 * to be changed. Is anyone willing to do this at all???
 */
int setup_audio(struct AUDIO_HEADER *header)
{
	if (A_AUDIO_PLAY)  
		if (AUDIO_BUFFER_SIZE==0)
			audioOpen(t_sampling_frequency[header->ID][header->sampling_frequency],
					(header->mode!=3 && !A_DOWNMIX),A_SET_VOLUME);
		else
			audioBufferOpen(t_sampling_frequency[header->ID][header->sampling_frequency],
					(header->mode!=3 && !A_DOWNMIX),A_SET_VOLUME);
	return 0;
}

void close_audio(void)
{
	if (A_AUDIO_PLAY)
		if (AUDIO_BUFFER_SIZE!=0)
			audioBufferClose();
		else
			audioClose();
}

int ready_audio(void)
{
#ifdef LINUX_REALTIME
	return ready_fancy_audio();
#else
	return 1;
#endif
}

/* TODO: add some kind of error reporting here
 */
void play(char *inFileStr, char *outFileStr)
{
	if (strcmp(inFileStr,"-")==0)
		in_file=stdin;
	else {
		if ((in_file=fopen(inFileStr,"r"))==NULL) {
			warn("Could not open file: %s\n",inFileStr);
			return;
		}
#ifdef ALLATONCE
		in_file_size = Rd$Length(in_file->rd);
		in_file_copy = malloc(in_file_size);
		in_file_position = 0;
		fread(in_file_copy, in_file_size, 1, in_file);
		fclose(in_file);
#endif
	}
	if (outFileStr) {
		if (strcmp(outFileStr,"-")==0)
			out_file=stdout;
	else
		if ((out_file=fopen(outFileStr,"w"))==NULL) {
			warn("Could not write to file: %s\n",outFileStr);
			return;
		}
		msg("Converting: %s\n",inFileStr);
	}

	if (A_AUDIO_PLAY)
		msg("Playing: %s\n",inFileStr);

	/*
	 * 
	 */
#ifdef ALLATONCE
	while (1) {
	    in_file_position = 0;
	    
	    decodeMPEG();
	}
#else
	decodeMPEG();
#endif
	
	close_audio();

	if (!A_AUDIO_PLAY) fclose(out_file);
	msg("\n");
}
