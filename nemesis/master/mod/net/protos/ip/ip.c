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
**      ip.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Encapsulate data into IP datagrams (as per rfc791)
** 
** ENVIRONMENT: 
**
**      Network subsystem
** 
** ID : $Id: ip.c 1.2 Thu, 18 Feb 1999 15:09:28 +0000 dr10009 $
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

#include <IPMod.ih>
#include <IP.ih>

#include <Net.ih>
#include <StretchAllocator.ih>
#include <HeapMod.ih>
#include <Heap.ih>
#include <IO.ih>
#include <Protocol.ih>
#include <Ether.ih>
#include <EtherMod.ih>
#include <FlowMan.ih>

#include <sequence.h>
#include <pktcx.h>
#include <IOMacros.h>

#include <slab.c>

#define TRC(_x)

/* this turns on both RX and TX frag tracing */
//#define FRAG_TRACE

#ifdef FRAG_TRACE
#define FRTRC(x) x
#define FTTRC(x) x
#else
#define FRTRC(x)
#define FTTRC(x)
#endif

#define IP_PARANOIA

/* error codes */
#define ERR_DUP		1
#define ERR_TRIMMEDOUT	2
#define ERR_FASTESCAPE	3
#define ERR_NONCONTIG	4
#define ERR_NEEDFINAL	5

/* ------------------------------------------------------------ */

/* byte offsets from packet start */
#define IPHEAD_VERSLEN   0  /* 4 bits: 4 bits: version: header len in words */
#define IPHEAD_SERVTYPE  1  /* octet: largely ignored */
#define IPHEAD_TOTLEN    2  /* uint16:length of datagram in octets, incl head*/
#define IPHEAD_IDENT     4  /* uint16: serial number of packet, for frag purp*/
#define IPHEAD_FLAGSFRAG 6  /* 3 bits: 0xy: x=Don't Frag; y=More Frags */
#define IPHEAD_TTL       8  /* octet: time to live */
#define IPHEAD_PROTO     9  /* octet: protocol: udp=17; tcp=6 etc */
#define IPHEAD_HCHKSUM  10  /* uint16: IP chksum over header only */
#define IPHEAD_SRCIP    12  /* network IPaddr: source address */
#define IPHEAD_DESTIP   16  /* network IPaddr: destination address */

#define DEF_TTL         64  /* default time to live */
#define MAX_HEADERS     10  /* size of header mem pool, in 32-byte units */

#define MF		0x2000	/* More Fragments bit */
#define OFFMASK		0x1fff	/* frag offset */

/* Minimal header is 5 words(=20 bytes). Variable headers arn't supported */
typedef uint8_t ipheader[20];

#define MAX_FRAGS	6   /* maximum number of fragments per datagram */
#define MAX_RECS       10   /* maximum number of recs in a fragment */

/* fragments are keyed on this */
typedef struct {
    uint32_t	src_addr;
    uint32_t	dst_addr;
    uint16_t	id;
    uint8_t	proto;
} key_t;


/* This structure holds meta-data on fragments */
typedef struct _frag_t {
    uint16_t	start;		/* start offset, in bytes */
    uint16_t	end;		/* end offset, in bytes */
    bool_t	final;		/* True if this is the last fragment */
    struct _frag_t *next;	/* pointer to next fragment */
    pktcx_t	*pc;		/* packet context for this fragment */
    uint32_t	nrecs;
    IO_Rec	recs[MAX_RECS];	/* XXX fixed size */
} frag_t;




typedef struct {
    Net_IPHostAddr src;		/* IP addresses, in "network" order */
    Net_IPHostAddr dest;	/* IP addresses, in "network" order */
    uint8_t	ttl;		/* current time to live for new packets */
    uint8_t	proto;		/* protocol to use in headers */
    uint16_t	ident;		/* current ident field */
    bool_t	dont_frag;	/* set the don't fragment bit if True */
    uint32_t	offset;		/* byte offset to where IP header starts */
    uint32_t	mtu;		/* link MTU */
    /* RX uses this for reassembly */
    key_t	key;		/* key for frag currently being assembled */
    frag_t	*dgram;		/* list of fragments in reassembled datagram */
    pktcx_t	*junk;		/* packet ctxs we want to send back to client
				     because they're no use anymore */
    slaballoc_t	*fr_alloc;	/* allocator for frag_t's */
    frag_t	*rxQ;		/* queue for completely reassembled dgram */
    
} ipCom_st;


typedef struct {
    IO_cl       *io;            /* IO to receive from */
    ipCom_st    *common;
    bool_t	 rxp;           /* True if this is an RX io */
    uint32_t	 lastrxaddr;    /* only valid if RX io */
    Heap_clp	 heap;		/* heap for extra packet buffers */
    word_t	 saved_pktcx;	/* tx only: pc for datagram in progess */
} io_st;

typedef struct {
    Protocol_cl	*base;    /* protocol IP rides on */
    IO_cl	*iotx;
    IO_cl	*iorx;
    FlowMan_Flow flow;
    char	*ifname;  /* name for the last interface we're over */
    FlowMan_clp	 fman;
} ip_st;


/* prototypes */
static INLINE int rx_check(io_st *st, uint32_t *nrecs, IO_Rec *recs,
			   uint32_t max_recs, word_t *value);
static INLINE bool_t tx_encaps(io_st *st, uint32_t nrecs, IO_Rec *recs,
			       word_t value, Time_T until);
static INLINE bool_t ip_out(io_st *st, uint32_t nrecs, IO_Rec *recs,
			    word_t value, Time_T until);
static INLINE bool_t tx_bufcol(io_st *st, uint32_t max_recs, IO_Rec *recs,
			       Time_T timeout, uint32_t *nrecs, word_t *value);


/* ------------------------------------------------------------ */

/* IPMod stuff */
static IPMod_New_fn New_m;
static IPMod_Piggyback_fn Piggyback_m;
static IPMod_op modops = {New_m, Piggyback_m};
static const IPMod_cl modcl = {&modops, BADPTR};
CL_EXPORT(IPMod, IPMod, modcl);

/* IP ops */
static  Protocol_GetTX_fn               GetTX_m;
static  Protocol_GetRX_fn               GetRX_m;
static  Protocol_QueryMTU_fn            QueryMTU_m;
static  Protocol_QueryHeaderSize_fn     QueryHeaderSize_m;
static  Protocol_QueryTrailerSize_fn    QueryTrailerSize_m;
static  Protocol_Dispose_fn             Dispose_m;
static  IP_SetDF_fn                     SetDF_m;
static  IP_SetDest_fn                   SetDest_m;
static  IP_Retarget_fn                  Retarget_m;
static  IP_SetOption_fn                 SetOption_m;
static  IP_GetLastPeer_fn               GetLastPeer_m;
static  IP_GetFlow_fn           	GetFlow_m;

static IP_op ops = {GetTX_m, GetRX_m,
		    QueryMTU_m, QueryHeaderSize_m, QueryTrailerSize_m,
		    Dispose_m, SetDF_m, SetDest_m, Retarget_m, SetOption_m,
		    GetLastPeer_m, GetFlow_m};

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



static IO_clp bindIO(IOOffer_cl *io_offer, HeapMod_cl *hmod, Heap_clp *heap) 
{
    IO_clp res; 
    IOData_Shm *shm;
    
    if(io_offer != NULL) {

	if(*heap) {   /* We have some memory => bind using it */
	    Type_Any    any;
	    IDCClientBinding_clp cb;

	    shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
	    
	    SEQ_ELEM(shm, 0).buf.base = 
		HeapMod$Where(hmod, *heap, &(SEQ_ELEM(shm, 0).buf.len));
	    SEQ_ELEM(shm, 0).param.attrs  = 0;
	    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
	    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;
	    
	    cb = IOOffer$ExtBind(io_offer, shm, Pvs(gkpr), &any);
	    if(!ISTYPE(&any, IO_clp)) {
		eprintf("ip.c [bindIO]: IOOffer$ExtBind failed.\n");
		if(cb) IDCClientBinding$Destroy(cb);
		RAISE_TypeSystem$Incompatible();
	    }
	    res = NARROW (&any, IO_clp);
	} else { 
	    /* Just bind to offer and create a heap afterwards. */
	    res = IO_BIND(io_offer);
	    shm = IO$QueryShm(res); 
	    if(SEQ_LEN(shm) != 1) 
		eprintf("ip.c [bindIO]: got > 1 data areas in channel!\n");

	    /* Ignore extra areas for now */
	    *heap = HeapMod$NewRaw(hmod, SEQ_ELEM(shm, 0).buf.base, 
				   SEQ_ELEM(shm, 0).buf.len);
	}

	return res; 
    } 
    
    return (IO_cl *)NULL;
}



static IP_clp internal_new(IPMod_cl *self,
			   FlowMan_clp fman,
			   Protocol_cl *base,
			   const Net_IPHostAddr *dest,
			   const Net_IPHostAddr *src,
			   uint8_t proto,
			   uint8_t defttl,
			   FlowMan_Flow flow,
			   char *ifname,
			   Heap_clp txheap)
{
    IP_cl *cl;
    ip_st *st;
    ipCom_st *com;
    io_st *rxst;
    io_st *txst;
    IO_cl *rxio;
    IO_cl *txio;

    st = Heap$Malloc(Pvs(heap), sizeof(IP_cl) +
		                sizeof(ip_st) +
		                sizeof(ipCom_st) +
		                (2 * (sizeof(io_st) + sizeof(IO_cl))) );
    if (!st)
    {
	printf("IP.c:internal_new: out of memory\n");
	return NULL;
    }

    /* lay out memory */
    cl  = (IP_cl *)    (st + 1);
    com = (ipCom_st *) (cl + 1);
    rxst= (io_st *)    (com+ 1);
    txst= (io_st *)    (rxst+1);
    rxio= (IO_cl *)    (txst+1);
    txio= (IO_cl *)    (rxio+1);

    /* plumb it together */
    cl->op = &ops;
    cl->st = st;
    st->iotx = txio;
    st->iorx = rxio;
    st->flow = flow;
    st->ifname = ifname;
    st->fman = fman;
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
    txst->lastrxaddr = 0; /* not that this is ever going to get seen */
    txst->heap   = txheap;
    txst->saved_pktcx = 0;

    TRY
	rxst->io = Protocol$GetRX(base);
    CATCH_Protocol$Simplex()
	rxst->io = NULL;
    ENDTRY
    rxst->common = com;
    rxst->rxp    = True;
    rxst->lastrxaddr = 0;
    rxst->heap   = NULL;  /* no one should need it on the RX side */
    rxst->saved_pktcx = 0; /* no one should be using this */

    /* rest of state init */
    st->base    = base;            /* protocol under us */
    com->src.a  = src->a;          /* squirrel away the src address to use */
    com->dest.a = dest->a;         /* ditto for destination addr */
    com->proto  = proto;           /* IP protocol number */
    com->ttl    = (defttl)? defttl : DEF_TTL;
    com->ident  = 0;               /* start id tags at 0 */
    com->dont_frag = True;         /* XXX until we can handle fragments */
    com->offset = Protocol$QueryHeaderSize(base);
    com->mtu    = Protocol$QueryMTU(base);

    /* reassembly state */
    com->dgram	  = NULL;	/* no fragments yet */
    com->junk	  = NULL;	/* no junk either */
    com->rxQ	  = NULL;	/* no pending datagrams */
    com->fr_alloc = slaballoc_new(Pvs(heap), sizeof(frag_t), MAX_FRAGS+1);
    if (!com->fr_alloc)
    {
	eprintf("IP: urk! out of memory creating frag allocator\n");
	Heap$Free(Pvs(heap), st);
	return NULL;
    }

    return cl;
}


/* This function does the main work of routeing to an interface,
 * ARPing for the remote IP, get flowid & offers set up to pass to
 * EtherMod$New() */
static IP_clp
New_m(
        IPMod_cl        *self,
        FlowMan_clp     fman    /* IN */,
        const Net_IPHostAddr    *dest   /* IN */,
        Net_IPHostAddr	   	*src    /* IN */,
        const Netif_TXQoS       *txqos  /* IN */,
	IPMod_Mode	mode,
        uint8_t proto   /* IN */,
        uint8_t defttl  /* IN */,
   /* RETURNS */
        Heap_clp        *rxheap, 
        Heap_clp        *txheap)
{
    FlowMan_SAP		  sap;
    FlowMan_RouteInfo	 *NOCLOBBER ri;
    FlowMan_RouteInfoSeq *NOCLOBBER ris=NULL;
    Net_EtherAddr	 *srchw;
    Net_EtherAddr	  desthw; /* not a pointer */
    char		  buf[24];
    FlowMan_Flow	  flow;
    IOOffer_clp	          rxoffer, txoffer;
    IO_clp		  rxio, txio;
    EtherMod_clp	  ethmod;
    Ether_clp		  eth;
    char		 *ifname;

    /* if the remote addr is not 0.0.0.0, then assume this is an
     * active open, so route on destaddr.  Otherwise, route on local
     * addr, to find interface to "listen" on */
    if (dest->a)
    {
	uint32_t NOCLOBBER	addr;

	/* active route */
	sap.tag = FlowMan_PT_UDP; /* XXX or TCP, it doesn't matter */
	sap.u.UDP.addr = *dest;
	ri = FlowMan$ActiveRoute(fman, &sap);

	/* ARP for the dest address (or maybe the
	 * gateway if one is needed) */
	addr = (ri->gateway)? ri->gateway : dest->a;
	TRY {
	    FlowMan$ARP(fman, addr, &desthw);
	} CATCH_Context$NotFound(UNUSED name) {
	    addr = 0;
	} ENDTRY;
	if(!addr)
	{
	    printf("IP$New: ARP for %08x failed\n",
		   ntohl((ri->gateway)? ri->gateway : dest->a));
	    RAISE_FlowMan$NoRoute(); /* XXX not quite right failure */
	}
    }
    else
    {
	/* passive route */
	sap.tag = FlowMan_PT_UDP;
	sap.u.UDP.addr = *src;
	ris = FlowMan$PassiveRoute(fman, &sap);
	ri = &(SEQ_ELEM(ris, 0)); /* XXX just pick the first interface found */
	memset(&desthw, 0, sizeof(desthw)); /* redundant, really */
    }

    /* lookup the source MAC address */
    sprintf(buf, "svc>net>%s>mac", ri->ifname);
    srchw = NAME_FIND(buf, Net_EtherAddrP);

    /* find the interface's IP address, and set "src" to it */
    sprintf(buf, "svc>net>%s>ipaddr", ri->ifname);
    *src = *(NAME_FIND(buf, Net_IPHostAddrP));

    /* ok, now have dest and src MAC addresses.  Open a flow to the
     * device driver (assume is of native kind). */
    flow = FlowMan$Open(fman, ri->ifname, txqos, Netif_TXSort_Native, 
			&rxoffer, &txoffer);
    if (!flow)
    {
	printf("IP$New: FlowMan$Open failed\n");
	/* XXX need to think more about failure case! */
	RAISE_FlowMan$NoRoute();
    }

    /* keep a copy of the interface name we are using, so we can check
     * if it changes
     */
    ifname = strdup(ri->ifname);

    /* can now free all the routing information */
    if (dest->a)
    {
	free(ri->ifname);
	free(ri->type);
	free(ri);
    }
    else
    {
	/* XXX almost certainly leaks ifname memory */
	SEQ_FREE(ris);
    }

    /*
     * bind to the IOs and sort out some memory
     */
    {
	HeapMod_clp hmod = NAME_FIND("modules>HeapMod", HeapMod_clp);

	rxio = bindIO(rxoffer, hmod, rxheap);

	if (mode == IPMod_Mode_Split)
	{
	    /* If we're in Split mode, then get separate data areas
	     * for transmit and receive */
	    txio = bindIO(txoffer, hmod, txheap);
	}
	else
	{
	    /* Otherwise re-use the RX data area as the TX data area.
	     * Need to have done the RX side first, in order to get
	     * the permissions correct */
	    txio = bindIO(txoffer, hmod, rxheap);
	    *txheap = *rxheap;
	}
    }

    /* create the Ethernet encapsulation layer */
    ethmod = NAME_FIND("modules>EtherMod", EtherMod_clp);
    eth = EtherMod$New(ethmod, &desthw, srchw, FRAME_IPv4, rxio, txio);

    return internal_new(self, fman, (Protocol_clp)eth, dest, src, proto, 
			defttl, flow, ifname, *txheap);
}


IP_clp Piggyback_m(IPMod_cl *self,
		   Protocol_cl *base,
		   const Net_IPHostAddr *dest,
		   const Net_IPHostAddr *src,
		   uint8_t proto,
		   uint8_t defttl,
		   FlowMan_Flow flow,
		   Heap_clp txheap)
{
    return internal_new(self, NULL, base, dest, src, proto, defttl, flow, 
			NULL, /* unknown interface name => maybe not ether */
			txheap);
}


/* ------------------------------------------------------------ */

/* for debugging use */
static void UNUSED dump_IOVec(uint32_t nrecs, IO_Rec *recs)
{
    int i;

    printf("   %d IORecs:\n", nrecs);
    for(i=0; i<nrecs; i++)
	printf("     base:%p len:%d\n", (void *)recs[i].base, recs[i].len);
}

static void
DumpPkt (IO_Rec *recs, uint32_t nrecs)
{
    char buf[80];
    int len=0;

    for (; nrecs; nrecs--, recs++)
    {
	uint32_t	i;
	uint8_t    *p;

	p = (uint8_t *) recs->base;
    
	for (i = 0; i < recs->len; i++)
	{
	    len += sprintf(buf+len, "%02x ", *p++);
	    if (i % 16 == 15)
	    {
		printf("%s\n", buf);
		len=0;
	    }
	}
	if (len)
	{
	    printf("%s\n", buf);
	    len=0;
	}
	printf("\n");
	    
    }
    printf ("------\n");
}


/* ------------------------------------------------------------ */
/* queued IO implementation */

#include "../queuedIO.c"


/* ------------------------------------------------------------ */
/* IP implementation */


/* encapsulate the data within an IP header */
/* This is the top layer, which looks after segmenting the data into
 * one (or more) fragments to be transmitted by the "ip_out()" bottom
 * layer */
static INLINE bool_t tx_encaps(io_st *st, uint32_t nrecs, IO_Rec *recs,
			       word_t value, Time_T until)
{
    int payload;	/* total number of payload bytes to send */
    uint32_t offset;
    int maxpktpayload;
    int maxfstpayload;
    int fragsize;	/* payload bytes in the current fragment */
    int front_hdrsz;	/* length of our headers + those in front (ie lower) */
    int len;
    uint8_t *fullhdr;	/* base of header memory */
    int fullhdrlen;
    uint8_t *iphdr;	/* base for IP header we're building */
    IO_Rec *fr_recs;	/* fragment recs */
    uint32_t fr_nrecs;
    uint32_t fr_currec;
    int remain;		/* bytes of IO_Rec unused last time round */
    bool_t res;
    pktcx_t *pc;
    int i;

    TRC(printf("IP:tx_encaps: nrecs=%d, (%p+%d), (%p+%d)%s\n",
	       nrecs,
	       recs[0].base, recs[0].len,
	       recs[1].base, recs[1].len,
	       (nrecs > 2)? ", ..." : ""));

    if (nrecs < 1)
    {
	printf("IP: tx: no data?\n");
	return False;
    }

    /* range check */
    front_hdrsz = st->common->offset + sizeof(ipheader);
    if (recs[0].len < front_hdrsz)
    {
	printf("IP: first IORec is too small for IP header (%d < %d)\n",
		recs[0].len, front_hdrsz);
	return False;
    }

    /* work out size of datagram */
    payload = 0;
    for(i=1; i<nrecs; i++)
	payload += recs[i].len;
    /* NOTE: payload _doesn't_ include rec[0] length */

    /* if payload plus headers from layers above us > 64Kb, then it's
     * an oversize PDU */
    {
	int totdata = payload + recs[0].len - front_hdrsz;
	if (totdata > 0xffff)
	{
	    printf("IP: tx packet is too long (%d bytes), not transmitting\n",
		   totdata);
	    return False;
	}
    }

    offset = 0;  /* in bytes, not 8-byte units */

    /* work out what the max payload (in bytes) is per packet */
    /* it is the MTU minus the headers in front of us, snapped to the
     * next lowest 8-byte boundary (so the offset calculations are
     * correct) */
    maxpktpayload = (st->common->mtu - front_hdrsz) & ~7;
    /* however, the first frag to go out will include all of rec[0],
     * so this needs a special maxpayload size */
    maxfstpayload = (st->common->mtu - recs[0].len) & ~7;

    FTTRC(printf("IP:tx: front_hdrsz=%d, payload=%d, maxpktpayload=%d, "
		 "maxfstpayload=%d\n",
		 front_hdrsz, payload, maxpktpayload, maxfstpayload));

    /* "fr_currec" says which "recs" we're processing.  Indexes
     * recs[], not fr_recs[]. */
    fr_currec = 1;

    remain = 0;

    fullhdr = NULL;
    res = False;  /* keep gcc happy */
    while(payload > 0)
    {
	/* first time round, use client's header, otherwise allocate a
         * fresh one */
	if (fullhdr == NULL)
	{
	    fullhdr = recs[0].base;
	    fullhdrlen = recs[0].len;
	}
	else
	{
	    fullhdr = Heap$Malloc(st->heap, front_hdrsz);
	    if (!fullhdr)
	    {
		printf("IP: fragmentation required, but out of memory\n");
		return False;
	    }
	    fullhdrlen = front_hdrsz;
	}
	iphdr = fullhdr + st->common->offset;

	/* how much data can we send in this fragment? */
	/* If it's the head frag, the max is a little lower, since we
	 * take note of the data bytes that may go out as part of
	 * rec[0]. */
	if (offset == 0)
	    fragsize = MIN(payload, maxfstpayload);
	else
	    fragsize = MIN(payload, maxpktpayload);

	FTTRC(printf("IP:tx: fragsize=%d\n", fragsize));

	/* if we have a rec partially left over from last time,
	 * re-write the IO_Rec to reflect what we have.
	 * XXX relies on recs[] staying valid after ip_out(). */
	if (remain)
	{
	    ((char*)recs[fr_currec].base) += recs[fr_currec].len;
	    recs[fr_currec].len = remain;
	}

	/* change the client's rec[] array in-place to describe the
	 * fragment we're about to send: */
	fr_recs = &recs[fr_currec-1];
	fr_nrecs = 1;	/* we're always going to send the header rec */

	/* write the packet header desc */
	fr_recs[0].base = fullhdr;
	fr_recs[0].len  = fullhdrlen;
	/* tot up how much we're going to send */
	len = 0;
	while(len < fragsize)
	{
#ifdef IP_PARANOIA
	    if (fr_currec == nrecs)
		printf("IP:tx: dropped off end of client recs array?!?\n");
#endif
	    len += recs[fr_currec].len;
	    fr_currec++;
	    fr_nrecs++;
	}
	/* correct any overshoot */
	if (len > fragsize)
	{
	    fr_currec--;
	    remain = len - fragsize;
	    recs[fr_currec].len -= remain;
	    FTTRC(printf("IP:tx: trimmed rec %d, remain=%d\n",
			 fr_currec, remain));
	}
	else
	{
	    /* rec was used up entirely */
	    remain = 0;
	}

	/* are there any silly zero-length recs in the client request? */
	/* XXX AND: this is _such_ a hack. gag. barf. ptuh. */
	if (fr_currec < nrecs && recs[fr_currec].len == 0)
	{
	    fr_nrecs++;		/* add it to the outgoing request */
	    fr_currec++;	/* but don't consider it again */
	}

	payload -= fragsize;

	/* write frag offset and flags */
	iphdr[IPHEAD_FLAGSFRAG] = ((offset>>3) >> 8) |	/* offset hi-byte */
	    ((st->common->dont_frag)? 1 << 6 : 0) |	/*DF*/
	    ((payload > 0)? 1 << 5 : 0);		/*MF*/
	iphdr[IPHEAD_FLAGSFRAG+1] = (offset>>3) & 0xff;	/* offset lo-byte */

	/* if we're doing fragments, then substitute in our own packet
	 * context */
	if ((payload > 0) || (offset > 0))
	{
	    if (!st->heap)
	    {
		printf("IP: fragmentation required, but no txheap!\n");
		res = False;
		break;
	    }
	    /* keep the client's original packet context if this
	     * is the head fragment */
	    if (offset == 0)
	    {
#ifdef IP_PARANOIA
		if (st->saved_pktcx != 0)
		    printf("IP:tx: saved_pktcx already in use?\n");
#endif
		FTTRC(printf("IP:tx: saving client pktcx=%p\n", (void*)value));
		st->saved_pktcx = value;
	    }

	    pc = Heap$Malloc(st->heap, sizeof(pktcx_t));
	    if (!pc)
	    {
		printf("IP: fragmentation required, but out of memory on "
		       "tx heap\n");
		res = False;
		break;
	    }
	    *pc = *(pktcx_t *)value;	/* clone existing details */

	    /* mark it as special (ours), so we can recognise it when
	     * it's been transmitted. Bottom bit (bit0) is 1 for more
	     * frags, 0 on last frag. Bit1 is 1 for heads, 0 for body
	     * frags.  Clearly, "10" (ie head but no more frags) is invalid,
	     * since if we're here we must be doing fragments. */
	    pc->pktid = (((word_t)st) & ~3)    |
			((payload > 0)? 1 : 0) |
			((offset == 0)? 2 : 0);
	    FTTRC(printf("IP:tx: pktid=%p\n", (void*)pc->pktid));

	    /* initialise the refresh info */
	    pc->ri_nrecs = fr_nrecs;
	    for(i=0; i<fr_nrecs; i++)
		pc->ri_recs[i] = fr_recs[i];

	    value = (word_t)pc;
	}

	/* XXX hack: the head frag includes headers beyond ours as well,
	 * so the offset is correspondingly slightly larger */
	if (offset == 0)
	    offset = recs[0].len - front_hdrsz;
	offset += fragsize;

	res = ip_out(st, fr_nrecs, fr_recs, value, until);
	if (!res)
	{
	    printf("IP:tx: error from ip_out\n");
	    break;
	}
    }

    /* step the IP id on one, ready for next time we're called */
    st->common->ident++;

    return res;
}
    

/* Complete the IP header, and send the data out on "st->io". Needs the total
 * "size" passed in for use in the IP header.  Returns the return code
 * from "IO$PutPkt()" */
static INLINE bool_t ip_out(io_st *st, uint32_t nrecs, IO_Rec *recs,
			    word_t value, Time_T until)
{
    int i;
    int size;
    uint32_t sum;
    uint8_t *hdr;

    TRC(printf("IP:ip_out: nrecs=%d, (%p+%d), (%p+%d)\n",
	       nrecs,
	       recs[0].base, recs[0].len,
	       recs[1].base, recs[1].len));

    size=0;
    for(i=0; i<nrecs; i++)
	size += recs[i].len;
    /* take off encapsulation overhead */
    size -= st->common->offset;

    /* fill in the rest of the header */
    hdr = ((char *)recs[0].base) + st->common->offset;
    hdr[IPHEAD_VERSLEN] = (4 << 4) | 5; /* IPv4, header len=5 32bit words */
    hdr[IPHEAD_SERVTYPE]= 0;            /* nothing special */

    hdr[IPHEAD_TOTLEN] = size >> 8;
    hdr[IPHEAD_TOTLEN+1]=size & 0xff;
    hdr[IPHEAD_IDENT]  = st->common->ident >> 8;
    hdr[IPHEAD_IDENT+1]= st->common->ident & 0xff;
    /* FLAGSFRAG filled in by tx_encaps */
    hdr[IPHEAD_TTL]    = st->common->ttl;
    hdr[IPHEAD_PROTO]  = st->common->proto;
    hdr[IPHEAD_HCHKSUM]  = 0;  /* before calc of hec */
    hdr[IPHEAD_HCHKSUM+1]= 0;
    *(uint32_t *)&(hdr[IPHEAD_SRCIP])  = st->common->src.a;
    *(uint32_t *)&(hdr[IPHEAD_DESTIP]) = st->common->dest.a;
    /* XXX options go here */

    /* calculate IP checksum. Magic pilfered from ebsdk boot rom source: */
    /* "To calculate the one's complement sum calculate the 32 bit
     *  unsigned sum and add the top and bottom words together."  */
    sum = 0;
    for(i=0; i < sizeof(ipheader); i+=2)
    {
	sum += (hdr[i] << 8) + hdr[i+1];
    }
    sum = (sum & 0xffff) + (sum >> 16);
    sum += (sum >> 16);  /* Add in possible carry. */
    if (sum == 0xffff)
	sum = 0;

    /* Take 1s complement and slap it in */
    sum = ~sum;
    hdr[IPHEAD_HCHKSUM]   = sum >> 8;
    hdr[IPHEAD_HCHKSUM+1] = sum & 0xff;

    return IO$PutPkt(st->io, nrecs, recs, value, until);
}


/* Called when a client wants to collect buffers of transmitted packet */
static INLINE bool_t tx_bufcol(io_st *st, uint32_t max_recs, IO_Rec *recs,
			       Time_T timeout, uint32_t *nrecs, word_t *value)
{
    bool_t res;
    pktcx_t *pc;

    TRC(printf("IP:tx_bufcol: max_recs=%d, recs=%p\n", max_recs, recs));

    while(1)
    {
	res = IO$GetPkt(st->io, max_recs, recs, timeout, nrecs, value);
	if (!res)
	    return False;

	/* if we don't have a packet context, then we can do nothing special */
	if (!*value)
	    return True;

	pc = (void*)*value;

	/* if this isn't one of our buffers, just pass it on up */
	if ((pc->pktid & ~3) != (((word_t)st) & ~3))
	    return True;

	FTTRC(printf("IP:tx_bufcol: got frag buf, pktid=%p\n",
		     (void*)pc->pktid));

	/* this is one of the buffers we created for fragment purposes:
	 * don't return the memory to the client, but recycle it
	 * ourselves. */

	/* work out what kind of packet this is */
	{
	    int type = pc->pktid & 3;
#ifdef IP_PARANOIA
	    if (type == 2)
		printf("IP:tx_bufcol: unexpected packet type %x, "
		       "can't happen\n", pc->pktid);
#endif

	    /* if it's not a head frag, we can free the header memory */
	    if ((type & 2) == 0)
	    {
		FTTRC(printf("IP:tx_bufcol: freeing header at %p\n",
			     pc->ri_recs[0].base));
		Heap$Free(st->heap, pc->ri_recs[0].base);
	    }

	    /* we can always free the packet context, since it's ours */
	    Heap$Free(st->heap, pc);

	    /* if it's the last fragment, we can return the original
	     * transaction to the client */
	    if ((type & 1) == 0)
	    {
		int i;

		FTTRC(printf("IP:tx_bufcol: final frag\n"));

		/* returned "value" is the saved one */
		*value = st->saved_pktcx;
		st->saved_pktcx = 0;
		pc = (void*)(*value);

		/* returned "recs" is the original one (only if the client
		 * supplied a packet context, however) */
		if (pc)
		{
		    if (pc->ri_nrecs > max_recs) /* URK: not enough room! */
		    {
			printf("IP:tx_bufcol: not enough room in client recs! "
			       "(%d > %d)\n", pc->ri_nrecs, max_recs);
			*nrecs = pc->ri_nrecs;
			return False;
			/* XXX should really stock this up for client to
			 * try again later, though. */
		    }
		    FTTRC(printf("IP:tx_bufcol: restoring client recs\n"));
		    for(i=0; i < pc->ri_nrecs; i++)
			recs[i] = pc->ri_recs[i];
		    *nrecs = pc->ri_nrecs;
		}
		return True;
	    }
	}
	/* back to the top to collect another fragment */
    }

}




/* drop "bytes" off the front of "fr" */
static void chop_front(frag_t *fr, uint32_t bytes)
{
    uint32_t chop;
    uint32_t r = 0;

    FRTRC(eprintf("IP: chop_front of %d bytes ", bytes));

    while(bytes)
    {
	if (r == fr->nrecs)
	{
	    printf("IP: chop_front: out of data to chop\n");
	    return;
	}
	FRTRC(eprintf("@"));
	chop = MIN(bytes, fr->recs[r].len);
	fr->recs[r].len -= chop;
	((char *)(fr->recs[r].base)) += chop;
	fr->start += chop;
	bytes -= chop;
	r++;
    }
    FRTRC(eprintf("\n"));
}


/* drop "bytes" off the back of "fr" */
static void chop_end(frag_t *fr, uint32_t bytes)
{
    uint32_t chop;
    int r = fr->nrecs-1;

    FRTRC(eprintf("IP: chop_end of %d bytes ", bytes));

    while(bytes)
    {
	if (r == -1)
	{
	    printf("IP: chop_end: out of data to chop\n");
	    return;
	}
	FRTRC(eprintf("@"));
	chop = MIN(bytes, fr->recs[r].len);
	fr->recs[r].len -= chop;
	fr->end -= chop;
	bytes -= chop;
	r--;
    }
    FRTRC(eprintf("\n"));
}


/* clear the frags from st->common->dgram, chaining their packet
 * contexts onto st->common->junk */
static void dgram2junk(io_st *st)
{
    frag_t *f = st->common->dgram;
    frag_t *n;
    pktcx_t *pc;

    while(f)
    {
	/* slap f->pc onto the front of st-common->junk */
	pc = f->pc;
	while(pc && pc->next)
	    pc = pc->next;
	if (pc)
	{
	    pc->next = st->common->junk;
	    st->common->junk = f->pc;
	}

	n = f->next;
	slab_free(st->common->fr_alloc, f);
	f = n;
    }
}

/* pinch "newfr" out of the frag list and recycle its buffers */
/* Also has the side effect of clearing out st->common->junk (done by 
 * the queuedIO stuff) */
#define recycle_newfr() \
do {									\
    PCTRC(eprintf("recycle_newfr\n"));					\
    if (prev)								\
	prev->next = newfr->next;	/* out of the frag list */	\
    else								\
	st->common->dgram = newfr->next;				\
									\
    *value = (word_t)newfr->pc;						\
    *nrecs = 0;								\
									\
    slab_free(st->common->fr_alloc, newfr);				\
    return AC_DITCH;							\
} while(0)





/* see if we can do anything with this buffer to aid our reassembly */
static INLINE int reassemble(io_st *st,
			     uint8_t *hdr,
			     int flagsOffset,
			     int hlen,
			     uint32_t *nrecs,
			     IO_Rec *recs,
			     uint32_t max_recs,
			     word_t *value)
{
    key_t key;
    frag_t *newfr;
    frag_t *prev;
    frag_t *fr;
    bool_t contig;
    int bytenum;
    int i;

    key.src_addr = *(uint32_t *)(hdr + IPHEAD_SRCIP);
    key.dst_addr = *(uint32_t *)(hdr + IPHEAD_DESTIP);
    key.proto = *(uint8_t *)(hdr + IPHEAD_PROTO);
    key.id = *(uint16_t *)(hdr + IPHEAD_IDENT);

    /* get a frag_t to describe this packet */
    newfr = slab_alloc(st->common->fr_alloc);
    if (!newfr)
    {
	printf("IP: oops, no frag_t's left, dropping packet\n");
	return AC_DITCH;
    }
    
    newfr->start = (flagsOffset & OFFMASK) << 3;
    newfr->end = newfr->start +
	ntohs(*(uint16_t *)(hdr + IPHEAD_TOTLEN)) - hlen;
    if (newfr->start > newfr->end) /* wrapped around calculating the end */
    {
	printf("IP: warning: oversize fragment (srcIP=%08x), ditching\n",
	       ntohl(key.src_addr));
	slab_free(st->common->fr_alloc, newfr);
	return AC_DITCH;
    }

    FRTRC(eprintf("IP: flagsOffset=%x\n", flagsOffset));
    newfr->final = (flagsOffset & MF)? False : True;
    newfr->next = NULL;
    newfr->pc = *(pktcx_t**)value;
    newfr->nrecs = *nrecs;
    for(i=0; i<*nrecs; i++)
	newfr->recs[i] = recs[i];

    /* is this a head? */
    if (!(flagsOffset & OFFMASK))
    {
	FRTRC(eprintf("IP: got head\n"));
	/* if we were already reassembling something else grumble a
	 * little */
	if (st->common->dgram)
	{
	    printf("IP: warning: reassembly interrupted\n");
	    /* Need to release the partially reassembled recs.  Put
	     * their packet contexts onto the junk queue, and return
	     * them to the client.  Any AC_WAIT from now on needs to
	     * be conditional on the contents of the junk queue. */
	    dgram2junk(st);
	    st->common->dgram = NULL;
	    /* we don't slab_free() newfr since we're going to use it */
	}

	/* fill in the key */
	st->common->key = key;
    }
    else
    {
	/* not a head, so would like to clear recs[0].len to zero.
	 * problem is that the last 8 bytes of recs[0] (usually
	 * UDP etc) are infact our payload. So we just trim the
	 * header. */
	((char*)newfr->recs[0].base) += (st->common->offset + hlen);
	newfr->recs[0].len -= (st->common->offset + hlen);

	/* check the key matches */
	if ((key.id != st->common->key.id) ||
	    (key.src_addr != st->common->key.src_addr) ||
	    (key.proto != st->common->key.proto) ||
	    (key.proto != st->common->key.proto))
	{
	    printf("IP: warning: mismatched frag ID, dropping\n");
	    /* The only thing that can be done is to ditch the
	     * packet and hope a head comes in shortly to resync us. */
	    slab_free(st->common->fr_alloc, newfr);
	    return AC_DITCH;
	}
    }

    /* find where this fragment goes */
    /* (while we're at it, check if what we have so far is contiguous) */
    contig = True;
    bytenum = 0;
    fr = st->common->dgram;
    prev = NULL;
    while(fr && (fr->start < newfr->start))
    {
	if (fr->start != bytenum)
	    contig = False;	/* not what we expected, so not contiguous */
	bytenum = fr->end;
	prev = fr;
	fr = fr->next;
    }
    /* link in the new frag */
    if (prev)
	prev->next = newfr;
    else
	st->common->dgram = newfr;
    newfr->next = fr;

    /* situation is now: "newfr" linked into main fragment list, and
     * if "prev" is non-NULL, then "newfr" starts after "prev". It may
     * start at the same byte as "fr", of course. */

    FRTRC(eprintf("IP: linked in: %p <<%p>> %p\n", prev, newfr, newfr->next));

    /* NB: be very careful with the boundary conditions here, it's
     * easy to get off-by-one byte errors. For more details, see
     * and1000 lab book 3, entry for 2/2/98 */

    /* does "newfr" fit complete within "prev":
     *   .----------------.
     *   |   prev         |
     *   `----------------'
     *        .-------.
     *        | newfr |
     *        `-------'
     * If this is the case, "newfr" can be recycled as junk.
     */
    if (prev && (prev->end > newfr->end))
	recycle_newfr();

    /* trim front, if needed:
     *   .---------.
     *   |   prev  |
     *   `---------'
     *          .--:----------.
     *          |  :  newfr   |
     *          `--:----------'
     *             ^ cut along the dotted line
     */
    if (prev && (prev->end > newfr->start))
	chop_front(newfr, prev->end - newfr->start);

    /* trim back, if needed:
     *------.        .---------.
     * prev |        |   fr    |
     *------'        `---------'
     *      .--------:---.
     *      |  newfr :   |
     *      `--------:---'
     *               ^ cut along the dotted line
     */
    if (fr && (fr->start < newfr->end))
	chop_end(newfr, newfr->end - fr->start);

    /* XXX (1) this does not handle the case where "newfr" extends
     * past "fr" -- it will overly chop "newfr":
     * -----.        .------.
     * prev |        |  fr  |
     * -----'        `------'
     *      .--------:----------.
     *      | newfr  :  chomp!  |
     *      `--------:----------'
     *                      |<->|  oops!  We may have needed that part
     */
    if (fr && (fr->end < newfr->end))
    {
	printf("IP: warning: new frag(%d->%d) extends over old frag(%d->%d). "
	       "Reassembly will probably fail :(\n",
	       (int)newfr->start, (int)newfr->end,
	       (int)fr->start, (int)fr->end);
    }

    /* did it get trimmed into oblivion? */
    if (newfr->start == newfr->end)
	recycle_newfr();

    /* take account of the new frag on whether we're contiguous so far */
    if (newfr->start != bytenum)
	contig = False;
    bytenum = newfr->end; /* XXX not quite right, given (1) above */


    /*
     * Have we finished reassembly of the entire datagram?
     */

    /* fast escape if the stuff before here isn't even contiguous */
    if (!contig)
    {
	FRTRC(eprintf("IP: not contig, waiting\n"));
	return AC_WAIT;
    }

    /* just need to check the stuff after this is both contig and 
     * last frag has "final" set */
    FRTRC(eprintf("IP: do we have all frags?\n"));
    prev = newfr;
    while(fr)
    {
	FRTRC(eprintf("IP: expecting %d, fr=%p[%d:%d]%s\n",
		     bytenum, fr, (int)fr->start, (int)fr->end,
		     fr->final?"" : "MF"));
	if (bytenum != fr->start)
	{
	    FRTRC(eprintf("IP: nope\n"));
	    return AC_WAIT;
	}
	bytenum = fr->end;
	prev = fr;
	fr = fr->next;
    }
    FRTRC(eprintf("IP: contig, checking for final frag...\n"));

    /* do we have a "final" frag? */
    if (prev->final)
    {
        FRTRC(eprintf("IP: yup, delivering\n"));
	if (st->common->rxQ)
	{
	    eprintf("IP: rxQ not empty, can't happen\n");
	    ntsc_dbgstop();
	}
	st->common->rxQ = st->common->dgram;
	st->common->dgram = NULL;
	return AC_DELIVER;
    }
    else
    {
	FRTRC(eprintf("IP: waiting for final frag\n"));
	return AC_WAIT;
    }
}



/* check IP headers on received data */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * Task is to check IP headers
 * Return AC_TOCLIENT if looks ok, AC_DITCH to try again with the same buffers,
 * or AC_WAIT to try again with fresh buffers
 */
static INLINE int rx_check(io_st *st, uint32_t *nrecs, IO_Rec *recs,
			   uint32_t max_recs, word_t *value)
{
    int i;
    uint32_t sum;
    int hlen;
    int flagsOffset;
    uint8_t *hdr;

    if (*nrecs < 1)
    {
	printf("IP: programmer error: rx_check() called with nrecs=%d\n",
		*nrecs);
	return AC_DITCH;
    }

    TRC(printf("IP: rx base:%p len:%d\n", recs[0].base, recs[0].len));

    if (recs[0].len < st->common->offset + sizeof(ipheader))
    {
	printf("IP: warning: short first IORec (%d < %d), dropping packet\n",
		recs[0].len, st->common->offset + sizeof(ipheader));
	return AC_DITCH;
    }

    hdr = ((char *)recs[0].base) + st->common->offset;

    /* header len in bytes */
    hlen = (hdr[IPHEAD_VERSLEN] & 0xf) * 4;

    /* XXX needs to come out when options are in */
    if (hlen != 20)
    {
	printf("IP: warning: recvd packet with header size=%d bytes. "
		"Looks like some IP options, dropping it\n", hlen);
	return AC_DITCH; /* otherwise everyone else will get confused */
    }

    /* Verify IP checksum. Magic pilfered from ebsdk boot rom source: */
    /* "To calculate the one's complement sum calculate the 32 bit
     *  unsigned sum and add the top and bottom words together."  */

    /* XXX: this code is subly different from the calc checksum code:
     * one of them is probably wrong when carry is concerned. I put my
     * money on this one being bogus. and1000 */
    sum = 0;
    for(i=0; i < hlen; i+=2)
    {
	sum += (hdr[i] << 8) + hdr[i+1];
    }
    sum = (sum & 0xffff) + ((sum >> 16) & 0xffff);
    sum += (sum >> 16);  /* Add in possible carry. */
    if (sum != 0xffff)
    {
	printf("IP: warning: recvd packet with bad IP header checksum %#x, "
		"dropping it\n",
		sum);
	DumpPkt(recs, *nrecs);
	return AC_DITCH;
    }

    {
	uint32_t destip = *(uint32_t *)(hdr + IPHEAD_DESTIP);
	if (st->common->src.a && /* don't check if our address is 0.0.0.0 */
	    destip != st->common->src.a &&
	    destip != 0xffffffff /* ignore bcast */)
	{
	    printf("IP: warning: recvd packet "
		   "with dest != my addr (%x != %x)!\n",
		   ntohl(destip),
		   ntohl(st->common->src.a));
	    return AC_DITCH;
	}
    }


    /* note who we last rxed from */
    st->lastrxaddr = *(uint32_t *)(hdr + IPHEAD_SRCIP);


    /* If its a fragment, try re-assembly */
    flagsOffset = ntohs(*(uint16_t *)(hdr + IPHEAD_FLAGSFRAG));
    if (flagsOffset & (MF|OFFMASK))
	return reassemble(st, hdr, flagsOffset, hlen, nrecs, recs, max_recs,
			  value);

    return AC_TOCLIENT;
}

/* ------------------------------------------------------------ */
/* Misc utility functions */

static IO_clp 
GetTX_m (
        Protocol_cl     *self )
{
    ip_st   *st = (ip_st *) self->st;
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
    ip_st   *st = (ip_st *) self->st;
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

uint32_t QueryMTU_m(Protocol_cl *self)
{
    ip_st *st;

    st = self->st;

    return Protocol$QueryMTU(st->base);
}    

static uint32_t 
QueryHeaderSize_m (
        Protocol_cl     *self )
{
    ip_st *st = self->st;

    /* IP header + the stack below us's headers */
    return sizeof(ipheader) + Protocol$QueryHeaderSize(st->base);
}

static uint32_t 
QueryTrailerSize_m (
        Protocol_cl     *self )
{
    ip_st *st = self->st;

    /* Don't have a trailer, so it's just the trailers from below */
    return Protocol$QueryTrailerSize(st->base);
}

void Dispose_m(Protocol_cl *self)
{
    ip_st *st;

    st = self->st;

    /* XXX finished off any other derivatives */
    Protocol$Dispose(st->base);
    /* XXX my packet header space */
    Heap$Free(Pvs(heap), st);
}

static bool_t
SetDF_m (
        IP_cl   *self,
        bool_t  df   /* IN */ )
{
    ip_st *st   = (ip_st *) self->st;
    io_st *txst = st->iotx->st;
    bool_t old;

    old = txst->common->dont_frag;
    txst->common->dont_frag = df;

    return old;
}

static void 
SetOption_m (
        IP_cl   *self )
{
    /* XXX not even got an interface spec for this yet. */
    return;
}

static void
SetDest_m(IP_cl *self, const Net_IPHostAddr *destIP)
{
    ip_st *st   = self->st;
    io_st *txst = st->iotx->st;

    txst->common->dest.a = destIP->a;
}

static void
Retarget_m (
        IP_cl   *self,
        const Net_IPHostAddr    *destIP /* IN */ )
{
    ip_st *st = self->st;
    io_st *txst = st->iotx->st;
    FlowMan_SAP		sap;
    FlowMan_RouteInfo	* NOCLOBBER ri;
    Net_EtherAddr	desthw; /* not a pointer */
    uint32_t NOCLOBBER	addr;


    /* do we know if we're actually running over ethernet? */
    if (!st->ifname || !st->fman)
    {
	printf("IP$Retarget: unknown support layer\n");
	RAISE_FlowMan$NoRoute();
    }

    if (txst->common->dest.a == destIP->a)
    {
	/* We're already pointing at this destination - don't bother
           rerouting */
	return;
    }

    /* set outgoing address */
    txst->common->dest.a = destIP->a;

    /* active route */
    sap.tag = FlowMan_PT_UDP; /* XXX or TCP, it doesn't matter */
    sap.u.UDP.addr = *destIP;
    ri = FlowMan$ActiveRoute(st->fman, &sap);

    /* check the interface required isn't a different one */
    if (strcmp(ri->ifname, st->ifname))
    {
	printf("IP$Retarget: other interface required (using %s, need %s)\n",
	       st->ifname, ri->ifname);
	free(ri->ifname);
	free(ri->type);
	free(ri);
	RAISE_FlowMan$NoRoute();
    }


    /* ARP for the dest address (or maybe the
     * gateway if one is needed) */
    addr = (ri->gateway)? ri->gateway : destIP->a;
    TRY {
	FlowMan$ARP(st->fman, addr, &desthw);
    } CATCH_Context$NotFound(UNUSED name) {
	addr = 0;
    } ENDTRY;
    if (!addr)
    {
	printf("IP$Retarget: ARP for %08x failed\n", ntohl(addr));
	RAISE_FlowMan$NoRoute(); /* XXX not quite right failure */
    }
    
    free(ri->ifname);
    free(ri->type);
    free(ri);

    /* set the Ethernet destination address */
    Ether$SetDest((Ether_clp)st->base, &desthw);
}


static void
GetLastPeer_m(
        IP_cl   *self,
        Net_IPHostAddr  *peeraddr       /* OUT */ )
{
    ip_st *st = self->st;
    io_st *rxst = st->iorx->st;

    peeraddr->a = rxst->lastrxaddr;
}

static FlowMan_Flow
GetFlow_m(
        IP_cl   *self )
{
    ip_st *st = self->st;

    return st->flow;
}


/* End of ip.c */
