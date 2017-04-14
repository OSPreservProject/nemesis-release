/*
 * FILE		:	carnage.c
 * AUTHOR	:	Paul Barham
 * DATE		:	Wed Apr  7 17:02:46 BST 1993
 */

/*****************************************************************/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <mutex.h>
#include <time.h>
#include <ntsc.h>

#include <stdio.h>
#include <regionstr.h>
#include <environment.h>
/*****************************************************************/

#include "geometry_types.h"		/* 3D stuff... */
#include "geometry.h"
#include "render.h"
#include "viper.poly"			/* Spaceship object */

#include "Carnage_st.h"		/* State record */

#define TRC(x) 
#define DB(x) 

/* Constants... */

#define BLACK 0x00
#define WHITE 0xFFFF

#define RPHI 1
#define RTHETA 1
#define RGAMMA 2

#define XPOS 0
#define YPOS 464

#define EYEPOINT	-400
#define VIPER_X		0
#define VIPER_Y		50
#define VIPER_Z		-900
#define VIPER_START_Z   -5000
#define VIPER_TURN_Z    -2200
#define SPEED_FACTOR    3
#define VIPER_SPEED     160
#define VIPER_ACCN      2
#define PI		128 /* 3.1415927 */

/* Prototypes... */
static void InitDemo  (Carnage_st *st);
static void RunDemo   (Carnage_st *st);
static void ShowFrame (Carnage_st *st);
static void ProcessEvents(Carnage_st *st);

/* -------------------------------------------------------------- */

void Main (Closure_clp self)
{
  uint32_t	win_width, win_height;
  int           carnage_num = VP$DomainID(Pvs(vp)) % 5;
  int32_t  seed = (int32_t) NOW();
  int rand(void) {
      const int IM=139968;
      const int IA=3877;
      const int IC=29573;
      seed = (seed*IA + IC) % IM;
      return seed>>2;
  }
  Carnage_st *st;

  TRC(printf ("Carnage: domain %d entered.\n", carnage_num));
 
  /* 
   * Create local state 
   */
  TRC(printf("Carnage: creating local state.\n"));
  st = Heap$Malloc(Pvs(heap),sizeof(*st));
  if (!st) RAISE_Heap$NoMemory ();

  TRC(printf("Carnage: state at %lx size %lx\n", st, sizeof(*st)));

  TRC(printf("Carnage: Initialising state\n"));

  st->frame = 0;

  st->viper_posv.x  = VIPER_X;
  st->viper_posv.y  = VIPER_Y;
  st->viper_posv.z  = VIPER_Z;

  st->viper_rotv.phi   = 0;
  st->viper_rotv.theta = 0;
  st->viper_rotv.gamma = 0;

  st->camera_posv.x = 0;
  st->camera_posv.y = -50;
  st->camera_posv.z = 700;

  st->camera_rotv.phi   = 0;
  st->camera_rotv.theta = 0;
  st->camera_rotv.gamma = 0;

  st->nsegs[0] = 0;
  st->nsegs[1] = 0;
  st->segbuffer = 0;

  MU_INIT(&st->mu);
  
  {
      CRendDisplayMod_clp cdmod = NAME_FIND("modules>CRendDisplayMod", 
					    CRendDisplayMod_clp);

      st->display = CRendDisplayMod$OpenDisplay(cdmod, "");

  }

  st->corner_x = carnage_num * 128;
  st->corner_y = (carnage_num+1) * 128;

  st->last_second = NOW();
  st->frames_last_second = 0;

  st->corner_x = GET_INT_ARG(ENV_WINDOW_X_LOCATION, carnage_num*128 % 1024);
  st->corner_y = GET_INT_ARG(ENV_WINDOW_Y_LOCATION, carnage_num*128 % 1024);

  win_height = GET_INT_ARG(ENV_WINDOW_HEIGHT, 256);
  win_width = GET_INT_ARG(ENV_WINDOW_WIDTH, 256);
  
  st->corner_x = rand() % (1024 - 16 - win_width);
  st->corner_y = rand() % (768 - 32 - win_height);
  

  TRC(printf("Carnage: %dx%d+%d+%d\n", win_width, 
	 win_height, st->corner_x, st->corner_y));

  /* Pick a number, any number */
  st->viper_scale = win_width * 4;
  st->window      = CRendDisplay$CreateWindow(st->display, 
					      st->corner_x, 
					      st->corner_y,
					      win_width, 
					      win_height);

  if (!st->window) {
      fprintf(stderr,"Carnage: CreateWindow failed\n");
      return;
  }

  CRend$SetForeground(st->window, BLACK);

  CRend$FillRectangle(st->window, 0, 0, win_width, win_height);

  CRend$Map(st->window);

  st->geom.x = win_width;
  st->geom.y = win_height;
  st->geom.z = EYEPOINT;

  InitDemo(st);
  RunDemo(st);
  
  return;
}


/* Init our 3D stuff and the cube structures */

static void InitDemo(Carnage_st *st)
{
  /* Set up camera orientation */
  
  GeomGenRotMatrix(&st->camera_rotm, &st->camera_rotv);
}



/* Run the demo */

static 
void RunDemo(Carnage_st *st)
{
    int32_t        z      = VIPER_START_Z;
    int32_t        speed  = 0;

    for(;;)
    {
	st->viper_rotv.phi = PI;

	/* Thrust out of screen */
	for (;
	     z <= VIPER_TURN_Z; 
	     z += speed/SPEED_FACTOR) {

	    if (speed < VIPER_SPEED) {
		speed += VIPER_ACCN;
	    }
	    st->viper_posv.z = z;
	    st->viper_rotv.gamma += 1;
	    ShowFrame(st);
	}

	/* Brake and turn */
	for (st->viper_rotv.phi = PI; 
	     st->viper_rotv.phi > 0;
	     st->viper_rotv.phi -= 2) {

	    if (speed > 0) {
		speed -= VIPER_ACCN;
	    }

	    z += speed/SPEED_FACTOR;
	    st->viper_posv.z = z;
	    st->viper_rotv.gamma += 1;
	    ShowFrame(st);
	}
	
	/* Stop */
	while (speed > 0) {
	    speed -= VIPER_ACCN;
	    z += speed/SPEED_FACTOR;
	    st->viper_posv.z = z;
	    st->viper_rotv.gamma += 1;
	    ShowFrame(st);
	}

	/* Thrust into screen */
	for (;
	     z > VIPER_START_Z; 
	     z -= speed/SPEED_FACTOR) {

	    if (speed < VIPER_SPEED) {
		speed += VIPER_ACCN;
	    }

	    st->viper_rotv.gamma += 1;
	    st->viper_posv.z = z;
	    ShowFrame(st);
	}
	
	/* Brake and turn */
	for (st->viper_rotv.phi = 0; 
	     st->viper_rotv.phi < PI;
	     st->viper_rotv.phi += 2) {
	    if (speed > 0) {
		speed -= VIPER_ACCN;
	    }
	    z -= speed/SPEED_FACTOR;
	    st->viper_posv.z = z;
	    st->viper_rotv.gamma += 1;
	    ShowFrame(st);
	}

	/* Stop */
	while (speed > 0) {
	    speed -= VIPER_ACCN;
	    z -= speed/SPEED_FACTOR;
	    st->viper_posv.z = z;
	    st->viper_rotv.gamma += 1;
	    ShowFrame(st);
	}

	{
	    
	    char buf[256];
	    
	    word_t fps = ((st->frame - st->frames_last_second) * SECONDS(1))/ 
		(NOW() - st->last_second) ;
	    
	    st->frames_last_second = st->frame;
	    
	    st->last_second = NOW();
	    
	    sprintf(buf, "FPS : %u     ", fps);
	    
	    CRend$DrawString(st->window,  128, 16, buf);
	}
	

    }
}

static void ShowFrame(Carnage_st *st)
{
  /* Compute the new shape of the ship */
  st->segbuffer = 1 - st->segbuffer;
  st->nsegs[st->segbuffer] = RenderPoly(st, 
					&viper_poly,
					st->viper_scale,
					&st->viper_posv,
					&st->viper_rotv,
					&st->camera_posv,
					&st->camera_rotm,
					st->segs[st->segbuffer],
					&st->geom);
  
  /* Erase old ship */
  CRend$SetForeground(st->window, BLACK);
  CRend$DrawSegments(st->window,
		 st->segs[1 - st->segbuffer], 
		 st->nsegs[1 - st->segbuffer]);
  
  /* Draw new ship */
  CRend$SetForeground(st->window, WHITE);
  CRend$DrawSegments(st->window,
		 st->segs[st->segbuffer],  
		 st->nsegs[st->segbuffer]);

  {  
    char          buf[256];
    
    sprintf (buf, "Frame: %d", st->frame);

    CRend$DrawString(st->window, 0, 16, buf);
  }

  ProcessEvents(st);

  CRend$Flush(st->window);

  st->frame++;

}

static void ProcessEvents(Carnage_st *st) {

    WS_Event ev;

    while(CRendDisplay$EventsPending(st->display)) {
	
	CRendDisplay$NextEvent(st->display, &ev);

	switch (ev.d.tag) {
	    
	case WS_EventType_Expose:
	{	
	    int i;
	    WS_Rectangle *r = &ev.d.u.Expose;

	    CRend$ExposeRectangle(st->window, r->x1, r->y1, 
				  r->x2 - r->x1, 
				  r->y2 - r->y1);
	    
	    break;
	}
	default: 

	    break;
	}
    }
}


