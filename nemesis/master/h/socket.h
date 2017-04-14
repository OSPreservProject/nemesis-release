/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      h/socket.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Header file with definitions and prototypes for sockets on Nemesis.
**      Initial version taken from /usr/include/sys/socket.h on Digital 
**      Alpha OSF 1.3.2 as an example, and modified at will.
** 
** ENVIRONMENT: 
**
**      XXX At the moment sockets stuff is all in the connection
**      manager domain, but probably should just be a user library
**      module later on. Not quite true anymore.
** 
** */

#ifndef SOCKET_H
#define SOCKET_H

/*
 * Definitions related to sockets: types, address families, options.
 */

#include <nemesis.h>
#include <ARPMod.ih>
#include <EtherMod.ih>
#include <IPMod.ih>
#include <Net.ih>
#include <UDPMod.ih>
#include <netorder.h>
#include <netdb.h>
#include <mutex.h>
#include <iana.h>

/* May as well put this here, since its the only place its used */
struct timeval {
  int	tv_sec;		/* seconds */
  int	tv_usec;	/* microseconds */
};


/*
 * Types
 */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SOCK_RAW	3		/* raw-protocol interface */
#define	SOCK_RDM	4		/* reliably-delivered message */
#define	SOCK_SEQPACKET	5		/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local addr and port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */


/*
 * Structure used for manipulating linger option.
 */
struct	linger {
    int	l_onoff;		/* option on/off */
    int	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_UNIX		1		/* local to host (pipes, portals) */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_ISO		7		/* ISO protocols */
#define	AF_OSI		AF_ISO
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */
#define AF_DECnet	12		/* DECnet */
#define AF_DLI		13		/* DEC Direct data link interface */
#define AF_LAT		14		/* LAT */
#define	AF_HYLINK	15		/* NSC Hyperchannel */
#define	AF_APPLETALK	16		/* Apple Talk */
#define	AF_ROUTE	17		/* Internal Routing Protocol */
#define	AF_LINK		18		/* Link layer interface */
#define	pseudo_AF_XTP	19		/* eXpress Transfer Protocol (no AF) */
#define AF_NETMAN	20		/* DNA Network Management */
#define AF_X25		21		/* X25 protocol */
#define AF_CTF		22		/* Common Trace Facility */
#define AF_WAN		23		/* Wide Area Network protocols */

#define AF_USER		24		/* Wild card (user defined) protocol */

#define	AF_MAX		25


struct sockaddr {
    uint16_t sa_family; /* address family */
    char	sa_data[14]; /* actually longer; address value */
};



/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct sockproto {
    uint16_t	sp_family;		/* address family */
    uint16_t	sp_protocol;		/* protocol */
};



/* Structure for internet addresses. */
struct in_addr {
    uint32_t s_addr;
};


/* Structure describing an Internet (IP) socket address. */
struct sockaddr_in {
    uint16_t       sin_family;      /* Address family       */
    uint16_t       sin_port;        /* Port number          */
    struct in_addr sin_addr;        /* Internet address     */    
    char           sin_zero[8];     /* zero pad to make this up to 16 bytes */
};



/*
 * Protocol families, same as address families for now.
 */
#define	PF_UNSPEC	AF_UNSPEC
#define	PF_UNIX		AF_UNIX
#define	PF_INET		AF_INET
#define	PF_IMPLINK	AF_IMPLINK
#define	PF_PUP		AF_PUP
#define	PF_CHAOS	AF_CHAOS
#define	PF_NS		AF_NS
#define	PF_ISO		AF_ISO
#define	PF_OSI		AF_ISO
#define	PF_ECMA		AF_ECMA
#define	PF_DATAKIT	AF_DATAKIT
#define	PF_CCITT	AF_CCITT
#define	PF_SNA		AF_SNA
#define PF_DECnet	AF_DECnet
#define PF_DLI		AF_DLI
#define PF_LAT		AF_LAT
#define	PF_HYLINK	AF_HYLINK
#define	PF_APPLETALK	AF_APPLETALK
#define	PF_ROUTE	AF_ROUTE
#define	PF_LINK		AF_LINK
#define	PF_XTP		pseudo_AF_XTP	/* really just proto family, no AF */
#define PF_NETMAN	AF_NETMAN
#define PF_X25		AF_X25
#define PF_CTF		AF_CTF
#define PF_WAN		AF_WAN

#define	PF_MAX		AF_MAX

/*
 * Maximum queue length specifiable by listen.
 */
#define	SOMAXCONN	8


/*
 * Option flags per-socket.
 */
#define SO_DEBUG        0x0001      /* turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002      /* socket has had listen() */
#define SO_REUSEADDR    0x0004      /* allow local address reuse */
#define SO_KEEPALIVE    0x0008      /* keep connections alive */
#define SO_DONTROUTE    0x0010      /* just use interface addresses */
#define SO_BROADCAST    0x0020      /* permit sending of broadcast msgs */
#define SO_USELOOPBACK  0x0040      /* bypass hardware when possible */
#define SO_LINGER       0x0080      /* linger on close if data present */
#define SO_OOBINLINE    0x0100      /* leave received OOB data in line */
#define SO_REUSEPORT    0x0200      /* allow local addr and port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF       0x1001      /* send buffer size */
#define SO_RCVBUF       0x1002      /* receive buffer size */
#define SO_SNDLOWAT     0x1003      /* send low-water mark */
#define SO_RCVLOWAT     0x1004      /* receive low-water mark */
#define SO_SNDTIMEO     0x1005      /* send timeout */
#define SO_RCVTIMEO     0x1006      /* receive timeout */
#define SO_ERROR        0x1007      /* get error status and clear */
#define SO_TYPE         0x1008      /* get socket type */


/* the fd_set type and related macros */
typedef word_t fd_set;  /* really a "struct socket *", but don't tell anyone */

/* this fd_set is not much of a set: it only holds one fd at a time,
 * the last one to be added being the only one remembered.  The
 * FD_CLR() macro is a little too enthusiatic about clearing the fd
 * out of the set. */
#define FD_ZERO(setp)      ((*(setp))=0)
#define FD_SET(fd, setp)   ((*(setp))=fd)
#define FD_CLR(fd, setp)   ((*(setp))=0)
#define FD_ISSET(fd, setp) ((*(setp))==fd)


/* nasty ioctl() type thingies for compatability with stupid broken
 * programs who think they know what a magic number will do when
 * handed to the kernel. Oh, foolish mortals! */
#define FIONBIO          0x5421


struct msghdr {
    addr_t     msg_name;           /* optional address */
    uint32_t   msg_namelen;        /* size of address */
    IO_Rec     *msg_recs;          /* scatter/gather array */
    uint32_t   msg_nrecs;          /* # elements in msg_iov */
#if 0
    caddr_t    msg_control;        /* ancillary data, see below */
    uint32_t   msg_controllen;     /* ancillary data buffer len */
    int msg_flags;                 /* flags on received message */
#endif
};


typedef struct {
    int (*init)();
    int (*accept)();
    int (*bind)();
    int (*connect)();
    int (*get_addr_len)();
    int (*listen)();
    int (*recv)();
    int (*send)();
    int (*close)();
    int (*select)();
} proto_ops;

typedef enum {
    SS_UNBOUND = 0,           /* allocated, but not bound */
    SS_BOUND,                 /* bound, but unconnected   */
    SS_CONNECTING,            /* in process of connecting */
    SS_CONNECTED,             /* connected to socket      */
    SS_DISCONNECTING          /* in process of disconnecting  */
} socket_state;


struct socket {
    mutex_t          lock;       /* exclusive access to socket struct */
    mutex_t          rxlock;     /* exclusive access to RX side */
    mutex_t          txlock;     /* exclusive access to TX side */
    socket_state     state;      /* connected, disconnected, etc */
    uint32_t         flags;      /* various flags */
    uint32_t         type;       /* datagram, stream, etc, etc */
    uint32_t	     magic;      /* magic number for robustness */
    const proto_ops  *ops;       /* protocol specific operations */
    void             *data;      /* protocol specific data */
    uint32_t         refs;       /* number of references to this socket */
    struct socket    *next;      /* next socket in the chain */
    struct sockaddr  laddr;      /* local address */
    struct sockaddr  raddr;      /* peer address */
};


extern int accept(int, struct sockaddr *, int *);
extern int bind(int, const struct sockaddr *, int);
extern int connect(int, const struct sockaddr *, int);
extern int getpeername(int, struct sockaddr *, int *);
extern int getsockname(int, struct sockaddr *, int *);
extern int getsockopt(int, int, int, char *, int *);
extern int listen(int, int);
extern int recv(int, char *, int, int);
extern int recvfrom(int, char *, int, int, struct sockaddr *, int *);
extern int recvmsg(int, struct msghdr *, int);
extern int send(int, const char *, int, int);
extern int sendmsg(int, struct msghdr *, int);
extern int sendto(int, const char *, int, int, const struct sockaddr *, int);
extern int setsockopt(int, int, int, const char *, int);
extern int socket(int, int, int);
extern int socketpair(int, int, int, int [2]);
extern int shutdown(int, int);
extern int select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern int ioctl(int fd, int, ...);
extern int close(int fd);

#endif /* SOCKET_H */

/* End of socket.h */
