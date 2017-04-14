/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Nemesis client rendering window system.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      String rendering functions.
** 
** ENVIRONMENT: 
**
**      Shared library
** 
** ID : $Id: events.c 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <time.h>

#include <WS.h>

#include "WSprivate.h"


void WSNextEvent(Display* d,
	    WSEvent* ev)
{
  IO_Rec   rec;
  word_t   value;
  uint32_t nrecs; 

  /* If we have an event already then return it and send ack */
  if (d->ev) {

    *ev = *(d->ev);

    rec.base = d->ev;
    d->ev = NULL;

    rec.len  = sizeof(WSEvent);
    IO$PutPkt(d->evio, 1, &rec, 0, FOREVER);

  } else {   
    /* Await the next event */
    IO$GetPkt(d->evio, 1, &rec, FOREVER, &nrecs, &value);
    *ev = *(WSEvent *)rec.base;
    IO$PutPkt(d->evio, 1, &rec, 0, FOREVER);
  }
  
}

bool_t 
WSEventsPending(Display *d)
{
  IO_Rec rec;
  word_t value;
  uint32_t nrecs;

  /* Already have an event? */
  if (d->ev) 
    return True;
  
  /* Can we get one? */
  if (IO$GetPkt(d->evio, 1, &rec, 0, &nrecs, &value)) {
    d->ev = (WSEvent *)rec.base;
    return True;
  }

  return False;
}
