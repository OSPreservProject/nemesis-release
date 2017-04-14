/*
 *	ether.h
 *	-------
 *
 * $Id: ether.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 * An Ethernet frame is
 *
 *  + 14 bytes header (see below)
 *  + 46--1500 bytes of data
 *  + 4 bytes FCS (ie CRC)
 */

#ifndef _ETHER_H_
#define _ETHER_H_

#define ETHER_ADR_LEN		(6)

typedef struct ether_hdr {
    unsigned char		dst_addr[ETHER_ADR_LEN];
    unsigned char		src_addr[ETHER_ADR_LEN];
    unsigned short		proto;
} EtherHdr;

#define ETHER_TYPE_AMOEBA	(2222)
#define ETHER_TYPE_XNS		(0x0600)
#define ETHER_TYPE_IP		(0x0800)
#define ETHER_TYPE_ARP		(0x0806)
#define ETHER_TYPE_RARP		(0x8035)
#define ETHER_TYPE_MSDL1	(0x6660)
#define ETHER_TYPE_MSDL2	(0x6661)

#define ETHER_HDR_SIZE		(14)

#define ETHERMIN		(64)
#define ETHERMAX		(1518)

#define ARP_TYP_REQ		(1)
#define ARP_TYP_REP		(2)
#define RARP_TYP_REQ		(3)
#define RARP_TYP_REP		(4)

#define ARES_HRD_ETHER		(1)

#define ARES_PRO_IP		(2048)

typedef struct EtherArpPkt {
    EtherHdr			ether_hdr;
    unsigned short		ar_hrd;
    unsigned short		ar_pro;
    unsigned char		ar_hln;
    unsigned char		ar_pln;
    unsigned short		ar_op;
    unsigned char		ar_sha[6];
    unsigned char		ar_spa[4];
    unsigned char		ar_tha[6];
    unsigned char		ar_tpa[4];
} EtherArpPkt;

#endif /* _ETHER_H_ */

