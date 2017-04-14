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
**     tcp_input.c      
** 
** FUNCTIONAL DESCRIPTION:
** 
**     TCP input 
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: tcp_input.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1994, 1995
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
 *      @(#)tcp_input.c 8.12 (Berkeley) 5/24/95
 *      Id: tcp_input.c,v 1.54.2.5 1997/10/04 08:54:12 davidg Exp 
 */




/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>

#include <iana.h>

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
#include "ip_var.h"
#include "tcp.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "fm_constants.h"

#ifdef DEFAULT_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* DEFAULT_TRACE */


/* forward decl. */
void show_all(struct tcpiphdr *ti);
void tcp_respond(struct tcpiphdr *ti, IO_Rec *recs, int nr_recs,
		 iphost_st *host_st, intf_st *ifs,
		 tcp_seq ack, tcp_seq seq, int flags);

void tcp_input(IO_Rec *recs, int nr_recs, iphost_st *host_st,
	       intf_st *ifs, uint32_t m_flags, uint8_t iphlen)
{
    /* iphlen: size of ip header incl. possible options */
    register struct tcpiphdr *ti;
    uint16_t len, tlen;  /* tlen: ip length without IP header start of code */
    struct tcphdr *tcphdr;
    uint16_t sum;
    uint8_t tcphlen;      /* tcp header length, incl. options */

    TRC(printf("tcp input with ip header length %d\n", iphlen));
    host_st->tcpstat.tcps_rcvtotal++;    

    ti = ( struct tcpiphdr *)(recs[0].base + sizeof(struct ether_header));
    tcphdr = (struct tcphdr *)(recs[0].base +
			       sizeof(struct ether_header) +
			       iphlen);
    tcphlen = tcphdr->th_off << 2;
    TRC(printf("tcp header length %d\n", tcphlen)); 

    if (iphlen > sizeof (struct ip))
	ip_stripoptions(recs);
    /* XXX AND: iphlen now wrong? */

    if ((iphlen + tcphlen) < sizeof (struct tcpiphdr)) {
        host_st->tcpstat.tcps_rcvshort++;
        goto drop;
    }

    /*
     * Checksum extended TCP header and data.
     */
    tlen = ((struct ip *)ti)->ip_len;
    TRC(printf("tcp option length %d\n", tcphlen- sizeof(struct tcphdr)));
    len = sizeof (struct ip) + tlen;
    TRC(printf("tlen %d, len %d\n", tlen, len));
    ti->ti_next = ti->ti_prev = 0;
    ti->ti_x1 = 0;
    ti->ti_len = htons((uint16_t)tlen);
    if (recs[1].len == 0) 
      sum = in_cksum((uint16_t *)ti, len);
    else {
      sum = cksum_morerecs((uint16_t *)ti,
			   recs[0].len - sizeof(struct ether_header),
			   recs,
			   nr_recs);
      TRC(printf("tcp_input: checksum more recs %d\n", sum));
    }
    if (sum) {
	printf("flowman: tcp_input: checksum error (%x)\n", sum);
	host_st->tcpstat.tcps_rcvbadsum++;
	goto drop;
    }
    TRC(else
	printf("tcp_input: checksum OK\n"));

    TRC(show_all(ti));

    /*
     * Convert TCP protocol specific fields to host format.
     */

    ti->ti_seq = ntoh32(ti->ti_seq);
    ti->ti_ack = ntoh32(ti->ti_ack);
    ti->ti_win = ntoh16(ti->ti_win);
    ti->ti_urp = ntoh16(ti->ti_urp);


    if (tcphdr->th_flags & TH_SYN) {
        TRC(printf("tcp syn message\n"));
	goto dropwithreset;
    }

    /* AND: shouldn't really get here */
#define F(f, c) (tcphdr->th_flags & f)? c : '-'
    printf("flowman: tcp_input: no SYN set (%c%c%c%c%c%c), ignoring\n",
	   F(TH_FIN,  'f'),
	   F(TH_SYN,  's'),
	   F(TH_RST,  'r'),
	   F(TH_PUSH, 'p'),
	   F(TH_ACK,  'a'),
	   F(TH_URG,  'u'));
#undef F

    return;

dropwithreset:
    /*
     * Generate a RST, dropping incoming segment.
     * Make ACK acceptable to originator of segment.
     * Don't bother to respond if destination was broadcast/multicast.
     */
    if ((tcphdr->th_flags & TH_RST) || m_flags & (M_BCAST|M_MCAST) ||
	IN_MULTICAST(ntoh32(ti->ti_dst.s_addr)))
	goto drop;

    if (tcphdr->th_flags & TH_ACK) {
	tcp_respond(ti, recs, nr_recs, host_st, ifs,
		    (tcp_seq)0, ti->ti_ack, TH_RST);
    } else {
	TRC(printf("tcp_input: else ti_len %d, iphlen %d \n",
		   ntoh16(ti->ti_len), iphlen));
	/* Thiemo setting ti_len to host order and reducing length field*/
	ti->ti_len = (ntoh16(ti->ti_len) -
		      iphlen -
		      (tcphlen - sizeof(struct tcphdr)));
	if (tcphdr->th_flags & TH_SYN)
	    ti->ti_len++;
	TRC(printf("ti-ti_len network: %d\n", ti->ti_len));
	tcp_respond(ti, recs, nr_recs, host_st, ifs,
		    ti->ti_seq + ti->ti_len, (tcp_seq)0,
		    TH_RST|TH_ACK);
    }
    return;

 drop:
    printf("flowman: tcp_input: ditched packet\n");
}


/* routing stuff is still missing */
void tcp_respond(struct tcpiphdr *ti, IO_Rec *recs, int nr_recs,
		 iphost_st *host_st, intf_st *ifs, tcp_seq ack,
		 tcp_seq seq, int flags)
{
    register int tlen=0;

    TRC(printf("tcp respond \n"));
#define xchg(a,b,type) { type t; t=a; a=b; b=t; }
    xchg(ti->ti_dst.s_addr, ti->ti_src.s_addr, uint32_t);
    xchg(ti->ti_dport, ti->ti_sport, uint16_t);
#undef xchg

    ti->ti_len = htons((uint16_t)(sizeof (struct tcphdr) + tlen));
    tlen += sizeof (struct tcpiphdr);
    ti->ti_next = ti->ti_prev = 0;
    ti->ti_x1 = 0;
    TRC(printf("ack before setting back %d\n", ack));
    ti->ti_seq = hton32(seq);
    ti->ti_ack = hton32(ack);
    ti->ti_x2 = 0;
    ti->ti_off = sizeof (struct tcphdr) >> 2;
    ti->ti_flags = flags;
    ti->ti_win = hton16(ti->ti_win);
    ti->ti_urp = 0;
    ti->ti_sum = 0;
    TRC(printf("calling cksum with len %d\n", tlen));

    /* checksum computes over tcp and ip (overlay) header */
    if (recs[1].len == 0) {
	ti->ti_sum = in_cksum((uint16_t *)ti, tlen);
    } else {
	printf("tcp_respond: checksum more recs, "
	       "nr recs %d rec 0 len %d rec 1 len%d\n",
	       nr_recs, recs[0].len, recs[1].len);
	ti->ti_sum = cksum_morerecs((uint16_t *)ti,
				    recs[0].len - sizeof(struct ether_header),
				    recs, nr_recs);
    }
    TRC(printf("calling cksum done\n"));

    ((struct ip *)ti)->ip_len = tlen;
    ((struct ip *)ti)->ip_ttl = MAXTTL;

    TRC(printf("tcp respond calling ip_output\n"));

    ip_output(recs, nr_recs, host_st, ifs, 0, flags);
}







void show_all(struct tcpiphdr *ti) {
    uint8_t  UNUSED *act;
    int UNUSED i;
    uint16_t UNUSED *act16;
/*
    act = ti; 
    for(i=0;i<40;i++) {
	printf("%d: %d    ", i, *act);
	act++; 
    }

    act16 = ti;
    for(i=0;i<20;i++) {
	printf("%d: d%d x%x    ", i, *act16, *act16);
	act16++;
    }
    */
    printf("\nipovly\n");
    printf("ti_next %d\n", ti->ti_next);
    printf("ti_prev %d\n", ti->ti_prev);
    printf("ti_x1 %d\n", ti->ti_x1);
    printf("ti_pr %d\n", ti->ti_pr);
    printf("ti_len %d\n", ti->ti_len);
    printf("ti_src %x\n", ti->ti_src.s_addr);
    printf("ti_dst %x\n", ti->ti_dst.s_addr);
/*
    printf("ti_len %d\n", ntoh16(ti->ti_len));
    printf("ti_src %x\n", ntoh32(ti->ti_src.s_addr));
    printf("ti_dst %x\n", ntoh32(ti->ti_dst.s_addr));
    */
    printf("\ntcp header\n");
/*
    printf("ti_sport %d\n", ti->ti_sport);
    printf("ti_dport %d\n", ti->ti_dport);
    printf("ti_seq %d\n", ti->ti_seq);
    printf("ti_ack %d\n", ti->ti_ack);
    printf("ti_win %d\n", ti->ti_win);
    printf("ti_sum %d\n", ti->ti_sum);
    printf("ti_urp %d\n", ti->ti_urp);
    */
    printf("ti_sport %d\n", ntoh16(ti->ti_sport));
    printf("ti_dport %d\n", ntoh16(ti->ti_dport));
    printf("ti_seq %d\n", ntoh32(ti->ti_seq));
    printf("ti_ack %d\n", ntoh32(ti->ti_ack));
    printf("ti_win %d\n", ntoh16(ti->ti_win));
    printf("ti_sum %d\n", ntoh16(ti->ti_sum));
    printf("ti_urp %d\n", ntoh16(ti->ti_urp));

}
