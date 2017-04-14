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
**      ip_output.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ip output
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: ip_output.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */
/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *      @(#)ip_output.c 8.3 (Berkeley) 1/21/94
 *      Id: ip_output.c,v 1.44.2.7 1997/09/30 16:25:08 davidg Exp 
 */



/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
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


/* multicast, broadcast, fragmentation and routing still has to be done */
int ip_output(IO_Rec *recs, int nr_recs, iphost_st *host_st,
	      intf_st *ifs, uint8_t *opt, /*ro ,*/int flags/*, imo*/)
{
  /* 
     opt is the pointer to the IP options to include
     flags are the flags: IP_FORWARDING, IP_ROUTETOIF, IP_ALLOWBROADCAST,
     IP_RAWOUTPUT
  */
    int hlen = sizeof (struct ip);
    int mtu = 1514;	/* XXX AND: need proper story */
    struct ip *ip;

    TRC(printf("flowman in ip_output\n"));
    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));

    /*
     * insert options: not done yet
     *
     */
    
    /*
     * Fill in IP header.
     */
    if ((flags & (IP_FORWARDING|IP_RAWOUTPUT)) == 0) {
	/* OBS, ip_hl and version might cause problems, only 4 bit */
	TRC(printf("ip_output inserting some header fields\n"));
	ip->ip_v = IPVERSION;
	ip->ip_hl = (hlen>>2);
	
	TRC(printf("inserted ip hlen: %d\n", hlen>>2));
	ip->ip_off &= IP_DF;
	ip->ip_id = htons(host_st->ip_id++);
	host_st->ipstat.ips_localout++;
    } else { /* packet has already header */
	hlen = (ip->ip_hl) << 2;
    }


    /*
     * route selection: routing not implemented yet 
     *
     */

    /*
     * multicast destionation handling: to be done
     *
     */

    /*
     * If source address not specified yet, use address
     * of outgoing interface. 
     */
    if (ip->ip_src.s_addr == INADDR_ANY) {
	ip->ip_src.s_addr = ifs->ipaddr; 
    }
	
    /* 
     * broadcast addresses check: to be done
     *
     */

    /* UNUSED sendit: */

    /*
     * If small enough for interface, can just send directly.
     */
    /* XXX AND: should this be "<="? */
    if (ip->ip_len < mtu-(sizeof(struct ether_header))) {
	TRC(printf("in ip_output:\n"));
	TRC(printf("ip->len (network):  %d \n", hton16(ip->ip_len)));
	TRC(printf("ip->len (host) : %d\n", ip->ip_len));
	TRC(printf("ip->off (host) : %d\n", ip->ip_off));
	TRC(printf("ip->off (net) : %d\n", hton16(ip->ip_off)));
	/* converte ip-off, ip-len back to network order since they
	   have been changed to host order by ip_input. ip_id
	   has already been changed back to network order above */
	ip->ip_off = hton16(ip->ip_off);
	ip->ip_len = hton16(ip->ip_len);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum((uint16_t *)ip, hlen);
	/* call ether_output */
	/* 0 is opt and opt-len */
	TRC(printf("flowman ip_output callin ether output\n"));
	ether_output(recs, nr_recs, host_st, ifs, 0, 0);
	goto done;
    } else {
	printf("flowman: ip_output: packet too long (%d>%d)\n",
	       ip->ip_len, mtu-(sizeof(struct ether_header)));
	goto bad;
    }


    /*
     * fragmentation: to be done
     * 
     */

done:
    return 0;
bad:
    goto done;
}
