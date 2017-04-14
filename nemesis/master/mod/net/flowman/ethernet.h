/*
   SICS adaptation
   ID: $Id: ethernet.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
*/

/*
 * Fundamental constants relating to ethernet.
 *
 * Id: ethernet.h,v 1.2 1996/08/06 21:14:21 phk Exp 
 *
 */

#ifndef _NET_ETHERNET_H_
#define _NET_ETHERNET_H_


/* get some Nemesis stuff */
#include <Net.ih>
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>

   
/*
 * The number of bytes in an ethernet (MAC) address.
 */
#define	ETHER_ADDR_LEN		6

/*
 * The number of bytes in the type field.
 */
#define	ETHER_TYPE_LEN		2

/*
 * The number of bytes in the trailing CRC field.
 */
#define	ETHER_CRC_LEN		4

/*
 * The length of the combined header. (+2 for pad)
 */
#define	ETHER_HDR_LEN		(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN+2)

/*
 * The minimum packet length.
 */
#define	ETHER_MIN_LEN		64

/*
 * The maximum packet length.
 */
#define	ETHER_MAX_LEN		1518

/*
 * A macro to validate a length with
 */
#define	ETHER_IS_VALID_LEN(foo)	\
	((foo) >= ETHER_MIN_LEN && (foo) <= ETHER_MAX_LEN)

/*
 * Structure of a 10Mb/s Ethernet header.
 */



struct  ether_header {
	uchar_t pad[2];
        uchar_t ether_dhost[ETHER_ADDR_LEN];
        uchar_t ether_shost[ETHER_ADDR_LEN];
        uint16_t        ether_type;
};

/*
 * Structure of a 48-bit Ethernet address.
 */
struct	ether_addr {
	uchar_t octet[ETHER_ADDR_LEN];
};

void ether_input(IO_Rec *recs, int nr_recs, Net_EtherAddr *mac, iphost_st *host_st);


void ether_output(IO_Rec *recs, int nr_recs, iphost_st *st, intf_st *ifs, uint8_t *ip_opt, int ip_opt_len);
#endif
