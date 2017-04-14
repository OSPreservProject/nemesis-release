/*
 * 3c509 driver state
 *
 * $Id: 3c509_st.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
 */

#ifndef el3_st_h
#define el3_st_h

#include <nemesis.h>
#include <Netcard.ih>
#include <SimpleIO.ih>
#include <IO.ih>
#include <Context.ih>
#include <Channel.ih>
#include <Event.ih>
#include <SRCThread.ih>
#include <Net.ih>

#include <irq.h>

typedef struct Netcard_st Netcard_st;
struct Netcard_st {
    /* Configuration information read at probe time */
    uint16_t		cfg_address;
    uint16_t		cfg_irq;
    Net_EtherAddr	cfg_hwaddr;
    uint16_t		cfg_media;

    /* Current configuration information */
    uint16_t		address;
    uint16_t		irq;
    Net_EtherAddr 	hwaddr;
    uint16_t		media;

    /* If active then packets can be sent and received, and configuration
       is locked */
    bool_t		active;

    /* buffer read from to pad packets to minimum length */
    char		*padbuf;
    /* buffer to Punt() packets up from */
    char		*rxbuf;

#if 0
    /* Statistics */
    Ethercard_Stats stats;
#endif /* 0 */

    /* Mutex controls access to card ports */
    mutex_t		mu;

    /* The context we export to the world */
    /* XXX do we want to keep this? */
    Context_clp		cx;

    /* DCB pointer and endpoint for interrupt stub */
    irq_stub_st		stub;
    Event_Count		event;
    Event_Val		ack;

    /* Condition used to signal free FIFO space to transmitter */
    SRCThread_Condition	txready;

    /* SimpleIO used to send rx'ed packets up on */
    SimpleIO_clp	rxio;

    /* SimpleIO used to send transmitted recs back on */
    SimpleIO_clp	txfree;
};

#endif /* el3_st_h */
