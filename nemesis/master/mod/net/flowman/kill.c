/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      kill.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      free, deallocate, or deconfigure streams, flows, ports etc
** 
** ENVIRONMENT: 
**
**      FlowMan internals
** 
** ID : $Id: kill.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>

#include <Netif.ih>
#include <PF.ih>
#include <InetCtl.ih>
#include <LMPFCtl.ih>

#include <netorder.h>

#include "flowman_st.h"
#include "kill.h"

#ifdef KILL_TRACE
#define TRC(x) x
#else
#define TRC(x)
#endif

/* Turn off demuxing of stream "s" from flow "f", freeing the memory
 * used by "s".  You still need to unlink "s" from any lists it might
 * be on, and free ports that are no longer in use. */
void kill_conn(flow_t *f, conn_t *c)
{
    del_conn(f, c);
    free(c);
}


/* Stop transmissions on "f", and don't demux receives anymore.  Also frees
 * the memory used in conn_t descriptors and the flow descriptor */
void kill_flow(flow_t *f)
{
    conn_t *c, *nxt;

    TRC(printf("kill_flow(%p)\n", f));

    /* disable TX, then blow away TX IO */
    Netif$SetTxFilter(f->intf->card->netif, f->txhdl, NULL);
    Netif$RemoveTransmitter(f->intf->card->netif, f->txhdl);
    if (f->txpf)
	PF$Dispose(f->txpf); /* free filter if it existed */

    /* similar for RX side; first reconfigure so demux doesn't happen to
     * the IO, then blow the IO channel out the water */
    c = f->conns;
    while(c)
    {
	nxt = c->next;
	kill_conn(f, c);
	c = nxt;
    }

    /* Now RX IO should no longer get any traffic, so blow it away */
    Netif$RemoveReceiver(f->intf->card->netif, f->rxhdl);

    free(f);
}


/* Walk over all clients in "host", checking each flow in each client.
 * If the flow goes via "intf", it is killed using kill_flow().  Used
 * on interface removal. */
void kill_flows_by_intf(iphost_st *host, intf_st *intf)
{
    flowman_st	*client;
    flow_t	*f, *next, *prev;

    /* foreach "client" in "host" */
    client = host->clients;
    while(client)
    {
	/* foreach "flow" in "client" */
	f = client->flows;
	prev = NULL;
	while(f)
	{
	    next = f->next;
	    if (f->intf == intf)
	    {
		/* notify client (XXX and hope this isn't recursive IDC) */
		if (client->inetctl)
		    InetCtl$LostFlow(client->inetctl, f);
		kill_flow(f);
		if (prev)
		    prev->next = next;
		else
		    client->flows = next;
	    }
	    else
	    {
		prev = f;
	    }
	    f = next;
	}
	client = client->next;
    }
}


/* Kill ports in "plist" owned by "client" */
void kill_ports_by_client(port_alloc_t **plist, flowman_st *client)
{
    port_alloc_t *p, *prev, *nxt;

    p = *plist;

    prev = NULL;
    while(p)
    {
	nxt = p->next;
	if (p->client == client)
	{
	    if (prev)
		prev->next = p->next;
	    else
		(*plist) = p->next;
	    free(p);
	}
	else
	{
	    prev = p;
	}
	p = nxt;
    }
}


/* Walk over "routetab", freeing any routes that go through "intf" */
void kill_routes_by_intf(route_t **routetab, intf_st *intf)
{
    route_t *r, *prev, *next;

    r = *routetab;
    prev = NULL;

    while(r)
    {
	next = r->next;
	if (r->intf == intf)
	{
	    if (prev)
		prev->next = r->next;
	    else
		(*routetab) = r->next;
	    free(r);
	}
	else
	{
	    prev = r;
	}
	r = next;
    }
}


/* ------------------------------------------------------------ */
/* non-freeing (utility) functions */


/* Reconfigure the RX packet filter */
void del_conn(flow_t *f, conn_t *c)
{
    switch(c->lsap.tag) {
    case FlowMan_PT_TCP:
	if (!LMPFCtl$DelTCP(f->intf->card->rx_pfctl,
			    c->lsap.u.TCP.port,	/* dest port */
			    c->rsap.u.TCP.port,	/* src port */
			    c->rsap.u.TCP.addr.a,	/* src addr */
			    f->rxhdl))
	    printf("del_conn(tcp): unknown connection: r:%08x/%04x l:%04x\n",
		   c->rsap.u.TCP.addr.a, c->rsap.u.TCP.port,
		   c->lsap.u.TCP.port);	   
	break;

    case FlowMan_PT_UDP:
	if (!LMPFCtl$DelUDP(f->intf->card->rx_pfctl,
			   c->lsap.u.UDP.port,	/* dest port */
			   c->rsap.u.UDP.port,	/* src port */
			   c->rsap.u.UDP.addr.a,	/* src addr */
			   f->rxhdl))
	    printf("del_conn(udp): unknown connection: r:%08x/%04x l:%04x\n",
		   c->rsap.u.UDP.addr.a, c->rsap.u.UDP.port,
		   c->lsap.u.UDP.port);
	break;

    default:
	printf("del_conn: unknown SAP %d\n", c->lsap.tag);
    }
}


void add_conn(flow_t *f, conn_t *c)
{
    TRC(printf("add_conn: laddr=%08x, lport=%x; raddr=%08x, rport=%x\n",
	       ntohl(c->lsap.u.UDP.addr.a), ntohs(c->lsap.u.UDP.port),
	       ntohl(c->rsap.u.UDP.addr.a), ntohs(c->rsap.u.UDP.port)));
    switch(c->lsap.tag) {
    case FlowMan_PT_TCP:
    	LMPFCtl$AddTCP(f->intf->card->rx_pfctl,
		       c->lsap.u.TCP.port,	/* dest port */
		       c->rsap.u.TCP.port,	/* src port */
		       c->rsap.u.TCP.addr.a,	/* src addr */
		       f->rxhdl);
	break;

    case FlowMan_PT_UDP:
    	LMPFCtl$AddUDP(f->intf->card->rx_pfctl,
		       c->lsap.u.UDP.port,	/* dest port */
		       c->rsap.u.UDP.port,	/* src port */
		       c->rsap.u.UDP.addr.a,	/* src addr */
		       f->rxhdl);
	break;

    default:
	printf("add_conn: unknown SAP %d\n", c->lsap.tag);
    }
}
