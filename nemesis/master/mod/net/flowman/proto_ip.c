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
**      proto_ip.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      IP protocol support for FlowMan
** 
** ENVIRONMENT: 
**
**      FlowManager internal
** 
** ID : $Id: proto_ip.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>
#include <exceptions.h>

#include <FlowMan.ih>

#include "flowman_st.h"
#include "proto_ip.h"



/* XXX TODO: keeping the port list sorted would make this O(nports)
 * rather than the O(n^2) it currently is! */
bool_t ip_alloc_port(flowman_st *client, FlowMan_SAP *lsap)
{
    port_alloc_t *p, **start;
    bool_t	autoalloc = False;
    uint16_t port = lsap->u.UDP.port;  /* UDP and TCP layed out the same */

    if (port == 0)
    {
	printf("flowman: using autoalloc\n");
	autoalloc = True;
	port = AUTO_BASE;
    }

    if (lsap->tag == FlowMan_PT_UDP)
	start = &client->host->UDPportuse;
    else
	start = &client->host->TCPportuse;
    p = *start;

    /* search port list to see if "port" is free */
    while(p)
    {
	if (p->port == port)
	{
	    if (autoalloc)
	    {
		port = htons(ntohs(port) + 1);	/* try another port */
		if (ntohs(port) < 1024)
		    return False;  /* looped around */
		p = *start;	/* cos list isn't sorted need to start here */
	    }
	    else
	    {
		return False;	/* in use */
	    }
	}
	else
	{
	    p = p->next;
	}
    }

    /* not in use, so allocate it */
    p = Heap$Malloc(Pvs(heap), sizeof(*p));
    if (!p)
    {
	printf("ip_alloc_port: out of memory\n");
	return False;
    }

    p->port = port;
    p->client = client;
    p->next = *start;
    (*start) = p;

    /* if we're doing autoalloc, people will probably appreciate
     * knowing which port they got allocated */
    if (autoalloc)
	lsap->u.UDP.port = port;

    return True;
}




#define MAXRTLOOP 10		/* how many route lookups to tolerate */

FlowMan_RouteInfo *ip_route(route_t *routetab, uint32_t addr, int depth)
{
    FlowMan_RouteInfo *ret;
    route_t	*r;

    r = routetab;

    /* check the routing table for an explicit route */
    while(r)
    {
	if (r->destip == (addr & r->destmask))
	{
	    /* is this through a gateway? */
	    if (r->gateway)
	    {
		/* route to the gateway */
		if (depth < MAXRTLOOP)
		{
		    FlowMan_RouteInfo *ri;
		    ri = ip_route(routetab, r->gateway, depth+1);
		    ret = malloc(sizeof(*ret));
		    ret->ifname = ri->ifname;
		    ret->type = ri->type;
		    ret->gateway = r->gateway;
		    /* don't free ri->ifname, since we'd need to strdup() it
		     * earlier */
		    free(ri);
		    return ret;
		}
		else
		{
		    printf("FlowMan$Route: routeing loop detected "
			   "(at gateway %08x)\n", ntohl(r->gateway));
		    RAISE_FlowMan$NoRoute();
		}
	    }
	    /* else no gatewaying needed */
	    ret = malloc(sizeof(*ret));
	    ret->ifname = strdup(r->intf->name);
	    ret->type = strdup("Ether");
	    ret->gateway = 0;
	    return ret;
	}
	r = r->next;
    }

    /* hmm, don't seem to have a route */
    RAISE_FlowMan$NoRoute();
    return NULL; /* keep gcc happy */
}


FlowMan_RouteInfoSeq *ip_passiveroute(flowman_st *st, const FlowMan_SAP *lsap)
{
    uint32_t		addr;
    FlowMan_RouteInfoSeq *ris;
    intf_st		*ifst;
    FlowMan_RouteInfo	info;

    addr = lsap->u.UDP.addr.a;

    /* room for 1 interface to start with */
    ris = SEQ_CLEAR(SEQ_NEW(FlowMan_RouteInfoSeq, 1, Pvs(heap)));

    /* Go over list of interfaces, returning those that are in this IP
     * host, and match this address */
    /* XXX should we use the routeing table instead? */
    ifst = st->host->ipconf->ifs;
    while(ifst)
    {
	/* match if the interface is in our host, and the address
	 * being routed is 0.0.0.0, or the address of the interface */
	if (ifst->host == st->host &&
	    ((addr == 0) || (ifst->ipaddr == addr)))
	{
	    /* this interface is valid, add it to the seq */
	    info.ifname = strdup(ifst->name);
	    info.type = strdup("Ether");
	    info.gateway = 0;  /* not meaningful in passive route */

	    SEQ_ADDL(ris, info);
	}
	ifst = ifst->next;
    }

    /* is the sequence still empty? */
    if (!SEQ_LEN(ris))
    {
	SEQ_FREE(ris);
	RAISE_FlowMan$NoRoute();
    }

    return ris;
}
