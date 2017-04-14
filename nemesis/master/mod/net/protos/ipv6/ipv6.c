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
**      ipv6.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Encapsulate data into IP v6 datagrams (as per rfc1883)
** 
** ENVIRONMENT: 
**
**      Network subsystem
** 
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>

#include <IPV6Mod.ih>
#include <IPV6.ih>

#include <Net.ih>
#include <StretchAllocator.ih>
#include <HeapMod.ih>
#include <Heap.ih>
#include <IO.ih>
#include <Protocol.ih>
#include <Ether.ih>
#include <EtherMod.ih>
#include <FlowMan.ih>

#define TRC(_x) 

/* add more (slower) checks to buffer handling code */
#define BUFFER_PARANOIA

/* ------------------------------------------------------------ */

/* byte offsets from header start of various headers */

/* 
** First, we look at the IP V6 header.
** It looks a lot like this:
**

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |Version| Prio. |                   Flow Label                  |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         Payload Length        |  Next Header  |   Hop Limit   |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                         Source Address                        +
   |                                                               |
   +                                                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                      Destination Address                      +
   |                                                               |
   +                                                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
**
** The 'next header' field contains a number identifying the following
** header; this can be UDP or TCP, in which case things are dandy, or
** might be one of the many 'new' IP V6 headers which we deal with below.
** 
**
*/
#define IPHDR_LEN      40  /* (fixed) size of an IP V6 header in octets */ 
#define IPHDR_VERSPRI   0  /* 4 bits | 4 bits;  version (=6) | 'priority' */
#define IPHDR_FLOW      1  /* 3 octets; identifies the ipv6 'flow' */
#define IPHDR_LENGTH    4  /* uint16: length of payload (i.e. after IPV6 header) */
#define IPHDR_NEXTHDR   6  /* octet: specifies the type of the next header */
#define IPHDR_HOP_MAX   7  /* octet: maximum number of hops which may be taken */
#define IPHDR_SRCIPV6   8  /* 16 octet source IP V6 address. */
#define IPHDR_DESTIPV6 24  /* 16 octet destination IP V6 address. */

#define IPV6_VERSION    6  /* version of this IP ;-) */ 
#define IPV6_ADDRLEN   16  /* length in octets of an IP V6 address */


/* Priority values & meanings */
#define PRI_UNKNOWN     0  /* uncharacterised traffic */
#define PRI_FILLER      1  /* filler traffic such as netnews. */
#define PRI_UNATTEND    2  /* unattend data transfer (e.g. email) */
#define PRI_RSVD1       3  /* reserved. */
#define PRI_ATTEND      4  /* attended data transfer (e.g. ftp, nfs) */
#define PRI_RSVD2       5  /* reserved. */
#define PRI_INTERACT    6  /* interactive traffic (e.g. telnet, X) */
#define PRI_CONTROL     7  /* internet control traffic (e.g. ICMP/xGP) */
#define PRI_RT(_pri) (8+(_pri)) /* pri levels 0 to 7 for real time traffic */

/* Defaults for some of the above */
#define DEF_PRIORITY   PRI_UNKNOWN 
#define DEF_HOP_MAX    16
#define DEF_FLOW       0

/* 
** Next comes the Hop-by-hop options header.
** This header looks rather like this:
**

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Next Header  |  Hdr Ext Len  |                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
   |                                                               |
   .                                                               .
   .                            Options                            .
   .                                                               .
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

** The 'next header' field specifies the next header, which can be a futher
** IP V6 header or a ULP header. The header extension length is the length in
** octets of the header, excluding the first 8 octets.
*/

#define HOP_HDR_ID      0 /* value used to specify that next header is hop-by-hop */
#define HOP_NEXTHDR     0 /* octet: specifies the type of the next header */
#define HOP_EXTLEN      1 /* octet: length of this header excluding first 8 octets */
#define HOP_OPTIONS     2 /* variable len (min 6 octets) TLV encoded options */
#define HOP_LEN(_hptr)  (_hptr[1]+8)  /* macro for getting len; hptr is a (char *) */

/* 
** In addition to the 'standard' options, there is an addition hop-by-hop option: 
**
   Jumbo Payload option  (alignment requirement: 4n + 2)

                                       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                                       |      194      |Opt Data Len=4 |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     Jumbo Payload Length                      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

**
** This option is used to send IP V6 datagrams with a length of > 65536 octets (since
** the IP header length field is only 16-bits). To use it, one specifies a value of
** zero in the IP header (octets 4&5). This means that the the 32-bit length in the 
** jumbo-payload is used instead. 
** Since implementations will want to find this out rather early on in their processing
** of the datagram, its probably a good idea to have this option, if it's being used,
** as the first option, and the hop-by-hop option header as the first option header. 
*/
#define JUMBO_OPTID  194 /* octet value identifying the jumbo payload option */
#define JUMBO_OPTLEN   4 /* length, in octets, of the jumbo payload option data */


/*
** Another header is the Routing header. This can be used for similar purposes
** as IPv4's 'source route' options. 
** It looks like this:
**
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Next Header  |  Hdr Ext Len  |  Routing Type | Segments Left |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   .                                                               .
   .                       type-specific data                      .
   .                                                               .
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

** The 'next header' & 'hdr ext len' fields are as before. 
** The routing type specifies the variant of routing header- current only 
** one type is defined, with a value of 0. 
** Segments left specifies the number of routing segements still to be 
** processed (i.e. (self + #intermediate hosts + ultimate dest)-1).
**
** The type-specific data depends on the routing type, but since there's only 
** one routing type atm, we describe it below.
**

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   Reserved    |             Strict/Loose Bit Map              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                           Address[1]                          +
   |                                                               |
   +                                                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   .                               .                               .
   .                               .                               .
   .                               .                               .
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   +                                                               +
   |                                                               |
   +                           Address[n]                          +
   |                                                               |
   +                                                               +
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

** The 'strict/loose' bitmap indicates (left to right) for each segement of the 
** route if the next destination must be a neighbour (strict,1) or not (loose,0).
** This is followed by N IPV6 addresses, each 128bits long, where N==segments left,
** and must be <= 23.
** NB: we do _not_ use the number of segments left to compute the length of this 
** header - instead we use the hdr ext len field. This is because the header is 
** not modified in length by any of the intermediate nodes.
*/

#define ROUTE_HDR_ID   43 /* value used to specify that next header is routing */
#define ROUTE_NEXTHDR   0 /* octet: specifies the type of the next header */
#define ROUTE_EXTLEN    1 /* octet: length of this header excluding first 8 octets */
#define ROUTE_TYPE      2 /* octet: type of routing header; for now must be 0 */
#define ROUTE_SEGSLEFT  3 /* octet: number of segments left; 0 <= SEGSLEFT <= 23 */
#define ROUTE_RSVD      4 /* octet: reserved; should be inited to zero; ignored later */
#define ROUTE_BITMAP    5 /* 24 bits; loose/strict mask for addresses */
#define ROUTE_ADR(_hptr, _n) (_hptr[8]+(_n<<4)) /* computes offsets of addr N, 0<=N */
#define ROUTE_LEN(_hptr)     (_hptr[1]+8)  /* computes hdr len; hptr is a (char *) */


/*
** An important header now (!) - the fragment header. 
** Unlike IPV4, fragmentation in IPV6 is only performed by source nodes, i.e. us. 
** So we have to deal with the below:
**

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Next Header  |   Reserved    |      Fragment Offset    |Res|M|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Identification                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

** The next header field is as before, but there is no header ext len in 
** this case, since the fragment header is a fixed size (8 bytes). 
** The rsvd field is inited to zero and subsequently ignored.
** The fragment offset is similar to IPV4's- it specifies a 13-bit number which
** determines the offset (in octets) of the data in this datagram relative to the
** start of the _fragmentable_ part of the datagram, where the 'fragmentable' part
** is all the datagram saving (a) the IPV6 header, and (b) any other optional headers
** which should be processed in the case of all fragments.
** The next 2-bits are reserved (SBZ), and the final bit is the 'more fragments' 
** flag; a zero means last fragment.
** The 32-bit identification field is a number assigned to all fragments from the 
** same datagram and should be 'unique enough'; that is, there should not be fragments
** from the same host but from different datagrams alive at the same time.
*/

#define FRAG_HDR_ID    44 /* value used to specify that next header is fragment */
#define FRAG_NEXTHDR    0 /* octet: specifies the type of the next header */
#define FRAG_RSVD       1 /* octet: inited to 0; ignored later */
#define FRAG_OFFSET     2 /* 13 bit number specifying number of octets offset */
#define FRAG_OFF(_hdr)  (((uint16_t *)_hdr)[2]>>3) /* handy macro */
#define FRAG_LAST(_hdr) (!(_hdr[3]&1)) /* is this the last frag? _hdr is a (char *) */
#define FRAG_IDENT      4 /* uint32_t; unique identifier for all frags in this dgram */

/*
** The next optional header is a pretty useless one for now; the destination
** options header. It looks like this:
**

   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Next Header  |  Hdr Ext Len  |                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
   |                                                               |
   .                                                               .
   .                            Options                            .
   .                                                               .
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

** The next header and hdr ext len fields are as usual, and the options are
** TLV encoded standard options. 
*/
#define DEST_HDR_ID    60 /* value used to specify that next header is desintation */
#define DEST_NEXTHDR    0 /* octet: specifies the type of the next header */
#define DEST_EXTLEN     1 /* octet: length of this header excluding first 8 octets */
#define DEST_OPTIONS    2 /* variable len (min 6 octets) TLV encoded options */
#define DEST_LEN(_hptr) (_hptr[1]+8)  /* macro for getting len; hptr is a (char *) */

/* 
** Finally, we have the special 'no next header' identifier; this value, when placed
** in the 'next header' field of any of the above indicates that nothing more follows.
** Pretty damn useful, eh?
*/
#define NO_HDR_ID     59 /* value used to specify that no more headers follow */


/* The rest of the protocol numbers we are concerned with are found in protos.h */
#include <protos.h>






typedef uint8_t ipv6header[40];
typedef uint8_t ipv6addr[16];

typedef struct {
    ipv6addr    src;   /* IP addresses, in network order */
    ipv6addr    dest;  /* IP addresses, in network order */
    uint8_t	pri;   /* current priority for datagrams */
    uint8_t	hmax;  /* current hop max for datagrams */
    uint8_t	proto; /* protocol to use in headers */
    uint32_t	flow;  /* current flow field for frags */
    uint32_t	ident; /* current ident field for frags */
    /* header memory handling */
#define MAX_HEADERS     10  /* size of header mem pool */
    ipv6header	*freestack[MAX_HEADERS]; /*stack of pointers to free headers*/
    uint32_t	sp;
    ipv6header	*header_base;   /* pointer to header pool start */
} IPV6Com_st;


typedef struct {
    IO_cl	*io;   /* IO to receive from */
    IPV6Com_st	*common;
    bool_t	rxp;   /* True if this is an RX io */
} io_st;


typedef struct {
    Protocol_cl	*base; /* protocol IPV6 rides on */
    Heap_clp	heap;  /* heap the rest of stack will use for data & headers */
    IO_cl	*iotx;
    IO_cl	*iorx;
} ipv6_st;


/* prototypes */
static uint32_t strip(io_st *st, uint32_t nrecs, IO_Rec *recs);
static uint32_t encaps(io_st *st, uint32_t nrecs,
		       IO_Rec *recs, IO_Rec *newrecs);
static uint32_t rescue(io_st *st, uint32_t max_recs, uint32_t nrecs,
		       IO_Rec *recs, IO_Rec *newrecs);


/* convert an address into a header base word_t */
#define HDRBASE(p) ((word_t)(p) & ~((word_t)0x1f))

/* ------------------------------------------------------------ */

/* IPV6Mod stuff */
static IPV6Mod_New_fn New_m;
static IPV6Mod_Piggyback_fn Piggyback_m;
static IPV6Mod_op modops = {New_m, Piggyback_m};
static const IPV6Mod_cl modcl = {&modops, BADPTR};
CL_EXPORT(IPV6Mod, IPV6Mod, modcl);

/* IPV6 ops */
static  Protocol_GetTX_fn               GetTX_m;
static  Protocol_GetRX_fn               GetRX_m;
static  Protocol_QueryMTU_fn            QueryMTU_m;
static  Protocol_QueryStackDepth_fn     QueryStackDepth_m;
static  Protocol_Dispose_fn             Dispose_m;
static IPV6_op ops = {
    GetTX_m, 
    GetRX_m,
    QueryMTU_m, 
    QueryStackDepth_m, 
    Dispose_m,
};

/* IO ops */
static  IO_PutPkt_fn            PutPkt_m;
static  IO_GetPkt_fn            GetPkt_m;
static  IO_GetPktUntil_fn       GetPktUntil_m;
static  IO_PutPktNoBlock_fn     PutPktNoBlock_m;
static  IO_GetPktNoBlock_fn     GetPktNoBlock_m;
static  IO_GetPoolInfo_fn	GetPoolInfo_m;
static  IO_Slots_fn             Slots_m;
static  IO_Dispose_fn           IO_Dispose_m;

static  IO_op   io_ops = {
    PutPkt_m,
    GetPkt_m,
    GetPktUntil_m,
    PutPktNoBlock_m,
    GetPktNoBlock_m,
    GetPoolInfo_m,
    Slots_m,
    IO_Dispose_m
};


/*
** htoncpy() is used to convert arbitrarily large octet strings from
** host order to network order. Main use expected is for IPV6 addresses.
** XXX at the moment this (a) assumes hosts are little endian (b) is dead inefficient.
*/
static void htoncpy(char *to, char *from, size_t n) 
{
    int i;

    for(i=0; i < n; i++)
	to[i]= from[(n-1)-i];
}


#define DUMP_HDR
#ifdef DUMP_HDR
/*
** dmp_ipv6addr(): spits out the hex bytes of the address 
*/
static void dmp_ipv6addr(char *addr)
{
    int i;
    
    for(i=0; i < IPV6_ADDRLEN; i++)
	printf("%02x ", addr[i]&0xFF);
}
#endif /* DUMP_HDR */


IPV6_clp New_m(IPV6Mod_cl *self, uint8_t *dest, uint8_t proto,
	       uint8_t max_hops, uint8_t pri, uint32_t flow, Heap_clp *heap)
{
    /* not implemented */
    return (IPV6_cl *)NULL;
}


IPV6_clp Piggyback_m(IPV6Mod_cl *self, Protocol_cl *base, uint8_t *dest, uint8_t *src,
		     uint8_t proto, uint8_t max_hops, uint8_t pri, uint32_t flow,
		     Heap_clp heap)
{
    IPV6_cl *cl;
    ipv6_st *st;
    IPV6Com_st *com;
    io_st *rxst;
    io_st *txst;
    IO_cl   *rxio;
    IO_cl   *txio;
    int i;

    st = Heap_Malloc(Pvs(heap), sizeof(IPV6_cl) +
		                sizeof(ipv6_st) +
		                sizeof(IPV6Com_st) +
		                (2 * (sizeof(io_st) + sizeof(IO_cl))) );
    if (!st) {
	printf("IPV6.c: Piggyback_m: out of memory\n");
	return NULL;
    }

    /* lay out memory */
    cl  = (IPV6_cl *)(st + 1);
    com = (IPV6Com_st *)(cl + 1);
    rxst= (io_st *)(com+ 1);
    txst= (io_st *)(rxst+1);
    rxio= (IO_cl *)(txst+1);
    txio= (IO_cl *)(rxio+1);

    /* plumb it together */
    cl->op = &ops;
    cl->st = st;
    st->iotx = txio;
    st->iorx = rxio;
    txio->op = &io_ops;
    txio->st = txst;
    rxio->op = &io_ops;
    rxio->st = rxst;

    /* init IO state */
    TRY
	txst->io = Protocol_GetTX(base);
    CATCH_Protocol_Simplex()
	txst->io = NULL;
    ENDTRY
    txst->common = com;
    txst->rxp    = False;

    TRY
	rxst->io = Protocol_GetRX(base);
    CATCH_Protocol_Simplex()
	rxst->io = NULL;
    ENDTRY
    rxst->common = com;
    rxst->rxp    = True;

    /* rest of state init */
    st->base   = base;             /* protocol under us */

    htoncpy(com->src, src, IPV6_ADDRLEN);
    htoncpy(com->dest, dest, IPV6_ADDRLEN);

    com->proto = proto;            /* User-level protocol number */

    com->hmax = (max_hops)? max_hops : DEF_HOP_MAX;
    com->pri  = (pri)? pri : DEF_PRIORITY;
    com->flow = (flow) ? flow : DEF_FLOW;


    st->heap  = heap;

    com->ident= 0;                /* start id tags at 0 */

    com->header_base = Heap_Malloc(st->heap, sizeof(ipv6header) * MAX_HEADERS);
    if (!com->header_base)
    {
	printf("IPV6.c: Piggyback_m: out of memory(2)\n");
	Heap_Free(st->heap, st);
	return NULL;
    }

    /* push all headers onto the freestack */
    com->sp = 0; /* nothing on stack initially */
    for(i=0; i<MAX_HEADERS; i++)
	com->freestack[com->sp++] = &(com->header_base[i]);

    return cl;
}


/* ------------------------------------------------------------ */

/* for debugging use */
static void dump_IOVec(uint32_t nrecs, IO_Rec *recs)
{
    int i;

    printf("   %d IORecs:\n", nrecs);
    for(i=0; i<nrecs; i++)
	printf("     base:%p len:%d\n", (void *)recs[i].base, recs[i].len);
}

#ifdef BUFFER_PARANOIA
static void dump_freestack(io_st *st)
{
    int i;
    char buf[80]="Freestack: ";
    int len;

    len = strlen(buf);
    for(i=0; i < st->common->sp; i++)
	len += sprintf(buf+len, "%p ", st->common->freestack[i]);

    printf("%s\n", buf);
}
#endif /* BUFFER_PARANIOA */

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
	printf ("\n");
    }
    printf ("------\n");
}

/* ------------------------------------------------------------ */
/* Fake IO interface */

static void 
PutPkt_m (
        IO_cl   *self,
        uint32_t        nrecs   /* IN */,
        IO_Rec_ptr      recs    /* IN */,
        word_t        value   /* IN */ )
{
    io_st *st = (io_st *) self->st;
    IO_Rec *newrecs;

    if (st->rxp)
    {
	IO_PutPkt(st->io, nrecs, recs, value); /* send empties */
    }
    else
    {
	/* encaps and transmit */
	newrecs = alloca(sizeof(IO_Rec) * (nrecs + 2));
	nrecs = encaps(st, nrecs, recs, newrecs);
	if (nrecs)
	    IO_PutPkt(st->io, nrecs, newrecs, value);
    }       
}

static uint32_t 
GetPkt_m (
        IO_cl   *self,
        uint32_t        max_recs        /* IN */,
        IO_Rec_ptr      recs    /* IN */,
        word_t        *value  /* OUT */ )
{
    io_st *st = (io_st *) self->st;
    uint32_t nrecs;
    IO_Rec *newrecs;

    if (st->rxp)
    {
	/* get packet and strip headers */
	nrecs = IO_GetPkt(st->io, max_recs, recs, value);
	return strip(st, nrecs, recs);
    }
    else
    {
	newrecs = alloca(sizeof(IO_Rec) * (max_recs + 1));
	nrecs = IO_GetPkt(st->io, max_recs+1, newrecs, value);
	return rescue(st, max_recs, nrecs, recs, newrecs);
    }
}

static uint32_t 
GetPktUntil_m (
        IO_cl   *self,
        uint32_t        max_recs        /* IN */,
        IO_Rec_ptr      recs    /* IN */,
        Time_T  timeout /* IN */,
        word_t        *value  /* OUT */ )
{
    io_st *st = (io_st *) self->st;
    uint32_t nrecs;
    IO_Rec *newrecs;

    if (st->rxp)
    {
	/* get packet and strip headers */
	nrecs = IO_GetPktUntil(st->io, max_recs, recs, timeout, value);
	return strip(st, nrecs, recs);
    }
    else
    {
	newrecs = alloca(sizeof(IO_Rec) * (max_recs + 1));
	nrecs = IO_GetPktUntil(st->io, max_recs+1, newrecs, timeout, value);
	return rescue(st, max_recs, nrecs, recs, newrecs);
    }
}

static void 
PutPktNoBlock_m (
        IO_cl   *self,
        uint32_t        nrecs   /* IN */,
        IO_Rec_ptr      recs    /* IN */,
        word_t        value   /* IN */ )
{
    io_st *st = (io_st *) self->st;
    IO_Rec *newrecs;

    if (st->rxp)
    {
	IO_PutPktNoBlock(st->io, nrecs, recs, value); /* send empties */
    }
    else
    {
	/* encaps and transmit */
	newrecs = alloca(sizeof(IO_Rec) * (nrecs + 2));
	nrecs = encaps(st, nrecs, recs, newrecs);
	if (nrecs)
	    IO_PutPktNoBlock(st->io, nrecs, newrecs, value);
    }       
}

static bool_t 
GetPktNoBlock_m (
        IO_cl   *self,
        uint32_t        max_recs        /* IN */,
        IO_Rec_ptr      recs    /* IN */,
        uint32_t        *nrecs  /* OUT */,
        word_t        *value  /* OUT */ )
{
    io_st *st = (io_st *) self->st;
    bool_t success;
    IO_Rec *newrecs;

    if (st->rxp)
    {
	/* get packet and strip headers */
	success = IO_GetPktNoBlock(st->io, max_recs, recs, nrecs, value);
	if (success)
	    *nrecs = strip(st, *nrecs, recs);
	return success;
    }
    else
    {
	newrecs = alloca(sizeof(IO_Rec) * (max_recs + 1));
	success = IO_GetPktNoBlock(st->io, max_recs+1, newrecs, nrecs, value);
	if (success)
	    *nrecs = rescue(st, max_recs, *nrecs, recs, newrecs);
	return success;
    }
}


static void 
GetPoolInfo_m (
        IO_cl   *self,
        IDC_Buffer      *buf    /* OUT */ )
{
    io_st *st = (io_st *) self->st;

    IO_GetPoolInfo(st->io, buf);
}

static uint32_t 
Slots_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    return IO_Slots(st->io);
}

static void 
IO_Dispose_m (
        IO_cl   *self )
{
    io_st *st = (io_st *) self->st;

    /* XXX more thought needed. Should we take this to mean destroy the
     * whole protocol? */
    IO_Dispose(st->io);
}


/* ------------------------------------------------------------ */
/* IP implementation */

/* rescue our headers from the empty (previously transmitted) IORecs */
/* Assumptions/environment: */
/* "st" is our state.
 * "max_recs" is the size of "recs"
 * "max_recs"+1 is the size of "newrecs"
 * "newrecs" has been filled with "nrecs" of packet data
 * Task is to fill "recs" with what we feel is appropriate. Return new
 * size of "recs".
 */
static uint32_t rescue(io_st *st, uint32_t max_recs, uint32_t nrecs,
		       IO_Rec *recs, IO_Rec *newrecs)
{
    int i;
    int base, len;
    word_t hdr;  /* big enough to hold an address */

    /* decide how much to keep */
    if (nrecs < 2)
    {
	printf("IP: error: base protocol didn't return enough IORecs (%d), "
		"can't rescue my header\n", nrecs);
	base = 0;
	len = MIN(nrecs, max_recs);
	if (nrecs > max_recs)
	    printf("(also dropped %d recs - can't happen)\n",
		    nrecs - max_recs);
    }
    else if ((hdr=HDRBASE(newrecs[0].base)) <
	     (word_t)st->common->header_base ||
	     hdr > (word_t)(st->common->header_base
			    + MAX_HEADERS * sizeof(ipv6header)))
    {
	printf("IP: nasty error: first IORec isn't valid IP "
		"header buf (base=%p)\n", st->common->header_base);
	dump_IOVec(nrecs, newrecs);
	dump_freestack(st);
	base = 0;
	len = MIN(nrecs, max_recs);
	if (nrecs > max_recs)
	    printf("(also dropped %d recs)\n", nrecs - max_recs);
    }
    else /* looks plausable */
    {
#ifdef BUFFER_PARANOIA
	for(i=0; i<st->common->sp; i++)
	{
	    if (hdr == (word_t)st->common->freestack[i])
	    {
		printf("IPV6: warning: first IORec is one of mine, "
			"but already marked as free?\n");
		dump_IOVec(nrecs, newrecs);
		dump_freestack(st);
	    }
	}
#endif /* BUFFER_PARANOIA */

	/* looks ok, drop our header off the front */
	base = 1;
	len = nrecs - 1;
	/* mark it as free */
	if (st->common->sp < MAX_HEADERS)
	    st->common->freestack[st->common->sp++] = (ipv6header *)hdr;
	else
	    printf("IPV6: GetFree: cant happen: free buffers stack overflow\n");
    }

    /* copy rest of IORecs into the returns area */
    for(i=base; i<base+len; i++)
	recs[i-base] = newrecs[i];
    
    return len;
}


/* encapsulate the data within an IPV6 header */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * "newrecs" is "nrecs"+1 long
 * Task is to fill "newrecs" with ether header prefix, and maybe trailer.
 * Return the size of "newrecs", or 0 for error
 */
static uint32_t encaps(io_st *st, uint32_t nrecs,
		       IO_Rec *recs, IO_Rec *newrecs)
{
    int i;
    int size;
    uint8_t *hdr;

    /* add IP header */
    if (!st->common->sp)
    {
	printf("IPV6: no free header slots (%d outstanding tx packets)\n",
		MAX_HEADERS);
	return 0;
    }
    newrecs[0].base = st->common->freestack[--st->common->sp];
    newrecs[0].len  = IPHDR_LEN;

    /* move rest of packet bits down */
    for(i=1; i<=nrecs; i++)
	newrecs[i] = recs[i-1];
    nrecs++;

    /* fill in the header */
    hdr = newrecs[0].base;


    hdr[IPHDR_VERSPRI] = (IPV6_VERSION << 4) | st->common->pri;
    memcpy(&(hdr[IPHDR_FLOW]), (char *)&(st->common->flow), 3);

    /* work out size of datagram */
    size = 0;
    for(i=0; i<nrecs; i++)
	size += newrecs[i].len;

    if (size > 0xffff)  /* if > 64Kb => need to use jumbo payload. */
    {
	printf("IPV6: tx packet (%d bytes) needs jumbo payload\n", size);
	
	/* XXX erm, do the jumbo payload stuff here. but need frag impl first */
	printf("IPV6: halting since jumbo payload not yet implemented.\n");
	ntsc_halt();
	return 0;
    }

    /* XXX if (size > MTU) then fragment it */

    hdr[IPHDR_LENGTH] = (uint16_t)size;


    /* XXX we assume for now that the ULP follows the ipv6 hdr directly */
    hdr[IPHDR_NEXTHDR]= st->common->proto; 
    hdr[IPHDR_HOP_MAX]= st->common->hmax;

    memcpy(&(hdr[IPHDR_SRCIPV6]), st->common->src, IPV6_ADDRLEN);
    memcpy(&(hdr[IPHDR_DESTIPV6]), st->common->dest, IPV6_ADDRLEN);


    /* XXX other optional headers could go here */

    return nrecs;
}


/* strip off IP headers from received data */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * Task is to trim "recs" to the payload of this protocol
 * Return the size of "recs".
 */
static uint32_t strip(io_st *st, uint32_t nrecs, IO_Rec *recs)
{
    int i;
    uint8_t *hdr;

    if (recs[0].len < IPHDR_LEN)
    {
	printf("IPV6: warning: short first IORec, "
		"not stripping IP header\n");
	return nrecs;
    }

    hdr= (uint8_t *)recs[0].base;

#ifdef DUMP_HDR
    /* dump out the IPV6 header. */
    printf("Dumping IPV6 header...\n");
    printf("Version:  %x\n", (hdr[IPHDR_VERSPRI]>>4)&0x0f); 
    printf("Priority: %x\n", hdr[IPHDR_VERSPRI]&0x0f);
    printf("Flow:     %x\n", ((hdr[IPHDR_FLOW]&0xFF)<<16)|
	    ((hdr[IPHDR_FLOW+1]&0xFF)<<8)|(hdr[IPHDR_FLOW+2]&0xFF));
    printf("Length:   %x\n", ((uint16_t *)hdr)[IPHDR_LENGTH]&0xFFFF);
    printf("Next hdr: %x\n", hdr[IPHDR_NEXTHDR]&0xFF); 
    printf("Hop Max:  %x\n", hdr[IPHDR_HOP_MAX]&0xFF);
    printf("Src IP:   "); dmp_ipv6addr(&hdr[IPHDR_SRCIPV6]); printf("\n");
    printf("Dst IP :  "); dmp_ipv6addr(&hdr[IPHDR_SRCIPV6]); printf("\n");
#endif /* DUMP_HDR */

    /* check if the ip address in the header is correct */
    if(!(memcmp(&(hdr[IPHDR_DESTIPV6]),	st->common->src, IPV6_ADDRLEN)))
	printf("IPV6: warning: recvd packet with dest != my addr\n");

    recs[0].len  -= IPHDR_LEN;
    recs[0].base += IPHDR_LEN;
    
    if (recs[0].len == 0)
    {
	printf("IPV6: warning: first IORec was exactly IP header size\n");
	if (nrecs < 2)
	{
	    printf("IPV6: error: packet consisted only of IP headers?\n");
	    return 0;
	}
	/* shunt up other parts of packet */
	for(i=1; i<nrecs; i++)
	    recs[i-1] = recs[i];
	nrecs--;
    }

    return nrecs;
}

/* ------------------------------------------------------------ */
/* Misc utility functions */

static IO_clp 
GetTX_m(Protocol_cl *self )
{
    ipv6_st   *st = (ipv6_st *) self->st;
    io_st *txst = st->iotx->st;

    if (txst->io)
	return st->iotx;
    else
	RAISE_Protocol_Simplex();

    /* middlc bug: functions marked NEVER RETURNS should generate
     * function prototypes with NORETURN() macro. Otherwise gcc
     * complains needlessly. XXX keep gcc happy about returns: */
    return NULL;
}

static IO_clp 
GetRX_m(Protocol_cl *self)
{
    ipv6_st   *st = (ipv6_st *) self->st;
    io_st *rxst = st->iorx->st;

    if (rxst->io)
	return st->iorx;
    else
	RAISE_Protocol_Simplex();

    /* middlc bug: functions marked NEVER RETURNS should generate
     * function prototypes with NORETURN() macro. Otherwise gcc
     * complains needlessly. XXX keep gcc happy about returns: */
    return NULL;
}

uint32_t QueryMTU_m(Protocol_cl *self)
{
    ipv6_st *st;

    st = self->st;

    return Protocol_QueryMTU(st->base);
}    


static uint32_t 
QueryStackDepth_m(Protocol_cl *self )
{
    ipv6_st   *st = self->st;

    /* one for the IPV6 header, then whatever our base requires */
    return 1 + Protocol_QueryStackDepth(st->base);
}



void Dispose_m(Protocol_cl *self)
{
    ipv6_st *st;

    st = self->st;

    /* XXX finished off any other derivatives */
    Protocol_Dispose(st->base);
    /* XXX my packet header space */
    Heap_Free(Pvs(heap), st);
}


/* End of ip.c */
