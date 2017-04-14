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
**	Console.c
**
** FUNCTIONAL DESCRIPTION:
** 
**      Redirection of 'Console' outpur
** 
** ENVIRONMENT: 
**
**      System domain
** 
** ID : $Id: Console.c 1.2 Sat, 08 May 1999 17:24:22 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>
#include <links.h>
#include <time.h>
#include <ecs.h>
#include <IDCMacros.h>
#include <ConsoleWr.ih>
#include <ConsoleControl.ih>
#include <Threads.ih>


#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/* The console is the 'output of last resort' for domains that need to
   talk to something. Where it goes is configured using the 'console='
   command line option. It's exported as sys>console */

/* I intend this code to run in the System domain, although it could
   potentially go somewhere else. */

static Wr_PutC_fn       Wr_PutC_m;
static Wr_PutStr_fn     Wr_PutStr_m;
static Wr_PutChars_fn   Wr_PutChars_m;
static Wr_Seek_fn       Wr_Seek_m;
static Wr_Flush_fn      Wr_Flush_m;
static Wr_Close_fn      Wr_Close_m;
static Wr_Length_fn     Wr_Length_m;
static Wr_Index_fn      Wr_Index_m;
static Wr_Seekable_fn   Wr_Seekable_m;
static Wr_Buffered_fn   Wr_Buffered_m;
static Wr_Lock_fn       Wr_Lock_m;
static Wr_Unlock_fn     Wr_Unlock_m;
static Wr_LPutC_fn      Wr_LPutC_m;
static Wr_LPutStr_fn    Wr_LPutStr_m;
static Wr_LPutChars_fn  Wr_LPutChars_m;
static Wr_LFlush_fn     Wr_LFlush_m;

static Wr_op wr_op = {
    Wr_PutC_m,
    Wr_PutStr_m,
    Wr_PutChars_m,
    Wr_Seek_m,
    Wr_Flush_m,
    Wr_Close_m,
    Wr_Length_m,
    Wr_Index_m,
    Wr_Seekable_m,
    Wr_Buffered_m,
    Wr_Lock_m,
    Wr_Unlock_m,
    Wr_LPutC_m,
    Wr_LPutStr_m,
    Wr_LPutChars_m,
    Wr_LFlush_m,
};

static  ConsoleControl_AddWr_fn        ConsoleControl_AddWr_m;
static  ConsoleControl_AddOffer_fn     ConsoleControl_AddOffer_m;
static  ConsoleControl_RemoveWr_fn     ConsoleControl_RemoveWr_m;
static  ConsoleControl_RemoveOffer_fn  ConsoleControl_RemoveOffer_m;

static  ConsoleControl_op       control_op = {
    ConsoleControl_AddWr_m,
    ConsoleControl_AddOffer_m,
    ConsoleControl_RemoveWr_m,
    ConsoleControl_RemoveOffer_m
};

struct output {
    struct output *next, *prev;
    Wr_clp wr;
    IDCOffer_clp offer;
    IDCClientBinding_clp binding;
    bool_t flush;
};

typedef struct Console_st Console_st;
struct Console_st {
    Wr_cl cl;
    ConsoleControl_cl control_cl;
    mutex_t mu;
    struct output outputs;
};

static void Wr_PutC_m(Wr_cl *self, int8_t ch)
{
    Console_st *st=self->st;
    struct output *o=st->outputs.next;

    while (o!=&st->outputs) {
	if (ch=='\n') {
	    Wr$PutC(o->wr, '\r');
	}
    
	Wr$PutC(o->wr, ch);
	if (o->flush) Wr$Flush(o->wr);
	o=o->next;
    }
}

static void Wr_PutStr_m(Wr_cl *self, string_t string)
{
    Wr_PutChars_m(self, string, strlen(string));
}

static void Wr_PutChars_m(Wr_cl *self, Wr_Buffer string, uint64_t nb)
{
    Console_st *st=self->st;
    struct output *o=st->outputs.next;

    int i=0,j=0;

    while(i<nb) {

	/* Fine the next newline (or end of characters) */
	while((j<nb) && (string[j]!='\n')) {
	    j++;
	}
	
	/* Write the line to each of our output devices */
	while (o!=&st->outputs) {
	    Wr$PutChars(o->wr, string+i, j-i);

	    /* If we're at a newline, convert it to '\r\n' */	    
	    if(string[j]=='\n') {
		Wr$PutChars(o->wr, "\r\n", 2);
	    }
		
	    if (o->flush) Wr$Flush(o->wr);
	    o=o->next;
	}

	/* Move to next section of string */
	i=++j;
    }
}

static void Wr_Seek_m(Wr_cl *self, uint64_t n)
{
    RAISE_Wr$Failure(1);
}

static void Wr_Flush_m(Wr_cl *self)
{
    Console_st *st=self->st;
    struct output *o=st->outputs.next;

    while (o!=&st->outputs) {
	Wr$Flush(o->wr);
	o=o->next;
    }
}

static void Wr_Close_m(Wr_cl *self)
{
    /* NULL function */
}

static uint64_t Wr_Length_m(Wr_cl *self)
{
    return 0; /* Dunno */
}

static uint64_t Wr_Index_m(Wr_cl *self)
{
    return 0; /* Same as Length */
}

static bool_t Wr_Seekable_m(Wr_cl *self)
{
    return False;
}

static bool_t Wr_Buffered_m(Wr_cl *self)
{
    return True;
}

static void Wr_Lock_m(Wr_cl *self)
{
    /* We don't do this: because we are IDC_EXPORTed it can lead to
       deadlocks. We need better client-side writer stubs. */
}

static void Wr_Unlock_m(Wr_cl *self)
{
}

static void Wr_LPutC_m(Wr_cl *self, int8_t ch)
{
    Wr_PutC_m(self, ch);
}

static void Wr_LPutStr_m(Wr_cl *self, string_t s)
{
    Wr_PutStr_m(self, s);
}

static void Wr_LPutChars_m(Wr_cl *self, Wr_Buffer string, uint64_t nb)
{
    Wr_PutChars_m(self, string, nb);
}

static void Wr_LFlush_m(Wr_cl *self)
{
    Wr_Flush_m(self);
}

static bool_t ConsoleControl_AddWr_m (
    ConsoleControl_cl       *self,
    Wr_clp  wr      /* IN */,
    bool_t flush)
{
    Console_st     *st = self->st;
    struct output *o;

    o=Heap$Malloc(Pvs(heap), sizeof(*o));

    o->wr=wr;
    o->offer=NULL;
    o->binding=NULL;
    o->flush=flush;

    MU_LOCK(&st->mu);
    LINK_ADD_TO_HEAD(&st->outputs, o);
    MU_RELEASE(&st->mu);
    return True;
}

static bool_t ConsoleControl_AddOffer_m (
    ConsoleControl_cl       *self,
    IDCOffer_clp    offer   /* IN */,
    bool_t flush)
{
    Console_st     *st = self->st;
    struct output * NOCLOBBER o;

    if (!TypeSystem$IsType(Pvs(types), IDCOffer$Type(offer), Wr_clp__code))
	return False;

    o=Heap$Malloc(Pvs(heap), sizeof(*o));

    o->offer=offer;
    o->flush=flush;

    TRY {
#if 0
	o->binding=IDCOffer$Bind(offer, Pvs(gkpr), &any);
	o->wr=NARROW(&any, Wr_clp);
#else
	o->wr=IDC_BIND(offer, Wr_clp);
#endif /* 0 */
    } CATCH_ALL {
	FREE(o);
	RERAISE;
    } ENDTRY;

    MU_LOCK(&st->mu);
    LINK_ADD_TO_HEAD(&st->outputs, o);
    MU_RELEASE(&st->mu);
    return True;
}

static bool_t ConsoleControl_RemoveWr_m (
    ConsoleControl_cl       *self,
    Wr_clp  wr      /* IN */ )
{
    Console_st     *st = self->st;
    struct output *o, *n;
    bool_t done;

    MU_LOCK(&st->mu);
    o=st->outputs.next;

    done=False;
    while (o!=&st->outputs) {
	if (o->wr==wr && !o->binding) {
	    LINK_REMOVE(o);
	    n=o; o=o->next; FREE(n);
	    done=True;
	} else {
	    o=o->next;
	}
    }
    MU_RELEASE(&st->mu);

    return done;
}

static bool_t ConsoleControl_RemoveOffer_m (
    ConsoleControl_cl       *self,
    IDCOffer_clp    offer   /* IN */ )
{
    Console_st     *st = self->st;
    struct output *o, *n;
    bool_t done;

    MU_LOCK(&st->mu);
    o=st->outputs.next;

    done=False;
    while (o!=&st->outputs) {
	if (o->offer==offer) {
	    LINK_REMOVE(o);
/*	    IDCClientBinding$Destroy(o->binding); */ /* XXX */
	    /* This is waiting for the general IDC cleanup; we can't really
	       do it properly until then. */
	    n=o; o=o->next; FREE(n);
	    done=True;
	} else {
	    o=o->next;
	}
    }
    MU_RELEASE(&st->mu);

    return done;
}

ConsoleControl_clp InitConsole(Wr_clp *console)
{
    struct Console_st *console_st;

    console_st = Heap$Malloc(Pvs(heap), sizeof(*console_st));

    CL_INIT(console_st->cl, &wr_op, console_st);
    CL_INIT(console_st->control_cl, &control_op, console_st);

    MU_INIT(&console_st->mu);

    LINK_INIT(&console_st->outputs);

    /* Chuck another thread into Pvs(entry) in case we need to do
       IDC to our own domain (serial driver, for example) */
    {
	Closure_clp c=Entry$Closure(Pvs(entry));
	Threads$Fork(Pvs(thds), c->op->Apply, c, 0);
    }

    *console=&console_st->cl;
    return &console_st->control_cl;
}
