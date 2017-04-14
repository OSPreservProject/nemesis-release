/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      net/Netif
** 
** FUNCTIONAL DESCRIPTION:
** 
**      A generic top half for packet-based network device drivers.
** 
** ENVIRONMENT: 
**
**      A device driver domain
**
*/


#include <nemesis.h>
#include <stdio.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <time.h>
#include <ecs.h>

#include <Netcard.ih>
#include <NetifMod.ih>
#include <SRCThread.ih>
#include <IOEntryMod.ih>
#include <QoSEntryMod.ih>
#include <QoSEntry.ih>
#include <IOTransport.ih>
#include <IDCCallback.ih>
#include <PF.ih>
#include <FragPool.ih>

#include <IOMacros.h>
#include <VPMacros.h>

#include <netorder.h>
#include <ether.h>
#include "pool.h"

#include <nettrace.h>

#undef NETIF_TRACE
#undef DEBUG

/* log "n" packets TXed for bandwidth calculations */
#define BANDWIDTH_LOG 8000
#undef BANDWIDTH_LOG

/* log "n" packets dropped */
#define DROP_LOG 60000
#undef DROP_LOG

/* Where to put the log */
#define NFS_SERVER "128.232.1.8"   /* madras.cl.cam.ac.uk */
#define MOUNT_DIR  "/tmp"
#define FILENAME   "netif-log"

/* Maximum number of bytes of header we're willing to handle (used to
 * allocated private pool of memory) */
#define MAX_HEADER_LEN 128

/* Default "laxity" parameter to use in SetQoS operations */
#define DEF_LAXITY 0

#ifdef NETIF_TRACE
#define TRC(x) printf x
#else
#define TRC(x) 
#endif

#ifdef TXQ_TRACE
#define TXQTRC(x) x
#else
#define TXQTRC(x)
#endif

/*#define TXQ_PARANOIA*/

#ifdef DEBUG
#define DB_TRC(_x) _x
#else
#define DB_TRC(_x)
#endif

#ifdef DROP_LOG
#include <Filesystem.ih>
#include <Directory.ih>
#endif /* DROP_LOG */

/* simulate lossy network */
//#define DROP_ONE_IN 128
#ifdef DROP_ONE_IN
#warning DROP_ONE_IN defined - lossy network simulation engaged!
#endif

#define ETRC(_x) 

/* compare the six bytes at "a" with those at "b" */
/* "a" is aligned to 2 bytes before a word,
   "b" is aligned to a word. */
#define EQ6(a, b) \
({						\
    char *_a = (a);				\
    char *_b = (b);				\
    ((_a[0] == _b[0]) &&			\
     (_a[1] == _b[1]) &&			\
     (_a[2] == _b[2]) &&			\
     (_a[3] == _b[3]) &&			\
     (_a[4] == _b[4]) &&			\
     (_a[5] == _b[5]));				\
})


/* Module stuff ------------------------------------------- */

static NetifMod_New_fn New_m;
static Netif_Up_fn Up_m;
static Netif_Down_fn Down_m;
static Netif_AddReceiver_fn AddReceiver_m;
static Netif_RemoveReceiver_fn RemoveReceiver_m;
static Netif_AddTransmitter_fn AddTransmitter_m;
static Netif_RemoveTransmitter_fn RemoveTransmitter_m;
static Netif_SetRxFilter_fn SetRxFilter_m;
static Netif_SetTxFilter_fn SetTxFilter_m;
static Netif_Inject_fn Inject_m;
static Netif_SetQoS_fn SetQoS_m;

static NetifMod_op m_op = { New_m };
static const NetifMod_cl m_cl = { &m_op, (addr_t)0xdeadbeef };

CL_EXPORT(NetifMod,NetifMod,m_cl);

static Netif_op n_op = { 
    Up_m, 
    Down_m, 
    AddReceiver_m, 
    RemoveReceiver_m, 
    AddTransmitter_m, 
    RemoveTransmitter_m,
    SetRxFilter_m,
    SetTxFilter_m,
    Inject_m,
    SetQoS_m,
};

static void TXThread(addr_t data); /* Transmit packets */

static SimpleIO_Punt_fn receive_packet; 
static SimpleIO_op rx_ms = {
    receive_packet, 
};


static SimpleIO_Punt_fn txfree_packet;
static SimpleIO_op txfree_ms = {
    txfree_packet, 
};

/*
 * IDCCallback methods 
 */
static	IDCCallback_Request_fn 		RXIO_Request_m, TXIO_Request_m;
static	IDCCallback_Bound_fn 		RXIO_Bound_m, TXIO_Bound_m;
static	IDCCallback_Closed_fn 		RXIO_Closed_m, TXIO_Closed_m;

static	IDCCallback_op	rxiocb_ms = {
    RXIO_Request_m,
    RXIO_Bound_m,
    RXIO_Closed_m
};

static	IDCCallback_op	txiocb_ms = {
    TXIO_Request_m,
    TXIO_Bound_m,
    TXIO_Closed_m
};

/* State -------------------------------------------------- */


/* State for receivers */
struct rx_st {
    IO_cl       *io;
    void        *base;           /* base of valid data area  */
    void        *limit;          /* limit of valid data area */
};

/* State for transmitters */
struct tx_st {
    PF_clp	 filter;
    Netif_TXSort sort; 
    IO_cl       *io;
    void        *base;           /* base of valid data area  */
    void        *limit;          /* limit of valid data area */
    FragPool_key key;		 /* head frags set it, body frags must match */
    int		 outstanding;	 /* num *slots* down in card driver */
};

#ifdef BANDWIDTH_LOG
typedef struct {
    uint32_t	tstamp;		/* when */
    uint32_t	nbytes;		/* how much */
    IO_clp	io;		/* for who */
} logitem_t;
#endif /* BANDWIDTH_LOG */

#ifdef DROP_LOG
typedef struct {
/*    Time_T	tnow;*/
    uint32_t	tnow;
    uint32_t	nbytes;
    uint32_t	io;		/* who wasn't keeping up, or NULL */
} droplog_t;
#endif /* DROP_LOG */

/* a couple of data types for the linked list of pending qos set
 * operations */
typedef struct _qositem {
    struct _qositem *next;
    struct _qositem *prev;
} qositem_t;

typedef struct _qosdata {
    struct _qosdata *next;
    struct _qosdata *prev;
    void	    *iohandle;
    Netif_TXQoS	     qos;
} qosdata_t;

/* description of something sent to the device driver for asynchronous
 * transmit */
typedef struct {
    IO_Rec	hrec;	/* header description */
    IO_clp	io;	/* where the data came from */
    word_t	value;	/* user-supplied data value */
} tx_qitem;



typedef struct Netif_st Netif_st;
struct Netif_st {
    Heap_clp heap;
    Netcard_clp card;
    uint32_t mtu;
    Net_EtherAddrP mac;

    /* Receiver things: */
    /* The receive packet filter; maps packets to rx_st's */
    PF_clp	rx_filter;
    FragPool_clp pool;		/* delay the processing of fragments */

    /* Number of bytes in packet the above filter requires */
    uint32_t	snaplen;

    /* Packet buffer for receiver */
    uint8_t	*rxbuff;

    /* Entry for receiver */
    IOEntry_clp rx;
    SimpleIO_cl rxio; /* thing we receive on */

    /* Transmitter things: */
    QoSEntry_clp tx;
    SimpleIO_clp txio; /* thing to send on */
    SimpleIO_cl	txfree; /* thing we get "tx sent" messages back on */

    /* Callbacks on rx and tx io offers respectively */
    IDCCallback_cl rxiocb_cl;
    IDCCallback_cl txiocb_cl;
    
    /* mapping of IO -> QoS setting for use when someone binds to
     * the Io in question */
    qositem_t	qos;

    /* number of nanoseconds to charge per byte of packet sent */
    uint32_t	costPerByte;

    /* transmit queue */
    Event_Count	txqec;		/* advanced for each freed txq item */
    Event_Val	txqack;		/* advanced for each used txq item */
    uint32_t	txqsz;		/* max size of tx Q. */
    tx_qitem	**txring;	/* base of the ring of free Q desc */
    int		outstanding;	/* total pkts down in card-specific driver */

    /* stats */
    uint32_t	rxpackets;	/* number of packets received */
    uint32_t	txpackets;	/* number of packets transmitted */

#ifdef TXQ_PARANOIA
    uint32_t	txqitems;	/* current size of tx Q */
    uint32_t	txqmax;		/* largest length of tx Q recently */
    uint32_t	txqfree;	/* slotno of next expected free */
    int qlen[256];
    int xxpos;
#endif

    /* debugging stuff */
#ifdef BANDWIDTH_LOG
    logitem_t	log[BANDWIDTH_LOG];	/* large array of log entries */
    uint32_t	logsize;		/* next free log slot */
#endif /* BANDWIDTH_LOG */
#ifdef DROP_LOG
    droplog_t	drops[DROP_LOG];
    uint32_t	dropsize;
    bool_t	drop_xfer;		/* True when log send in progress */
#endif /* DROP_LOG */
};


#ifdef DROP_LOG
static void drop_xfer_thread(Netif_st *st);
#endif /* DROP_LOG */

#include "pool.h"

static bool_t sanity_check_rx(struct rx_st *rxst, IO_Rec *recs, uint32_t nrecs)
{
    int i; 

    /* On receive, we want all recs to decribe memory within 
       our pre-negotiated data area. */
    for(i = 0; i < nrecs; i++) {
	if( recs[i].base < rxst->base || 
	    (recs[i].base + recs[i].len) > rxst->limit) {
	    eprintf("netifRX: invalid IO_Rec[%d]: base %p limit %p\n", 
		    i, recs[i].base, recs[i].base + recs[i].len);
	    eprintf("but our data area is [%p, %p)\n", rxst->base, 
		    rxst->limit);
	    return False;
	}
    }
    return True;
}	


static bool_t sanity_check_tx(struct tx_st *txst, IO_Rec *recs, uint32_t nrecs)
{
    int i; 

    if(txst->sort == Netif_TXSort_Native) {
	/* We're in native mode => want all recs to decribe memory within 
	   our pre-negotiated data area. */
	for(i = 0; i < nrecs; i++) {
	    if( recs[i].base < txst->base || 
		(recs[i].base + recs[i].len) > txst->limit) {
		eprintf("netifTX: invalid IO_Rec[%d]: base %p limit %p\n", 
			i, recs[i].base, recs[i].base + recs[i].len);
		eprintf("but our data area is [%p, %p)\n", txst->base, 
			txst->limit);
		return False;
	    }
	}
	return True;
    } 

    if(txst->sort == Netif_TXSort_Socket) {
	/* We're in socket mode => we want exactly 2 recs, with the
	   first describing memory within our pre-negotiated data
	   area, and the last one describing some nailed, contiguous
	   and readable memory. */
	if(nrecs != 2)
	{
	    eprintf("netif: sanity_check_tx failed: nrecs=%d, expecting 2\n",
		    nrecs);
	    return False;
	}

	if( recs[0].base < txst->base || 
	    (recs[0].base + recs[0].len) > txst->limit) {
	    eprintf("netifTX: invalid IO_Rec[0]: base %p limit %p\n", 
		    recs[0].base, recs[0].base + recs[0].len);
	    eprintf("but our data area is [%p, %p)\n", txst->base, 
		    txst->limit);
	    return False;
	}

	/* XXX SMH: now check that rec[1] describes some happy memory */

	return True;
    }

    /* Urk! Got some unsupported tx sort; not happy */
    eprintf("netif: unknown tx sort %d\n", txst->sort);
    return False; 
}	



/* copy data to client */
static void to_client(void *netif,
		      PF_Handle rx, uint32_t nrecs, IO_Rec *recs,
		      uint32_t hdrsz);


/* new ring ones */
static INLINE int TXQ_ALLOC(Netif_st *st)
{
    int t;
    Event_Val ev;
#ifdef TXQ_PARANOIA
    bool_t dumped = False;
#endif
    
    TXQTRC(printf("TXQ_ALLOC entered\n"));

    /* wait till resources become available */
    do {
	ev = EC_AWAIT_UNTIL(st->txqec, st->txqack, NOW() + SECONDS(1));
	if (EC_LT(ev, st->txqack))
	{
#ifdef TXQ_PARANOIA
	    int i;
#endif
	    /* This shouldn't really happen: it indicates trying to
	     * transmit harder than the card can keep up PLUS the card
	     * hasn't responded for 1 second.  Bad news, really. */

	    printf("TXQ_ALLOC: timeout getting transmit descriptor\n");

#ifdef TXQ_PARANOIA
	    if (!dumped)
	    {
		printf("(dumping last 256 TXQ operations)\n");
		for(i=0; i < 256; i++)
		    printf("%d%c\n", st->qlen[i], (i==st->xxpos)?'*' : ' ');
		dumped = True;
	    }
#endif
	}
    } while(EC_LT(ev, st->txqack));

    Threads$EnterCS(Pvs(thds), False);
    
    t = (uint32_t)(st->txqack) % (st->txqsz);  /* slot to use */
    st->txqack++;

#ifdef TXQ_PARANOIA
    st->txqitems++;
    st->qlen[st->xxpos] = st->txqitems;
    st->xxpos++;
    st->xxpos &= 0xff;
    st->txqmax = MAX(st->txqmax, st->txqitems);
#endif

    Threads$LeaveCS(Pvs(thds));

    TXQTRC(printf("TXQ_ALLOC: using %d, next is %d/%d\n",
		  t, st->txqack, st->txqsz));

    return t;
}


static INLINE void TXQ_FREE(Netif_st *st, int d)
{

    TXQTRC(printf("TXQ_FREE for %d\n", d));

    Threads$EnterCS(Pvs(thds), False);
#ifdef TXQ_PARANOIA
    if (d != st->txqfree)
    {
	int i;
	printf("TXQ_FREE: out of order free! (free for %d, expecting %d)\n",
	       d, st->txqfree);
	printf("(dumping last 256 TXQ operations)\n");
	for(i=0; i < 256; i++)
	    printf("%d%c\n", st->qlen[i], (i==st->xxpos)?'*' : ' ');
	
    }

    st->txqitems--;

    st->qlen[st->xxpos] = st->txqitems;
    st->xxpos++;
    st->xxpos &= 0xff;

    st->txqfree = (st->txqfree + 1) % (st->txqsz);
#endif

    /* bump the event count to allow others in */
    EC_ADVANCE(st->txqec, 1);

    Threads$LeaveCS(Pvs(thds));
}




/* Server threads --------------------------------------------- */


/* entry from device driver on pkt rx */
static void receive_packet(SimpleIO_cl *self, uint32_t nrecs, 
			   IO_Rec *recs, word_t value, addr_t owner)
{
    Netif_st *st= self->st;
    struct rx_st *rxst = NULL;
    bool_t frag;
    uint32_t hdrsz;

    /*
     * stage 1: the demux
     */

    /* Try the packet filter on the packet */
    Threads$EnterCS(Pvs(thds), False);
    if (st->rx_filter)
	rxst = (struct rx_st *)PF$Apply(st->rx_filter, recs, nrecs,
				      &frag, &hdrsz);
    Threads$LeaveCS(Pvs(thds));

#ifdef LATENCY_TRACE
    if (recs[0].len == 1022)
	STAMP_PKT_UNA(recs[0].base, NETIF_AFTER_RXPF);
#endif /* LATENCY_TRACE */

    /* are we asked to postpone this packet? */
    if (frag)
    {
	FragPool$PutPkt(st->pool, nrecs, recs, (word_t)rxst, hdrsz);
	/* This may have achieved one of three things:
           	a) queued the packet for later
		b) been a frag head, so caused a backlog to clear to_client()
		c) been sent to_client(), if we've seen the head already
	   In all cases, we're through processing, so */
	return;
    }

    /*
     * stage 2: send it to the client
     */
    
    /* was anyone actually listening? */
    if (!rxst)
	return;

    /* ok, so send it */
    to_client(st, (PF_Handle)rxst, nrecs, recs, hdrsz);
}



/* copy the data described by "nrecs" and "recs" to the client "rxhdl" */
static void to_client(void *netifst, PF_Handle rxhdl, 
		      uint32_t nrecs, IO_Rec *recs, uint32_t hdrsz)
{
    Netif_st *st = netifst;
    struct rx_st *rxst = (void*)rxhdl;
    IO_Rec rxrec[10]; /* XXX shouldn't be hardwired */
    uint32_t cli_nrecs;
    word_t value;
    uint32_t cli_currec, cli_curremain;
    int i, cnt, plen;
    char *pptr;
    char *cli_curbase;
    bool_t ok;

#ifdef DROP_ONE_IN
    /* simulate lossy network by ditching the packet */
    if ((st->rxpackets % DROP_ONE_IN) == (DROP_ONE_IN-1))
    {
	eprintf("~");
	st->rxpackets++;	/* need this otherwise keep dropping! */
	return;
    }
#endif

    if (rxst->io && IO$GetPkt(rxst->io, 10 /* XXX bad constant */, rxrec, 
			    0, &cli_nrecs, &value))
    {

#if 1
	/* Sanity check that all recs describe mem with our data area */
	if(!sanity_check_rx(rxst, &rxrec[0], cli_nrecs)) 
	    /* XXX Should we punt the recs back up here? 
	       I think *probably* although client will get confused */
	    return;
#endif

#ifdef DEBUG
	printf("to_client: io=%p rxrec=%p, nrecs=%x, value=%x\n",
	       rxst->io, rxrec, cli_nrecs, value);
	for(i=0; i < cli_nrecs; i++)
	    printf("\tRec %d: base=%p, len=%d\n", i, rxrec[i].base, rxrec[i].len);
#endif	

	/* iterate over the received recs, scattering them into the
	 * client's recs */

	/* start with the client's first rec */
	cli_currec = 0;			/* rec in use */
	cli_curremain = rxrec[0].len;	/* remaining room in rec */
	cli_curbase = rxrec[0].base;	/* current start of rec */
	rxrec[0].len = 0;		/* current size of rec */

	ok = True; 

	/* iterate over device driver recs */
	for(i=0; i<nrecs; i++)
	{
	    pptr = (char *)recs[i].base;
	    plen = recs[i].len;

	    /* if this is the header, then maybe we want to truncate the
	     * header rec so the payload lands in the right place. */
	    if (i == 0)
		cli_curremain = MIN(cli_curremain, hdrsz);

	    /* scatter rx'ed fragment into client's recs */
	    while(plen > 0 && cli_curremain > 0)
	    {
		cnt = MIN(cli_curremain, plen);

		memcpy(cli_curbase, pptr, cnt);

		plen		-= cnt;
		pptr		+= cnt;
		cli_curremain	-= cnt;
		rxrec[cli_currec].len += cnt;
		cli_curbase	+= cnt;

		/* if we've exhausted this client rec, go on to the
                 * next one */
		if (cli_curremain == 0)
		{
		    cli_currec++;
		    if (cli_currec < cli_nrecs) /* still have client recs */
		    {
			cli_curremain = rxrec[cli_currec].len;
			cli_curbase   = rxrec[cli_currec].base;
			rxrec[cli_currec].len = 0;
		    }
		}
	    } /* end while */

	    if (plen) /* something went wrong - bits of packet left */
	    {
		int i;
		int tot;

		printf("to_client: packet too long for client IORecs D[");

		tot = 0;
		for(i=0; i<nrecs; i++)
		{
		    printf("%s%d", i?" ":"", recs[i].len);
		    tot += recs[i].len;
		}

		printf(":%d] C[", tot);
		tot = 0;

		for(i=0; i<cli_nrecs; i++)
		{
		    printf("%s%d", i?" ":"", rxrec[i].len);
		    tot += rxrec[i].len;
		}

		printf(":%d]\n", tot);
		ok = False;
		/* XXX if there's a pktcx, maybe could recover? */
		break;
	    }
	}

	/* make any remaining client recs zero length */
	for(cli_currec++; cli_currec < cli_nrecs; cli_currec++)
	    rxrec[cli_currec].len = 0;
	
	if (ok)
	{
	    st->rxpackets++;
	    IO$PutPkt(rxst->io, cli_nrecs, (IO_Rec *)rxrec, value, 0);
	}
	else
	{
	    printf("Urmk! Not returning the recs. Damn.\n");
	}
	
    } else {
	/* There's nobody listening; drop the packet */
//	eprintf("!");
#ifdef DROP_LOG
	if (st->dropsize == DROP_LOG)
	{
	    if (!st->drop_xfer)
	    {
		st->drop_xfer = True;  /* don't want to log any more */
		printf("Netif: drop log full\n");
/*		Threads$Fork(Pvs(thds), drop_xfer_thread, st, 0);*/
	    }
	}
	else
	{
	    uint32_t i = st->dropsize++;
/*	    st->drops[i].tnow   = NOW();*/
	    st->drops[i].tnow   = rpcc();
	    st->drops[i].nbytes = recs[0].len;
	    st->drops[i].io     = rxst->io;
	}
#endif /* DROP_LOG */
    }
}




static void RXConThread(addr_t data)
{
    Netif_st *st = data;
    struct rx_st *rxst;
    IO_clp io;

    /* Manages receiver connection state */
    while (1) {
	io=IOEntry$Rendezvous(st->rx, FOREVER);

	Threads$EnterCS(Pvs(thds), False);
	rxst     = (struct rx_st *)IO_HANDLE(io);
	rxst->io = io;
	Threads$LeaveCS(Pvs(thds));

	/* We don't actually do anything to the IO here; we just save it
	   in the appropriate place for RXThread to pick up. */
    }
}

#ifdef TXQ_PARANOIA
static void stat_thd(Netif_st *st)
{
    int max, spot;

    while(1)
    {
	PAUSE(SECONDS(1));
	max  = (volatile int)(st->txqmax);
	spot = (volatile int)(st->txqitems);
	st->txqmax = 0;
	printf("-- %d (%d)\n", spot, max);
    }
}
#endif

/* This thread services all the transmitters */
static void TXThread(addr_t data)
{
    Netif_st *st = data;
    struct tx_st *txst;
    IO_clp io;
    IO_Rec recs[10]; /* XXX maybe not on stack */
    int txqno;
    tx_qitem *txq;
    uint32_t nrecs;
    word_t value;
    uint32_t i, length;
    bool_t frag;
    uint32_t hdrsz;

#ifdef TXQ_PARANOIA
    Threads$Fork(Pvs(thds), stat_thd, st, 0);
#endif

    Threads$EnterCS(Pvs(thds), False);

    while (True) {

	Threads$LeaveCS(Pvs(thds));

	/* Grab some memory for this header, and a descriptor to keep
         * track of which packets for which clients are down in the
         * driver.  May block waiting for a descriptor to be
         * returned. This ensures that we can actually handle any
         * packet that Rendezvous might return, ie, the DMA ring will
         * have an entry for us. */
	txqno = TXQ_ALLOC(st);
	txq = st->txring[txqno];

#if 0
	if (st->outstanding != 0)
	    eprintf("netif: alloc, but outstanding=%d\n",
		    st->outstanding);
#endif

	/* Now block waiting for work from a client */
	io = IOEntry$Rendezvous((IOEntry_clp)st->tx, FOREVER);

	Threads$EnterCS(Pvs(thds), False);

	/* Find out whose IO it is */
	txst = (struct tx_st *)IO_HANDLE(io);

	/* make sure we have enough slots in the return FIFO for both
	 * this transaction, plus all the ones we have
	 * outstanding. Need to count FIFO-added headers, so is really 4 */
	/* XXX AND: this assumes we're about to get 3 slots worth of
	 * data in the IO$GetPkt() shortly. */
	if(!IO$AwaitFreeSlots(io, txst->outstanding + 4, 0))
	{
	    printf("netif: suspending %p (%d,%d)\n",
		   io, txst->outstanding, st->outstanding);
	    IOEntry$Suspend((IOEntry_clp)st->tx, io);
	    TXQ_FREE(st, txqno);
	    continue; 
	}

	ETRC(printf("Netif$TXThread: back from Rdzvs, io at %p\n", io));

	/* get packet from client */
	IO$GetPkt(io, 10, recs, FOREVER, &nrecs, &value);
	if (nrecs < 1)
	{
	    printf("netif: TXThread: no data?!?\n");
	    TXQ_FREE(st, txqno);
	    continue;
	}

	/* if we get more than 3 recs, then the outstanding count for
         * this channel will be a bit screwed. */
	if (nrecs > 3)
	    printf("netif: got %d recs, expecting <4\n", nrecs);

	/* Sanity check that the recs meet our TX conditions */
	if(!sanity_check_tx(txst, recs, nrecs)) {
	    /* XXX punt recs back up? probably should... */
	    TXQ_FREE(st, txqno);
	    continue;
	}

	/* Check the header buffer we picked off the txq stack is
         * large enough for this particular client's header.  If not,
         * we're in deep doo doo. */
	if (txq->hrec.len < recs[0].len)
	{
	    printf("netif: TXThread: private header buf too small (%d<%d)\n",
		   txq->hrec.len, recs[0].len);
	    TXQ_FREE(st, txqno);
	    continue;
	}


	{
	    addr_t tmp;

	    /* grab our private memory */
	    tmp = txq->hrec.base;

	    /* save the client's header details... */
	    txq->hrec = recs[0];
	    /* ...and swap in our header memory */
	    recs[0].base = tmp;
	    txq->value = value;		/* save the user data */
	}

	length=0;
	for (i=0; i<nrecs; i++)
	    length+=recs[i].len;

#ifdef LATENCY_TRACE
	if (length == 1022)
	{
	    ENTER_KERNEL_CRITICAL_SECTION();
	    STAMP_PKT_USER(recs[1].base, NETIF_FROMCLIENT);
	    LEAVE_KERNEL_CRITICAL_SECTION();
	}
#endif /* LATENCY_TRACE */

	/* Account the time we're about to spend processing this packet
	 * to the QosEntry */
	QoSEntry$Charge(st->tx, io, (int64_t)length * st->costPerByte);

	/* hack to get the numbers out */
#ifdef BANDWIDTH_LOG
	if (st->logsize == BANDWIDTH_LOG)
	{
	    int i;
	    printf("=========== LOG DUMP BEGINS ================\n");
	    for(i=0; i<BANDWIDTH_LOG; i++)
	    {
		printf("%08x: %d %p\n",
		       st->log[i].tstamp,
		       st->log[i].nbytes,
		       st->log[i].io);
		PAUSE(MILLISECS(30));
	    }
	    st->logsize=0;
	    printf("=========== SCHEDULER DUMP BEGINS ================\n");
	    QoSEntry$dbg(st->tx);
	    ntsc_halt();  /* XXX for the moment */
	}

	{
	    uint32_t i = st->logsize++;
	    st->log[i].tstamp = rpcc();
	    st->log[i].nbytes = length;
	    st->log[i].io = io;
	}
#endif /* BANDWIDTH_LOG */

#define TX_FILTER	
#ifdef TX_FILTER

	/* Filter and send */
	/* (don't filter whole packet, just first fragment. PF must
         * disallow if it can't access stuff it needs) */

	{
	    bool_t pf_happy, fragpool_happy;

	    /* must have filter installed */
	    if (!txst->filter)
		goto txfail;

	    /* and filter must allow it */
	    frag = False;
	    pf_happy = PF$CopyApply(txst->filter, &txq->hrec, 1,
				    recs[0].base, &frag, &hdrsz);
	    if (!pf_happy)
		goto txfail;

	    /* if it was a frag, FragPool has to approve packet too */
	    if (frag)
	    {
		fragpool_happy = FragPool$TXFilt(st->pool, &txst->key,
						 recs[0].base, recs[0].len);
		if (!fragpool_happy)
		    goto txfail;
	    }


	    /* Ok, commit! */
	    txst->outstanding += nrecs+1;

#ifdef LATENCY_TRACE
	    if (length == 1022)
	    {
		ENTER_KERNEL_CRITICAL_SECTION();
		STAMP_PKT_USER(recs[1].base, NETIF_AFTER_TXPF);
		LEAVE_KERNEL_CRITICAL_SECTION();
	    }
#endif /* LATENCY_TRACE */

	    /* is this packet actually just for us? */
	    if (!EQ6((char*)recs[0].base, (char*)st->mac))
	    {
		/* if not, put it on the wire */
		ETRC(printf("Netif$TXThread: Punting packet.\n"));
		txq->io = io;
//		eprintf("v%p\n", txq->hrec.base);
		st->outstanding++;
		SimpleIO$Punt(st->txio, nrecs, recs, value, (void*)txqno);
	    }
	    else
	    {
		/* otherwise short circuit it right back up the RX side */
		/*printf("packet for me!\n");*/
		/* XXX tx q gets out of order! */
		st->outstanding++;
		SimpleIO$Punt(&st->rxio, nrecs, recs, value, 0);
		txq->io = io;
		txfree_packet(&st->txfree, nrecs,
			      recs, value, (void*)txqno);
	    }

	    /* back to the QoSEntry$Rendezvous() */
	    continue;

	    /* error "recovery" code: */
	txfail:
	    printf("Netif: rejected tx packet");
	    if (!txst->filter)
		printf(" (no filter set)\n");
	    else if (!pf_happy)
		printf(" (filter failed)\n");
	    else if (!fragpool_happy)
		printf(" (FragPool IP id mismatch)\n");
	    else
		printf(" (<internal error>)\n");

	    st->outstanding++;
	    txq->io = io;
	    txfree_packet(&st->txfree, nrecs, recs, value, (void*)txqno);
	}
#else
	/* always successful. Don't do this at home kiddies! */
#warning NO TRANSMIT FILTERING IN USE
	/* finally copy across the client's header */
	memcpy(recs[0].base, txq->header, txq->hlen);
	    
	txq->io = io;
	SimpleIO$Punt(st->txio, nrecs, recs, value, txqno);
#endif


    } /* while true */

    printf("Netif$TXThread: BAD NEWS. Exiting (?)\n");
}


/* This needs to be called with mutual exclusion against the TX thread */
static void txfree_packet(SimpleIO_cl *self, uint32_t nrecs, 
			  IO_Rec *recs, word_t value, addr_t owner)
{
    Netif_st	*st=self->st;
    tx_qitem	*txq = st->txring[(uint32_t)owner];
    struct tx_st *txst;
    IO_clp	io;
    addr_t	tmp;

    if (nrecs < 1)
    {
	printf("netif: txfree: no data?!?\n");
	return;
    }

    st->outstanding--;

#if 0
    if (st->outstanding != 0)
	eprintf("outstanding=%d\n", st->outstanding);
#endif

    /* account a successful transmission */
    st->txpackets++;

    /* note who this is to go back to */
    io = txq->io;
    txst = (struct tx_st *)IO_HANDLE(io); 

    /* swap client's rec[0] back in */
    tmp = recs[0].base;
    recs[0] = txq->hrec;

    /* don't use the supplied value, restore the one we saved */
    value = txq->value;

    /* Move our memory back on free list */
    txq->hrec.base = tmp;
    txq->hrec.len = MAX_HEADER_LEN;  /* XXX nasty const! */
    txq->io = NULL;

    /* make the descriptor available */
    TXQ_FREE(st, (uint32_t)owner);

    /* Return the IO_Rec vector */
    ETRC(printf("Netif$TXThread: returning IOs.\n"));

    if (!IO$PutPkt(io, nrecs, recs, value, 0))
    {
	eprintf("Netif: txfree_packet: client FIFO full, dropping packet\n");
    }
    else
    {
	/* once the PutPkt has succeeded, we no longer need to keep
         * track of the slots outstanding, since the FIFO will do it
         * for us. */
	txst->outstanding -= nrecs+1;
    }
}


/* Control routines ---------------------------------------- */

static bool_t Up_m(Netif_cl *self)
{
    Netif_st *st=self->st;

    return Netcard$Up(st->card);
}

static bool_t Down_m(Netif_cl *self)
{
    Netif_st *st=self->st;

    return Netcard$Down(st->card);
}


static IOOffer_clp AddReceiver_m(Netif_cl *self, /* OUT */ PF_Handle *handle)
{
    Netif_st *st=self->st;
    IOTransport_clp iot;
    IOOffer_clp offer;
    IDCService_clp service;
    IOData_Shm *shm; 
    struct rx_st *rx;

    TRC(("Netif: AddReceiver called\n"));

    if(!(rx = Heap$Malloc(st->heap, sizeof(*rx))))
	RAISE_Heap$NoMemory();

    rx->io = NULL;

    /* 
    ** Create the IOOffer; the 'protocol' for receive IO's is to 
    ** have 3 recs per transaction: 
    **   rec[0]   ==  header
    **   rec[1]   ==  user data (payload)
    **   rec[2]   ==  trailer   (typically empty)
    ** We want the client to allocate memory for these in advance. 
    ** For a depth of 128 == 32 packets of 3 recs + 1 rec header (added 
    ** by the IO code), we ask for a minimum of 64K memory (which allows 
    ** up to 2K per transaction).
    */
#define RXIO_DEPTH 128
#define RXIO_SHMSZ 65536

    iot   = NAME_FIND("modules>IOTransport", IOTransport_clp);
    
    shm   = SEQ_NEW(IOData_Shm, 1, st->heap); 
    SEQ_ELEM(shm, 0).buf.base     = NO_ADDRESS; /* not yet allocated */
    SEQ_ELEM(shm, 0).buf.len      = RXIO_SHMSZ;
    SEQ_ELEM(shm, 0).param.attrs  = 0;
    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;
    
    offer = IOTransport$Offer(iot, st->heap, RXIO_DEPTH, IO_Mode_Rx, 
			      IO_Kind_Master, shm, Pvs(gkpr), &st->rxiocb_cl, 
			      st->rx, (word_t)rx, &service);
    
    

    TRC(("Netif: AddReceiver: done\n"));
    *handle = (PF_Handle)rx;
    return offer;
}

static bool_t RemoveReceiver_m(Netif_cl *self, PF_Handle handle)
{
    /* XXX not yet implemented */
    return False;
}


/* Callbacks for IOOffers created by AddReceiver */
static bool_t RXIO_Request_m (IDCCallback_cl *self, IDCOffer_clp offer,
			     Domain_ID dom, ProtectionDomain_ID pdom,
			     const Binder_Cookies *clt_cks, 
			     Binder_Cookies *svr_cks)
{
    Binder_Cookie  shm_desc;
    uint32_t vpn, npages; 

    
    /* sanity check the data area */
    if (SEQ_LEN (clt_cks) != 2) return False;
    shm_desc = SEQ_ELEM (clt_cks, 1);
    TRC(eprintf("RXIO$Request: client's given me buf at %p, length %x\n", 
		shm_desc.a, shm_desc.w)); 

    if(shm_desc.w < RXIO_SHMSZ) {
	eprintf("RXIO$Request: insufficient shared memory.\n");
	return False;
    }
    
    vpn    = (word_t)shm_desc.a >> PAGE_WIDTH; 
    npages = (word_t)shm_desc.w >> PAGE_WIDTH; 
	
    /* XXX check translations of region and see if can nail */

    return True;
}

static bool_t RXIO_Bound_m (IDCCallback_cl *self, IDCOffer_clp offer,
			    IDCServerBinding_clp binding, Domain_ID dom,
			    ProtectionDomain_ID pdom, 
			    const Binder_Cookies *clt_cks, 
			    Type_Any *server)    
{
    Netif_st     *st = self->st;
    IO_cl        *io = NARROW(server, IO_clp);
    struct rx_st *handle;

    handle = (struct rx_st *)IO_HANDLE(io); 

    /* store the info about the data area */
    handle->base   = SEQ_ELEM (clt_cks, 1).a;
    handle->limit  = handle->base + SEQ_ELEM (clt_cks, 1).w; 

    return True; 
}

static void RXIO_Closed_m (IDCCallback_cl *self, IDCOffer_clp offer,
			   IDCServerBinding_clp binding, 
			   const Type_Any *server)
{
    /* XXX Clean up handle, etc */
    return;
}



static IOOffer_clp AddTransmitter_m(Netif_cl *self, const Netif_TXQoS *qos,
				    Netif_TXSort sort, PF_Handle *handle)
{
    Netif_st *st=self->st;
    struct tx_st *txst;
    IOTransport_clp iot;
    IOOffer_clp offer;
    IDCService_clp service;
    IOData_Shm *shm; 
    qosdata_t *newqos;

    /* Allocate state record */
    txst = Heap$Malloc(st->heap, sizeof(*txst));
    TRC(printf("netif: new transmitter state at %p\n", txst));

    txst->sort   = sort;
    txst->filter = NULL; /* no transmissions until set tx filter */
    txst->io     = NULL; /* no IO until bound to by client */

    newqos = Heap$Malloc(st->heap, sizeof(*newqos));
    if (!newqos) RAISE_Heap$NoMemory();
    newqos->iohandle = txst;	    /* so we can recognise it later */
    newqos->qos = *qos;
    LINK_ADD_TO_TAIL(&st->qos, newqos);
    
    iot   = NAME_FIND("modules>IOTransport", IOTransport_clp);
    /* 
    ** Create the IOOffer; the 'protocol' for tranmit IO's is to 
    ** have 3 recs per transaction, as above. In the normal case (i.e. 
    ** sort is 'Native') we expect all recs to be within a pre-validated 
    ** region. However if the sort is 'Socket' we only require that the 
    ** client pre-allocate memory for the header and trailer recs in 
    ** order to prevent excessive copying. The user-data rec, rec[1], 
    ** is validated on a per-transaction basis. 
    ** 
    ** For a depth of 32 = 8 packets of 3 recs + 1 rec header (added 
    ** by the IO code), we ask for a minimum of 64K in the case of 
    ** 'Normal' transmitters (which allows 2K per transaction). In the
    ** case of 'Socket' transmitters, we only specify a minimum of 8K 
    ** memory (which allows 1K bytes for headers/trailers (+ related) 
    ** per transaction). 
    */


#define TXIO_DEPTH 32
#define TXIO_NATIVE_SHMSZ 65536
#define TXIO_SOCKET_SHMSZ 16384

    shm   = SEQ_NEW(IOData_Shm, 1, st->heap); 
    SEQ_ELEM(shm, 0).buf.base     = NO_ADDRESS; /* not yet allocated */
    SEQ_ELEM(shm, 0).buf.len      = 
	(sort == Netif_TXSort_Native) ? TXIO_NATIVE_SHMSZ : TXIO_SOCKET_SHMSZ;
    SEQ_ELEM(shm, 0).param.attrs  = 0;
    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;
    
    offer = IOTransport$Offer(iot, st->heap, TXIO_DEPTH, IO_Mode_Tx, 
			      IO_Kind_Master, shm, Pvs(gkpr), &st->txiocb_cl, 
			      (IOEntry_clp)st->tx, (word_t)txst, &service);
    
    /* the 'handle' is our tx state */
    *handle = (PF_Handle)txst;
    return offer;
}

static bool_t RemoveTransmitter_m(Netif_cl *self, PF_Handle handle)
{
    /* XXX not yet implemented */
    return False;
}


static bool_t TXIO_Request_m (IDCCallback_cl *self, IDCOffer_clp offer,
			     Domain_ID dom, ProtectionDomain_ID pdom,
			     const Binder_Cookies *clt_cks, 
			     Binder_Cookies *svr_cks)
{
    Binder_Cookie  shm_desc;
    uint32_t vpn, npages; 

    /* sanity check the data area */
    if (SEQ_LEN (clt_cks) != 2) return False;
    shm_desc = SEQ_ELEM (clt_cks, 1);
    TRC(eprintf("TXIO$Request: client's given me buf at %p, length %x\n", 
		shm_desc.a, shm_desc.w)); 

    vpn    = (word_t)shm_desc.a >> PAGE_WIDTH; 
    npages = (word_t)shm_desc.w >> PAGE_WIDTH; 
	
    /* XXX check translations of region and see if can nail */
#if 0
    if( (rc = ntsc_gprot(vpn, npages, 0x7))) {
	eprintf("ntsc_gprot returned error %ld\n", (long)rc);
	return False;
    } 
#endif


    return True;
}

static bool_t TXIO_Bound_m (IDCCallback_cl *self, IDCOffer_clp offer,
			   IDCServerBinding_clp binding, Domain_ID dom,
			   ProtectionDomain_ID pdom, 
			   const Binder_Cookies *clt_cks, Type_Any *server)    
{
    Netif_st *st = self->st;
    IO_cl    *io = NARROW(server, IO_clp);
    qosdata_t *q;
    Time_ns p, s;
    bool_t x;
    struct tx_st *handle;

    handle = (struct tx_st *)IO_HANDLE(io); 

    /* set the association between handle and the io */
    if (handle->io)
	printf("Netif: warning: handle %p already bound to %p, "
	       "rebinding to %p\n", handle, handle->io, io);
    handle->io = io;


    /* check the data area matches our size requirements */
    if(SEQ_ELEM(clt_cks, 1).w < ( (handle->sort == Netif_TXSort_Native) ? 
				  TXIO_NATIVE_SHMSZ : TXIO_SOCKET_SHMSZ)) {
	eprintf("TXIO$Request: insufficient shared memory.\n");
	return False;
    }

    /* store the info about the data area */
    handle->base   = SEQ_ELEM (clt_cks, 1).a;
    handle->limit  = handle->base + SEQ_ELEM (clt_cks, 1).w; 

    handle->outstanding = 0;	/* no packets queued with CSD yet */

    q = (qosdata_t *)st->qos.next;
    while(q != &st->qos)
    {
	TRC((":: considering iohandle at %p: (p:%qx s:%qx x:%s)\n",
	     q->iohandle, q->qos.p, q->qos.s, q->qos.x?"True":"False"));
	if (q->iohandle == handle)
	{
	    p = q->qos.p;
	    s = q->qos.s;
	    x = q->qos.x;
	    TRC(("p = %qx,  s = %qx,  x = %s\n", p, s, x?"True":"False"));
	    QoSEntry$SetQoS(st->tx, io, p, s, x, DEF_LAXITY);
	    LINK_REMOVE(q);
	    Heap$Free(st->heap, q);
	    return True;
	}
	/* else */
	q = q->next;
    }
    printf("Netif: No QoS for iohandle=%p, can't happen?!?\n", handle);

    return False; 
}

static void TXIO_Closed_m (IDCCallback_cl *self, IDCOffer_clp offer,
			   IDCServerBinding_clp binding, 
			   const Type_Any *server)
{
    /* XXX Clean up handle, remove QoS, etc */
    return;
}



static bool_t SetRxFilter_m(Netif_cl *self, PF_cl *pf) 
{
    Netif_st *st = (Netif_st *)self->st;
    st->rx_filter = pf;
    st->snaplen   = PF$Snaplen(pf);
    return True;
}

static bool_t SetTxFilter_m(Netif_cl *self, PF_Handle handle, PF_cl *pf) 
{
    struct tx_st *tx = (struct tx_st *)handle;

    tx->filter = pf;
    return True;
}

static bool_t Inject_m(Netif_cl *self, PF_Handle handle, IO_Rec *pkt, 
		       uint32_t nrecs, word_t value)
{
    printf("Netif$Inject() called.  But I'm not doing anything.\n");
    return False;
}


static bool_t SetQoS_m(Netif_cl *self,
		       PF_Handle handle,
		       const Netif_TXQoS *qos)
{
    Netif_st *st = (Netif_st *)self->st;
    struct tx_st *tx = (struct tx_st *)handle;

    if (tx->io)
    {
	QoSEntry$SetQoS(st->tx, tx->io, qos->p, qos->s, qos->x, DEF_LAXITY);
	return True;
    }
    else
    {
	return False;
    }
}


static SimpleIO_cl *New_m(NetifMod_cl *self, Netcard_clp card, 
			  SimpleIO_cl *txio,
			  uint32_t maxtxq,
			  uint32_t mtu, uint32_t rate,
			  Net_EtherAddrP mac,
			  /* OUT */ SimpleIO_clp *txfree,
			            Netif_clp *netif)
{
    Netif_cl *cl;
    Netif_st *st;
    QoSEntryMod_clp qosem;
    IOEntryMod_clp  ioem;
    int	i;

    TRC(("NetifMod$New: entered.\n"));

    /* Allocate space for closure and state */
    cl = Heap$Malloc(Pvs(heap), sizeof(*cl) + sizeof(*st));

    cl->op = &n_op;
    cl->st = st=(Netif_st *)(cl+1);
    *netif = cl;

    qosem = NAME_FIND("modules>QoSEntryMod", QoSEntryMod_clp);
    ioem  = NAME_FIND("modules>IOEntryMod", IOEntryMod_clp);

    /* Fill in state */
    st->heap = Pvs(heap);
    st->card = card;
    st->mtu  = mtu;

#if 0
    /* check that "mac" starts word aligned, as needed by EQ6() */
    if (ALIGN4(mac) != (word_t)mac)
    {
	printf("NetifMod$New: mac address passed in not aligned correctly\n");
	return NULL;
    }
#endif

    st->mac  = mac;
    st->txio = txio;

    st->txfree.op = &txfree_ms;
    st->txfree.st = st;

    st->rxio.op = &rx_ms;
    st->rxio.st = st;

    st->rx_filter = NULL;
    st->snaplen   = 0;
    st->rxbuff    = Heap$Malloc(Pvs(heap), mtu*2);

    st->rx = IOEntryMod$New(ioem, Pvs(actf), Pvs(vp), 
			    VP$NumChannels(Pvs(vp)));
    st->tx = QoSEntryMod$New(qosem, Pvs(actf), Pvs(vp), 
			     VP$NumChannels(Pvs(vp)));

    /* header buffers */
    st->txqsz = maxtxq;
    st->txring = Heap$Malloc(Pvs(heap),
			     /* ring slots: */
			     (sizeof(tx_qitem*) * maxtxq) +
			     /* tx_qitems: */
			     (sizeof(tx_qitem) * maxtxq) +
			     /* header buffers: must be aligned to 64 bytes */
			     MAX_HEADER_LEN + 
			     (MAX_HEADER_LEN * maxtxq));
    TRC(("txstack at %p\n", st->txstack));
    {
	/* lay out the tx_qitem pointers */
	word_t itembase = (word_t)(st->txring) +
	                                 (sizeof(tx_qitem*) * maxtxq);
#define ALIGN64(_x) (((word_t)(_x)+63)&(~63))
	word_t hdrbase  = ALIGN64(itembase + (sizeof(tx_qitem) * maxtxq));
	
	TRC(("tx_qitem base at 0x%08x\n", itembase));
	TRC(("header base at 0x%08x\n", hdrbase));
	for(i=0; i<maxtxq; i++)
	{
	    st->txring[i] = (addr_t)(itembase + (sizeof(tx_qitem)*i));
	    TRC(("  + tx_qitem %d at %p\n", i, st->txring[i]));
	    st->txring[i]->hrec.base = (addr_t)(hdrbase + (MAX_HEADER_LEN*i));
	    st->txring[i]->hrec.len = MAX_HEADER_LEN;
	    TRC(("  + header %d at %p\n", i, st->txring[i]->hrec.base));
	}
    }
#ifdef TXQ_PARANOIA
    st->txqitems = 0;	/* empty Q */
    st->txqfree  = 0;	/* we expect slot 0 to be freed next */
    st->xxpos    = 0;   /* start of the buffer */
    st->txqmax	 = 0;
#endif
    st->txqec = EC_NEW();
    /* bump the event count "maxtxq"-1 times since we're starting with
     * all buffers available */
    if (maxtxq - 1)
	EC_ADVANCE(st->txqec, maxtxq-1);
    st->txqack = 0;	/* start allocating from slot 0 */

    st->outstanding=0;

    /* work out the cost per byte given the line rate */
    st->costPerByte = 1000000000 / rate * 8;
    /* cost is 1 / rate (to get seconds per bit), times 10^9 to
     * get nanoseconds per bit, times 8 to get nanoseconds per byte */

    CLP_INIT(&st->txiocb_cl, &txiocb_ms, st);
    CLP_INIT(&st->rxiocb_cl, &rxiocb_ms, st);
    LINK_INIT(&st->qos);

    st->rxpackets = 0;
    st->txpackets = 0;

#ifdef BANDWIDTH_LOG
    st->logsize = 0;
#endif /* BANDWIDTH_LOG */
#ifdef DROP_LOG
    st->dropsize = 0;
    st->drop_xfer = False;
    ENTER_KERNEL_CRITICAL_SECTION();
    (*((addr_t*)(0x1108))) = st->drops;
    (*((word_t*)(0x110c))) = DROP_LOG * sizeof(droplog_t);
    LEAVE_KERNEL_CRITICAL_SECTION();
#endif /* DROP_LOG */

    /* initialise the FragPool */
    st->pool = fragpool_new(mtu, st, to_client);

    /* Create threads */
    TRC(("NetifMod$New: forking threads...\n"));
    Threads$Fork(Pvs(thds), TXThread, st, 0);
    Threads$Fork(Pvs(thds), RXConThread, st, 0);

    TRC(("NetifMod$New: done.\n"));
    *txfree = &st->txfree;
    return &(st->rxio);
}


/* End of netif.c */
