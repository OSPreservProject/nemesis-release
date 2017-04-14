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
**      ether_input.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of ether_input routine
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: ether_input.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
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
#include "tcp.h"
#include "fm_constants.h"

#ifdef DEFAULT_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* DEFAULT_TRACE */


void ether_input(IO_Rec *recs, int nr_recs,
		 Net_EtherAddr *mac, iphost_st *host_st)
{
    struct ether_header *header;
    uint32_t m_flags=0;

    TRC(printf("flowman: in ether_input\n"));
    if (recs[0].base == NULL)
    {
	printf("flowman ether_input: header failure\n");
	goto bad_ether;
    }

    if (recs[0].len < ETHER_HDR_LEN)
    {
	printf("flowman: ether_input: warning: short first IORec, "
	       "ether hdr corrupted?\n");
	goto bad_ether;  
    }


    /* offset of 0 to get to ether header */
    header = (struct ether_header *)recs[0].base;


    /* check multicast address */
    if (header->ether_dhost[0] & 1) {
        goto bad_addr;
	/* XXX
	m_flags |= M_MCAST;
	printf("ethernet_input received multicast datagram\n");
	ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));
	printf("addr: %x\n", ntohl(ip->ip_dst.s_addr));
        goto for_us;   too much overload at the moment */
    }

    /* check ethernet host address */
    if (memcmp(header->ether_dhost, mac, 6)) {
	TRC(unsigned char *p = header->ether_dhost);
	TRC(printf("flowman ether_input: warning: "
		   "recvd packet with dest (%02x:%02x:%02x:%02x::%02x:%02x) "
		   "!= my addr(%02x:%02x:%02x:%02x:%02x:%02x)!\n",
		   p[0], p[1], p[2], p[3], p[4], p[5],
		   mac->a[0], mac->a[1], mac->a[2],
		   mac->a[3], mac->a[4], mac->a[5]));
	goto bad_addr;
    }

    
    /*for_us:     wait for multicast stuff, then needed*/
    /* check for types */
    TRC(printf("ntoh16 header etherype: %x\n", ntoh16(header->ether_type)));
    switch (ntoh16(header->ether_type)) {
        case ETHERTYPE_IP:
	  TRC(printf("flowman ether_input: OK, type IP\n"));
	  TRC(printf("ether_input dest %x\n", header->ether_dhost));
	  TRC(printf("ether_input src %x\n", header->ether_shost));

	  ip_input(recs, nr_recs, host_st, m_flags);

	  break;

        case ETHERTYPE_ARP:
	  printf("flowman: ether_input Ethernet ARP type: can't happen!\n");
	  break;

    }

    return;
    
bad_ether:
    printf("flowman: ether_input: error received packet\n");
    return;

bad_addr:        /* packet not for us */
    return;
}

