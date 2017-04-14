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
**      icmp.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of ICMP for the Flow Manager
** 
** ENVIRONMENT: 
**
**      Flow Manager
** 
** ID : $Id: icmp.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <IOMacros.h>
#include <stdio.h>
#include <netorder.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <ecs.h>
#include <iana.h>

#include <ICMP.ih>
#include <ICMPMod.ih>
#include <Heap.ih>
#include <Ether.ih>
#include <EtherMod.ih>
#include <IP.ih>
#include <IPMod.ih>
#include <Threads.ih>
#include <Protocol.ih>
#include <IO.ih>
#include "flowman_st.h"

/* VERBOSE defined means print something for each icmp packet, even
   if its been handled */
#undef VERBOSE

#ifdef ICMP_DEBUG
#define TRC(x) printf x
#else
#define TRC(x)
#endif

#ifdef VERBOSE
#define TRCV(x) printf x
#else
#define TRCV(x)
#endif



typedef struct {
    char        *name; 
    Ether_cl	*eth;   /* ether layer of our stack */
    IP_cl	*ip;    /* IP layer of our stack */
    IO_cl       *rx;
    IO_cl       *tx;
    Heap_cl     *rxheap;
    Heap_cl     *txheap;
    uint32_t	 mtu;
    uint32_t	 headsz;
    uint32_t	 tailsz;
    void	*txbuf;
    void	*rxbase;
    /* rate limit stuff */
    Time_ns	lastmsg;	/* time at which the last message was sent */
    int		backlog; 	/* how many messages we've skipped */
    int		credits;	/* how many messages we're allowed to send */
} ICMP_st;

/* ICMP packet header */
typedef struct {
    uint8_t	type;   /* major ICMP packet type */
    uint8_t	code;   /* sub-code within packet type */
    uint16_t	chksum; /* checksum over packet from ICMP headers */
    /* next 32bits can be divided into 2 shorts, a 8bit pointer, or a
     * 32bit word depending on ICMP packet type. */
    union {
	struct {
	    uint16_t s1;
	    uint16_t s2;
	} s;
	struct {
	    uint8_t pointer;
	    uint8_t pad[3];
	} p;
	uint32_t    word;
    } u;
} icmp_pkt;

static const uint8_t ethaddr_broadcast[] =
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* icmp dispatch table */
typedef void icmp_handler_fn(ICMP_st *st, uint32_t nrecs, IO_Rec *recs);
static const icmp_handler_fn error;
static const icmp_handler_fn echo_reply;
static const icmp_handler_fn dest_unreach;
static const icmp_handler_fn src_quench;
static const icmp_handler_fn redirect;
static const icmp_handler_fn echo_req;
static const icmp_handler_fn time_exc;
static const icmp_handler_fn param_prob;
static const icmp_handler_fn ts_req;
static const icmp_handler_fn ts_reply;
static const icmp_handler_fn addrmask_req;
static const icmp_handler_fn addrmask_reply;

/* need to note the class each ICMP falls into, since we're not
 * allowed to generate ICMP in response to icmp_errors, or reply to
 * broadcast errors. */
typedef enum {
    Query,
    Error
} icmp_class_t;

typedef struct {
    icmp_class_t	class;
    icmp_handler_fn	*handler;
} icmp_info;

static icmp_info const icmp_tbl[] =
{
    {Error, echo_reply},	/* 0*/
    {Error, error},		/* 1*/
    {Error, error},		/* 2*/
    {Error, dest_unreach},	/* 3*/
    {Error, src_quench},		/* 4*/
    {Error, redirect},		/* 5*/
    {Error, error},		/* 6*/
    {Error, error},		/* 7*/
    {Query, echo_req},		/* 8*/
    {Error, error},		/* 9*/
    {Error, error},		/*10*/
    {Error, time_exc},		/*11*/
    {Error, param_prob},		/*12*/
    {Query, ts_req},		/*13*/
    {Error, ts_reply},		/*14*/
    {Error, error},		/*15 information request - obsolete*/
    {Error, error},		/*16 information reply   - obsolete*/
    {Query, addrmask_req},	/*17*/
    {Error, addrmask_reply}	/*18*/
};

/* prototypes */
static void icmp_RX(ICMP_st *st);
static void do_icmp(ICMP_st *st, uint32_t nrecs, IO_Rec *recs);
static void send(ICMP_st *st, uint32_t nrecs, IO_Rec *recs);
static int ratelimit(ICMP_st *st);

/* ------------------------------------------------------------ */

/* ICMP Module stuff */
static	ICMP_Send_fn 		ICMP_Send_m;
static  ICMP_Dispose_fn         ICMP_Dispose_m;
static	ICMP_op	icmp_ops = {ICMP_Send_m, ICMP_Dispose_m};

/* ------------------------------------------------------------ */
/* ICMPMod stuff */

static  ICMPMod_New_fn          ICMPMod_New_m;
static  ICMPMod_op      mod_ops = {ICMPMod_New_m};
static  const ICMPMod_cl      modcl = { &mod_ops, NULL };
PUBLIC  const ICMPMod_cl * const icmpmod = &modcl;  /* exported to other files */

/*---------------------------------------------------- Entry Points ----*/

static ICMP_clp 
ICMPMod_New_m(ICMPMod_cl *self, IO_cl *rxio, IO_cl *txio, 
	      Heap_cl *rxheap, Heap_cl *txheap, const Net_IPHostAddr *myip, 
	      const Net_EtherAddr *myether, char *ifname)
{
    ICMP_cl	*cl;
    ICMP_st	*st;
    EtherMod_cl *ethmod;
    Ether_cl	*eth;
    IPMod_cl	*ipmod;
    IP_cl	*ip;
    Net_EtherAddr hwaddr;
    Net_IPHostAddr destip;
    int		i;
    IO_Rec	recs[3];

    cl = Heap$Malloc(Pvs(heap), sizeof(ICMP_cl) + sizeof(ICMP_st));
    if (!cl)
    {
	printf("icmp: New(): out of memory\n");
	return NULL;
    }

    st       = (ICMP_st *)(cl + 1);
    cl->st   = st;
    cl->op   = &icmp_ops;
    st->name = strduph(ifname, Pvs(heap));

    /* rate limit stuff */
    st->lastmsg = 0;	/* will eventually get set */
    st->backlog = 0;
    st->credits = 10;

    /* Build our stack */
    TRC(("ICMP$New: building IP stack\n"));
    hwaddr = (Net_EtherAddr){{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    ethmod = NAME_FIND("modules>EtherMod", EtherMod_clp);
    eth = EtherMod$New(ethmod, &hwaddr, myether, FRAME_IPv4, 
		       rxio, txio);

    ipmod    = NAME_FIND("modules>IPMod", IPMod_clp);
    destip.a = 0x00000000;
    ip       = IPMod$Piggyback(ipmod, (Protocol_clp)eth, &destip, myip,
			       IP_PROTO_ICMP, 0, 0, txheap);

    st->eth  = eth;
    st->ip   = ip;

    /* send some empties down to start things */
    st->headsz = Protocol$QueryHeaderSize((Protocol_clp)ip);
    st->tailsz = Protocol$QueryTrailerSize((Protocol_clp)ip);
	
    TRC(("ICMP$New: priming recv pipe\n"));
    st->tx = Protocol$GetTX((Protocol_clp)ip);
    st->rx = Protocol$GetRX((Protocol_clp)ip);
#define PIPEDEPTH 5
    st->mtu = Protocol$QueryMTU((Protocol_clp)ip);
    /* align the MTU up to a 4-byte boundary for Alphas */
    st->mtu = ((st->mtu+3)&(~3));

    st->rxbase = Heap$Malloc(rxheap, st->mtu * PIPEDEPTH);
    if (!st->rxbase)
	printf("icmp: New: out of memory(2)\n"); /* die shortly */
    st->txbuf = Heap$Malloc(txheap, st->mtu);
    if (!st->txbuf)
	printf("icmp: New: out of memory(3)\n");

    for(i=0; i<PIPEDEPTH; i++)
    {
	recs[0].base = st->rxbase + (st->mtu * i);
	recs[0].len  = st->headsz;
	recs[1].base = st->rxbase + (st->mtu * i) + st->headsz;
	recs[1].len  = st->mtu - st->headsz - st->tailsz;
	recs[2].base = st->rxbase + (st->mtu * (i+1)) - st->tailsz;
	recs[2].len  = st->tailsz;
	TRC(("icmp: priming %d: base:%p len:%d\n",
	     i, recs[1].base, recs[1].len));
	IO$PutPkt(st->rx, 3, recs, 0, FOREVER);
    }

    /* fork off the receiver thread */
    TRC(("ICMP$New: forking icmp_RX thread\n"));
    Threads$Fork(Pvs(thds), icmp_RX, st, 8192);

    return cl;
}


static void ICMP_Dispose_m(ICMP_cl *self)
{
    ICMP_st       *st = self->st;

    /* XXX free protocol stuff too */

    Heap$Free(st->rxheap, st->rxbase);
    Heap$Free(st->txheap, st->txbuf);
    Heap$Free(Pvs(heap), self);
}



static void icmp_RX(ICMP_st *st)
{
    IO_Rec recs[3];
    int nrecs;
    word_t value;

    TRC(("icmp_RX: thread started\n"));

    TRY {
	while(1)
	{
	    IO$GetPkt(st->rx, 3, recs, FOREVER, &nrecs, &value);
	    if (nrecs!=3)
	    {
		printf("icmp_RX: IO$GetPkt() returned not 3 recs?\n");
	    }
	    else
	    {
		do_icmp(st, nrecs, recs);
		/* grow packet back to original size, and give it to
		 * driver to recv into. */
		recs[0].len = st->headsz;
		recs[1].len = st->mtu - (st->headsz + st->tailsz);
		recs[2].len = st->tailsz;
		IO$PutPkt(st->rx, nrecs, recs, 0, FOREVER);
	    }
	}
    } CATCH_Thread$Alerted() {
	/* we get here if either the IO$GetPkt or the IO$PutPkt raise
	 * Thread$Alerted, eg because the device driver went away
	 */
	
	printf("flowman: card %s went away, cleaning up (not really)\n",
	       st->name);
	/* thread dies by falling off the end */
    } ENDTRY	
}

/* called to process each ICMP packet. 3 IO_Recs, starting at "recs". */
static void do_icmp(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;
    uint32_t len  = recs[1].len;
    uint8_t *hdr  = recs[0].base;
    uint32_t csum;
    uint16_t *p;

    /* 1 byte of type, 1 byte of sub-code, 2 bytes of checksum,
     * plus 4 bytes of either pad or extra info */
    if (len < 8)
    {
	printf("icmp: recv of short packet (%d<8)\n", len);
	return;
    }

    /* verify checksum */
    if (len & 1)
    {
	printf("icmp: packet len(%d) is not 16-bit aligned\n", len);
	return;
    }
    p = recs[1].base;
    csum = 0;
    while(len)
    {
	csum += ntohs(*p);
	p++;
	len -= 2;	
    }
    csum = (csum & 0xffff) + (csum >> 16);
    csum += (csum >> 16);
    if (csum != 0xffff)
    {
	printf("icmp: recv with bad checksum (%x)\n", csum);
	return;
    }

    if (pkt->type > 18)
    {
	printf("icmp: recv of packet with unknown icmp type %d\n", pkt->type);
	return;
    }

    /* if this is was from a broadcast, note it and warn of excessive
     * quantities */
    /* XXX should really check the flags on a pktcx thingy */
    if (!memcmp(hdr + 2, ethaddr_broadcast, 6))
    {
	/* RFC1122 says we're not allowed to reply to broadcasted
         * error class ICMP */
	if (icmp_tbl[pkt->type].class == Error)
	{
	    if (ratelimit(st))
		printf("icmp: broadcast error-class ICMP, not replying\n");
	    return;
	}

	if (ratelimit(st))
	    printf("icmp: warning: replying to broadcast\n");
    }

    /* call the handler */
    (icmp_tbl[pkt->type].handler)(st, nrecs, recs);
}

/* ------------------------------------------------------------ */
/* ICMP handlers */

static void echo_reply(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;

    printf("icmp: received ICMP Echo Reply: id=%d, seq=%d\n",
	    pkt->u.s.s1, pkt->u.s.s2);
}

static void error(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;

    printf("icmp: invalid icmp type %d\n",
	    pkt->type);
}

/* these reasons get more insane the further down the list :) */
static const char * const destmsg[]=
{
    "network unreachable",
    "host unreachable",
    "protocol unreachable",
    "port unreachable",
    "fragmentation required, but Don't Fragment set",
    "source route failed",
    "destination network unknown",
    "destination host unknown",
    "source host isolated",
    "communications with destination network administratively prohibited",
    "communications with destination host administratively prohibited",
    "network unreachable for type of service",
    "host unreachable for type of service"
};

static void dest_unreach(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;

    if (pkt->code > (sizeof(destmsg)/sizeof(char *)))
    {
	printf("icmp: recv ICMP Destination Unreachable, "
		"but additional code was out of range (%d)\n", pkt->code);
	return;
    }

    printf("icmp: received ICMP Destination Unreachable: %s\n",
	    destmsg[pkt->code]);

    /* XXX integrate this into the connection setup stuff */
}

static void src_quench(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    /* XXX this breaks rfc1123 (Host Requirements). Needs sorting pronto */
    printf("icmp: *** received ICMP Source Quench ***\n");
}

static void redirect(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;

    /* XXX pass on to routing code */
    printf("icmp: received ICMP Redirect: use %I as a gateway\n",
	    pkt->u.word);
}

static void echo_req(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;
    uint8_t *hdr = (void *)(recs[0].base);

    TRCV(("icmp: received ICMP Echo Request\n"));

    /* reply to it */
    pkt->type = ICMP_ECHO_REPLY;
    /* set the IP and ethernet destinations */
    IP$SetDest(st->ip, (Net_IPHostAddr *)(hdr + 16 + 12));
    Ether$SetDest(st->eth, (Net_EtherAddr *)(hdr +  8));
    send(st, nrecs, recs);
    TRCV(("icmp: reply sent\n"));
}


static const char * const timemsg[]=
{
    "ttl expired",
    "fragment reassembly timed out"
};

static void time_exc(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;

    if (pkt->code > (sizeof(timemsg)/sizeof(char *)))
    {
	printf("icmp: recv ICMP Time Exceeded, "
		"but additional code was out of range (%d)\n", pkt->code);
	return;
    }

    printf("icmp: received ICMP Time Exceeded: %s\n", timemsg[pkt->code]);
    /* XXX work out which stream its from, and report the error */
}

static void param_prob(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    printf("icmp: received ICMP Paramater Problem\n");
    /* XXX work out who; and fix it */
}
	    
static void ts_req(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    printf("icmp: received ICMP Timestamp Request\n");
    /* XXX return the time of day: but do we want to do this? */
}

static void ts_reply(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    printf("icmp: received ICMP Timestamp Reply\n");
    /* XXX feed it back to the Send(TimeStampRequest) function */
}

static void addrmask_req(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    printf("icmp: received ICMP Address Mask Request; "
	    "someone thinks we're a router?\n");
    /* XXX we shouldn't see these! */
}

static void addrmask_reply(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    icmp_pkt *pkt = recs[1].base;

    printf("icmp: received ICMP Address Mask Reply: %I\n",
	    pkt->u.word);
    /* XXX pass on to other layers */
}


/*------------------------------------------------ Send Entry Point ----*/

static void 
ICMP_Send_m (
	ICMP_cl	*self,
	ICMP_Msg	type	/* IN */,
	uint8_t		code,
	const Net_IPHostAddr	*dest	/* IN */ )
{
  /* NULL implementation at the moment */
}


/* internally used function to send a packet */
static void send(ICMP_st *st, uint32_t nrecs, IO_Rec *recs)
{
    word_t value;
    int i;
    uint32_t sum;
    uint8_t *hdr;
    icmp_pkt *pkt;

    /* Calc the checksum */
    pkt = recs[1].base;
    hdr = recs[1].base;
    pkt->chksum = 0; /* zero the checksum while calculating it */
    
    /* Verify IP checksum. Magic pilfered from ebsdk boot rom source: */
    /* "To calculate the one's complement sum calculate the 32 bit
     *  unsigned sum and add the top and bottom words together."  */
    sum = 0;
    for(i=0; i < recs[1].len; i+=2)
    {
	sum += (hdr[i] << 8) + hdr[i+1];
    }
    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum >> 16);  /* Add in possible carry. */
    if (sum == 0xffff)
	sum = 0;

    /* Take 1s complement and slap it in */
    sum = ~sum;
    pkt->chksum = htons((uint16_t)sum);


    
    IO$PutPkt(st->tx, nrecs, recs, 0, FOREVER); /* transmit... */
    IO$GetPkt(st->tx, nrecs, recs, FOREVER, &nrecs, &value); 
    /*...and pick up the free buffer*/
}


/* This function returns 1 if a message is allowed to be printed, 0
 * otherwise.  If messages have been skipped, then a summary of how
 * many is printed.  */
#define MS_PER_MSG     1000
#define MAX_BURST        10
static int ratelimit(ICMP_st *st)
{
    int newcreds;
    Time_ns now = NOW();

    /* calculate how many credits have accrued. Get 100 credits for each
     * message that's accrued. */
    newcreds = ((uint32_t)((now - st->lastmsg) / 1000000)) / MS_PER_MSG;
    st->lastmsg = now;

    /* cap the credits at 10 messages */
    st->credits = MIN(MAX_BURST, st->credits + newcreds);

    /* if we have enough credits, then we're ok */
    if (st->credits >= 1)
    {
	if (st->backlog > 0)
	{
	    printf("msg ratelimit: suppressed %d messages\n", st->backlog);
	    st->backlog = 0;
	}
	st->credits--;
	return 1;
    }
    else
    {
	/* not enough credits, just count the messages */
	st->backlog++;
	return 0;
    }
}


/* End of icmp.c */
