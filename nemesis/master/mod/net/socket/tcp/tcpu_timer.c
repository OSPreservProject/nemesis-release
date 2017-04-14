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
*  File              : tcpu_timer.c                                        *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

/*
 *	tcpu_timer.c	
 */
#include <nemesis.h>
#include <stdio.h>
#include <FlowMan.ih>
#include "inet.h"
#include <socket.h>


#if 0
int	tcp_keepidle = TCPTV_KEEP_IDLE;
int	tcp_keepintvl = TCPTV_KEEPINTVL;
int	tcp_maxidle;
int     loop = 0;
#endif
extern int tcp_backoff[];

int slow_timo(sockfd)
struct socket *sockfd;
{
    struct tcp_data *td;
    struct tcpcb *tp;
    int i;
    int timer = TCPT_NTIMERS;
    
    timer++;
    
    td=(struct tcp_data *)(((struct socket *)sockfd)->data);
    tp=td->tcp_cb;
    if(tp == NULL) {
	ntsc_dbgstop();
    }
    
    for(i = 0; i < TCPT_NTIMERS; i++) {
	if(tp->t_timer[i] && --tp->t_timer[i] == 0) {
	    timer = i;
	    break;
	}
    }
    tp->t_idle++;
    if(tp->t_rtt)
	tp->t_rtt;
    return(timer);
}

/*
 * Cancel all timers for TCP tp.
 */
void tcp_canceltimers(struct tcpcb *tp)
{
    register int i;
    
    for (i = 0; i < TCPT_NTIMERS; i++)
	tp->t_timer[i] = 0;
}



				/* tcpu timer processing */
void tcpu_timers(tp, timer)
    struct  tcpcb *tp;
    int timer;
{
    register int rexmt;
    unsigned long xsum = 0;

    
    if(tp == NULL) {
	ntsc_dbgstop();
    }
    
    switch (timer) {
	
	/*
	 * 2 MSL timeout in shutdown went off.  If we're closed but
	 * still waiting for peer to close and connection has been idle
	 * too long, or if 2MSL time is up from TIME_WAIT, delete connection
	 * control block.  Otherwise, check again in a bit.
	 */
    case TCPT_2MSL:
	if (tp->t_state != TCPS_TIME_WAIT &&
	    tp->t_idle <= tp->tcp_maxidle)
	    tp->t_timer[TCPT_2MSL] = tp->tcp_keepintvl;
	else
	    tcpu_close(tp);
	
	break;
	
	/*
	 * Retransmission timer went off.  Message has not
	 * been acked within retransmit interval.  Back off
	 * to a longer retransmit interval and retransmit one segment.
	 */
    case TCPT_REXMT:
	if (++tp->t_rxtshift > TCP_MAXRXTSHIFT)
	    {
		tp->t_rxtshift = TCP_MAXRXTSHIFT;
		tp->tcpstat.tcps_timeoutdrop++;
		{
		    printf(" the socket %d has been closed :too many retransmissions\n",
			   tp->sock);
		    tcp_drop(tp,tp->t_softerror ?
			     tp->t_softerror : ETIMEDOUT );
		    break;
		}
	    }
	tp->tcpstat.tcps_rexmttimeo++;
	rexmt = TCP_REXMTVAL(tp) * tcp_backoff[tp->t_rxtshift];
	TCPT_RANGESET(tp->t_rxtcur, rexmt,
		      tp->t_rttmin, TCPTV_REXMTMAX);
	tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
	
	/*
	 * If losing, let the lower level know and try for
	 * a better route.  Also, if we backed off this far,
	 * our srtt estimate is probably bogus.  Clobber it
	 * so we'll take the next rtt measurement as our srtt;
	 * move the current srtt into rttvar to keep the current
	 * retransmit times until then.
	 */
	if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4)
	    {
		/*	in_losing(tp->t_inpcb);*/
		tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
		tp->t_srtt = 0;
	    }
	tp->snd_nxt = tp->snd_una;
	
	BUFF_OUT(tp->sock)->nbeltsnd = 0;
	/*
	 * If timing a segment in this window, stop the timer.
	*/
	tp->t_rtt = 0;
	/*
	 * Close the congestion window down to one segment
	 * (we'll open it by one segment for each ack we get).
	 * Since we probably have a window's worth of unacked
	 * data accumulated, this "slow start" keeps us from
	 * dumping all that data as back-to-back packets (which
	 * might overwhelm an intermediate gateway).
	 *
	 * There are two phases to the opening: Initially we
	 * open by one mss on each ack.  This makes the window
	 * size increase exponentially with time.  If the
	 * window is larger than the path can handle, this
	 * exponential growth results in dropped packet(s)
	 * almost immediately.  To get more time between 
	 * drops but still "push" the network to take advantage
	 * of improving conditions, we switch from exponential
	 * to linear window opening at some threshhold size.
	 * For a threshhold, we use half the current window
	 * size, truncated to a multiple of the mss.
	 *
	 * (the minimum cwnd that will give us exponential
	 * growth is 2 mss.  We don't allow the threshhold
	 * to go below this.)
	 */
	{
	    u_int win = MIN(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
	    if (win < 2)
		win = 2;
	    tp->snd_cwnd = tp->t_maxseg;
	    tp->snd_ssthresh = win * tp->t_maxseg;
	    tp->t_dupacks = 0;
	}
	(void) tcpu_output(&tp);
	break;
	
	/*
	 * Persistance timer into zero window.
	 * Force a byte to be output, if possible.
	 */
    case TCPT_PERSIST:
	tp->tcpstat.tcps_persisttimeo++;
	tcp_setpersist(tp);
	tp->t_force = 1;
	(void) tcpu_output(&tp);
	tp->t_force = 0;
	break;
	
	/*
	 * Keep-alive timer went off; send something
	 * or drop connection if idle for too long.
	 */
    case TCPT_KEEP:
	tp->tcpstat.tcps_keeptimeo++;
	if (tp->t_state < TCPS_ESTABLISHED)
	    goto dropit;
	if (tp->t_state <= TCPS_CLOSE_WAIT) {
	    if (tp->t_idle >= tp->tcp_keepidle + tp->tcp_maxidle)
		goto dropit;
	    /*
	     * Send a packet designed to force a response
	     * if the peer is up and reachable:
	     * either an ACK if the connection is still alive,
	     * or an RST if the peer has closed the connection
	     * due to timeout or reboot.
	     * Using sequence number tp->snd_una-1
	     * causes the transmitted zero-length segment
	     * to lie outside the receive window;
	     * by the protocol spec, this requires the
	     * correspondent TCP to respond.
	     */
	    tp->tcpstat.tcps_keepprobe++;
#ifdef TCP_COMPAT_42
	    /*
	     * The keepalive packet must have nonzero length
	     * to get a 4.2 host to respond.
	     */
	    tcp_respond(tp, tp->t_template, tp->rcv_nxt - 1, tp->snd_una - 1, 0, False);
#else
	    tcp_respond(tp, tp->t_template, tp->rcv_nxt, tp->snd_una - 1, 0, False);
#endif
	    tp->t_timer[TCPT_KEEP] = tp->tcp_keepintvl;
	} else
	    tp->t_timer[TCPT_KEEP] = tp->tcp_keepidle;
	break;
      dropit:
	tp->tcpstat.tcps_keepdrops++;
	printf("the socket %d has been closed : the peer does not answer any more !\n", tp->sock);
	tcp_drop(tp, ETIMEDOUT);
	break;
    }

}
