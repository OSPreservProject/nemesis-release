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
*  File              : tcpu_var.h                                          *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/


/* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tcp_var.h	7.10 (Berkeley) 6/28/90
 */

/*
 * Kernel variables for tcp.
 */


/*
 *  File d'attente de sortie
 */

#define PRIME_DEPTH 40

#define BUF_SYS 16000 /* size of socket buffer */
                      /* BUF_SYS must be >= MAX_BUF */
#define MAX_BUF_was_this 14600
#define TCPMAXCON 10
#define TCPU_TTL 30        /*default time to live for TCPU segs */
#define TCP_MAX_SIZE 1500  /* maximum TCP segment size */
#define MAX_BUF ((TCP_MAX_SIZE-40) * (PRIME_DEPTH/2) )
/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {

        int     tcps_badsum;
        int     tcps_badoff;
        int     tcps_hdrops;
        int     tcps_badsegs;
        int     tcps_unack;

	u_long	tcps_connattempt;	/* connections initiated */
	u_long	tcps_accepts;		/* connections accepted */
	u_long	tcps_connects;		/* connections established */
	u_long	tcps_drops;		/* connections dropped */
	u_long	tcps_conndrops;		/* embryonic connections dropped */
	u_long	tcps_closed;		/* conn. closed (includes drops) */
	u_long	tcps_segstimed;		/* segs where we tried to get rtt */
	u_long	tcps_rttupdated;	/* times we succeeded */
	u_long	tcps_delack;		/* delayed acks sent */
	u_long	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u_long	tcps_rexmttimeo;	/* retransmit timeouts */
	u_long	tcps_persisttimeo;	/* persist timeouts */
	u_long	tcps_keeptimeo;		/* keepalive timeouts */
	u_long	tcps_keepprobe;		/* keepalive probes sent */
	u_long	tcps_keepdrops;		/* connections dropped in keepalive */

	u_long	tcps_sndtotal;		/* total packets sent */
	u_long	tcps_sndpack;		/* data packets sent */
	u_long	tcps_sndbyte;		/* data bytes sent */
	u_long	tcps_sndrexmitpack;	/* data packets retransmitted */
	u_long	tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u_long	tcps_sndacks;		/* ack-only packets sent */
	u_long	tcps_sndprobe;		/* window probes sent */
	u_long	tcps_sndurg;		/* packets sent with URG only */
	u_long	tcps_sndwinup;		/* window update-only packets sent */
	u_long	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	u_long	tcps_rcvtotal;		/* total packets received */
	u_long	tcps_rcvpack;		/* packets received in sequence */
	u_long	tcps_rcvbyte;		/* bytes received in sequence */
	u_long	tcps_rcvbadsum;		/* packets received with ccksum errs */
	u_long	tcps_rcvbadoff;		/* packets received with bad offset */
	u_long	tcps_rcvshort;		/* packets received too short */
	u_long	tcps_rcvduppack;	/* duplicate-only packets received */
	u_long	tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u_long	tcps_rcvpartduppack;	/* packets with some duplicate data */
	u_long	tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u_long	tcps_rcvoopack;		/* out-of-order packets received */
	u_long	tcps_rcvoobyte;		/* out-of-order bytes received */
	u_long	tcps_rcvpackafterwin;	/* packets with data after window */
	u_long	tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u_long	tcps_rcvafterclose;	/* packets rcvd after "close" */
	u_long	tcps_rcvwinprobe;	/* rcvd window probe packets */
	u_long	tcps_rcvdupack;		/* rcvd duplicate acks */
	u_long	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u_long	tcps_rcvackpack;	/* rcvd ack packets */
	u_long	tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u_long	tcps_rcvwinupd;		/* rcvd window update packets */
};


struct	tcpiphdr *tcp_template();

void tcpu_close(struct tcpcb *tp);
void tcpu_timers(struct tcpcb *tp, int timer);

int close_tcpu(), send_tcpu(), recv_tcpu(), accept_tcpu(), connect_tcpu(), bind_tcpu(), socket_tcpu();


struct tcp_data {
    struct tcpcb *tcp_cb;
    struct  buffout   *buff_out;
   struct  buffin    *buff_in;
   struct  datapointer *data_pt;
   int connected;
   char    *buffoutend;
}; 

#define GET_TCPCB(sockfd) (((struct tcp_data *)((sockfd)->data))->tcp_cb)

struct buffout{
  u_char buf[MAX_BUF+1]; /*buffer */
  int  una  ;          /* 1st octet not acknoledged */
  int  nbeltq ;         /* nb of elt in the queue    */
  int nbeltsnd ;        /* nb element snd to be acknoledged */
};  

struct buffin {
  u_char buf[MAX_BUF]; /*buffer */
  int  next ;          /*pointeur sur le prochain octet a recevoir ou envoyer*/
  int  nbeltq;          /* 1st octet not acknoledged */
};

struct datapointer {
  char *header;   /*[sizeof(struct tcpiphdr)];*/
  char *pointer;
  char *reass_pointer;
  char reass_buffer[MAX_BUF];
  int length;
};

#ifdef MAIN_FILE
struct  tcpcb *tcpcb_tab[TCPMAXCON];
struct  buffout   *buff_out[TCPMAXCON];
char    *buffoutend[TCPMAXCON]; 
struct  buffin    *buff_in[TCPMAXCON];
struct  datapointer data_pt[TCPMAXCON];
int     connected[TCPMAXCON];
#endif

#ifdef NOT_MAIN_FILE
extern struct tcpcb *tcpcb_tab[TCPMAXCON];
extern struct  buffout   *buff_out[TCPMAXCON];
extern char    *buffoutend[TCPMAXCON]; 
extern struct  buffin    *buff_in[TCPMAXCON];
extern struct  datapointer data_pt[TCPMAXCON];
extern connected[TCPMAXCON];
#endif

#define BUFF_OUT(a) (((struct tcp_data * )(a->data))->buff_out)
#define BUFF_IN(a) (((struct tcp_data * )(a->data))->buff_in)
#define BUFFOUTEND(a) (((struct tcp_data * )(a->data))->buffoutend)
#define CONNECTED(a) (((struct tcp_data * )(a->data))->connected)
#define DATA_PT(a) (((struct tcp_data * )(a->data))->data_pt)


/* A tcp_conn_st carries all the state associated with a particular
   connection */

typedef struct _tcp_conn_st {
    bool_t         stack_built;
    IO_cl         *txio;
    IO_cl         *rxio;
    IP_cl         *ip;
    uint32_t       ipHdrLen;
    Ether_cl      *eth;
    Heap_clp       rxheap;
    Heap_clp       txheap;
    bool_t         useMultipleDataRecs;
    FlowMan_ConnID cid;
    FlowMan_Flow   flow;
} tcp_conn_st;

typedef struct _tcp_rx_block tcp_rx_block;

struct _tcp_rx_block {
    tcp_rx_block *next;
    tcp_rx_block *prev;
    /* Data starts here */
};

/*
 * Tcp control block, one per tcp; fields:
 */
    
struct tcpcb {
/* Chain of blocks received by listening socket and passed to this
   connection */
        tcp_rx_block rx_blocks;  
        tcp_rx_block syn_rx_blocks;
        tcp_rx_block  *storedRXBlock;
        
        struct tcpiphdr *seg_next;	/* sequencing queue */
	struct tcpiphdr *seg_prev;
	struct socket *sock;      /* describteur de la sock. associee*/  
	int     prog;                   /* for the test the performance anna */
	struct  timeval time;
	int     time_ack;
	int     time_send;
	short	t_state;		/* state of this connection */
        short	t_timer[TCPT_NTIMERS];	/* tcp timers */
	short	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	short	t_rxtcur;		/* current retransmit value */
	short	t_dupacks;		/* consecutive dup acks recd */
	u_short	t_maxseg;		/* maximum segment size */
	short	t_force;		/* 1 if forcing out a byte */
	u_char	t_flags;
	short   t_drap;
	int     t_sema;
	
#define	TF_ACKNOW	0x01		/* ack peer immediately */
#define	TF_DELACK	0x02		/* ack, but try to delay it */
#define	TF_NODELAY	0x04		/* don't delay packets to coalesce */
#define	TF_NOOPT	0x08		/* don't use tcp options */
#define	TF_SENTFIN	0x10		/* have sent FIN */
	struct	tcpiphdr *t_template;	/* skeletal packet for transmit */

/*
 * The following fields are used as in the protocol specification.
 * See RFC783, Dec. 1981, page 21.
 */
/* send sequence variables */
        tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */
	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	u_short	snd_wnd;		/* send window */
/* receive sequence variables */
	u_short	rcv_wnd;		/* receive window */
	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_up;			/* receive urgent pointer */
	tcp_seq	irs;			/* initial receive sequence number */
/*
 * Additional variables for this implementation.
 */
/* receive variables */
	tcp_seq	rcv_adv;		/* advertised window */
/* retransmit variables */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
/* congestion control (for slow start, source quench, retransmit after loss) */
	u_short	snd_cwnd;		/* congestion-controlled window */
	u_short snd_ssthresh;		/* snd_cwnd size threshhold for
					 * for slow start exponential to
					 * linear switch
					 */
/*
 * transmit timing stuff.  See below for scale of srtt and rttvar.
 * "Variance" is actually smoothed difference.
 */
	short	t_idle;			/* inactivity time */
	short	t_rtt;			/* round trip time */
	tcp_seq	t_rtseq;		/* sequence number being timed */
	short	t_srtt;			/* smoothed round-trip time */
	short	t_rttvar;		/* variance in round-trip time */
	u_short	t_rttmin;		/* minimum rtt allowed */
	u_short	max_sndwnd;		/* largest window peer has offered */

/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
#define	TCPOOB_HAVEDATA	0x01  
#define	TCPOOB_HADDATA	0x02
	short	t_softerror;		/* possible error not yet reported */

        struct	tcpstat tcpstat;	
        int     tcp_ttl;
        int tcp_mssdflt;
        int tcp_rttdflt;
        int tcprexmtthresh;
        int tcpprintfs;
        int tcpcksum;
        int tcpnodelack;
        int tcp_keepidle;
        int tcp_keepintvl;
        int tcp_maxidle;
        int loop;
        struct  tcpiphdr tcp_saveti;
        char *iov_base;
        IO_Rec wiov[6];
        FlowMan_cl *fman;
        tcp_conn_st conn_st;
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/* The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		8	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		3	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	4	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	2	/* multiplier for rttvar; 2 bits */



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
 * This macro assumes that the value of TCP_RTTVAR_SCALE
 * is the same as the multiplier for rttvar.
 */
#define	TCP_REXMTVAL(tp) \
	(((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar)

