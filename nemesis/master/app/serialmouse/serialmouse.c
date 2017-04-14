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
**      Serial mouse driver
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Drives serial mice
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: serialmouse.c 1.1 Thu, 18 Feb 1999 15:08:31 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <IDCMacros.h>
#include <contexts.h>

#include <stdio.h>

#include <Rd.ih>
#include <Serial.ih>
#include <Mouse.ih>

#include <IOEntry.ih>

#include <IOMacros.h>

#define TRC(x)

Closure_Apply_fn Main;
static Closure_op   ms = { Main };
static const Closure_cl cl = {&ms, NULL};

CL_EXPORT(Closure, SerialMouse, cl);

typedef struct _sm_st {
    Rd_clp r;
    IO_clp io;
    bool_t middle_state;
    Mouse_Buttons prev;
    IOEntry_clp entry;
} sm_st;

static void BuildEv(sm_st *st, Mouse_Event *state, uint8_t *data)
{
    state->time=NOW();
    state->buttons=
	(data[0]&0x20 ? SET_ELEM(Mouse_Button_Left) : 0) |
	(data[0]&0x10 ? SET_ELEM(Mouse_Button_Right) : 0);
    st->prev=state->buttons;
    state->buttons|=
	(st->middle_state ? SET_ELEM(Mouse_Button_Middle) : 0);
    
    state->dx=(char)(((data[0] & 0x03) << 6) | (data[1] & 0x3F));
    state->dy=-(char)(((data[0] & 0x0C) << 4) | (data[2] & 0x3F));
}

#define INPUT_BUZSZ 32
static void InterruptThread(sm_st *st)
{
    uint8_t c;
    uint8_t buff[INPUT_BUZSZ];
    uint8_t x;
    IO_Rec rec;
    uint32_t nrecs, val;
    Mouse_Event *ev;
    Mouse_Event temp;
    bool_t first=True;
    bool_t threebutton=False;

    x=0;
    while(1) {
	c=Rd$GetC(st->r);

	if (first) {
	    first=False;
	    if (c=='3') threebutton=True;
	    TRC(printf("serialmouse: three button mouse\n"));
	}

	if (c&0x40) {
	    x=1;
	    buff[0]=c;
	} else {
	    if (x >= INPUT_BUZSZ)
	    {
		printf( "serialmouse: input buffer overrun!\n");
		x=0;	/* not sure if throwing away the data is good */
	    }
	    buff[x++]=c;
	}

	if (x==3) {
	    if (st->io && IO$GetPkt(st->io, 1, &rec, 0, &nrecs, &val)) {
		ev=(void *)rec.base;
		BuildEv(st, ev, buff);
		IO$PutPkt(st->io, 1, &rec, 1, 0);
	    } else {
		ev=&temp;
		BuildEv(st, ev, buff);
		TRC(printf("Mouse packet: %c%c%c "
			    "%d %d\n",
			    SET_IN(ev->buttons, Mouse_Button_Left)?'L':'-',
			    SET_IN(ev->buttons, Mouse_Button_Middle)?'M':'-',
			    SET_IN(ev->buttons, Mouse_Button_Right)?'R':'-',
			    ev->dx, ev->dy));
	    }
	}

	if (x==4) {
	    if (st->middle_state != (!!(c&0x20))) {
		/* Something changed */
		st->middle_state=!!(c&0x20);
		if (st->io &&
		    IO$GetPkt(st->io, 1, &rec, 0, &nrecs, &val)) {
		    ev=(void *)rec.base;
		    ev->time=NOW();
		    ev->dx=0; ev->dy=0;
		    ev->buttons=st->prev |
			(st->middle_state?SET_ELEM(Mouse_Button_Middle):0);
		    IO$PutPkt(st->io, 1, &rec, 1, 0);
		} else {
		    ev=&temp;
		    ev->time=NOW();
		    ev->dx=0; ev->dy=0;
		    ev->buttons=st->prev |
			(st->middle_state?SET_ELEM(Mouse_Button_Middle):0);
		    TRC(fprintf(
			stderr,"Mouse ext packet: %c%c%c\n",
			SET_IN(ev->buttons,Mouse_Button_Left)?'L':'-',
			SET_IN(ev->buttons,Mouse_Button_Middle)?'M':'-',
			SET_IN(ev->buttons,Mouse_Button_Right)?'R':'-'
			));
		}
	    }
	}
	if (x>4) {
	    st->middle_state=False; /* Stop middle button stick at startup */
	}
    }
}

static void WaitThread(sm_st *st)
{
    /* Wait around for a client */
    while (1) {
	st->io=IOEntry$Rendezvous(st->entry, FOREVER);
    }
}

void Main(Closure_clp self)
{
    Serial_clp s;
    Rd_clp r;
    uint8_t c;
    uint32_t i;
    bool_t present;
    sm_st *st;

    TRC(printf("serialmouse: started\n"));
    s=IDC_OPEN("dev>serial1>control", Serial_clp);

    r=IDC_OPEN("dev>serial1>rd", Rd_clp);

    Serial$SetSpeed(s, 1200);
    Serial$SetProtocol(s, 7, Serial_Parity_None, 1);
    Serial$SetControlLines(s, 0);
    Serial$SendBreak(s);
    Serial$SetControlLines(s, SET_ELEM(Serial_ControlLine_RTS) |
			   SET_ELEM(Serial_ControlLine_DTR));

    /* Look for the mouse presence string */
    i=10; present=False;
    while(i && !present) {
	c=Rd$GetC(r);
	TRC(printf("Mouse returned ID character '%c'\n", c));
	if (c=='M') present=1;
    }

    if (!present) {
	printf("serialmouse: no serial mouse detected\n");
	return;
    }

    printf("serialmouse: serial mouse detected\n");

    /* Export ourselves */
    st=Heap$Malloc(Pvs(heap), sizeof(*st));
    if (!st) {
	fprintf(stderr,"SerialMouse: no memory\n");
	RAISE_Heap$NoMemory();
	return;
    }

    st->entry = IO_EXPORT("dev>mouse_serial", 0, IO_Mode_Rx, IO_Kind_Slave,  
			   4096, 15, NULL);
    st->r=r;
    st->io=NULL;
    st->middle_state=False;

    /* Start the server thread */
    Threads$Fork(Pvs(thds), InterruptThread, st, 0);
    Threads$Fork(Pvs(thds), WaitThread, st, 0);
}
