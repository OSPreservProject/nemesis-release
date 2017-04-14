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
**      nicstar_aal5.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Very simple AAL5 interface to Nicstar Driver.
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
** ID : $Id: nicstar_aal5.c 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
** LOG: $Log: nicstar_aal5.c,v $
** LOG: Revision 1.14  1998/06/23 11:31:48  smh22
** LOG: y
** LOG:
** LOG: Revision 1.13  1998/04/01 14:43:43  smh22
** LOG: new IO world
** LOG:
** LOG: Revision 1.12  1997/04/10 17:00:21  pbm1001
** LOG: Cleaned up
** LOG:
** LOG: Revision 1.11  1997/04/09 21:55:01  pbm1001
** LOG: Reorganised
** LOG:
** LOG: Revision 1.10  1997/04/08 21:56:49  rjb17
** LOG: noclobbers
** LOG:
** LOG: Revision 1.9  1997/01/05 17:19:22  pbm1001
** LOG: Use IONotify, maybe work with C3 cards again
** LOG:
** LOG: Revision 1.1  1996/11/11 14:38:52  pbm1001
** LOG: Initial revision
** LOG:
 * Revision 1.1  1995/10/12  11:08:46  prb12
 * Initial revision
 *
**
*/

#include <nemesis.h>
#include <stdio.h>

#include <ATM.ih>
#include <IOMacros.h>
#include <nicstar.h>
#include <Heap.ih>
#include <IOEntryMod.ih>
#include <VP.ih>
#include <Threads.ih>

/*
** AAL5 Interface (temporary)
*/
#include <AALPod.ih>

static AALPod_Open_fn    AALPod_Open_m;
static AALPod_Bind_fn    AALPod_Bind_m;

static AALPod_op Nicstar_AALPod_ms = { AALPod_Open_m, 
				       AALPod_Bind_m};

typedef struct {
    ATM_VPI          vpi;
    ATM_VCI          vci;
    AALPod_Master    master;
    AALPod_Mode      mode;
    AALPod_Dir       dir;
} Nicstar_AAL_Connect_st;



extern AALPod_clp nicstar_aal_init(Nicstar_st *st);

static void Nicstar_TX_Thread(nicstar_txclient*);
static void init_txclient(Nicstar_st *, Nicstar_AAL_Connect_st *, IO_clp);
static void init_rxclient(Nicstar_st *, Nicstar_AAL_Connect_st *, IO_clp);

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

/*
** Init; called by driver initialisation code to do AALPod specific things.
*/
AALPod_clp nicstar_aal_init(Nicstar_st *st)
{
    int NOCLOBBER i;
    
    CLP_INIT(&st->aalpod_cl, &Nicstar_AALPod_ms, st);
    CLP_INIT(&st->iocb_cl,   &iocb_ms, st);
   
    /* 
     * Export the AAL5 Interface
     */
    
    /* Assume that if we have more than 10 atm device entries, 
       some error has probably occurred */
    
    for(i=0; i<10; i++) {
	
	char devname[32];
	
	sprintf(devname, "dev>atm%d", i);
	
	TRY {
	    IDC_EXPORT (devname, AALPod_clp, &st->aalpod_cl);
	    
	    printf("Exported %s\n", devname);
	    break;
	} CATCH_Context$Exists() {
	} ENDTRY;
    }

    if(i==10) {
	fprintf(stderr, "Error exporting nicstar into dev>\n");
    }
    
    return &st->aalpod_cl;
}


/*
** AALPod: Open Method.
** This method is invoked by a potential client across IDC. If everything goes
** ok, we'll create an IOTransport offer to which the client can bind. We also
** start up a thread to deal with the IOs to/from the client.
*/


static IDCOffer_clp AALPod_Open_m (
        AALPod_cl      *self,
	AALPod_Dir      dir,
	AALPod_Master   master,
        AALPod_Mode     mode,    /* IN */
        ATM_VPI          vpi,     /* IN */
        ATM_VCI          vci,      /* IN */
	uint32_t              slots)
{
    Nicstar_st *st = self->st;
    Nicstar_AAL_Connect_st *cst;
    IOTransport_clp iot;
    IOOffer_clp offer;
    IDCService_clp service;  

    PRINT_NORM("(%p, %d, %d, %d, %d, %d, %d)\n",
	       self, dir, master, mode, (uint32_t)vpi, (uint32_t)vci, slots);

    if(master == AALPod_Master_Server) {
	
	if(st->rev_C3) {
	    PRINT_ERR("Rev C3 card: can't master channel\n");
	    
	    RAISE_AALPod$BadMaster();
	}

	if(dir == AALPod_Dir_TX) {
	    
	    PRINT_ERR("Can't master TX channel\n");
	    
	    RAISE_AALPod$BadMaster();
	    
	}    

	if(mode == AALPod_Mode_AAL34) {
	    
	    /* This should be possible, but the card is broken */
	    PRINT_ERR("Can't master AAL3/4 RX channel\n");

	    RAISE_AALPod$BadMaster();
	}

	if(!st->allow_client_master) {
	    PRINT_ERR("Client RX master mode not permitted\n");

	    RAISE_AALPod$BadMaster();
	}

    }



    if(!slots) slots = st->default_num_buffers;

    if(st->num_reserve_bufs - slots < MIN_RESERVE_BUFS) {

	PRINT_ERR("Request would overcommit buffers\n");
	
	RAISE_AALPod$NoResources();

    }

    nicstar_open(st, dir, vpi, vci);

    st->num_reserve_bufs -= slots;
    

    /* Create the IO info */

    /* Create connect state */
    cst = (Nicstar_AAL_Connect_st *)Heap$Malloc (Pvs(heap), sizeof (*cst));
    if (!cst) RAISE_AALPod$NoResources();    
    
    cst->vpi     = vpi;
    cst->vci     = vci;
    cst->master  = master;
    cst->mode    = mode;
    cst->dir     = dir;

    /* Create the offer */
    iot = NAME_FIND ("modules>IOTransport", IOTransport_clp);
    offer = IOTransport$Offer (iot, Pvs(heap), slots, (AALPod_Dir_TX) ? 
			       IO_Mode_Tx : IO_Mode_Rx, IO_Kind_Master, 
			       NULL, Pvs(gkpr), &st->iocb_cl, NULL,
			       (word_t)cst, &service);

    return (IDCOffer_cl *)offer;
}

static void AALPod_Bind_m (AALPod_cl      *self,
			   AALPod_Dir      dir,
			   AALPod_Master   master,
			   AALPod_Mode     mode,    /* IN */
			   ATM_VPI          vpi,     /* IN */
			   ATM_VCI          vci,      /* IN */
			   IDCOffer_clp   offer)
{
    Nicstar_st *st = self->st;
    Nicstar_AAL_Connect_st *cst;
    IO_clp io;

    PRINT_NORM("(%p, %d, %d, %d)\n",
	       self, mode, (uint32_t)vpi, (uint32_t)vci);

    if(master == AALPod_Master_Server) {
	
	if(st->rev_C3) {
	    PRINT_ERR("Rev C3 card: can't master channel\n");
	    
	    RAISE_AALPod$BadMaster();
	}

	if(dir == AALPod_Dir_TX) {
	    PRINT_ERR("Can't master TX channel\n");

	    RAISE_AALPod$BadMaster();
	}

	if(mode == AALPod_Mode_AAL34) {
	    
	    /* This should be possible, but the card is broken */
	    PRINT_ERR("Can't master AAL3/4 RX channel\n");

	    RAISE_AALPod$BadMaster();
	}

	if(!st->allow_client_master) {
	    PRINT_ERR("Client RX master mode not permitted\n");

	    RAISE_AALPod$BadMaster();
	}

    }

    nicstar_open(st, dir, vpi, vci);

    /* Create connect state */
    cst = (Nicstar_AAL_Connect_st *)Heap$Malloc (Pvs(heap), sizeof (*cst));
    if (!cst) RAISE_AALPod$NoResources();    
    
    cst->vpi     = vpi;
    cst->vci     = vci;
    cst->master  = master;
    cst->mode    = mode;
    cst->dir     = dir;

    io = IO_BIND(offer);

    if(dir == AALPod_Dir_RX) {
	init_rxclient(st, cst, io);
    } else {
	init_txclient(st, cst, io);
    }
    
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
    Nicstar_st             *st = self->st;
    IO_cl                  *io = NARROW(server, IO_clp);
    Nicstar_AAL_Connect_st *cst;

    cst = (Nicstar_AAL_Connect_st *)IO_HANDLE(io);

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


/*
** IO threads.
*/
static void init_rxclient(Nicstar_st *st, Nicstar_AAL_Connect_st *cst, IO_clp io)
{
    nicstar_rxclient* rxclient;
    uint32_t status = 0;

    PRINT_NORM("st:%p VPI:%d VCI:%d IO:%p\n",
	       cst, (uint32_t)cst->vpi, (uint32_t)cst->vci, io);
    
    switch (cst->mode) {

    case AALPod_Mode_AAL0:
	status = RCT_AAL0;
	if(!st->rev_C3) status |= RCT_LGBUFS;
	break;

    case AALPod_Mode_AAL34:
	status = RCT_AAL34;
	break;

    case AALPod_Mode_AAL5:
    case AALPod_Mode_AAL5_Raw:
	status = RCT_AAL5;
	if (!st->rev_C3) status |= RCT_LGBUFS;
	break;

    default:
	RAISE_AALPod$BadMode();
	break;
    }

    nicstar_open_rx_connection(st, cst->vci, status);
    
    st->num_clients++;

    /* Record the RX client details */
    rxclient = nicstar_allocrxclient(st);
    rxclient->vci = cst->vci;
    rxclient->master = cst->master;
    rxclient->mode = cst->mode;
    rxclient->io = io;
    rxclient->bufs_allowed = IO$QueryDepth(io) / 2;
    rxclient->enabled = True;
    rxclient->definitely_connected = False;
    st->vci_map[cst->vci].rxclient = rxclient;

#if 0    
    ((IO_State *) (io->st))->binding = rxclient; 
#else
#warning no binding avail; does this matter?
#endif

    nicstar_enable_rx_connection(st, cst->vci);

#if 0
    while(True) {
	
	/* Client has started to fill empty channel */

	PRINT_NORM("Enabling RX connection on VCI %d\n", cst->vci);
	rxclient->enabled = True;
	nicstar_enable_rx_connection(st, cst->vci);

	/* Sleep until the driver disables the RX channel due to the client not keeping up */

	EC_AWAIT(rxclient->ec, ++rxclient->ev);

	PRINT_NORM("Client channel disabled - waiting for fresh packet\n");

	if(IO$GetPkt(io, 0, NULL, &value) != 1) {
	    ntsc_halt();
	}

    }
#endif
}
    
    




#define NICSTAR_NCELLS(x)  ((((x+47)/16)*683)/2048)
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
* COOL EH?
*/
static void init_txclient(Nicstar_st *st, Nicstar_AAL_Connect_st *cst, IO_clp io)
{
    unsigned vci = cst->vci;
    nicstar_txclient* txclient;
    
    printf("Nicstar_AAL5_TXThread: st:%p VPI:%d VCI:%d IO:%p\n",
		cst, (uint32_t)cst->vpi, (uint32_t)cst->vci, io);
    
    st->num_clients++;
    /* Record the TX client details */
    txclient = nicstar_alloctxclient(st);
    TRC(printf("TXCLIENT %x\n", txclient));
    txclient->io = io;
    txclient->ev = 0;
    txclient->mode = cst->mode;
    txclient->vci = vci;
    txclient->st = st;
    TRC(printf("Advance EC %lx by %x\n", 
		txclient->ec, NICSTAR_TXQLEN-1));
    EC_ADVANCE(txclient->ec, NICSTAR_TXQLEN-1);

    Threads$Fork(Pvs(thds), Nicstar_TX_Thread, txclient, 0);

}
    
void Nicstar_TX_Thread(nicstar_txclient *txclient) {

    uint32_t vci = txclient->vci;
    Nicstar_st *st = txclient->st;
    IO_clp io = txclient->io;
    uint32_t i;
    uint32_t NOCLOBBER rc;
    
    /* Nearly Minimalist loop */
    while(True) {
	word_t p = txclient->ev % NICSTAR_TXQLEN;
	struct rbuf *r;
	word_t len;
	
	/* Wait until TX is available for this client */
	EC_AWAIT(txclient->ec, txclient->ev);
	txclient->ev++;
	
	r = &txclient->rb[p];
	TRY {
	    
	    RGET(r, io, R_WAIT); 
	}
	CATCH_Thread$Alerted()
	    {
		printf("Nicstar: TX channel closed\n");
		nicstar_freetxclient(st, txclient);
		st->vci_map[vci].tx = 0;
		st->num_clients--;
		return;
	    }
	ENDTRY;

	for (len = 0, i = 0; i < r->r_nrecs; i++) len += r->r_recs[i].len;
	r->r_len  = len;
	r->r_type = RT_FULL;
	r->r_st = txclient;
	
	PRINT_TX("Got TX buffer %p (%p) to send\n", r, r->r_recs[0].base);
	

	switch (txclient->mode) {

	case AALPod_Mode_AAL5:
	    rc = nicstar_send_aal5(st, vci, r, False);
	    break;

	case AALPod_Mode_AAL5_Raw:
	    if (NICSTAR_NCELLS(len)*48 != len) {
		
		PRINT_ERR("Cell size %d invalid on vci %d\n", len, vci);
		rc = -EINVAL;
	    } else {
		rc = nicstar_send_aal5(st, vci, r, True);
	    }
	    break;

	case AALPod_Mode_AAL0:
	    if (len != 48) {
		PRINT_ERR("Cell size %d invalid on vci %d\n", len, vci);
		rc = -EINVAL;
	    } else {
		rc = nicstar_send_aal0(st, vci, r);
	    }
	    break;

	case AALPod_Mode_AAL34:
	    
	    if(NICSTAR_NCELLS(len)*48 != len) {
		PRINT_ERR("Cell size %d invalid on vci %d\n", len, vci);
		rc = -EINVAL;
	    } else {
		rc = nicstar_send_aal34(st, vci, r);
	    }
	    break;

	default:
	    PRINT_ERR("Bad AAL mode %d\n", txclient->mode);
	}

	if (rc) {
	    
	    PRINT_ERR("TX Error - ditching packet %p vci %d\n", 
		      r->r_recs->base, vci);
	    RPUT(r);
	    
	}
    }
}




