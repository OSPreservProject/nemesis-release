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
**      ipconf.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of IPConf.if
** 
** ENVIRONMENT: 
**
**      Setup and startup of IP related networking
** 
** ID : $Id: ipconf.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */

/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <IOMacros.h>

/* type checking */
#include <IPConf.ih>
#include <FlowMan.ih>

#include <Context.ih>
#include <HeapMod.ih>
#include <ICMPMod.ih>
#include <Netif.ih>
#include <ARPMod.ih>

#include <PF.ih>
#include <LMPFMod.ih>
#include <LMPFCtl.ih>

/* local interfaces */
#include "flowman_st.h"
#include "kill.h"

#ifdef IPCONF_DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif

static  IPConf_NewIPHost_fn             NewIPHost_m;
static  IPConf_FreeIPHost_fn            FreeIPHost_m;
static  IPConf_OpenCard_fn              OpenCard_m;
static  IPConf_CloseCard_fn             CloseCard_m;
static  IPConf_NewInterface_fn          NewInterface_m;
static  IPConf_FreeInterface_fn         FreeInterface_m;
static  IPConf_AddInterface_fn          AddInterface_m;
static  IPConf_RemoveInterface_fn       RemoveInterface_m;
static  IPConf_AddRoute_fn              AddRoute_m;
static  IPConf_DelRoute_fn              DelRoute_m;
static  IPConf_bootp_fn                 bootp_m;


IPConf_op       ipconf_ms = {
  NewIPHost_m,
  FreeIPHost_m,
  OpenCard_m,
  CloseCard_m,
  NewInterface_m,
  FreeInterface_m,
  AddInterface_m,
  RemoveInterface_m,
  AddRoute_m,
  DelRoute_m,
  bootp_m
};



/* Import other interfaces */
extern ICMPMod_cl *icmpmod;
extern IDCCallback_op  fcback_ms;
extern FlowMan_op      flowman_ms;


/* prototypes */
int getDxIOs(Netif_clp netif, PF_clp txfilter, const char *purpose,
		    /* RETURNS */
		    PF_Handle   *rxhdl,    PF_Handle   *txhdl,
		    IO_clp      *rxio,     IO_clp      *txio,
		    IOOffer_clp *rxoffer,  IOOffer_clp *txoffer,
		    Heap_clp    *heap);

/* extern decl */
extern void default_packet_rec(struct default_rec_st *rec_info);

/* ------------------------------------------------------------ */
/* IP configuration interface */

/* Create a new IP host with FlowMan interface, and initialise its
 * state */
static IPConf_IPHost NewIPHost_m(
   IPConf_cl    *self,
   string_t     name)
{
    IPConf_st		*st = self->st;
    iphost_st		*host;
    IDCTransport_clp	shmt;
    Type_Any		any;
    IDCOffer_clp	flowmanoffer;

    host = Heap$Malloc(Pvs(heap), sizeof(iphost_st));
    if (!host)
    {
	printf("Out of memory allocating IP host state\n");
	return 0; /* NULL */
    }

    host->name = strdup(name);
    host->ipconf = st;
    host->UDPportuse = NULL;  /* no ports allocated yet */
    host->TCPportuse = NULL;
    host->clients = NULL;
    host->routetab = NULL;
    host->ifs = NULL;
    /* XXX more state init goes here */

    /* "host->cl" is a placeholder IREF FlowMan.  When clients bind,
     * a callback uses "host" to initialise a fresh client state
     * record.  */
    CL_INIT(host->cl, NULL, NULL);
    CL_INIT(host->fcback, &fcback_ms, host);

    /* register the ip host in the IPConf list, so we can check that
     * IP addresses arn't re-used */
    host->next = st->iphosts;
    st->iphosts = host;

    /* make the offer */
    shmt = NAME_FIND("modules>ShmTransport", IDCTransport_clp);
    ANY_INIT(&any, FlowMan_clp, &host->cl);
    flowmanoffer = IDCTransport$Offer(shmt, &any, &host->fcback,
				      Pvs(heap), Pvs(gkpr),
				      Pvs(entry), &host->service);

    /* and export the offer to >svc>net>name */
    CX_ADD_IN_CX(st->svcnet, name, flowmanoffer, IDCOffer_clp);

    return host;    
}


static bool_t FreeIPHost_m (
   IPConf_cl    *self,
   IPConf_IPHost        iphost  /* IN */ )
{
    IPConf_st     *st = self->st;
    iphost_st	*host = (iphost_st *)iphost;

    /* find and remove it from the linked list */
    if (!DEL_LIST(st->iphosts, host))
    {
	printf("FreeIPHost: unknown host %p\n", host);
	return False;
    }

    /* XXX need to clear out all interfaces and ports */
    printf("FreeIPHost: awooga! Not fully implemented\n");

    /* cleanup large quantities of state */
    free(host->name);
    IDCService$Destroy(host->service);
    free(host);

    /* any more? */

    return True;
}

/* ------------------------------------------------------------ */
/* Card handling */

static IPConf_Card OpenCard_m (
   IPConf_cl    *self,
   string_t     name    /* IN */ )
{
    IPConf_st UNUSED *st = self->st;
    NOCLOBBER Netif_clp	netif;
    card_st	*cdst;
    bool_t	done;
    char	buf[80];
    char 	* NOCLOBBER curname;
    char	*end;
    char	*dupname;

    TRC(printf("OpenCard(\"%s\") called\n", name));

    /* try to bind to dev>"name">control, and do a Netif$Up() on it */
    /* XXX hack: "name" might be a comma separated list; try them in
     * turn */
    dupname=strdup(name);
    done = False;

    /* loop until we have something we like */
    for(;;)
    {
	Type_Any any;

	/* back to the first name */
	curname = dupname;

	/* try each in turn */
	while(curname != (void*)1)
	{
	    end = strchr(curname, ',');
	    if (end) *end = 0;

	    sprintf(buf, "dev>%.40s>control", curname);
	    TRC(printf("OpenCard: looking for %s\n", buf));
	    if (Context$Get(Pvs(root), buf, &any))
	    {
		TRY {
		    TRC(printf("OpenCard: binding to Netif offer\n"));
		    netif = IDC_OPEN(buf, Netif_clp);
		    done = True;
		} CATCH_ALL {
		    fprintf(stderr, "OpenCard: bind to %s failed: "
			    "unhandled exception\n", buf);
		    netif = NULL;
		    done = True;
		} ENDTRY;
	    }
	    if (done) break;

	    if (end) *end=',';
	    curname = end+1;
	}
	if (done) break;

	PAUSE(MILLISECS(500));
    }

    if (!netif)		/* bind failed */
	return 0;

    if (!Netif$Up(netif))
	RAISE_IPConf$InUse();

    /* looks plausable, so allocate a state structure for the card */
    cdst = Heap$Malloc(Pvs(heap), sizeof(card_st));
    if (!cdst)
    {
	printf("OpenCard: out of memory\n");
	return 0;
    }

    cdst->netif    = netif;
    cdst->name     = strdup(curname);
    cdst->bootpres = NULL; /* not bootp'ed yet */

    /* can now free the mangled name list */
    free(dupname);

    /* get the MAC address */
    sprintf(buf, "dev>%.40s>addr", cdst->name);
    cdst->mac = NAME_FIND(buf, Net_EtherAddrP);

    /* and the protection domain of the driver */
    sprintf(buf, "dev>%.40s>pdom", cdst->name);
    cdst->pdom = NAME_FIND(buf, ProtectionDomain_ID);

    cdst->rxheap = NULL;    /* we get this on initial bind */
    cdst->txheap = NULL;    /*  "" */

    /* get the filtering stuff up */
    cdst->pfmod = NAME_FIND("modules>LMPFMod", LMPFMod_clp);
    cdst->rx_pf = LMPFMod$NewRX(cdst->pfmod, &cdst->rx_pfctl);
    Netif$SetRxFilter(netif, cdst->rx_pf);

    TRC(printf("OpenCard succeeded; cdst=%p\n", cdst));
    return cdst;
}


static void CloseCard_m (
   IPConf_cl    *self,
   IPConf_Card  card    /* IN */ )
{
    IPConf_st     *st = self->st;
    card_st	*cdst = (card_st *)card;
    intf_st	*thisif, *nxt;

    /* kill all interfaces on this card */
    thisif = st->ifs;
    while(thisif)
    {
	nxt = thisif->next;
	if (thisif->card == cdst)
	    FreeInterface_m(self, thisif);
	thisif = nxt;
    }

    Netif$SetRxFilter(cdst->netif, NULL);
    PF$Dispose(cdst->rx_pf);

    /* any more for any more? */

    Netif$Down(cdst->netif);

    if (cdst->bootpres)
    {
	bootp_retval *bp = cdst->bootpres;
	if (bp->bootfile)
	    free(bp->bootfile);
	if (bp->hostname)
	    free(bp->hostname);
	if (bp->domainname)
	    free(bp->domainname);
	free(bp);
    }

    Heap$Free(Pvs(heap), cdst);
}


/* ------------------------------------------------------------ */
/* Interface handling */

/* Convert a textual dotted quad IP address to a network endian IP address */
int get_ip(const char *txt, uint32_t *ip)
{
    uint32_t ret;
    int a, b, c, d;

    ret = sscanf(txt, "%d.%d.%d.%d", &a, &b, &c, &d);
    if (ret != 4)
	return 0;

    if (a < 0 || a > 255 ||
	b < 0 || b > 255 ||
	c < 0 || c > 255 ||
	d < 0 || d > 255)
	return 0;

    ret = (a<<24) | (b<<16) | (c<<8) | d;
    (*ip) = htonl(ret);

    return 1;
}


/* Create a context in a well-known place describing this interface,
 * in enough detail that clients can correctly format packets that go
 * through it */
static void new_cx_if(IPConf_st *st, intf_st *ifst)
{
    TRC(printf("new_cx_if(): intf_st at %p\n", ifst));

    {
	/* make 'intfname' context (do _not_ add it yet) */
	ContextMod_clp cmod = NAME_FIND ("modules>ContextMod", ContextMod_clp);
        ifst->cx = ContextMod$NewContext(cmod, Pvs(heap), Pvs(types));
    }

    /* Store the info into the net context. */
    CX_ADD_IN_CX(ifst->cx, "ipaddr", &ifst->ipaddr, Net_IPHostAddrP);
    /* no need for the netmask - client's don't need to know */
    CX_ADD_IN_CX(ifst->cx, "mac", ifst->card->mac, Net_EtherAddrP);
    CX_ADD_IN_CX(ifst->cx, "pdom", ifst->card->pdom, ProtectionDomain_ID);
    CX_ADD_IN_CX(ifst->cx, "rxheap", ifst->card->rxheap, Heap_clp);
    CX_ADD_IN_CX(ifst->cx, "txheap", ifst->card->txheap, Heap_clp);

    /* make a copy of the qosctl, if there was one */
    {
	Type_Any any;
	char buf[80];
	sprintf(buf, "dev>%s>qosctl", ifst->card->name);
	if (Context$Get(Pvs(root), buf, &any))
	    Context$Add(ifst->cx, "qosctl", &any);
    }

    /* if we have any bootp results, insert them here */
    if (ifst->card->bootpres)
    {
	bootp_retval	*bp = ifst->card->bootpres;
	/* unconditional entries */
	TRC(printf("Doing unconditional bootp results\n"));
	CX_ADD_IN_CX(ifst->cx, "bootfile", bp->bootfile, string_t);
	CX_ADD_IN_CX(ifst->cx, "serverIP", &bp->serverip, Net_IPHostAddrP);

	/* entries from cookies */
	if (bp->hostname)
	    CX_ADD_IN_CX(ifst->cx, "myHostname", bp->hostname, string_t);
	if (bp->domainname)
	    CX_ADD_IN_CX(ifst->cx, "domainname", bp->domainname, string_t);
	if (bp->nameserver)
	    CX_ADD_IN_CX(ifst->cx, "nameserver",
			 &bp->nameserver, Net_IPHostAddrP);
	if (bp->bootfileSize)
	    CX_ADD_IN_CX(ifst->cx, "bootfileSize", bp->bootfileSize, uint32_t);
	if (bp->swapserver)
	    CX_ADD_IN_CX(ifst->cx, "swapserver", bp->swapserver, uint32_t);
	/* ... etc etc (I haven't put all of them in here yet) */
    }

    /* Finally, link it into the main namespace */
    TRC(printf("linking into main namespace\n"));
    TRY {
	CX_ADD_IN_CX(st->svcnet, ifst->name,
		     ifst->cx, Context_clp);
    } CATCH_ALL {
	printf("NewInterface: exception while creating context\n");
    } ENDTRY;
}

static void del_cx_if(intf_st *ifst)
{
    /* remove the interface context from svc>net */
    Context$Remove(ifst->host->ipconf->svcnet, ifst->name);

    /* and remove contents of the context */
    Context$Destroy(ifst->cx);
}

static IPConf_Interface NewInterface_m(
   IPConf_cl    *self,
   IPConf_Card  card    /* IN */,
   string_t	name,
   string_t     ipaddr  /* IN */,
   string_t     netmask /* IN */ )
{
    IPConf_st   *st = self->st;
    intf_st	*ifst;
    intf_st	*p;
    card_st	*cdst = card;
    uint32_t	ip, mask;

    TRC(printf("NewInterface(self=%p, card=%p, name=%s, ipaddr=%s, "
	       "netmask=%s)\n",
	       self, card, name, ipaddr, netmask));	       

    if (!get_ip(ipaddr, &ip))
    {
	printf("NewInterface: '%s' is an invalid dotted-quad address\n",
	       ipaddr);
	return 0;
    }

    /* check this IP address is not already in use */
    /* XXX load balancing / striping might require this check not to
     * be enforced */
    p = st->ifs;
    while(p)
    {
	if (p->ipaddr == ip)
	{
	    printf("NewInterface: %s already bound to an interface\n", ipaddr);
	    return 0;
	}
	p = p->next;
    }

    if (!get_ip(netmask, &mask))
    {
	printf("NewInterface: '%s' is an invalid dotted-quad address\n");
	return 0;
    }

    ifst = Heap$Malloc(Pvs(heap), sizeof(intf_st));
    if (!ifst)
    {
	printf("NewInterface: out of memory\n");
	return 0;
    }

    /* fill in the state */
    ifst->card = cdst;
    ifst->host = NULL;  /* none yet */
    ifst->name = strdup(name);
    ifst->ipaddr = ip;		/* net order */
    ifst->netmask = mask;	/* net order */

    /* create the ICMP handler for this interface */
    {
	Net_IPHostAddr	myip;
	ARPMod_clp	arpmod;
	IO_clp		rxio;
	IO_clp		txio;
	/* XXX this should be kept so we can clean it up: */
	IOOffer_clp	tx_offer;
	PF_Handle	txhandle;
	PF_clp		arp_txfilt;
	IOOffer_clp	rx_offer;
	PF_Handle	rxhandle;

	myip.a   = ifst->ipaddr;

#if 0
	TRC(printf("NewInterface: creating ICMP handler\n"));
	ifst->icmp_txfilt = LMPFMod$NewTXICMP(cdst->pfmod,
					      cdst->mac,
					      ifst->ipaddr);

	/* AND: we use the txheap as the rxheap here, since we want to
	 * be able to loop packets that we've received back into onto
	 * the transmit side */
	{
	    Heap_clp heap;
	    getDxIOs(cdst->netif, ifst->icmp_txfilt, "ICMP",
		     /* RETURNS: */
		     &ifst->icmp_rxhdl, &ifst->icmp_txhdl,
		     &rxio,             &txio,
		     &ifst->icmp_rxoff, &ifst->icmp_txoff,
		     &heap);

	    LMPFCtl$SetICMP(cdst->rx_pfctl, ifst->icmp_rxhdl, ifst->ipaddr);

	    ifst->icmp = ICMPMod$New(icmpmod, rxio, txio,
				     heap, heap, 
				     &myip, cdst->mac, name);
	    LMPFCtl$SetICMP(cdst->rx_pfctl, ifst->icmp_rxhdl, ifst->ipaddr);
	}
#endif
	/* XXX hacky: create the card's arper here.  Should really be
	 * created and freed by card, and configured here. */

	/* Install funky ARP context */
	TRC(printf("NewInterface: creating ARP module\n"));
	arpmod   = NAME_FIND("modules>ARPMod", ARPMod_clp);

	arp_txfilt = LMPFMod$NewTXARP(cdst->pfmod, cdst->mac);

	getIOs(cdst->netif, arp_txfilt, "ARP",
	       /*RETURNS:*/
	       &rxhandle,	&txhandle,
	       &rxio,		&txio,
	       &rx_offer,	&tx_offer,
	       &cdst->rxheap,	&cdst->txheap);

	LMPFCtl$SetARP(cdst->rx_pfctl, rxhandle);

	cdst->arper = ARPMod$New(arpmod, rxio, txio,
				 cdst->rxheap, cdst->txheap, 
				 &myip, cdst->mac);
    }

    /* link it into the interface list */
    ifst->next = st->ifs;
    st->ifs = ifst;

    /* create the context that describes it */
    new_cx_if(st, ifst);

    return ifst;
}


static bool_t FreeInterface_m (
   IPConf_cl    *self,
   IPConf_Interface     intf    /* IN */ )
{
    IPConf_st     *st = self->st;
    intf_st	  *ifst = (intf_st *)intf;

    /* if this is still in use, remove it */
    if (ifst->host)
	RemoveInterface_m(self, ifst->host, ifst);

    /* delete the information context */
    del_cx_if(ifst);

    /* remove it from linked lists */
    if (!DEL_LIST(st->ifs, ifst))
    {
	printf("FreeInterface: %p not found on unused list (can't happen?)\n",
	       ifst);
	return False;
    }

    /* blow the ICMP stuff */
    LMPFCtl$SetICMP(ifst->card->rx_pfctl, 0, ifst->ipaddr);
    PF$Dispose(ifst->icmp_txfilt);
    ICMP$Dispose(ifst->icmp);

    /* other resources should have been freed by RemoveInterface() */
    free(ifst);

    return True;
}


static bool_t AddInterface_m (
   IPConf_cl    *self,
   IPConf_IPHost        iphost  /* IN */,
   IPConf_Interface     intf    /* IN */ )
{
    IPConf_st UNUSED *st = self->st;
    intf_st	  *ifst = (intf_st*)intf;
    iphost_st	  *host = (iphost_st*)iphost;
    /* Thiemo */
    intf_st       *help;
    struct default_rec_st       *rec_info;


    /* is this interface already in an IP host? */
    if (ifst->host)
    {
	printf("AddInterface: intf %p already in a host (%p)\n",
	       ifst, ifst->host);
	return False;
    }

    ifst->host = host;
    /* Thiemo */
    if (host->ifs == NULL) {
	host->ifs = ifst;
	ifst -> next = NULL;
    }
    else {  /* put new interface in front of list of this host */
	help = host->ifs;
	host->ifs = ifst;
	ifst->next = help;
	printf("ipconf: new interface added in front of interface-list of host\n");
    }


    /* XXX start Thread that will listen for default channel here */
    rec_info = Heap$Malloc(Pvs(heap), sizeof(struct default_rec_st));
    if(rec_info == NULL) {
	printf("AddInterface: no memory for default recv_info\n");
	return False;
    }
    rec_info->mycard = ifst->card;
    rec_info->myhost = host;
    TRC(printf("found card with name %s\n", ifst->card->name));
    rec_info->intf = ifst;
    
    Threads$Fork(Pvs(thds), default_packet_rec, rec_info, 16*8192);


    return True;
}


static bool_t RemoveInterface_m (
   IPConf_cl    *self,
   IPConf_IPHost        iphost  /* IN */,
   IPConf_Interface     intf    /* IN */ )
{
    IPConf_st UNUSED *st = self->st;
    iphost_st	*host = iphost;
    intf_st	*ifst = intf;

    if (ifst->host != host)
    {
	printf("RemoveInterface: %p is not in host %p\n", ifst, host);
	return False;
    }

    /* pull the plug on network activity though this interface */

    /* blow routes through this interface */
    kill_routes_by_intf(&ifst->host->routetab, ifst);

    /* don't need to blow port allocations, since they are not to a
     * specific interface. But we _do_ need to kill flows using the
     * interface: */
    kill_flows_by_intf(ifst->host, ifst);

    /* mark it as no longer in a host anymore */
    ifst->host = NULL;

    return True;
}


/* ------------------------------------------------------------ */
/* Routes */

static bool_t AddRoute_m (
   IPConf_cl    *self,
   IPConf_IPHost        iphost  /* IN */,
   string_t     destnet /* IN */,
   string_t     destmask        /* IN */,
   IPConf_Interface     intf    /* IN */,
   string_t     gateway /* IN */ )
{
    IPConf_st UNUSED *st = self->st;
    route_t	*rt;
    uint32_t	ip, mask;
    route_t	*p;
    intf_st	*ifst = intf;

    if (!get_ip(destnet, &ip))
    {
	printf("AddRoute: bad dotted-quad IP address '%s'\n", destnet);
	return False;
    }
	
    if (!get_ip(destmask, &mask))
    {
	printf("AddRoute: bad dotted-quad IP address '%s'\n", destnet);
	return False;
    }

    rt = Heap$Malloc(Pvs(heap), sizeof(route_t));
    if (!rt)
	return False;

    rt->destip = (ip & mask);	/* canonical form */
    rt->destmask = mask;

    if (*gateway == '\000')
    {
	rt->gateway = 0;	/* no gateway needed */
    }
    else
    {
	if (!get_ip(gateway, &rt->gateway))
	{
	    printf("AddRoute: invalid gateway addr '%s'\n", gateway);
	    goto abort;
	}
    }

    rt->intf = ifst;

    /* check this doesn't clash with an existing route */
    p = ifst->host->routetab;
    while(p)
    {
	if (p->destip == rt->destip)
	{
	    printf("AddRoute: already have a route to %I/%I\n",
		   rt->destip, rt->destmask);
	    goto abort;
	}
	p = p->next;
    }

    /* link new route in */
    rt->next = ifst->host->routetab;
    ifst->host->routetab = rt;

    TRC(printf("AddRoute %I/%I -> %p (via %I)\n",
	       rt->destip, rt->destmask, rt->intf, rt->gateway));

    return True;

abort:
    free(rt);
    return False;
}


static bool_t DelRoute_m (
   IPConf_cl    *self,
   IPConf_IPHost        iphost  /* IN */,
   string_t     destnet /* IN */,
   string_t     destmask        /* IN */,
   IPConf_Interface     intf    /* IN */,
   string_t     gateway /* IN */ )
{
    IPConf_st UNUSED *st = self->st;
    iphost_st	*host = (iphost_st *)iphost;
    route_t	*p;
    route_t	*prev;
    uint32_t	ip, mask, gate;

    if (!get_ip(destnet, &ip))
    {
	printf("DelRoute: '%s' is not a valid IP address\n", destnet);
	return False;
    }

    if (!get_ip(destmask, &mask))
    {
	printf("DelRoute: '%s' is not a valid netmask\n", destmask);
	return False;
    }

    if (!get_ip(gateway, &gate))
    {
	printf("DelRoute: '%s' is not a valid gateway\n", gateway);
	return False;
    }

    ip &= mask;

    prev = NULL;
    p = host->routetab;
    while(p)
    {
	if (p->destip == ip &&  /* this condition should be enough... */
	    p->intf == intf &&  /* ...but we might as well check the others */
	    p->gateway == gate)
	{
	    if (prev)
		prev->next = p->next;
	    else
		host->routetab = p->next;
	    return True;
	}
	prev = p;
	p = p->next;
    }
    
    printf("DelRoute: no route to %s/%s (via %s)\n",
	   destnet, destmask, gateway);
    return False;
}


/* ------------------------------------------------------------ */
/* bootp */

static bool_t bootp_m (
   IPConf_cl    *self,
   IPConf_Card   card        /* IN */,
   uint32_t     timeout /* IN */
   /* RETURNS */,
   string_t     *ipaddr,
   string_t     *netmask,
   string_t     *gateway )
{
    IPConf_st UNUSED *st = self->st;
    card_st	*cdst = (card_st*)card;
    int 	ret;
    bootp_retval *b;
    char	buf[20];

    b = Heap$Malloc(Pvs(heap), sizeof(*b));
    if (!b)
    {
	printf("IPConf: out of memory allocating bootp info structure\n");
	return True;
    }

    TRC(printf("IPConf: doing bootp on card %s...\n", cdst->name));
    ret = bootp(cdst, timeout, b);
    if (ret)
    {
	printf("IPConf: bootp timed out\n");
	(*ipaddr) = strdup(""); 
	(*netmask) = strdup("");
	(*gateway) = strdup("");
	return True;
    }

    cdst->bootpres = b;

    /* play IDC memory games */
    sprintf(buf, "%I", b->ip);
    (*ipaddr) = strdup(buf);

    sprintf(buf, "%I", b->netmask);
    (*netmask) = strdup(buf);

    sprintf(buf, "%I", b->gateway);
    (*gateway) = strdup(buf);

    return False; /* we didn't time out */
}




/* ------------------------------------------------------------ */
/* Helper functions */


/* This is the same bindIO as in ip.c */
static IO_clp bindIO(IOOffer_cl *io_offer, HeapMod_cl *hmod, Heap_clp *heap,
		     const char *purpose)
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

	    TRC(eprintf("bindIO: reusing heap at %p, len=%x (%s)\n",
			SEQ_ELEM(shm, 0).buf.base,
			SEQ_ELEM(shm, 0).buf.len,
			purpose));
	    
	    cb = IOOffer$ExtBind(io_offer, shm, Pvs(gkpr), &any);
	    if(!ISTYPE(&any, IO_clp)) {
		eprintf("ipconf.c [bindIO]: IOOffer$ExtBind failed.\n");
		if(cb) IDCClientBinding$Destroy(cb);
		RAISE_TypeSystem$Incompatible();
	    }
	    res = NARROW (&any, IO_clp);
	} else { 
	    /* Just bind to offer and create a heap afterwards. */
	    res = IO_BIND(io_offer);
	    shm = IO$QueryShm(res); 
	    if(SEQ_LEN(shm) != 1) 
		eprintf("ipconf.c [bindIO]: got > 1 data areas in channel!\n");

	    /* Ignore extra areas for now */
	    *heap = HeapMod$NewRaw(hmod, SEQ_ELEM(shm, 0).buf.base, 
				   SEQ_ELEM(shm, 0).buf.len);
	    TRC(eprintf("bindIO: new heap at %p, len=%x (%s)\n",
			SEQ_ELEM(shm, 0).buf.base, 
			SEQ_ELEM(shm, 0).buf.len,
			purpose));
	}

	return res; 
    } 
    
    return (IO_cl *)NULL;
}


/* Publicly exported function that creates two new Rbuf channels, one for TX,
 * one for RX.  Returns useful info about the channels, in particular:
 *   o the packet filter handle
 *   o a bound IO
 *   o the original offer
 *   o an appropriate heap
 * If the heap argument is NULL, a new heap is created, otherwise it
 * is used as a pre-existing heap.
 */
int getIOs(Netif_clp netif, PF_clp txfilter, const char *purpose,
	   /* RETURNS */
	   PF_Handle   *rxhdl,    PF_Handle   *txhdl,
	   IO_clp      *rxio,     IO_clp      *txio,
	   IOOffer_clp *rxoffer,  IOOffer_clp *txoffer,
	   Heap_clp    *rxheap,   Heap_clp    *txheap)
{
    Netif_TXQoS txqos = {SECONDS(10), 0, True};
    HeapMod_clp hmod  = NAME_FIND("modules>HeapMod", HeapMod_clp);

    /* get an offer for the TX side */
    *txoffer = Netif$AddTransmitter(netif, &txqos, 
				    Netif_TXSort_Native, 
				    /* OUT */ txhdl);
    Netif$SetTxFilter(netif, *txhdl, txfilter);

    /* now for the RX side */
    *rxoffer = Netif$AddReceiver(netif, /*OUT*/rxhdl);
    /* users of this function need to setup their own receive filter */

    /* now bind to the IOOffers, and sort out some memory */
    {
	char buf[80];
	sprintf(buf, "%s RX", purpose);
	*rxio = bindIO(*rxoffer, hmod, rxheap, buf);
	sprintf(buf, "%s TX", purpose);
	*txio = bindIO(*txoffer, hmod, txheap, buf);
    }

    return 0;
}


static IO_clp bindStr(IOOffer_clp io_offer, Stretch_clp str,
		      const char *purpose)
{
    IOData_Shm *shm;
    Type_Any    any;
    IDCClientBinding_clp cb;

    shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));

    SEQ_ELEM(shm, 0).buf.base =
	Stretch$Range(str, &(SEQ_ELEM(shm, 0).buf.len));
    SEQ_ELEM(shm, 0).param.attrs  = 0;
    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;

    TRC(eprintf("bindStr: new str is at %p, len=%x (%s)\n",
		SEQ_ELEM(shm, 0).buf.base,
		SEQ_ELEM(shm, 0).buf.len,
		purpose));
	    
    cb = IOOffer$ExtBind(io_offer, shm, Pvs(gkpr), &any);
    if(!ISTYPE(&any, IO_clp)) {
	eprintf("ipconf.c [bindStr]: IOOffer$ExtBind failed.\n");
	if(cb) IDCClientBinding$Destroy(cb);
	RAISE_TypeSystem$Incompatible();
    }

    return NARROW(&any, IO_clp);
}


/* Same as above, but arrange that the heap is suitable for both TX
 * and RX IO channels. */
int getDxIOs(Netif_clp netif, PF_clp txfilter, const char *purpose,
		    /* RETURNS */
		    PF_Handle   *rxhdl,    PF_Handle   *txhdl,
		    IO_clp      *rxio,     IO_clp      *txio,
		    IOOffer_clp *rxoffer,  IOOffer_clp *txoffer,
		    Heap_clp    *heap)
{
    Netif_TXQoS	txqos = {SECONDS(10), 0, True};
    HeapMod_clp	hmod  = NAME_FIND("modules>HeapMod", HeapMod_clp);
    IOData_Shm	*shm;
    Stretch_clp	str;

    /* get an offer for the TX side */
    *txoffer = Netif$AddTransmitter(netif, &txqos, 
				    Netif_TXSort_Native, 
				    /* OUT */ txhdl);
    Netif$SetTxFilter(netif, *txhdl, txfilter);

    /* now for the RX side */
    *rxoffer = Netif$AddReceiver(netif, /*OUT*/rxhdl);
    /* users of this function need to setup their own receive filter */

    /* work out how much buffering we need */
    shm = IOOffer$QueryShm(*txoffer);
    /* XXX AND: should really reconcile needs of both RX and TX sides;
     * just use TX for now */

    /* Get a stretch that's read/write with the pdom of the offer's
     * server */
    str = Gatekeeper$GetStretch(Pvs(gkpr),
				IDCOffer$PDID((IDCOffer_clp)(*txoffer)),
				SEQ_ELEM(shm, 0).buf.len,
				SET_ELEM(Stretch_Right_Read) |
				SET_ELEM(Stretch_Right_Write),
				PAGE_WIDTH,
				PAGE_WIDTH);

    /* now bind to the IOOffers, and sort out some memory */
    {
	char buf[80];
	sprintf(buf, "%s RX", purpose);
	*rxio = bindStr(*rxoffer, str, buf);
	sprintf(buf, "%s TX", purpose);
	*txio = bindStr(*txoffer, str, buf);
    }

    /* make a heap out of the stretch */
    *heap = HeapMod$New(hmod, str);

    return 0;
}
