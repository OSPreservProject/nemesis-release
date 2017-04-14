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
**      ip_input.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ip input handling
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: ip_input.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *      @(#)ip_input.c  8.2 (Berkeley) 1/4/94
 * Id: ip_input.c,v 1.50.2.8 1997/09/15 23:10:55 ache Exp 
 *      ANA: ip_input.c,v 1.5 1996/09/18 14:34:59 wollman Exp 
 */


/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netorder.h>
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
#include "ip_var.h"
#include "tcp.h"

#ifdef DEFAULT_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* DEFAULT_TRACE */

/* forward decl. */
void ip_stripoptions(IO_Rec *recs);


/* note that stuff like multicast, fragmentation and option processing is
   still missing */
void ip_input(IO_Rec *recs, int nr_recs, iphost_st *host_st, uint32_t m_flags)
{
    struct ip *ip;
    /*
      struct ipq *fp;
      struct in_ifaddr *ia;
      */
    uint32_t hlen;  /* size of IP header length (incl. options) */
    uint32_t sum;
    struct in_addr dest_addr;
    intf_st    *ifs;
    intf_st    *intf;
    int i;

    TRC(printf("flowman: in ip_input\n"));

    if (recs[0].len < sizeof(struct ip)+sizeof(struct ether_header))
	goto tooshort;

    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));

    if (ip->ip_v != IPVERSION) {
	printf("flowman: default handler: bad IP version (%d != %d)\n",
	       ip->ip_v, IPVERSION);
	host_st->ipstat.ips_badvers++;
	goto bad_ip;
    }

    hlen = ip->ip_hl << 2;
    TRC(printf("hlen: d %d \n", hlen));
    TRC(printf("size of ip packet: %d\n", ntoh16(ip->ip_len)));

    /* check that packet is not too long, i.e. remove padding bytes
       added by etherlayer. We do this only if just the last io rec
       contains these padding bytes */
    sum = 0;
    for(i=0; i<nr_recs; i++) {
	TRC(printf("rec %d len %d\n", i, recs[i].len));
	sum += recs[i].len;
    }

    TRC(printf("ip_input: length of all recs: %d\n", sum));
    if (sum > ntoh16(ip->ip_len)+sizeof(struct ether_header))
    {
	int eth_pad = sum - (sizeof(struct ether_header) + ntoh16(ip->ip_len));

	TRC(printf("ip_input: packet contains some ether padding\n"));
	if (recs[nr_recs-1].len >= eth_pad)
	{
	    /* just adapt length of this last field if it is longer
	       than extra length */
	    recs[nr_recs-1].len -= eth_pad;
	    TRC(printf("new length of last rec%d\n", recs[nr_recs-1].len));
	}
	else
	{
	    printf("flowman: default handler: ip_input: "
		   "cannot trim ether padding, (%d < %d)\n",
		   recs[nr_recs-1].len, eth_pad);
	    {int i;
	    printf("packet shape: ");
	    for(i=0; i<nr_recs; i++)
		printf("%d ", recs[i].len);
	    printf("\n");
	    }
	}
    }


    if (hlen < sizeof(struct ip)) { /* minimum header length */
	printf("flowman: default handler: "
	       "IP header smaller than minimum header length (%d < %d)\n",
	       hlen, sizeof(struct ip));
	host_st->ipstat.ips_badhlen++;
	goto bad_ip;
    }


    if ((sum = in_cksum((uint16_t *)ip, hlen))) {
	/* RFC1122 says we should silently ignore this; but it's
         * useful info. */
	printf("flowman: default handler: failed IP checksum\n");
	host_st->ipstat.ips_badsum++;
	goto bad_ip;
    }

    /*
     * Convert fields to host representation.
     */

    ip->ip_len = ntohs(ip->ip_len);
    if (ip->ip_len < hlen) {
	printf("flowman: total length(%d) < header length(%d)\n",
	       ip->ip_len, hlen);
	host_st->ipstat.ips_badlen++;
	goto bad_ip;
    }

    /* OBS, does not change values forever!! */
    ip->ip_id = ntoh16(ip->ip_id);
    ip->ip_off = ntoh16(ip->ip_off);

    TRC(printf("ip id %x\n", ip->ip_id));
    TRC(printf("ip len %d\n", ip->ip_len));
    TRC(printf("ip off %x\n", ip->ip_off));

    /*
     * IpHack's section.
     * Right now when no processing on packet has done
     * and it is still fresh out of network we do our black
     * deals with it.
     * - Firewall: deny/allow/divert
     * - Xlate: translate packet's addr/port (NAT).
     * - Wrap: fake packet's addr/port <unimpl.>
     * - Encapsulate: put it in another IP and send out. <unimp.>
     */

    /* option processing */

    /* forwarding? , replace by just testing for right IP-adress */
 
    /* get dest IP addr of incoming msg */
    dest_addr = ip->ip_dst;
    TRC(printf("ip_dst address %I\n", dest_addr.s_addr));

    /* check if one of host addresses fits */
    /* lock data structure XXX */
    ifs = host_st->ifs;
    while (ifs) {
	TRC(printf("Checking %I\n", ifs->ipaddr));
	if(ntohl(ifs->ipaddr) ==  ntohl(dest_addr.s_addr)) break;
	ifs = ifs->next;
    }

    if(!ifs) {
	printf("flowman: ip_input: IP address %I not one of ours\n",
	       dest_addr.s_addr);
	goto bad_ip;	
    } else {
	TRC(printf("ip input: interface %p found\n", ifs));
	intf = ifs;
    }

    ip->ip_len -= hlen; 

    /* protocol switching, in bsd handled by protosw but this
       is overhead here since we only have IP */
    /* replace by switch for us */
    TRC(printf("ip protocol: x %x d %d\n", ip->ip_p, ip->ip_p)); 

    switch(ip->ip_p) {
    case IPPROTO_IP:
	TRC(printf("flowman IP switch: IP\n"));
	printf("flowman: IP on IP unimplemented\n");
	break;	

    case IPPROTO_ICMP:
	TRC(printf("flowman IP switch\n"));
	icmp_input(recs, nr_recs, host_st, intf, m_flags,  hlen, ip->ip_len);
	break;

    case IPPROTO_IGMP:
	printf("flowman: IP switch: IGMP unimplemented\n");
	break;

    case IPPROTO_TCP:
	TRC(printf("flowman IP switch: TCP\n"));
	tcp_input(recs, nr_recs, host_st, intf, m_flags, hlen);
	break;

    case IPPROTO_UDP:
	TRC(printf("flowman IP switch: UDP\n"));
	udp_input(recs, nr_recs, host_st, intf, m_flags);
	break;

    default:
	printf("flowman: IP switch: unknown protocol %d\n", ip->ip_p);
    }

    return;

bad_ip:
tooshort:
    printf("flowman: ip_input: ERROR, ignoring bogus packet\n");
    return;
}



uint16_t in_cksum(uint16_t *ip, uint16_t len) {
    /* naive: see Stevens p. 236 */
    int32_t sum=0;   /* assume 32 bit long, 16 bit short */
    
    while (len > 1) {
	TRC(printf("len %d sum: x%x    ", len, sum));
	TRC(printf("adding: x%x \n", *((uint16_t *) ip)));
	sum += *((uint16_t *) ip)++;
	if (sum & 0x80000000)  {/* if high-order bit set fold */
	    TRC(printf("folding sum x%x d%d\n", sum, sum));
	    sum = (sum & 0xFFFF) + (sum >>16);
	}
	len -= 2;
    }
    
    if (len) {    /* take care of left over byte */
	TRC(printf("there is aleft-over byte\n"));
	sum += (uint16_t) *(uchar_t *)ip;
    }

    /* fold 32-bit sum to 16 bits */
    while(sum >> 16)
	sum = ( sum & 0xFFFF) + (sum >> 16);
    
    TRC(printf("sum before returning x%x d%d\n", sum, sum));
    return ~sum;
}

/*
 * Strip out IP options, at higher
 * level protocol in the kernel.
 */
/* XXX OBS can be very expensive in Nemesis */
/* XXX AND: almost certainly ends up with duplicate data in the packet */
void ip_stripoptions(IO_Rec *recs) 
{
    struct ip *ip;
    uint8_t olen;     /* option length */
    uint16_t tot_len;  /* total length of IP packet */
    uint16_t xlen;  /* length of IP packet - header - options */
    char *src;
    char *dest;

    printf("OBS in ip_dostripoptions\n");
    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));     
    olen = (ip->ip_hl << 2) - sizeof (struct ip);
    tot_len = ip->ip_len;
    xlen = tot_len - sizeof(struct ip) - olen;
    printf("option length: %d, tot_len: %d, xlen: %x\n", olen, tot_len, xlen);
    src = (char *)(recs[0].base + sizeof(struct ether_header)+sizeof(struct ip)+ olen);
    dest = (char *)(recs[0].base + sizeof(struct ether_header)+sizeof(struct ip));
    memcpy(dest, src, olen);
    /* update header and length of IP packet */
    printf("memcpy done, updating length values\n");
    ip->ip_hl = sizeof(struct ip) >> 2;
    ip->ip_len = xlen + sizeof(struct ip);
}
	     
/* problem with buffers (iorecs) of odd length. If a buffer that is not
   the last one of the packet has odd length the last byte is shifted
   to the left (8 times) and then added to the sum. The first byte
   of the following iorec is then as a byte added to the sum and the
   following bytes become 2byte pairs. */
uint16_t cksum_morerecs(uint16_t *ip, uint16_t len, IO_Rec *recs, int max_recs)
{
    /* naive: see Stevens p. 236 */
    int32_t sum=0;   /* assume 32 bit long, 16 bit short */
    uint16_t rlen;
    int i;
    bool_t byte_over = False;

    TRC(printf("computing checksum for header len %d\n", len));
    while (len > 1) {
        TRC(printf("len %d sum: x%x    ", len, sum));
        TRC(printf("adding: x%x \n", *((uint16_t *) ip)));
        sum += *((uint16_t *) ip)++;
        if (sum & 0x80000000)  {/* if high-order bit set fold */
            TRC(printf("folding sum x%x d%d\n", sum, sum));
            sum = (sum & 0xFFFF) + (sum >>16);
        }
        len -= 2;
    }
    
    if (len) {    /* take care of left over byte */
        TRC(printf("there is aleft-over byte\n"));
        sum += (uint16_t) *(uchar_t *)ip;
    }
    TRC(printf("sum after header %x\n", sum));

    /* compute checksum over the other IO_Recs */
    for(i=1;i<max_recs;i++) {
        rlen = recs[i].len;
        ip = (uint16_t *)(recs[i].base);
        TRC(printf("computing checksum for rec %d len %d\n", i, rlen));
        while (rlen > 1) {
            if (!byte_over) {
                TRC(printf("len %d sum: x%x    ", rlen, sum));
                TRC(printf("adding: x%x \n", *((uint16_t *) ip)));
                sum += *((uint16_t *) ip)++;
                if (sum & 0x80000000)  {/* if high-order bit set fold */
                    TRC(printf("folding sum x%x d%d\n", sum, sum));
                    sum = (sum & 0xFFFF) + (sum >>16);
                }
                rlen -= 2;
            }
            else {
		/* XXX AND: This only works if the two recs describe
		 * contiguous regions of memory. */
                TRC(printf("len %d sum: x%x    ", rlen, sum));
                TRC(printf("adding: x%x \n", *((uint8_t *) ip)));
                sum += (uint16_t) ((*((uint8_t *) ip))<<8);
		/* XXX change due to compiler warnings 
                ip = (((uint8_t *)ip)++);
                ip = (uint16_t *)ip;
		*/
                ip = (uint16_t *)(((uint8_t *)ip)++);  /* XXX new for above */
                if (sum & 0x80000000)  {/* if high-order bit set fold */
                    TRC(printf("folding sum x%x d%d\n", sum, sum));
                    sum = (sum & 0xFFFF) + (sum >>16);
                }
                rlen--;
                byte_over = False;
            }
        }
        
        if (rlen) {    /* take care of left over byte */
            if (i == max_recs-1) {
                TRC(printf("there is a left-over byte\n"));
                TRC(printf("adding: x%x to sum %x\n", *((uint8_t *) ip), sum));
                sum += (uint16_t) *(uchar_t *)ip; 
                TRC(printf("after adding left over byte len %d sum: x%x    ", rlen, sum));
            }
            else {
                TRC(printf("there is a left-over byte, not last rec\n"));
                TRC(printf("adding: x%x to sum %x\n", *((uint8_t *) ip), sum));
                sum += (uint16_t) ((*(uchar_t *)ip)); 
                TRC(printf("after adding left over byte len %d sum: x%x    ", rlen, sum));
                byte_over = True;
            }
        }
    }
    TRC(printf("sum before folding to 16 bits %x\n", sum));
    /* fold 32-bit sum to 16 bits */
    while(sum >> 16)
        sum = ( sum & 0xFFFF) + (sum >> 16);
    
    TRC(printf("sum 1 before returning %x\n", sum));

    return ~sum;
}
