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
**      ip_icmp.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ICMP handling
** 
** ENVIRONMENT: 
**
**      
**      
** 
** ID : $Id: ip_icmp.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
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
 *      @(#)ip_icmp.c   8.2 (Berkeley) 1/4/94
 *      Id: ip_icmp.c,v 1.22.2.2 1997/08/25 16:33:02 wollman Exp 
 */



/* Suck in that Nemesis world! */
#include <nemesis.h>
#include <stdio.h>
#include <string.h>
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
#include <InetCtl.ih>

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

#ifdef ICMP_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif /* ICMP_TRACE */

#define UNREACHABLE 1
#define SOURCEQUENCH 2
#define HEADERPARAM 3
#define TIMEEXCEEDED 4

/* icmp dispatch table */
typedef void icmp_handler_fn(IO_Rec *recs, int nr_recs, iphost_st *host_st,
			     intf_st *ifs,  uint32_t m_flags,
			     uint8_t iph_len, uint16_t ipt_len);
static const icmp_handler_fn icmp_error_msg;
static const icmp_handler_fn icmp_echo_reply;
static const icmp_handler_fn icmp_dest_unreach;
static const icmp_handler_fn icmp_src_quench;
static const icmp_handler_fn icmp_redirect;
static const icmp_handler_fn icmp_echo_req;
static const icmp_handler_fn icmp_time_exc;
static const icmp_handler_fn icmp_param_prob;
static const icmp_handler_fn icmp_ts_req;
static const icmp_handler_fn icmp_ts_reply;
static const icmp_handler_fn icmp_addrmask_req;
static const icmp_handler_fn icmp_addrmask_reply;

static icmp_handler_fn * const icmp_handler[] =
{
    icmp_echo_reply,		/* 0*/
    icmp_error_msg,		/* 1*/
    icmp_error_msg,		/* 2*/
    icmp_dest_unreach,		/* 3*/
    icmp_src_quench,		/* 4*/
    icmp_redirect,		/* 5*/
    icmp_error_msg,		/* 6*/
    icmp_error_msg,		/* 7*/
    icmp_echo_req,		/* 8*/
    icmp_error_msg,		/* 9*/
    icmp_error_msg,		/*10*/
    icmp_time_exc,		/*11*/
    icmp_param_prob,		/*12*/
    icmp_ts_req,		/*13*/
    icmp_ts_reply,		/*14*/
    icmp_error_msg,		/*15 information request - obsolete*/
    icmp_error_msg,		/*16 information reply   - obsolete*/
    icmp_addrmask_req,		/*17*/
    icmp_addrmask_reply		/*18*/
};

/* data for application info */
typedef struct _appl_info_st   {
    InetCtl_clp	inetctl;
    uint32_t	dst_ip;
    uint16_t	port;
    uint8_t	which;
    uint8_t	code;
} appl_info_st;


/* forward decl.*/
static void icmp_reflect(IO_Rec *recs, int nr_recs, iphost_st *host_st,
			 intf_st *ifs, uint32_t m_flags,
			 uint8_t iph_len, uint16_t ipt_len);

static void icmp_send(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		      intf_st *ifs, uint8_t iph_len, uint16_t ipt_len);

static flowman_st *get_client(int port, iphost_st *host_st);

static void inform_application(appl_info_st* info);

static uint16_t get_port(IO_Rec *recs, uint8_t iph_len); 


void icmp_input(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		intf_st *ifs, uint32_t m_flags,
		uint32_t iph_len, uint16_t ipt_len)
{
    /* iph_len is the size of the ip header incl. options */
    /* ipt_len : total length of ip packet OBS, without header */
    
    register struct icmp *icp;
    int icmplen;
    int code;
    struct icmpstat icmpstat;
    int ip_extra_len = 0;  /* Thiemo: ip options length */
    
    TRC(printf("flowman: icmp_input with header length d %d, "
	       "total length d %d\n", iph_len, ipt_len));

    /*
     * Locate icmp structure in mbuf, and check
     * that not corrupted and of at least minimum length.
     */
    
    if (iph_len > sizeof(struct ip)) {
	ip_extra_len = iph_len - sizeof(struct ip);
    }
    TRC(printf("ip_extra_len %d\n", ip_extra_len));
    
    /* compute where in icmp begins. IP options can be
       there as well (in front of icmp) */
    
    icp = (struct icmp *)(recs[0].base + 
			  sizeof(struct ether_header) + 
			  sizeof(struct ip) +
			  ip_extra_len);

    /* compute icmp length */

/*  icmplen = ipt_len - iph_len; */
    icmplen = ipt_len;

    TRC(printf("icmp length : d %d\n", icmplen));
    TRC(printf("icmp_input, no ntohx  type %x code %x\n",
	       icp->icmp_type, icp->icmp_code));
    TRC(printf("icmp_id: %d\n", ntoh16(icp->icmp_id)));
    TRC(printf("icmp_seq: %d\n", ntoh16(icp->icmp_seq)));

    if (icmplen < ICMP_MINLEN) {
	printf("flowman: icmp_input: message too short\n");
	icmpstat.icps_tooshort++;
	goto freeit;
    }
    
    /* checksum */
    if (nr_recs == 1)
    {
        if (in_cksum((uint16_t *)icp, icmplen)) {
	    printf("flowman: icmp_input: checksum error\n");
	    icmpstat.icps_checksum++;
	    goto freeit;
	}
    }
    else
    {
        TRC(printf("icmp_input: checksum for more than 1 rec rec2 len %d\n",
		   recs[1].len));
	if ((cksum_morerecs((uint16_t *)icp,
			    recs[0].len-sizeof(struct ether_header) - iph_len,
			    recs, nr_recs)))
	{
	    printf("flowman: icmp_input: checksum error for >1 rec\n");
	    icmpstat.icps_checksum++;
	    goto freeit;
	}	    
    }

    
    /*
     * Message type specific processing.
     */
    if (icp->icmp_type > ICMP_MAXTYPE ||
	icp->icmp_type > (sizeof(icmp_handler)/sizeof(icmp_handler_fn *)) )
    {
	printf("flowman: icmp_input: icmp type %d too large\n",
	       icp->icmp_type);
	goto raw;
    }
    icmpstat.icps_inhist[icp->icmp_type]++;
    code = icp->icmp_code;
    
    TRC(printf("icmp switching with type%d\n", icp->icmp_type));
    (icmp_handler[icp->icmp_type])(recs, nr_recs, host_st, ifs,
				   m_flags, iph_len, ipt_len);
    TRC(printf("icmp done\n"));
    return;

raw:
freeit:
    printf("flowman: ignoring ICMP\n");
}


void icmp_echo_reply(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		     intf_st *ifs, uint32_t m_flags,
		     uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: unhandled ICMP: icmp_echo_reply\n");
}


void icmp_error_msg(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		    intf_st *ifs, uint32_t m_flags,
		    uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: unhandled ICMP: icmp_error_msg\n");
}


void icmp_dest_unreach(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		       intf_st *ifs, uint32_t m_flags,
		       uint8_t iph_len, uint16_t ipt_len)
{
    struct icmp		*icp;
    struct ip		*ip;
    int			port;
    flowman_st		*fm_st;
    appl_info_st	*ainfo;

    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));
    icp = (struct icmp *)(recs[0].base + sizeof(struct ether_header) +iph_len);

    printf("flowman: ICMP: got icmp_dest_unreach\n");

    /* try to get port number and .. */
    TRC(printf("from %x\n", ntoh32(ip->ip_src.s_addr)));
    TRC(printf("icmp type: %d\n", icp->icmp_type));
    TRC(printf("icmp code: %d\n", icp->icmp_code));
    /* it is OK to get address and port from IP resp UDP header
       of the (previously sent) message since the IP header
       and the first 8 bytes are put into the message of the
       sender */
    port = get_port(recs, iph_len);
    fm_st = get_client(port, host_st);

    if (fm_st != NULL)
    {
	/* XXX AND: might want to make this into an Rbufs channel
	 * one day.  Or convince Zoo to do MIDDL ANNOUNCEMENTS. */
	ainfo = Heap$Malloc(Pvs(heap),sizeof(appl_info_st));
	if (ainfo == NULL) {
	    printf("flowman: no memory for appl info\n");
	    return;
	}
	ainfo->inetctl = fm_st->inetctl;
	ainfo->which = UNREACHABLE;
	ainfo->dst_ip = ntoh32(ip->ip_src.s_addr);
	ainfo->port = port;
	ainfo->code = icp->icmp_code;
	Threads$Fork(Pvs(thds), inform_application, ainfo, 8192);
    }
    else
    {
	printf("flowman: ICMP dest unreach, "
	       "but no client callback installed\n");
    }
}


void icmp_src_quench(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		     intf_st *ifs, uint32_t m_flags,
		     uint8_t iph_len, uint16_t ipt_len)
{
    struct icmp *icp;
    struct ip *ip;
    int port;
    flowman_st *fm_st;
    appl_info_st *ainfo;

    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));
    icp = (struct icmp *)(recs[0].base + sizeof(struct ether_header) +iph_len);

    printf("flowman: icmp_src_quench\n");

    TRC(printf("icmp code: %d\n", icp->icmp_code));

    /* it is OK to get address and port from IP resp UDP header
       of the (previously sent) message since the IP header
       and the first 8 bytes are put into the message of the
       sender */

    port = get_port(recs, iph_len);
    fm_st = get_client(port, host_st);
    if (fm_st != NULL) {
	ainfo = Heap$Malloc(Pvs(heap),sizeof(appl_info_st));
	if (ainfo == NULL) {
	    printf("no memory for appl info\n");
	    return;
	}
	ainfo->inetctl = fm_st->inetctl;
	ainfo->which = SOURCEQUENCH;
	ainfo->dst_ip = ntoh32(ip->ip_src.s_addr);
	ainfo->port = port;
	ainfo->code = icp->icmp_code;
	Threads$Fork(Pvs(thds), inform_application, ainfo, 8192);
    }
    else
	printf("flowman: ICMP src quench: cannot inform client about event\n");

}


void icmp_redirect(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		   intf_st *ifs, uint32_t m_flags,
		   uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_redirect unhandled\n");
}


void icmp_echo_req(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		   intf_st *ifs, uint32_t m_flags,
		   uint8_t iph_len, uint16_t ipt_len)
{
    struct icmp *icp;
    struct ip *ip;
    struct icmpstat icmpstat;

    ip = (struct ip *)(recs[0].base + sizeof(struct ether_header));
    TRC(printf("icmp_echo_req->must send back an echo_reply\n"));

    icp = (struct icmp *)(recs[0].base +
			  sizeof(struct ether_header) +
			  iph_len);

    TRC(printf("icmp_echo: ip len %d\n", ip->ip_len));

    /* re-write the packet in-place */
    icp->icmp_type = ICMP_ECHOREPLY;

    TRC(printf("icmp type: %d wanted %d\n", icp->icmp_type, ICMP_ECHOREPLY));

    /* reflect */
    TRC(printf("icmp id: %d\n", ntoh16(icp->icmp_id)));
    TRC(printf("icmp seq: %d\n", ntoh16(icp->icmp_seq)));
    TRC(printf("icmp-echo-req packet length before: %d\n", ip->ip_len));
    TRC(printf("iph_len (network):%d\n", hton16(iph_len)));
    TRC(printf("iph_len (host):%d\n", iph_len));

    ip->ip_len += iph_len;     /* since ip_input deducts this */
    TRC(printf("icmp-echo-req packet length after: %d\n", ip->ip_len));

    icmpstat.icps_reflect++;
    icmpstat.icps_outhist[icp->icmp_type]++;

    icmp_reflect(recs, nr_recs, host_st, ifs, m_flags, iph_len, ipt_len);
}


void icmp_time_exc(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		   intf_st *ifs, uint32_t m_flags,
		   uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_time_exc unhandled\n");
}


void icmp_param_prob(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		     intf_st *ifs, uint32_t m_flags,
		     uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_param_prob unhandled\n");
}


void icmp_ts_req(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		 intf_st *ifs, uint32_t m_flags,
		 uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_ts_req unhandled\n");
}


void icmp_ts_reply(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		   intf_st *ifs, uint32_t m_flags,
		   uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_ts_reply unhandled\n");
}


void icmp_addrmask_req(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		       intf_st *ifs, uint32_t m_flags,
		       uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_addrmask_req unhandled\n");
}


void icmp_addrmask_reply(IO_Rec *recs, int nr_recs, iphost_st *host_st,
			 intf_st *ifs, uint32_t m_flags,
			 uint8_t iph_len, uint16_t ipt_len)
{
    printf("flowman: icmp_addrmask_reply unhandled\n");
}


/*****************      icmp_error      ******************/

/* state: almost everything updated, not destifp */
/* XXX AND: this is _way_ ugly */

/* AND: This function sends an ICMP out, and includes the old IP
 * datagram and the first 8 bytes of old IP payload (but missing out
 * any IP options present).  The "type" and "code" arguments set which
 * kind of ICMP is to be sent. */
void icmp_error(IO_Rec *recs, int nr_recs, iphost_st *host_st, intf_st *ifs,
		uint32_t m_flags, uint8_t type, uint8_t code,
		uint32_t dest, uint8_t destifp)
    /* dest: next-hop router address included in ICMP redirect message
       XXX struct ifnet *destifp; */
{
    register struct ip *oip;
    register struct icmp *icp;	/* icmp header */
    struct icmpstat icmpstat;
    struct icmp *old_icmp;	/* which may not exist, just tested */
    uint8_t oiplen;		/* length of orig IP header */

    if (type > ICMP_MAXTYPE) {
	printf("flowman: icmp_error: got strange icmp type %d\n", type);
	goto freeit;
    }

    oip = (struct ip *)(recs[0].base + sizeof(struct ether_header));
    oiplen = oip->ip_hl << 2;
    TRC(printf("old ip length %d\n", oiplen));
    if (type != ICMP_REDIRECT)
	icmpstat.icps_error++;

    /*
     * Don't send error if not the first fragment of message.
     * Don't error if the old packet protocol was ICMP
     * error message, only known informational types.
     */
    if (oip->ip_off &~ (IP_MF|IP_DF))
	goto freeit;

    /* this is either the RXed ICMP or the RXed UDP header */
    old_icmp = (struct icmp *)(recs[0].base +
			       sizeof(struct ether_header) +
			       oiplen);

    /* this is the ICMP header about to be TXed */
    icp = (struct icmp *)(recs[0].base +
			  sizeof(struct ether_header) +
			  sizeof(struct ip));

    if (oip->ip_p == IPPROTO_ICMP && type != ICMP_REDIRECT &&
	/*n->m_len >= oiplen + ICMP_MINLEN &&*/	
	recs[0].len >= sizeof(struct ether_header)+ oiplen + ICMP_MINLEN &&
	!ICMP_INFOTYPE(old_icmp->icmp_type)) {
	icmpstat.icps_oldicmp++;
	goto freeit;
    }

    /* Don't send error in response to a multicast or broadcast packet */
    if (m_flags & (M_BCAST|M_MCAST))
    {
	printf("flowman: icmp_error: not replying to broadcast/multicast\n");
	goto freeit;
    }

    /*
     * First, push back the old IP datagram and next 8 bytes, losing
     * any options in the process.
     */

    /* move the 64bits (8bytes) of old IP payload (actually old UDP
     * header), skipping over the new IP, the new ICMP, and the old IP
     * header.  May have overlap due to options in the old IP header. */
    memmove(((char*)icp)+8/*ICMP*/+sizeof(struct ip), old_icmp, 8);

    /* now copy RXed IP header into old IP header place, ignoring
     * options */
    memcpy(((char *)(oip+1))+8, oip, sizeof(struct ip));

    /* Trim outgoing packet to correct size */
    recs[0].len = (sizeof(struct ether_header) +
		   sizeof(struct ip) +
		   8 /* ICMP */ +
		   sizeof(struct ip) /* old IP */ +
		   8 /* first 64bits from old payload */);
    /* and don't need recs[1] anymore */
    recs[1].len = 0;

    /* log the output */
    icmpstat.icps_outhist[type]++;

    /* write the ICMP header */
    icp->icmp_type = type;
    icp->icmp_code = code;

    if (type == ICMP_REDIRECT) {
	icp->icmp_gwaddr.s_addr = dest;
    } else {
	icp->icmp_void = 0;
	/*
	 * The following assignments assume an overlay with the
	 * zeroed icmp_void field.
	 */
	if (type == ICMP_PARAMPROB) {
	    icp->icmp_pptr = code;
	    code = 0;
	} else if (type == ICMP_UNREACH &&
		   code == ICMP_UNREACH_NEEDFRAG && destifp) {
	    /*  XXX fragmentation stuff
 	    icp->icmp_nextmtu = htons(destifp->if_mtu);
	    */
	}
    }

    /* write bits of IP header icmp_reflect doesn't */
    oip->ip_len = recs[0].len - sizeof(struct ether_header);
    oip->ip_hl  = sizeof(struct ip) >> 2;
    oip->ip_p   = IPPROTO_ICMP;
    oip->ip_tos = 0;

    /* only send the first rec */
    icmp_reflect(recs, 1, host_st, ifs, m_flags, sizeof(struct ip),
		 8/*ICMP*/ + sizeof(struct ip) + 8/*64bits payload*/);

    return;

freeit:
    printf("flowman: error in icmp_error\n");
}



/*****************      icmp_reflect      ******************/

static void icmp_reflect(IO_Rec *recs, int nr_recs, iphost_st *host_st,
			 intf_st *ifs, uint32_t m_flags,
			 uint8_t iph_len, uint16_t icmpt_len)
{
    /* iph_len is the size of the ip header incl. options */
    /* icmpt_len : total length of icmp packet (excl. "new" IP header) */
    
    register struct ip *ip = (struct ip *)(recs[0].base +
					   sizeof(struct ether_header));
    struct in_addr t;
    int optlen = icmpt_len - iph_len;
  

    TRC(printf("flowman icmp reflect\n"));
    TRC(printf("flowman icmp_reflect ip len %d\n", ip->ip_len));

    /* part I: adress selection */

    /* src address becomes dest. address */
    t = ip->ip_dst;
    ip->ip_dst = ip->ip_src;

    /* at the moment (current ip_input.c) Thiemo knows that the packet
     * was for us */

    ip->ip_src = t;
    ip->ip_ttl = MAXTTL;
  


    /* part II: option construction */
    if (optlen > 0)  /* if any options present */
    {
	register uchar_t UNUSED *cp;
	int UNUSED opt, UNUSED cnt;
	uint32_t UNUSED len;
 
	TRC(printf("icmp option constructing, not done yet\n"));
	/* part III: final assembly */
    
	/*
	 * Now strip out original options by copying rest of first
	 * mbuf's data back, and adjust the IP length.
	 */
    }

    /*
      m->m_flags &= ~(M_BCAST|M_MCAST);
      */
    icmp_send(recs, nr_recs, host_st, ifs, iph_len, icmpt_len);
    return;

    /* UNSED done: */
    printf("flowman icmp_reflect: can't happen!\n");
}


/*****************      icmp_send      ******************/

static void icmp_send(IO_Rec *recs, int nr_recs, iphost_st *host_st,
		      intf_st *ifs, uint8_t iph_len, uint16_t icmpt_len)
{
    /* iph_len is the size of the ip header incl. options */
    /* icmpt_len : total length of icmp packet (excl. "new" IP header) */
 
    register struct icmp *icp;

    icp = (struct icmp *)(recs[0].base + sizeof(struct ether_header) +iph_len);

    TRC(printf("flowman icmp_send iph len %d\n", iph_len));

    icp->icmp_cksum = 0;
    /* checksum covers whole ICMP packet, not only ICMP header */
    TRC(printf("flowman icmp_send ipt len (for chksum) %d\n", icmpt_len));
    if (nr_recs == 1)
	icp->icmp_cksum = in_cksum((uint16_t *)icp, icmpt_len);
    else {
	icp->icmp_cksum = cksum_morerecs((uint16_t *)icp,
					 recs[0].len -
					 sizeof(struct ether_header) -
					 iph_len,
					 recs,
					 nr_recs);
    }

    TRC(printf("flowman icmp_send calling ip_output\n"));
    ip_output(recs, nr_recs, host_st, ifs, 0, 0);
    /* 0 are *opt and flags */
}


/* find out the client that uses this port since this client needs
   to be informed about something that has happened */
/* XXX only searches UDP port space currently */
flowman_st * get_client(int port, iphost_st *host_st) 
{
    port_alloc_t *pallocs;
    flowman_st *fclient=NULL;

    TRC(printf("starting get_client with port %d\n", port));
    if (host_st->UDPportuse == NULL)
	return NULL;

    pallocs = host_st->UDPportuse;

    while(pallocs)
    {
	TRC(printf("act client port %d\n", pallocs->port));
	if (pallocs->port == port)
	    break;
	pallocs = pallocs->next;
    }

    if (!pallocs)
	return NULL; 	/* not found */

    TRC(printf("port found %d\n", pallocs->port));
    fclient = pallocs->client;
    if (fclient == NULL) {
	printf("flowman: get_client: found client but it is NULL\n");
    } else {
	printf("flowman: application for icmp info found\n");
	return fclient;
    }

    return NULL;
}


/* inform application about event that has happened, e.g. a packet has
   been sent to unreachable host or port etc. */
static void inform_application(appl_info_st *info) 
{
    
    /* now try to inform client */
    if (info->inetctl != NULL)
    {
	printf("icmp trying to reach client application\n");
	TRY {
	    switch(info->which) {
	    case UNREACHABLE:
		InetCtl$Unreachable(info->inetctl,
				    info->dst_ip, info->port, info->code);
		break;
		
	    case SOURCEQUENCH:
		InetCtl$SourceQuench(info->inetctl,
				     info->dst_ip, info->port);
		break;

	    case HEADERPARAM:
		InetCtl$HeaderParam(info->inetctl,
				    info->dst_ip, info->port, info->code);
		break;

	    case TIMEEXCEEDED:
		InetCtl$TimeExceeded(info->inetctl,
				     info->dst_ip, info->port, info->code);
		break;

	    }
	} CATCH_ALL {
	    printf("flowman: inform_application: caught exception\n");
	    info->inetctl = NULL;
	}
	ENDTRY;
    }
    else
    {
	printf("flowman: inform_application: no inetctl for client\n");
    }
}



/* get the port number from the "old" UDP header included in received
   ICMP message */
/* should work for TCP as well since port is at same place in TCP header */
/* Returns port in network endian */
static uint16_t get_port(IO_Rec *recs, uint8_t iph_len)
{
    struct ip *oldip;  /* the IP header of the packet that caused the ICMP
			  message to be sent */
    struct udphdr *oldudp;
    uint8_t hlen;  /* length of older IP header */

    oldip = (struct ip *)(recs[0].base +
			  sizeof(struct ether_header) +
			  iph_len +
			  8); /* size of icmp */
    hlen = oldip->ip_hl << 2;
    TRC(printf("length of old IP header: %d\n", hlen));
    
    oldudp = (struct udphdr *)(recs[0].base +
			       sizeof(struct ether_header) +
			       hlen +
			       iph_len +
			       8);
    /* XXX maybe options present, have to check header length of oldip first */

    TRC(printf("remote address: %x\n", ntoh32(oldip->ip_dst.s_addr)));
    TRC(printf("local port: %d\n", ntoh16(oldudp->uh_sport)));

    return oldudp->uh_sport;
}
