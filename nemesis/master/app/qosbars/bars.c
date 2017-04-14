/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      app/qosctl
**
** FUNCTIONAL DESCRIPTION: 
** 
**      Generic QoS controller
** 
** ENVIRONMENT: 
**
**      WS client
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <ecs.h>
#include <ntsc.h>
#include <stdio.h>

#include <CRendDisplayMod.ih>
#include <CRendDisplay.ih>
#include <CRend.ih>

#include <IO.ih>
#include <IOMacros.h>

#include <keysym.h>
#include <ctype.h>

#include <environment.h>
#include <QoSCtl.ih>

#include <stdlib.h>
#include <bstring.h>

#define TRC(_x)
#define UTRC(_x)
#define XTRC(_x)

#define NO_QOS_MODE           /* allow Q key to diable QoS guarantees */


/* the granularity of the graph window scroll, in pixels */
#define GRAPHSCROLL 16
/* the spacing of loadbars vertically */
#define BARSPACING (st->pbar_height)
/* the space left for the info strip */
#define INFOSPACE (st->pbar_height + 4)
/* the thickness of loadbars */
#define BARHEIGHT ((st->pbar_height)-2)
/* indent of loadbars from left edge of loadbars window */
#define BARINDENT 108
/* indent of period label (eg x 10ms) from left edge of window */
#define PERIODINDENT 68
/* the width of the loadbars window */
#define win_width (st->pwin_width)
/* the dimenisons of the graph window, pixels */
#define GRAPH_WIDTH (st->pgr_width)
#define GRAPH_HEIGHT (st->pgr_height)
/* maximum number of loadbars */
#define MAXBARS 64
/* number of doublepixels width for the tabs */
#define TABWIDTH 8

/* How often to poll for new clients */
#define CHECKFREQUENCY 20
#define UPDATE_FREQUENCY 12
#define DELTA (SECONDS(1)/st->ratetarget)

#define DEFAULT_ROLLSIZE 3  /* reasonably smooth */

/* How to draw lines on the graph */
#define DRAWLINE_NONE 0
#define DRAWLINE_SINGLE 1
#define DRAWLINE_THICK 2
#define DRAWLINE_DOUBLE 3
#define DRAWLINE_PIXEL 4
#define DRAWLINE_NUM 5

#define DRAWLINE_DEFAULT DRAWLINE_SINGLE

#define KEY_DRAWLINE_NONE 'N'
#define KEY_DRAWLINE_SINGLE 'A'
#define KEY_DRAWLINE_THICK 'T'
#define KEY_DRAWLINE_DOUBLE 'D'
#define KEY_DRAWLINE_PIXEL 'P'

#define BARWIDTH (win_width - BARINDENT)

#define STMULTIPLE 16
#define STMASK 0xf
 
/* 
 * some useful colour constants 
 */
#define WHITE 0x7fff
#define BLACK 0x0000
#define GREEN 0x03e003e0
#define RED   0x7c007c00
#define BLUE  0x001f001f
#define YELLOW GREEN+RED
#define HALF_GREEN 0x1e001e0
#define HALF_RED   0x0300300
#define HALF_BLUE  0x0100010
#define GREY50 (HALF_GREEN+ HALF_RED + HALF_BLUE)
#define REDSHIFT 10
#define GREENSHIFT 5
#define BLUESHIFT 0

#define RDOMS 64
#define RLIMIT 32
#define RLIMITMASK (unsigned)31

#define RBASE (1<<16)
#define RREMAIN (30 - 18)
#define FRACTION_TO_BAR(x)  ((x * BARWIDTH)/RBASE)
#define FRACTION_TO_GRAPH(x) ((x * GRAPH_HEIGHT)/RBASE)

#define ROLLINDEX(dnum, index, xtraflag) \
(xtraflag + (dnum *2) + (index * RDOMS * 2))

#define TTO32(x) ((uint32_t) ((x / 1000)))

#define BARTOY(i) (((i) * BARSPACING) + INFOSPACE)
#define YTOBAR(y) (( y - INFOSPACE)/ BARSPACING)
#define USECS 1000

typedef uint32_t count_t;

const int colours[] = {
    GREEN,
    RED,
    BLUE,
    
    GREEN+RED,
    GREEN+BLUE,
    RED+BLUE,
    
    HALF_GREEN,
    HALF_RED,
    HALF_BLUE,
    
    HALF_GREEN+HALF_RED,
    HALF_GREEN+HALF_BLUE,
    HALF_RED+HALF_BLUE,
    
    HALF_GREEN + RED,
    HALF_GREEN + BLUE,
    HALF_RED + BLUE,
    
    GREEN + HALF_RED,
    GREEN + HALF_BLUE,
    RED + HALF_BLUE
};

typedef struct {
    Time_ns p, s, l;
    bool_t  x;
} qos_t;

#define BD_G 0
#define BD_X 1
#define BD_FIELDS 2

typedef struct {
    Domain_ID  did;    /* Client's unique identifier */
    char *names;       /* name of the current window */
    QoSCtl_Handle hdl; /* QosCtl handle for each window */

    int drawline;      /* graph drawing mode for each window */
    count_t     bardata[BD_FIELDS];/* current datums for each window */
    Time_T     period;  /* period of each client */
    Time_T     slice;  /* slice of each client */
    bool_t     extra;  /* extra time flag for each client */
    Time_T     latency; /* latency of each client */

    Time_T     saved_period;  /* period of each client */
    Time_T     saved_slice;  /* slice of each client */
    bool_t     saved_extra;  /* extra time flag for each client */
    bool_t     preserve;     /* if true, always preserve QoS parms*/

  /* New Query interface returns:
    tg : total guaranteed time the domain received between its
           creation and the time mentioned in sg
    tx : total extra time the domain received between its
           creation and the time mentioned in sx
    sg : The system time the last sample of guaranteed time was taken
           (usually the end of a domain's period)
    tg : The system time the last sample of extra time was taken
           (this will often be time Now)
 */
    Time_T  last_tg, last_sg, last_tx, last_sx;

} bar_st;


typedef struct {
    CRendDisplay_cl * display;  /* display connection */
    CRend_cl *window, *graph;   /* bars and graph crends */
    int      win_height;        /* present height of the bars window */
    int      redraw;            /* if set, a redraw is pending */
    WS_Buttons buttons;         /* old mouse button state */

    int dobars;			/* Draw a 'loadbars' style display */
    int dograph;		/* Draw a scrolling graph */

    int selected;		/* Which bar the user clicked on last */

    QoSCtl_cl *qosctl;          /* closure pointer to the QosCtl we use */
    int nbars;                  /* number of bars presently being rendered */

    Time_T     finishtime[STMULTIPLE]; /* times when main loop completed */

    char input[256];            /* input buffer for keyboard */
    char *inputptr;             /* pointer in to input array */
    int pgr_width;              /* graph width */
    int pgr_height;             /* graph height */
    int pwin_width;             /* bars window width */
    int pbar_height;            /* height of a particular bar */
    int rollsize;               /* smoothing factor */
    int rollsizedisp;           /* smoothing factor being displayed */
    count_t *rollbuffer;        /* pointer to buffer for rolling average */
    count_t *rolltotal;         /* pointer to buffer for rolling totals */
    int count;                  /* number of times round main loop */
    bool_t dump;		/* set if we are to dump all parameters */
    WS_WindowID barsid;         /* WS ID for bars window */
    WS_WindowID graphid;        /* WS ID for graph window */
    count_t totalloc;           /* total ammount of CPU allocated */
    count_t totused;            /* total ammount of CPU used */
    int rate;                   /* rate of going round loop */
    int ratetarget;             /* target for rate */

    int qos_mode;
    int graphscroll;

    bar_st b[MAXBARS];          /* bar states */
    uint32_t graphprev[MAXBARS], graphprevg[MAXBARS];
} lbar_st;

#define FOREGROUND 0x7fff
#define BACKGROUND 0x0


#define NO_QOS_PERIOD (1000000000-1)
#define NO_QOS_SLICE  (1000000-1)

typedef struct { 
    Domain_ID     did; 
    QoSCtl_Handle hdl;
    char          *name;
} new_client_t;

static int new_clients_cmp( const void * a, const void *b )
{ 
    return (int) ( ((new_client_t *)a)->did - 
		   ((new_client_t* )b)->did ); 
}


static void updatedominfo(lbar_st *st, bool_t render) 
{
    int h,i;
    /* update domain information */
    
    uint32_t nclients;
    QoSCtl_Client *clients;
    int old, oldheight;
    int oldnbars = st->nbars;
    new_client_t new_clients[MAXBARS];
    
    UTRC(fprintf(stderr,"poll\n"));
    /* Poll for new list 'clients' */

    if (QoSCtl$ListClients(st->qosctl, &clients, &nclients) < 0) { 
	eprintf("Urk! ListClients failed! Oh dear!\n");
	exit(1);
    }

    for(i=0; i < nclients; i++) {
	new_clients[i].hdl  = clients[i].hdl;
	new_clients[i].did  = clients[i].did;
	new_clients[i].name = clients[i].name;
    }
    
    qsort(new_clients, nclients, sizeof(new_client_t), new_clients_cmp );

    /* h and i are now equivalent */
    for(h = 0, i = 0; h < nclients && i < MAXBARS; i++, h++) {
	int j;

	for(j=i;j<st->nbars && st->b[j].did != new_clients[h].did ;j++);

	if(j>=st->nbars) {
	    /* i is a new domain */
	    st->b[i].hdl   = new_clients[h].hdl;
	    st->b[i].did   = new_clients[h].did;
	    st->b[i].names = new_clients[h].name;
	    XTRC(fprintf(stderr, "New: %d  hdl %p  did %qx %s\n",
			 i, st->b[i].hdl, st->b[i].did, st->b[i].names));

#ifdef NO_QOS_MODE
	    /* HACK HACK XXX IAP 
	       Always preserve QoS parms for some domains... */

	    if( !strcmp(st->b[i].names,"serial") ||
		!strcmp(st->b[i].names,"ps2")  ) {
		st->b[i].preserve = 1;
		fprintf(stderr,"Preserve: %s\n",st->b[i].names);
	    }
#endif
	} else if(j>i) {
	    
	    /* domain(s) deleted. Copy data */
	    bcopy( &(st->b[j]), &(st->b[i]),
		   (st->nbars - j) * sizeof(bar_st) );
	    st->b[i].hdl = new_clients[h].hdl;
	    XTRC(fprintf(stderr,"Ski: %d  hdl %p  did %qx (was %d) %s\n",
			 i,st->b[i].hdl,st->b[i].did,j,st->b[i].names));

	} else 	{
	    /* domain already existed */
	    st->b[i].hdl = new_clients[h].hdl;
	    XTRC(fprintf(stderr,"OK : %d  hdl %p  did %qx %s\n",
			 i,st->b[i].hdl,st->b[i].did,st->b[i].names));
	}
    }

    old = st->nbars;
    oldheight = st->nbars;
    
    st->nbars  = i;
    st->redraw = 1;

    /* don't draw more than MAXBARS bars */
    if (st->nbars >= MAXBARS) st->nbars = MAXBARS;
    
    if (oldnbars != st->nbars) {
	UTRC(printf("There are now %d bars (was %d)\n", st->nbars, oldnbars));
    }
    
    /* resize window */
    if (st->dobars && render) {
	UTRC(fprintf(stderr,"resize? want %d got %d; num %d height %d\n", BARTOY(st->nbars+1), st->win_height));
	
	if (st->win_height != BARTOY(st->nbars+1)) {
	    UTRC(fprintf(stderr,"resize!\n"));
	    st->win_height = BARTOY(st->nbars+1) ;
	    TRY {
		CRend$ResizeWindow(st->window,
				   win_width, st->win_height);
	    } CATCH_CRend$NoResources() {
		fprintf(stderr,"Warning: Qosctl cannot resize\n");
		st->nbars = old;
		st->win_height = oldheight;
	    } ENDTRY;
	}
    }
    
    /* blank text area */
    UTRC(fprintf(stderr,"blank\n"));
    if (st->dobars && render) {
	CRend$SetForeground(st->window, 0);
	CRend$FillRectangle(st->window, 
			    0, 0, 
			    BARINDENT, 
			    st->win_height);
    }

    UTRC(fprintf(stderr,"draw\n"));
    /* handle each domain in turn */
    for (i=0; i<st->nbars; i++) {
	char *nameptr;
	
	/* find out about it's QoS parameters */
	st->b[i].extra = QoSCtl$GetQoS(st->qosctl, st->b[i].hdl, 
				     &st->b[i].period, &st->b[i].slice, 
				     &st->b[i].latency);

	/** HACK HACK HACK XXXX IAP This is so gross it makes me
	  nauseous. What really needs to happen is that:
	  a. QoSCtl gets an epoch value, so you can tell when the 
	  info has been updated.
	  b. QoSCtl should return a Domain Id type value that can be used 
	  to identify individual domains across calls to ListClients.
	  
	  **/
#ifdef NO_QOS_MODE
	if( st->qos_mode == 0 &&
	    st->b[i].period != NO_QOS_PERIOD &&
	    st->b[i].slice  != NO_QOS_SLICE ) {

	    st->b[i].saved_period = st->b[i].period;
	    st->b[i].saved_slice  = st->b[i].slice;
	    st->b[i].saved_extra  = st->b[i].extra;
	    
	    st->b[i].period = NO_QOS_PERIOD;
	    st->b[i].slice  = NO_QOS_SLICE;
	    st->b[i].extra  = 1;
	    
	    QoSCtl$SetQoS(st->qosctl, st->b[i].hdl, 
			  st->b[i].period, st->b[i].slice,
			  st->b[i].latency, 
			  st->b[i].extra);
	}
#endif

	/* is it's period sensible? */
	if (st->b[i].period) {

	    /* render the colour key for each domain */
	    if (st->dobars && render) {
		TRC(fprintf(stderr,"Domain %d render\n",i));
		if (st->b[i].drawline) {
		    int base;
		    base = BARTOY(i);
		    
		    CRend$SetForeground(st->window, colours[i]);
		    switch (st->b[i].drawline) {
		    case DRAWLINE_SINGLE:
			CRend$FillRectangle(st->window, 0,base, 
					    TABWIDTH*2, BARHEIGHT);
			break;
		    case DRAWLINE_THICK:
			CRend$DrawLine(st->window,0, TABWIDTH*2,
				       base,base+BARHEIGHT);
			CRend$DrawLine(st->window,0, TABWIDTH*2,
				       base+BARHEIGHT,base);
			break;
		    case DRAWLINE_DOUBLE:
			CRend$DrawLine(st->window, TABWIDTH,TABWIDTH,
				       base,base+BARHEIGHT);
			CRend$DrawLine(st->window,0, TABWIDTH*2,
				       base + (BARHEIGHT>>1), 
				       base + (BARHEIGHT >>1));
			break;
		    case DRAWLINE_PIXEL:
			CRend$FillRectangle(st->window, 2, base+2,
					    (TABWIDTH*2)-2, BARHEIGHT-2);
			break;
		    }
		    
		}
	    }
	    
	    
	    /* if we are in the middle of a dump, dump information for
               this domain */
	    /* st->dump is set by the events thread
               to signal a dump, and cleared below */
	    if (st->dump) 
		printf("Domain %d name %s period %dus slice "
		       "%dus latency %dus extra %s\n", 
		       i, st->b[i].names, 
		       ((uint32_t) st->b[i].period / (USECS)), 
		       ((uint32_t) st->b[i].slice/ (USECS)), 
		       ((uint32_t) st->b[i].latency / (USECS)), 
		       st->b[i].extra ? "yes":"no");
	    
	    nameptr = st->b[i].names;
	    
	    /* Display Name and QoS Parameters */
	    if (st->dobars && render) {
		CRend$SetForeground(st->window, (i==st->selected) ? 
				    WHITE:YELLOW);
		CRend$SetBackground(st->window, BLACK);
		CRend$DrawString(st->window, TABWIDTH*2, BARTOY(i),nameptr);
		
		
		if( st->qos_mode) {
		    
		    NOCLOBBER uint32_t mantissa;
		    Time_ns base = 1;
		    char buff[10];
		    char ch = 'n';
		    
		    if (st->b[i].extra) 
			CRend$DrawString(st->window, PERIODINDENT, 
					 BARTOY(i), "x"); 
		    
		    if (st->b[i].period < 1000) base = 1, ch = 'n';
		    if (st->b[i].period >= 1000 && 
			st->b[i].period < 1000000) 
			base = 1000, ch='u';
		    if (st->b[i].period >= 1000000 && 
			st->b[i].period < 1000000000) 
			base = 1000000, ch='m';
		    if (st->b[i].period >= 1000000000) 
			base = 1000000000, ch=' ';
		    mantissa = (uint32_t)(st->b[i].period / base);
		    sprintf(buff,"%d%cs", mantissa, ch);
		    CRend$DrawString(st->window, PERIODINDENT + 10, 
				     BARTOY(i),buff);

		} else {
		    CRend$DrawString(st->window, PERIODINDENT + 10, 
					     BARTOY(i)," - ");
		}
		TRC(fprintf(stderr,"Render complete\n"));
	    }
	    
	}
    }
    UTRC(fprintf(stderr,"done\n"));
    /* at this point, we (probably) just did a dump, so clear the dump
       request flag */
    st->dump = 0;
    
    TRC(fprintf(stderr,"Done\n"));
    /* we've done a redraw, so clear it if one was requested */
    st->redraw = 0;

    if (st->nbars >= MAXBARS) st->nbars = MAXBARS;

}

static void EventHandler(lbar_st *st) 
{
    WS_Event ev;
    int freeze = 0;  /* never leave this loop if freeze is true
			(used for screen dumps etc */

    /* are there pending events on any of our (two) windows? */
    while(CRendDisplay$EventsPending(st->display) ) {
	/* get an event */
	CRendDisplay$NextEvent(st->display, &ev);
	/* handle the event */
	switch (ev.d.tag) {
	case WS_EventType_KeyPress:
	{
	    const int sym = ev.d.u.KeyPress;
	    TRC(fprintf(stderr,"Key press: %x %c\n", sym, sym));
	    TRC(eprintf("Key press: %x %c\n", sym, sym));

	    switch(sym) {
		
	    case XK_plus:
	    case XK_KP_Add:
	    case XK_equal:
		/* if the user hit the "="/"+" key, increment rollsize */
		if (st->rollsize < RLIMIT-1) {
		    st->rollsize ++;
		}
		TRC(fprintf(stderr,"Rollsize up to %d\n", st->rollsize));
		break;

	    case XK_minus:
	    case XK_KP_Subtract:
		/* if the user hit the "-"/"_" key, decrement rollsize */
		if (st->rollsize) st->rollsize --;
		TRC(fprintf(stderr,"Rollsize down to %d\n", st->rollsize));
		break;

	    case XK_period: st->ratetarget++; break;
	    case XK_comma: if(st->ratetarget) st->ratetarget--; break;

	    case 'd':
	    case 'D':
		/* if the user hit the "d"/"D" key, make a dump pending */
		st->dump = 1;
		break;

#ifdef NO_QOS_MODE
	    case 'q':
	    case 'Q':
	    {
		/* if the user hit "Q" key, toggle 'QoS' */
		int i;
		
		st->qos_mode ^= 1;
		if (!st->qos_mode) {
		    
		    fprintf(stderr,"Disable QoS\n");

		    for(i = 0; i < st->nbars; i++) {

			st->b[i].saved_period = st->b[i].period;
			st->b[i].saved_slice  = st->b[i].slice;
			st->b[i].saved_extra  = st->b[i].extra;
			
			/* XXX HACK XXX IAP avoid embarassing
			   ps2/serial lockups */
			
			if( !st->b[i].preserve ) {
			    st->b[i].period = NO_QOS_PERIOD;
			    st->b[i].slice  = NO_QOS_SLICE;
			    st->b[i].extra  = 1;
			}
		    }
		} else {
		    fprintf(stderr,"Enable QoS\n");

		    for(i = 0; i < st->nbars; i++) {
			st->b[i].period = st->b[i].saved_period;
			st->b[i].slice  = st->b[i].saved_slice;
			st->b[i].extra  = st->b[i].saved_extra;
			
		    }
		}
		
		for(i = 0; i < st->nbars; i++) {
		    QoSCtl$SetQoS(st->qosctl, st->b[i].hdl, 
				  st->b[i].period, st->b[i].slice,
				  st->b[i].latency, 
				  st->b[i].extra);
		}
		st->redraw = 1;
		break;
	    }
#endif

	    case '!': freeze ^= 1; break;

	    case XK_Return:
		/* if the user hit the return key, see if we have a
		   new valid period for the current domain and deal
		   with it if we do */

		TRC(fprintf(stderr,"[%s]\n", st->input));
		
		if (isdigit(*st->input)) {
		    
		    Time_ns base;
		    uint32_t mantissa;
		    char *ptr;
		    mantissa= 0;
		    ptr = st->input;
		    
		    TRC(fprintf(stderr,"First character is %d\n", *ptr));
		    TRC(printf("First character is %d\n", *ptr));
		    while ((*ptr) && ((*ptr) >= '0' && (*ptr) <='9')) {
			mantissa = (mantissa*10) + ((*ptr)-'0');
			TRC(printf("After character %c, mantissa = %d\n", 
				   *ptr, mantissa));
			ptr++;
		    }
		    TRC(printf("Mantissa: %d\n", mantissa));
		    
		    if (*ptr) {
			base = 0;
			*ptr = toupper(*ptr); 

			switch (*ptr) {
			case 'N':
			    base = 1;
			    break;
			case 'U':
			    base = 1000;
			    break;
			case 'M':
			    base = 1000000;
			    break;
			case 'S':
			    base = 1000000000;
			    break;
			default:
			    printf("Unknown base '%c'\n", *ptr);
			    break;
			}
			if (base) {
			    Time_ns newperiod, newslice;
			    
			    newperiod = base * mantissa;

			    if (st->b[st->selected].period) {
				/* XXX note that this blows up for 
				   periods/slices bigger than 104 days */
				newslice = 
				    (st->b[st->selected].slice << 20)
				    / st->b[st->selected].period;
				newslice *= newperiod;
				newslice >>= 20;

				TRC(printf("(p,s): (%qd,%qd)->(%qd,%qd)\n", 
					   st->b[st->selected].period, 
					   st->b[st->selected].slice,
					   newperiod, newslice));
					   
				QoSCtl$SetQoS(st->qosctl, 
					      st->b[st->selected].hdl, 
					      newperiod, newslice, 
					      st->b[st->selected].latency, 
					      st->b[st->selected].extra );
				
				st->b[st->selected].period = newperiod;
				st->b[st->selected].slice = newslice;
			    }

			}
		    }
		    st->redraw = 1;
		}
		st->inputptr = st->input;
		break;
		
	    default:
		/* if it's in the range [32,127] stick it into the
		 *  input buffer, so we can see sequences like "20ms" */
		if (sym >= XK_space && sym <= XK_asciitilde) {
		    *(st->inputptr) = sym;
		    st->inputptr ++;
		    *(st->inputptr) = 0;
		}
		break;
	    
	    }
	    break;
	}

	/***** end of keyboard event handling code *****/

	case WS_EventType_Mouse:
	{
	    WS_MouseData *d;
	    int domnum;
	    d = &ev.d.u.Mouse;

	    /* mouse clicked in bars window */
	    if (ev.w == st->barsid && st->qos_mode ) {

		domnum = YTOBAR(d->y);

		if (domnum >= 0 ) {
		    /* set the drawline mode */
		    /* if left button just went down (st->buttons is
		       old mouse state) */
		    if (SET_IN(d->buttons, WS_Button_Left) && 
			!SET_IN(st->buttons, WS_Button_Left) && 
			d->x < TABWIDTH*2) {
			
			if (st->b[domnum].drawline) {
			    st->b[domnum].drawline=0;
			} else {
			    st->b[domnum].drawline=DRAWLINE_DEFAULT;
			}
			st->redraw = 1;
			st->buttons = d->buttons;
			break;
		    }
		    /* if middle button just went down (st->buttons is
		       old mouse state) */
		    if (SET_IN(d->buttons, WS_Button_Middle) &&
			!SET_IN(st->buttons, WS_Button_Middle) &&  
			d->x < TABWIDTH*2) {
			/* advance drawline mode by 1 */
			st->b[domnum].drawline = (st->b[domnum].drawline+1) % 
			                         DRAWLINE_NUM;
			st->redraw = 1;
			st->buttons = d->buttons;
			break;
		    }

		    if ((SET_IN(d->buttons, WS_Button_Left) || 
			 SET_IN(d->buttons, WS_Button_Middle))) {
			int bar;
			
			TRC(fprintf(stderr,"mouse event (%d,%d) buttons %d:%d:%d\n", 
				    d->x, d->y, 
				    SET_IN(st->buttons, WS_Button_Left), 
				    SET_IN(st->buttons, WS_Button_Middle), 
				    SET_IN(st->buttons, WS_Button_Right)));
			bar = st->selected;

			/* select a new bar iff: left button just went
			   down or middle button went down */
			if ((!SET_IN(st->buttons, WS_Button_Left) && 
			     SET_IN(d->buttons, WS_Button_Left)) ||
			    (!SET_IN(st->buttons, WS_Button_Middle) && 
			     SET_IN(d->buttons, WS_Button_Middle))) {
			    if (domnum>= 0 && 
				domnum < st->nbars) {
				st->selected = bar = domnum;
				st->redraw = 1;
				TRC(fprintf(stderr,"Selecting bar %d\n", bar));
			    }
			}
		           
			/* set slice if we are in the bars themselves */
			/* don't check old mouse state, to allow dragging */
			TRC(fprintf(stderr, "OLD: p=%qx s=%qx\n",
				    st->b[bar].period,
				    st->b[bar].slice));
			if (SET_IN(d->buttons, WS_Button_Left) && 
			    d->x > BARINDENT) {
			    st->b[bar].slice = st->b[bar].period * 
				(d->x - BARINDENT);
			    st->b[bar].slice /= BARWIDTH;

			    /* and update the scheduler */
			    QoSCtl$SetQoS(st->qosctl, st->b[bar].hdl,
					  st->b[bar].period, 
					  st->b[bar].slice, 
					  st->b[bar].latency, 
					  st->b[bar].extra);
			}

			/* toggle extra time if the mouse button just
			   went down */
			if (SET_IN(d->buttons, WS_Button_Middle) && 
			    !SET_IN(st->buttons, WS_Button_Middle)) {

			    if (st->b[bar].extra) st->b[bar].extra=0; 
                               else st->b[bar].extra=1;

			    /* and update the scheduler */
			    QoSCtl$SetQoS(st->qosctl, st->b[bar].hdl, 
					  st->b[bar].period, 
					  st->b[bar].slice, 
					  st->b[bar].latency, 
					  st->b[bar].extra);
			}
			TRC(fprintf(stderr, "NEW: p=%qx s=%qx l=%qx x=%d\n",
				    st->b[bar].period,
				    st->b[bar].slice,
				    st->b[bar].latency,
				    st->b[bar].extra));
			
		    }
		}	
	    }
	    /* store the mouse buttons state so we can get it back */
	    st->buttons = d->buttons;

	    break;
	}
	case WS_EventType_Expose:
	{	
	    WS_Rectangle *r = &ev.d.u.Expose;
	    
	    TRC(fprintf(stderr,"Expose event (%d,%d)-(%d,%d) window %qx\n",
			ev.d.u.Expose.x1, ev.d.u.Expose.y1,
			ev.d.u.Expose.x2, ev.d.u.Expose.y2, ev.w));
	    
	    if (st->dobars && ev.w == st->barsid) {
		CRend$ExposeRectangle(st->window, r->x1, r->y1, 
				      r->x2 - r->x1, 
				      r->y2 - r->y1);
		
		CRend$Flush(st->window);
	    }
	    if (st->dograph && ev.w == st->graphid) {
		CRend$ExposeRectangle(st->graph, r->x1, r->y1, 
				      r->x2 - r->x1, 
				      r->y2 - r->y1);
		
		CRend$Flush(st->graph);
	    }
	    break;
	}
	default: 
	    
	    break;
	}

	/* Freeze everything until somebody causes another event so we
	   can dump the screen without rendering artifacts */
	if( freeze ) {
	    while (!CRendDisplay$EventsPending(st->display)) {
		PAUSE(MILLISECS(100));
	    }
	}
    }
}

static void pollqos(lbar_st *st)
{
    Time_ns now = NOW();
    int i;

    st->totalloc = 0;
    st->totused = 0;

    /* go through and grab the data */
    for (i = 0; i < st->nbars; i++) {
	Time_ns tg, sg, tx, sx;
	int     rc;

	TRC(printf("Domain %d\n", i));
	if (st->b[i].period)
	    st->totalloc += st->b[i].slice * 100 / st->b[i].period;

	/* query the qosctl for the domain's behaviour */
	rc = QoSCtl$Query(st->qosctl, st->b[i].hdl, 
			  &tg, &sg, &tx, &sx);

	if (!rc) {

	    /* domain is buggered, so assume it's not doing anything */
	    st->b[i].bardata[BD_G] = 0;
	    st->b[i].bardata[BD_X] = 0;
	    TRC(printf("dom%d: Read duff data from QoSCtl\n",i));

		    
	} else {
	    Time_ns delta_tg = tg - st->b[i].last_tg;
	    Time_ns delta_sg = sg - st->b[i].last_sg;
	    Time_ns delta_tx = tx - st->b[i].last_tx;
	    Time_ns delta_sx = sx - st->b[i].last_sx;

	    count_t gval, xval;

	    st->b[i].last_tg = tg;
	    st->b[i].last_sg = sg;
	    st->b[i].last_tx = tx;
	    st->b[i].last_sx = sx;
			
	    /* Note that these are added up from scratch every
	     * time qosbars polls the QosCtl interface so if
	     * there hasn't been a sample, we must invent a
	     * likely number or the bars/idle time will look
	     * wrong! 
	     *
	     * If there hasn't been another sample then all we
	     * can do is assume the previous one is still the case.
	     * This may look a bit wierd for domains with large 
	     * periods who block a lot!
	     */ 
	    gval = (delta_sg) ?
		((delta_tg * (Time_ns)RBASE) / delta_sg) : 
		((now - sg < st->b[i].period) ? st->b[i].bardata[BD_G] : 0);
		    
	    xval = (delta_sx) ? 
		((delta_tx * (Time_ns)RBASE) / delta_sx) : 
		((now - sg < st->b[i].period) ? st->b[i].bardata[BD_X] : 0);

	    st->totused += gval;
	    st->totused += xval;

	    st->b[i].bardata[BD_G] = gval;
	    st->b[i].bardata[BD_X] = xval;

	    if (gval > RBASE || xval > RBASE) {
		printf("dom%d: cg %x cx %x total %x gval %d "
		       "xval %d rbase %d\n",
		       i, TTO32(tg), TTO32(tx), gval, xval, RBASE);
	    }
	} 
    } /* end of grab data loop */

}

static void renderbars(lbar_st *st)
{
    NOCLOBBER int graphx;
    int graphy, graphyg;
    int i;

    /* now go through and plot the information */
    for (i = 0; i < st->nbars; i++) {

	const uint32_t prev = (st->count - st->rollsize) & RLIMITMASK;
	const uint32_t here = st->count & RLIMITMASK;
	count_t gval, xval;
	uint32_t gsize, xsize, remsize;

	/* gval and xval are the guranteed and extra fractions */
	gval = st->b[i].bardata[BD_G];
	xval = st->b[i].bardata[BD_X];
		
	if (i < RDOMS) {
	    st->rollbuffer[ROLLINDEX(i, here, 0)] = gval;
	    st->rollbuffer[ROLLINDEX(i, here, 1)] = xval;
	}
		
	if (st->rollsize > 1 && i < RDOMS) {
	    int j;
	    int count;
	    count = 0;
	    gval = xval = 0;
	    for (j=prev; j!=here ; j=(j+1)&RLIMITMASK, count++) {
		gval += st->rollbuffer[ROLLINDEX(i,j,0)];
		xval += st->rollbuffer[ROLLINDEX(i,j,1)];
	    }
	    gval /= count;
	    xval /= count;
	}
		    
	/* convert gval and xval to screen offsets */
	gsize = FRACTION_TO_BAR(gval);
	xsize = FRACTION_TO_BAR(xval);
	graphy = FRACTION_TO_GRAPH(gval+xval);
	graphyg = FRACTION_TO_GRAPH(gval);
	graphy = (GRAPH_HEIGHT * (gval +xval)) / RBASE;
	graphyg = (GRAPH_HEIGHT * (gval)) / RBASE;
		
	if (gsize > BARWIDTH) {
	    gsize = BARWIDTH;
	    TRC(fprintf(stderr,"gsize was to big for client %d\n", i));
	}
	if (gsize + xsize > BARWIDTH) {
	    xsize = BARWIDTH - gsize;
	    TRC(fprintf(stderr,"gsize and xsize summed to more "
			"than bar width for client %d\n", i));
	}
		
	remsize = BARWIDTH - gsize - xsize;
	if (graphy > GRAPH_HEIGHT - 2) graphy = GRAPH_HEIGHT -2;
		
	/* draw the bars themselves */
	if (st->dobars) {		    

	    CRend$SetForeground(st->window, GREEN);
	    CRend$FillRectangle(st->window,
				BARINDENT,
				BARTOY(i),
				gsize, BARHEIGHT);
		    
	    CRend$SetForeground(st->window, RED);
	    CRend$FillRectangle(st->window,
				BARINDENT + gsize,
				BARTOY(i),
				xsize, BARHEIGHT);
		    
	    CRend$SetForeground(st->window, BLACK);
	    CRend$FillRectangle(st->window,
				BARINDENT + gsize + xsize,
				BARTOY(i),
				remsize, BARHEIGHT);

	    CRend$SetForeground(st->window, BLUE);

	}
		
	if (st->qos_mode) {
	    uint64_t ssize;

	    if (st->b[i].period>>6) {

		ssize = (BARWIDTH>>6) * 
		    st->b[i].slice / (st->b[i].period>>6);
		CRend$DrawLine(st->window, BARINDENT, 
			       BARINDENT + ssize,
			       BARTOY(i) + (BARHEIGHT/2), 
			       BARTOY(i) + BARHEIGHT/2);
	    }		    
	}
		
	/* and draw the graph */
	if (st->dograph) {

	    CRend$SetForeground(st->graph, colours[i]);

	    graphx  = (GRAPH_WIDTH - GRAPHSCROLL) + st->graphscroll;

	    switch (st->b[i].drawline) {
	    case DRAWLINE_NONE:
		break;

	    case DRAWLINE_SINGLE:
		CRend$DrawLine(st->graph,graphx-1, graphx, 
			       GRAPH_HEIGHT - 1 - st->graphprev[i], 
			       GRAPH_HEIGHT - 1 - graphy);
		break;

	    case DRAWLINE_THICK:
		CRend$DrawLine(st->graph, graphx, graphx, 
			       GRAPH_HEIGHT - graphy, 
			       GRAPH_HEIGHT - graphyg);
		break;

	    case DRAWLINE_DOUBLE:
		CRend$DrawLine(st->graph, graphx, graphx, 
			       GRAPH_HEIGHT - 1- st->graphprevg[i], 
			       GRAPH_HEIGHT -1 - graphyg);
		CRend$DrawLine(st->graph, graphx, graphx, 
			       GRAPH_HEIGHT - 1- st->graphprev[i], 
			       GRAPH_HEIGHT -1 - graphy);
		break;

	    case DRAWLINE_PIXEL:
		CRend$DrawPoint(st->graph,  
				graphx, 
				GRAPH_HEIGHT -1 - graphy);
		break;
	    }
	    
	    st->graphprev[i] = graphy;
	    st->graphprevg[i] = graphyg;
	}
    }
    
    /* update the info strip with useful information */
    if (st->dobars) { 
	char buf[128];
	
	if(st->qos_mode) {
	    sprintf(buf, "commit %3d%%    redraw rate %3d  \n",
		    st->totalloc, st->ratetarget);
	} else { 
	    sprintf(buf, "QoS DISABLED   redraw rate %3d  \n", 
		    st->ratetarget);
	}

	CRend$SetForeground(st->window, WHITE);
	CRend$SetBackground(st->window, BLACK);
	CRend$DrawString(st->window, 0, 0, buf);
    }
	
	
    /* do the graph window */
    if (st->dograph) {
	
	CRend$SetForeground(st->graph, GREY50);
	CRend$DrawLine(st->graph, graphx, graphx, 
		       0, GRAPH_HEIGHT - FRACTION_TO_GRAPH(st->totused));
	st->graphscroll++;

	if (st->graphscroll == GRAPHSCROLL) {

	    /* scroll the graph window */
	    CRend$Flush(st->graph);
		
	    CRend$ScrollRectangle(st->graph, 
				  GRAPHSCROLL,
				  0, 
				  GRAPH_WIDTH - GRAPHSCROLL,
				  GRAPH_HEIGHT,
				  -GRAPHSCROLL,
				  0);

	    CRend$SetForeground(st->graph, BLACK);
	    CRend$FillRectangle(st->graph,
				GRAPH_WIDTH - GRAPHSCROLL, 0,
				GRAPHSCROLL, GRAPH_HEIGHT);
	    st->graphscroll = 0;
	}
    }
 
    /* clear the dredraw flag */
    if (st->redraw) st->redraw = 0;
}

void Main(Closure_clp self) 
{
    lbar_st *st;
    CRendDisplayMod_clp dispmod;
    NOCLOBBER int check;
    int x;

    TRC(fprintf(stderr,"qosctl starting\n"));

    /* set up state record */
    st = calloc(1,sizeof(*st));
    if(!st) {
	eprintf("qosbars: no memory!\n");
	ntsc_halt();
    }

    /* Try and get a QosCtl_clp from somewhere.
     * First try >qosctl,
     *
     * If that doesn't exist, try >qosctl_name to get a name, and look
     * that up.
     *
     * If _that_ doesn't work, give up.  */

    st->qosctl = GET_ARG("qosctl", QoSCtl_clp, NULL);
    if (!st->qosctl) {
	char *name = GET_STRING_ARG("qosctl_name", NULL);
	if (name)
	    st->qosctl = GET_ARG(name, QoSCtl_clp, NULL);
	if(!st->qosctl)
	    st->qosctl = GET_ARG("sys>schedctl", QoSCtl_clp, NULL);
	if (!st->qosctl) {
	    printf("qosbars: no qosctl found, exiting\n");
	    exit(1);
	}
    }

    st->pgr_width   = GET_INT_ARG("graphwidth", 256);
    st->pgr_height  = GET_INT_ARG("graphheight", 100);
    st->pwin_width  = GET_INT_ARG("winwidth", 500);
    st->pbar_height = GET_INT_ARG("barheight", 8);
    st->dobars      = GET_BOOL_ARG("bars",1);
    st->dograph     = GET_BOOL_ARG("graph",1);
    st->win_height  = BARTOY(st->nbars+1);
    st->count       = 0;
    st->dump        = 0;
    st->rate        = 0;
    st->ratetarget  = UPDATE_FREQUENCY;
    st->qos_mode    = 1;
    st->redraw      = 2;
    st->nbars       = 0;
    st->buttons     = 0;
    st->selected    = 0;
    st->inputptr    = st->input;
    *(st->inputptr) = 0;

    st->rollsize     = DEFAULT_ROLLSIZE; 
    st->rollsizedisp = -1;
    st->rollbuffer   = calloc(1,sizeof(count_t)*RDOMS*RLIMIT*2);
    st->rolltotal    = calloc(1,sizeof(count_t)*RDOMS*2);
    if (!st->rollbuffer || !st->rolltotal) {
	eprintf("Heap too small\n");
	exit(1);
    }

    check = 0;

    /* find out about the domains we have for the first time */
    updatedominfo(st, 0);

    /********* start graphical stuff ************/

    /* Get hold of the client rendering module */
    dispmod = NAME_FIND("modules>CRendDisplayMod", CRendDisplayMod_clp);

    /* sort out display */
    st->display = CRendDisplayMod$OpenDisplay(dispmod, "");
    TRC(fprintf(stderr,"Got display\n"));

    /* create bars window */
    if (st->dobars) {
	st->window = CRendDisplay$CreateWindow(st->display, 0,0,
					       win_width, st->win_height);
	st->barsid = CRend$GetID(st->window);
	CRend$Map(st->window);
    }

    /* create graph window */
    if (st->dograph) {
	TRC(fprintf(stderr,"Map 1\n"));
	st->graph = CRendDisplay$CreateWindow(st->display, win_width, 0,
					      GRAPH_WIDTH, GRAPH_HEIGHT);
	st->graphid = CRend$GetID(st->graph);
	CRend$Map(st->graph);

	for (x=0; x<MAXBARS; x++) {
	    st->graphprev[x]= st->graphprevg[x]=0;
	    st->b[x].drawline = DRAWLINE_DEFAULT;
	}
    }

    TRC(fprintf(stderr,"Main window id %qd\nGraph window id %qd\n", 
		st->barsid, st->graphid));

    /* set the titles */
    {
	WS_cl *ws = CRendDisplay$GetWS(st->display);
	if(st->dograph) {
	    WS$SetTitle(ws, st->graphid, "graph");
	}
	if(st->dobars) {
	    WS$SetTitle(ws, st->barsid, "bars");
	}
    }

    TRC(fprintf(stderr,"Entering loop\n"));
    while (1) {

	/* is it time to update domain information? */
	if (!check || st->redraw) {
	    updatedominfo(st, 1);
	    TRC(fprintf(stderr,"There are %d domains\n", st->nbars));
	    check = CHECKFREQUENCY;
	}
	check--;

	/* do we need to bother gathering running data? */
	if (st->dobars || st->dograph) {
 	    
	     /* Poll the QoSCtl to update the client data */
	     pollqos(st);

	     /* Render the information */
	     renderbars(st);
	}

	/* handle any events pending */
	EventHandler(st);

	/* Flush any winbdow updates to the frame buffer */
	if (st->dobars) CRend$Flush(st->window);
	if (st->dograph) CRend$Flush(st->graph);

	/* Aim to update the display every DELTA nanoseconds */
	{
	    Time_ns n, delta;

	    n = NOW();
	    delta = n - st->finishtime[(st->count-1) & STMASK];

	    if (delta <= DELTA) 
		PAUSE(DELTA - delta);

	    if (n != st->finishtime[st->count & STMASK]) {
		st->rate = SECONDS(STMULTIPLE) / 
		    (n - st->finishtime[st->count & STMASK]);
	    }
	    st->finishtime[st->count & STMASK] = n;
	}

	/* keep track of how many times we gone round the loop, for
	   indexing into the rolling average array */
	st->count++;
    }
}
