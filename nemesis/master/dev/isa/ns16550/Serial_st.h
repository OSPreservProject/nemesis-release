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
**      Serial state record.
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State for NS16550 driver interrupt stub.
** 
** ENVIRONMENT: 
**
**      Internal to program Serial
** 
** ID : $Id: Serial_st.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _Serial_st_h_
#define _Serial_st_h_

#ifdef SERIAL_STATS
#define STAT(_x) _x
#else
#define STAT(_x) 
#endif

#include <Serial.ih>
#include <Rd.ih>
#include <Wr.ih>
#include <IDCCallback.ih>

#include <irq.h>

#define SERIAL_BUFSIZE        8192    /* size of buffer for interrupt mode */

typedef enum {
    uart_type_none,
    uart_type_8250,
    uart_type_16450,
    uart_type_16550,
    uart_type_16550a
} uart_type;

/* NS16550 registers */

struct uart {
    uint32_t base;
    uint32_t irq;
    uchar_t  ier;
    uchar_t  lcr;
    uchar_t  mcr;
    uart_type type;
    bool_t   fifo;
};


/*
 * Serial interrupt stub and driver state
 */

typedef struct _Serial_st Serial_st;

/* FIFO buffer for serial IO */
typedef struct {
    volatile uchar_t       buf[SERIAL_BUFSIZE];
    volatile uint32_t      head;
    volatile uint32_t      tail;
    volatile uint32_t      free;
    uint32_t               size;
} buf_t;
#define BUF_IS_EMPTY(_buf) ((_buf)->head == (_buf)->tail)

/*
 * Structure for reader state. Both serial lines provide a reader.
 */
struct _Serial_Rd_st {
    SRCThread_Condition	cv;	/* Kicked by a put into the buffer. */
    SRCThread_Mutex	mu;	/* Lock on clients.		    */
    int32_t		ungetted;	/* one-char backup buffer   */
    int32_t		lastread;	/* used for ungetc	    */
    Domain_ID           current_client;
};

struct _Serial_Wr_st {
    SRCThread_Condition cv; /* Kicked when buffer becomes empty */
    SRCThread_Mutex mu;     /* Lock on clients */
};

struct _Serial_st {
    Context_cl           cx;
    Serial_cl            serial_cl;
    Rd_cl                rd_cl;
    Wr_cl                wr_cl;
    IDCOffer_clp         serial_offer;
    IDCOffer_clp         rd_offer;
    IDCOffer_clp         wr_offer;

    char                 name[16];    /* For messages */

    bool_t               halt;        /* Do we halt the machine on Ctrl-D? */

    /* RO State */
    irq_stub_st          stub;        /* Interrupt stub state              */
    
#ifdef SERIAL_STATS
    /* statistics */
    volatile uint32_t	 n_ints;      /* No. of interrupts 		   */
    volatile uint32_t 	 n_tx;        /* No. of chars transmitted	   */
    volatile uint32_t 	 n_rx;        /* No. of chars recieved	           */
    volatile uint32_t	 n_misc;      /* No. of misc. events		   */
#endif
    
    Event_Count          event;       /* Interrupt Event Count             */
    
    /* Hardware regs */
    struct uart          uart; 
    
    /* Protect R/W state */
    SRCThread_Mutex      mu;          /* Lock to access hardware or buffers */

    /* Serial input/output buffers */
    buf_t                txbuf;
    buf_t                rxbuf;

    /* Useful transmitter flag */
    bool_t               stalled;

    /* Reader structure */
    struct _Serial_Rd_st rd;
    struct _Serial_Wr_st wr;
};

/*
 * Externs
 */
extern void	SerialInitRd	 ( Serial_st *st );
extern void     SerialInitWr     ( Serial_st *st );

extern uchar_t  SerialGetcBuffer ( buf_t *buf );

extern void Serial_PutChars(Serial_st *st, uchar_t *s, uint32_t nc);

extern Rd_op rd_op;
extern Wr_op wr_op;
extern IDCCallback_op rd_cb_op;

#endif /* _Serial_st_h_ */
