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
**      What is it?
** 
** FUNCTIONAL DESCRIPTION:
** 
**      What does it do?
** 
** ENVIRONMENT: 
**
**      Where does it do it?
** 
** ID : $Id: flowman_st.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#ifndef _flowman_st_h_
#define _flowman_st_h_

#include <Context.ih>
#include <Net.ih>
#include <Netif.ih>
#include <ICMP.ih>
#include <IDCCallback.ih>
#include <FlowMan.ih>
#include <IPConf.ih>
#include <LMPFMod.ih>
#include <LMPFCtl.ih>
#include "prot_stat.h"


/* Forward declarations: */
typedef struct _intf_st intf_st;
typedef struct _iphost_st iphost_st;
typedef struct _bootp_retval bootp_retval;
typedef struct _route_t route_t;
typedef struct _ipstat_st ipstat_st;
typedef struct _udpstat_st udpstat_st;
typedef struct _tcpstat_st tcpstat_st;


/* A conn_t is data that's been multiplexed on top of a flow. */
typedef struct _conn_t {
    /* we keep SAPs here since data could be TCP or UDP (or even
     * something else) streams */
    FlowMan_SAP		lsap;	/* port reservation for this */
    FlowMan_SAP		rsap;
    struct _conn_t	*next;
} conn_t;

/* Structure describing a single flow (ie, QoS-regulated connection to
 * device driver, bearing some conns) */
typedef struct _flow_t {
    conn_t		*conns;
    PF_clp		txpf;	/* transmit filter */
    PF_Handle		txhdl;	/* transmit handle */
    PF_Handle		rxhdl;	/* receive handle */
    IOOffer_clp	        txoff;	/* TX offer from device driver */
    IOOffer_clp	        rxoff;	/* RX offer from device driver */
    intf_st		*intf;	/* interface it's to */
    struct _flow_t	*next;
} flow_t;


/* Per-domain client-specific information (FlowMan_st) */
typedef struct _flowman_st {
    FlowMan_cl		 cl;
    Domain_ID		 dom;		/* client domain ID */
    ProtectionDomain_ID	 pdom;		/* client pdom */
    InetCtl_clp		 inetctl;	/* client's callback interface */
    IDCOffer_clp	 ctl_offer;	/* offer for above */
    flow_t		*flows;		/* list of flows currently in action */
    iphost_st		*host;		/* the IP host state */
    IDCService_clp	 service;
    struct _flowman_st	*next;
} flowman_st; /* this can be used as a client id */


/* link list of these document port allocations: */
typedef struct _port_alloc_t {
    int		port;		/* the resource allocated (network endian) */
    flowman_st	*client;	/* who to */
    struct _port_alloc_t *next;
} port_alloc_t;

#define AUTO_BASE htons(16000) /* start port number for auto-allocated ones */


/* state for the IP configuration interface */
typedef struct {
    IPConf_cl	cl;
    iphost_st	*iphosts;	/* linked list of IP hosts */
    intf_st	*ifs;		/* linked list of all interfaces */
    Context_clp	svcnet;		/* svc>net context */
} IPConf_st;


/* state for an IP host */
struct _iphost_st {
    FlowMan_cl		cl;		/* template FlowMan for this IP host */
    char 		*name;		/* friendly name of this IP host */
    IDCService_clp	service;	/* server side control interface */
    IDCCallback_cl	fcback;		/* per-client callbacks */
    IPConf_st		*ipconf;
    route_t		*routetab;	/* routing table */
    /* port allocation tables */
    port_alloc_t	*UDPportuse;	/* UDP port table */
    port_alloc_t	*TCPportuse;	/* TCP port table */
    flowman_st		*clients;	/* list of all clients */
    struct _iphost_st	*next;
    intf_st             *ifs;           /* list of all interfaces on host */
    ipstat_st           ipstat;
    udpstat_st          udpstat;
    tcpstat_st          tcpstat;
    uint64_t            ip_id;          /* XXX sensible? current ip_id */
}; /* iphost_st */


/* per-card state */
/* The card layer is where multiplexing is controlled from. */
typedef struct _card_st {
    Netif_clp		netif;
    char		*name;   /* textual name within dev context */

    LMPFMod_cl		*pfmod;	 /* cache of modules>LMPFMod */
    PF_cl		*rx_pf;  /* receive packet filter */
    LMPFCtl_cl		*rx_pfctl; /* control interface for RX packet filter */

    Net_EtherAddr	*mac;    /* MAC address of this interface */

    /* XXX ARP needs to be able to claim to be multiple IP addrs */
    Context_cl		*arper;  /* active context that does ARP */

    ProtectionDomain_ID	pdom;    /* prot. domain of the driver for this i/f */
    Heap_clp    	rxheap;  /* heap flowman shares [r] with this i/f  */
    Heap_clp    	txheap;  /* heap flowman shares [rw] with this i/f   */

    bootp_retval	*bootpres;	/* result of bootp, if any */
} card_st;


/* per-interface state */
/* This is supposed to be IP-specific stuff */
struct _intf_st {
    card_st		*card;	 /* card this interface is over */
    iphost_st		*host;	 /* IP host this interface is in */
    char		*name;	 /* name in the >svc>net> context */

    /* ICMP related stuff */
    ICMP_cl		*icmp;   /* thing that answers ICMP */
    PF_Handle		icmp_txhdl;
    IOOffer_clp	        icmp_txoff;
    PF_Handle		icmp_rxhdl;
    IOOffer_clp	        icmp_rxoff;
    PF_clp		icmp_txfilt;
    /* Thiemo default stuff */
    PF_Handle		def_txhdl;
    IOOffer_clp	        def_txoff;
    PF_Handle		def_rxhdl;
    IOOffer_clp         def_rxoff;
    PF_clp		def_txfilt;
    Heap_clp            def_heap;
    IO_clp  io_tx;
    IO_clp  io_rx;
    uint32_t		ipaddr;  /* this interface's IP address */
    uint32_t		netmask; /* this interface's netmask */

    Context_clp		cx;	 /* context which describes this interface */

    intf_st		*next;	 /* linked list of all i/f on this card */
}; /* intf_st */



/* ------------------------------------------------------------ */
/* Routing stuff */

struct _route_t {
    uint32_t		destip;		/* destination IP address (net order)*/
    uint32_t		destmask;	/* netmask */
    uint32_t		gateway;	/* gateway to use (or 0 for none) */
    intf_st		*intf;		/* which interface to use */
    route_t		*next;
}; /* route_t */



/* Perform BOOTP on "if", stashing results in "st" */
struct _bootp_retval {
    uint32_t	ip;		/* our IP address */
    uint32_t	serverip;	/* bootp server who replied */
    uint32_t	bootpgateway;	/* who gateways bootp packets out of subnet */
    char	*bootfile;	/* path to the boot file */
    /* these may not be valid (they come from cookies): */
    uint32_t	netmask;	/* our netmask */
    uint32_t	gateway;	/* IP address of gateway out of this subnet */
    uint32_t	nameserver;	/* XXX first suggested nameserver */
    int32_t	timeOffset;	/* time offset from UTC in secs (-1=invalid) */
    uint32_t	timeserver;	/* XXX first suggested time server */
    uint32_t	logserver;	/* XXX first suggested log server */
    uint32_t	lprserver;	/* XXX first suggested print server */
    uint32_t	swapserver;	/* XXX first suggested swap server */
    char	*hostname;	/* might not be a FQDN */
    uint32_t	bootfileSize;	/* in bytes */
    
    char	*domainname;	/* DNS domainname */
};

/* struct that the default receiver thread gets as a parameter */
struct default_rec_st {
    card_st       *mycard;
    iphost_st     *myhost;
    IPConf_clp    ipconf_clp;
    IPConf_Interface intf;
};


/* functions */
extern int bootp(card_st *card, int32_t timeout, bootp_retval *retval);

extern int getIOs(Netif_clp netif, PF_clp txfilter, const char *purpose,
		  /* RETURNS */
		  PF_Handle   *rxhdl,    PF_Handle   *txhdl,
		  IO_clp      *rxio,     IO_clp      *txio,
		  IOOffer_clp *rxoffer,  IOOffer_clp *txoffer,
		  Heap_clp    *rxheap,   Heap_clp    *txheap);

/* List macros */

/* Delete "item" from "list", returing pointer to "item", or NULL for
 * failure */
#define DEL_LIST(list, item) 			\
({						\
    __typeof__(list) prev = NULL;		\
    __typeof__(list) p = list;			\
    while(p)					\
    {						\
	if (p == item)				\
	{					\
	    if (prev)				\
		prev->next = p->next;		\
	    else				\
		list = p->next;			\
	    break;				\
	}					\
	prev = p;				\
	p = p->next;				\
    }						\
    p;						\
})


#endif /* _flowman_st_h_ */
