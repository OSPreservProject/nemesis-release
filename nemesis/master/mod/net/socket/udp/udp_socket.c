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
**      udp_socket.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      UDP specific parts of socket library
** 
** ENVIRONMENT: 
**
**      Shared library in user space
** 
** ID : $Id: udp_socket.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <socket.h>
#include <netorder.h>
#include <iana.h>

#include <IDCMacros.h>

#include <FlowMan.ih>
#include <UDPMod.ih>
#include <HeapMod.ih>
#include <Net.ih>
#include <IOMacros.h>
#include <pktcx.h>
#include <ntsc.h>

#include "udp_socket.h"

#define PARANOID

#if 0
#define TRC(x) x
#define TRACING
#else
#define TRC(x)
#endif

/* prototypes for udp methods */
static int udp_socket_init(struct socket *sock);
static int udp_accept(struct socket *sock);
static int udp_bind(struct socket *sock, 
		    const struct sockaddr *address, int address_len);
static int udp_connect(struct socket *sock, 
		       const struct sockaddr *address, int address_len);
static int udp_get_addr_len(void);
static int udp_listen(struct socket *sock);
static int udp_recv(struct socket *sock, char *buffer, int length, int flags, 
	     struct sockaddr *address, int *address_len);
static int udp_send(struct socket *sock, const char *message, int len,
		    int flags, const struct sockaddr *dest_addr, int dest_len);
static int udp_close(struct socket *sock);
static int udp_select(struct socket *sock, int nfds, fd_set *readfds,
		      fd_set *writefds, fd_set *exceptfds,
		      struct timeval *timeout);

const proto_ops udp_ops= {
    udp_socket_init, 
    udp_accept, 
    udp_bind,
    udp_connect,
    udp_get_addr_len, 
    udp_listen,
    udp_recv,
    udp_send,
    udp_close,
    udp_select
};

const typed_ptr_t UDPSocketOps_export = TYPED_PTR(addr_t__code, &udp_ops);

/* prototype */
static int build_stack(struct udp_data *udp_data,
		       struct sockaddr_in *laddr,
		       struct sockaddr_in *raddr);


static int udp_socket_init(struct socket *sock)
{
    struct udp_data *udp_data;

    if(!(udp_data= (struct udp_data *)
	 Heap$Calloc(Pvs(heap), sizeof(struct udp_data), 1)))
    {
	printf("udp_socket_init: out of memory. oops.\n");
	return -1;
    }

    sock->data = udp_data;

    /* initialise the udp specific stuff XXX what is there to do? */

    /* get our binding (or most likely the copy cached by the object
     * table) */
    udp_data->fman = IDC_OPEN("svc>net>iphost-default", FlowMan_clp);

    TRC(eprintf("udp_skt init done, private data at %p\n", udp_data));
    return 0;
} 



/* 
** accept doesn't make much sense on udp
*/
static int udp_accept(struct socket *sock)
{
    printf("XXX bad call accept() on udp/dgram/inet socket.\n");
    return -1;
}


static int udp_bind(struct socket *sock, const struct sockaddr *address,
	     int address_len)
{ 
    struct sockaddr_in *inaddr= (struct sockaddr_in *)address;
    struct udp_data    *ud= (struct udp_data *)sock->data;    
    FlowMan_SAP sap;

    if(address->sa_family != AF_INET) {
	printf("udp_bind: non INET family %d\n", address->sa_family);
	return -1;
    }

    sap.tag = FlowMan_PT_UDP;
    sap.u.UDP.port = inaddr->sin_port;
    sap.u.UDP.addr.a = 0;  /* XXX bind() is for all interfaces anyway */


    if (!FlowMan$Bind(ud->fman, &sap))
	return -1;

    /* remember the address and port we were allocated, and hack the
     * client's copy */
    inaddr->sin_port = sap.u.UDP.port;
    memcpy(&(sock->laddr), inaddr, sizeof(struct sockaddr_in));

    sock->state = SS_BOUND;

    return 0;
}



/*
 * Limit the flow to only receiving from one remote end point.
 */
static int udp_connect(struct socket *sock, const struct sockaddr *address,
		       int address_len)
{
    struct sockaddr_in *inaddr= (struct sockaddr_in *)address;
    struct udp_data    *ud= (struct udp_data *)sock->data;    

    if(address->sa_family != AF_INET)
    {
	printf("udp_connect: non INET family %d\n", address->sa_family);
	return -1;
    }

    if (sock->state != SS_BOUND)
    {
	printf("udp_connect: socket is not in bound state (%d != %d)\n",
		sock->state, SS_BOUND);
	return -1;
    }

    memcpy(&(sock->raddr), inaddr, sizeof(struct sockaddr_in));

    /* build the stack if needed */
    if (sock->state != SS_CONNECTED)
    {
	if (!build_stack((struct udp_data *)sock->data,
			 (struct sockaddr_in *)&(sock->laddr),
			 (struct sockaddr_in *)&(sock->raddr)))
	{
	    printf("udp_connect: problem with building stack\n");
	    return -1;
	}
	sock->state = SS_CONNECTED;
    }
    else
    {
	FlowMan_SAP	lsap, rsap;
	struct sockaddr_in *laddr;
	struct sockaddr_in *raddr;
	FlowMan_ConnID	cid;
	
	/* the stack's already been built, so we just need to swizzle the
	 * packet filters (send() will retarget the stack as needed) */
	laddr = (struct sockaddr_in *)&sock->laddr;
	raddr = (struct sockaddr_in *)&sock->raddr;
	lsap.tag = FlowMan_PT_UDP;
	lsap.u.UDP.addr.a = laddr->sin_addr.s_addr;
	lsap.u.UDP.port = laddr->sin_port;
	rsap.tag = FlowMan_PT_UDP;
	rsap.u.UDP.addr.a = raddr->sin_addr.s_addr;
	rsap.u.UDP.port = raddr->sin_port;
	cid = UDP$GetConnID(ud->udp);
	if (!FlowMan$ReAttach(ud->fman, ud->flow, cid, &lsap, &rsap))
	{
	    printf("ReAttach failed!\n");
	    return False;
	}
    }

    return 0;
}


static int udp_get_addr_len(void)
{
    return sizeof(struct sockaddr_in);
}



/* 
** listen doesn't make much sense on UDP
*/
static int udp_listen(struct socket *sock)
{
    printf("XXX bad call listen on udp/dgram/inet socket.\n");
    return -1;
}


#define MAXRECS 18	/* which is enough for 6 fragments */
static int udp_recv(struct socket *sock, char *buffer, int length, int flags, 
		    struct sockaddr *address, int *address_len)
{
    struct sockaddr_in *inaddr  = (struct sockaddr_in *)address;
    struct udp_data    *udp_data= (struct udp_data *)sock->data;
    IO_Rec rxrecs[MAXRECS];
    word_t value;
    uint32_t nrecs;
    uint32_t cnt=0;
    int rxlen=0;
    int i;
    pktcx_t *pktcx, *np;

    MU_LOCK(&sock->lock);

    /* XXX really want to keep a domain-wide cache of connections */
    if(sock->state != SS_CONNECTED) /* stack needs building */
    {
	struct sockaddr_in *laddr = (struct sockaddr_in *)&(sock->laddr);

	/* remote address isn't used for anything in recv */
	build_stack(udp_data, laddr, NULL);
	sock->state = SS_CONNECTED;
    }

    /* done with global state, now need the acquire the receive resource
     * lock */
    MU_RELEASE(&sock->lock);
    MU_LOCK(&sock->rxlock);

    do {
	/* organise mem for header */
	IO$GetPkt(udp_data->rxio, MAXRECS, rxrecs, FOREVER, &nrecs, &value);
	pktcx = (void*)value;
	if (nrecs > MAXRECS)
	{
	    printf("udp_recv: %d recs needed, only %d supplied\n",
		   nrecs, MAXRECS);
	    goto fail;
	}
	if (!pktcx)
	{
	    printf("udp_recv: didn't get back a pktcx?\n");
	    goto fail;
	}
	if (nrecs > 0)
	{
	    for(i=1; i<nrecs; i++)
	    {
		if (rxrecs[i].len == 0)
		    continue;  /* skip empty recs */
		cnt = MIN(rxrecs[i].len, length);
		if (cnt == 0)
		    break;	/* out of client buffer */
		memcpy(buffer, rxrecs[i].base, cnt);
		buffer += cnt;
		length -= cnt;
		rxlen += cnt;
	    }
	}
	else /* nrecs == 0 */
	{
	    /* no valid data, but the packet context probably contains
	     * some recs for us, so recyle the buffers and try
	     * receiving again */
	    eprintf("udp: recycle (shouldn't happen)\n");
	    while(pktcx)
	    {
		np = pktcx->next;
		pktcx->next = NULL;	/* break any chain */
		pktcx->error = 0;
		pktcx->cid = 0;
		IO$PutPkt(udp_data->rxio, pktcx->ri_nrecs, pktcx->ri_recs,
			  (word_t)pktcx, FOREVER);
		pktcx = np;
	    }
	    /* back to the top... */
	}
    } while(nrecs == 0);

    if (cnt == 0)
    {
	printf("udp_recv: warning: truncated the received packet "
		"to fit buffer\n");
    }
    
    TRC(eprintf("udp_recv: got back %d recs.\n", nrecs));

    if (inaddr)
    {
	FlowMan_SAP sap;

	inaddr->sin_family = AF_INET;
	if (UDP$GetLastPeer(udp_data->udp, &sap))
	    inaddr->sin_addr.s_addr = sap.u.UDP.addr.a;
	else
	    inaddr->sin_addr.s_addr = 0;  /* IP layer not present */
	inaddr->sin_port = sap.u.UDP.port;
	*address_len = sizeof(*inaddr);
    }

    /* send empty buffer(s) back down */
    TRC(eprintf("udp_recv: sending buffers back down\n"));
    while(pktcx)
    {
	np = pktcx->next;
	pktcx->next = NULL;	/* break any chain */
	pktcx->error = 0;
	pktcx->cid = 0;
	nrecs = pktcx->ri_nrecs;
	for(i=0; i<nrecs; i++)
	    rxrecs[i] = pktcx->ri_recs[i];
	IO$PutPkt(udp_data->rxio, nrecs, rxrecs, (word_t)pktcx, FOREVER);
	pktcx = np;
    }

    MU_RELEASE(&sock->rxlock);
    return rxlen;

fail:
    MU_RELEASE(&sock->rxlock);
    return -1;
}


/* Retarget a stack that's already been build to a different
 * destination address, "daddr". "laddr" is supplied for convenience */
static bool_t retarget(struct udp_data *ud,
		       struct sockaddr_in *laddr,
		       struct sockaddr_in *daddr)
{
    uint32_t NOCLOBBER arpaddr;
    FlowMan_RouteInfo * NOCLOBBER ri = NULL;
    FlowMan_SAP rsap;
    Net_IPHostAddr ipaddr;
    Net_EtherAddr  hwaddr;

    /* route the new dest addr, and make sure we don't need to change
     * interface */
    rsap.tag = FlowMan_PT_UDP;
    rsap.u.UDP.addr.a = daddr->sin_addr.s_addr;
    rsap.u.UDP.port = daddr->sin_port;
    TRY {
	ri = FlowMan$ActiveRoute(ud->fman, &rsap);
    } CATCH_FlowMan$NoRoute() {
	ri = NULL;
    } ENDTRY;
    if (!ri)
    {
	printf("retarget: no route to host %x\n",
	       ntohl(daddr->sin_addr.s_addr));
	return False;
    }

    if (strcmp(ri->ifname, ud->curif))
    {
	printf("retarget: route to %x requires going through %s, but stack "
	       "is built to %s\n",
	       ntohl(daddr->sin_addr.s_addr),
	       ri->ifname,
	       ud->curif);
	free(ri->ifname);
	free(ri->type);
	free(ri);
	return False;
    }

    if (ri->gateway)
	arpaddr = ri->gateway;
    else
	arpaddr = daddr->sin_addr.s_addr;

    TRC(eprintf("retarget: ARPing for %I\n", arpaddr));
    {
	bool_t NOCLOBBER ok = True;
	TRY {
	    FlowMan$ARP(ud->fman, arpaddr, &hwaddr);
	} CATCH_Context$NotFound(UNUSED name) {
	    ok = False;
	} ENDTRY;
	if(!ok)
	    return False;
    }

    TRC(eprintf("Retargeted remote hwaddr: %02x:%02x:%02x:%02x:%02x:%02x%s\n",
		hwaddr.a[0], hwaddr.a[1], hwaddr.a[2],
		hwaddr.a[3], hwaddr.a[4], hwaddr.a[5],
		(ri->gateway)?" (using gateway)":""));

    free(ri->ifname);
    free(ri);

    Ether$SetDest(ud->eth, &hwaddr);
    ipaddr.a = daddr->sin_addr.s_addr;
    IP$SetDest(ud->ip, &ipaddr);
    UDP$SetTXPort(ud->udp, daddr->sin_port);

    return True;
}
    

static int udp_send(struct socket *sock, const char *message, int len,
		    int flags, const struct sockaddr *dest_addr, int dest_len)
{
    struct udp_data *udp_data= (struct udp_data *)sock->data;
    IO_Rec txrecs[3];
    struct sockaddr_in *laddr= (struct sockaddr_in *)&(sock->laddr);
    struct sockaddr_in *raddr= (struct sockaddr_in *)&(sock->raddr);
    int i;
    uint32_t nrecs;
    pktcx_t *pktcx;

    TRC(eprintf("udp_send: entered\n"));

    MU_LOCK(&sock->lock);

    if(sock->state != SS_CONNECTED) /* stack needs building */
    {
	if (dest_addr)
	{
	    *raddr = *(struct sockaddr_in *)dest_addr;
	}
	else
	{
	    MU_RELEASE(&sock->lock);
	    return -1; /* ENOTCONN or summit */
	}

	TRC(eprintf("udp_send: building stack\n"));
	build_stack(udp_data, laddr, NULL);
	sock->state = SS_CONNECTED;

	/* now target it */
	TRC(eprintf("udp_send: retargeting to %I:%d\n",
		    raddr->sin_addr, raddr->sin_port));
	if(!retarget(udp_data, laddr, raddr)) {
	    TRC(eprintf("...retarget failed\n"));
	    /* ARP failed */
	    MU_RELEASE(&sock->lock);
	    return -1; /* EHOSTDOWN or sim */
	}
    }
    else
    {
	struct sockaddr_in *daddr = (struct sockaddr_in *)dest_addr;

	TRC(eprintf("udp_send: stack already built\n"));
	if (daddr->sin_addr.s_addr == raddr->sin_addr.s_addr
	    && daddr->sin_port == raddr->sin_port)
	{
	    TRC(eprintf("... and targeted correctly\n"));
	}
	else
	{
	    TRC(eprintf("... but incorrectly targeted; retargeting\n"));
	    /* retarget stack for this destination */
	    if(!retarget(udp_data, laddr, daddr)) {
		/* ARP failed */
		MU_RELEASE(&sock->lock);
		return -1; /* EHOSTDOWN or sim */
	    }
	    *raddr = *daddr;
	    TRC(eprintf("udp_send: retargeted stack\n"));
	}
    }

    /* done with global state, now need the acquire the receive resource
     * lock */
    MU_RELEASE(&sock->lock);
    MU_LOCK(&sock->txlock);

    /* organise mem for headers */
    txrecs[0].len  = udp_data->head;
    txrecs[0].base = udp_data->txbuf;

    /* 
    ** XXX SMH: for the moment, we must copy the data to be transmitted
    ** onto a heap which is readable by the driver domain. This is not, 
    ** however, the most efficient way one can imagine doing transmit.
    ** Some alternatives that should be investigated are:
       a) make the driver enter kernel mode s.t. it can read the data.
          This is essentially what used to happen before coz drivers
	  ran in kernel mode the entire time. 
       b) map the clients data readable to the driver domain on the fly.
          This would require some way of getting hold of the stretch
	  containing the message buffer, and might be a very bad idea
	  indeed coz lots of other stuff would be mapped too.
       c) force clients to give us data which is readable by the driver
          pdom. This is trickier than it sounds since in general clients
	  won't know the pdom of the driver for the interface upon which
	  their message will eventually be transmitted. Though we could
	  arrange for all network drivers to be in the same pdom (!?!).
    ** Even apart from those considerations, this temporary solution should
    ** be modified s.t. the mallocing is done out of band, or not at all
    ** [i.e. perhaps have a stretch, not a heap, and then can just memcpy
    **  to the start of this. requires some locking though ;-]
    */
    txrecs[1].len  = len;
    if(!(txrecs[1].base = Heap$Malloc(udp_data->txheap, len))) {
	eprintf("udp_send: failed to malloc txrec buf!\n"); 
	MU_RELEASE(&sock->txlock); 
	return -1;   /* error */
    }
    memcpy((char *)txrecs[1].base, message, len);

    if(!(pktcx = Heap$Calloc(udp_data->txheap, sizeof(pktcx_t), 1))) {
	eprintf("udp_send: failed to malloc packet context!\n"); 
	Heap$Free(udp_data->txheap, txrecs[1].base);
	MU_RELEASE(&sock->txlock); 
	return -1;   /* error */
    }

    pktcx->ri_nrecs = 2;
    for(i=0; i<2; i++)
	pktcx->ri_recs[i] = txrecs[i];

    TRC(eprintf("udp_send: About to IO$PutPkt...\n"));


    if (!IO$PutPkt(udp_data->txio, 2, txrecs, (word_t)pktcx, FOREVER))
    {
	txrecs[1].len = -1;  /* error */
	goto out;
    }

    /* pick up the tx buffer from the driver */
    do {
	IO$GetPkt(udp_data->txio, 2, txrecs, NOW() + SECONDS(10),
		  &nrecs, (word_t*)&pktcx);
	if (nrecs == 0) {
	    printf("udp_send: timeout recovering tx buffers\n");
	} else {
	    if (nrecs != 2)
	    {
		printf("udp_send: didn't recover enough tx buffers (%d!=3)\n",
		       nrecs);
	    }
	}
    } while(nrecs != 2);
    if (!pktcx)
	printf("udp_send: didn't get packet context back?\n");
    if (pktcx && pktcx->error)
	printf("udp_send: error %d sending\n", pktcx->error);

out:
    Heap$Free(udp_data->txheap, txrecs[1].base);
    Heap$Free(udp_data->txheap, pktcx);

    MU_RELEASE(&sock->txlock);

    return txrecs[1].len;
}


static int udp_close(struct socket *sock)
{
    MU_LOCK(&sock->lock);
    Heap$Free(Pvs(heap), sock->data);
    /* XXX anything else? */
    /* YES: sort out state of socket XXX */
    MU_RELEASE(&sock->lock);
    return 0;
}


static int udp_select(struct socket *sock, int nfds,
		      fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		      struct timeval *timeout)
{
    IO_Rec rec;
    uint32_t nrecs;
    word_t value;
    Time_ns t;
    struct udp_data *udp_data= (struct udp_data *)sock->data;

    MU_LOCK(&sock->lock);

    /* XXX really want to keep a domain-wide cache of connections */
    if(sock->state != SS_CONNECTED) /* stack needs building */
    {
	struct sockaddr_in *laddr = (struct sockaddr_in *)&(sock->laddr);

	/* remote address isn't used for anything in recv */
	build_stack(udp_data, laddr, NULL);
	sock->state = SS_CONNECTED;
    }
    MU_RELEASE(&sock->lock);
    MU_LOCK(&sock->rxlock);

    if (timeout)
    {
	t = NOW() + 
	    timeout->tv_sec * SECONDS(1) + 
	    timeout->tv_usec * MICROSECS(1);
    }
    else
    {
	t = FOREVER;
    }

    value = 0;
    IO$GetPkt(udp_data->rxio, 0, &rec, t, &nrecs, &value);
    if (value)
    {
	eprintf("udp_select: got a packet cx?!?\n");
	ntsc_dbgstop();
    }

#ifdef PARANOID
    if (NOW() > t + SECONDS(2))
	printf("udp_select: warning: took >2 secs to return from select?\n");
#endif

    MU_RELEASE(&sock->rxlock);

    if (nrecs)
    {
//	eprintf("sel ok\n");
	return 1;  /* there's data available */
    }
    else
    {
//	eprintf("sel t/o\n");
	return 0;  /* no data, but not an error - we timed out */
    }
}


/* ------------------------------------------------------------ */
/* Support functions */

#ifdef TRACING
static void dump_rec(IO_Rec *recs, int nrecs, void *pc)
{
    int i;

    printf(" --%p: ", pc);
    for(i=0; i < nrecs; i++)
	printf("%p(%d) ", recs[i].base, recs[i].len);
    printf("\n");
}
#endif /* TRACING */


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
		eprintf("udp_socket.c [bindIO]: IOOffer$ExtBind failed.\n");
		if(cb) IDCClientBinding$Destroy(cb);
		RAISE_TypeSystem$Incompatible();
	    }
	    res = NARROW (&any, IO_clp);
	} else { 
	    /* Just bind to offer and create a heap afterwards. */
	    res = IO_BIND(io_offer);
	    shm = IO$QueryShm(res); 
	    if(SEQ_LEN(shm) != 1) 
		eprintf("udp_socket.c [bindIO]: "
			"got > 1 data areas in channel!\n");

	    /* Ignore extra areas for now */
	    *heap = HeapMod$NewRaw(hmod, SEQ_ELEM(shm, 0).buf.base, 
				   SEQ_ELEM(shm, 0).buf.len);
	}

	return res; 
    } 
    
    return (IO_cl *)NULL;
}


/* "udp_data" is where to build up the state.
 * "laddr" is the local address. Must be valid, or 0 to autoalloc.
 * "raddr" is the remote address. Can be NULL to indicate a receive stack,
 *   not yet configured for transmit 
 * Caller must hold sock->lock
 */
static int build_stack(struct udp_data *udp_data,
		struct sockaddr_in *laddr,
		struct sockaddr_in *raddr)
{
    FlowMan_SAP		lsap, rsap;
    FlowMan_RouteInfo	* NOCLOBBER ri=NULL;
    FlowMan_RouteInfoSeq * NOCLOBBER ris=NULL;
    IOOffer_clp	        rxoffer, txoffer;
    Net_EtherAddr	hwaddr;
    char		buf[32];
    EtherMod_cl        *ethmod;
    IPMod_cl           *ipmod;
    UDPMod_cl          *udpmod;
    int			i, j;
    IO_Rec		recs[3];
    pktcx_t            *pktcx;

    /*sockets get best effort transmit QoS */
    Netif_TXQoS		txqos = { SECONDS(1), 0, True }; 

    rsap.tag = FlowMan_PT_UDP;
    rsap.u.UDP.addr.a = (raddr)? raddr->sin_addr.s_addr : 0;
    rsap.u.UDP.port   = (raddr)? raddr->sin_port : 0;
    lsap.tag = FlowMan_PT_UDP;
    lsap.u.UDP.addr.a = laddr->sin_addr.s_addr;
    lsap.u.UDP.port   = laddr->sin_port;

    /* find the interface to listen on. */

    /* XXX we don't know if this is for incoming or outgoing. We can
     * guess, based on whether we have a raddr at all.  Should really
     * have multiple stacks for rx on all interfaces, and single for
     * transmit on outgoing one. For the moment, we'll route for
     * outgoing packets */
    if (raddr)  /* if we've got the raddr, then assume TX */
    {
	TRY {
	    ri = FlowMan$ActiveRoute(udp_data->fman, &rsap);
	} CATCH_FlowMan$NoRoute() {
	    ri = NULL;
	} ENDTRY;
	if (!ri)
	{
	    printf("build_stack: no route to host %x\n",
		   ntohl(rsap.u.UDP.addr.a));
	    return -1;
	}
	TRC(eprintf("build_stack: routed %x to intf %s, gateway %x\n",
		   ntohl(rsap.u.UDP.addr.a), ri->ifname, ntohl(ri->gateway)));
    }
    else /* assume RX */
    {
	TRY {
	    ris = FlowMan$PassiveRoute(udp_data->fman, &lsap);
	} CATCH_FlowMan$NoRoute() {
	    ris = NULL;
	} ENDTRY;
	if (!ris)
	{
	    printf("build_stack: address %x not on a local interface\n",
		   ntohl(lsap.u.UDP.addr.a));
	    return -1;
	}
	/* XXX for the moment, pick the first valid interface and use that */
	ri = &(SEQ_ELEM(ris, 0));
	TRC(eprintf("build_stack: %x is on intf %s\n",
		   ntohl(lsap.u.UDP.addr.a), ri->ifname));	
    }

    /* note the interface we're using so we can tell if it changes */
    udp_data->curif = strdup(ri->ifname);

    /* get the IP address of the interface, so we know what to put in
     * the source address field of the IP header */
    sprintf(buf, "svc>net>%s>ipaddr", ri->ifname);
    lsap.u.UDP.addr = *(NAME_FIND(buf, Net_IPHostAddrP));
    laddr->sin_addr.s_addr = lsap.u.UDP.addr.a;    

    /* Grab the rest of the stack modules we'll need: */
    /* get an ethernet module */
    ethmod = NAME_FIND("modules>EtherMod", EtherMod_clp);
    TRC(eprintf("socket_init: found an ethermod at %p\n", ethmod));
    /* get an IP module */
    ipmod = NAME_FIND("modules>IPMod", IPMod_clp);
    TRC(eprintf("socket_init: found an IP mod at %p\n", ipmod));
    /* get a UDP module */
    udpmod = NAME_FIND("modules>UDPMod", UDPMod_clp);
    TRC(eprintf("socket_init: found a UDP mod at %p\n", udpmod));

    /* First we lookup the source MAC address */
    {
	Net_EtherAddr	*ethaddr;
	sprintf(buf, "svc>net>%s>mac", ri->ifname);
	ethaddr = NAME_FIND(buf, Net_EtherAddrP);
	memcpy(&(udp_data->src_hw.a), ethaddr, 6);
    }

    /* if we have a remote addr, then arp for it
     * (or maybe the gateway to it) */
    if (raddr) {
	uint32_t NOCLOBBER addr;
	bool_t	NOCLOBBER ok = True;
	addr = (ri->gateway)? ri->gateway : rsap.u.UDP.addr.a;
	TRY {
	    FlowMan$ARP(udp_data->fman, addr, &hwaddr);
	} CATCH_Context$NotFound(UNUSED name) {
	    ok = False;
	} ENDTRY;
	if(!ok)
	    return -1; /* ARP failed => EHOSTDOWN or sim */
    } else
	hwaddr = (Net_EtherAddr){{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

    memcpy(&(udp_data->dst_hw.a), &hwaddr, 6);

    TRC(eprintf("Remote hwaddr: %02x:%02x:%02x:%02x:%02x:%02x%s\n",
	       hwaddr.a[0], hwaddr.a[1], hwaddr.a[2],
	       hwaddr.a[3], hwaddr.a[4], hwaddr.a[5],
	       (ri->gateway)?" (using gateway)":""));

    /* get some IO offers */
    TRC(eprintf("udp_build_stack: laddr=%x, lport=%d raddr=%x rport=%d\n",
	    lsap.u.UDP.addr.a, lsap.u.UDP.port,
	    rsap.u.UDP.addr.a, rsap.u.UDP.port));

    udp_data->flow =
	FlowMan$Open(udp_data->fman, ri->ifname, &txqos, Netif_TXSort_Socket,
		     &rxoffer, &txoffer);
    if (!udp_data->flow)
    {
	printf("udp_build_stack: FlowMan$Open failed\n");
	return -1;
    }
    
    /* can now get rid of routeing information */
    if (raddr)
    {
	free(ri->ifname);
	free(ri->type);
	free(ri);
    }
    else
    {
	/* XXX almost certainly leaks ifname memory */
#if 0
	/* keep this out until I can work out why it doesn't work with
	 * multiple cards. AND. */
	SEQ_FREE(ris);
#endif
    }

    {
	/* bind to the IO Offers, and get some memory */
	HeapMod_clp hmod = NAME_FIND("modules>HeapMod", HeapMod_clp);
	IO_clp      rxio, txio;

	rxio = bindIO(rxoffer, hmod, &udp_data->rxheap);
	txio = bindIO(txoffer, hmod, &udp_data->txheap);

	/* build the stack */
	udp_data->eth = EtherMod$New(ethmod, &(udp_data->dst_hw),
				     &(udp_data->src_hw),
				     FRAME_IPv4,
				     rxio, txio);
    }

    { 
	Net_IPHostAddr dst, src;
	
	dst.a = (raddr)? raddr->sin_addr.s_addr : 0;
	src.a = laddr->sin_addr.s_addr;
	
	udp_data->ip = IPMod$Piggyback(ipmod,
				       (Protocol_clp)udp_data->eth,
				       &dst, &src,
				       IP_PROTO_UDP, 127, 
				       udp_data->flow,
				       udp_data->txheap);
    }

    udp_data->udp = UDPMod$NewCustomIP(udpmod, udp_data->fman,
				       &lsap, &rsap, &txqos,
				       udp_data->ip);

    if (!udp_data->udp)
    {
	printf("udp_build_stack: UDPMod$NewCustomIP failed\n");
	Protocol$Dispose((Protocol_clp)udp_data->ip);
	return -1; /* XXX other clearup? */
    }


    udp_data->mtu = Protocol$QueryMTU((Protocol_clp)udp_data->udp);
    udp_data->mtu = ALIGN4(udp_data->mtu);
    udp_data->head= Protocol$QueryHeaderSize((Protocol_clp)udp_data->udp);
    udp_data->tail= Protocol$QueryTrailerSize((Protocol_clp)udp_data->udp);

    udp_data->txio = Protocol$GetTX((Protocol_clp)udp_data->udp);
    udp_data->rxio = Protocol$GetRX((Protocol_clp)udp_data->udp);

    /* allocate some space for the tx headers; we only require some for 
       a single head and tail, since we take the middle rec (the payload) 
       directly from user-space. */
    udp_data->txbuf = Heap$Malloc(udp_data->txheap,
				  (udp_data->head + udp_data->tail));
    if (!udp_data->txbuf)
	printf("udp_build_stack: oops, out of memory!\n");

    {
	/* stride on by pktsz for each packet */
	uint32_t pktsz = udp_data->mtu + udp_data->head + udp_data->tail;

	/* Allocate PIPE_DEPTH mtu's for rx; a depth of 12 comes 
	   around as (6 * 2).  6 for an 8Kb fragment, and 2 of them 
	   because you might have one partially reassembled datagram and 
	   one more coming in as a retransmission */
#define PIPE_DEPTH 12
	udp_data->rxbuf = Heap$Malloc(udp_data->rxheap, (PIPE_DEPTH * pktsz));

	/* We also allocate some space for packet contexts; currently this
	   comes from our txheap since this will tend to be overly large */
	udp_data->pktcx = Heap$Calloc(udp_data->txheap,
				      sizeof(pktcx_t), PIPE_DEPTH);
	if (!udp_data->pktcx)
	    printf("udp_build_stack: oops, out of memory (pktcxs)!\n");
	pktcx = udp_data->pktcx;
    


	/* prime the RX pipe */
	for(i=0; i < PIPE_DEPTH; i++)
	{
	    recs[0].base = udp_data->rxbuf + (pktsz * i);
	    recs[0].len  = udp_data->head;
	    recs[1].base = udp_data->rxbuf + (pktsz * i) + udp_data->head;
	    recs[1].len  = udp_data->mtu;
	    recs[2].base = udp_data->rxbuf + (pktsz * (i+1)) - udp_data->tail;
	    recs[2].len  = udp_data->tail;
#ifdef TRACING
	    dump_rec(recs, 3, pktcx);
#endif /* TRACING */
	    /* set up the packet context */
	    pktcx->ri_nrecs = 3;
	    for(j=0; j<3; j++)
		pktcx->ri_recs[j] = recs[j];
	    IO$PutPkt(udp_data->rxio, 3, recs, (word_t)pktcx, FOREVER);
	    pktcx++;
	}
    }
    
    return 0;
}


/* End of udp_socket.c */
