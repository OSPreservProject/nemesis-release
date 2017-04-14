/* 
   SICS adaption:
   $Id: ip_var.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
 */
/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)ip_var.h	8.2 (Berkeley) 1/9/95
 *	Id: ip_var.h,v 1.24.2.3 1997/09/16 12:03:45 ache Exp 
 */

#ifndef _NETINET_IP_VAR_H_
#define	_NETINET_IP_VAR_H_

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 */
struct ipovly {
	uint32_t	ih_next, ih_prev;	/* for protocol sequence q's */
	uchar_t	ih_x1;			/* (unused) */
	uchar_t	ih_pr;			/* protocol */
	uint16_t	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
};

/*
 * Ip reassembly queue structure.  Each fragment
 * being reassembled is attached to one of these structures.
 * They are timed out after ipq_ttl drops to 0, and may also
 * be reclaimed if memory becomes tight.
 */
#if 0 /* Thiemo */
struct ipq {
	struct	ipq *next,*prev;	/* to other reass headers */
	uchar_t	ipq_ttl;		/* time for reass q to live */
	uchar_t	ipq_p;			/* protocol of this fragment */
	uint16_t	ipq_id;			/* sequence id for reassembly */
	struct	ipasfrag *ipq_next,*ipq_prev;
					/* to ip headers of fragments */
	struct	in_addr ipq_src,ipq_dst;
#ifdef IPDIVERT
	uint16_t ipq_divert;		/* divert protocol port */
#endif
};
#endif

/*
 * Ip header, when holding a fragment.
 *
 * Note: ipf_next must be at same offset as ipq_next above
 */
struct	ipasfrag {
#ifdef LITTLE_ENDIAN /*BYTE_ORDER == LITTLE_ENDIAN   Thiemo */ 
	uchar_t	ip_hl:4,
		ip_v:4;
#endif
#ifdef BIG_ENDIAN /* Thiemo BYTE_ORDER == BIG_ENDIAN */
	uchar_t	ip_v:4,
		ip_hl:4;
#endif
	uchar_t	ipf_mff;		/* XXX overlays ip_tos: use low bit
					 * to avoid destroying tos;
					 * copied from (ip_off&IP_MF) */
	uint16_t	ip_len;
	uint16_t	ip_id;
	uint16_t	ip_off;
	uchar_t	ip_ttl;
	uchar_t	ip_p;
	uint16_t	ip_sum;
	struct	ipasfrag *ipf_next;	/* next fragment */
	struct	ipasfrag *ipf_prev;	/* previous fragment */
};

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN	40

struct ipoption {
	struct	in_addr ipopt_dst;	/* first-hop dst if source routed */
	char	ipopt_list[MAX_IPOPTLEN];	/* options proper */
};

/*
 * Structure attached to inpcb.ip_moptions and
 * passed to ip_output when IP multicast options are in use.
 */
#if 0  /* Thiemo */
struct ip_moptions {
	struct	ifnet *imo_multicast_ifp; /* ifp for outgoing multicasts */
	uchar_t	imo_multicast_ttl;	/* TTL for outgoing multicasts */
	uchar_t	imo_multicast_loop;	/* 1 => hear sends if a member */
	uint16_t	imo_num_memberships;	/* no. memberships this socket */
	struct	in_multi *imo_membership[IP_MAX_MEMBERSHIPS];
	uint32_t	imo_multicast_vif;	/* vif num outgoing multicasts */
};

#endif
#if 0 /* Thiemo -> moved to prot_stat.h */
struct	ipstat {
	uint32_t	ips_total;		/* total packets received */
	uint32_t	ips_badsum;		/* checksum bad */
	uint32_t	ips_tooshort;		/* packet too short */
	uint32_t	ips_toosmall;		/* not enough data */
	uint32_t	ips_badhlen;		/* ip header length < data size */
	uint32_t	ips_badlen;		/* ip length < ip header length */
	uint32_t	ips_fragments;		/* fragments received */
	uint32_t	ips_fragdropped;	/* frags dropped (dups, out of space) */
	uint32_t	ips_fragtimeout;	/* fragments timed out */
	uint32_t	ips_forward;		/* packets forwarded */
	uint32_t	ips_cantforward;	/* packets rcvd for unreachable dest */
	uint32_t	ips_redirectsent;	/* packets forwarded on same net */
	uint32_t	ips_noproto;		/* unknown or unsupported protocol */
	uint32_t	ips_delivered;		/* datagrams delivered to upper level*/
	uint32_t	ips_localout;		/* total ip packets generated here */
	uint32_t	ips_odropped;		/* lost packets due to nobufs, etc. */
	uint32_t	ips_reassembled;	/* total packets reassembled ok */
	uint32_t	ips_fragmented;		/* datagrams successfully fragmented */
	uint32_t	ips_ofragments;		/* output fragments created */
	uint32_t	ips_cantfrag;		/* don't fragment flag was set, etc. */
	uint32_t	ips_badoptions;		/* error in option processing */
	uint32_t	ips_noroute;		/* packets discarded due to no route */
	uint32_t	ips_badvers;		/* ip version != 4 */
	uint32_t	ips_rawout;		/* total raw ip packets generated */
	uint32_t	ips_toolong;		/* ip length > max ip packet size */
};
#endif

/* flags passed to ip_output as last parameter */
#define	IP_FORWARDING		0x1		/* most of ip header exists */
#define	IP_RAWOUTPUT		0x2		/* raw ip header exists */
#define	IP_ROUTETOIF		SO_DONTROUTE	/* bypass routing tables */
#define	IP_ALLOWBROADCAST	SO_BROADCAST	/* can send broadcast packets */
/* Thiemo
struct route;
*/
extern struct	ipstat	ipstat;
extern uint16_t	ip_id;				/* ip packet ctr, for ids */
extern int	ip_defttl;			/* default IP ttl */

#endif /* _NETINET_IP_VAR_H_ */
