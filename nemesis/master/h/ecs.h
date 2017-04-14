/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      ecs.h
** 
** DESCRIPTION:
** 
**      Handy macros for dealing with event counts & sequencers. 
** 
** ID : $Id: ecs.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _ecs_h_
#define _ecs_h_

#include <Events.ih>

/* 
 * Eventcount and Sequencer stuff
 */

/*
 * The following 4 macros do the right thing even if event values have
 * wrapped, on the ASSUMPTION that the values being compared never
 * differ by 2^(word-size - 1) or more.  NB. "Event_Val"s are incremented
 * as UNsigned quantities and their differences are compared with 0 here as
 * SIGNED quantities.
 */

#define EC_LT(ev1,ev2)		     (((sword_t) ((ev1) - (ev2))) < 0)
#define EC_LE(ev1,ev2)		     (((sword_t) ((ev1) - (ev2))) <= 0)
#define EC_GT(ev1,ev2)		     (((sword_t) ((ev1) - (ev2))) > 0)
#define EC_GE(ev1,ev2)		     (((sword_t) ((ev1) - (ev2))) >= 0)

#define EC_NEW()         	     Events$New   	(Pvs(evs))
#define EC_FREE(ec)        	     Events$Free    	(Pvs(evs),ec)
#define EC_READ(ec)        	     Events$Read    	(Pvs(evs),ec)
#define EC_ADVANCE(ec,inc) 	     Events$Advance 	(Pvs(evs),ec,inc)
#define EC_AWAIT(ec,val)   	     Events$Await       (Pvs(evs),ec,val)
#define EC_AWAIT_UNTIL(ec,val,until) Events$AwaitUntil  (Pvs(evs),ec,val,until)

#define SQ_NEW()         	     Events$NewSeq   (Pvs(evs))
#define SQ_FREE(sq)        	     Events$FreeSeq  (Pvs(evs),sq)
#define SQ_READ(sq)		     Events$ReadSeq  (Pvs(evs),sq)
#define SQ_TICKET(sq)                Events$Ticket   (Pvs(evs),sq)


/* Pause for a while (you'll need to include "time.h" too) */
#define PAUSE(ns) \
  Events$AwaitUntil (Pvs(evs), NULL_EVENT, 1, NOW() + (ns))


#endif /* _ecs_h_ */

