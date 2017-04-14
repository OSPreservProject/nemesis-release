/*
*****************************************************************************
**                                                                          *
**  Copyright 1993, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**	dev/pci/oppo      
** 
** FUNCTIONAL DESCRIPTION:
** 
** 	Otto state record.
**
** ENVIRONMENT: 
**
**      Internal to module Otto
** 
** ID : $Id: state.h 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
** LOG: $Log: state.h,v $
** LOG: Revision 1.3  1998/04/01 14:44:32  smh22
** LOG: use IDCCallback, not IONotify
** LOG:
** LOG: Revision 1.2  1997/06/27 14:55:46  and1000
** LOG: keep note of normal or w13 operation
** LOG:
** LOG: Revision 1.1  1997/04/07 15:31:52  prb12
** LOG: renamed Otto_st.h
** LOG:
** LOG: Revision 1.1  1997/02/14 14:38:24  nas1007
** LOG: Initial revision
** LOG:
** LOG: Revision 1.1  1997/02/12 13:05:04  nas1007
** LOG: Initial revision
** LOG:
 * Revision 1.1  1995/10/12  11:08:46  prb12
 * Initial revision
 *
 * Revision 1.1  1995/03/01  11:56:40  prb12
 * Initial revision
 *
**
*/

#ifndef _Otto_st_h_
#define _Otto_st_h_

#include <irq.h>

#include <AALPod.ih>
#include <IO.ih>
#include <IDCCallback.ih>

#include "otto_platform.h"
#include "otto_hw.h"

/*
 * Otto driver state
 */
typedef struct _Otto_st Otto_st;
struct _Otto_st {
    /*
     * Interrupt stub state
     */
    irq_stub_st      stub;
    Event_Count      event;	        /* Interrupt Event Count */
    Event_Val        ack;

    /*
     * WS Hardware regs 
     */
    volatile uint32_t     *tcir;        /* TURBOChannel Interrupt reg */
    volatile uint32_t     *tcimr;       /* TURBOChannel Interrupt Mask reg Rd*/
    volatile uint32_t     *tcimrw;      /* TURBOChannel Interrupt Mask reg Wr*/
    volatile uint32_t     *ssr;         /* System support reg */
    volatile uint32_t     *simr;        /* System interrupt mask reg */

    uint32_t tcir_mask;		/* Value used to mask TC Interrupt */

    uint32_t irq;           /* The 'irq' (i.e. TC opt slot)    */
    
    /* 
     * Otto Regs 
     */
    struct otto_hw        *otto_hw;

    /* 
     * Per client state
     */
    struct {			/* Per client state record */
	word_t    next;		/* Free list */
	dcb_ro_t *owner;		/* DCB of client */
	IO_clp    io;		/* RX IO channel */
	
	word_t  ring;		/* H/W ring allocated */
	
#define OTTO_NRXCLIENTS OTTO_NRINGS
    } rxclient[OTTO_NRXCLIENTS];
    word_t rxclientfreelist;
    
    struct {
	word_t  next;		/* Free list */
	
	dcb_ro_t *owner;		/* DCB of client */
	IO_clp  io;		/* TX IO channel */
	
	Event_Count ec;		/* Count packets TXed */
	Event_Val   ev;		/* Count packets enqueued */
	
#define OTTO_TXQLEN 16
	struct otto_txpkt *pkt[OTTO_TXQLEN];
	
#define OTTO_NTXCLIENTS OTTO_NRINGS
    } txclient[OTTO_NTXCLIENTS];
    word_t txclientfreelist;

    IDCCallback_cl iocb_cl;
    AALPod_cl      aalpod_cl;

    /* W13 mode support */
    bool_t	w13mode;	/* True if W13 mode is engaged */
    bool_t	normalmode;	/* True if normal AALPod clients around */
};

extern word_t otto_allocrxclient (Otto_st *st);
extern word_t otto_alloctxclient (Otto_st *st);
extern void otto_freetxclient (Otto_st *st, word_t index);
extern void otto_freerxclient (Otto_st *st, word_t index);

#endif /* _Otto_st_h_ */
