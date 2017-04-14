/*
   SICS adaptation
   ID: $Id: prot_stat.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
*/
/*
 * Copyright (c) 1982, 1986, 1993
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
 *      @(#)ip_var.h    8.2 (Berkeley) 1/9/95
 *      Id: ip_var.h,v 1.24.2.3 1997/09/16 12:03:45 ache Exp 
 */

/* ip, udp, and tcp stat. */

struct _ipstat_st {
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


struct	_udpstat_st {
				/* input statistics: */
	uint32_t	udps_ipackets;		/* total input packets */
	uint32_t	udps_hdrops;		/* packet shorter than header */
	uint32_t	udps_badsum;		/* checksum error */
	uint32_t	udps_badlen;		/* data length larger than packet */
	uint32_t	udps_noport;		/* no socket on port */
	uint32_t	udps_noportbcast;	/* of above, arrived as broadcast */
	uint32_t	udps_fullsock;		/* not delivered, input socket full */
	uint32_t	udpps_pcbcachemiss;	/* input packets missing pcb cache */
	uint32_t	udpps_pcbhashmiss;	/* input packets not for hashed pcb */
				/* output statistics: */
	uint32_t	udps_opackets;		/* total output packets */
};


struct	_tcpstat_st {
	uint32_t	tcps_connattempt;	/* connections initiated */
	uint32_t	tcps_accepts;		/* connections accepted */
	uint32_t	tcps_connects;		/* connections established */
	uint32_t	tcps_drops;		/* connections dropped */
	uint32_t	tcps_conndrops;		/* embryonic connections dropped */
	uint32_t	tcps_closed;		/* conn. closed (includes drops) */
	uint32_t	tcps_segstimed;		/* segs where we tried to get rtt */
	uint32_t	tcps_rttupdated;	/* times we succeeded */
	uint32_t	tcps_delack;		/* delayed acks sent */
	uint32_t	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	uint32_t	tcps_rexmttimeo;	/* retransmit timeouts */
	uint32_t	tcps_persisttimeo;	/* persist timeouts */
	uint32_t	tcps_keeptimeo;		/* keepalive timeouts */
	uint32_t	tcps_keepprobe;		/* keepalive probes sent */
	uint32_t	tcps_keepdrops;		/* connections dropped in keepalive */

	uint32_t	tcps_sndtotal;		/* total packets sent */
	uint32_t	tcps_sndpack;		/* data packets sent */
	uint32_t	tcps_sndbyte;		/* data bytes sent */
	uint32_t	tcps_sndrexmitpack;	/* data packets retransmitted */
	uint32_t	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	uint32_t	tcps_sndacks;		/* ack-only packets sent */
	uint32_t	tcps_sndprobe;		/* window probes sent */
	uint32_t	tcps_sndurg;		/* packets sent with URG only */
	uint32_t	tcps_sndwinup;		/* window update-only packets sent */
	uint32_t	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	uint32_t	tcps_rcvtotal;		/* total packets received */
	uint32_t	tcps_rcvpack;		/* packets received in sequence */
	uint32_t	tcps_rcvbyte;		/* bytes received in sequence */
	uint32_t	tcps_rcvbadsum;		/* packets received with ccksum errs */
	uint32_t	tcps_rcvbadoff;		/* packets received with bad offset */
	uint32_t	tcps_rcvshort;		/* packets received too short */
	uint32_t	tcps_rcvduppack;	/* duplicate-only packets received */
	uint32_t	tcps_rcvdupbyte;	/* duplicate-only bytes received */
	uint32_t	tcps_rcvpartduppack;	/* packets with some duplicate data */
	uint32_t	tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	uint32_t	tcps_rcvoopack;		/* out-of-order packets received */
	uint32_t	tcps_rcvoobyte;		/* out-of-order bytes received */
	uint32_t	tcps_rcvpackafterwin;	/* packets with data after window */
	uint32_t	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	uint32_t	tcps_rcvafterclose;	/* packets rcvd after "close" */
	uint32_t	tcps_rcvwinprobe;	/* rcvd window probe packets */
	uint32_t	tcps_rcvdupack;		/* rcvd duplicate acks */
	uint32_t	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	uint32_t	tcps_rcvackpack;	/* rcvd ack packets */
	uint32_t	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	uint32_t	tcps_rcvwinupd;		/* rcvd window update packets */
	uint32_t	tcps_pawsdrop;		/* segments dropped due to PAWS */
	uint32_t	tcps_predack;		/* times hdr predict ok for acks */
	uint32_t	tcps_preddat;		/* times hdr predict ok for data pkts */
	uint32_t	tcps_pcbcachemiss;
	uint32_t	tcps_cachedrtt;		/* times cached RTT in route updated */
	uint32_t	tcps_cachedrttvar;	/* times cached rttvar updated */
	uint32_t	tcps_cachedssthresh;	/* times cached ssthresh updated */
	uint32_t	tcps_usedrtt;		/* times RTT initialized from route */
	uint32_t	tcps_usedrttvar;	/* times RTTVAR initialized from rt */
	uint32_t	tcps_usedssthresh;	/* times ssthresh initialized from rt*/
	uint32_t	tcps_persistdrop;	/* timeout in persist state */
	uint32_t	tcps_badsyn;		/* bogus SYN, e.g. premature ACK */
	uint32_t	tcps_mturesent;		/* resends due to MTU discovery */
	uint32_t	tcps_listendrop;	/* listen queue overflows */
};
