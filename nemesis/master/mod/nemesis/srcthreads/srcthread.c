/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      mod/nemesis/Threads/SRCThread.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      SRC Mutexes and Condition Variables over events
** 
** ENVIRONMENT: 
**
**      User-land; uses Pvs (evs).
** 
** ID : $Id: srcthread.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <ecs.h>

#include <Pervasives.ih>
#include <SRCThreadMod.ih>
#include <SRCThread.ih>

#ifdef TRACE_SRCTHREAD
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/*
 * Imports
 */



/*
 * Module stuff.
 *
 * This module is pure apart from events, which are picked up from the pvs.
 *
 */

static SRCThread_InitMutex_fn	 InitMutex_m;
static SRCThread_FreeMutex_fn	 FreeMutex_m;
static SRCThread_Lock_fn	 Lock_m;
static SRCThread_Release_fn	 Release_m;
static SRCThread_InitCond_fn	 InitCond_m;
static SRCThread_FreeCond_fn	 FreeCond_m;
static SRCThread_Wait_fn	 Wait_m;
static SRCThread_Signal_fn	 Signal_m;
static SRCThread_Broadcast_fn	 Broadcast_m;
static SRCThread_WaitUntil_fn	 WaitUntil_m;

static SRCThread_op ms = {
    InitMutex_m,
    FreeMutex_m,
    Lock_m,
    Release_m,
    InitCond_m,
    FreeCond_m,
    Wait_m,
    Signal_m,
    Broadcast_m,
    WaitUntil_m,
};

static SRCThreadMod_New_fn SRCThreadMod_New; 
static SRCThreadMod_op srcth_ms = {
    SRCThreadMod_New, 
};

static const SRCThreadMod_cl srcth_cl = { &srcth_ms, NULL };
CL_EXPORT (SRCThreadMod, SRCThreadMod, srcth_cl);


typedef struct {
    SRCThread_cl    cl;
    Heap_cl        *heap; 
} SRCThread_st; 

/*----------------------------------------- SRC Thread Mod Entry Points ---*/
static SRCThread_cl *SRCThreadMod_New(SRCThreadMod_cl *self, Heap_cl *heap) 
{
    SRCThread_st *st; 

    if(!(st = (SRCThread_st *)Heap$Malloc(heap, sizeof(*st)))) 
	return NULL;

    st->heap = heap; 

    CL_INIT(st->cl, &ms, st);
    return &st->cl; 
}


/*--------------------------------------------- SRC Thread Entry Points ---*/

/*
 * Mutexes
 */

static void
InitMutex_m (SRCThread_cl *self, SRCThread_Mutex *mu)
{
    mu->ec = EC_NEW ();
    mu->sq = SQ_NEW ();
    /* ec and sq both have val == 0 */

    return;
}

static void
FreeMutex_m (SRCThread_cl *self, SRCThread_Mutex *mu)
{
    SQ_FREE (mu->sq);
    EC_FREE (mu->ec);
}

static void
Lock_m (SRCThread_cl *self, SRCThread_Mutex *mu)
{
    EC_AWAIT (mu->ec, SQ_TICKET (mu->sq));
}

static void
Release_m (SRCThread_cl *self, SRCThread_Mutex *mu)
{
    EC_ADVANCE (mu->ec, 1);
}

/*
 * Condition variables
 */

static void
InitCond_m (SRCThread_cl *self, SRCThread_Condition *c)
{
    c->ec = EC_NEW ();
    c->sq = SQ_NEW ();

    TRC (eprintf ("SRCThreadMod$InitCond: ec=%x sq=%x\n",
		  c->ec, c->sq));
    
    /* the first Wait should block */
    (void) SQ_TICKET (c->sq);
    return;
}

static void
FreeCond_m (SRCThread_cl *self, SRCThread_Condition *c)
{
    SQ_FREE (c->sq);
    EC_FREE (c->ec);
}

static void
Wait_m (SRCThread_cl *self, SRCThread_Mutex *mu, SRCThread_Condition *c)
{
    Event_Val	t;
    
    t = SQ_TICKET (c->sq);
    Release_m  (self, mu);
    EC_AWAIT (c->ec, t);
    Lock_m     (self, mu);
}

static bool_t
WaitUntil_m (SRCThread_cl *self, SRCThread_Mutex *mu,
	     SRCThread_Condition *c, Time_T until)
{
    Event_Val	t;
    Event_Val	ev;
    
    t = SQ_TICKET (c->sq);
    Release_m  (self, mu);
    ev = EC_AWAIT_UNTIL (c->ec, t, until);
    Lock_m     (self, mu);

    if (EC_LT(ev, t))
	return False;	/* timed out */
    else
	return True;
}

static void
Signal_m (SRCThread_cl *self, SRCThread_Condition *c)
{
    EC_ADVANCE (c->ec, 1);
}

static void
Broadcast_m (SRCThread_cl *self, SRCThread_Condition *c)
{
    Event_Val	to, from;

    to   = SQ_READ (c->sq);
    from = EC_READ (c->ec);

    if (EC_GT (to, from))
	EC_ADVANCE (c->ec, to - from);		       /* ?? to - from - 1 */
}

/*-------------------------------------------------------------------------*/


/*
 * End mod/nemesis/Threads/SRCThread.c
 */
