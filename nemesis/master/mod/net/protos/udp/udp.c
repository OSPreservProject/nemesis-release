/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	udp.c
**
** FUNCTIONAL DESCRIPTION:
**
**	UDP protocol implementation (see rfc768)
**
** ENVIRONMENT: 
**
**	User space protocol stack
**
** ID : $Id: udp.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>
#include <iana.h>
#include <pktcx.h>

#include <UDP.ih>
#include <UDPMod.ih>

#include <IP.ih>
#include <IPMod.ih>

#include <nettrace.h>

/* UDPMod stuff */
static UDPMod_New_fn New_m;
static UDPMod_NewCustomIP_fn NewCustomIP_m;
static UDPMod_Piggyback_fn Piggyback_m;
static UDPMod_op modops = {New_m, NewCustomIP_m, Piggyback_m};
static const UDPMod_cl modcl = {&modops, BADPTR};
CL_EXPORT (UDPMod, UDPMod, modcl);

/* UDP ops */
static  Protocol_GetTX_fn               GetTX_m;
static  Protocol_GetRX_fn               GetRX_m;
static  Protocol_QueryMTU_fn            QueryMTU_m;
static  Protocol_QueryHeaderSize_fn     QueryHeaderSize_m;
static  Protocol_QueryTrailerSize_fn    QueryTrailerSize_m;
static  Protocol_Dispose_fn             Dispose_m;
static  UDP_SetTXPort_fn		SetTXPort_m;
static  UDP_Retarget_fn                 Retarget_m;
static  UDP_GetLastPeer_fn              GetLastPeer_m;
static  UDP_GetFlow_fn			GetFlow_m;
static  UDP_GetConnID_fn                GetConnID_m;
static  UDP_op ops = {GetTX_m, GetRX_m,
		      QueryMTU_m, QueryHeaderSize_m, QueryTrailerSize_m,
		      Dispose_m,
		      SetTXPort_m, Retarget_m, GetLastPeer_m,
		      GetFlow_m, GetConnID_m};

/* IO ops */
static  IO_PutPkt_fn            PutPkt_m;
static  IO_AwaitFreeSlots_fn    AwaitFreeSlots_m;
static  IO_GetPkt_fn            GetPkt_m;
static  IO_QueryDepth_fn        IO_QueryDepth_m;
static  IO_QueryKind_fn         IO_QueryKind_m;
static  IO_QueryMode_fn         IO_QueryMode_m;
static  IO_QueryShm_fn          IO_QueryShm_m;
static  IO_QueryEndpoints_fn    IO_QueryEndpoints_m;
static  IO_Dispose_fn           IO_Dispose_m;

static  IO_op   io_ops = {
    PutPkt_m,
    AwaitFreeSlots_m,
    GetPkt_m,
    IO_QueryDepth_m,
    IO_QueryKind_m,
    IO_QueryMode_m,
    IO_QueryShm_m,
    IO_QueryEndpoints_m,
    IO_Dispose_m
};

/* ------------------------------------------------------------ */

#define MAX_HEADERS   10

typedef uint8_t udpheader[8];
#define UDPHEAD_SRC   0
#define UDPHEAD_DEST  2
#define UDPHEAD_LEN   4
#define UDPHEAD_CHK   6

typedef struct {
    uint16_t	destport;
    uint16_t	srcport;
    uint32_t	offset;
} UDPCom_st;

typedef struct {
    IO_cl	*io;     /* IO to the stack below us */
    UDPCom_st	*common;
    bool_t	rxp;     /* True if this is a RX IO */
    uint16_t	lastrxport;	/* valid if this is an RX IO */
} io_st;

typedef struct {
    Protocol_cl	*base;
    IO_cl	*iotx;
    IO_cl	*iorx;
    bool_t	overIP;		/* True if we shoudl allow GetLastPeer() */
    FlowMan_ConnID	connid;
} UDP_st;

/* prototypes */
static INLINE int rx_check(io_st *st, uint32_t *nrecs, IO_Rec *recs,
			   uint32_t max_recs, pktcx_t *pc, word_t *value);
static INLINE bool_t tx_encaps(io_st *st, uint32_t nrecs, IO_Rec *recs);

/* ------------------------------------------------------------ */

static UDP_clp 
internal_new(
        UDPMod_cl       *self,
        Protocol_clp    base    /* IN */,
        uint16_t        destport        /* IN */,
        uint16_t        srcport /* IN */,
	FlowMan_ConnID	cid,
	bool_t		overIP)
{
    UDP_cl *cl;
    UDP_st *st;
    UDPCom_st *com;
    io_st  *rxst;
    io_st  *txst;
    IO_cl  *rxio;
    IO_cl  *txio;

    st = Heap$Malloc(Pvs(heap), sizeof(UDP_cl) + sizeof(UDP_st) +
	                        sizeof(UDPCom_st) +
		                (2*(sizeof(io_st) + sizeof(IO_cl))));
    if (!st)
    {
	printf("udp.c: internal_new: out of memory\n");
	return NULL;
    }
    
    /* lay out memory */
    cl  = (UDP_cl *)   (st + 1);
    com = (UDPCom_st *) (cl + 1);
    rxst= (io_st *)    (com+ 1);
    txst= (io_st *)    (rxst+1);
    rxio= (IO_cl *)    (txst+1);
    txio= (IO_cl *)    (rxio+1);

    /* plumb it together */
    cl->op = &ops;
    cl->st = st;
    st->iotx = txio;
    st->iorx = rxio;
    st->overIP = overIP;
    st->connid = cid;
    txio->op = &io_ops;
    txio->st = txst;
    rxio->op = &io_ops;
    rxio->st = rxst;

    /* init IO state */
    TRY
	txst->io = Protocol$GetTX(base);
    CATCH_Protocol$Simplex()
	txst->io = NULL;
    ENDTRY
    txst->common = com;
    txst->rxp    = False;
    txst->lastrxport = 0;  /* not that this is actually valid */

    TRY
	rxst->io = Protocol$GetRX(base);
    CATCH_Protocol$Simplex()
	rxst->io = NULL;
    ENDTRY
    rxst->common = com;
    rxst->rxp    = True;
    rxst->lastrxport = 0;

    /* rest of state init */
    st->base     = base;             /* protocol under us */
    com->destport = destport;
    com->srcport  = srcport;
    com->offset   = Protocol$QueryHeaderSize(base);

    return cl;
}


static UDP_clp
New_m(
        UDPMod_cl       *self,
        FlowMan_clp     fman    /* IN */,
        FlowMan_SAP     *lsap   /* INOUT */,
        const FlowMan_SAP       *rsap   /* IN */,
        const Netif_TXQoS       *txqos  /* IN */,
	UDPMod_Mode	mode,
   /* RETURNS */
        Heap_clp        *rxheap,
        Heap_clp        *txheap )
{
    IPMod_clp	ipmod;
    IP_clp	ip;
    IPMod_Mode	ipmode;

    /* bind the port we want to use */
    if (!FlowMan$Bind(fman, lsap))
    {
	/* bind failed. bummer dude. */
	RAISE_UDPMod$PortInUse();
    }

    /* really redundate switch, but it keeps the compiler happy */
    switch(mode) {
    case UDPMod_Mode_Split:
	ipmode = IPMod_Mode_Split;
	break;
    case UDPMod_Mode_Dx:
	ipmode = IPMod_Mode_Dx;
	break;
    default:
	printf("udp: bogus mode %d, can't happen\n", mode);
	ipmode = IPMod_Mode_Split;
    }

    /* build the stack below us */
    ipmod = NAME_FIND("modules>IPMod", IPMod_clp);
    ip = IPMod$New(ipmod, fman, &rsap->u.UDP.addr, &lsap->u.UDP.addr,
		   txqos, ipmode, IP_PROTO_UDP, 0 /*ttl*/, rxheap, txheap);

    return NewCustomIP_m(self, fman, lsap, rsap, txqos, ip);
}

static UDP_clp
NewCustomIP_m(
        UDPMod_cl         *self,
        FlowMan_clp        fman    /* IN */,
        FlowMan_SAP       *lsap    /* INOUT */,
        const FlowMan_SAP *rsap    /* IN */,
        const Netif_TXQoS *txqos   /* IN */,
        IP_clp             ip      /* IN */)
{
    FlowMan_ConnID	cid;

    /* we don't do the bind here, since sockets wants to bind separately */
#if 0
    /* bind the port we want to use */
    if (!FlowMan$Bind(fman, lsap))
    {
	/* bind failed. bummer dude. */
	Protocol$Dispose((Protocol_clp)ip); /* XXX is his enough? */
	RAISE_UDPMod$PortInUse();
    }
#endif

    /* configure the packet filters */
    cid = FlowMan$Attach(fman, IP$GetFlow(ip), lsap, rsap);
    if (!cid)
    {
	printf("UDPMod$New: FlowMan$Attach failed\n");
	/* XXX more clearup needed */
	return NULL;
    }

    /* slap the UDP stuff on top of the stack */
    return internal_new(self, (Protocol_clp)ip, rsap->u.UDP.port, 
			lsap->u.UDP.port, cid, True);
}

static UDP_clp 
Piggyback_m(
        UDPMod_cl       *self,
        Protocol_clp    base    /* IN */,
        uint16_t        destport        /* IN */,
        uint16_t        srcport /* IN */,
	FlowMan_ConnID	cid)
{
    return internal_new(self, base, destport, srcport, cid, False);
}


/*---------------------------------------------------- Entry Points ----*/

/* for debugging use */
static void UNUSED dump_IOVec(uint32_t nrecs, IO_Rec *recs)
{
    int i;

    printf("   %d IORecs:\n", nrecs);
    for(i=0; i<nrecs; i++)
	printf("     base:%p len:%d\n", (void *)recs[i].base, recs[i].len);
}

/* ------------------------------------------------------------ */
/* Fake IO interface */

#define MyIO_PutPkt(st, nrecs, recs, value, until)        \
          IO$PutPkt(st->io, nrecs, recs, value, until)
#define MyIO_GetPkt(st, max_recs, recs, until, nrecs, value)     \
          IO$GetPkt(st->io, max_recs, recs, until, nrecs, value)

#include "../fakeIO.c"

/* ------------------------------------------------------------ */
/* UDP implementation */

/* encapsulate the data within a UDP header */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * Task is to put UDP header in "recs[0]"
 * Return True for transmit, False for bad tx attempted.
 */
static INLINE bool_t tx_encaps(io_st *st, uint32_t nrecs, IO_Rec *recs)
{
    int i;
    int size;
    uint8_t *hdr;

#if 0
    for(i=0; i<nrecs; i++)
	eprintf("UDPtx:   base:%p len:%d\n", recs[i].base, recs[i].len);
#endif /* 0 */

    /* range check */
    if (recs[0].len < (st->common->offset + sizeof(udpheader)))
    {
	printf("UDP: first IORec is too small for UDP header (%d < %d)\n",
		recs[0].len, st->common->offset + sizeof(udpheader));
	return False;
    }

    /* work out size of datagram */
    size = 0;
    for(i=0; i<nrecs; i++)
	size += recs[i].len;
    /* take off encapsulation overhead */
    size -= st->common->offset;

    if (size > 0xffff)
    {
	printf("UDP: tx packet is too long: %d bytes\n", size);
	return False;
    }

    /* fill in the header */
    hdr = ((char *)recs[0].base) + st->common->offset;
    *(uint16_t *)&(hdr[UDPHEAD_SRC]) = st->common->srcport;
    *(uint16_t *)&(hdr[UDPHEAD_DEST])= st->common->destport;

    hdr[UDPHEAD_LEN]   = (size & 0xff00) >> 8;
    hdr[UDPHEAD_LEN+1] =  size & 0x00ff;

    /* rfc768 says a checksum of 0 means we couldn't be arsed
     * calculating it */
    hdr[UDPHEAD_CHK]   = 0;
    hdr[UDPHEAD_CHK+1] = 0;

#ifdef LATENCY_TRACE
    if (recs[1].len == 1024 - 44)
	STAMP_PKT_USER(recs[1].base, CLIENT_TOP);
#endif LATENCY_TRACE

    return True;
}


/* check UDP headers on received data */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * Task is to check UDP headers
 * Return True for accept, False for reject.
 */
static INLINE int rx_check(io_st *st, uint32_t *nrecs, IO_Rec *recs,
			   uint32_t max_recs, pktcx_t *pc, word_t *value)
{
    uint8_t *hdr;

    if (*nrecs < 1)
    {
	printf("UDP: programmer error: rx_check() called with nrecs=%d",
		*nrecs);
	return AC_DITCH;
    }

    /* XXX check UDP checksum - one day */

    /* range check */
    if (recs[0].len < st->common->offset + sizeof(udpheader))
    {
	printf("UDP: warning: short first IORec (%d < %d), dropping packet\n",
		recs[0].len, st->common->offset + sizeof(udpheader));
	return AC_DITCH;
    }

    hdr = ((char *)recs[0].base) + st->common->offset;

    if (*(uint16_t *)(hdr + UDPHEAD_DEST) != st->common->srcport)
    {
	printf("UDP: warning: recvd packet with dest port != my src port "
		"(%#x != %#x), packet dropped\n",
		ntohs(*(uint16_t *)(hdr + UDPHEAD_DEST)),
		ntohs(st->common->srcport));
	return AC_DITCH;
    }

    /* make data length equal to udp packet data (in case we've rx'ed
     * a small udp packet, but taken in a minimum sized ethernet
     * frame. Sub 8 for UDP header. Take the min so we don't fall
     * outside the buffer. */
    recs[1].len = MIN(recs[1].len,
		      ntohs(*(uint16_t *)(hdr + UDPHEAD_LEN)) - 8);

    /* record which port this came from, in case client asks us */
    st->lastrxport = *(uint16_t *)(hdr + UDPHEAD_SRC);

#ifdef LATENCY_TRACE
    if (recs[1].len == 1024 - 44)
	STAMP_PKT_USER(recs[1].base, CLIENT_TOP);
#endif LATENCY_TRACE

    return AC_TOCLIENT;
}


/* ------------------------------------------------------------ */
/* Misc utility functions */

static IO_clp 
GetTX_m (
        Protocol_cl     *self )
{
    UDP_st   *st = (UDP_st *) self->st;
    io_st *txst = st->iotx->st;

    if (txst->io)
	return st->iotx;
    else
	RAISE_Protocol$Simplex();

    /* middlc bug: functions marked NEVER RETURNS should generate
     * function prototypes with NORETURN() macro. Otherwise gcc
     * complains needlessly. XXX keep gcc happy about returns: */
    return NULL;
}

static IO_clp 
GetRX_m (
        Protocol_cl     *self )
{
    UDP_st   *st = (UDP_st *) self->st;
    io_st *rxst = st->iorx->st;

    if (rxst->io)
	return st->iorx;
    else
	RAISE_Protocol$Simplex();

    /* middlc bug: functions marked NEVER RETURNS should generate
     * function prototypes with NORETURN() macro. Otherwise gcc
     * complains needlessly. XXX keep gcc happy about returns: */
    return NULL;
}

static uint32_t 
QueryMTU_m (
	Protocol_cl	*self )
{
    UDP_st	*st = (UDP_st *) self->st;

    return Protocol$QueryMTU(st->base);
}

static uint32_t 
QueryHeaderSize_m (
        Protocol_cl     *self )
{
    UDP_st *st = self->st;

    /* UDP header + the stack below us's headers */
    return sizeof(udpheader) + Protocol$QueryHeaderSize(st->base);
}

static uint32_t 
QueryTrailerSize_m (
        Protocol_cl     *self )
{
    UDP_st *st = self->st;

    /* Don't have a trailer, so it's just the trailers from below */
    return Protocol$QueryTrailerSize(st->base);
}

static void 
Dispose_m (
	Protocol_cl	*self )
{
    UDP_st	*st = (UDP_st *) self->st;

    /* XXX finished off any other derivatives */
    Protocol$Dispose(st->base);
    /* XXX rip out packet filters? */
    Heap$Free(Pvs(heap), st);
}

static void 
SetTXPort_m (
    UDP_cl  *self,
    uint16_t        port    /* IN */ )
{
    UDP_st        *st   = (UDP_st *) self->st;
    io_st         *txst = st->iotx->st;

    txst->common->destport = port;
}

static void
Retarget_m (
        UDP_cl  *self,
        const FlowMan_SAP       *rsap   /* IN */ )
{
    UDP_st	*st = self->st;
    io_st	*txst = st->iotx->st;

    if (st->overIP)
	IP$Retarget((IP_clp)st->base, &rsap->u.UDP.addr);

    /* change our dest port */
    txst->common->destport = rsap->u.UDP.port;
}

static bool_t
GetLastPeer_m (
        UDP_cl  *self,
        FlowMan_SAP     *rsap   /* OUT */ )
{
    UDP_st	*st = self->st;
    io_st	*rxst = st->iorx->st;

    /* can always tell the port */
    rsap->u.UDP.port = rxst->lastrxport;

    if (st->overIP)
    {
	/* can also know the IP address */
	IP$GetLastPeer((IP_clp)st->base, &rsap->u.UDP.addr);
    }

    return st->overIP;
}

static FlowMan_Flow
GetFlow_m (
        UDP_cl  *self )
{
    UDP_st        *st = self->st;

    if (st->overIP)
	return IP$GetFlow((IP_clp)st->base);
    else
	return NULL;  /* XXX must be a better way */
}

static FlowMan_ConnID
GetConnID_m (
        UDP_cl  *self )
{
    UDP_st        *st = self->st;

    return st->connid;
}


/* End of udp.c */
