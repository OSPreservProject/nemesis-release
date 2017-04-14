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
**      otto_aal5.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Very simple AAL5 interface to Otto Driver.
**
**      XXX WARNING WARNING WARNING WARNING WARNING WARNING WARNING 
**
**      Note that this whole interface is currently VERY broken.
**      The OTTO had to be beaten around the head to convince it 
**      to RX AAL5 PDUs on different receive rings.  Currently there
**      are some fairly tricky hardware resource limits.  
**    
**      Note that to reduce per-packet overheads the receive buffer
**      rings on each VCI must be kept permanently stocked with
**      receive buffer descriptors.  No full buffers will be sent to
**      the client unless an empty buffer is available to replace it.
**      
**      XXX WARNING WARNING WARNING WARNING WARNING WARNING WARNING 
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <stdio.h>
#include <time.h>
#include <ecs.h>

#include <ATM.ih>
#include <IOMacros.h>
#include <otto.h>
#include <Heap.ih>
#include <IOEntryMod.ih>
#include <VP.ih>
#include <Threads.ih>

/*
** AAL Interface (temporary)
*/
#include <AALPod.ih>

static AALPod_Open_fn    AALPod_Open_m;
static AALPod_Bind_fn    AALPod_Bind_m;

static AALPod_op Otto_AALPod_ms = { AALPod_Open_m, 
				       AALPod_Bind_m};
#include "state.h"

typedef struct {
    Otto_st		*otto_st;
} Otto_AALPod_st;

typedef struct {
    Otto_AALPod_st  *aal_st;
    ATM_VPI          vpi;
    ATM_VCI          vci;
    AALPod_Master    master;
    AALPod_Mode      mode;
    IOEntry_clp      entry;
    AALPod_Dir       dir;
} Otto_AAL_Connect_st;

extern AALPod_cl *otto_aal_init(Otto_st *otto_st);

static void Otto_AALPod_RXThread(addr_t data);
static void Otto_AALPod_TXThread(addr_t data);

static void init_txclient(Otto_st *, Otto_AAL_Connect_st *, IO_clp);
static void init_rxclient(Otto_st *, Otto_AAL_Connect_st *, IO_clp);


/*
 * IDCCallback methods 
 */
static	IDCCallback_Request_fn 		IO_Request_m;
static	IDCCallback_Bound_fn 		IO_Bound_m;
static	IDCCallback_Closed_fn 		IO_Closed_m;

static	IDCCallback_op	iocb_ms = {
    IO_Request_m,
    IO_Bound_m,
    IO_Closed_m
};


#define TRC(x)
#define QTRC(x) 

/*
** Init; called by driver initialisation code to do AALPod specific things.
*/
AALPod_cl *otto_aal_init(Otto_st *otto_st)
{
    AALPod_cl *Otto_AALPod_clp;
    Otto_AALPod_st *st;
    
    st = (Otto_AALPod_st *)Heap$Malloc(Pvs(heap), 
					sizeof(Otto_AALPod_st) +
					sizeof(AALPod_cl));
    if (st == NULL) 
	RAISE_Heap$NoMemory();

    /* Fill in state record */
    st->otto_st= otto_st;
    
    Otto_AALPod_clp = (AALPod_clp)(st+1);
    CLP_INIT(Otto_AALPod_clp, &Otto_AALPod_ms, st);

    CLP_INIT(&st->otto_st->iocb_cl, &iocb_ms, st);

    return Otto_AALPod_clp;
}


/*
** AALPod: Open Method.
** This method is invoked by a potential client across IDC. If everything goes
** ok, we'll create an IOTransport offer to which the client can bind.
*/


static IDCOffer_clp AALPod_Open_m (
        AALPod_cl      *self,
	AALPod_Dir      dir,
	AALPod_Master   master,
        AALPod_Mode     mode,    /* IN */
        ATM_VPI         vpi,     /* IN */
        ATM_VCI         vci,      /* IN */
	uint32_t        slots)
{
    Otto_AALPod_st *st = self->st;
    Otto_AAL_Connect_st *cst;
    IOTransport_clp iot;
    IOOffer_clp offer;
    IDCService_clp service;  
    IOEntryMod_clp emod;
    IOEntry_clp entry;

    PRINT_NORM("(%p, %d, %d, %d, %d, %d, %d)\n",
	       self, dir, master, mode, (uint32_t)vpi, (uint32_t)vci, slots);

    if (st->otto_st->w13mode)
    {
	PRINT_ERR("oppo: w13 mode previously engaged\n");
	RAISE_ATM$VCIInUse(0, 0);
    }

    if(master == AALPod_Master_Server) {

	/* Not with the otto */
	PRINT_ERR("oppo driver cannot master\n");
	RAISE_AALPod$BadMaster();
    }

    if(mode == AALPod_Mode_AAL0 || mode == AALPod_Mode_AAL34 || 
       mode == AALPod_Mode_AAL5)
    {
	/* Not implemented yet */
	PRINT_ERR("oppo driver does not support selected mode\n");
	RAISE_AALPod$BadMode();
    }

    if(!slots) slots = 128; /* Value taken from old Otto driver */

    /* Create the IO info */
 
    /* Create a (default) entry. XXX SMH: should this be custom? */
    emod= NAME_FIND ("modules>IOEntryMod", IOEntryMod_clp);
    entry = IOEntryMod$New (emod, Pvs(actf), Pvs(vp),
			    VP$NumChannels (Pvs(vp)));
    /* Create connect state */
    cst = (Otto_AAL_Connect_st *)Heap$Malloc (Pvs(heap), sizeof (*cst));
    if (!cst) RAISE_AALPod$NoResources();    
    
    cst->vpi     = vpi;
    cst->vci     = vci;
    cst->master  = master;
    cst->mode    = mode;
    cst->dir     = dir;
    cst->aal_st  = st;
    cst->entry   = entry;

    /* Create the offer */
    iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);
    offer = IOTransport$Offer (iot, Pvs(heap), slots, (AALPod_Dir_TX) ? 
			       IO_Mode_Tx : IO_Mode_Rx, IO_Kind_Master, 
			       NULL, Pvs(gkpr), &st->otto_st->iocb_cl, 
			       entry, (word_t)cst, &service);

    st->otto_st->normalmode = True;

    return (IDCOffer_cl *)offer;
}

static void AALPod_Bind_m (AALPod_cl      *self,
			   AALPod_Dir      dir,
			   AALPod_Master   master,
			   AALPod_Mode     mode,    /* IN */
			   ATM_VPI         vpi,     /* IN */
			   ATM_VCI         vci,     /* IN */
			   IDCOffer_clp    offer)
{
    /* XXX NAS Should implement this */
}



/* IDCCallback methods; used to fork service threads. */

static bool_t IO_Request_m (IDCCallback_cl *self, IDCOffer_clp offer,
			    Domain_ID dom, ProtectionDomain_ID pdid,
			    const Binder_Cookies *clt_cks, 
			    Binder_Cookies *svr_cks)
{
    return True;
}

static bool_t IO_Bound_m (IDCCallback_cl *self, IDCOffer_clp offer,
			  IDCServerBinding_clp binding, Domain_ID dom,
			  ProtectionDomain_ID pdid, 
			  const Binder_Cookies *clt_cks, Type_Any *server)    
{
    Otto_st             *st = self->st;
    IO_cl               *io = NARROW(server, IO_clp);
    Otto_AAL_Connect_st *cst;

    cst = (Otto_AAL_Connect_st *)IO_HANDLE(io);

    if(cst->dir == AALPod_Dir_RX) 
	init_rxclient(st, cst, io);
    else
	init_txclient(st, cst, io);

    return True; 
}

static void IO_Closed_m (IDCCallback_cl *self, IDCOffer_clp offer,
			 IDCServerBinding_clp binding, const Type_Any *server)
{
    return;
}






/* XXX NAS Should move some initialisation code here */

static void init_txclient(Otto_st *st, Otto_AAL_Connect_st *cst, IO_clp io)
{
    (void)Threads$Fork(Pvs(thds), Otto_AALPod_TXThread, cst, 0);
}

static void init_rxclient(Otto_st *st, Otto_AAL_Connect_st *cst, IO_clp io)
{
   (void)Threads$Fork(Pvs(thds), Otto_AALPod_RXThread, cst, 0); 
}

/*
** IO threads.
*/
static void Otto_AALPod_RXThread(addr_t data)
{
    Otto_AAL_Connect_st *st = (Otto_AAL_Connect_st *)data;
    Otto_AALPod_st *aal_st = st->aal_st;
    Otto_st *otto_st         = aal_st->otto_st;
    struct otto_hw *otto_hw  = otto_st->otto_hw;
    OTTO_LOCKTMP s;
    word_t ring;
    word_t rxclient;
    IO_clp io= IOEntry$Rendezvous(st->entry, FOREVER);
    
    printf("Otto_AALPod_RXThread: st:%p VPI:%d VCI:%d IO:%p\n",
	    st, (uint32_t)st->vpi, (uint32_t)st->vci, io);
    
    
    OTTO_LOCK (&otto_hw->statelock, s);
    
    /* Get a receive ring */
    ring = otto_allocring(otto_hw);
    initring(otto_st, ring);
    
    /* Record the RX client details */
    rxclient = otto_allocrxclient(otto_st);
    otto_st->rxclient[rxclient].io = io;
	    otto_st->rxclient[rxclient].ring = ring;
    otto_hw->rings[ring].rxclient = rxclient;
    
    /* Update the VC */
    otto_hw->vcs[st->vci].rxring   = ring; 
    otto_hw->vcs[st->vci].rxclient = rxclient;
    otto_setrxvc (otto_hw, st->vci, 0, 1, 0, otto_hw->rings[ring].desc);  
    
    TRC(printf("Otto_AALPod_RXThread: Using ring %d (desc %d) for VCI %d\n", 
		ring, otto_hw->rings[ring].desc, st->vci));
    
    /* Try to queue RX buffers */
    TRC(printf("Otto_AALPod_RXThread: Queue buffers ====================\n"));
    
#define OTTO_RCVPKTS    (OTTO_MAXPKTSPERRING  -1 )
    queuercvbuffers (otto_st, ring, OTTO_RCVPKTS);
    
    OTTO_UNLOCK (&otto_hw->statelock, s);

    EC_AWAIT_UNTIL(EC_NEW(), 1, FOREVER);
}
    
    




#define OTTO_NCELLS(x)  ((((x+47)/16)*683)/2048)
/* This works for x < 32721 and gccarm -O2 turns it into very neat code
* viz:
*       add     r3, r0, #47
*       mov     r3, r3, lsr #4
*       add     r0, r3, r3, asl #2
*       add     r0, r3, r0, asl #1
*       rsb     r0, r0, r0, asl #5
*       add     r0, r3, r0, asl #1
*       mov     r0, r0, lsr #11
*
* XXX On Alpha Make sure you use OTTO_NCELLS(word_t len)
*
*       srl     t1, 0x4, t1
*       s4addq  t1, t1, t0
*       s4subq  t0, t1, t0
*       s8addq  t0, t0, t0
*       s4subq  t0, t1, t0
*       srl     t0, 0xb, t0
*       addl    t0, zero, t3
*
*/
static void Otto_AALPod_TXThread(addr_t data)
{
    Otto_AAL_Connect_st *st = (Otto_AAL_Connect_st *)data;
    Otto_AALPod_st *aal_st = st->aal_st;
    Otto_st *otto_st         = aal_st->otto_st;
    struct otto_hw *otto_hw  = otto_st->otto_hw;
    unsigned vci = st->vci;
    word_t txclient;
    word_t i;


    IO_clp io= IOEntry$Rendezvous(st->entry, FOREVER);
    
    printf("Otto_AAL5_TXThread: st:%p VPI:%d VCI:%d IO:%p\n",
		st, (uint32_t)st->vpi, (uint32_t)st->vci, io);
    
    PAUSE(SECONDS(5));
    
    OTTO_LOCK (&otto_hw->statelock, s);
    
    /* Record the TX client details */
    txclient = otto_alloctxclient(otto_st);
    TRC(printf("TXCLIENT %x\n", txclient));
    otto_st->txclient[txclient].io = io;
    otto_st->txclient[txclient].ev = 0;
    TRC(printf("Advance EC %lx by %x\n", 
		otto_st->txclient[txclient].ec, OTTO_TXQLEN-1));
    EC_ADVANCE(otto_st->txclient[txclient].ec, OTTO_TXQLEN-1);
    
    /* XXX Static allocation of TX packets for this client */
    for (i = 0; i < OTTO_TXQLEN; i++) {
	word_t txpktindex = otto_alloctxpkt(otto_hw);
	struct otto_txpkt *txpkt = &otto_hw->txpkts[txpktindex];
	otto_st->txclient[txclient].pkt[i] = txpkt;
	otto_prepare_txpkt(otto_st, txpkt, vci);
    }
    
    /* Update the VC structure */
    otto_hw->vcs[st->vci].txclient = txclient;
    
#ifdef DUBIOUS_SCHEDULED_VCI
    /* XXX Dubious scheduled VCI - add QoS parameters and an OOB channel
       to the I/F */
    /* void otto_settxvc (struct otto_hw *ohw, unsigned vc,
     *                    int usesched, int enflowcntl, int overflow, 
     *                    int credits) */
    otto_settxvc (otto_hw, st->vci, 1, 0, 0, 0);  
    if (otto_update_bandwidth (otto_hw, st->vci, 
			       (20 * otto_hw->schedlen / 155)) == 1) {
	TRC(printf("TXConnect_m: set B/W to 20 Mbps\n"));
	otto_newsched(otto_hw);
    }
#endif 
    
    OTTO_UNLOCK (&otto_hw->statelock, s);

    TRY {
    
	/* Nearly Minimalist loop */
	while(True) {
	    word_t p = otto_st->txclient[txclient].ev & (OTTO_TXQLEN-1);
	    struct otto_txpkt *txpkt;
	    struct rbuf *r;
	    word_t len;
	
	    /* Wait until TX is available for this client */
	    EC_AWAIT(otto_st->txclient[txclient].ec,
		     otto_st->txclient[txclient].ev);
	    otto_st->txclient[txclient].ev++;
	
	    txpkt = otto_st->txclient[txclient].pkt[p];
	    r = txpkt->r;
	    RGET(r, io, R_WAIT); 
	    for (len = 0, i = 0; i < r->r_nrecs; i++) len += r->r_recs[i].len;
	    r->r_len  = len;
	    r->r_type = RT_FULL;
	
	    /* Must be an integral number of cells */
	    if (!otto_st->w13mode && OTTO_NCELLS(len)*48 != len) goto ditch;
	
	    if (!otto_enqueue_txpkt(otto_st, txpkt, vci)) goto ditch;
	
	    continue;
	    
	ditch:
	    TRC(printf("DITCH")); /* XXX Temp nastiness - won't work with mem. prot! */ 
	    
	    /* Ditch the packet */
	    RPUT(txpkt->r);
	}

    } CATCH_Thread$Alerted() {
	/* IO$Dispose() probably called */
	printf("oppo: TX Thread$Alerted: shutting down\n");

	/* if we were in w13mode, then we're not any more */
	otto_st->w13mode = False;

	/* XXX need to clear up other data structures! */
    } ENDTRY;
}




