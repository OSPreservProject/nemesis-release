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
**   udp_input.c     
** 
** FUNCTIONAL DESCRIPTION:
** 
**   UDP input   
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: udp_input.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993, 1995
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
 *      @(#)udp_usrreq.c        8.6 (Berkeley) 5/23/95
 *      Id: udp_usrreq.c,v 1.30.2.3 1997/09/30 16:25:11 davidg Exp 
 */



/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <netorder.h>

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
#include "fm_constants.h"
#include "ip_var.h"

#ifdef DEFAULT_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* DEFAULT_TRACE */

void udp_input(IO_Rec *recs, int nr_recs, iphost_st *host_st,intf_st *ifs,
	       uint32_t m_flags)
{
    register struct ip *ip;
    register struct udphdr *uh;
    int len;
    struct ip save_ip;
    uint16_t sum;
    int tlen;
    uint8_t iphlen;

    TRC(printf("flowman udp_input\n"));
    host_st->udpstat.udps_ipackets++;
    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));

    /*
     * Strip IP options, if any; should skip this,
     * make available to user, and use on returned packets,
     * but we don't yet have a way to check the checksum
     * with options still present.
     */
    /* XXX AND: why are we worrying about IP options in UDP processing code?
     * Why hasn't this been done in ip_input? */
    iphlen = ip->ip_hl << 2;
    TRC(printf("iphlen %d\n", iphlen));
    if (iphlen > sizeof (struct ip)) {
	ip_stripoptions(recs);
	iphlen = sizeof(struct ip);
    }
    

    if (recs[0].len < (sizeof(struct ether_header) + 
		       sizeof(struct ip) +
		       sizeof(struct udphdr)))
    {
	printf("flowman: udp_input ERROR: recs[0].len too small (%d<%d)\n",
	       recs[0].len,
	       sizeof(struct ether_header) +
	       sizeof(struct ip) +
	       sizeof(struct udphdr));
	host_st->udpstat.udps_hdrops++;
	goto bad;
    }

    uh = (struct udphdr *)(recs[0].base +
			   sizeof(struct ether_header) +
			   sizeof(struct ip));

    len = ntoh16((int16_t)uh->uh_ulen);

    TRC(printf("flowman udp_input len udp header %d\n", len));
    TRC(printf("ip ip_len %d\n", ntoh16(ip->ip_len)));
    TRC(printf("udp-input:\nudp src port %d\n", ntoh16(uh->uh_sport)));
    TRC(printf("udp dest port %d\n", ntoh16(uh->uh_dport)));
    TRC(printf("udp length %d\n", ntoh16(uh->uh_ulen)));
    TRC(printf("udp sum %d\n", ntoh16(uh->uh_sum)));    
    if (ip->ip_len != len) {
	if (len > ip->ip_len || len < sizeof(struct udphdr)) {
	    printf("flowman: udp_input: bad header length (udp=%d, ip=%d)\n",
		   len, ip->ip_len);
	    host_st->udpstat.udps_badlen++;
	    goto bad;
	}
    }

    /*
     * Save a copy of the IP header in case we want restore it
     * for sending an ICMP error message in response.
     */
    save_ip = *ip;
    TRC(printf("source addr %x\n", hton32(ip->ip_src.s_addr)));
    if (uh->uh_sum) {
	/* checksum is computed over some IP fields and UDP header
	   and UDP data */
	((struct ipovly *)ip)->ih_next = 0;
	((struct ipovly *)ip)->ih_prev = 0;
	((struct ipovly *)ip)->ih_x1 = 0;
	((struct ipovly *)ip)->ih_len = uh->uh_ulen;      
	tlen = len + sizeof(struct ip);
	TRC(printf("udp tlen (udp + ip): %d\n", tlen));
	if (nr_recs == 1) {
	    if ((sum = in_cksum((uint16_t *)ip, len+ sizeof(struct ip)))) {
	        host_st->udpstat.udps_badsum++;
		printf("flowman: udp_input: error checksum\n");
		goto bad;
	    }
	}
	else {
	    if ((sum = cksum_morerecs((uint16_t *)ip, recs[0].len-sizeof(struct ether_header), recs, nr_recs))) {
	        host_st->udpstat.udps_badsum++;
		printf("flowman: udp_input: error checksum more recs\n");
		goto bad;
	    }
	}	
	TRC(printf("udp calculated cksum, ok\n"));
    }
    TRC(else
	printf("no udp cksum was set in incoming packet\n"));

    /* packet was for a port with no application associated, so send
       ICMP unreachable */
    if (m_flags & (M_BCAST | M_MCAST)) {
	host_st->udpstat.udps_noportbcast++;
	goto bad;
    }

    *ip = save_ip;
    TRC(printf("udp calling icmp_error\n"));
    TRC(printf("udp ip_len %d, iphlen %d\n", ip->ip_len, iphlen));
    /* reset length field, remember that ip_input subtracted header length from
       ip->ip_len */
    ip->ip_len += iphlen;
    TRC(printf("udp calling icmp_error with nr recs %d rec max len %d\n",
	       nr_recs, recs[nr_recs-1].len));
    icmp_error(recs, nr_recs, host_st, ifs, m_flags,
	       ICMP_UNREACH, ICMP_UNREACH_PORT, 0, 0);
    return;
    
bad:
    printf("flowman: udp_input: error, ignoring UDP\n");
}

/* End of udp_input.c */
