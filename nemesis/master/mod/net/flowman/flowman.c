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
**      flowman.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of FlowMan.if
** 
** ENVIRONMENT: 
**
**      Interface between client net library modules and generic
**      driver layer.
** 
** ID : $Id: flowman.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */

/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <ecs.h>
#include <IDCMacros.h>
#include <netorder.h>

#include <iana.h>

/* type checking */
#include <IPConf.ih>
#include <FlowMan.ih>

#include <Context.ih>
#include <ICMPMod.ih>
#include <Netif.ih>
#include <Net.ih>
#include <ARPMod.ih>
#include <Domain.ih>
#include <IO.ih>
#include <IOMacros.h>

#include <PF.ih>
#include <LMPFMod.ih>
#include <LMPFCtl.ih>

/* local interfaces */
#include "flowman_st.h"
#include "kill.h"
#include "proto_ip.h"
#include "ethernet.h"

/* Suck in other modules */
extern IPConf_op ipconf_ms;


#ifdef FLOWMAN_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* FLOWMAN_TRACE */

#ifdef DEFAULT_TRACE
#define DTRC(_x) _x
#else
#define DTRC(_x)
#endif /* DEFAULT_TRACE */

/* Define this to print an @-sign every second using eprint */
/*define FLOWMAN_HEARTBEAT*/


/* Entry point: applying this closure exports an IDCOffer for an IREF
 * IPConf to svc>net>IPConf, which allow the system administrator to 
 * create and configure IP hosts  */
static  Closure_Apply_fn Main;
static  Closure_op      cl_ms = {
  Main
};
static  const Closure_cl      cl_cl = { &cl_ms, NULL };
CL_EXPORT(Closure, IPConfMod, cl_cl);



/* FlowMan closure stuff */
static  FlowMan_Bind_fn                 Bind_m;
static  FlowMan_UnBind_fn               UnBind_m;
static  FlowMan_PassiveRoute_fn         PassiveRoute_m;
static  FlowMan_ActiveRoute_fn          ActiveRoute_m;
static  FlowMan_ARP_fn          	ARP_m;
static  FlowMan_RegisterInetCtl_fn      RegisterInetCtl_m;
static  FlowMan_Open_fn                 Open_m;
static  FlowMan_AdjustQoS_fn            AdjustQoS_m;
static  FlowMan_Close_fn                Close_m;
static  FlowMan_Attach_fn               Attach_m;
static  FlowMan_ReAttach_fn             ReAttach_m;
static  FlowMan_Detach_fn               Detach_m;

FlowMan_op      flowman_ms = {
  Bind_m,
  UnBind_m,
  PassiveRoute_m,
  ActiveRoute_m,
  ARP_m,
  RegisterInetCtl_m,
  Open_m,
  AdjustQoS_m,
  Close_m,
  Attach_m,
  ReAttach_m,
  Detach_m
};

/* Callbacks called when clients bind/die */
static  IDCCallback_Request_fn          IDCCallback_Request_m;
static  IDCCallback_Bound_fn            IDCCallback_Bound_m;
static  IDCCallback_Closed_fn           IDCCallback_Closed_m;

IDCCallback_op  fcback_ms = {
  IDCCallback_Request_m,
  IDCCallback_Bound_m,
  IDCCallback_Closed_m
};


#ifdef FLOWMAN_HEARTBEAT
static void alive_thd(void *data)
{
    while(1)
    {
	PAUSE(SECONDS(1));
	eprintf("@");
    }
}
#endif


/* forward declaration */
void default_packet_rec(struct default_rec_st *rec_info);
/* externel decl. */
extern int getDxIOs(Netif_clp netif, PF_clp txfilter, const char *purpose,
		    /* RETURNS */
		    PF_Handle   *rxhdl,    PF_Handle   *txhdl,
		    IO_clp      *rxio,     IO_clp      *txio,
		    IOOffer_clp *rxoffer,  IOOffer_clp *txoffer,
		    Heap_clp    *heap);


/* ------------------------------------------------------------ */
/* Main code: export svc>net>IPConf */

void Main(Closure_clp self)
{
    IPConf_st *st;

    TRC(printf("IPConf: starting up...\n"));

//    RW(Pvs(vp))->mm_ep = NULL_EP;

    /* allocate the state structure */
    st = Heap$Malloc(Pvs(heap), sizeof(IPConf_st));
    if (!st)
    {
	printf("IPConf init: no memory for state structure\n");
	return;
    }

    /* initialise the structure */
    st->iphosts = NULL;		/* no hosts */
    st->ifs = NULL;		/* no interfaces yet */

    /* build closure */
    st->cl.op = &ipconf_ms;
    st->cl.st = st;

    /* create a context for misc info we make available */
    st->svcnet = CX_NEW("svc>net");

#ifdef FLOWMAN_HEARTBEAT
    Threads$Fork(Pvs(thds), alive_thd, 0, 0);
#endif


    /* IDC export the IPConf interface */
    TRC(printf("IPConf: exporting myself\n"));
    IDC_EXPORT_IN_CX(st->svcnet, "IPConf", IPConf_clp, &st->cl);
}



/* ------------------------------------------------------------ */
/* Per-client callbacks */


/* Called when client binds, to see if we admit them */
static bool_t IDCCallback_Request_m(
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdom    /* IN */,
        const Binder_Cookies    *clt_cks        /* IN */,
        Binder_Cookies  *svr_cks        /* OUT */ )
{
    flowman_st UNUSED *st = self->st;

    TRC(printf("FlowMan callback: bind request from %qx\n", dom));

    return True; /* we accept the bind */
}


/* ------------------------------------------------------------ */
/* Domain-specific IREF FlowMan generation.
 * Called on bind for all clients to get their own private FlowMan
 * offer. Allocates and inits a new client state record (flowman_st) */
static bool_t IDCCallback_Bound_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
        Domain_ID       dom     /* IN */,
        ProtectionDomain_ID    pdom    /* IN */,
	const Binder_Cookies	*clt_cks /* IN */,
	Type_Any	*server /* INOUT */)
{
    iphost_st	*host = self->st;
    flowman_st	*st;

    st = Heap$Malloc(Pvs(heap), sizeof(*st));
    if (!st)
    {
	printf("FlowMan:new_client: out of memory\n");
	return False;
    }

    /* initialise the new client state */
    st->flows = NULL;
    st->host = host;
    st->dom = dom;
    st->pdom = pdom;
    st->inetctl = NULL;
    st->ctl_offer = NULL;
    CL_INIT(st->cl, &flowman_ms, st);

    /* note the existence of the client */
    st->next = host->clients;
    host->clients = st;

    /* state swizzle: the Type_Any we return is the newly inited st->cl */
    server->val = (word_t)&st->cl;

    TRC(printf("FlowMan callback: bind succeeded: iphost=%p, client=%p\n",
	       host, st));

    return True;
}


/* Called when client disappears (death or voluntary close) */
static void IDCCallback_Closed_m (
        IDCCallback_cl  *self,
        IDCOffer_clp    offer   /* IN */,
        IDCServerBinding_clp    binding /* IN */,
	const Type_Any *server /* IN */)
{
    iphost_st UNUSED	*host = self->st;
    FlowMan_clp		fman;
    flowman_st		*st;
    flow_t		*f, *nxt;

    /* need to cast server->val (a uint64_t) to a word_t first so gcc
     * on intel doesn't complain about making a pointer from an
     * integer of different size */
    fman = (void*)((word_t)server->val);
    st = fman->st;

    printf("flowman: domain %d died, cleaning up\n", st->dom & 0xffff);

    /* Garbage collect anything we allocated for this client: */
    /* ports, connections, packet filters, this state */

    /* go over the flow list, killing them all */
    f = st->flows;
    while(f)
    {
	nxt = f->next;
	kill_flow(f);
	f = nxt;
    }
    st->flows = NULL; /* a bit redundant, since this will shortly be freed */
	
    /* go over the port lists, killing those owned by the client */
    kill_ports_by_client(&st->host->UDPportuse, st);
    kill_ports_by_client(&st->host->TCPportuse, st);

    /* remove client from list */
    if (!DEL_LIST(st->host->clients, st))
	printf("FlowMan client cleanup: client %p not on client list?\n", st);

    free(st);
}



/* ------------------------------------------------------------ */
/* FlowMan methods */


static bool_t Bind_m(FlowMan_cl *self, /*IN OUT*/FlowMan_SAP *lsap)
{
    flowman_st    *st = self->st;

    if ((lsap->tag == FlowMan_PT_UDP) ||
	(lsap->tag == FlowMan_PT_TCP))
    {
	if (ip_alloc_port(st, lsap))
	    return True;
	else
	    return False;
    }
    else
    {
	printf("FlowMan$Bind: can't handle PF %d\n", lsap->tag);
	return False;
    }
}


static bool_t UnBind_m(FlowMan_cl *self, const FlowMan_SAP *lsap)
{
    flowman_st	*st = self->st;
    flow_t	*f;
    conn_t	*c;
    port_alloc_t *p, *prev, **plist;
    uint16_t	port;

    if ((lsap->tag != FlowMan_PT_UDP) &&
	(lsap->tag != FlowMan_PT_TCP))
    {
	printf("FlowMan$UnBind: unhandled PF %d\n", lsap->tag);
	return False;
    }

    port = lsap->u.UDP.port;

    /* is this client still using this port? */
    f = st->flows;
    while(f)
    {
	c = f->conns;
	while(c)
	{
	    if (((c->lsap.tag == FlowMan_PT_UDP) ||
		(c->lsap.tag == FlowMan_PT_TCP)) &&
		c->lsap.u.UDP.port == port)
		return False;
	    c = c->next;
	}
	f = f->next;
    }

    /* not in use, so we can free it */
    if (lsap->tag == FlowMan_PT_UDP)
	plist = &st->host->UDPportuse;
    else
	plist = &st->host->TCPportuse;

    p = *plist;
    prev = NULL;
    while(p)
    {
	if (p->port == port)
	{
	    if (prev)
		prev->next = p->next;
	    else
		(*plist) = p->next;
	    free(p);
	    return True;
	}
	p = p->next;
    }

    /* we didn't find the port? */
    printf("FlowMan$UnBind: port %d not bound\n", ntohs(port));
    return False;
}




static FlowMan_RouteInfoSeq *PassiveRoute_m(
        FlowMan_cl      *self,
        const FlowMan_SAP       *lsap   /* IN */ )
{
    flowman_st	*st = self->st;

    /* We currently don't route ATM SAPs */
    if (lsap->tag != FlowMan_PT_TCP &&
	lsap->tag != FlowMan_PT_UDP)
	RAISE_FlowMan$NoRoute();

    return ip_passiveroute(st, lsap);
}


static FlowMan_RouteInfo *ActiveRoute_m(
    FlowMan_cl      *self,
    const FlowMan_SAP *rsap)
{
    flowman_st	*st = (flowman_st *) self->st;

    /* We currently don't route ATM SAPs */
    if (rsap->tag != FlowMan_PT_TCP &&
	rsap->tag != FlowMan_PT_UDP)
	RAISE_FlowMan$NoRoute();

    return ip_route(st->host->routetab, rsap->u.UDP.addr.a, 0);
}


/* random helper funtion for interface name -> ifst mapping */
intf_st *ifname2ifst(iphost_st *host, const char *name)
{
    intf_st *ifst;

    ifst = host->ipconf->ifs;
    while(ifst)
    {
	if (!strcmp(ifst->name, name))
	    return ifst;
	ifst = ifst->next;
    }
    printf("ifname2ifst: no interface named '%s'\n", name);
    return NULL;
}


static void
ARP_m (
        FlowMan_cl      *self,
        uint32_t        ipaddr  /* IN */,
	Net_EtherAddr	*hwaddr /* OUT */)
{
    flowman_st    *st = (flowman_st *) self->st;
    FlowMan_RouteInfo *ri;
    intf_st *ifst;
    char buf[20];
    Net_EtherAddrP NOCLOBBER ret = NULL;

    /* route the IP address to see which interface we ARP out of */
    ri = ip_route(st->host->routetab, ipaddr, 0);
    TRY {
	ifst = ifname2ifst(st->host, ri->ifname);
	if (!ifst)
	{
	    printf("ARP: interface not found, can't happen\n");
	    RAISE_FlowMan$NoRoute();
	}
	
	sprintf(buf, "Eth10:%08x", ipaddr);
	
	/* NB: raises Context$NotFound() if ARP fails => ok (see Flowman.if) */
	
	ret = NAME_LOOKUP(buf, ifst->card->arper, Net_EtherAddrP);

    } FINALLY {
	free(ri->type);
	free(ri->ifname);
	free(ri);
    } ENDTRY;
    
    TRC(printf("FlowMan$ARP: ARP for %x returning "
	       "%02x:%02x:%02x:%02x:%02x:%02x\n",
	       ntohl(ipaddr),
	       ret->a[0], ret->a[1], ret->a[2],
	       ret->a[3], ret->a[4], ret->a[5]));

    memcpy(hwaddr, ret, sizeof(*hwaddr));
}


static void ctlbind_thread(void *data)
{
    flowman_st *st = data;

    /* XXX race: st can disappear out from thread's feet! */
    TRY {
	st->inetctl = IDC_BIND(st->ctl_offer, InetCtl_clp);
    } CATCH_ALL {
	printf("FlowMan$RegisterInetCtl: caught exception\n");
	st->inetctl = NULL;
    }
    ENDTRY;

    /* thread can now die */
}

static bool_t RegisterInetCtl_m(FlowMan_cl *self, IDCOffer_clp inetctl)
{
    flowman_st	*st = self->st;

    st->ctl_offer = inetctl;

    /* Start a thread to bind to the offer in the background */
    Threads$Fork(Pvs(thds), ctlbind_thread, st, 0);

    return True;
}


static FlowMan_Flow Open_m (
        FlowMan_cl      *self,
        string_t        ifname  /* IN */,
        const Netif_TXQoS       *txqos  /* IN */,
        Netif_TXSort    sort /* IN */,
   /* RETURNS */
        IOOffer_clp    *rxoffer,
        IOOffer_clp    *txoffer )
{
    flowman_st	*st = self->st;
    flow_t	*f;
    intf_st	*ifst;

    ifst = ifname2ifst(st->host, ifname);
    if (!ifst)
	return 0;

    f = Heap$Malloc(Pvs(heap), sizeof(*f));
    if (!f)
    {
	printf("FlowMan$Open: out of memory\n");
	return 0;
    }

    /* no data connections as yet */
    f->conns = NULL;
    /* remember which interface this flow is through */
    f->intf = ifst;
    /* no transmit filter as yet */
    f->txpf = NULL;

    /* Get IO channels from device driver */
    TRY {
	f->txoff = Netif$AddTransmitter(ifst->card->netif, txqos, sort, 
					&f->txhdl);
	f->rxoff = Netif$AddReceiver(ifst->card->netif, &f->rxhdl);
    } CATCH_ALL {
	free(f);
	RERAISE;
    }
    ENDTRY;

    /* link onto the flow list for this client */
    f->next = st->flows;
    st->flows = f;

    /* return the whole shebang */
    *rxoffer = f->rxoff;
    *txoffer = f->txoff;
    return (FlowMan_Flow)f;
}


static bool_t AdjustQoS_m(
        FlowMan_cl      *self,
        FlowMan_Flow  flow     /* IN */,
        const Netif_TXQoS       *qos    /* IN */ )
{
    flowman_st UNUSED *st = self->st;
    flow_t	*f = (flow_t *)flow;

    return Netif$SetQoS(f->intf->card->netif, f->txhdl, qos);
}


static bool_t Close_m(
        FlowMan_cl      *self,
        FlowMan_Flow  flow     /* IN */ )
{
    flowman_st UNUSED *st = self->st;
    flow_t	*f = (flow_t*)flow;

    /* is this really all there is to it? */
    kill_flow(f);

    return True;
}


static FlowMan_ConnID Attach_m(
        FlowMan_cl      *self,
        FlowMan_Flow  flow     /* IN */,
        const FlowMan_SAP       *lsap   /* IN */,
        const FlowMan_SAP       *rsap   /* IN */ )
{
    flowman_st *st = self->st;
    flow_t	*f = (flow_t *)flow;
    conn_t	*c;
    port_alloc_t *p;
    uint8_t	proto;

    switch (lsap->tag) {
    case FlowMan_PT_TCP:
	proto = IP_PROTO_TCP;
	p = st->host->TCPportuse;
	break;
    case FlowMan_PT_UDP:
	proto = IP_PROTO_UDP;
	p = st->host->UDPportuse;
	break;
    default:
	printf("FlowMan$Attach: unknown protocol %d\n", lsap->tag);
	return 0;
    }

    /* check the client owns the local port in question */
    while(p)
    {
	if (p->port == lsap->u.UDP.port)
	    break;
	 p = p->next;
    }
    if (!p)
    {
	printf("FlowMan$Attach: domain %qx attempted to attach "
	       "to unbound port %d\n",
	       st->dom,
	       ntohs(lsap->u.UDP.port));
	return 0;
    }

    /* check client specified the correct local IP address */
    if (lsap->u.UDP.addr.a != f->intf->ipaddr)
    {
	printf("FlowMan$Attach: domain %qx attempted to attach "
	       "with bogus local IP address (%x != %x)\n",
	       st->dom,
	       ntohl(lsap->u.UDP.addr.a), ntohl(f->intf->ipaddr));
	return 0;
    }

    /* create a new conn_t */
    c = Heap$Malloc(Pvs(heap), sizeof(*c));
    if (!c)
    {
	printf("FlowMan$Attach: out of memory\n");
	return 0;
    }

    /* fill in the new structure */
    c->lsap = *lsap;
    c->rsap = *rsap;
    c->next = f->conns;
    f->conns = c;

    /* if this is the first attach, then create and set the transmit
     * filter, otherwise ignore the transmit stuff */
    /* XXX TX filter only gets set once! */
    if (!f->txpf)
    {
	f->txpf = LMPFMod$NewTX(f->intf->card->pfmod,
				f->intf->card->mac,	/* src mac */
				f->intf->ipaddr,	/* src ip */
				proto,
				lsap->u.UDP.port);
	Netif$SetTxFilter(f->intf->card->netif, f->txhdl, f->txpf);
    }

    add_conn(f, c);

    return (FlowMan_ConnID)c;
}


static bool_t ReAttach_m(
        FlowMan_cl      *self,
	FlowMan_Flow	flow,
        FlowMan_ConnID  cid     /* IN */,
        const FlowMan_SAP       *lsap   /* IN */,
        const FlowMan_SAP       *rsap   /* IN */ )
{
    flowman_st UNUSED *st = self->st;
    conn_t	*c = (conn_t *)cid;
    flow_t	*f = (flow_t *)flow;
    port_alloc_t *p;

    switch (lsap->tag) {
    case FlowMan_PT_TCP:
	p = st->host->TCPportuse;
	break;
    case FlowMan_PT_UDP:
	p = st->host->UDPportuse;
	break;
    default:
	printf("FlowMan$ReAttach: unknown protocol %d\n", lsap->tag);
	return False;
    }

    /* check the client owns the local port in question */
    while(p)
    {
	if (p->port == lsap->u.UDP.port)
	    break;
	 p = p->next;
    }
    if (!p)
    {
	printf("FlowMan$ReAttach: domain %qx attempted to attach "
	       "to unbound port %d\n",
	       st->dom,
	       ntohs(lsap->u.UDP.port));
	return False;
    }

    /* check client specified the correct local IP address */
    if (lsap->u.UDP.addr.a != f->intf->ipaddr)
    {
	printf("FlowMan$ReAttach: domain %qx attempted to attach "
	       "with bogus local IP address (%x != %x)\n",
	       st->dom,
	       ntohl(lsap->u.UDP.addr.a), ntohl(f->intf->ipaddr));
	return 0;
    }

    /* remove old demux for this flow, and put the new one in */
    /* XXX TX stuff isn't changed */

    /* XXX race between del and add; need to make these atomic. */
    del_conn(f, c);
    /* take a copy of the new connection endpoints */
    c->lsap = *lsap;
    c->rsap = *rsap;
    add_conn(f, c);

    return True;
}


static bool_t Detach_m(
        FlowMan_cl      *self,
        FlowMan_Flow  flow     /* IN */,
        FlowMan_ConnID  cid     /* IN */ )
{
    flowman_st UNUSED *st = self->st;
    flow_t	*f = (flow_t *)flow;
    conn_t	*c = (conn_t *)cid;

    /* blow "c" from connection list */
    if (!DEL_LIST(f->conns, c))
    {
	printf("FlowMan$Detach: unknown CID %p\n", c);
	return False;
    }

    kill_conn(f, c); /* does implicit free() of c */

    /* was this the last connection? */
    if (!f->conns)
    {
	/* garbage collect the TX filter */
	Netif$SetTxFilter(f->intf->card->netif, f->txhdl, NULL);
	PF$Dispose(f->txpf);
	f->txpf = NULL;
	/* but not the RX stuff, cos this flow might get re-used */
    }

    return True;
}


/* Handle the recv of otherwise unhandled packets */
void default_packet_rec(struct default_rec_st *rec_info)
{
    card_st     *cdst;
    char	*rxbuf;
    word_t	value;
    IO_Rec	rxrecs[2];
    int		header_size = 128;
    int		mtu = 1514;	/* XXX hardwired constants */
    int		i;
    intf_st	*intfp; 
    iphost_st	*host_st;
    bool_t	ok = False;    
    Net_IPHostAddr ipaddr;
    Net_IPHostAddr *ipaddrp;
    char	buf[32];
    uint32_t	rec_recs;       /* nr of recs the received in call */

    TRC(printf("packet recv running\n"));

    cdst    = rec_info->mycard;
    host_st = rec_info->myhost;
    intfp   = rec_info->intf;


    sprintf(buf, "svc>net>%s>ipaddr", intfp->name);
    ipaddrp = NAME_FIND(buf, Net_IPHostAddrP);
    ipaddr = *ipaddrp;

    intfp->def_txfilt = LMPFMod$NewTXDef(cdst->pfmod, cdst->mac, ipaddr.a);

    getDxIOs(cdst->netif, intfp->def_txfilt, "DEF",
		     /* RETURNS: */
		     &intfp->def_rxhdl, &intfp->def_txhdl,
		     &intfp->io_rx,      &intfp->io_tx,
		     &intfp->def_rxoff, &intfp->def_txoff,
		     &intfp->def_heap);

    /* sanity check what we got  */
    if (intfp->def_rxoff == NULL || intfp->def_rxhdl==0)
        printf("flowman: error: bad recv handle or offer\n");
    if (intfp->io_tx == NULL)
        printf("flowman: default handler: tx bind failed\n");    
    if (intfp->io_rx == NULL)
        printf("flowman: default handler: rx bind failed\n");
    if (intfp->def_heap == NULL)
        printf("flowman: default handler: def_heap == NULL\n");

    ok = Netif$SetTxFilter(cdst->netif, intfp->def_txhdl, intfp->def_txfilt);
    if (!ok)
        printf("flowman: cannot set tx filter\n");

    /* install default filter */
    ok = LMPFCtl$SetDefault(cdst->rx_pfctl, intfp->def_rxhdl);
    if (!ok)
        printf("flowman: cannot set rx default filter\n");

    /* set xxstat to 0 */
    memset(&host_st->ipstat,  0, sizeof(ipstat_st));
    memset(&host_st->tcpstat, 0, sizeof(tcpstat_st));
    memset(&host_st->udpstat, 0, sizeof(udpstat_st));

     /* Thiemo: should be something with time of day, so that it grows
      * after crashing */
    host_st->ip_id = NOW() & 0xffff;

    mtu = ALIGN4(mtu);

#define PIPEDEPTH 16
    rxbuf = Heap$Malloc(intfp->def_heap, PIPEDEPTH * (mtu + header_size));
    
    for(i=0; i<PIPEDEPTH; i++)
    {
        /* chop up memory */
        rxrecs[0].base = rxbuf + i*(mtu+header_size);
        rxrecs[0].len  = header_size;
        rxrecs[1].base = rxbuf + i*(mtu+header_size) + header_size;
        rxrecs[1].len  = mtu;

        /* send recs */
//	TRC(printf("prime : %p+%d %p+%d \n",
//		   rxrecs[0].base, rxrecs[0].len,
//		   rxrecs[1].base, rxrecs[1].len));

	/* Actually, want to skip the first 2 bytes of header, so Ethernet
	 * frames land mis-aligned, but IP layer and up is correctly
	 * aligned */
	((char *)rxrecs[0].base) += 2;
	rxrecs[0].len -= 2;
        if (!IO$PutPkt(intfp->io_rx, 2, rxrecs, 0, 0))
	    printf("flowman: default prime %d failed\n", i);

//        TRC(printf("prime %d sent\n", i));
    }

    while (1)
    {
        /* loop and get incoming packets */
	DTRC(printf("flowman: default: waiting for packet..\n"));
        
        IO$GetPkt(intfp->io_rx, 2, rxrecs, FOREVER, &rec_recs, &value);
	((char *)rxrecs[0].base) -= 2;
	rxrecs[0].len  += 2;

        DTRC(printf("flowman: got packet on default channel, "
		    "nr of IO_Recs %d, rec[0].len=%d\n",
		    rec_recs, rxrecs[0].len));

        ether_input(rxrecs, rec_recs, cdst->mac, host_st);

        /* send down an empty packet after adapting to orig. size */
	/* XXX check base */
        rxrecs[0].len = header_size;
        rxrecs[1].len = mtu-header_size;

	/* again, send down the version which is advanced slightly */
	((char *)rxrecs[0].base) += 2;
	rxrecs[0].len -= 2;
	IO$PutPkt(intfp->io_rx, 2, rxrecs, 0, 0);
    }
}



/* End of flowman.c */

