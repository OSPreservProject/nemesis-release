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
**      ether.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Encapsulate data into ethernet frames
** 
** ENVIRONMENT: 
**
**      Network subsystem
** 
** ID : $Id: ether.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
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
#include <pktcx.h>

#include <Net.ih>
#include <StretchAllocator.ih>
#include <HeapMod.ih>
#include <IO.ih>
#include <IDCOffer.ih>
#include <Protocol.ih>
#include <Ether.ih>
#include <EtherMod.ih>

#include <nettrace.h>
#include "../fakeIO.h"

#define TRC(_x) 

/* ------------------------------------------------------------ */

typedef struct {
    uint8_t	pad[2];  /* pad the structure up to 16 bytes */
    uint8_t     dest[6];
    uint8_t     src[6];
    uint16_t	frameType;
} etherheader;

typedef struct {
    IO_clp	io;      /* real IO to use */
    bool_t	rxp;     /* True if this is an RX IO */
    etherheader *header; /* pre-computed header */
    /* Non-base protocols will need an offset here too */
} io_st;

/* ether state */
typedef struct {
    IO_cl	*iorx; /* fake rx io clp */
    IO_cl	*iotx; /* fake tx io clp */
} ether_st;

#define ETH_MIN_LEN    46  /* minimum size of frame data, in bytes */
#define ETH_HEADER_LEN (sizeof(etherheader))

static const uint8_t ethaddr_broadcast[] =
  { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* ------------------------------------------------------------ */

/* prototypes */
static INLINE int rx_check(io_st *st, uint32_t *nrecs, IO_Rec *recs,
			   uint32_t max_recs, pktcx_t *pc, word_t *value);
static INLINE bool_t tx_encaps(io_st *st, uint32_t nrecs, IO_Rec *recs);

/* EtherMod stuff */
static EtherMod_New_fn New_m;
static EtherMod_op modops = {New_m};
static const EtherMod_cl modcl = {&modops, BADPTR};
CL_EXPORT(EtherMod, EtherMod, modcl);

/* Ether ops */
static Protocol_GetTX_fn GetTX_m;
static Protocol_GetRX_fn GetRX_m;
static Protocol_QueryMTU_fn QueryMTU_m;
static Protocol_QueryHeaderSize_fn QueryHeaderSize_m;
static Protocol_QueryTrailerSize_fn QueryTrailerSize_m;
static Protocol_Dispose_fn Dispose_m;
static Ether_SetDest_fn SetDest_m;
static Ether_op ops = {GetTX_m, GetRX_m,
		       QueryMTU_m, QueryHeaderSize_m, QueryTrailerSize_m,
		       Dispose_m, SetDest_m};

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



Ether_clp New_m(EtherMod_cl *self, const Net_EtherAddr *dest,
		const Net_EtherAddr *src, uint16_t frameType,
		IO_clp rxio, IO_clp txio)
{
    ether_st    *st;
    io_st       *rxst, *txst;
    IO_cl       *rxcl, *txcl;
    etherheader *header;
    Ether_cl    *cl;
    HeapMod_clp hmod; 

    st = Heap$Malloc(Pvs(heap), sizeof(Ether_cl) +
		                sizeof(ether_st) +
		                sizeof(etherheader) +
		                (2 * (sizeof(io_st)+sizeof(IO_cl))));
    if (!st)
    {
	printf("Ether.c: New_m: out of memory\n");
	return BADPTR;
    }
    
    /* lay out our memory */
    cl     = (Ether_cl *)(st + 1);
    header = (etherheader *)(cl+1);
    rxst   = (io_st *)(header + 1);
    txst   = (io_st *)(rxst+1);
    rxcl   = (IO_cl *)(txst+1);
    txcl   = (IO_cl *)(rxcl+1);
    
    cl->op = &ops;
    cl->st = st;

    /* link everything together */
    st->iorx = rxcl;
    st->iotx = txcl;

    rxcl->op = &io_ops;
    rxcl->st = rxst;
    txcl->op = &io_ops;
    txcl->st = txst;

    /* fill in args */
    rxst->rxp    = True;
    rxst->header = header;
    txst->rxp    = False;
    txst->header = header;

    /* bind to the IOs */
    hmod     = NAME_FIND("modules>HeapMod", HeapMod_clp);
    rxst->io = rxio;
    txst->io = txio;

    /* pre-calc the header */
    memcpy(&(header->dest), dest, 6);
    memcpy(&(header->src),  src,  6);
    header->frameType = frameType;

    TRC(printf("ether.c: New successful, returning %p\n", cl));
    return cl;
}


/* ------------------------------------------------------------ */
/* Ether implementation */

/* for debugging use */
static void UNUSED dump_IOVec(uint32_t nrecs, IO_Rec *recs)
{
    int i;

    printf("   %d IORecs:\n", nrecs);
    for(i=0; i<nrecs; i++)
	printf("     base:%p len:%d\n", (void *)recs[i].base, recs[i].len);
}


static void UNUSED
DumpPkt (IO_Rec *recs, uint32_t nrecs)
{
    char buf[80];
    int len=0;
    int r;

    for (r=0; r < nrecs; r++)
    {
	uint32_t	i;
	uint8_t    *p;

	p = (uint8_t *) recs[r].base;
    
	for (i = 0; i < recs[r].len; i++)
	{
	    len += sprintf(buf+len, "%02x ", *p++);
	    if (i % 16 == 15)
	    {
		printf("%s(len:%d)\n", buf, len);
		len=0;
	    }
	}
	if (len)
	{
	    printf("%s[len:%d]\n", buf, len);
	    len=0;
	}
	printf("\n");
	    
    }
    printf ("------\n");
}

/* ------------------------------------------------------------ */
/* Fake IO implementation */

#define MyIO_PutPkt(st, nrecs, recs, value, until)	\
({							\
    recs[0].base += 2;					\
    recs[0].len  -= 2;					\
    IO$PutPkt(st->io, nrecs, recs, value, until);	\
})

#define MyIO_GetPkt(st, max_recs, recs, until, nrecs, value)	\
({								\
  bool_t ok; 							\
  ok = IO$GetPkt(st->io, max_recs, recs, until, nrecs, value);	\
  if(*nrecs && (*nrecs <= max_recs)) {				\
    recs[0].base -= 2;						\
    recs[0].len  += 2;						\
  }								\
  ok; 								\
})



/* ------------------------------------------------------------ */
/* Protocol implementation */


/* encapsulate the data within an ether header */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * Task is to fill start of "recs[0]" with ether header data.
 */
static INLINE bool_t tx_encaps(io_st *st, uint32_t nrecs, IO_Rec *recs)
{

    /* Offset for ethernet is always 0, since we're a base protocol.
     * Other protocols will need to look where to splash their headers. */

    /* range check */
    if (recs[0].len < sizeof(etherheader))
    {
	printf("Ether: first IORec was too short to fill in ether header "
		"(%d < %d)\n", recs[0].len, sizeof(etherheader));
	return False;
    }

    /* Copy ethernet header in. Assume buffer is sensibly aligned */
    *(etherheader *)recs[0].base = *st->header;

    /* Here we used to test if the frame size was too small, and pad
     * if necessary.  But now we assume the ether card driver will do
     * that (because some cards can do it in hardware) */

#ifdef LATENCY_TRACE
    if (recs[1].len == 1024 - 44)
	STAMP_PKT_USER(recs[1].base, CLIENT_BASE);
#endif LATENCY_TRACE

    return True;
}


/* strip off ether headers from received data */
/* Assumptions/environment: */
/* "st" is our state.
 * "recs" has been filled with "nrecs" of packet data
 * Task is to trim "recs" to the payload of this protocol
 * Return the size of "recs".
 */
static INLINE int rx_check(io_st *st, uint32_t *nrecs, IO_Rec *recs,
			   uint32_t max_recs, pktcx_t *pc, word_t *value)
{
    etherheader *hdr;

    TRC(printf("ether: got packet:\n"));
    TRC(dump_IOVec(nrecs, recs));
/*    TRC(DumpPkt(recs, nrecs));*/
/*    for(i=0; i<100000000; i++)
	;
	*/
/*    PAUSE(SECONDS(1));
    TRC(printf("ether: end pause\n"));*/

    if (*nrecs < 1)
    {
	printf("Ether: programmer error: rx_check() called with nrecs=%d",
		*nrecs);
	return AC_DITCH;
    }

    if (recs[0].len < sizeof(etherheader))
    {
	printf("Ether: warning: short first IORec, ether hdr corrupted?\n");
	return AC_DITCH;
    }

/*    printf("ether: len:%d\n", recs[0].len);*/

    /* who was supposed to get this packet? */
    hdr = recs[0].base; /* offset of 0 to get to ether header */
    if (memcmp(hdr->dest, st->header->src, 6) &&
	memcmp(hdr->dest, ethaddr_broadcast, 6))
	printf("Ether: warning: recvd packet with dest != my addr!\n");

#ifdef LATENCY_TRACE
    if (recs[1].len == 1024 - 44)
	STAMP_PKT_USER(recs[1].base, CLIENT_BASE);
#endif LATENCY_TRACE

    return AC_TOCLIENT;
}

/* Don't include this until after tx_encaps or rx_check or they won't
   get inlined */

#include "../fakeIO.c"

/* ------------------------------------------------------------ */

static IO_clp 
GetTX_m (
        Protocol_cl     *self )
{
    ether_st   *st = (ether_st *) self->st;
    io_st    *txst = st->iotx->st;

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
    ether_st   *st = (ether_st *) self->st;
    io_st    *rxst = st->iorx->st;

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
    return 1516;  /* = 1500(payload) + 14(header) + 2(header padding) */
}

static uint32_t 
QueryHeaderSize_m (
        Protocol_cl     *self )
{
    return sizeof(etherheader); /* base protocol, so nothing else */
}

static uint32_t 
QueryTrailerSize_m (
        Protocol_cl     *self )
{
    return 0; /* no need for a trailer */
}


void Dispose_m(Protocol_cl *self)
{
    ether_st *st;

    st = self->st;

    /* XXX finished off any other derivatives */
#if 0
    IO$Dispose(st->iorx);
    IO$Dispose(st->iotx);
#endif /* 0 */
    Heap$Free(Pvs(heap), st);
}

#if 0
static void 
Go_m (
        BaseProtocol_cl        *self,
        IDCOffer_clp  rx      /* IN */,
        IDCOffer_clp  tx      /* IN */ )
{
    ether_st      *st = (ether_st *) self->st;
    io_st  *txst = st->iotx->st;
    io_st  *rxst = st->iorx->st;

    txst->io = tx? IO_BIND(tx) : NULL;
    rxst->io = rx? IO_BIND(rx) : NULL;
}
#endif

static void SetDest_m(Ether_cl *self, const Net_EtherAddr *destHW)
{
    ether_st      *st = self->st;
    io_st  *txst = st->iotx->st;
    
    memcpy(&(txst->header->dest), destHW, 6);
}

/* End of ether.c */
