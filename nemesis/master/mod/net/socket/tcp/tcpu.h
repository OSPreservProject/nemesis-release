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
*  File              : tcpu.h                                              *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

/* @(#) Gipsi	SPIX+SOCKET	netinet/tcp.h	1.1	85/08/06
 *	4..2BSD	tcp.h	6.1	83/07/29
 */

#include <socket.h>
#include <bstring.h>

#ifndef _U_TYPES
#define _U_TYPES
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef unsigned long u_long;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;
typedef unsigned long ulong;
#endif

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

typedef	u_int	tcp_seq; /* was u_long */
/*
 * TCP header.
 * Per RFC 793, September, 1981.
 */
struct tcphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */
        u_char   th_off;
	u_char	th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */
};

#define	TCPOPT_EOL	0
#define	TCPOPT_NOP	1
#define	TCPOPT_MAXSEG	2

#define TCP_MAXWIN_was 65520 /*65535-20 */
#define TCP_MAXWIN MAX_BUF
#define TCP_MSS (TCP_MAX_SIZE-40) /* size of maximum packet size (ethernet) 1500 - 40 */
			 /* (size of tcpiphdr ) */

typedef unsigned int word32;       /*  32-bit */ 
typedef unsigned short short16;    /*  16-bit */  
typedef unsigned char byte8;       /*  8-bit  */  

/* TCP FSM state definitions.
 * Per RFC793, September, 1981.
 */
/* RS extracetd from /usr/include/netinet/tcp_fsm.h */

#define TCP_NSTATES     11

#define TCPS_CLOSED             0       /* closed */
#define TCPS_LISTEN             1       /* listening for connection */
#define TCPS_SYN_SENT           2       /* active, have sent syn */
#define TCPS_SYN_RECEIVED       3       /* have send and received syn */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define TCPS_ESTABLISHED        4       /* established */
#define TCPS_CLOSE_WAIT         5       /* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define TCPS_FIN_WAIT_1         6       /* have closed, sent fin */
#define TCPS_CLOSING            7       /* closed xchd FIN; await FIN ACK */
#define TCPS_LAST_ACK           8       /* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define TCPS_FIN_WAIT_2         9       /* have closed, fin is acked */
#define TCPS_TIME_WAIT          10      /* in 2*msl quiet wait after close */

#define TCPS_HAVERCVDSYN(s)     ((s) >= TCPS_SYN_RECEIVED)
#define TCPS_HAVERCVDFIN(s)     ((s) >= TCPS_TIME_WAIT)

/* from /usr/include/errno.h */
/* non-blocking and interrupt i/o */
#define EWOULDBLOCK     35              /* Operation would block */
#define EAGAIN          EWOULDBLOCK     /* ditto */
#define EINPROGRESS     36              /* Operation now in progress */
#define EALREADY        37              /* Operation already in progress */

/* ipc/network software */

        /* argument errors */
#define ENOTSOCK        38              /* Socket operation on non-socket */
#define EDESTADDRREQ    39              /* Destination address required */
#define EMSGSIZE        40              /* Message too long */
#define EPROTOTYPE      41              /* Protocol wrong type for socket */
#define ENOPROTOOPT     42              /* Protocol not available */
#define EPROTONOSUPPORT 43              /* Protocol not supported */
#define ESOCKTNOSUPPORT 44              /* Socket type not supported */
#define EOPNOTSUPP      45              /* Operation not supported on socket */
#define EPFNOSUPPORT    46              /* Protocol family not supported */
#define EAFNOSUPPORT    47              /* Address family not supported by protocol f
amily */
#define EADDRINUSE      48              /* Address already in use */
#define EADDRNOTAVAIL   49              /* Can't assign requested address */

        /* operational errors */
#define ENETDOWN        50              /* Network is down */
#define ENETUNREACH     51              /* Network is unreachable */
#define ENETRESET       52              /* Network dropped connection on reset */
#define ECONNABORTED    53              /* Software caused connection abort */
#define ECONNRESET      54              /* Connection reset by peer */
#define ENOBUFS         55              /* No buffer space available */
#define EISCONN         56              /* Socket is already connected */
#define ENOTCONN        57              /* Socket is not connected */
#define ESHUTDOWN       58              /* Can't send after socket shutdown */
#define ETOOMANYREFS    59              /* Too many references: can't splice */
#define ETIMEDOUT       60              /* Connection timed out */
#define ECONNREFUSED    61              /* Connection refused */

        /* */
#define ELOOP           62              /* Too many levels of symbolic links */
#define ENAMETOOLONG    63              /* File name too long */

/* should be rearranged */
#define EHOSTDOWN       64              /* Host is down */
#define EHOSTUNREACH    65              /* No route to host */

struct tcpcb;

int tcp_stack(struct tcpcb *tp, struct sockaddr_in *local, 
	      struct sockaddr_in *remote);

bool_t tcp_route(struct tcpcb *tp,const uint32_t realip, 
		 uint32_t *useip, Net_EtherAddr *destmac);

int tcpu_output(struct tcpcb **tp_in);
int get_res(struct tcpcb *tp,struct timeval *timeout);
struct tcp_data *tcp_data_init(struct socket *sock);
void tcpu_input(struct tcpcb **tp_in,int len);
int slow_timo(struct socket *sockfd);
void flush_rx_packet(struct tcpcb *tp);
void tcp_drop(struct tcpcb *tp, int errno);
void tcp_xmit_timer(struct tcpcb *tp);
void tcp_canceltimers(struct tcpcb *tp);
void tcpu_timers(struct tcpcb *tp, int timer);
void tcp_setpersist(struct tcpcb *tp);

struct tcpiphdr;
bool_t matches_hostport(struct tcpiphdr *hdr, struct tcpcb *tp);
void tcp_respond(struct tcpcb *tp, struct tcpiphdr *ti, 
		 tcp_seq ack, tcp_seq seq, 
		 int flags, bool_t swap);

int tcp_reass(struct tcpcb *tp, struct tcpiphdr *ti);
u_int TCP_xsum_init (struct tcpiphdr *tcpip, u_int xsum);
u_int in_cksum(uint8_t *mp, int len);
u_int TCP_xsum_final (u_int xsum);

#include "tcp_fsm.h"
#include "tcpip.h"
