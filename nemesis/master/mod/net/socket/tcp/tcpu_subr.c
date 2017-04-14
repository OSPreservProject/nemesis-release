/**************************************************************************\
*          Copyright (c) 1995 INRIA Sophia Antipolis, FRANCE.              *
*                                                                          *
* Permission to use, copy, modify, and distribute this material for any    *
* purpose and without fee is hereby granted, provided that the above       *
* copyright notice and this permission notice appear in all copies.        *
* WE MAKE NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY OF THIS     *
* MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS", WITHOUT ANY EXPRESS   *
* OR IMPLIED WARRANTIES.                                                   *
\**************************************************************************/
/**************************************************************************\
*                                                                          *
*  File              : tcpu_subr.c                                         *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

/*
 *	@(#)tcpu_subr.c	7.20 
*/

#include <nemesis.h>
#include <stdio.h>
#include <netdb.h>
#include <links.h>
#include <time.h>
#include <FlowMan.ih>
#include "inet.h"
#include <iana.h>
#include <IO.ih>
#include <IOMacros.h>
#include <IDCMacros.h>

#define  PR_SLOWHZ 2 /* from /usr/include/sys/protosw.h */

/* patchable/settable parameters for tcp */
/*
int	tcp_ttl = TCP_TTL;
int 	tcp_mssdflt = TCP_MSS;
int 	tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
*/

const  string_t tcpstates[] = {
    "CLOSED",	
    "LISTEN",	
    "SYN_SENT",	
    "SYN_RCVD",
    "ESTABLISHED",	
    "CLOSE_WAIT",	
    "FIN_WAIT_1",	
    "CLOSING",
    "LAST_ACK",	
    "FIN_WAIT_2",	
    "TIME_WAIT"
};


extern	struct inpcb *tcp_last_inpcb;
const uchar tcp_outflags[TCP_NSTATES]= 
{ 
    TH_RST|TH_ACK, 
    0, 
    TH_SYN, 
    TH_SYN|TH_ACK,
    TH_ACK, 
    TH_ACK, 
    TH_FIN|TH_ACK, 
    TH_FIN|TH_ACK, 
    TH_FIN|TH_ACK, 
    TH_ACK, 
    TH_ACK
};

const int tcp_backoff[TCP_MAXRXTSHIFT + 1]
= { 1,2,4,8,16,32,64,64,64,64,64,64,64 };

/*
 * Tcp initialization
 */
struct tcp_data *tcp_data_init(sock)
    struct socket *sock;
{
    struct tcpcb *tp;
    struct tcp_data *td;

    sock->data=
	(struct tcp_data *)Heap$Calloc(Pvs(heap), sizeof(struct tcp_data) +
				       sizeof(struct tcpcb) +
				       sizeof(struct buffout) +
				       sizeof(struct buffin) +
				       sizeof(struct datapointer) 
				       ,1);

/* RS: might need to cast these assignments... */
    td=sock->data;
    tp=(struct tcpcb *)(td+1);
    td->tcp_cb=tp;
    td->buff_out=(struct buffout *)(tp+1);
    td->buff_in=(struct buffin *)(td->buff_out+1); 
    td->data_pt=(struct datapointer *)(td->buff_in+1);

    bzero(tp, sizeof(*tp));

    tp->seg_next = (struct tcpiphdr *)tp;
    tp->seg_prev = (struct tcpiphdr *)tp;
    tp->sock = 0;
    tp->t_flags = 0;             /* sends options! */
    tp->time_ack = 0;            /* time for ack and packets */
    tp->time_send = 0;           /* time for ack and packets */
    /*
     * Init srtt to TCPTV_SRTTBASE (0), so we can tell that we have no
     * rtt estimate.  Set rttvar so that srtt + 2 * rttvar gives
     * reasonable initial retransmit time.
     */
    tp->t_srtt = TCPTV_SRTTBASE;
    tp->t_rttmin = TCPTV_MIN;
    TCPT_RANGESET(tp->t_rxtcur,
                  ((TCPTV_SRTTBASE >> 2) + (TCPTV_SRTTDFLT << 2)) >> 1,
                  TCPTV_MIN, TCPTV_REXMTMAX);
    tp->snd_cwnd = TCP_MAXWIN;
    tp->snd_ssthresh = TCP_MAXWIN;
    tp->tcp_ttl = TCP_TTL;
    tp->tcp_mssdflt = TCP_MSS;
    tp->tcp_rttdflt = TCPTV_SRTTDFLT / PR_SLOWHZ;
    tp->t_maxseg = tp->tcp_mssdflt;
    tp->t_rttvar = tp->tcp_rttdflt * PR_SLOWHZ << 2;
    tp->tcprexmtthresh=3;
    tp->tcpprintfs=1;
    tp->tcpcksum=1;
    tp->tcpnodelack=1;
    tp->tcp_keepidle=TCPTV_KEEP_IDLE;
    tp->tcp_keepintvl=TCPTV_KEEPINTVL;
    tp->loop=0;

    tp->conn_st.stack_built = False;

    LINK_INITIALISE(&tp->rx_blocks);
    LINK_INITIALISE(&tp->syn_rx_blocks);
    
    tp->fman = IDC_OPEN("svc>net>iphost-default", FlowMan_clp);

    return (sock->data);
}

/*
 * Create template to be used to send tcp packets on a connection.
 * Call after host entry created, allocates an mbuf and fills
 * in a skeletal tcp/ip header, minimizing the amount of work
 * necessary when the connection is used.
 */
struct tcpiphdr *tcp_template(tp)
    struct tcpcb *tp;
{
    register struct tcpiphdr *ti;
    struct sockaddr_in *laddr = (struct sockaddr_in *)(&tp->sock->laddr);
    
    if (tp->t_template == NULL) {
	ti = tp->wiov[0].base; /* XXX */
    } else {
	eprintf("tcp_template:connection exists already!\n");
	return((struct tcpiphdr *)-1);
    }
    
    ti->ti_next       = 0;
    ti->ti_prev       = 0;
    ti->ti_x1         = 0;
    ti->ti_pr         = 100;
    ti->ti_pr         = IP_PROTO_TCP;
    ti->ti_len        = htons(sizeof (struct tcphdr));
    ti->ti_src.s_addr = laddr->sin_addr.s_addr;
    ti->ti_dst.s_addr = 0;
    ti->ti_sport      = laddr->sin_port;
    ti->ti_dport      = 0;
    ti->ti_seq        = 0;
    ti->ti_ack        = 0;
#if 0
    ti->ti_x2         = 5;
    ti->ti_off        = sizeof (struct tcphdr) >> 2; /* 5 */
#endif
    ti->ti_off        = (sizeof (struct tcphdr) >> 2) << 4; /* 5 */
    ti->ti_flags      = 0;
    ti->ti_win        = 0;
    ti->ti_sum        = 0;
    ti->ti_urp        = 0;
    return (ti);
}

struct qelem {
      struct  qelem *q_forw;
      struct  qelem *q_back;
      char    *q_data;
};

void remque(struct qelem *elem)
{
  elem->q_back->q_forw=elem->q_forw;
  elem->q_forw->q_back=elem->q_back;
}

void insque(struct qelem *pos,struct qelem *elem)
{
   elem->q_back=pos;
   elem->q_forw=pos->q_forw;
   pos->q_forw->q_back=elem;
   pos->q_forw=elem;
}

/*
 * Send a single message to the TCP at address specified by
 * the given TCP/IP header.  If m == 0, then we make a copy
 * of the tcpiphdr at ti and send directly to the addressed host.
 * This is used to force keep alive messages out using the TCP
 * template for a connection tp->t_template.  If flags are given
 * then we send a message back to the TCP which originated the
 * segment ti, and discard the mbuf containing it and any other
 * attached mbufs.
 *
 * In any case the ack and sequence number of the transmitted
 * segment are as specified by the parameters.
 */


void tcp_respond(struct tcpcb *tp, struct tcpiphdr *ti, 
	    tcp_seq ack, tcp_seq seq, int flags, bool_t swap)
{
    register int tlen;
    addr_t buf;
    struct tcpiphdr *tt;
    int win = 0;
    IO_Rec iov;
    int val,hs;
    u_int xsum;
    uint32_t nrecs; 

    TRC(eprintf("Entered tcp_respond(%p) flags = %s%s%s%s%s%s\n",
		tp,
		(flags & TH_URG) ? "U":"",
		(flags & TH_ACK) ? "A":"",
		(flags & TH_PUSH) ? "P":"",
		(flags & TH_RST) ? "R":"",
		(flags & TH_SYN) ? "S":"",
		(flags & TH_FIN) ? "F":""));

    hs = tp->conn_st.ipHdrLen;
    if (tp) {
	win =  MAX_BUF - BUFF_IN(tp->sock)->nbeltq;
    }

    if((buf = Heap$Malloc(Pvs(heap), sizeof(struct tcpiphdr))) == NULL)
    {
	fprintf(stderr, "tcp_respond: no space for malloc\n");
	return;
    }

    bzero (buf,sizeof(struct tcpiphdr));
    tt=buf;
    if(flags == 0) {
#ifdef TCP_COMPAT_42
#error Not doing TCP_COMPAT_42
#endif
	tlen = 0;
	
	bcopy((char *)ti, (char *)tt, sizeof(struct tcpiphdr));
	flags = TH_ACK;
    } else {
	tlen = 0;
	
	bcopy((char *)ti, (char *)tt, sizeof(struct tcpiphdr));
    }
    
    if(swap) {
	Net_IPHostAddr ipaddr, gatewayip;
	Net_EtherAddr mac;

	/* We need to swap the source and dest ports and set up the
           protocol stack */

	tt->ti_sport = ti->ti_dport;
	tt->ti_dport = ti->ti_sport;

	ipaddr.a = ti->ti_src.s_addr;

	/* XXX - we're trashing the old IP and Ether dest addresses - but
	   this shouldn't matter, since if they were set to anything
	   meaningful, the flowman RX filters should stop us receiving
	   anything from anyone else anyway, so we wouldn't need to
	   respond to anyone else */
	
	if(!tcp_route(tp, ipaddr.a, &gatewayip.a, &mac)) {
	    TRC(fprintf(stderr, "TCP: Failed to get MAC address for %I\n",
			ti->ti_src.s_addr));
	    return;
	}

	TRC(eprintf("tcp_respond: Setting dest eth address to %02x:%02x:%02x:%02x:%02x:%02x\n",
		    mac.a[0], mac.a[1], mac.a[2], mac.a[3], mac.a[4], mac.a[5]));  
	Ether$SetDest(tp->conn_st.eth, &mac);

	TRC(eprintf("tcp_respond: Setting dest ip address to %I\n", ipaddr.a));
	IP$SetDest(tp->conn_st.ip, &ipaddr);
    }
    tt->ti_len = ((u_short)(sizeof (struct tcphdr) + tlen));
    tt->ti_seq = htonl(seq);
    tt->ti_ack = htonl(ack);
    tt->ti_off = (sizeof (struct tcphdr) >> 2) << 4;
    tt->ti_flags = flags;
    tt->ti_win = htons((u_short)win);
    tt->ti_urp = 0;
    tt->ti_sum = 0;
    
    xsum = TCP_xsum_init(tt, 0);
    tt->ti_len = htons((u_short)(sizeof (struct tcphdr) + tlen));
    tt->ti_sum = htons( ((short16) TCP_xsum_final(xsum) ));
    
    iov.base=(char *)buf;
    iov.len  = sizeof(struct tcpiphdr);

    TRC({
	struct tcpiphdr *hdr = iov.base;
	eprintf("TCP: Sending packet: destination = %I:%04x\n",
		hdr->ti_dst.s_addr, ntohs(hdr->ti_dport));
    });
    
    IO$PutPkt(tp->conn_st.txio, 1, &iov, 0, FOREVER);
    IO$GetPkt(tp->conn_st.txio, 1, &iov, FOREVER, &nrecs, &val);
    FREE(buf);

}

/*
 * Close a TCP control block:
 *	discard all space held by the tcp
 *	discard internet protocol block
 *	wake up any sleepers
 */

void tcpu_close(struct tcpcb *tp) {
    register struct tcpiphdr *ti;
    struct tcpiphdr *prev;
    
    /* free the reassembly queue, if any */
    ti = tp->seg_next;
    while (ti != (struct tcpiphdr *)tp) 
	{
	    ti = (struct tcpiphdr *)ti->ti_next;
            prev=(ti->ti_prev);
            /***
	    remque(ti->ti_prev);
	    free(ti->ti_prev);
            ****/
	    remque(prev);
	    FREE(prev);
	}
    
    tp->tcpstat.tcps_closed++;

}


/*
 * Drop a TCP connection, reporting
 * the specified error.  If connection is synchronized,
 * then send a RST to peer.
 */

void tcp_drop(struct tcpcb *tp, int errno) {
    if (TCPS_HAVERCVDSYN(tp->t_state)) 
	{
	    tp->t_state = TCPS_CLOSED;
	    tcpu_output(&tp);
	    tp->tcpstat.tcps_drops++;
	}
    else
	tp->tcpstat.tcps_conndrops++;
    if (errno == ETIMEDOUT && tp->t_softerror)
	errno = tp->t_softerror;
    (void)tcpu_close(tp);
}

void free_tcpcb(sockfd)
    struct socket *sockfd;
{
    free(GET_TCPCB(sockfd));
}


bool_t matches_hostport(struct tcpiphdr *hdr, struct tcpcb *tp) {

    struct sockaddr_in *inaddr = (struct sockaddr_in *) &tp->sock->raddr;
    
    TRC(eprintf("hdr = %I:%u\n", hdr->ti_src.s_addr, (ntohs(hdr->ti_sport))));
    TRC(eprintf("tp(%p) = %I:%u\n", tp, inaddr->sin_addr.s_addr, ntohs(inaddr->sin_port)));

    if((inaddr->sin_addr.s_addr != hdr->ti_src.s_addr) ||
       (inaddr->sin_port != hdr->ti_sport)) {
	TRC(eprintf("Doesn't match!\n"));
	return False;
    } else {
	TRC(eprintf("Matches!\n"));
	return True;
    }
}

bool_t tcp_route(struct tcpcb *tp, uint32_t realip, 
		 uint32_t *useip, Net_EtherAddr *destmac) {
    
    FlowMan_SAP sap;
    FlowMan_RouteInfo * NOCLOBBER ri = NULL;
    
    sap.tag = FlowMan_PT_TCP;
    sap.u.TCP.addr.a = realip;
    sap.u.TCP.port = 0;

    TRY {
	ri = FlowMan$ActiveRoute(tp->fman, &sap);
    } CATCH_FlowMan$NoRoute() {
	printf("TCP: No route to host %I\n", realip);
    } ENDTRY;
    
    if(!ri) {
	return False;
    }

    TRC(eprintf("tcp_route: Routed %I via %I\n", realip, ri->gateway));

    if(ri->gateway) {
	*useip = ri->gateway;
    } else {
	*useip = realip;
    }

    FREE(ri->ifname);
    FREE(ri->type);
    FREE(ri);

    TRY {
	FlowMan$ARP(tp->fman, *useip, destmac);
    } CATCH_ALL {
	return False;
    } ENDTRY;

    return True;
}
    

    
