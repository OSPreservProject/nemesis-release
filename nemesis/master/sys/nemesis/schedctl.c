/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
**
** ID : $Id: schedctl.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <ntsc.h>
#include <time.h>
#include <dcb.h>
#include <sdom.h>
#include <exceptions.h>
#include <contexts.h>

#include <QoSCtl.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	QoSCtl_GetQoS_fn 		GetQoS_m;
static	QoSCtl_SetQoS_fn 		SetQoS_m;
static	QoSCtl_Query_fn 		Query_m;
static	QoSCtl_ListClients_fn 		ListClients_m;

static	QoSCtl_op	ms = {
    GetQoS_m,
    SetQoS_m,
    Query_m,
    ListClients_m
};

#define MAXCLIENTS 64


#define PARANOIA
#ifdef PARANOIA
#define ASSERT(cond,fail) {						\
    if (!(cond)) { 							\
	{fail}; 							\
	while(1); 							\
    }									\
}
#else
#define ASSERT(cond,fail)
#endif


typedef struct {
    Time_ns p, s, l;
    bool_t  x;
} qos_t;

typedef struct {
    dcb_ro_t     *dcb;
    SDom_t       *sdom;
    Domain_ID     did;
    qos_t         qos;
} client_t;


typedef struct {
    QoSCtl_cl     cl;
    int 	  nclients;
    bool_t        bogus;
    client_t      clientp[MAXCLIENTS];
    QoSCtl_Client clients[MAXCLIENTS];
    Context_clp   proc_cx;
} qosctl_st;


#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x) 
#endif

bool_t sched_ctl_init()
{
    qosctl_st *st;
    NOCLOBBER bool_t res = False;

    TRC(eprintf("sched_ctl_init: entered.\n"));
    if(!(st = Heap$Malloc(Pvs(heap), sizeof(*st)))) {
	eprintf("sched_qosctl_init(): malloc failed.\n");
	ntsc_dbgstop();
    }

    TRC(eprintf("sched_ctl_init: alloced state.\n"));
    
    st->nclients = 0;
    st->bogus = False;
    memset(&st->clientp, 0, sizeof(client_t) * MAXCLIENTS);
    memset(&st->clients, 0, sizeof(QoSCtl_Client) * MAXCLIENTS);
    
    TRC(eprintf("sched_ctl_init: bzero'd state.\n"));

    CL_INIT(st->cl, &ms, st);
    CX_ADD("sys>schedctl", &st->cl, QoSCtl_clp);

    TRC(eprintf("sched_ctl_init: added 'sys>schedctl'.\n"));

    TRY {
	st->proc_cx = NAME_FIND("proc>domains", Context_clp);
	res = True;
    } CATCH_Context$NotFound(UNUSED name) {
	eprintf("sched_ctl_init: lookup of 'proc>domains' failed.\n");
	res = False;
    } ENDTRY;

    TRC(eprintf("sched_ctl_init: returning home.\n"));
    return res;
}



static INLINE uint32_t getidx(qosctl_st *st, QoSCtl_Handle hdl) 
{
    int i;

    for(i=0; i < st->nclients; i++) 
	if(st->clients[i].hdl == hdl)
	    break;

    if(i == st->nclients) {
	printf("Urk! GetQoS! Badhandle %p!\n", hdl);
	ntsc_dbgstop();
/*	RAISE("QoSCtl_BadHandle", hdl); */
    }

    return i;
}



/*---------------------------------------------------- Entry Points ----*/

static bool_t GetQoS_m (QoSCtl_cl *self, QoSCtl_Handle	hdl /* IN */
			/* RETURNS */, Time_T *p, Time_T *s, Time_T *l )
{
    qosctl_st *st = self->st;
    qos_t *q;
    int i = getidx(st, hdl);
    bool_t x;

    ASSERT(hdl == st->clients[i].hdl, ntsc_dbgstop(); );

    q = &st->clientp[i].qos;
    if(st->clientp[i].sdom) {
	ENTER_KERNEL_CRITICAL_SECTION();
	*p = (q->p = st->clientp[i].sdom->period);
	*s = (q->s = st->clientp[i].sdom->slice);
	*l = (q->l = st->clientp[i].sdom->latency);
	x = (q->x = st->clientp[i].sdom->xtratime);
	LEAVE_KERNEL_CRITICAL_SECTION();
    } else {
	*p = *s = *l = 0;
	return False;
    }

    return x;
}

static bool_t SetQoS_m (QoSCtl_cl *self, QoSCtl_Handle hdl /* IN */,
			Time_T	p, Time_T s, Time_T l, bool_t x)
{
    qosctl_st *st = self->st;
    qos_t *q;
    int i = getidx(st, hdl);

    ASSERT(hdl == st->clients[i].hdl, ntsc_dbgstop(); );

    q = &st->clientp[i].qos;
    
    if(st->clientp[i].sdom) {
	ENTER_KERNEL_CRITICAL_SECTION();
	q->p = (st->clientp[i].sdom->period = p);
	q->s = (st->clientp[i].sdom->slice = s);
	q->l = (st->clientp[i].sdom->latency = l);
	q->x = (st->clientp[i].sdom->xtratime = x);
	LEAVE_KERNEL_CRITICAL_SECTION();
	return True;
    } else 
	return False;
}
 

/* 
** Query() tells us how much guaranteed and xtra time we 
** got at the last sample; tot is the time elapsed over the last
** sample (e.g. the domain's period or the QoSEntry's period, etc).
*/
static Time_T Query_m(QoSCtl_cl *self, QoSCtl_Handle hdl, /* RETURNS */ 
		      Time_T *tg, Time_T *sg, Time_T *tx, Time_T *sx )
{
    qosctl_st *st = self->st;
    qos_t *q;
    Time_T now = NOW();
    int i = getidx(st, hdl);
    client_t *client = &st->clientp[i];

    ASSERT(hdl == st->clients[i].hdl, ntsc_dbgstop(); );

#if 0
    eprintf("Query: [did %lx]: dcb at %p, sdom at %p\n", 
	    client->did, client->dcb, client->sdom);
#endif

    q = &st->clientp[i].qos;

    if(st->clientp[i].sdom) {

	/* This should really be atomic as well..... */
	ENTER_KERNEL_CRITICAL_SECTION(); 
	*tg = client->sdom->ac_g;
	*sg = client->sdom->ac_lp;
	*tx = client->sdom->ac_x;
	*sx = now;   /* This scheduler could be modified to provide a 
			more useful value. */
	LEAVE_KERNEL_CRITICAL_SECTION();

	return 1;
    } 
    else /* Client handle is out of date */
    {  
	return 0;
    }

}


static bool_t ListClients_m(QoSCtl_cl *self, QoSCtl_Client **clients, 
			    uint32_t *nclients )
{
    qosctl_st *st = self->st;
    bool_t changes = False;
    Context_clp dom_cx; 
    Context_Names *NOCLOBBER names;
    Type_Any any;
    dcb_ro_t *dcbro;
    SDom_t *sdom;
    string_t name;
    int i, nnames;

    names  = Context$List(st->proc_cx);
    nnames = SEQ_LEN(names);

    if(st->bogus || (nnames != st->nclients)) { /* got an update to do */
	
	st->nclients = nnames;
	st->bogus = False;
	changes = True;

	for(i=0; i < SEQ_LEN(names); i++) {
	    if(!Context$Get(st->proc_cx, SEQ_ELEM(names, i), &any)) {
		printf("Something severely shafted.\n");
		RAISE("QoSCtl_Context_Error", 0);
	    } 
	    
	    /* get sdom and name info, etc */
	    dom_cx = NARROW(&any, Context_clp);
	    dcbro  = NAME_LOOKUP("dcbro", dom_cx, addr_t);
	    ENTER_KERNEL_CRITICAL_SECTION();
	    name   = NAME_LOOKUP("name", dom_cx, string_t);

	    st->clients[i].hdl   = (word_t)&st->clients[i];
	    st->clients[i].name  = name;
	    st->clients[i].did   = dcbro->id;
	    st->clientp[i].dcb   = dcbro;


	    st->clientp[i].sdom  = (sdom = dcbro->sdomptr);
	    if(sdom) {
		st->clientp[i].qos.p = sdom->period;
		st->clientp[i].qos.s = sdom->slice;
		st->clientp[i].qos.l = sdom->latency;
		st->clientp[i].qos.x = sdom->xtratime;
	    } else {
		/* Domain created, but not yet added to scheduler (no sdom) */
		memset(&(st->clientp[i].qos), 0, sizeof(qos_t));
		st->bogus = True; /* XXX PRB HACK HACK */
	    }
	    LEAVE_KERNEL_CRITICAL_SECTION();
	}

    }

    *clients  = st->clients;
    *nclients = st->nclients;

    SEQ_FREE_ELEMS (names);
    SEQ_FREE (names);

    return changes;
}
