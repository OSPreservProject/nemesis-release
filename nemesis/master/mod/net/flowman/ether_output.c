/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, Swedish Institute of Computer Science                   *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      ether_output.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ethernet output
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: ether_output.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */
/*
 * Copyright (c) 1982, 1989, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)if_ethersubr.c      8.1 (Berkeley) 6/10/93
 * Id: if_ethersubr.c,v 1.26.2.3 1997/09/30 12:29:00 davidg Exp 
 */


/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>
#include <exceptions.h>
#include <contexts.h>
#include <nemesis_tgt.h>

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
#include <Threads.ih>

/* local interfaces */
#include "flowman_st.h"
#include "kill.h"
#include "proto_ip.h"

/* ip stuff */
#include "ethernet.h"
#include "if_ether.h"
#include "in.h"
#include "ip.h"
#include "udp.h"
#include "udp_var.h"
#include "ip_icmp.h"
#include "icmp_var.h"
#include "tcp.h"

#ifdef DEFAULT_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* DEFAULT_TRACE */

/* extern & forward decl. */
extern intf_st *ifname2ifst(iphost_st *host, const char *name);

void show_packet(IO_Rec *txrecs);

void
ether_output(IO_Rec *recs,       int nr_recs,
	     iphost_st *host_st, intf_st *ifs,
	     uint8_t *ip_opt,    int ip_opt_len)
{
    /* ip is the ip-header buf a pointer to the "payload"
       (which might be e.g. a ICMP header and stuff XXX has changed , buf
       needed ??*/
    IO_clp io_out;
    int mtu = 1514;
    struct ip *iph;
    struct ether_header *eth;
    word_t value;
    FlowMan_RouteInfo UNUSED *ri;
    intf_st UNUSED *arp_ifst;
    Net_EtherAddrP NOCLOBBER UNUSED ret = NULL;
    char UNUSED buf[20];
    int  UNUSED plen;             /* length of payload */
    int  rec_recs;         /* nr of received recs */


    TRC(printf("in ether_output\n"));

    iph = (struct ip *)(recs[0].base + sizeof(struct ether_header));
    eth = (struct ether_header *)(recs[0].base);

    io_out = ifs->io_tx;

    /* always send replies to the Ethernet address which sent the
     * request */
    memcpy(eth->ether_dhost, eth->ether_shost, ETHER_ADDR_LEN);
    memcpy(eth->ether_shost, ifs->card->mac, ETHER_ADDR_LEN);

    /* XXX not needed ?! */
    eth->ether_type = hton16(ETHERTYPE_IP);

    /* to do check overall size */
    if ((ETHER_HDR_LEN + sizeof(struct ip) + ip_opt_len) > mtu) {
	printf("flowman ether-output: frame too long, "
	       "fragmentation required\n");
	goto eo_bad;
    }
    
    /*    TRC(show_packet(recs));  */

    TRC(printf("trying to send message now, base=%p\n", recs[0].base));

    ((char *)recs[0].base) += 2;
    recs[0].len  -= 2;
    IO$PutPkt(io_out, nr_recs, recs, 0, 0);
    TRC(printf("packet send, getting empty packet\n"));

    IO$GetPkt(io_out, nr_recs, recs, 0, &rec_recs, &value);     
    ((char *)recs[0].base) -= 2;
    recs[0].len  += 2;

    TRC(printf("flowman etheroutput done...\n"));
    return;
    
eo_bad:
    printf("flowman: default: ERROR ether_output\n");
    
}

void show_packet(IO_Rec *txrecs) {
    struct ip *ip;
    struct ether_header UNUSED *eth;
    struct icmp UNUSED *icmp;
    int hlen;
    int UNUSED i;
    struct udphdr UNUSED *udp;
    struct tcphdr UNUSED *tcp;
    uint8_t iphlen;



    ip = (struct ip *)(txrecs[0].base + sizeof(struct ether_header));
    iphlen = ip->ip_hl << 2;

#ifdef ETH
    eth = (struct ether_header *)(txrecs[0].base);
    printf("ether output showing packet\n");

    printf("header ethertype: %x\n", eth->ether_type);
    printf("header etherdest: %x\n", eth->ether_dhost);
    for(i=0;i<6;i++)
	printf(" %02x ",eth->ether_dhost[i]);

    printf("\nheader ethersource: %x\n", eth->ether_shost);
    for(i=0;i<6;i++)
	printf(" %02x ", eth->ether_shost[i]);
#endif
    printf("IP header\n");
    printf("ip version %x   ", ip->ip_v);    
    hlen = ip->ip_hl << 2;
    printf("ip hlen: d %d  ", hlen);
    printf("ip tos %x   ", ip->ip_tos);
    printf("ip len %d  ", ntoh16(ip->ip_len));
    printf("ip id %x   ", ntoh16(ip->ip_id));

    printf("ip off %x  ", ntoh16(ip->ip_off)); 
    printf("ip ttl %x  ", ip->ip_ttl); 
    printf("ip proto %x  ", ip->ip_p); 
    printf("ip sum %x   ", ip->ip_sum);
    printf("ip src addr %x   ", ntohl(ip->ip_src.s_addr));
    printf("ip dst addr %x\n", ntohl(ip->ip_dst.s_addr));

    if (ip->ip_p == IPPROTO_ICMP) {
	/* ICMP */
	printf("ICMP header:\n");
	icmp = (struct icmp *)(txrecs[1].base);
	printf("icmp type: %d   ", icmp->icmp_type);
	printf("icmp code: %d   ", icmp->icmp_code);
	printf("icmp cksum: %d   ", ntoh16(icmp->icmp_cksum));
	printf("icmp seq: %d   ", ntoh16(icmp->icmp_seq));
	printf("icmp id: %d\n", ntoh16(icmp->icmp_id));

	printf("old IP header:\n");
	ip = (struct ip *)(txrecs[0].base + sizeof(struct ether_header)+28);
        printf("ip version %x  ", ip->ip_v);    
	hlen = ip->ip_hl << 2;
	printf("ip hlen: d %d  ", hlen);
	printf("ip tos %x   ", ip->ip_tos);
	printf("ip len %d  ", ntoh16(ip->ip_len));
	printf("ip id %x   ", ntoh16(ip->ip_id));
	
	printf("ip off %x  ", ntoh16(ip->ip_off)); 
	printf("ip ttl %x  ", ip->ip_ttl); 
	printf("ip proto %x  ", ip->ip_p);
	printf("ip sum %x   ", ip->ip_sum);
	printf("ip src addr %x   ", ntohl(ip->ip_src.s_addr));
	printf("ip dst addr %x\n", ntohl(ip->ip_dst.s_addr));

	udp = (struct udphdr *)(txrecs[0].base + sizeof(struct ether_header)+48);
	printf("\nudp src port %d\n", ntoh16(udp->uh_sport));
	printf("udp dest port %d\n", ntoh16(udp->uh_dport));
	printf("udp length %d\n", ntoh16(udp->uh_ulen));
	printf("udp sum %d\n", ntoh16(udp->uh_sum));
    }

#ifdef UDP
    /* udp header after ip */
    udp = (struct udphdr *)(txrecs[0].base + sizeof(struct ether_header)+iphlen);
    printf("\nudp src port %d\n", ntoh16(udp->uh_sport));
    printf("udp dest port %d\n", ntoh16(udp->uh_dport));
    printf("udp length %d\n", ntoh16(udp->uh_ulen));
    printf("udp sum %d\n", ntoh16(udp->uh_sum));
#endif

#ifdef TCP
    /* tcp header after ip */
    tcp = (struct tcphdr *)(txrecs[0].base + sizeof(struct ether_header)+iphlen);
    printf("tcp header:\n");
    printf("sport %d\n", ntoh16(tcp->th_sport));
    printf("dport %d\n", ntoh16(tcp->th_dport));
    printf("seq %d\n", ntoh32(tcp->th_seq));
    printf("ack %d\n", ntoh32(tcp->th_ack));
    printf("win %d\n", ntoh16(tcp->th_win));
    printf("sum %d\n", ntoh16(tcp->th_sum));    
    printf("urg %d\n", ntoh16(tcp->th_urp));
#endif
}





