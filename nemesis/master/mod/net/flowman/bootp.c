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
**      bootp.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Performs BOOTP on one interface, and file away the reply
** 
** ENVIRONMENT: 
**
**      Network initialisation time
** 
** ID : $Id: bootp.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <contexts.h>
#include <time.h>
#include <netorder.h>
#include <iana.h>

#include <Net.ih>
#include <Netif.ih>
#include <ProtectionDomain.ih>
#include <EtherMod.ih>
#include <Ether.ih>
#include <IPMod.ih>
#include <IP.ih>
#include <UDPMod.ih>
#include <UDP.ih>
#include <Protocol.ih>
#include <BaseProtocol.ih>
#include <IDCOffer.ih>

#include <LMPFCtl.ih>

#include <IOMacros.h>

#include "flowman_st.h"

#ifdef BOOTP_TRACE
#define TRC(x) x
#else
#define TRC(x)
#endif /* BOOTP_TRACE */

/* define IDENTIFY_THYSELF if you want "0xc0ffee" tacked onto the end of
 * the client's claimed hardware address */
/*#define IDENTIFY_THYSELF*/

/* ------------------------------------------------------------ */

#define OP_REQUEST  1
#define OP_REPLY    2
#define HTYPE_ETHER 1
#ifdef IDENTIFY_THYSELF
#define HLEN	    9
#define ADD_ID_TRAILER() \
do {						\
    p->hwaddr[6] = 0xc0;			\
    p->hwaddr[7] = 0xff;			\
    p->hwaddr[8] = 0xee;			\
} while(0)
#else
#define HLEN        6
#define ADD_ID_TRAILER()
#endif

/* Vendor cookies - we only handle rfc1048 style cookies */

#define VENDOR_RFC1048 "\x63\x82\x53\x63"
#define VENDOR_NONE    "\x00\x00\x00\x00"

/* These #defines shamelessly nicked from Jon Peatfield's user-space
 * bootp client. But that's ok, since I had a hand in it anyawy. and1000 */

/* End of cookie */
#define TAG_END			((unsigned char) 255)
/* padding for alignment */
#define TAG_PAD			((unsigned char)   0)
/* Subnet mask */
#define TAG_SUBNET_MASK		((unsigned char)   1)
/* Time offset from UTC for this system */
#define TAG_TIME_OFFSET		((unsigned char)   2)
/* List of routers on this subnet */
#define TAG_GATEWAY		((unsigned char)   3)
/* List of rfc868 time servers available to client */
#define TAG_TIME_SERVER		((unsigned char)   4)
/* List of IEN 116 name servers */
#define TAG_NAME_SERVER		((unsigned char)   5)
/* List of DNS name servers */
#define TAG_DOMAIN_SERVER	((unsigned char)   6)
/* List of MIT-LCS UDL log servers */
#define TAG_LOG_SERVER		((unsigned char)   7)
/* List of rfc865 cookie servers */
#define TAG_COOKIE_SERVER	((unsigned char)   8)
/* List of rfc1179 printer servers (in order to try) */
#define TAG_LPR_SERVER		((unsigned char)   9)
/* List of Imagen Impress servers (in prefered order) */
#define TAG_IMPRESS_SERVER	((unsigned char)  10)
/* List of rfc887 Resourse Location servers */
#define TAG_RLP_SERVER		((unsigned char)  11)
/* Hostname of client */
#define TAG_HOST_NAME		((unsigned char)  12)
/* boot file size */
#define TAG_BOOT_SIZE		((unsigned char)  13)
/* RFC 1395 */
/* path to dump to in case of crash */
#define TAG_DUMP_FILE		((unsigned char)  14)
/* domain name for use with the DNS */
#define TAG_DOMAIN_NAME		((unsigned char)  15)
/* IP address of the swap server for this machine */
#define TAG_SWAP_SERVER		((unsigned char)  16)
/* The path name to the root filesystem for this machine */
#define TAG_ROOT_PATH		((unsigned char)  17)
/* RFC 1497 */
/* filename to tftp with more options in it */
#define TAG_EXTEN_FILE		((unsigned char)  18)
/* RFC 1533 */
/* The following are in rfc1533 and may be used by BOOTP/DHCP */
/* IP forwarding enable/disable */
#define TAG_IP_FORWARD          ((unsigned char)  19)
/* Non-Local source routing enable/disable */
#define TAG_IP_NLSR             ((unsigned char)  20)
/* List of pairs of addresses/masks to allow non-local source routing to */
#define TAG_IP_POLICY_FILTER    ((unsigned char)  21)
/* Maximum size of datagrams client should be prepared to reassemble */
#define TAG_IP_MAX_DRS          ((unsigned char)  22)
/* Default IP TTL */
#define TAG_IP_TTL              ((unsigned char)  23)
/* Timeout in seconds to age path MTU values found with rfc1191 */
#define TAG_IP_MTU_AGE          ((unsigned char)  24)
/* Table of MTU sizes to use when doing rfc1191 MTU discovery */
#define TAG_IP_MTU_PLAT         ((unsigned char)  25)
/* MTU to use on this interface */
#define TAG_IP_MTU              ((unsigned char)  26)
/* All subnets are local option */
#define TAG_IP_SNARL            ((unsigned char)  27)
/* broadcast address */
#define TAG_IP_BROADCAST        ((unsigned char)  28)
/* perform subnet mask discovery using ICMP */
#define TAG_IP_SMASKDISC        ((unsigned char)  29)
/* act as a subnet mask server using ICMP */
#define TAG_IP_SMASKSUPP        ((unsigned char)  30)
/* perform rfc1256 router discovery */
#define TAG_IP_ROUTERDISC       ((unsigned char)  31)
/* address to send router solicitation requests */
#define TAG_IP_ROUTER_SOL_ADDR  ((unsigned char)  32)
/* list of static routes to addresses (addr, router) pairs */
#define TAG_IP_STATIC_ROUTES    ((unsigned char)  33)
/* use trailers (rfc893) when using ARP */
#define TAG_IP_TRAILER_ENC      ((unsigned char)  34)
/* timeout in seconds for ARP cache entries */
#define TAG_ARP_TIMEOUT         ((unsigned char)  35)
/* use either Ethernet version 2 (rfc894) or IEEE 802.3 (rfc1042) */
#define TAG_ETHER_IEEE          ((unsigned char)  36)
/* default TCP TTL when sending TCP segments */
#define TAG_IP_TCP_TTL          ((unsigned char)  37)
/* time for client to wait before sending a keepalive on a TCP connection */
#define TAG_IP_TCP_KA_INT       ((unsigned char)  38)
/* don't send keepalive with an octet of garbage for compatability */
#define TAG_IP_TCP_KA_GARBAGE   ((unsigned char)  39)
/* NIS domainname */
#define TAG_NIS_DOMAIN		((unsigned char)  40)
/* list of NIS servers */
#define TAG_NIS_SERVER		((unsigned char)  41)
/* list of NTP servers */
#define TAG_NTP_SERVER		((unsigned char)  42)
/* and stuff vendors may want to add */
#define TAG_VEND_SPECIFIC       ((unsigned char)  43)
/* NetBios over TCP/IP name server */
#define TAG_NBNS_SERVER         ((unsigned char)  44)
/* NetBios over TCP/IP NBDD servers (rfc1001/1002) */
#define TAG_NBDD_SERVER         ((unsigned char)  45)
/* NetBios over TCP/IP node type option for use with above */
#define TAG_NBOTCP_OTPION       ((unsigned char)  46)
/* NetBios over TCP/IP scopt option for use with above */
#define TAG_NB_SCOPE            ((unsigned char)  47)
/* list of X Window system font servers */
#define TAG_XFONT_SERVER        ((unsigned char)  48)
/* list of systems running X Display Manager (xdm) available to this client */
#define TAG_XDISPLAY_SERVER     ((unsigned char)  49)

/* While the following are only allowed for DHCP */
/* DHCP requested IP address */
#define TAG_DHCP_REQ_IP         ((unsigned char)  50)
/* DHCP time for lease of IP address */
#define TAG_DHCP_LEASE_TIME     ((unsigned char)  51)
/* DHCP options overload */
#define TAG_DHCP_OPTOVER        ((unsigned char)  52)
/* DHCP message type */
#define TAG_DHCP_MESS_TYPE      ((unsigned char)  53)
/* DHCP server identification */
#define TAG_DHCP_SERVER_ID      ((unsigned char)  54)
/* DHCP ordered list of requested parameters */
#define TAG_DHCP_PARM_REQ_LIST  ((unsigned char)  55)
/* DHCP reply message */
#define TAG_DHCP_TEXT_MESSAGE   ((unsigned char)  56)
/* DHCP maximum packet size willing to accept */
#define TAG_DHCP_MAX_MSGSZ      ((unsigned char)  57)
/* DHCP time 'til client needs to renew */
#define TAG_DHCP_RENEWAL_TIME   ((unsigned char)  58)
/* DHCP  time 'til client needs to rebind */
#define TAG_DHCP_REBIND_TIME    ((unsigned char)  59)
/* DHCP class identifier */
#define TAG_DHCP_CLASSID        ((unsigned char)  60)
/* DHCP client unique identifier */
#define TAG_DHCP_CLIENTID       ((unsigned char)  61)


typedef struct {
    uint8_t	op;
    uint8_t	htype;
    uint8_t	hlen;
    uint8_t	hops;
    uint32_t	transaction_id;	/* pseudo rand number */
    uint16_t	seconds;	/* seconds since machine started to boot */
    uint16_t	pad;
    uint32_t	clientIP;
    uint32_t	myIP;
    uint32_t	serverIP;
    uint32_t	gatewayIP;
    uint8_t	hwaddr[16];
    char	serverHostname[64];
    char	bootfile[128];
    uint8_t	vendor[64];	/* maybe should be a union */
} bootp_packet;

static int build_bootp(bootp_packet *p, int maxlen, Net_EtherAddr *mac,
		       int *bootbase);
static int bootp_recv(bootp_packet *p, int maxlen, Net_EtherAddr *mac,
		      bootp_retval *ret);
static void parse_cookie(bootp_packet *p, int maxlen, bootp_retval *ret);
static void ip_list(uint8_t *opt, int clen, uint32_t *ret);


/* ------------------------------------------------------------ */
static void UNUSED
DumpPkt (IO_Rec *recs, uint32_t nrecs)
{
  for (; nrecs; nrecs--, recs++)
  {
    uint32_t	i;
    uint8_t    *p;

    p = (uint8_t *) recs->base;
    
    for (i = 0; i < recs->len; i++)
    {
      printf ("%02x ", *p++);
      if (i % 16 == 15) printf ("\n");
    }
    printf ("\n");
  }
  printf ("------\n");
}


/* ------------------------------------------------------------ */

#if 0
NetEnv_t *bootp(FlowMan_st *st,
		if_st *if_st,
		Netif_cl *intf,   
		Net_EtherAddr *hwaddr,
		Context_cl *netcx)
#endif

int bootp(card_st *card, int32_t timeout, bootp_retval *ret)
{
    IOOffer_cl *rxoffer, *txoffer;
    IO_cl      *rxio,	 *txio;
    IO_Rec	txrec[3], rxrec[3];
    int		nrecs;
    EtherMod_cl *ethm;
    Ether_cl    *eth;
    IPMod_cl    *ipm;
    IP_cl	*ip;
    UDPMod_cl   *udpm;
    UDP_cl	*udp;
    IO_cl	*udptx;
    IO_cl	*udprx;
    word_t	value;
    Net_EtherAddr hwdestaddr;
    Net_IPHostAddr ipsrc;
    Net_IPHostAddr ipdest;
    int		replied;
    int 	backoff;
    Heap_Size	sz;
    uint32_t	hdrsz;
    uint32_t	trailsz;
    uint32_t	mtu;
    PF_Handle   rxhandle, txhandle;
    PF_clp	bootp_tx;
    int bootbase = -1; /* time we started bootping at */

    /* build a stack manually, since we want to hack the IP source
     * address to be 0.0.0.0 */

    /* get tx and rx IOs out */
    bootp_tx = LMPFMod$NewTX(card->pfmod,
			     card->mac,
			     0 /*src ip*/,
			     IP_PROTO_UDP,
			     UDP_PORT_BOOTP_CLIENT);

    TRC(printf("bootp: calling getIOs()\n"));
    getIOs(card->netif, bootp_tx, "bootp",
	   /*RETURNS:*/
	   &rxhandle,		&txhandle,
	   &rxio,		&txio,
	   &rxoffer,		&txoffer,
	   &card->rxheap,	&card->txheap);

    TRC(printf("bootp: adding demux, rxhandle %lx\n", rxhandle));
    LMPFCtl$AddUDP(card->rx_pfctl, UDP_PORT_BOOTP_CLIENT, 0, 0, rxhandle);

    /* slap an ethernet frame over the raw driver */
    ethm = NAME_FIND("modules>EtherMod", EtherMod_clp);
    memset(hwdestaddr.a, 0xff, 6); /* broadcast */
    TRC(printf("About to EtherMod_New...\n"));
    eth = EtherMod$New(ethm, &hwdestaddr,
		       card->mac, /* src */
		       FRAME_IPv4,
		       rxio, txio);

    /* put an IP datagram (talking UDP) within the ethernet frame */
    ipm      = NAME_FIND("modules>IPMod", IPMod_clp);
    ipsrc.a  = 0x00000000;  /* unknown IP source address */
    ipdest.a = 0xffffffff;  /* broadcast IP address */
    ip       = IPMod$Piggyback(ipm, (Protocol_clp)eth, &ipdest, &ipsrc,
			       IP_PROTO_UDP, 60, 0 /*flowid*/, card->txheap);

    /* put UDP datagram within the IP datagram */
    udpm = NAME_FIND("modules>UDPMod", UDPMod_clp);
    udp = UDPMod$Piggyback(udpm, (Protocol_clp)ip,
			   UDP_PORT_BOOTP_SERVER, /* dest */
			   UDP_PORT_BOOTP_CLIENT, /* reply-to */
			   0 /* cid */);

    /* get mem for tx packet */
    hdrsz   = Protocol$QueryHeaderSize((Protocol_clp)udp);
    trailsz = Protocol$QueryTrailerSize((Protocol_clp)udp);
    mtu = Protocol$QueryMTU((Protocol_clp)udp);

    sz = hdrsz + sizeof(bootp_packet) + trailsz;

    txrec[0].base = Heap$Malloc(card->txheap, sz);
    txrec[0].len  = hdrsz;
    txrec[1].base = (char *)txrec[0].base + hdrsz;
    txrec[1].len  = sizeof(bootp_packet);
    txrec[2].base = (char *)txrec[1].base + sizeof(bootp_packet);
    txrec[2].len  = trailsz;

    /* get mem for rx packet */
    sz = hdrsz + Protocol$QueryMTU((Protocol_clp)udp) + trailsz;
    rxrec[0].base = Heap$Malloc(card->rxheap, sz);
    rxrec[1].base = (char *)rxrec[0].base + hdrsz;
    rxrec[2].base = (char *)rxrec[1].base + mtu;
    /* rxrec[].len fields set later */

    udptx = Protocol$GetTX((Protocol_clp)udp);
    udprx = Protocol$GetRX((Protocol_clp)udp);

    /* prime recv */
    rxrec[0].len = hdrsz;
    rxrec[1].len = mtu;
    rxrec[2].len = trailsz;
    IO$PutPkt(udprx, 3, rxrec, 0, FOREVER);

    replied = 0;
    backoff = 1; /* 2 sec initial exponential backoff if no reply 1st time */
    while(!replied)
    {
	/* fill in the packet data */
	build_bootp(txrec[1].base, txrec[1].len, card->mac, &bootbase);

	/* send it */
	TRC(printf("bootp: sending request...\n"));
	IO$PutPkt(udptx, 3, txrec, 0, FOREVER);
	
	/* wait for a reply */
	if(IO$GetPkt(udprx, 3, rxrec, NOW() + SECONDS(backoff), 
		     &nrecs, &value)) {
	    replied = bootp_recv(rxrec[1].base, rxrec[1].len, card->mac, ret);

	    if (!replied)
	    {
		/* re-prime recv, since we'll need it again */
		rxrec[0].len = hdrsz;
		rxrec[1].len = mtu;
		rxrec[2].len = trailsz;
		IO$PutPkt(udprx, 3, rxrec, 0, FOREVER);
	    }
	} 

	/* pick up the tx buffer from the driver */
	IO$GetPkt(udptx, 3, txrec, FOREVER, &nrecs, &value);

	if (!replied)
	{
	    timeout -= backoff;
	    if (timeout < 0)
		return 1;  /* timed out */
	    backoff += backoff; /* XXX add randomness too! */
	    if (backoff > 60)
		backoff = 60; /* ceiling at 60 seconds */
	}
    }


    /* XXX tear down stack, PF$Dispose etc */

    return 0;
}


/* Called when a bootp packet arrives */
/* returns 0 = not happy
           1 = we happy */
static int bootp_recv(bootp_packet *p, int maxlen, Net_EtherAddr *mac,
		      bootp_retval *ret)
{
    char buf[80];

    TRC(printf("bootp: got reply\n"));
    if (p->op != OP_REPLY)
    {
	printf("bootp: packet wasn't a bootp reply (op code %d != 2)\n",
		p->op);
	return 0;
    }

    /* check the transaction id */
    if (memcmp(&p->transaction_id, ((char*)mac)+2, 4))
    {
	TRC(printf("bootp: ... but xid doesn't match\n"));
	return 0;	
    }

    memset(ret, 0x00, sizeof(*ret));
    ret->timeOffset = -1; /* invalid */
    memcpy(&ret->ip, &p->myIP, 4);
    memcpy(&ret->serverip, &p->serverIP, 4);
    memcpy(&ret->bootpgateway, &p->gatewayIP, 4);
    ret->bootfile = strdup(p->bootfile);

#define FMT_IP(_ip, _buf)			\
    sprintf(_buf, "%d.%d.%d.%d",		\
	    ((_ip & 0xff000000) >> 24),		\
	    ((_ip & 0x00ff0000) >> 16),		\
	    ((_ip & 0x0000ff00) >>  8),		\
	     (_ip & 0x000000ff))

    FMT_IP(ntohl(p->clientIP), buf);
    TRC(printf("bootp: client IP is %s\n", buf));
    FMT_IP(ntohl(p->myIP), buf);
    TRC(printf("bootp: My IP address is %s\n", buf));
    FMT_IP(ntohl(p->serverIP), buf);
    TRC(printf("bootp:     server IP is %s\n", buf));
    FMT_IP(ntohl(p->gatewayIP), buf);
    TRC(printf("bootp:    gateway IP is %s\n", buf));
    TRC(printf("bootp:      bootfile is `%s'\n", p->bootfile));

    parse_cookie(p, maxlen, ret);

    return 1;
}



static int build_bootp(bootp_packet *p, int maxlen, Net_EtherAddr *mac,
		       int *bootbase)
{
    if (sizeof(bootp_packet) > maxlen)
    {
	printf("build_bootp: not enough room in packet\n");
	return 1;
    }

    memset(p, 0, sizeof(bootp_packet));

    p->op    = OP_REQUEST;
    p->htype = HTYPE_ETHER;
    p->hlen  = HLEN;
    p->hops  = 0;  /* none, as yet */

    memcpy(&p->transaction_id, &(mac->a[2]), 4);

    TRC(printf("build_bootp: assigned transaction id (it is %x).\n",
	    p->transaction_id));

/* From RFC1542:
 * 3.2 Definition of the 'secs' field
 *
 *   The 'secs' field of a BOOTREQUEST message SHOULD represent the
 *   elapsed time, in seconds, since the client sent its first BOOTREQUEST
 *   message.  Note that this implies that the 'secs' field of the first
 *   BOOTREQUEST message SHOULD be set to zero.
 *
 *   Clients SHOULD NOT set the 'secs' field to a value which is constant
 *   for all BOOTREQUEST messages.
 */
    if ((*bootbase) != -1)
    {
	p->seconds = htons(((NOW() >> 30) & 0xffff) - (*bootbase));
    }
    else
    {
	(*bootbase) = (NOW() >> 30) & 0xffff;  /* note when boot started */
	p->seconds = 0;
    }

    memcpy(p->hwaddr, mac->a, 6);
    ADD_ID_TRAILER();

    /* Set the vendor information field to say we'd like to talk in
     * RFC1048 cookies (lastest version is currently RFC1497) */
    memcpy(p->vendor, VENDOR_RFC1048, 4);
    p->vendor[4] = TAG_END;  /* end of cookie list */
    
    return 0;
}


/* See if there are any vendor cookies, and parse them */
static void parse_cookie(bootp_packet *p, int maxlen, bootp_retval *ret)
{
    uint8_t *opt  = p->vendor;
    int      len  = maxlen - (((uint8_t *)&(p->vendor)) - ((uint8_t *)p));
    int      i;
    int      clen;
    /* a couple of scratch variables to print non-aligned values */
    uint32_t tmp32;
    uint16_t tmp16;

    if (!memcmp(p->vendor, VENDOR_RFC1048, 4))
    {
	TRC(printf("bootp: cookie vendor is RFC1048 (len=%d)\n", len));
	i=4;

	while(i < len)
	{
	    /* If we aren't at the end of the cookie and we will need
             * it extract len */
	    if ((i < len - 1) && (opt[i] != TAG_PAD) && (opt[i] != TAG_END))
		clen = opt[i+1];
	    else
		clen = 0;

	    /* If the length is over the cookie length, then warn and
             * truncate */
	    if (i + clen > len)
	    {
		printf("bootp: cookie: tag %d at %d (len %d) overruns\n",
		       opt[i], i, clen);
		clen = len - i;
	    }

	    switch(opt[i])
	    {
	    case TAG_END:
		i = len;
		break;

	    case TAG_PAD:
		i++;
		break;

	    case TAG_SUBNET_MASK:
		if (clen != 4)
		{
		    printf("bootp: cookie: truncated subnet mask\n");
		}
		else
		{
		    memcpy(&tmp32, opt+i+2, 4);
		    TRC(printf("bootp: cookie: subnet mask is %I\n",
			 tmp32));
		    ret->netmask = tmp32;
		}
		i += 6;
		break;

	    case TAG_TIME_OFFSET:
		memcpy(&tmp32, opt+i+2, 4);
		tmp32 = ntohl(tmp32);
		TRC(printf("bootp: cookie: time offset %d seconds from UTC\n",
		     tmp32));
		ret->timeOffset = tmp32;
		i += 2 + clen;
		break;

	    case TAG_GATEWAY:
		TRC(printf("bootp: cookie: gateways (%d):\n", clen / 4));
		ip_list(opt+i+2, clen, &ret->gateway);
		i += 2 + clen;
		break;

	    case TAG_TIME_SERVER:
		TRC(printf("bootp: cookie: timeservers (%d):\n", clen / 4));
		ip_list(opt+i+2, clen, &ret->timeserver);
		i += 2 + clen;
		break;

	    case TAG_DOMAIN_SERVER:
		TRC(printf("bootp: cookie: DNS servers (%d):\n", clen / 4));
		ip_list(opt+i+2, clen, &ret->nameserver);
		i += 2 + clen;
		break;

	    case TAG_LOG_SERVER:
		TRC(printf("bootp: cookie: log servers (%d):\n", clen / 4));
		ip_list(opt+i+2, clen, &ret->logserver);
		i += 2 + clen;
		break;

	    case TAG_LPR_SERVER:
		TRC(printf("bootp: cookie: lpr servers (%d):\n", clen / 4));
		ip_list(opt+i+2, clen, &ret->lprserver);
		i += 2 + clen;
		break;

	    case TAG_SWAP_SERVER:
		memcpy(&tmp32, opt+i+2, 4);
		TRC(printf("bootp: cookie: swap server %I\n", tmp32));
		ret->swapserver = tmp32;
		i += 2 + clen;
		break;

		/* NOTE WELL: rfc1497 deliberately doesn't specify
                 * whether this is a FQDN (fully-qualified domain
                 * name) or not */
	    case TAG_HOST_NAME:
	    {
		char buf[64]; /* 64 is enough, given size of vendor area */
		TRC(printf("bootp: cookie: host name \"%.*s\"\n",
		     clen, opt+i+2));
		/* ensure the string's NULL terminated */
		sprintf(buf, "%.*s", clen, opt+i+2);
		ret->hostname = strdup(buf);
		if (!ret->hostname)
		{
		    printf("bootp: out of memory (TAG_HOST_NAME)\n");
		    break;
		}
		i += 2 + clen;
		break;
	    }

	    case TAG_BOOT_SIZE:
		if (clen != 2)
		{
		    printf("bootp: cookie: truncated boot size\n");
		}
		else
		{
		    memcpy(&tmp16, opt+i+2, 2);
		    tmp16 = ntohs(tmp16);
		    TRC(printf("bootp: cookie: boot file size %d 512-byte blocks\n",
			 tmp16));
		    ret->bootfileSize = tmp16 * 512;
		}
		i += 2 + clen;
		break;

	    case TAG_DOMAIN_NAME:
	    {
		char buf[64];
		TRC(printf("bootp: cookie: domain name \"%.*s\"\n",
		     clen, &(opt[i+2])));
		sprintf(buf, "%.*s", clen, &(opt[i+2]));
		ret->domainname = strdup(buf);
		if (!ret->domainname)
		{
		    printf("bootp: out of memory (TAG_DOMAIN_NAME)\n");
		    break;
		}
		i += 2 + clen;
		break;
	    }

	    default:
		TRC(printf("bootp: unhandled tag:%d, len:%d\n", opt[i], clen));
		i += 2 + clen;
		break;
	    }
	}

	TRC(printf("bootp: done cookie processing\n"));
    }

    else if (!memcmp(p->vendor, VENDOR_NONE, 4))
    {
	/* do nothing */
    }

    else
    {
	printf("bootp: unhandled cookie vendor %d.%d.%d.%d\n",
	       p->vendor[0], p->vendor[1], p->vendor[2], p->vendor[3]);
    }
}


static void ip_list(uint8_t *opt, int clen, uint32_t *ret)
{
    int                 i;
    uint32_t            tmp32;

    for(i=0; i < clen; i += 4)
    {
	memcpy(&tmp32, opt+i, 4);
	TRC(printf("bootp: cookie:   + %I\n", tmp32));
	if (i == 0)  /* XXX return just the first, but print them all */
	    (*ret) = tmp32;
    }
}
