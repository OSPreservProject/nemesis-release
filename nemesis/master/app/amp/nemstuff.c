
#include <nemesis.h>
#include <ecs.h>
#include <exceptions.h>
#include <time.h>
#include <IDCMacros.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#undef fseek
#include "amp.h"
#include "controldata.h"
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
#include <CRendDisplayMod.ih>

#define TRC(_x) 
CRend_clp renderppmtocrend(CRendDisplay_clp display , int xpos, int ypos, FILE *f, int *sizex, int *sizey);

/*                              
@@@  @@@  @@@  @@@  @@@  @@@  
@@@  @@@  @@@  @@@  @@@  @@@  
@@!  !@@  @@!  !@@  @@!  !@@  
!@!  @!!  !@!  @!!  !@!  @!!  
 !@@!@!    !@@!@!    !@@!@!   
  @!!!      @!!!      @!!!    
 !: :!!    !: :!!    !: :!!   
:!:  !:!  :!:  !:!  :!:  !:!  
 ::  :::   ::  :::   ::  :::  
 :   ::    :   ::    :   ::   
 */        
#ifdef GUI
CRendDisplayMod_clp displaymod;
CRendDisplay_clp display;
CRend_clp crend;
CRend_clp control;
int ctl_width, ctl_height;
char nem_name[255];
char nem_msg[255];
#endif
unsigned char nem_play;
SRCThread_Mutex     nem_mu;      
SRCThread_Condition nem_go_again;
unsigned char nem_vol;

#define IMAGE_LOCATION "amp.ppm"

int fseek(FILE *stream, long offset, int whence) {
    TRC(printf("fseek file %x offset %d whence %d\n", stream, offset, whence));
    Rd$Seek ((stream)->rd, (offset) +				
	 (((whence) == SEEK_SET) ? 0 :				
	  ((whence) == SEEK_CUR) ? Rd$Index ((stream)->rd) :	
	  ((whence) == SEEK_END) ? Rd$Length ((stream)->rd) : 0));
    return 0;
}

long ftell(FILE *stream) {
    int x =  Rd$Index(stream->rd);
    TRC(printf("ftell file %x offset %d\n", stream, x));
    return x;
}
int rand() {
    return (int) (NOW());
}
#if 0
int open(const char *pathname, int flags) {
    int fd;
    TRC(printf("Opening %s with flags %d\n", pathname, flags));
    fd = (int) fopen(pathname, "rb");
    TRC(printf("File %s 'fd' %x\n", pathname, fd));
    if (!fd) {
	fprintf(stderr,"(Couldn't open %s, exitting)\n", fd);
	exit(1);
    }
    return fd;
}

int close(int fd) {
    TRC(printf("Close 'fd' %x\n", fd));
    return fclose((FILE *) fd);
}

typedef long off_t;
int lseek(int flides, off_t offset, int whence) {
    int x;
    TRC(printf("lseek 'fd' %x offset %d whence %d\n", flides, offset, whence));
    fseek((FILE *) flides, offset, whence);

    x=ftell((FILE *) flides);
    return x;
}

ssize_t read(int fd, void *buf, size_t count) {
    NOCLOBBER ssize_t x;
    NOCLOBBER int position = 0;
    NOCLOBBER uint8_t * NOCLOBBER ptr = buf;
    FILE *f = (FILE *) fd;
    uint32_t sum;
    int i;
    int length;
    TRC(printf("read 'fd' %x buffer %x count %x\n", fd, buf,count));
    bzero(buf, count);
    while (position < count) {
	TRY {
	    length = (count-position) > 1024 ? 1024 : (count-position);

	    x = Rd$GetChars(f->rd, ptr, length);
#if 0
	    sum = 0;
	    for (i=0; i<length; i++) {
		sum += (uint8_t) ( *(ptr + i));
	    }
	    printf("Checksum for block base %x is %x\n", position, sum);
#endif	    
	    ptr += x;
	    position += x;
	}	CATCH_Rd$Failure(why) {
	    TRC(printf("Rd$Failure code %d\n", why));
	} ENDTRY;
    }

    TRC(printf("fread returned %x\n", count));
    return count;
}
#endif 
extern int
strncasecmp(const char *s1, const char *s2, size_t n)
{
  register const unsigned char *p1 = (const unsigned char *) s1;
  register const unsigned char *p2 = (const unsigned char *) s2;
  register int ret;
  unsigned char c1;

  if (p1 == p2 || 
      n == 0)
      return 0;

  for (; !(ret = (c1 = tolower(*p1)) - tolower(*p2)) && n>0; p1++, p2++, n--)
      if (c1 == '\0') break;
  if (n==0) ret =0;
#if 0
  TRC(printf("Compare %s with %s insens n = %d result = %d\n",
	     s1,s2,n,ret));
#endif
  return ret;
}

int getc(FILE *stream) {
    return fgetc(stream);
}

void perror(const char *s) {
    fprintf(stderr,"perror call; %s\n", s);
}

int putchar(int c) {
    printf("%c",c);
    return c;
}


/* XXX XXX XXX */
FILE *fdopen(int filedes, char *rubbish) {
    if (filedes == 0) return stdout;
    if (filedes == 1) return stderr;

    return (FILE *) filedes;
}

void rewind( FILE *stream) {
    fseek(stream, 0L, SEEK_SET);
}	   
#ifdef GUI
#define BUTTON_HEIGHT 32
#define BUTTON_WIDTH 32

#define WHITE 0x7fff
#define GREY30 5417
#define BLACK 0x0000
#define GREEN 0x3e003e0
#define RED 0x7c007c00
#define BLUE 0x1f001f

void DrawGui( void ) 
{
    int i;

    CRend$SetBackground(control,GREY30);
    CRend$SetForeground(control,WHITE);
    {
	char fpsstr[7];
	CRend$DrawString(control, BUTTON_WIDTH*2+5, BUTTON_HEIGHT*2+5, nem_name);
    }

#ifdef BOUNCYTHING
    /* Now draw the bouncy thing */
    CRend$SetForeground(control, GREY30);
    CRend$FillRectangle(control, 2, 2, 220, 60);

    CRend$SetForeground(control, GREEN);
    for (i = 2; i<=32*7-3; i+=1)
    {
	CRend$DrawPoint(control, i, BUTTON_WIDTH + *((short*)sample_buffer+i)*40/65536);
    }

    CRend$SetForeground(control, GREY30);
    CRend$FillRectangle(control, BUTTON_WIDTH*2+1, BUTTON_HEIGHT*2+18, BUTTON_WIDTH*5-2, 12);
    CRend$SetForeground(control, WHITE);
    CRend$FillRectangle(control, BUTTON_WIDTH*2+3, BUTTON_HEIGHT*2+18, nem_vol*156/256, 11);

    CRend$ExposeRectangle(control, 0, 0, ctl_width, ctl_height);
#endif
    CRend$Flush(control);

}

void EventThread( void ) 
{
    WS_Event ev;

    while(1) {
	CRendDisplay$NextEvent(display, &ev);

	switch (ev.d.tag) {
	    
	case WS_EventType_Expose:
	{
	    DrawGui();
	    break;
	}
	case WS_EventType_Mouse:
	{
	    if (ev.w == CRend$GetID(control)) {
		WS_MouseData *d = &ev.d.u.Mouse;
		if (SET_IN(d->buttons, WS_Button_Left)) {
		    if (d->y > BUTTON_HEIGHT*2)
		    {
			int buttonnumber;
			buttonnumber = (d->x / BUTTON_WIDTH) +1;
			switch (buttonnumber) {
			case 1: 
			    /* Stop */
			    printf("stop\n");
			    MU_LOCK(&nem_mu);
			    {
				nem_play = 0;
			    }
			    MU_RELEASE(&nem_mu);
			    break;
			case 2:
			    /* Play */
			    printf("play\n");
			    MU_LOCK(&nem_mu);
			    {
				nem_play = 1;
			    }
			    MU_RELEASE(&nem_mu);
			    SIGNAL(&nem_go_again);
			    break;
			default:
			    if (d->y > 80)
			    {
				nem_vol = MIN(255, (d->x - BUTTON_WIDTH*2) * 256/158);  
			    }
			    break;
			}
		    }
		    DrawGui();
		}
		
	    }
	    break;
	}
	default: 
	    break;
	} /* switch */
    }
}



void nem_gui_init( char *name )
{
    displaymod = NAME_FIND("modules>CRendDisplayMod", CRendDisplayMod_clp);
    display = CRendDisplayMod$OpenDisplay(displaymod, "");
    strcpy(nem_name, name);

    {
	FILE *file = 0;
	while (!file) {
	    TRC(printf("jd: Loading buttons\n"));
	    file = fopen(IMAGE_LOCATION, "r");
	    if (!file) PAUSE(SECONDS(1));
	}
	control = renderppmtocrend(display,600,20+ 115*(VP$DomainID(Pvs(vp)) % 2), file, 
					&ctl_width, &ctl_height); 
	fclose(file);
    }
    CRend$Map(control);

    /* Initialise stuff */
    MU_INIT (&(nem_mu));
    CV_INIT (&(nem_go_again));
    nem_play = 1;
    nem_vol = 255;

    /* Draw the GUI */
    DrawGui();

    /* Fork the events thread */
    Threads$Fork(Pvs(thds), EventThread, 0, 0);
}
#endif
