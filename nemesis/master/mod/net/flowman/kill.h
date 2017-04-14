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
**      kill.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Prototypes for killing things
** 
** ENVIRONMENT: 
**
**      FlowMan internal
** 
** ID : $Id: kill.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#ifndef kill_h_
#define kill_h_

/* kill_<foo> functions deallocate resources associated with <foo> */

/* Simple kills: */

/* Turn off demuxing of conn "c" from flow "f", freeing the memory
 * used by "c".  You still need to unlink "c" from any lists it might
 * be on, and free ports that are no longer in use. */
void kill_conn(flow_t *f, conn_t *c);

/* Stop transmissions on "f", and don't demux receives anymore.  Blow
 * away IO channels from driver to client.  Also frees the memory used
 * in conn_t descriptors and the flow descriptor */
void kill_flow(flow_t *f);


/* More complex kills: */

/* Unbind and free memory for ports in "plist" owned by "client".
 * Used on client death. */
void kill_ports_by_client(port_alloc_t **plist, flowman_st *client);

/* Walk over "routetab", freeing any routes that go through
 * "intf". Used on interface removal. */
void kill_routes_by_intf(route_t **routetab, intf_st *intf);

/* Walk over all clients in "host", checking each flow in each client.
 * If the flow goes via "intf", it is killed using kill_flow().  Used
 * on interface removal. */
void kill_flows_by_intf(iphost_st *host, intf_st *intf);



/* Less violent functions that reconfigure things, without freeing them */

/* Reconfigure RX packet filter to not demux connection "c" */
void del_conn(flow_t *f, conn_t *c);

/* Reconfigure RX packet filter to demux packets on the connection "c"
 * to the flow "f". */
void add_conn(flow_t *f, conn_t *c);


#endif /* kill_h_ */
