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
**      socket.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Initial implementation of Nemesis sockets. Hoho.
** 
** ENVIRONMENT: 
**
**      Jump-tabled library module in user space.
** 
*/

#include <nemesis.h>
#include <stdio.h>
#include <socket.h>
#include <string.h>
#include <contexts.h>
#include <exceptions.h>

#include <FlowMan.ih>
#include <Netif.ih>
#include <IOMacros.h>
#include <IDCMacros.h>

#include <autoconf/net_resolver.h>
#ifdef CONFIG_NET_RESOLVER_SUPPORT
#include <ResolverMod.ih>
#include <Resolver.ih>
#endif

#ifdef SOCKET_DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif

/* We have the client keep all our state by returning the address of a
 * struct socket as the socket fd.  Yuk!  We can buy a little robustness
 * by using a magic number in the data we hand to the client, though. */
#define SOCK_MAGIC  0x4f085d52


/*
 *  Allocate a socket
 */
struct socket *sock_alloc(void)
{
    struct socket *sock;

    /* XXX erm; should we malloc this on the Pvs(heap) ? */
    sock = Heap$Malloc(Pvs(heap), sizeof(struct socket));
    if(!sock)
    {
	fprintf(stderr, "sock_alloc: out of mem. oops.\n");
	return(NULL);
    }

    /* Scope of the locks:
     *   "lock" is for access to this structure
     *   "txlock" is to serialise transmitter resource usage
     *   "rxlock" is to serialise receiver resource usage
     */
    MU_INIT(&sock->lock);
    MU_INIT(&sock->txlock);
    MU_INIT(&sock->rxlock);
    sock->state = SS_UNBOUND;
    sock->flags = 0;
    sock->type  = 0;
    sock->magic = SOCK_MAGIC;
    sock->ops   = NULL;
    sock->data  = NULL;
    sock->refs  = 1;
    sock->next  = NULL;
    memset(&(sock->laddr), 0, sizeof(struct sockaddr));
    memset(&(sock->raddr), 0, sizeof(struct sockaddr));

    return sock;
}


/*
** XXX SMH: from below, we have the standard socket calls. 
** Note that we don't stuff around with closures at the moment, mainly coz
** I'd like to implement this as a jumptabled or static library in the end. 
*/



/* Locking: none at this level (though protocol level might lock) */
int accept(int socket, struct sockaddr *address, int *address_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;
    int res;

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "accept: socket %d has bad magic number (%#x != %#x)",
	       socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    if((res = cur_sock->ops->accept(cur_sock, address, address_len))==-1)
    {
        fprintf(stderr, "accept: protocol accept failed.\n");
    }

    return res;

}

/* Locking: exclusive, including protocol bind. */
int bind(int socket, const struct sockaddr *address, int address_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "bind: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if(cur_sock->state != SS_UNBOUND)
    {
	fprintf(stderr, "bind: socket %d has bad state (=%d)\n",
		socket, cur_sock->state);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    if(cur_sock->ops->bind(cur_sock, address, address_len)==-1)
    {
	TRC(fprintf(stderr, "bind: protocol bind failed.\n"));
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    cur_sock->state= SS_BOUND;

    MU_RELEASE(&cur_sock->lock);

    /* XXX is that it? */
    return 0;
}

/* Locking: state only */
int connect(int socket, const struct sockaddr *address, int address_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "connect: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if(cur_sock->state != SS_BOUND)
    {
	fprintf(stderr, "connect: socket %d is not bound (state=%d)\n", 
		socket, cur_sock->state);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    MU_RELEASE(&cur_sock->lock);

    /* XXX connect sets the state of the socket itself */
    if(cur_sock->ops->connect(cur_sock, address, address_len)==-1)
    {
	fprintf(stderr, "connect: protocol connect failed.\n");
	return -1;
    }


    /* XXX is that it? */
    return 0;
}


/*
** Return the destination 'name' (i.e. address) of a connected socket 
** Locking: exclusive [for state and addrs].
*/
int getpeername(int socket, struct sockaddr *address, int *address_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "getpeername: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if(cur_sock->state != SS_CONNECTED)
    {
	fprintf(stderr, "getpeername: socket %d is not connected (state=%d)\n", 
		socket, cur_sock->state);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    *address_len= cur_sock->ops->get_addr_len();
    memcpy(address, &(cur_sock->raddr), *address_len);

    MU_RELEASE(&cur_sock->lock);

    return 0;
}

/*
** Return the local 'name' (i.e. address) of a (at least bound) socket 
** Locking: exclusive [for state and addrs].
*/
int getsockname(int socket, struct sockaddr *address, int *address_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "getsockname: socket %d has bad magic number "
		"(%#x != %#x)", socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if(cur_sock->state != SS_BOUND)
    {
	fprintf(stderr, "getsockname: socket %d is not bound (state=%d)\n", 
		socket, cur_sock->state);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    *address_len= cur_sock->ops->get_addr_len();
    memcpy(&(cur_sock->laddr), address, *address_len);

    MU_RELEASE(&cur_sock->lock);

    return 0;
}

int getsockopt(int socket, int level, int option_name, 
	       char *option_val, int *option_len)
{ 
    /* XXX not implemented */
    return -1;
}

int listen(int socket, int backlog)
{ 
    /* XXX not implemented */
    return -1;
}


/*
** recv: get 'length' bytes from a socket. The socket must already be
** connected.  (e.g. from previous accept, connect, etc)
** Locking: state only.
*/
int recv(int socket, char *buffer, int length, int flags)
{ 
    struct socket *cur_sock = (struct socket *)socket;
    int nbytes;

    TRC(fprintf(stderr, "recv called: socket=%d, buf=%p, len=%d, flags=%d\n",
	    socket, buffer, length, flags));

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "recv: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

   
    if(cur_sock->state != SS_BOUND && cur_sock->state != SS_CONNECTED)
    {
	fprintf(stderr, "recv: socket %d is not connected.\n", socket);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    MU_RELEASE(&cur_sock->lock);

    TRC(fprintf(stderr, "About to do proto recv on socket %x\n", socket));
    /* (this releases cur_sock->lock) */
    nbytes= cur_sock->ops->recv(cur_sock, buffer, length, 
				flags, (struct sockaddr *)0, 0);

    if(nbytes==-1)
	fprintf(stderr, "recv: protocol recv failed.\n");

    return nbytes;
}


/*
** recvfrom: get 'length' bytes from the address 'address' and store 'em 
** into the buffer. The socket must be bound.
** Locking: state only.
*/
int recvfrom(int socket, char *buffer, int length, int flags, 
	     struct sockaddr *address, int *address_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;
    int nbytes;

    TRC(fprintf(stderr, "recvfrom called: socket=%d, buf=%p, len=%d, flags=%d, and...\n", 
		socket, buffer, length, flags));
    TRC(fprintf(stderr, "               : address is %p, addr len is %p\n",
	 address, address_len));

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "recvfrom: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if(cur_sock->state != SS_BOUND && cur_sock->state != SS_CONNECTED)
    {
	fprintf(stderr, "recvfrom: socket %d is not bound.\n", socket);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    MU_RELEASE(&cur_sock->lock);

    TRC(fprintf(stderr, "About to do proto recv on socket %x\n", socket));
    /* (this releases cur_sock->lock) */
    nbytes= cur_sock->ops->recv(cur_sock, buffer, 
			       length, flags, address, address_len);
    
    if(nbytes==-1)
	fprintf(stderr, "recvfrom: protocol recv failed.\n");

    return nbytes;
}

int recvmsg(int socket, struct msghdr *msghdr, int flags)
{
    /* XXX not implemented */
    return -1;
}


/*
** Send 'length' bytes from the buffer 'message' on the named socket.
** The socket must be connected. 
** Locking: state only.
*/
int send(int socket, const char *message, int length, int flags)
{ 
    struct socket *cur_sock = (struct socket *)socket;
    int nbytes;

    TRC(fprintf(stderr, "send called: socket=%d, message=%p, len=%d, flags=%d\n", 
	    socket, message, length, flags));

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "send: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if(cur_sock->state != SS_CONNECTED && cur_sock->state != SS_BOUND)
    {
	fprintf(stderr, "send: socket %d is not connected.\n", socket);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    MU_RELEASE(&cur_sock->lock);

    nbytes= cur_sock->ops->send(cur_sock, message, length, flags, 
				(struct sockaddr *)0, 0);
    
    if(nbytes==-1)
	fprintf(stderr, "send: protocol send failed.\n");

    return nbytes;
}



/*
** Send 'length' bytes from the buffer 'message' on the named socket,
** to the address specified by 'dest_addr' and 'dest_len'.
** The socket must be bound.
** Locking: state only.
*/
int sendto(int socket, const char *message, int len, int flags, 
	   const struct sockaddr *dest_addr, int dest_len)
{ 
    struct socket *cur_sock = (struct socket *)socket;
    int nbytes;

    TRC(fprintf(stderr, "sendto called: socket=%d, message=%p, len=%d, "
	    "flags=%d, and...\n", socket, message, len, flags));
    TRC(fprintf(stderr, "             : address is %p, addr len is %d\n",
	    dest_addr, dest_len));

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "send: socket %d has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    MU_LOCK(&cur_sock->lock);

    if (cur_sock->state != SS_BOUND && cur_sock->state != SS_CONNECTED)
    {
	fprintf(stderr, "sendto: socket %d is not bound.\n", socket);
	MU_RELEASE(&cur_sock->lock);
	return -1;
    }

    MU_RELEASE(&cur_sock->lock);

    TRC(fprintf(stderr, "About to do proto send on socket %d\n", socket));
    nbytes= cur_sock->ops->send(cur_sock, message, len, flags,
				dest_addr, dest_len);

    if(nbytes==-1)
	fprintf(stderr, "sendto: protocol send failed.\n");

    return nbytes;
}

int setsockopt(int socket, int level, int option_name, 
	       const char *option_val, int option_len)
{ 
    fprintf(stderr, "Warning: unimplemented routine setsockopt called.\n");
    
    /* XXX not implemented */
    return -1;
}

int socket(int family, int type, int protocol)
{ 
    struct socket *sock;

    TRC(fprintf(stderr, "socket called: family=%d, type=%d, proto=%d\n",
	    family, type, protocol));

    if(family!=AF_INET) {
	/* we only support internet socket atm */
	fprintf(stderr, "socket: unsupported or bad family %d\n", family);
	return -1;
    }

    if(type!=SOCK_DGRAM && type!=SOCK_STREAM) {
	/* we only support datagrams atm */
	fprintf(stderr, "socket: unsupported or bad type %d\n", type);
	return -1;
    }

    /* guess the protocol if it's 0 */
    if (protocol == 0)
    {
	switch(type)
	{
	case SOCK_DGRAM:
	    protocol = IP_PROTO_UDP;
	    break;

	case SOCK_STREAM:
	    protocol = IP_PROTO_TCP;
	    break;

	default:
	    fprintf(stderr, "socket: unable to guess protocol type\n");
	    break;
	}
    }

    if(protocol!=IP_PROTO_UDP && protocol!=IP_PROTO_TCP) {
	fprintf(stderr, "socket: unsupported or bad protocol %d\n", protocol);
	return -1;
    }


    /* create the socket now! */
    sock = sock_alloc();
    if(!sock)
    {
	fprintf(stderr, "socket: out of sockets. bad news.\n");
	return -1;
    }

    TRC(fprintf(stderr, "socket: assigned socket number %d\n", (int)sock));

    sock->type= type;

    /* get the method table for this protocol */
    switch(protocol)
    {
    case IP_PROTO_UDP:
	sock->ops = (proto_ops *)NAME_FIND("modules>UDPSocketOps", addr_t);
	break;

    case IP_PROTO_TCP:
	sock->ops = (proto_ops *)NAME_FIND("modules>TCPSocketOps", addr_t);
	break;

    default:
	fprintf(stderr, "socket: NOTREACHED file %s line %d\n", 
		__FILE__, __LINE__);
	sock->ops = BADPTR;
	break;
    }

    if (sock->ops->init(sock) == -1)
    {
	fprintf(stderr, "socket: error while doing protocol-specific inits\n");
	return -1;
    }

    return (int) sock;
}



int socketpair(int family, int type, int protocol, int socket_vector[2])
{
    /* XXX not implemented */
    return -1;
}

/* Locking: exclusive (and may destroy muteces) */
int shutdown(int socket, int how)
{ 
    struct socket *cur_sock = (struct socket *)socket;
    
    TRC(fprintf(stderr, "socket_shutdown called on socket %d (how is %d)\n", socket, how));

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "shutdown: socket %d has bad magic number "
		"(%#x != %#x)",	socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }
    
    MU_LOCK(&cur_sock->lock);

    if(cur_sock->refs) {
	if(--cur_sock->refs == 0) { 
	    MU_RELEASE(&cur_sock->lock); /* There can be only one ... */
	    if(cur_sock->ops->close(cur_sock)==-1) {
		fprintf(stderr, "shutdown: proto close failed on socket %d\n",
			socket);
		return -1;
	    }
	    MU_FREE(&cur_sock->lock);
	    FREE(cur_sock);  /* free the socket structures */
	    return 0;
	}
    } else
	fprintf(stderr, "shutdown: socket %d is not referenced. botch.\n", 
		socket);

    MU_RELEASE(&cur_sock->lock);

    return 0;
}




/* ------------------------------------------------------------ */
/* Below here are some of the grungies. more speculative bits that
 * could only loosely be called part of the sockets code. It's mainly
 * support code. */

/*
 * XXX PRB Note that this select implementation can return prematurely
 * since it waits for ANY packets on the IO - this includes broadcasts
 * AND: well, not really, since the IO presumably has a packet filter
 * that ignores packets which aren't for this port. 
 * Locking: none. 
 */
int select(int nfds, 
	   fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	   struct timeval *timeout)
{
    int rc = 0;
    struct socket *sock;

    TRC(fprintf(stderr, "select: called\n"));

    /* We only know how to select on a single read FD */
    if (*readfds)
    {
	/* check socket for plausability */
	sock = (struct socket *)(*readfds);
	if (sock->magic != SOCK_MAGIC)
	{
	    fprintf(stderr, "select: socket %p has bad magic number "
		    "(%#x != %#x)", socket, sock->magic, SOCK_MAGIC);
	    return -1;
	}

	rc = sock->ops->select(sock, nfds, readfds,
			       writefds, exceptfds, timeout);
    }

    TRC(fprintf(stderr, "select: return %d\n", rc));
    return rc;
}



/* Here on in is _definitely_ support, and ultimately will need moving
 * to a better home */


/* XXX nasty hack!!! */
/* Locking: none [XXX should check state I reckon XXX] */
int close(int sock)
{
    struct socket *cur_sock= (struct socket *)sock;

    if (cur_sock->magic != SOCK_MAGIC)
    {
	fprintf(stderr, "close: socket %p has bad magic number (%#x != %#x)",
		socket, cur_sock->magic, SOCK_MAGIC);
	return -1;
    }

    TRC(fprintf(stderr, "socket close called\n"));

    cur_sock->ops->close((struct socket *)cur_sock);
    MU_FREE(&cur_sock->lock);
    FREE(cur_sock);
    return 0;
}


/* Sigh - NFS thinks it needs this. */
int ioctl(int fd, int request, ...)
{
    switch (request)
    {
    case FIONBIO:
	break;

    default:
	printf("Daisy, Daisy, give me your answer, do.\n");
	printf("Getting hazy; can't divide three from two.\n");
	printf("My answers; I can not see 'em-\n");
	printf("They are stuck in my Penti-um.\n");
	printf("I could be fleet,\n");
	printf("My answers sweet,\n");
	printf("With a workable CPU.\n");
	printf("  -- http://ps.cus.umist.ac.uk/~dar/pages/h2-comp/pentium.html\n");
	break;
    }

    return 0;
}


/* More cludges for NFS */


/* XXX DO NOT USE THIS!! It leaks memory: there is no work
 * around. This is only to support broken older software */

#ifdef CONFIG_NET_RESOLVER_SUPPORT
static int resolv(const char *name, uint32_t *addr)
{    
    ResolverMod_clp	rmod;
    Resolver_clp NOCLOBBER resolv;
    Net_IPHostAddrSeqP	addresses;
    Net_IPHostAddr	address;
    Net_IPHostAddr	*ns;
    int	NOCLOBBER	found;
    int			i;
    int	NOCLOBBER	err;

    err = 0;
    TRY {
	rmod = NAME_FIND("modules>ResolverMod", ResolverMod_clp);
	ns = NAME_FIND("svc>net>eth0>nameserver", Net_IPHostAddrP); 
	resolv = ResolverMod$New(rmod, ns);
    } CATCH_ALL {
	err = 1;
    } ENDTRY;
    if (err)
	return err;

    found = Resolver$Resolve(resolv, name, &addresses);

    TRC(printf("gethostbyname: found = %d\n", found));

    if (found == 1)
    {
	/* XXX just pick the first address returned */
	if (SEQ_LEN(addresses) < 1)
	{
	    printf("gethostbyname: no addresses back from resolver?\n");
	    return 2;
	}
	else
	{
	    address = SEQ_ELEM(addresses, 0);
	    (*addr) = address.a;
	    SEQ_FREE(addresses);
	}
    }
    else
    {
	printf("gethostbyname: error in resolving\n");
	return 3;
    }

    TRC(printf("gethostbyanme: resolv: std exit\n"));
    return 0;
}
#else
static int resolv(const char *name, uint32_t *addr)
{    
    printf("gethostbyname: resolver required, but not compiled in\n");
    return 1;
}
#endif /* CONFIG_NET_RESOLVER_SUPPORT */

struct hostent *gethostbyname(const char *name)
{
    unsigned int	a,b,c,d;
    struct hostent	*hent;
    uint32_t		address;

    /* first try the dotted quad stuff */
    if (sscanf(name, "%d.%d.%d.%d", &a,&b,&c,&d) == 4)
    {
	address = a | b<<8 | c<<16 | d<<24;
    }
    else /* try a full resolve */
    {
	if (resolv(name, &address))
	{
	    fprintf(stderr, "gethostbyname: resolver failure\n");
	    return NULL;
	}
    }

    TRC(printf("gethostbyname: got addr=%I\n", address));

    hent = Heap$Malloc(Pvs(heap), sizeof(struct hostent) + 2*sizeof(uint32_t));
    if (!hent)
    {
	fprintf(stderr, "gethostbyname: out of memory\n");
	return NULL;
    }

    hent->h_name = name;
    hent->h_aliases  = NULL;
    hent->h_addrtype = AF_INET;
    hent->h_length   = 4;  /* standard IP address */
    hent->h_addr_list= ((char **)&(hent->h_addr_list))+1;
    hent->h_addr_list[0] = address;
    hent->h_addr_list[1] = NULL;
    
    return hent;
}

/* Oh, yeah, steering clear of this would be a good move too */
struct protoent *getprotobyname(const char *proto)
{
    struct protoent *pent;

    pent = Heap$Malloc(Pvs(heap), sizeof(struct protoent));
    if (!pent)
    {
	fprintf(stderr, "getprotobyname: out of memory\n");
	return NULL;
    }

    if (!strcmp(proto, "udp"))
	pent->p_proto = IP_PROTO_UDP;
    else if (!strcmp(proto, "tcp"))
	pent->p_proto = IP_PROTO_TCP;
    else {
	fprintf(stderr, "getprotobyname: unknown protocol `%s'\n", proto);
	return NULL;
    }

    pent->p_name = proto;
    pent->p_aliases = NULL;

    return pent;
}

/* End of socket.c */
