/*
   sics adaptation
   ID: $Id: tcp_var.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
*/
/*
 * Copyright (c) 1982, 1986, 1993, 1994, 1995
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
 *	@(#)tcp_var.h	8.4 (Berkeley) 5/24/95
 * 	Id: tcp_var.h,v 1.36 1996/09/13 23:54:03 pst Exp 
 */

#ifndef _NETINET_TCP_VAR_H_
#define _NETINET_TCP_VAR_H_


/*
 * Structure to hold TCP options that are only used during segment
 * processing (in tcp_input), but not held in the tcpcb.
 * It's basically used to reduce the number of parameters
 * to tcp_dooptions.
 */
struct tcpopt {
	uint32_t	to_flag;		/* which options are present */
#define TOF_TS		0x0001		/* timestamp */
#define TOF_CC		0x0002		/* CC and CCnew are exclusive */
#define TOF_CCNEW	0x0004
#define	TOF_CCECHO	0x0008
	uint32_t	to_tsval;
	uint32_t	to_tsecr;
	tcp_cc	to_cc;		/* holds CC or CCnew */
	tcp_cc	to_ccecho;
};


#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		32	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		5	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	16	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	4	/* shift for rttvar; 2 bits */
#define	TCP_DELTA_SHIFT		2	/* see tcp_input.c */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This version of the macro adapted from a paper by Lawrence
 * Brakmo and Larry Peterson which outlines a problem caused
 * by insufficient precision in the original implementation,
 * which results in inappropriately large RTO values for very
 * fast networks.
 */
#define	TCP_REXMTVAL(tp) \
	((((tp)->t_srtt >> (TCP_RTT_SHIFT - TCP_DELTA_SHIFT))  \
	  + (tp)->t_rttvar) >> TCP_DELTA_SHIFT)

/* XXX
 * We want to avoid doing m_pullup on incoming packets but that
 * means avoiding dtom on the tcp reassembly code.  That in turn means
 * keeping an mbuf pointer in the reassembly queue (since we might
 * have a cluster).  As a quick hack, the source & destination
 * port numbers (which are no longer needed once we've located the
 * tcpcb) are overlayed with an mbuf pointer.
 */
#define REASS_MBUF(ti) (*(struct mbuf **)&((ti)->ti_t))

#if 0 /* Thiemo moved to prot_stat.h */
/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
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
#endif

/*
 * Names for TCP sysctl objects
 */
#define	TCPCTL_DO_RFC1323	1	/* use RFC-1323 extensions */
#define	TCPCTL_DO_RFC1644	2	/* use RFC-1644 extensions */
#define	TCPCTL_MSSDFLT		3	/* MSS default */
#define TCPCTL_STATS		4	/* statistics (read-only) */
#define	TCPCTL_RTTDFLT		5	/* default RTT estimate */
#define	TCPCTL_KEEPIDLE		6	/* keepalive idle timer */
#define	TCPCTL_KEEPINTVL	7	/* interval to send keepalives */
#define	TCPCTL_SENDSPACE	8	/* send buffer space */
#define	TCPCTL_RECVSPACE	9	/* receive buffer space */
#define	TCPCTL_KEEPINIT		10	/* receive buffer space */
#define TCPCTL_MAXID		11

#define TCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "rfc1323", CTLTYPE_INT }, \
	{ "rfc1644", CTLTYPE_INT }, \
	{ "mssdflt", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
	{ "rttdflt", CTLTYPE_INT }, \
	{ "keepidle", CTLTYPE_INT }, \
	{ "keepintvl", CTLTYPE_INT }, \
	{ "sendspace", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
	{ "keepinit", CTLTYPE_INT }, \
}

#ifdef KERNEL
extern	struct inpcbhead tcb;		/* head of queue of active tcpcb's */
extern	struct inpcbinfo tcbinfo;
extern	struct tcpstat tcpstat;	/* tcp statistics */
extern	int32_t tcp_mssdflt;	/* XXX */
extern	uint32_t tcp_now;		/* for RFC 1323 timestamps */
#endif /* KERNEL */

#endif /* _NETINET_TCP_VAR_H_ */
