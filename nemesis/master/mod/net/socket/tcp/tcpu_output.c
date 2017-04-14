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
*  File              : tcpu_output.c                                       *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

/*
 *	@(#)tcpu_output.c	7.22 (Berkeley) 8/31/90
 */

#define panic printf
#define NOT_MAIN_FILE
#define TCPOUTFLAGS

#include <nemesis.h>
#include <stdio.h>
#include <time.h>
#include <netdb.h>
#include <FlowMan.ih>
#include "inet.h"
#include <iana.h>
#include <IO.ih>
#include <IOMacros.h>
#include <IDCMacros.h>


#define min(a,b) ((a)>(b)?(b):(a))

extern const uchar tcp_outflags[];
extern const int tcp_backoff[];


/*
 * Tcp output routine: figure out what should be sent and send it.
 */

int tcpu_output(struct tcpcb **tp_in)
{
    register struct tcpcb *tp;
    register struct tcpiphdr *tcpip;
    register int len, len1, win; 
    int off, flags, offset ,start, start1;
    int idle, sendalot, nbdatasnd;
    struct socket *socket;
    struct buffout *buff_out_ptr;
    u_int xsum;
    u_int val;
    int nrecs;
    
    /*
     * Determine length of data that should be transmitted,
     * and flags that will be used.
     * If there is some data or critical controls (SYN, RST)
     * to send, then transmit; otherwise, investigate further.
     */
    
    tp = *tp_in;
    socket=tp->sock;
    buff_out_ptr = BUFF_OUT(socket);
    
    if(tp == NULL)
	return(-1);
    
    if (( tp == 0 )|| tp->t_state == TCPS_CLOSED )
	return(3);       
    
    /*
     * highest sequence number sent == sequence number not acknowledged 
     * which means that we have sent data and received acks.
     */
    idle = (tp->snd_max == tp->snd_una); 
    
    if (idle && tp->t_idle >= tp->t_rxtcur) {
	/*
	 * We have been idle for "a while" and no acks are
	 * expected to clock out any data we send --
	 * slow start to get ack "clock" running again.
	 */
	tp->snd_cwnd = tp->t_maxseg; 
    }

again:
    sendalot = 0;
    off = tp->snd_nxt - tp->snd_una; 
    win = MIN(tp->snd_wnd, tp->snd_cwnd);
    
    /*
     * If in persist timeout with window of 0, send 1 byte.
     * Otherwise, if window is small but nonzero
     * and timer expired, we will send what we can
     * and go to transmit state.
     */
    if (tp->t_force) 
    {
	if (win == 0) {
	    win = 1;
	} else {
	    tp->t_timer[TCPT_PERSIST] = 0;
	    tp->t_rxtshift = 0; 
	}
    }
    
    /* 
     * number of data to be sent
     */
    nbdatasnd = buff_out_ptr->nbeltq - buff_out_ptr->nbeltsnd;
    len = MIN( nbdatasnd, win);
    
    flags = tcp_outflags[tp->t_state];
    
    if (len < 0) 
    {
	/*
	 * If FIN has been sent but not acked,
	 * but we haven't been called to retransmit,
	 * len will be -1.  Otherwise, window shrank
	 * after we sent into it.  If window shrank to 0,
	 * cancel pending retransmit and pull snd_nxt
	 * back to (closed) window.  We will enter persist
	 * state below.  If the window didn't close completely,
	 * just wait for an ACK.
	 */
	len = 0;
	nbdatasnd = 0;
	if (win == 0) 
	{
	    tp->t_timer[TCPT_REXMT] = 0;
	    tp->snd_nxt = tp->snd_una;
	    buff_out_ptr->nbeltsnd = 0 ;
	}
    }
    
    if (len > tp->t_maxseg)
    {
	len = tp->t_maxseg;
	sendalot = 1;
    }
    
    
    if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + nbdatasnd))
	flags &= ~TH_FIN;
    
    win = MAX_BUF - BUFF_IN(socket)->nbeltq;
    
    /*
     * If our state indicates that FIN should be sent
     * and we have not yet done so, or we're retransmitting the FIN,
     * then we need to send.
     */
    
    if (flags & TH_FIN &&
	((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una)) 
	goto send;
    
    /*
     * Send if we owe peer an ACK.
     */
    if (tp->t_flags & TF_ACKNOW) {
	goto send;
    }
    if (flags & (TH_SYN|TH_RST)) 
	goto send;
    if (SEQ_GT(tp->snd_up, tp->snd_una)) 
	goto send;
    
    /*
     * Sender silly window avoidance.  If connection is idle
     * and can send all data, a maximum segment,
     * at least a maximum default-size segment do it,
     * or are forced, do it; otherwise don't bother.
     * If peer's buffer is tiny, then send
     * when window is at least half open.
     * If retransmitting (possibly after persist timer forced us
     * to send into a small window), then must resend.
     */
    if (len)
    {
	if (len == tp->t_maxseg) 
	    goto send;
	if ((idle || tp->t_flags & TF_NODELAY))
	    goto send; 
	if (tp->t_force) 
	    goto send;
/*	    if (len >= tp->t_maxseg / 2) 
	    goto send; torsten 24.5.95*/
	if (len >= tp->max_sndwnd / 2) 
	    goto send;
	if (SEQ_LT(tp->snd_nxt, tp->snd_max)) 
	    goto send;
    }
    
    /*
     * Compare available window to amount of window
     * known to peer (as advertised window less
     * next expected input).  If the difference is at least two
     * max size segments, or at least 50% of the maximum possible
     * window, then want to send a window update to peer.
     */
    if (win > 0)
    {
	long adv = win - (tp->rcv_adv - tp->rcv_nxt);
	if (adv >= (long) (2 * tp->t_maxseg)) 
	    goto send;
	if (2 * adv >= (long) MAX_BUF) 
	    goto send;                 
    }
    
    /*
     * TCP window updates are not reliable, rather a polling protocol
     * using ``persist'' packets is used to insure receipt of window
     * updates.  The three ``states'' for the output side are:
     *	idle			not doing retransmits or persists
     *	persisting		to move a small or zero window
     *	(re)transmitting	and thereby not persisting
     *
     * tp->t_timer[TCPT_PERSIST]
     *	is set when we are in persist state.
     * tp->t_force
     *	is set when we are called to send a persist packet.
     * tp->t_timer[TCPT_REXMT]
     *	is set when we are retransmitting
     * The output side is idle when both timers are zero.
     *
     * If send window is too small, there is data to transmit, and no
     * retransmit or persist is pending, then go to persist state.
     * If nothing happens soon, send when timer expires:
     * if window is nonzero, transmit what we can,
     * otherwise force out a byte.
     */
    
    if (nbdatasnd && tp->t_timer[TCPT_REXMT] == 0 &&
	tp->t_timer[TCPT_PERSIST] == 0)
	{
	    tp->t_rxtshift = 0;
	    tcp_setpersist(tp);
	}
    /*
     * No reason to send a segment, just return.
     */
    return(0);
    
send:
    
    /*
     * Initialize the header from the template
     * for sends on this connection.
     */
    
    
    if (tp->t_template == 0)
	panic("tcp_output");
    
    tcpip = tp->t_template;
    
    if (len) {
	if (tp->t_force && len == 1) {
	    tp->tcpstat.tcps_sndprobe++;
	} else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
	    tp->tcpstat.tcps_sndrexmitpack++;
	    tp->tcpstat.tcps_sndrexmitbyte += len;
	} else {
	    tp->tcpstat.tcps_sndpack++;
	    tp->tcpstat.tcps_sndbyte += len;
	}
    } else if (tp->t_flags & TF_ACKNOW)
	tp->tcpstat.tcps_sndacks++;
    else if (flags & (TH_SYN|TH_FIN|TH_RST))
	tp->tcpstat.tcps_sndctrl++;
    else if (SEQ_GT(tp->snd_up, tp->snd_una))
	tp->tcpstat.tcps_sndurg++;
    else
	tp->tcpstat.tcps_sndwinup++;
    
    /*
     * Fill in fields, remembering maximum advertised
     * window for use in delaying messages about window sizes.
     * If resending a FIN, be sure not to use a new sequence number.
     */		           
    if (flags & TH_FIN && tp->t_flags & TF_SENTFIN && 
	tp->snd_nxt == tp->snd_max) {
	tp->snd_nxt--;
    }

    tcpip->ti_seq = htonl(tp->snd_nxt);
    tcpip->ti_ack = htonl(tp->rcv_nxt);
    tcpip->ti_flags = flags;
    
    /*
     * Calculate receive window.  Don't shrink window,
     * but avoid silly window syndrome.
     */
    
    if (win < (long)(MAX_BUF / 4) && win < (long)tp->t_maxseg)
	win = 0;
    if (win > TCP_MAXWIN) 
	win = TCP_MAXWIN;
    if (win < (long)(tp->rcv_adv - tp->rcv_nxt))
	win = (long)(tp->rcv_adv - tp->rcv_nxt);
    tcpip->ti_win = htons((u_short)win);
    if (SEQ_GT(tp->snd_up, tp->snd_nxt))
    {
	tcpip->ti_urp = htons((u_short)(tp->snd_up - tp->snd_nxt));
	tcpip->ti_flags |= TH_URG;
    } else {
	/*
	 * If no urgent pointer to send, then we pull
	 * the urgent pointer to the left edge of the send window
	 * so that it doesn't drift into the send window on sequence
	 * number wraparound.
	 */
	tp->snd_up = tp->snd_una;		/* drag it along */
    }
    /*
     * If we can send everything we've got, set PUSH.
     * (This will keep happy those implementations which only
     * give data to the user when a buffer fills or
     * a PUSH comes in.)
     */
    
    if (len && (off + len == nbdatasnd)) 
	tcpip->ti_flags |= TH_PUSH;

    /* Put TCP length in extended header, and then
       checksum extended header and data. */
    /* 26.9.1994                                   */
    /* calculate TCP length                        */
    /* if(len) */ 
    tcpip->ti_len   = (u_short)(sizeof(struct tcphdr) + len);
    
    start1 = ((buff_out_ptr->una + buff_out_ptr->nbeltsnd)%MAX_BUF);

    tcpip->ti_sum = 0;
    
    xsum = TCP_xsum_init(tcpip, 0);
    tcpip->ti_len=htons(tcpip->ti_len);
    
    if (len > 0) {
	if((start1 + len)<=MAX_BUF) {
	    xsum += in_cksum(buff_out_ptr->buf + start1, len);
	} else {
	    len1 = MAX_BUF - start1;
	    if (len1 & 1)
	    {
		buff_out_ptr->buf[MAX_BUF] = buff_out_ptr->buf[0];
		xsum += in_cksum(buff_out_ptr->buf + start1, len1+1);
		xsum += in_cksum(buff_out_ptr->buf + 1, len-len1-1);
	    }
	    else			
	    {
		xsum += in_cksum(buff_out_ptr->buf + start1, len1);
		xsum += in_cksum(buff_out_ptr->buf, len-len1);
	    }
	}
    }
    tcpip->ti_sum = htons( ((short16) TCP_xsum_final(xsum) ));

    /*
     * In transmit state, time the transmission and arrange for
     * the retransmit.  In persist state, just set snd_max.
     */
    
    if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0) {
	tcp_seq startseq = tp->snd_nxt;
	
	/*
	 * Advance snd_nxt over sequence space of this segment.
	 */
	if (flags & (TH_SYN|TH_FIN))
	{
	    if (flags & TH_SYN)
		tp->snd_nxt++;
	    if (flags & TH_FIN)
	    {
		tp->snd_nxt++;
		tp->t_flags |= TF_SENTFIN;
	    }
	}
	tp->snd_nxt += len;
	if (SEQ_GT(tp->snd_nxt, tp->snd_max))
	{
	    tp->snd_max = tp->snd_nxt;
	    /*
	     * Time this transmission if not a retransmission and
	     * not currently timing anything.
	     */
	    if (tp->t_rtt == 0)
	    {
		tp->time_send = 1;
		tp->t_rtt = 1;
		tp->t_rtseq = startseq;
		tp->tcpstat.tcps_segstimed++;
	    }
	}
	
	/*
	 * Set retransmit timer if not currently set,
	 * and not doing an ack or a keep-alive probe.
	 * Initial value for retransmit timer is smoothed
	 * round-trip time + 2 * round-trip time variance.
	 * Initialize shift counter which is used for backoff
	 * of retransmit time.
	 */
	if (tp->t_timer[TCPT_REXMT] == 0 &&
	    tp->snd_nxt != tp->snd_una)
	{
	    tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
	    if (tp->t_timer[TCPT_PERSIST])
	    {
		tp->t_timer[TCPT_PERSIST] = 0;
		tp->t_rxtshift = 0;
	    }
	}
    } else if (SEQ_GT(tp->snd_nxt + len, tp->snd_max)) {
	tp->snd_max = tp->snd_nxt + len;
    }
    
    /* Put ports number and offset in tcpip header */
    /* set ti_len to IP length, ti_len is used in TCPK for ip_len
       calculation                                                */
    
    tcpip->ti_len   = htons((u_short)(sizeof(struct tcpiphdr) + len));
    
    /* modified 26.9.1994 */
    
    ((struct ip *)tcpip)->ip_len = tcpip->ti_len;
    ((struct ip *)tcpip)->ip_ttl = 60;
    
    tcpip->ti_next = 0;
    tcpip->ti_prev = 0;
    
    if(tp->t_state >= TCPS_ESTABLISHED && CONNECTED(socket)) {
	start = (buff_out_ptr->una + buff_out_ptr->nbeltsnd)%MAX_BUF;
	/*****
	  printf ("before PutPkt: start=%d, len=%d, una=%d, beltyq=%d\n",
	  start,len,buff_out_ptr->una,buff_out_ptr->nbeltsnd);
	  *****/

	if(len) {
	    if((start + len)<MAX_BUF) {
		tp->wiov[1].base=buff_out_ptr->buf + start;
		tp->wiov[1].len=len;
		nrecs=2;
	    } else {
		len1 = MAX_BUF - start ;
		tp->wiov[1].base = buff_out_ptr->buf+start;
		tp->wiov[1].len  = len1 ;
		tp->wiov[2].base = buff_out_ptr->buf;
		tp->wiov[2].len  = len-len1 ;
		nrecs=3;
	    }
	} else {
	    nrecs = 1;
	}
    } else {
	nrecs=1;
	if(len) {
	    start = (buff_out_ptr->una + buff_out_ptr->nbeltsnd)%MAX_BUF ;
	    if( len+ start >MAX_BUF) {
		
		offset= MAX_BUF - start ;
		tp->wiov[1].base=(char *)buff_out_ptr->buf + start;
		tp->wiov[1].len=offset;
		tp->wiov[2].base=(char *)buff_out_ptr->buf;
		tp->wiov[2].len=len - offset;
		nrecs=3;
	    } else {
		
		tp->wiov[1].base=(char *)buff_out_ptr->buf + start;
		tp->wiov[1].len=len;
		nrecs=2;
	    }
	}
	
#if 0
	TRC(eprintf("tcp: Transmitting packet\n");
	
	{
	    uint8_t *buf = tp->wiov[0].base;
	    for(i = 0; i < tp->wiov[0].len; i+=4) {
		printf("%02u: %02x%02x%02x%02x\n", i, buf[i], buf[i+1], buf[i+2], buf[i+3]);
	    } 
	});
#endif
    }

    /* XXX If data start is not 4-byte aligned, send the first few
       bytes in a separate fragment. Evil, but this code is hopefully
       not sticking around for long anyway. */

    if((nrecs > 1) && (tp->wiov[1].len > 3)) {
	int align = ((word_t)tp->wiov[1].base) & 3;
	if(align != 0) {
	    align = 4 - align;
	    tp->wiov[3] = tp->wiov[2];
	    tp->wiov[2].base = (addr_t)(((word_t)tp->wiov[1].base) + align);
	    tp->wiov[2].len = tp->wiov[1].len - align;
	    tp->wiov[1].len = align;
	    nrecs++;
	}
    }

	
    TRC(eprintf("Doing IO$PutPkt: {\n");
	{
	    int i;
	    for(i = 0; i < nrecs; i++) {
		eprintf("[ %p, %04x ]\n", tp->wiov[i].base, tp->wiov[i].len);
	    }
	    eprintf(" }\n");
	});
    
		
    /* XXX Yuk Yuk Yuk - work around DE500 driver limitations */
    if((!tp->conn_st.useMultipleDataRecs) && (nrecs > 2)) {
	IO_Rec recs[2];
	int i, offset = 0;
	uint32_t nrecs; 

	for(i = 1; i < nrecs; i++) {
	    offset += tp->wiov[i].len;
	}

	recs[0] = tp->wiov[0];
	recs[1].base = Heap$Malloc(Pvs(heap), offset);
	recs[1].len = offset;
	
	offset = 0;
	for(i = 1; i < nrecs; i++) {
	    memcpy(recs[1].base + offset, tp->wiov[i].base, tp->wiov[i].len);
	    offset += tp->wiov[i].len;
	}

	TRC({
	    struct tcpiphdr *hdr = recs[0].base;
	    eprintf("TCP: Sending packet: destination = %I:%04x\n",
		    hdr->ti_dst.s_addr, ntohs(hdr->ti_dport));
	});
	    
	IO$PutPkt(tp->conn_st.txio, 2, recs, 0, FOREVER);
	IO$GetPkt(tp->conn_st.txio, 2, recs, FOREVER, &nrecs, &val);

	FREE(recs[1].base);
    } else {
	TRC({
	    struct tcpiphdr *hdr = tp->wiov[0].base;
	    eprintf("TCP: Sending packet: destination = %I:%04x\n",
		    hdr->ti_dst.s_addr, ntohs(hdr->ti_dport));
	});
	IO$PutPkt(tp->conn_st.txio, nrecs, tp->wiov, 0, FOREVER);
	IO$GetPkt(tp->conn_st.txio, nrecs, tp->wiov, FOREVER, &nrecs, &val);
    }
    
    buff_out_ptr->nbeltsnd +=  len;    
    tp->tcpstat.tcps_sndtotal++;
    
    /*
     * Data sent (as far as we can tell).
     * If this advertises a larger window than any other segment,
     * then remember the size of the advertised window.
     * Any pending ACK has now been sent.
     */
    if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
	tp->rcv_adv = tp->rcv_nxt + win;
    tp->t_flags &= ~(TF_ACKNOW|TF_DELACK);
    
    if (sendalot)
	goto again;
    return(0);
}

/*************************************/
void tcp_setpersist(struct tcpcb *tp)
{
    register t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;
    
    if (tp->t_timer[TCPT_REXMT])
	panic("tcp_output/persist: REXMT");
    /*
     * Start/restart persistance timer.
     */
    TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
		  t * tcp_backoff[tp->t_rxtshift],
		  TCPTV_PERSMIN, TCPTV_PERSMAX);
    if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
	tp->t_rxtshift++;
}






