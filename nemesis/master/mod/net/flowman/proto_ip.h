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
**      proto_ip.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      IP protocol helper function prototypes
** 
** ENVIRONMENT: 
**
**      FlowMan internal
** 
** ID : $Id: proto_ip.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#ifndef proto_ip_h_
#define proto_ip_h_

/* Allocated "lsap" as a port on behalf of "client".  Returns True for
 * success, False for failure (out of memory, no free ports, port in
 * use).  A port of 0 cause auto-allocation to be attempted. */
bool_t ip_alloc_port(flowman_st *client, FlowMan_SAP *lsap);

/* Do an active route to "addr", using the routeing table "r".  The
 * "depth" argument specifies how many recursive levels deep we are */
FlowMan_RouteInfo *ip_route(route_t *r, uint32_t addr, int depth);

/* Do a passive route to find which interface "lsap" refers to.
 * Returns a list of them. */
FlowMan_RouteInfoSeq *ip_passiveroute(flowman_st *st, const FlowMan_SAP *lsap);

#endif /* proto_ip_h_ */
