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

    dumb_c.c
    Minimal control mode -- no interaction, just prints out messages.
    */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ecs.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>

#include "config.h"
#include "common.h"
#include "output.h"
#include "controls.h"
#include "instrum.h"
#include "playmidi.h"

#include <nemesis.h>
#include <CRendDisplayMod.ih>
#include <environment.h>
static void ctl_refresh(void);
static void ctl_total_time(int tt);
static void ctl_master_volume(int mv);
static void ctl_file_name(char *name);
static void ctl_current_time(int ct);
static void ctl_note(int v);
static void ctl_program(int ch, int val);
static void ctl_volume(int channel, int val);
static void ctl_expression(int channel, int val);
static void ctl_panning(int channel, int val);
static void ctl_sustain(int channel, int val);
static void ctl_pitch_bend(int channel, int val);
static void ctl_reset(void);
static int ctl_open(int using_stdin, int using_stdout);
static void ctl_close(void);
static int ctl_read(int32 *valp);
static void drawnote(int v, int note, int velocity, int status);
void drawvolume(int i);
static void ctl_pass_playing_list(int number_of_files, char *list_of_files[]);
static int cmsg(int type, int verbosity_level, char *fmt, ...);

CRend_clp renderppmtocrend(CRendDisplay_clp display, int xpos, int ypos, FILE *f, int *sizex, int *sizey);
#define MESSY 0
#define TEXTSIZE 9
#define MESSHEIGHT TEXTSIZE

#define COLOUR16(_r,_g,_b) ((_b)|((_g)<<5)|((_r)<<10))

#define TIMEY (MESSHEIGHT)

#define MAINX 227
#define MAINY (TEXTSIZE*4)

#define VOICEHEIGHT 16

#define CHYPOS(y) MAINY + (y*VOICEHEIGHT)

#define MAXVOICES 8*128

#define VOICEWIDTH 4

#define WHITE 0x7fff
#define GREY30 COLOUR16(12,12,12)
#define BLACK 0x0000

#define GREEN 0x3e003e0
#define RED 0x7c007c00
#define BLUE 0x1f001f
#define PLAYLISTWIDTH 124
#define PLAYLISTHEIGHT 100

#define VOLUMELEFT 160
#define VOLUMERIGHT 224
#define VOLMAP(x) ((((VOLUMERIGHT-VOLUMELEFT) * (x))>>8)+VOLUMELEFT)


 
CRend_clp crend;
int width, height;
CRendDisplay_clp display;

int numfiles;
char **filelist;

Event_Count paused_ev;
Event_Val   paused_val;
bool_t	    paused_bool;
bool_t      paused_init = False;

int nextsong = 0;
int pending_event = RC_NONE;

int volume[16];

void doevents(void *foo) {    
    WS_Event ev;
    while (1) {
 	CRendDisplay$NextEvent( display, &ev);

	switch (ev.d.tag) {
	    
	case WS_EventType_Expose:
	{	
	    WS_Rectangle *r = &ev.d.u.Expose;
	    CRend_clp window = NULL;

	    if (CRend$GetID(crend) == ev.w) {
		window = crend;
	    }


	    if (window) CRend$ExposeRectangle(window, r->x1, r->y1, 
				  r->x2 - r->x1, 
				  r->y2 - r->y1);
	    CRend$Flush(window);

	    break;
	}
	case WS_EventType_Mouse:
	     {
		WS_MouseData *d = &ev.d.u.Mouse;
		int buttonnumber;
		if (SET_IN(d->buttons,WS_Button_Left)) {
		    
		    if (d->x < 128 && d->y >= 36 && d->y <= 36 + numfiles * TEXTSIZE && pending_event == RC_NONE) {
			nextsong = (d->y - 36)/TEXTSIZE;
			pending_event = RC_NEXT;
			if (paused_bool)
			{
			    paused_bool = False;
			    EC_ADVANCE(paused_ev, 1);
			}
		    }
		    if (d->x >= VOLUMELEFT && d->x <= VOLUMERIGHT && d->y > MAINY && d->y < MAINY + VOICEHEIGHT * 16) {
			int ch;
			
			ch = (d->y - MAINY)/VOICEHEIGHT;
			
			volume[ch] = ((d->x - VOLUMELEFT)<<8)/(VOLUMERIGHT-VOLUMELEFT);
			drawvolume(ch);
		    }
		    if (d->y < 32 && d->x < 224) {
			buttonnumber = d->x / 32;
			if (pending_event== RC_NONE) {
			    if (paused_bool)
			    {
				paused_bool = False;
				EC_ADVANCE(paused_ev, 1);
			    }
			    switch (buttonnumber) {
			    case 0:
				printf("{first}");
				nextsong = 0;
				pending_event = RC_NEXT;
				break;
			    case 3:
				printf("{last}");
				nextsong = (numfiles - 1);
				pending_event = RC_NEXT;
				break;
			    case 1:
				printf("{prev}");
				nextsong -= 2;
				if (nextsong < 0) nextsong += numfiles;
				pending_event = RC_NEXT;
				break;
			    case 2:
				printf("{next}");
				pending_event = RC_NEXT;
				break;
			    case 4:
				printf("{start}");
				nextsong--;
				pending_event = RC_NEXT;
				break;
			    case 5:
				printf("{quit}");
				if (!paused_bool)
				{
				    paused_bool = True;
				    paused_val++;
				}
				break;
			    }
			}
		    }
		}
	     }
	    break;
	default: 

	    break;
	}
    }
}


/**********************************/
/* export the interface functions */

#define ctl dumb_control_mode

    FILE *infp, *outfp;

ControlMode ctl= 
{
  "dumb interface", 'd',
  1,0,0,
  ctl_open,ctl_pass_playing_list, ctl_close, ctl_read, cmsg,
  ctl_refresh, ctl_reset, ctl_file_name, ctl_total_time, ctl_current_time, 
  ctl_note, 
  ctl_master_volume, ctl_program, ctl_volume, 
  ctl_expression, ctl_panning, ctl_sustain, ctl_pitch_bend
};


void drawlist(int select) {
    int i       ;
    int colour;
    char *cptr;

    for (i=0; i<numfiles && (i+1)*TEXTSIZE<PLAYLISTHEIGHT; i++) {
	if (i==select) colour = COLOUR16(8,8,8); else colour = COLOUR16(12,12,12);
	CRend$SetForeground(crend, colour);
	CRend$SetBackground(crend, colour);
	CRend$FillRectangle(crend, 4,36+i*TEXTSIZE,PLAYLISTWIDTH, TEXTSIZE);
	CRend$SetForeground(crend, 0x7fff);	
	cptr = strrchr(filelist[i],'/');
	if (cptr) cptr++; else cptr = filelist[i];
	
	CRend$DrawString(crend, 4, 36+i*TEXTSIZE,cptr);

    }

    for (i=0; i<16; i++) {
	int j;
	for (j=0; j<167; j++) 
	    drawnote(i,j,0,VOICE_FREE);
    }
    CRend$SetForeground(crend, 0x7fff);	
    CRend$Flush(crend);
}

void drawvolume(int i) {
    int x;
    if (volume[i] < 0) volume[i] = 0;
    if (volume[i] >= 256) volume[i] = 0;
    x = VOLMAP(volume[i]);
    CRend$SetForeground(crend,GREY30);
    CRend$FillRectangle(crend, x, CHYPOS(i), VOLUMERIGHT-x, VOICEHEIGHT-5);
    CRend$SetForeground(crend, WHITE);
    CRend$FillRectangle(crend, VOLUMELEFT, CHYPOS(i), x-VOLUMELEFT, VOICEHEIGHT-5);
}

static int ctl_open(int using_stdin, int using_stdout)
{
    FILE *file;
    int i;

    if (!paused_init)
    {
	paused_ev = EC_NEW();
	paused_val = EC_READ(paused_ev);
	paused_bool = False;
	paused_init = True;
    }

   if (ctl.opened) return 0;

   display = CRendDisplayMod$OpenDisplay(NAME_FIND("modules>CRendDisplayMod", CRendDisplayMod_clp),""); 
   file = 0;
   while (!file) {
       fprintf(stderr,"Loading buttons\n");
       file = fopen("timidity.ppm", "r");
       if (!file) PAUSE(SECONDS(1));
   }
   crend = renderppmtocrend(display,GET_INT_ARG(ENV_WINDOW_X_LOCATION,50), GET_INT_ARG(ENV_WINDOW_Y_LOCATION,600), file, &width, &height); 
   fclose(file);

   for (i=0; i<16; i++) {
       volume[i] = 240; 
       drawvolume(i);
   }


   Threads$Fork(Pvs(thds), doevents, 0,0);


  ctl.opened=1;
  return 0;
}

static void ctl_close(void)
{ 
  fflush(outfp);
  ctl.opened=0;
}

static int ctl_read(int32 *valp)
{
    int r;
    r=pending_event;
    pending_event = RC_NONE;

    return r ;
}

static int cmsg(int type, int verbosity_level, char *fmt, ...)
{
    char buf[256];

  va_list ap;
  if ((type==CMSG_TEXT || type==CMSG_INFO || type==CMSG_WARNING) &&
      ctl.verbosity<verbosity_level)
    return 0;
  va_start(ap, fmt);

  vsprintf(buf, fmt, ap);
  va_end(ap);

  ctl_open(0,0);
#if 0
  CRend$SetForeground(crend, GREY30);
  CRend$FillRectangle(crend, 234,8,534,8);
#endif /* 0 */
  CRend$SetForeground(crend, 0x7fff);
  CRend$SetBackground(crend, GREY30 );
  CRend$DrawString(crend, 234,8, buf);
  CRend$Flush(crend);
  return 0;
}

static void ctl_refresh(void) { }

static void ctl_total_time(int tt)
{
  int mins, secs;

    {
	char buf[128];
      secs=tt/play_mode->rate;
      mins=secs/60;
      secs-=mins*60;
    CRend$SetForeground(crend, WHITE);
      sprintf(buf, "%3d:%02d", mins, secs);
      CRend$DrawString(crend,844,8,buf);
      CRend$Flush(crend);
    }
}

static void ctl_master_volume(int mv) {}

static void ctl_file_name(char *name)
{
}

static void ctl_current_time(int ct)
{
  int mins, secs;
  char buf[32];
      secs=ct/play_mode->rate;
      mins=secs/60;
      secs-=mins*60;
      sprintf(buf,"%3d:%02d", mins, secs);
      CRend$SetForeground(crend, WHITE);
  CRend$DrawString(crend,778, 8, buf);

}

static void drawnote(int v, int note, int velocity, int status) {
   int xl;
  int colour = BLACK;
  xl=(note * VOICEWIDTH) % (width - VOICEWIDTH - MAINX);
  switch(status)
    {
    case VOICE_DIE:
	colour = GREEN;
      break;
    case VOICE_FREE: 
	colour = COLOUR16(10,10,10);
      break;
    case VOICE_ON:
	colour = (0x7fff * (128+voice[v].velocity)) >> 8;
	break;
    case VOICE_OFF:
	colour = BLACK;
	break;
    case VOICE_SUSTAINED:
	colour = RED;
      break;
    }
  CRend$SetForeground(crend, colour);
  CRend$FillRectangle(crend, MAINX+ xl, 
		      CHYPOS(v) + (VOICEHEIGHT/2) - 4, 
		      VOICEWIDTH, 8);

  CRend$Flush(crend);

}

static void ctl_note(int v) {
   drawnote(voice[v].channel, voice[v].note, voice[v].velocity, voice[v].status);
}


static void ctl_program(int ch, int val) {
    char buf[8];
    CRend$SetForeground(crend, WHITE);
    sprintf(buf,"%3d", val);
    CRend$DrawString(crend, 140, CHYPOS(ch), buf);
    CRend$Flush(crend);
}

static void ctl_volume(int channel, int val) {}

static void ctl_expression(int channel, int val) {}

static void ctl_panning(int channel, int val) {}

static void ctl_sustain(int channel, int val) {}

static void ctl_pitch_bend(int channel, int val) {}

static void ctl_reset(void) {}

static void ctl_pass_playing_list(int number_of_files, char *list_of_files[])
{
	int i=0;

	ctl_open(0,0);

	numfiles = number_of_files;
	filelist = list_of_files;
	nextsong = 0;
	i=0;

	/* Main Loop */
	for (;;)
	{ 
	    i = nextsong;
	    nextsong= (nextsong + 1)%numfiles;
	    drawlist(i);
	    play_midi_file(list_of_files[i]);

	}
}

