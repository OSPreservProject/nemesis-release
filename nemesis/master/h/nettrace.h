/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      nettrace.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      macros for waypoint tracing
** 
** ENVIRONMENT: 
**
**      all over the network stack
** 
** ID : $Id: nettrace.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef nettrace_h_
#define nettrace_h_


/* Defining this turns on checks on packet length all over the network
 * stack.  Packets of length 1024 (including headers) are stamped all
 * over the place with a waymarker and the current rpcc() value. */
/*#define LATENCY_TRACE*/


#ifdef LATENCY_TRACE

/* waypoints */
enum netwaypoint_t {
    DRIVER_RX,		/* in device driver, on pkt RX from wire */
    NETIF_AFTER_RXPF,	/* in Netif, after running packet filter */
    NETIF_TOCLIENT,	/* in Netif, just before queueing packet in Rbuf */
    CLIENT_BASE,	/* in client, just after removing from Rbuf */
    CLIENT_TOP,		/* in client, about to return packet to application */
    NETIF_FROMCLIENT,	/* in Netif, new packet to TX */
    NETIF_AFTER_TXPF,	/* in Netif, after TX check */
    DRIVER_TX		/* in driver, queued for TX */
};

static const char * const netwaypoint_names[] =
{
    "DRIVER_RX",
    "NETIF_AFTER_RXPF",
    "NETIF_TOCLIENT",
    "CLIENT_BASE",
    "CLIENT_TOP",
    "NETIF_FROMCLIENT",
    "NETIF_AFTER_TXPF",
    "DRIVER_TX"
};

/* write "waypoint" and the current rpcc() value into the packet
 * payload at base (which is aligned) */
#define STAMP_PKT_USER(base, waypoint) \
do {						\
    uint32_t *p = (base);			\
    uint32_t *d = (base);			\
    d += p[0]+1;				\
    p[0] += 4;					\
    d[0] = (waypoint);				\
    d[1] = rpcc();				\
    d[2] = *((uint32_t*)(((char*)INFO_PAGE_ADDRESS)+0x200)); \
    d[3] = *((uint32_t*)(((char*)INFO_PAGE_ADDRESS)+0x208)); \
} while(0)


/* Same as above, but with 14 bytes of Ethernet header; this means the
 * UDP payload is misaligned */

/* some macros for unaligned load / store */
/* XXX assumes little-endian! */
#define LDUNA32(a)				\
({						\
    uint16_t *addr = (a);			\
    (addr[0]) | (addr[1] << 16);		\
})

#define STUNA32(a, v)				\
do {						\
    uint16_t *addr = (a);			\
    uint32_t value = (v);			\
    addr[0] = value & 0xffff;			\
    addr[1] = value >> 16;			\
} while(0)

#define STAMP_PKT_UNA(base, waypoint) \
do {						\
    char *p = (base);				\
    uint32_t *d;				\
    uint32_t step;				\
    d = (void*)(p+42);				\
    step = LDUNA32(p + 42);			\
    d += step+1;				\
    STUNA32(p+42, step + 4);			\
    STUNA32(d, (waypoint));			\
    STUNA32(d+1, rpcc());			\
    STUNA32(d+2, *((uint32_t*)(((char*)INFO_PAGE_ADDRESS)+0x200))); \
    STUNA32(d+3, *((uint32_t*)(((char*)INFO_PAGE_ADDRESS)+0x208))); \
} while(0)

#endif /* LATENCY_TRACE */

#endif /* nettrace_h_ */
