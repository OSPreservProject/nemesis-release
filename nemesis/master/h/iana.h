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
**      iana.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Internet Assigned Numbers Authority - commonly used values
** 
** ENVIRONMENT: 
**
**      Network subsystem
** 
** ID : $Id: iana.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _iana_h_
#define _iana_h_

#include <netorder.h>

/* Some commonly used values taken from the Assigned Numbers RFC
 * (currently rfc1700) */

/* Ethernet frame types (16 bits) */
#define FRAME_IPv4		htons(0x0800)
#define FRAME_ARP		htons(0x0806)

/* IP protocol types (single byte) */
#define IP_PROTO_ICMP		01
#define IP_PROTO_TCP		06
#define IP_PROTO_UDP		17

/* compatability for RPC library */
#define IPPROTO_UDP		IP_PROTO_UDP
#define IPPROTO_TCP		IP_PROTO_TCP

/* ICMP major types */
#define ICMP_ECHO_REPLY          0
#define ICMP_ECHO_REQUEST        8

/* Well known UDP/TCP port numbers */
#define UDP_PORT_BOOTP_SERVER	htons(67)
#define UDP_PORT_BOOTP_CLIENT	htons(68)

#endif /* _iana_h_ */
