/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1994, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved. 					             *
 **                                                                          *
 *****************************************************************************
 **
 ** FILE:
 **
 **      sys/Domains/Plumber.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 ** 	Implementation of Plumber.if over {alpha,ix86}/dcb.h
 ** 
 ** ENVIRONMENT: 
 **
 **      Privileged domain
 ** 
 ** ID : $Id: Plumber.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <ntsc.h>
#include <dcb.h>

#include <Plumber.ih>

/* #define PLUMBER_TRACE */

#ifdef PLUMBER_TRACE
#define TRC(x) x
#else
#define TRC(x)
#endif

/*
 * Module stuff - 
 */

static	Plumber_Init_fn		Plumber_Init_m;
static	Plumber_Connect_fn	Plumber_Connect_m;
static	Plumber_Disconnect_fn	Plumber_Disconnect_m;

static  Plumber_op		ms = 
{
    Plumber_Init_m,
    Plumber_Connect_m,
    Plumber_Disconnect_m
};

static  const Plumber_cl		cl = { &ms, NULL };

CL_EXPORT (Plumber, Plumber, cl);	/* XXX - not in "Modules>" ?	*/

/*---------------------------------------------------- Entry Points ----*/

/* 
 * Init assumes "dom"'s "eps" and "num_eps" are valid, and
 * builds the "ep_free" list. It is assumed to have a lock on the dcb.
 */

static bool_t
Plumber_Init_m (Plumber_cl	*self,
		Plumber_DCB_RO	dom /* IN */)
{
    dcb_ro_t     *rop = (dcb_ro_t *) (dom);
    dcb_rw_t     *rwp = rop->dcbrw;
    ep_ro_t      *epro;
    ep_rw_t      *eprw;
    
    word_t 	i;

    TRC (eprintf ("Plumber$Init: dom=%lx\n", dom));
  
    if (! rop ||
	(((word_t) rop) & (FRAME_SIZE - 1)) || /* aligned wrong	*/
	! (epro = rop->epros ) || /* no endpoints		*/
	! (eprw = rop->eprws ) || /* no endpoints		*/
	! rop->num_eps ) {
			     TRC (eprintf ("Plumber$Init: bad dom\n"));
			     return False;
			 }

    for (i = 0; i < rop->num_eps; i++) {
	epro->peer_dcb = NULL;
	epro->val      = 0;
	epro->peer_ep  = NULL_EP;
	epro->type     = Channel_EPType_RX; /* XXX SMH: not setup for kevent */
	eprw->state    = Channel_State_Free;
	eprw->ack      = (Event_Val)(eprw + 1);
	(rop->ep_fifo)[i] = 0;
	
	epro++;
	eprw++;
    }
    (--eprw)->ack	= 0;
    rwp->free_eps = rop->eprws;
    EP_FIFO_RESET(rwp);

    TRC (eprintf ("Plumber$Init: dom=%lx: %x free endpoints\n", dom, i));
    return True;
}


static bool_t 
Plumber_Connect_m (Plumber_cl	*self,
		   Plumber_DCB_RO	from /* IN */,
		   Plumber_DCB_RO	to /* IN */,
		   Channel_TX      tx,
		   Channel_RX      rx )
{
    dcb_ro_t  *tx_dcb = (dcb_ro_t *) (from);
    dcb_ro_t  *rx_dcb   = (dcb_ro_t *) (to);
    ep_ro_t   *tx_ep_ro;
    ep_ro_t   *rx_ep_ro;
    ep_rw_t   *tx_ep_rw;
    ep_rw_t   *rx_ep_rw;
  
    TRC (eprintf ("Plumber$Connect: (%lx, %lx)->(%x, %x)\n",
		  tx_dcb->id, tx, rx_dcb->id, rx));

    /* check end points indices are valid */
    if ( tx > tx_dcb->num_eps ) return False;
    if ( rx > rx_dcb->num_eps ) return False;

    /* Get hold of data structures */
    tx_ep_ro = &(tx_dcb->epros)[ tx ];
    tx_ep_rw = &(tx_dcb->eprws)[ tx ];
    rx_ep_ro = &(rx_dcb->epros)[ rx ];
    rx_ep_rw = &(rx_dcb->eprws)[ rx ];
  
    /* check either end is not already connected */
    if ( rx_ep_ro->peer_dcb || tx_ep_ro->peer_dcb ) return False;

    TRC (eprintf ("Plumber$Connect: sane\n"));

    tx_ep_ro->peer_dcb = rx_dcb;
    tx_ep_ro->peer_ep  = rx;
    tx_ep_ro->val      = 0;
    tx_ep_ro->type     = Channel_EPType_TX;
	
    tx_ep_rw->ack      = 0;
    tx_ep_rw->state    = Channel_State_Connected;

    rx_ep_ro->peer_dcb = tx_dcb;
    rx_ep_ro->peer_ep  = tx;
    rx_ep_ro->val      = 0;
    rx_ep_ro->type     = Channel_EPType_RX;

    rx_ep_rw->ack      = 0;
    rx_ep_rw->state    = Channel_State_Connected;

    return True;
}


static bool_t
Plumber_Disconnect_m (Plumber_cl	*self,
		      Plumber_DCB_RO	dom /* IN */,
		      Channel_EP	ep /* IN */ )
{
    dcb_ro_t  *this_dcb = dom;
    dcb_ro_t  *peer_dcb;
    Channel_EP peer_ep;
    ep_ro_t   *th_ep_ro;
    ep_ro_t   *pe_ep_ro;
    ep_rw_t   *th_ep_rw;
    ep_rw_t   *pe_ep_rw;
    bool_t     result;

    TRC(eprintf("Plumber_Disconnect(dom=%p, ep=%x)\n", dom, ep));

    /* check end points index is valid */
    if ( ep > this_dcb->num_eps ) return False;

    /* Get hold of data structures */
    th_ep_ro = &(this_dcb->epros)[ ep ];
    th_ep_rw = &(this_dcb->eprws)[ ep ];

    /* Check ep is connected */
    if ( !(th_ep_ro->peer_dcb )) {
	result = False;
    } else {  
	/* From this point on we assume consistency */
	peer_dcb = th_ep_ro->peer_dcb;
	peer_ep  = th_ep_ro->peer_ep;
	
	pe_ep_ro = &(peer_dcb->epros)[ peer_ep ];
	pe_ep_rw = &(peer_dcb->eprws)[ peer_ep ];
	
	pe_ep_ro->peer_dcb = NULL;
	pe_ep_ro->peer_ep  = NULL_EP;
	pe_ep_ro->val      = 0;
	
	pe_ep_rw->ack      = 0;
	pe_ep_rw->state    = Channel_State_Dead;
	
	/* If we are closing down the tx side, kick the other side */
	if ( th_ep_ro->type == Channel_EPType_TX ) {
	    ntsc_kevent(peer_dcb, (Channel_RX) peer_ep, 0);
	}
	
	th_ep_ro->peer_dcb = NULL;
	th_ep_ro->peer_ep  = NULL_EP;
	th_ep_ro->val      = 0;
	
	th_ep_rw->ack      = 0;
	th_ep_rw->state    = Channel_State_Dead;
	result = True;
    }

    return result;
}

/*
 * End sys/Domains/Plumber.c
 */
