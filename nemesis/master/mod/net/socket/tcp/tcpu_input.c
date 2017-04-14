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
*  File              : tcpu_input.c                                        *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

/*	TCP_input.c	GIPSI	6.1	86/04/09
 *	from SMI 1.1 85/05/30 ; from UCB 6.2 83/11/04
 */
#include <nemesis.h>
#include <stdio.h>
#include <links.h>
#include <time.h>
#include <netdb.h>
#include <FlowMan.ih>
#include "inet.h"
#include <iana.h>
#include <IO.ih>
#include <IOMacros.h>
#include <IDCMacros.h>

#define FSHIFT 11 /* from sys/param.h */

#define panic printf 
#define THRE_ACK 40000

#if 0
int     tcprexmtthresh = 3;
int	tcpprintfs = 1;		     /* cf. tcp_timer.c SMX */
static	int	tcpcksum = 1;
static	int	tcpnodelack = 0;     /* etait extern, cf. tcp_timer.c SMX */
int     test_win;
#endif
struct	tcpcb *tcp_newtcpcb();

/*
 * TCP input routine, follows pages 65-76 of the
 * protocol specification dated September, 1981 very closely.
 */
     
void tcpu_input(struct tcpcb **tp_in,int len) {
    struct tcpcb *tp;
    register struct tcpiphdr  *tq, *tcpip;  
    register int tiflags;
    int off;
    int todrop, acked, datainq, nb_old, ourfinisacked;
    int needoutput = 0;
    int dropsocket = 0; 
    int datasize;
    char *buf;
    struct socket *socket;
    u_int xsum, xsum1;
    short16 r_xsum;        
    struct buffout *buff_out_ptr;
    
    tq=0;
    tp = *tp_in;
    socket=tp->sock;
    
    TRC(eprintf("tcpu_input entered\n"));

    buff_out_ptr = BUFF_OUT(socket);
    tcpip = (struct tcpiphdr *)(DATA_PT(socket))->header;
	
    tcpip->ti_len = len;

    r_xsum=tcpip->ti_sum;
    tcpip->ti_sum=0;
    xsum = TCP_xsum_init(tcpip, 0);

    datasize = len - ((tcpip->ti_off >> 4) << 2);

    TRC(eprintf("tcp: datasize = %u\n", datasize));

    if (datasize > 0) {
	xsum += in_cksum(DATA_PT(socket)->pointer, datasize);
    }

    xsum1 = htons (TCP_xsum_final(xsum));
/**
    printf (" checked checksum = %x \n",xsum1);
***/
    
    tcpip->ti_sum=r_xsum;

    if (tcpip->ti_sum != (short16) xsum1) {
	eprintf("received checksum : %x expected checksum : %x tcpip length : %u\n",
	       tcpip->ti_sum, (short16) xsum1, len);
        eprintf ("Dropping packet\n");
        goto drop;
    }
    
    tcpip->ti_next = 0;
    tcpip->ti_prev = 0;
    tcpip->ti_x1 = 0;

    /*
     * Check that TCP offset makes sense,
     * pull out TCP options and adjust length.
     */
    off = tcpip->ti_off >> 2; /* shift right 4 bits, and then multiply by 4. In 
                                 total shift right 2 bits. */
    
    if (off < sizeof (struct tcphdr) || off > len) 
    {
	eprintf("tcpu_input: incoherence in off !\n");	
	tp->tcpstat.tcps_badoff++;
	goto drop;
    }
    
    tiflags = tcpip->ti_flags; 
    
    
    /*
     * Convert TCP protocol specific fields to host format.
     */

    tcpip->ti_seq = ntohl(tcpip->ti_seq);
    tcpip->ti_ack = ntohl(tcpip->ti_ack);
    tcpip->ti_win = ntohs(tcpip->ti_win);
    tcpip->ti_urp = ntohs(tcpip->ti_urp);

    TRC(eprintf("tcp: seq = %04x, ack = %04x, flags = %s%s%s%s%s%s\n",
	   tcpip->ti_seq, tcpip->ti_ack,
	   (tiflags & TH_URG) ? "U":"",
	   (tiflags & TH_ACK) ? "A":"",
	   (tiflags & TH_PUSH) ? "P":"",
	   (tiflags & TH_RST) ? "R":"",
	   (tiflags & TH_SYN) ? "S":"",
	   (tiflags & TH_FIN) ? "F":""));
	   
         
  /*
   * Segment received on connection.
   * Reset idle time and keep-alive timer.
   */
    tp->t_idle = 0;
    tp->t_timer[TCPT_KEEP] = TCPTV_KEEP;
    
/*
 * Header prediction: check for the two common cases
 * of a uni-directional data xfer. If the packet has
 * no control flags, is in-sequence, the window didn't
 * change and we're not retramsnittting, it's a
 * canditate. If the length is zero and the ack moved
 * forward, we're the sender side of the xfer. Just
 * free the data acked & wake any higher level process
 * that was blocked waitung for space. If the length
 * is non-zero and the ack didn't move, we're the
 * receiver side. If we're getting packets in-order
 * (the reassembly queu is empty), add the data to
 * the in buffer and note that we need a delayed ack.
 */
    if(tp->t_state == TCPS_ESTABLISHED &&
       (tiflags & (TH_SYN|TH_FIN|TH_RST|TH_URG|TH_ACK)) == TH_ACK &&
       tcpip->ti_seq == tp->rcv_nxt &&
       tcpip->ti_win && tcpip->ti_win == tp->snd_wnd &&
       tp->snd_nxt == tp->snd_max) {
	if (datasize == 0) {
	    if(SEQ_GT(tcpip->ti_ack, tp->snd_una) &&
	       SEQ_LEQ(tcpip->ti_ack, tp->snd_max) &&
	       tp->snd_cwnd >= tp->snd_wnd) {
		/*
		 * This is a pure ack for outstanding data.
		 */
		
		TRC(eprintf("tcp: received pure ack\n"));
		if(tp->t_rtt && SEQ_GT(tcpip->ti_ack, tp->t_rtseq)){
		    tp->time_send = 0;
		    tcp_xmit_timer(tp);
		}
		acked = tcpip->ti_ack - tp->snd_una;
		buff_out_ptr->nbeltq -=acked;
		buff_out_ptr->nbeltsnd -= acked;
		buff_out_ptr->una = (buff_out_ptr->una + acked)%MAX_BUF;
		tp->snd_una = tcpip->ti_ack;
		/*
		 * If all outstanding data are acked, stop
		 * retransmit timer, otherwise restart timer
		 * using current (possibly backed-off) value.
		 * If data are ready to send, let tcp_output
		 * decide between more output or persist.
		 */
		if(tp->snd_una == tp->snd_max)
		    tp->t_timer[TCPT_REXMT] = 0;
		else if (tp->t_timer[TCPT_PERSIST] == 0)
		    tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
		return; 
	    }
	} else if ((tcpip->ti_ack == tp->snd_una) &&  
		   (tp->seg_next == (struct tcpiphdr *)tp) &&
		   (datasize > 0)) {
	    /*
	     * This is a pure, in-sequence data packet
	     * with nothing on the reassembly queue and
	     * we have enough buffer space to take it.
	     */

	    TRC(eprintf("tcp: Got pure data packet\n"));
	    tp->rcv_nxt += datasize;
	    /*
	     * Drop TCP and IP headers the add data
	     * to in buffer.
	     */
	    DATA_PT(socket)->length = datasize;
	    tp->t_flags |= TF_DELACK;
	    tp->time_ack = 1;
	    return;
	}
    }
    
    /*
     * Calculate amount of space in receive window,
     * and then do TCP input processing.
     */

    if(tp->t_state != TCPS_LISTEN) {
	
	if(!matches_hostport(tcpip, tp)) {
	    
	    eprintf("Received packet from wrong address!\n");

	    if(tiflags & TH_SYN) {
		tcp_rx_block *r;
		eprintf("Packet is SYN - saving for later!\n");
		
		r = Heap$Malloc(Pvs(heap), sizeof(*r) +
				TCP_MAX_SIZE);

		memcpy(((addr_t)(r+1)), tcpip, TCP_MAX_SIZE);

		LINK_ADD_TO_TAIL(&tp->syn_rx_blocks, r);

		goto drop;

	    } else {

		eprintf("Dropping unknown packet\n");
		goto dropwithreset;

	    }
	}
    }

    if(tp->t_state >= TCPS_ESTABLISHED) 
	{
	    tq = (struct tcpiphdr *)Heap$Malloc(Pvs(heap),TCP_MAX_SIZE);
	    if(tq == NULL)
		{
		    eprintf("tcpu_input : malloc failed\n");
		    RAISE_Heap$NoMemory();
		}
	    buf = (char *)tq;

	    memcpy(buf,DATA_PT(tp->sock)->header, 
		   sizeof(struct tcpiphdr) + datasize);
/*
            memcpy(buf,DATA_PT(tp->sock)->header,tcpip_size);
            memcpy(buf,tcpip,tcpip_size+sizeof(struct ipovly));
	    memcpy(buf + tcpip_size+sizeof(struct ipovly), 
                   DATA_PT(tp->sock)->pointer,len - tcpip_size);
		   */
	    tcpip = (struct tcpiphdr *)buf;
	}
    
    {
	int win;
	
	win = MAX_BUF - BUFF_IN(socket)->nbeltq;
	if (win < 0)
	    win = 0;
	if(win < (int)(tp->rcv_adv - tp->rcv_nxt))
	    tp->rcv_wnd = (int)(tp->rcv_adv - tp->rcv_nxt);
	else
	    tp->rcv_wnd = win;
    }

    switch (tp->t_state) 
	{
	    
	    /*
	     * If the state is LISTEN then ignore segment if it contains an RST.
	     * If the segment contains an ACK then it is bad and send a RST.
	     * If it does not contain a SYN then it is not interesting; drop it.
	     * Otherwise initialize tp->rcv_nxt, and tp->irs, select an initial
	     * tp->iss, and send a segment:
	     *     <SEQ=ISS><ACK=RCV_NXT><CTL=SYN,ACK>
	     * Also initialize tp->snd_nxt to tp->iss+1 and tp->snd_una to tp->iss.
	     * Fill in remote peer address fields if not previously specified.
	     * Enter SYN_RECEIVED state, and process any other fields of this
	     * segment in this state.
	     */
	case TCPS_LISTEN: 
	{
	    if (tiflags & TH_RST)
		goto drop;
	    if (tiflags & TH_ACK) {
		TRC(eprintf("tcp %p: received ACK in state LISTEN - sending RST\n", tp));
		goto dropwithreset;
	    }

	    if ((tiflags & TH_SYN) == 0)
		goto drop;
	    
            {
		Net_IPHostAddr gatewayip, ipaddr;
		Net_EtherAddr hwaddr;
		struct sockaddr_in *inaddr = 
		    (struct sockaddr_in *)&tp->sock->raddr;

		ipaddr.a = tcpip->ti_src.s_addr;
		if(!tcp_route(tp, ipaddr.a, &gatewayip.a, &hwaddr)) {
		    TRC(fprintf(stderr, 
				"TCP: Failed to get MAC address for %I\n", 
				ipaddr.a));
		    goto drop;
		}
		
		Ether$SetDest(tp->conn_st.eth, &hwaddr);
		TRC(eprintf("tcpu_input: Setting dest ip address to %I\n", 
			    ipaddr.a));
		IP$SetDest(tp->conn_st.ip,&ipaddr);
		
		inaddr->sin_family = AF_INET;
		inaddr->sin_port = tcpip->ti_sport;
		inaddr->sin_addr.s_addr = ipaddr.a;

		tp->t_template->ti_dst.s_addr=ipaddr.a;
		tp->t_template->ti_dport=tcpip->ti_sport;
		
		TRC(eprintf("Set template ports to %u, %u\n", 
			    tp->t_template->ti_sport,
			    tp->t_template->ti_dport));
		
		
            }
	    
	    tp->iss  = NOW(); 
	    tp->irs  = tcpip->ti_seq;
	    tcpu_sendseqinit(tp);
	    tcpu_rcvseqinit(tp);
	    
	    tp->t_flags |= TF_ACKNOW;
	    tp->t_state = TCPS_SYN_RECEIVED;
	    tp->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT;
	    dropsocket = 0;		/* committed to socket */
	    goto trimthenstep6;
	}
	
	/*
	 * If the state is SYN_SENT:
	 * if seg contains an ACK, but not for our SYN, drop the input.
	 * if seg contains a RST, then drop the connection.
	 * if seg does not contain SYN, then drop it.
	 * Otherwise this is an acceptable SYN segment
	 * initialize tp->rcv_nxt and tp->irs
	 * if seg contains ack then advance tp->snd_una
	 * if SYN has been acked change to ESTABLISHED else SYN_RCVD state
	 * arrange for segment to be acked (eventually)
	 * continue processing rest of data/controls, beginning with URG
	 */
	case TCPS_SYN_SENT:
	    if ((tiflags & TH_ACK) &&
		/* this should be SEQ_LT; is SEQ_LEQ for BBN vax TCP only */
		(SEQ_LT(tcpip->ti_ack, tp->iss) ||
		 SEQ_GT(tcpip->ti_ack, tp->snd_max))) {
		TRC(eprintf("Received dodgy sequence - sending RST\n"));
		goto dropwithreset;
            }
	    if (tiflags & TH_RST)
		{
		    if (tiflags & TH_ACK) {
			tcp_drop(tp, ECONNREFUSED);
                        tp->t_state = TCPS_CLOSED;
                    }
		    goto drop;
		}
	    if ((tiflags & TH_SYN) == 0) {
		goto drop;
            }
	    if(tiflags & TH_ACK)
		{
		    tp->snd_una = tcpip->ti_ack;
		    if (SEQ_LT(tp->snd_nxt, tp->snd_una))
			tp->snd_nxt = tp->snd_una;
		}
	    tp->t_timer[TCPT_REXMT] = 0;
	    tp->irs = tcpip->ti_seq;
	    tcpu_rcvseqinit(tp);
	    tp->t_flags |= TF_ACKNOW;
	    
	    if (tiflags & TH_ACK && SEQ_GT(tp->snd_una, tp->iss))
		{
		    tp->t_state = TCPS_ESTABLISHED;
		    tcp_reass(tp, (struct tcpiphdr *)0);
		    /*
		     * If we didn't have to retramsnit the SYN,
		     * use its rtt as our initial srtt & rtt var.
		     */
		    if(tp->t_rtt)
			{
			    tp->t_srtt = tp->t_rtt << 3;
			    tp->t_rttvar = tp->t_rtt << 1;
			    TCPT_RANGESET(tp->t_rxtcur,
					  ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1,
					  TCPTV_MIN, TCPTV_REXMTMAX);
			    tp->t_rtt = 0;
			}
		}
	    else
		{
		    tp->t_state = TCPS_SYN_RECEIVED;
		}
	    
	  trimthenstep6:
	    /*
	     * Advance ti->ti_seq to correspond to first data byte.
	     * If data, trim to stay within window,
	     * dropping FIN if necessary.
	     */
	    tcpip->ti_seq++;
	    if (datasize > tp->rcv_wnd)
		{
		    len = tp->rcv_wnd;
		    datasize = len - ((tcpip->ti_off >> 4) << 2);
		    tcpip->ti_flags &= ~TH_FIN;
		}
	    tp->snd_wl1 = tcpip->ti_seq - 1;
	    goto step6;
	}/* end switch(tp->state) */	
    
    /*
     * States other than LISTEN or SYN_SENT.
     * First check that at least some bytes of segment are within 
     * receive window.
     */
    
    todrop = tp->rcv_nxt - tcpip->ti_seq;
    
    if(todrop > 0)
	{
	    if (tiflags & TH_SYN)
		{
		    tiflags &= ~TH_SYN;
		    tcpip->ti_seq++;
		    if (tcpip->ti_urp > 1)
			tcpip->ti_urp--;
		    else
			tiflags &= ~TH_URG;
		    todrop--;
		}
	    if(todrop > datasize ||
	       ((todrop == datasize) && ((tiflags & TH_FIN) == 0)) )
		{
		    /*
		     * if segment is just one to the left of the window,
		     * check two special cases:
		     * 1. Don't toss RST in response to 4.2-style keepalive.
		     * 2. If the only thing to drop is FIN, we can drop
		     *    it, but check the ACK or we will get into FIN
		     *    wars if our FINS crossed (both CLOSING).
		     * In either case, send ACK to resynchronize,
		     * but keep on processing for RST or ACK.
		     */
		    if ((tiflags & TH_FIN && todrop == datasize +1)
#ifdef TCP_COMPAT_42
			|| (tiflags & TH_RST && tcpip_ti_seq == tp->rcv_nxt -1)
#endif
			)
			{
			    todrop = datasize + 1;
			    tiflags &= ~TH_FIN;
			    tp->t_flags |= TF_ACKNOW;
			}
		    else
			goto dropafterack;
		}
	    else
		{
		    tp->tcpstat.tcps_rcvpartduppack++;
		    tp->tcpstat.tcps_rcvpartdupbyte += todrop;
		}
	    len -= todrop;
	    datasize = len - ((tcpip->ti_off >> 4) << 2);
	    
	    tcpip->ti_seq += todrop;
	    
	    bcopy(((char *)(tcpip + 1)) + todrop, 
		  ((char *)(tcpip + 1)), len); 

	    /* modified 14.3.95*/

	    if(tcpip->ti_urp > todrop) {
		tcpip->ti_urp -= todrop;
	    } else {
		tiflags &= ~ TH_URG;
		tcpip->ti_urp = 0;
	    }
	}
    
    /* 
     * If new data are received on a connection after the
     * user processes are gone, then RST the other end.
     */
    if (tp->t_state > TCPS_CLOSE_WAIT && datasize)
	{
	    tcpu_close(tp);
	    tp->tcpstat.tcps_rcvafterclose++;
	    TRC(eprintf("Received data after close: sending RST\n"));
	    goto dropwithreset;
	}

    /*
     * If segment ends after window, drop trailing data
     * (and PUSH and FIN); if nothing left, just ACK.
     */

    todrop = tcpip->ti_seq + datasize - (tp->rcv_nxt + tp->rcv_wnd);

    if(todrop > 0) {
	tp->tcpstat.tcps_rcvpackafterwin++;
	if(todrop >= datasize) {
	    tp->tcpstat.tcps_rcvbyteafterwin += datasize;
	    /*
	     * If window is closed can only take segments at
	     * window edge, and have to drop data and PUSH from
	     * incoming segments. Continue processing, but
	     * remember to ack. otherwise, drop segment and ack.
	     */
	    if ((tp->rcv_wnd == 0) && (tcpip->ti_seq == tp->rcv_nxt)) {
		tp->t_flags |= TF_ACKNOW;
		tp->tcpstat.tcps_rcvwinprobe++;
	    } else {
		goto dropafterack;
	    }
	} else {
	    tp->tcpstat.tcps_rcvbyteafterwin += todrop;
	}
	
	len -= todrop;
	datasize = len - ((tcpip->ti_off >> 4) << 2);
	
	tiflags &= ~(TH_PUSH|TH_FIN);
    }


  /*
   * If the RST bit is set examine the state:
   *    SYN_RECEIVED STATE:
   *	If passive open, return to LISTEN state.
   *	If active open, inform user that connection was refused.
   *    ESTABLISHED, FIN_WAIT_1, FIN_WAIT2, CLOSE_WAIT STATES:
   *	Inform user that connection was reset, and close tcb.
   *    CLOSING, LAST_ACK, TIME_WAIT STATES
   *	Close the tcb.
   */

    if (tiflags & TH_RST) {
	switch (tp->t_state) {
	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	    tp->t_state = TCPS_CLOSED;
	    tp->tcpstat.tcps_drops++;
	    tcpu_close(tp);
	    goto drop;
	    
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
	    tcpu_close(tp);
	    goto drop;
	}
    }
    
    
    
    /*
     * If a SYN is in the window, then this is an
     * error and we send an RST and drop the connection.
     */
    if (tiflags & TH_SYN)
	{
	    /*	tcp_drop(tp, ECONNRESET);
		goto dropwithreset;*/
	    goto drop ;
	}
    
    /*
     * If the ACK bit is off we drop the segment and return.
     */
    if ((tiflags & TH_ACK) == 0)
	goto drop;
    
    /*
     * Ack processing.
     */
    switch (tp->t_state) 
	{
	    /*
	     * In SYN_RECEIVED state if the ack ACKs our SYN then enter
	     * ESTABLISHED state and continue processing, othewise
	     * send an RST.
	     */
	case TCPS_SYN_RECEIVED:
	    if (SEQ_GT(tp->snd_una, tcpip->ti_ack) ||
		SEQ_GT(tcpip->ti_ack, tp->snd_max)) {
		TRC(eprintf("Got wrong ACK for our SYN - sending RST\n"));
		goto dropwithreset;
	    }
	    tp->t_state = TCPS_ESTABLISHED;
	    (void) tcp_reass(tp, (struct tcpiphdr *)0);
	    tp->snd_wl1 = tcpip->ti_seq - 1;
	    /* fall into ... */
	    
	    /*
	     * In ESTABLISHED state: drop duplicate ACKs; ACK out of range
	     * ACKs.  If the ack is in the range
	     *	tp->snd_una < ti->ti_ack <= tp->snd_max
	     * then advance tp->snd_una to ti->ti_ack and drop
	     * data from the retransmission queue.  If this ACK reflects
	     * more up to date window information we update our window information.
	     */
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_FIN_WAIT_2:
	case TCPS_CLOSE_WAIT:
	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
	    if (SEQ_LEQ(tcpip->ti_ack, tp->snd_una))
		{
		    if (datasize == 0 && tcpip->ti_win == tp->snd_wnd)
			{
			    tp->tcpstat.tcps_rcvdupack++;
			    if (tp->t_timer[TCPT_REXMT] == 0 ||
				tcpip->ti_ack != tp->snd_una)
				tp->t_dupacks = 0;
			    else if (++tp->t_dupacks == tp->tcprexmtthresh)
				{
				    tcp_seq onxt = tp->snd_nxt;
				    u_int win = 
					MIN(tp->snd_wnd, tp->snd_cwnd)/2/tp->t_maxseg;
				    
				    if (win < 2)
					win = 2;
				    tp->snd_ssthresh = win * tp->t_maxseg;
				    tp->t_timer[TCPT_REXMT] = 0;
				    tp->t_rtt = 0;
				    tp->snd_nxt = tcpip->ti_ack;
				    tp->snd_cwnd = tp->t_maxseg;
				    nb_old = buff_out_ptr->nbeltsnd ;
				    buff_out_ptr->nbeltsnd = 0;
				    (void) tcpu_output(&tp);
				    tp->snd_cwnd = tp->snd_ssthresh +
					tp->t_maxseg * tp->t_dupacks;
				    if (SEQ_GT(onxt, tp->snd_nxt))
					{
					    tp->snd_nxt = onxt;
					    buff_out_ptr->nbeltsnd = nb_old ;
					}
				    goto drop;
				} 
			} 
		    else
			tp->t_dupacks = 0;
		    break;
		}
	    tp->t_dupacks = 0;
	    if (SEQ_GT(tcpip->ti_ack, tp->snd_max))
		goto dropafterack;
	    
	    acked = tcpip->ti_ack - tp->snd_una ;
	    /*
	     * If transmit timer is running and timed sequence
	     * number was acked, update smoothed round trip time.
	     */
	    if (tp->t_rtt && SEQ_GT(tcpip->ti_ack, tp->t_rtseq))
		{
		    tp->time.tv_sec = 0;
		    tp->time.tv_usec = 0;
		    tcp_xmit_timer(tp);
		}
	    
	    /*
	     * If all outstanding data is acked, stop retransmit
	     * timer and remember to restart (more output or persist).
	     * If there is more data to be acked, restart retransmit
	     * timer, using current (possibly backed-off) value.
	     */
	    if (tcpip->ti_ack == tp->snd_max)        
		{
		    tp->t_timer[TCPT_REXMT] = 0;
		    needoutput = 1;
		}
	    else if (tp->t_timer[TCPT_PERSIST] == 0)
		tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
	    /*
	     * When new data is acked, open the congestion window.
	     * If the window gives us less than ssthrech packets
	     * in flight, open exponentially (maxseq per packet).
	     * Otherwisw open linearly (maxseq per packet).
	     * or maxseq^2 / cwnd per packet).
	     */
	    {
		int incr = tp->t_maxseg;
		
		if(tp->snd_cwnd > tp->snd_ssthresh)
		    incr = incr*incr/tp->snd_cwnd + incr/8 ;
		tp->snd_cwnd = MIN(tp->snd_cwnd + incr, TCP_MAXWIN);
	  }
	    
	    datainq = buff_out_ptr->nbeltsnd ; 	   
  
	    if (acked > datainq)
		{
		    buff_out_ptr->una = (buff_out_ptr->una+acked)%MAX_BUF;
		    buff_out_ptr->nbeltq -= acked ;
		    if (buff_out_ptr->nbeltq < 0) buff_out_ptr->nbeltq = 0;
				/* modified 15.3.95 */
		    tp->snd_wnd -= datainq;
		    buff_out_ptr->nbeltsnd = 0 ;
		    ourfinisacked = 1;
		} 
	    else
		{
		    buff_out_ptr->una = (buff_out_ptr->una+acked)%MAX_BUF;
		    buff_out_ptr->nbeltq -= acked ;
		    buff_out_ptr->nbeltsnd -= acked ;
		    tp->snd_wnd -= acked;
		    ourfinisacked = 0;
		}
	    tp->snd_una = tcpip->ti_ack;
	    
	    if (SEQ_LT(tp->snd_nxt, tp->snd_una))
		{
		    buff_out_ptr->nbeltsnd = 0;
		    tp->snd_nxt = tp->snd_una;
		}
	    
	    switch (tp->t_state)
		{
		    /*
		     * In FIN_WAIT_1 STATE in addition to the processing
		     * for the ESTABLISHED state if our FIN is now acknowledged
		     * then enter FIN_WAIT_2.
		     */
		case TCPS_FIN_WAIT_1:
		    if (ourfinisacked)
			{
			    /*
			     * If we can't receive any more
			     * data, then closing user can proceed.
			     */
			  tp->t_timer[TCPT_2MSL] = tp->tcp_maxidle;
			  tp->t_state = TCPS_FIN_WAIT_2;
			}
		  break;
		    
		    /*
		     * In CLOSING STATE in addition to the processing for
		     * the ESTABLISHED state if the ACK acknowledges our FIN
		     * then enter the TIME-WAIT state, otherwise ignore
		     * the segment.
		     */
		case TCPS_CLOSING:
		    if (ourfinisacked)
			{
			    tp->t_state = TCPS_TIME_WAIT;
			    tcp_canceltimers(tp);
			    /* modifie par GIPSI avant 2 * TCPTV_MSL */
			    tp->t_timer[TCPT_2MSL] = TCPTV_MSL;
			}
		  break;
		    
		    /*
		     * The only thing that can arrive in  LAST_ACK state
		     * is an acknowledgment of our FIN.  If our FIN is now
		     * acknowledged, delete the TCB, enter the closed state
		     * and return.
		     */
		case TCPS_LAST_ACK:
		    if (ourfinisacked)
			{
			    tcpu_close(tp);
			    goto drop;
			}
		    break;
		    /*
		     * In TIME_WAIT state the only thing that should arrive
		     * is a retransmission of the remote FIN.  Acknowledge
		     * it and restart the finack timer.
		     */
		case TCPS_TIME_WAIT:
		    /* modifie par GIPSI idem que precedemment */
		    tp->t_timer[TCPT_2MSL] = TCPTV_MSL;
		    goto dropafterack;
		}
	}
    
  step6:

    /*
     * Update window information.
     */
    if ((tiflags & TH_ACK) && 
	(SEQ_LT(tp->snd_wl1, tcpip->ti_seq) || tp->snd_wl1 == tcpip->ti_seq &&
	 (SEQ_LT(tp->snd_wl2, tcpip->ti_ack) ||
	  tp->snd_wl2 == tcpip->ti_ack && tcpip->ti_win > tp->snd_wnd)))
	{
	    tp->snd_wnd = tcpip->ti_win;
	    tp->snd_wl1 = tcpip->ti_seq;
	    tp->snd_wl2 = tcpip->ti_ack;
	    if (tp->snd_wnd > tp->max_sndwnd)
		tp->max_sndwnd = tp->snd_wnd;
	    needoutput = 1;
	}     
    
    /*
     * Process the segment text, merging it into the TCP sequencing queue,
     * and arranging for acknowledgment of receipt if necessary.
     * This process logically involves adjusting tp->rcv_wnd as data
     * is presented to the user (this happens in tcp_usrreq.c,
     * case PRU_RCVD).  If a FIN has already been received on this
     * connection then we just ignore the text.
     */
    if (((datasize > 0) || (tiflags&TH_FIN)) &&
	TCPS_HAVERCVDFIN(tp->t_state) == 0) {
	if(tcpip->ti_seq == tp->rcv_nxt &&
	   tp->seg_next == (struct tcpiphdr *)tp &&
	   tp->t_state == TCPS_ESTABLISHED)
	{
	    tp->rcv_nxt += datasize;
	    tp->tcpstat.tcps_rcvpack++;
	    tp->tcpstat.tcps_rcvbyte = datasize;
	    tiflags = tcpip->ti_flags & TH_FIN;
	    tp->t_flags |= TF_DELACK; 
	    
	    DATA_PT(socket)->length += datasize;		  
	    BUFF_IN(socket)->next = 
		(BUFF_IN(socket)->next + datasize)%MAX_BUF;
	    BUFF_IN(socket)->nbeltq += datasize;
	} else {
	    tq->ti_next = tq->ti_prev = 0;
	    tq->ti_x1 = (u_char)0;
	    tq->TI_LEN = datasize;
	    if ( (tq->ti_seq+tq->TI_LEN -tp->rcv_nxt) > MAX_BUF) {
		eprintf ("DROPPED PACKET\n");
		tp->t_flags |= TF_ACKNOW;
		goto drop; /* BY RS: drop packet if it will not fit in
			      the reassembly buffer */
	    }
	    if (tq->TI_LEN > 0) { /* torsten 27.7.95 */
		tiflags = tcp_reass(tp, tq);
	    }
	    tp->t_flags |= TF_ACKNOW;
	}
    } else {
	tiflags &= ~TH_FIN;
	if (tq) {
	    Heap$Free(Pvs(heap),tq);
	}
    }
  
    /*
     * If FIN is received ACK the FIN and let the user know
     * that the connection is closing.
     */
    if (tiflags & TH_FIN) {
	if (TCPS_HAVERCVDFIN(tp->t_state) == 0)
	{
	    tp->t_flags |= TF_ACKNOW;
	    tp->rcv_nxt++;
	}
	switch (tp->t_state)
	{
	    
	    /*
	     * In SYN_RECEIVED and ESTABLISHED STATES
	     * enter the CLOSE_WAIT state.
	     */
	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
	    tp->t_state = TCPS_CLOSE_WAIT;
	    break;
	    
	    /*
	     * If still in FIN_WAIT_1 STATE FIN has not been acked so
	     * enter the CLOSING state.
	     */
	case TCPS_FIN_WAIT_1:
	    tp->t_state = TCPS_CLOSING;
	    break;
	    
	    /*
	     * In FIN_WAIT_2 state enter the TIME_WAIT state,
	     * starting the time-wait timer, turning off the other 
	     * standard timers.
	     */
	case TCPS_FIN_WAIT_2:
	    tp->t_state = TCPS_TIME_WAIT;
	    tcp_canceltimers(tp);
		    /* modifie par GIPSI idem que precedemment */
	    tp->t_timer[TCPT_2MSL] = TCPTV_MSL;
	    break;
	    
	    /*
	     * In TIME_WAIT state restart the 2 MSL time_wait timer.
	     */
	case TCPS_TIME_WAIT:
	    /* modifiee par GIPSI idem que precedemment */
	    tp->t_timer[TCPT_2MSL] = TCPTV_MSL;
		    break;
	}
    }
    
    /*
     * Return any desired output.
     */
    if (needoutput || (tp->t_flags & TF_ACKNOW)) {
	tcpu_output(&tp);
    }
    return;
    
dropafterack:
    /*
     * Generate an ACK dropping incoming segment if it occupies
     * sequence space, where the ACK reflects our state.
     */
    if (tiflags&TH_RST) 
	goto drop;
    
    if (tq) {
	FREE(tq);
    }
    tp->t_flags |= TF_ACKNOW;
    tcpu_output(&tp);
    return;
    
dropwithreset:
    /*
     * Generate a RST, dropping incoming segment.
     * Make ACK acceptable to originator of segment.
     */
    if (tiflags & TH_RST)
	goto drop;
    if (tiflags & TH_ACK) {
	tcp_respond(tp, tcpip, (tcp_seq)0, tcpip->ti_ack, TH_RST, True);
    } else {
	if (tiflags & TH_SYN) {
	    len++;
	    datasize = len - ((tcpip->ti_off >> 4) << 2);
	}
	tcp_respond(tp, tcpip,
		    tcpip->ti_seq + datasize,
		    (tcp_seq)0,
		    TH_RST|TH_ACK, True);
    }
    return;
    
drop:
    /*
     * Drop space held by incoming segment and return.
     */
    
    if (tq) {
	FREE(tq);
    }
    return;
}

/*
 * Insert segment ti into reassembly queue of tcp with
 * control block tp.  Return TH_FIN if reassembly now includes
 * a segment with FIN.
 */

int tcp_reass(struct tcpcb *tp, struct tcpiphdr *ti)
{
    register struct tcpiphdr *q, *tt, *p,**qp;
    int flags;

    /*
     * Call with ti==0 after become established to
     * force pre-ESTABLISHED data up to user socket.
     */

    if (ti == 0)
	goto present;
    /*
     * Find a segment which begins after this one does.
     */
    
    for (q = tp->seg_next; (q != (struct tcpiphdr *)tp) /*&& (q != 0)*/
			       ;q = (struct tcpiphdr *)q->ti_next) 
	{  
	    if (SEQ_GT(q->ti_seq, ti->ti_seq))
		break;
	}

    qp = &(q->ti_prev);
    
    /*
     * If there is a preceding segment, it may provide some of
     * our data already.  If so, drop the data from the incoming
     * segment.  If it provides all of our data, drop us.
     */

/**
    if ( ((struct tcpiphdr *)q->ti_prev != (struct tcpiphdr *)tp))
******/
    if ( ( *qp != (struct tcpiphdr *)tp))
	{
	    register int i;
  
            /***
	    q = (struct tcpiphdr *)q->ti_prev;
            ****/
            q=(struct tcpiphdr *)*qp;
	    /* conversion to int (in i) handles seq wraparound */

            i = q->TI_LEN + q->ti_seq - ti->ti_seq;
	    if (i > 0)
		{
		    if (i >= ti->TI_LEN)
			goto drop;
		    ti->TI_LEN -= i;
		    ti->ti_seq += i;
		    bcopy(((char *)ti) + sizeof(struct tcpiphdr) + i,
			  ((char *)ti) + sizeof(struct tcpiphdr), ti->TI_LEN);
		}
	    q = (struct tcpiphdr *)(q->ti_next);
            qp=&(q->ti_prev);
	}

    /*
     * While we overlap succeeding segments trim them or,
     * if they are completely covered, dequeue them.
     */
    while (q != (struct tcpiphdr *)tp)
	{
	    register int i = (ti->ti_seq + ti->TI_LEN) - q->ti_seq;
	    if (i <= 0)
		break;
	    if (i < q->TI_LEN)
		{
		    q->ti_seq += i;
		    q->TI_LEN -= i;
		    bcopy(((char *)q) + sizeof(struct tcpiphdr) + i, ((char *)q) +
			  sizeof(struct tcpiphdr), q->TI_LEN);
		    break;
		}
	    else
		{
		    remque(q);
		    Heap$Free(Pvs(heap),q); 
		}
	    q = (struct tcpiphdr *)q->ti_next; 
            qp=&(q->ti_prev);
	}
    
    /*
     * Stick new segment in its place.
     */
/*****
    insque(ti,q->ti_prev);
    insque(ti,*qp);
*****/
    insque(*qp,ti);

    for (p = tp->seg_next; (p != (struct tcpiphdr *)tp); 
			   p = (struct tcpiphdr *)p->ti_next);

  present:

    /*
     * Present data to user, advancing rcv_nxt through
     * completed sequence space.
   */
    if (TCPS_HAVERCVDSYN(tp->t_state) == 0)
	return (0);
    ti = tp->seg_next;
    if (ti == (struct tcpiphdr *)tp || ti->ti_seq != tp->rcv_nxt)
	return (0);
    if (tp->t_state == TCPS_SYN_RECEIVED && ti->TI_LEN)
	return (0);
    DATA_PT(tp->sock)->reass_pointer=DATA_PT(tp->sock)->reass_buffer;
    do {
	tp->rcv_nxt += ti->TI_LEN;
	flags = ti->ti_flags & TH_FIN;
	tt = (struct tcpiphdr *)ti->ti_next;
	remque(ti);
	append_queue(ti, tp);
	ti = tt;
    } while (ti != (struct tcpiphdr *)tp && ti->ti_seq == tp->rcv_nxt);
    DATA_PT(tp->sock)->pointer=DATA_PT(tp->sock)->reass_buffer;
    return (flags);
  drop:
    return (0);
}

append_queue(ti, tp)
    register struct tcpiphdr *ti;
    register struct tcpcb    *tp;
{
    int offset;
    struct socket *socket = tp->sock;

/* modified 6.3.95 */
# if 0
    memcpy(DATA_PT(socket)->pointer, 
	   ((char *)ti) + sizeof(struct tcpiphdr),
	   ti->TI_LEN); 
    
    BUFF_IN(socket)->next = (BUFF_IN(socket)->next +  ti->TI_LEN) % MAX_BUF;
    BUFF_IN(socket)->nbeltq += ti->TI_LEN;
    
/* modified 6.3.95 */
    DATA_PT(socket)->pointer += ti->TI_LEN;
    DATA_PT(socket)->length += ti->TI_LEN;
#endif
    memcpy(DATA_PT(socket)->reass_pointer, 
	   ((char *)ti) + sizeof(struct tcpiphdr),
	   ti->TI_LEN); 
    
    BUFF_IN(socket)->next = (BUFF_IN(socket)->next +  ti->TI_LEN) % MAX_BUF;
    BUFF_IN(socket)->nbeltq += ti->TI_LEN;
    
/* modified 6.3.95 */
    DATA_PT(socket)->reass_pointer += ti->TI_LEN;
    DATA_PT(socket)->length += ti->TI_LEN;
    Heap$Free(Pvs(heap), ti );

    return (0);
}



/*
 * Collect new round-trip time estimate
 * and update averages and current timeout.
 */
void tcp_xmit_timer(struct tcpcb *tp)
{
    register short delta;
    
    tp->tcpstat.tcps_rttupdated++;
    if (tp->t_srtt != 0)
	{
	    /*
	     * srtt is stored as fixed point with 3 bits after the
	     * binary point (i.e., scaled by 8).  The following magic
	     * is equivalent to the smoothing algorithm in rfc793 with
	     * an alpha of .875 (srtt = rtt/8 + srtt*7/8 in fixed
	     * point).  Adjust t_rtt to origin 0.
	     */
	    delta = tp->t_rtt - 1 - (tp->t_srtt >> TCP_RTT_SHIFT);
	    if ((tp->t_srtt += delta) <= 0)
		tp->t_srtt = 1;
	    /*
	     * We accumulate a smoothed rtt variance (actually, a
	     * smoothed mean difference), then set the retransmit
	     * timer to smoothed rtt + 4 times the smoothed variance.
	     * rttvar is stored as fixed point with 2 bits after the
	     * binary point (scaled by 4).  The following is
	     * equivalent to rfc793 smoothing with an alpha of .75
	     * (rttvar = rttvar*3/4 + |delta| / 4).  This replaces
	     * rfc793's wired-in beta.
	     */
	    if (delta < 0)
		delta = -delta;
	    delta -= (tp->t_rttvar >> TCP_RTTVAR_SHIFT);
	    if ((tp->t_rttvar += delta) <= 0)
		tp->t_rttvar = 1;
	}
    else
	{
	    /* 
	     * No rtt measurement yet - use the unsmoothed rtt.
	     * Set the variance to half the rtt (so our first
	     * retransmit happens at 2*rtt)
	     */
	    tp->t_srtt = tp->t_rtt << TCP_RTT_SHIFT;
	    tp->t_rttvar = tp->t_rtt << (TCP_RTTVAR_SHIFT - 1);
	}
    tp->t_rtt = 0;
    tp->t_rxtshift = 0;
    
    /*
     * the retransmit should happen at rtt + 4 * rttvar.
     * Because of the way we do the smoothing, srtt and rttvar
     * will each average +1/2 tick of bias.  When we compute
     * the retransmit timer, we want 1/2 tick of rounding and
     * 1 extra tick because of +-1/2 tick uncertainty in the
     * firing of the timer.  The bias will give us exactly the
     * 1.5 tick we need.  But, because the bias is
     * statistical, we have to test that we don't drop below
     * the minimum feasible timer (which is 2 ticks).
     */
    TCPT_RANGESET(tp->t_rxtcur, TCP_REXMTVAL(tp),
		  tp->t_rttmin, TCPTV_REXMTMAX);
    
    /*
     * We received an ack for a packet that wasn't retransmitted;
     * it is probably safe to discard any error indications we've
     * received recently.  This isn't quite right, but close enough
     * for now (a route might have failed after we sent a segment,
     * and the return path might not be symmetrical).
     */
    tp->t_softerror = 0;
}

void tcp_xmit_timer2(struct tcpcb *tp, struct tcpiphdr *tcpip) {
    /*
     * If transmit timer is running and timed sequence
     * number was acked, update smoothed round trip time.
     */
    if (tp->t_rtt && SEQ_GT(tcpip->ti_ack, tp->t_rtseq)) {
	if (tp->t_srtt == 0) {
	    tp->t_srtt = tp->t_rtt << FSHIFT;
	} else {
	    /*	tp->t_srtt =
		(tcp_alpha * tp->t_srtt +
		(FSCALE - tcp_alpha) * tp->t_rtt * FSCALE)
		>> FSHIFT;*/
	    tp->t_srtt =
		0.9* tp->t_srtt +
		0.1* tp->t_rtt ;
	}
	tp->t_rtt = 0;
    }
    
    if (tcpip->ti_ack == tp->snd_max) {
	tp->t_timer[TCPT_REXMT] = 0;
    } else {
	TCPT_RANGESET(tp->t_timer[TCPT_REXMT],
		      2*tp->t_srtt,
		      TCPTV_MIN, TCPTV_MAX);
	tp->t_rtt = 1;
	tp->t_rxtshift = 0;
	tp->t_rtseq = tp->snd_una ;/* ??? */
    }
}

void flush_rx_packet(struct tcpcb *tp) {

    IO_Rec iov;

    if (tp->iov_base) {
    /* in this case you have to feed the iov in the receive channel */
    /* At this point I'm sure I have finished processing the packet  previously
      received */
	if(tp->storedRXBlock) {
	    FREE(tp->storedRXBlock);
	    tp->storedRXBlock = NULL;
	} else {
	    iov.base = tp->iov_base;
	    iov.len  = tp->conn_st.ipHdrLen + TCP_MAX_SIZE;
	    
	    TRC(eprintf("tcp_flush_rx_packet: Putting packet %p down "
			"IO channel\n", iov.base));
	    IO$PutPkt(tp->conn_st.rxio, 1, &iov, 0, FOREVER);
	}
	tp->iov_base=NULL;
    }
}    

/* get_res returns amount of data read, including TCP headers */
int get_res(struct tcpcb *tp,struct timeval *timeout)
{
    Time_ns t;
    IO_Rec iov;
    uint32_t nrecs, value;
    int off;
    struct tcpiphdr *hdr;

  
    if ( tp->t_state == TCPS_CLOSED )
	return(0);

    flush_rx_packet(tp);

    if(!LINK_EMPTY(&tp->rx_blocks)) {
	tcp_rx_block *b = tp->rx_blocks.next;

	int len;

	hdr = (struct tcpiphdr *)tp->iov_base = ((addr_t)(b + 1));
	off = (hdr->ti_t.th_off >> 4) * 4;
	DATA_PT(tp->sock)->header = tp->iov_base = ((addr_t)(b+1));
	DATA_PT(tp->sock)->pointer=tp->iov_base + off + OFFSETOF(struct tcpiphdr, ti_t);	
	len = ntohs(hdr->ti_i.ip_len) - (tp->conn_st.ipHdrLen - 16); /* XXX discount ether header */

	tp->storedRXBlock = b;
	LINK_REMOVE(b);
	TRC(eprintf("Returning stored block %p, len %u\n", hdr, len));
	return len;
    } else {
	
        t=NOW() +
	    timeout->tv_sec * SECONDS(1) +
	    timeout->tv_usec * MICROSECS(1);

	IO$GetPkt(tp->conn_st.rxio, 1, &iov, t, &nrecs, &value);
	
	if (nrecs) {
	    int len;
	    hdr = iov.base;
	    off = (hdr->ti_t.th_off >> 4) * 4;
	    DATA_PT(tp->sock)->header= iov.base;
	    DATA_PT(tp->sock)->pointer=iov.base + off + OFFSETOF(struct tcpiphdr, ti_t);
	    
	    tp->iov_base=iov.base;
	    
	    len = ntohs(hdr->ti_i.ip_len) - (tp->conn_st.ipHdrLen - 16); /* XXX discount ether header */
	    
	    TRC(eprintf("tcp: received segment length %u\n", len));
	    /* Return TCP segment length */
	    return len; 
	} else {
	    return EVENT_TIMER;
	}
    }
    
}
