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
*  File              : tcpu_usrreq.c                                       *
*  Author            : Torsten Braun                                       *
*  Last modification : July 27, 1995                                       *
\**************************************************************************/

#include <nemesis.h>
#include <stdio.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <time.h>
#include <ecs.h>
#include <netdb.h>
#include <FlowMan.ih>
#include "inet.h"
#include <Net.ih>
#include <IOMacros.h>
#include <salloc.h>
#include <HeapMod.ih>

void free_cb();

#define MAXPORT 8192
#define MINPORT 4096

int socket_tcpu_init();
int accept_tcpu();
int bind_tcpu();
int connect_tcpu();
int tcp_get_addr_len(void);
int recv_tcpu();
int send_tcpu();
int close_tcpu(struct socket *sock);
static int tcp_listen(struct socket *sock);
static int tcp_select(struct socket *sock, int nfds,
		      fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		      struct timeval *timeout);


const proto_ops tcp_ops= {
    socket_tcpu_init,
    accept_tcpu,
    bind_tcpu,
    connect_tcpu,
    tcp_get_addr_len,
    tcp_listen,
    recv_tcpu,
    send_tcpu,
    close_tcpu,
    tcp_select
};

const typed_ptr_t TCPSocketOps_export = TYPED_PTR(addr_t__code, &tcp_ops);

int socket_tcpu_init(struct socket *sock)
{
    struct tcp_data *td;
    struct tcpcb *tp;

    
    td = (struct tcp_data *)tcp_data_init(sock);
    
    BUFFOUTEND(sock) =  BUFF_OUT(sock)->buf + MAX_BUF;
    tp=GET_TCPCB(sock); 
    
    tp->t_state =  TCPS_CLOSED;
    tp->sock = sock;
    
    /* init timer */
    tp->t_timer[0]  = 0;
    tp->t_timer[1]  = 0;
    tp->t_timer[2]  = TCPTV_KEEP_INIT;
    tp->t_timer[3]  = 0;
    
    tp->t_template  = NULL;
    CONNECTED(sock) = 0;
  
    return ((int) sock);
}


/**************************************************************/
int bind_tcpu(struct socket *sock, const struct sockaddr *address, 
	      int addrlen)
{
    struct tcpcb *tp;
    struct sockaddr_in *inaddr= (struct sockaddr_in *)address;
    FlowMan_SAP sap;
    
    if((tp = GET_TCPCB(sock)) == NULL)
	return(-1);
    
    if(address->sa_family != AF_INET) {
	printf("tcp_bind: non INET family %d\n", address->sa_family);
	return -1;
    }

    sap.tag = FlowMan_PT_TCP;
    sap.u.TCP.port = inaddr->sin_port;
    sap.u.TCP.addr.a = inaddr->sin_addr.s_addr;

    if(!FlowMan$Bind(tp->fman, &sap)) {
	return -1;
    }
    /* remember the address and port we were allocated, and hack the
     * client's copy */
    inaddr->sin_port = sap.u.TCP.port;
    
    memcpy(&(sock->laddr), inaddr, sizeof(struct sockaddr_in));
    
    sock->state = SS_BOUND;
    
    printf ("TCPU_BIND DONE\n");
    return(1);

}


/* 
** listen not implemented (yet?)
*/
static int tcp_listen(struct socket *sock)
{
    printf("XXX bad call listen on tcp socket.\n");
    return -1;
}


static int tcp_select(struct socket *sock, int nfds,
		      fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		      struct timeval *timeout)
{
    printf("XXX select() on tcp socket unimplemented\n");
    return -1;
}
    


/**************************************************************/
int connect_tcpu(struct socket *sock, struct sockaddr_in *servaddr, 
		 int addrlen)
{
    struct tcpcb *tp;
    int error, res, timer;
    struct timeval timeout;
    struct sockaddr_in *laddr = (struct sockaddr_in *)(&sock->laddr);
    
    if((tp = GET_TCPCB(sock)) == NULL)
	return(-1);

    if (sock->state != SS_BOUND)
    {
	printf("tcp_connect: socket is not in bound state (%d != %d)\n",
		sock->state, SS_BOUND);
	return -1;
    }
    
    tcp_stack(tp,(struct sockaddr_in *)(&sock->laddr),servaddr);

    *(struct sockaddr_in *)(&tp->sock->raddr) = *servaddr;

/* Save the client and server addresses in the template, minimizing
   work when sending a packet to the server */
    
    tp->t_template->ti_dst.s_addr = servaddr->sin_addr.s_addr;
    tp->t_template->ti_dport = servaddr->sin_port;
    
    tp->t_template->ti_src.s_addr = laddr->sin_addr.s_addr;
    tp->t_template->ti_sport = laddr->sin_port;
  
/*
    tcpstat.tcps_connattempt++;
*/
    tp->t_template->ti_pr=IP_PROTO_TCP;
    tp->t_state                 = TCPS_SYN_SENT;
    tp->t_timer[TCPT_KEEP]      = TCPTV_KEEP_INIT;
    tp->iss                     = (tcp_seq) NOW();
    tcpu_sendseqinit(tp);

    if((error = tcpu_output(&tp)) < 0)
	return(-1);

  connect:
    timeout.tv_sec = 0;
    timeout.tv_usec = SLOW_TIMO;
    res=get_res(tp,&timeout);
    switch(res) 
    {
	case EVENT_ERROR: return(-1);
	case EVENT_TIMER: {
	    if((timer = slow_timo(tp->sock)) <= TCPT_NTIMERS)
		tcpu_timers(tp, timer);
	    else
		goto connect;  
	    break;
	}
	default:{
	    tcpu_input(&tp,res);
	    if(tp->t_state == TCPS_ESTABLISHED) {
			    CONNECTED(sock) = 1;
			    sock->state = SS_CONNECTED;
                            printf ("\n\nconnect_tcpu: good return\n\n\n");
			    return(1);
		}
	}
    }
    if((tp->t_state == TCPS_SYN_RECEIVED) || (tp->t_state == TCPS_SYN_SENT )) {
	goto connect;
    }
    
    
    printf ("connect_tcpu: bad return\n");
    PAUSE(TENTHS(1));
    return(-1); 
}


/*****************************************************************/
int accept_tcpu(sockfd, from, fromlen)
    struct socket *sockfd;
    struct sockaddr_in *from;
    int *fromlen;
{
    struct tcpcb *tp;
    int res, timer;
    struct timeval timeout;
    
    if((tp = GET_TCPCB(sockfd)) == NULL)
	return(-1);
    
    tp->t_state = TCPS_LISTEN ;

    tcp_stack(tp,(struct sockaddr_in *)&sockfd->laddr,NULL);

    tp->t_template->ti_dst.s_addr = from->sin_addr.s_addr;
    tp->t_template->ti_dport = from->sin_port;
    
    while (tp->t_state != TCPS_ESTABLISHED ) {

    listen:
	timeout.tv_sec = 0;
	timeout.tv_usec = SLOW_TIMO;
	res=get_res(tp,&timeout);
	switch(res) 
	{
	case EVENT_ERROR: return(-1);
	case EVENT_TIMER: 
	{
	    if(tp->t_state == TCPS_LISTEN)
		goto listen;
	    else {
		if((timer = slow_timo(tp->sock)) <= TCPT_NTIMERS) {
                    printf ("TIMER........\n");
		    tcpu_timers(tp, timer);
                }
		else
		    goto listen;
		break;
	    }
	}
	
	default: 
	    TRC(eprintf("Calling tcpu_input(accept)\n"));
	    tcpu_input(&tp,res);
	    
	    if(tp->t_state == TCPS_ESTABLISHED)
	    {
		struct socket *newsock;
		addr_t tmp;
		struct tcpcb *newtp;

		/* Rebuild the stack for this socket */
		*from = *(struct sockaddr_in *)(&tp->sock->raddr);
		*fromlen = sizeof(struct sockaddr_in);

		/* Flush back the waiting packet, if necessary */

		flush_rx_packet(tp);

		/* Allocate a socket for the new connection */

		newsock = (struct socket *)socket(AF_INET, SOCK_STREAM, 0);
		bind((int)newsock, &sockfd->laddr, sizeof(struct sockaddr_in));

		newtp = GET_TCPCB(newsock);

		TRC(eprintf("tcp %p: accepted: new tp = %p\n",
			    tp, newtp));
		
		/* Copy the old connection info across to the new
                   socket, and reset the state of the old one */

		newtp->conn_st = tp->conn_st;
		tp->conn_st.stack_built = False;
		
		/* Build the new connection */

		tcp_stack(tp, (struct sockaddr_in *)&sockfd->laddr, from);

		/* XXX Swizzle the internal states ... */

		tmp = sockfd->data;
		sockfd->data = newsock->data;
		newsock->data = tmp;

		newsock->raddr = sockfd->raddr;
		
		/* Transfer any packets as necessary between the two
                   connections */

		{
		    IO_Rec rec;
		    uint32_t nrecs;
		    word_t value;

		    tcp_rx_block *r = tp->rx_blocks.next;

		    while(r != &tp->rx_blocks) {
			tcp_rx_block *next = r->next;

			TRC(eprintf("Checking stored data rec %p\n", r));
			if(!matches_hostport((addr_t)(r + 1), tp)) {
			    TRC(eprintf("Transferring stored data rec %p\n", r));
			    LINK_REMOVE(r);
			    LINK_ADD_TO_TAIL(&newtp->rx_blocks, r);
			}
			r = next;
		    }

		    while(IO$GetPkt(newtp->conn_st.rxio, 1, &rec, 
				    0, &nrecs, &value)) {
			TRC(eprintf("Sucked data rec\n"));

			    r = Heap$Malloc(Pvs(heap), 
					    sizeof(*r) + rec.len);
			    
			    memcpy((addr_t)(r+1), rec.base, rec.len); 
			    
			    if(matches_hostport(rec.base, tp)) {
				TRC(eprintf("Transferring data rec %p\n", r));
				LINK_ADD_TO_TAIL(&tp->rx_blocks, r);
			    } else {
				TRC(eprintf("Not transferring data rec %p\n", r));
				LINK_ADD_TO_TAIL(&newtp->rx_blocks, r);
			    }

			    TRC(eprintf("tcp_accept(): Putting packet %p"
					" down IO channel\n", rec.base));
			    
			    IO$PutPkt(newtp->conn_st.rxio, 1, &rec, 
				      value, FOREVER);
			    
		    }

		}
		
		/* If there were any SYN packets waiting because they
                   had the wrong connection id on them, transfer them
                   to the main rx queue */

		while(!LINK_EMPTY(&tp->syn_rx_blocks)) {
		    tcp_rx_block *r = tp->syn_rx_blocks.next;
		    eprintf("Transferring syn_rx_block %p\n", r);
		    LINK_REMOVE(r);
		    LINK_ADD_TO_TAIL(&newtp->syn_rx_blocks, r);
		}

		

		tp->sock = newsock;
		newtp->sock = sockfd;

		

		CONNECTED(newsock) = 1;
		newsock->state = SS_CONNECTED;

		return((int)newsock);
	    }
	    
	    break;
	}
    }
    return(-1); 
}

/*****************************************************************/
int send_tcpu(sockfd, data, msglen, flags)
    struct socket *sockfd;
    char *data;
    int msglen;
    int flags;
{
    struct tcpcb *tp;
    int offset, space, nbsend, res, timer;
    int    start;
    int data_ptr = 0;
    int sent = 0;
    struct timeval timeout;
    
    MU_RELEASE(&sockfd->lock);
    
    if(msglen > MAX_BUF) 
	return(-1);
    
    if((tp = GET_TCPCB(sockfd)) == NULL) {
	printf("send_tcpu : get socket failure : %p\n", sockfd);
	return(-1);
    }
    
    if((tp->t_state > TCPS_ESTABLISHED)||(tp->sock == 0))
	return(-1);
    
    if (flags & TF_NODELAY)          /*nodelay flag 16.2.95 */
    {
	tp->t_flags |= TF_NODELAY;
    }
    else
    {
	tp->t_flags &= ~TF_NODELAY;
    }
    
    nbsend = msglen;
    do
    {

	space = MAX_BUF - BUFF_OUT(tp->sock)->nbeltq - 4;
	if (space < nbsend)
	{
	    nbsend = space;  
	    if (nbsend == 0)
		goto wait_ack;
	}
	start =  ((BUFF_OUT(tp->sock)->una + BUFF_OUT(tp->sock)->nbeltq)%MAX_BUF); 
	if(start + nbsend <= MAX_BUF)
	{
	    memcpy((char *)(BUFF_OUT(tp->sock)->buf) + start, 
		   data + data_ptr, nbsend);
	} else {
	    offset = MAX_BUF-start;
	    memcpy((char *)(BUFF_OUT(tp->sock)->buf) + start, 
		   data + data_ptr, offset);
	    memcpy((char *)(BUFF_OUT(tp->sock)->buf), 
		   data + offset + data_ptr, nbsend-offset); 
	}
	BUFF_OUT(tp->sock)->nbeltq  +=  nbsend;
	data_ptr = nbsend;
	sent += nbsend;
	nbsend = msglen - sent;
	tcpu_output(&tp);
	
    wait_ack:
	if(space <= TCP_MSS) { /* modified: was TCP_MAX_SIZE, 24.5.95 tb */
/*
  FD_ZERO(&mask);
  FD_SET(tp->sock, &mask);
  */
	    timeout.tv_sec = 0;
	    timeout.tv_usec = SLOW_TIMO;
/*
  res = select(tp->sock +1, &mask, (int *)0, (int *)0, &timeout); 
  */
	    res=get_res(tp,&timeout);
	    switch(res) 
	    {
	    case EVENT_ERROR: return(-1);
	    case EVENT_TIMER: {
		if((timer = slow_timo(tp->sock)) <= TCPT_NTIMERS)
		    tcpu_timers(tp, timer);
		break;
	    }
	    default: 
	    {
		TRC(eprintf("Calling tcpu_input(send1)\n"));
		tcpu_input(&tp,res);
		break;
	    }
	    }
	} else {
	again:
	    /*
	      FD_ZERO(&mask);
	      FD_SET(tp->sock, &mask);
	      */
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	    /*
	      res = select(tp->sock +1, &mask, (int *)0, (int *)0, &timeout); 
	      */
	    res=get_res(tp,&timeout);
	    switch(res) 
	    {
	    case EVENT_ERROR: 
		return(-1);
	    case EVENT_TIMER: 
		break;
	    default: {
		    TRC(eprintf("Calling tcpu_input(send2)\n"));
		    tcpu_input(&tp,res);
		    goto again; 
		}
	    }
	} 
    } while(sent < msglen);
    return(msglen);
}

/*********************************************************/
int recv_tcpu(sockfd, recbuf, max_data_size, flags)
    struct socket *sockfd;
    char *recbuf;
    int max_data_size;
    int flags;
{
    struct tcpcb *tp;
    int offset, last_msg, res, timer;
    struct timeval timeout;
    struct datapointer *data_ptr;
    struct buffin *buff_in_ptr;
    
    MU_RELEASE(&sockfd->lock);
    
    if((tp = GET_TCPCB(sockfd)) == NULL)
	{printf("recv_tcpu : get socket failure : %p\n", sockfd);
         PAUSE(SECONDS(2));
	return(-1);}
    
    if(max_data_size < TCP_MAX_SIZE){
	printf("Can't return data: receive buffer too small\n");
	return(-1);
    }
    
    data_ptr = (DATA_PT(sockfd));
    buff_in_ptr = BUFF_IN(sockfd);
    
    if(tp->t_state != TCPS_ESTABLISHED)
	return(0);
    
    do {

	timeout.tv_sec = 0;
	timeout.tv_usec = SLOW_TIMO;
	
	res=get_res(tp,&timeout);
	
	switch(res) 
	{
	case EVENT_ERROR: 
	    return(-1);
	case EVENT_TIMER:{
	    if((timer = slow_timo(tp->sock)) <= TCPT_NTIMERS) {
		tcpu_timers(tp, timer);
		break;
	    }
#if 0
	    Why should it do this bit????????
				 else{
				     tp->t_flags |= TF_ACKNOW;
				     tcpu_output(&tp);
				     tp->time_ack = 0;
				     break;
				 }
#endif
	    break;
	}
	default: 
	{
/******
  data_ptr->pointer = recbuf;
  fields updated by tcpu_input
  ********/
	    data_ptr->length = 0; /* RS: THIS IS NEEDED */
	    tcpu_input(&tp,res);
	    if(data_ptr->length != 0) {
		buff_in_ptr->nbeltq -= data_ptr->length;  
		if (buff_in_ptr->nbeltq < 0) {
		    buff_in_ptr->nbeltq = 0;  /* modified 7.3.95 */
		}
		memcpy (recbuf,data_ptr->pointer,data_ptr->length);
		if (tp->t_state== TCPS_ESTABLISHED) {
		    tp->t_flags |= TF_ACKNOW;
		    tcpu_output(&tp);
		}
		return(data_ptr->length);
	    }
	    else if((buff_in_ptr->nbeltq < max_data_size) &&
		    (tp->t_state > TCPS_ESTABLISHED))
	    {
		if(buff_in_ptr->nbeltq != 0) {
		    offset = MAX_BUF - buff_in_ptr->next;
		    if(offset >= buff_in_ptr->nbeltq)
			memcpy(recbuf, buff_in_ptr->buf +
			       buff_in_ptr->next, buff_in_ptr->nbeltq);
		    else
		    {
			memcpy(recbuf, 
			       buff_in_ptr->buf + buff_in_ptr->next, 
			       offset);
			memcpy(recbuf + offset, 
			       buff_in_ptr->buf, 
			       buff_in_ptr->nbeltq - offset);
		    }
		    last_msg = buff_in_ptr->nbeltq; 
		    buff_in_ptr->nbeltq = 0;
		    tp->t_flags |= TF_ACKNOW;
		    tcpu_output(&tp);
		    return(last_msg);			
		}
	    } 
	    else if (buff_in_ptr->nbeltq > 0){
		offset = MAX_BUF - buff_in_ptr->next;
		if(offset >= buff_in_ptr->nbeltq)
		    memcpy(recbuf, 
			   buff_in_ptr->buf + buff_in_ptr->next, 
			   buff_in_ptr->nbeltq);
		else
		{
		    memcpy(recbuf, buff_in_ptr->buf +
			   buff_in_ptr->next, offset);
		    memcpy(recbuf + offset, buff_in_ptr->buf, 
			   buff_in_ptr->nbeltq - offset);
		}
		offset = buff_in_ptr->nbeltq;
		buff_in_ptr->nbeltq = 0;
		tp->t_flags |= TF_ACKNOW;
		tcpu_output(&tp); 			 
		return(offset);
	    }
	}
	}
    } while(tp->t_state == TCPS_ESTABLISHED);
    return(0);
}


/******************************************************/
int close_tcpu(sockfd)
    struct socket *sockfd;
{
    struct tcpcb *tp;
    int res, timer;
    struct timeval timeout;
    
    if((tp = GET_TCPCB(sockfd)) == NULL)
	return(-1);

    while(BUFF_IN(sockfd)->nbeltq > 0 || 
	  (BUFF_OUT(sockfd)->nbeltq - BUFF_OUT(sockfd)->nbeltsnd) > 0) {
    
	tp->t_flags |= TF_ACKNOW;
	tcpu_output(&tp);
    }
    
begin:      
    if((tp->t_state == TCPS_SYN_RECEIVED) || 
       (tp->t_state == TCPS_ESTABLISHED)) {
	tp->t_state = TCPS_FIN_WAIT_1;
	tcpu_output(&tp);
    }
    
    if((tp->t_state == TCPS_SYN_SENT)|| (tp->t_state == TCPS_CLOSED)|| 
       (tp->sock==0))
    {
	tp->t_state = TCPS_CLOSED;
	tcpu_close(tp);
/***
  (void)free_cb(tp,sockfd);
  ****/
	return(1);
    }
    
    if(tp->t_state == TCPS_CLOSE_WAIT) {
	tp->t_state = TCPS_LAST_ACK;
	tcpu_output(&tp);
        printf ("CLOSE TCPU, old state = LAST_ACK, new state = %s\n", 
		tcpstates[tp->t_state]);
    }
    
    if(tp->t_state <= TCPS_TIME_WAIT) {
	
        /*
	  FD_ZERO(&mask);
	  FD_SET(tp->sock, &mask);
	  */
	timeout.tv_sec = 0;
	timeout.tv_usec = SLOW_TIMO;
	
        /*
	  res = select(tp->sock +1, &mask, (int *)0, (int *)0, &timeout); 
	  */
        res=get_res(tp,&timeout);
	switch(res) 
	{
	case EVENT_ERROR: return(-1);
	case EVENT_TIMER: {
	    
	    if((timer = slow_timo(tp->sock)) <= TCPT_NTIMERS) {
		tcpu_timers(tp, timer);
		return (1);
	    }
	    break;
	}
	default: {
	    TRC(eprintf("tcp: Calling tcpu_input(close)\n"));
	    tcpu_input(&tp,res);
	    break;
	}
	}
    }
    
    if(tp->t_state == TCPS_TIME_WAIT)
    {
	if(tp->sock == 0)
	{
	    /***
	      (void)free_cb(tp,sockfd);
	      ***/
	    return(1);
	}
    }
    goto begin;
}

/*  RS: This func. is to be called upon destruction of the socket,... somehow... */
void free_cb(tp,sockfd)
    struct tcpcb *tp;
    struct socket *sockfd;
{
    Protocol$Dispose((Protocol_clp)tp->conn_st.ip);
    Heap$Free(Pvs(heap),sockfd->data);
}


int tcp_get_addr_len(void)
{
    return sizeof(struct sockaddr_in);
}


static IO_clp bindIO(IOOffer_cl *io_offer, HeapMod_cl *hmod, Heap_clp *heap) 
{
    IO_clp res; 
    IOData_Shm *shm;
    
    if(io_offer != NULL) {

	if(*heap) {   /* We have some memory => bind using it */
	    Type_Any    any;
	    IDCClientBinding_clp cb;

	    shm = SEQ_NEW(IOData_Shm, 1, Pvs(heap));
	    
	    SEQ_ELEM(shm, 0).buf.base = 
		HeapMod$Where(hmod, *heap, &(SEQ_ELEM(shm, 0).buf.len));
	    SEQ_ELEM(shm, 0).param.attrs  = 0;
	    SEQ_ELEM(shm, 0).param.awidth = PAGE_WIDTH;
	    SEQ_ELEM(shm, 0).param.pwidth = PAGE_WIDTH;
	    
	    cb = IOOffer$ExtBind(io_offer, shm, Pvs(gkpr), &any);
	    if(!ISTYPE(&any, IO_clp)) {
		eprintf("tcpu_usrreq.c [bindIO]: IOOffer$ExtBind failed.\n");
		if(cb) IDCClientBinding$Destroy(cb);
		RAISE_TypeSystem$Incompatible();
	    }
	    res = NARROW (&any, IO_clp);
	} else { 
	    /* Just bind to offer and create a heap afterwards. */
	    res = IO_BIND(io_offer);
	    shm = IO$QueryShm(res); 
	    if(SEQ_LEN(shm) != 1) 
		eprintf("tcpu_usrreq.c [bindIO]: "
			"got > 1 data areas in channel!\n");

	    /* Ignore extra areas for now */
	    *heap = HeapMod$NewRaw(hmod, SEQ_ELEM(shm, 0).buf.base, 
				   SEQ_ELEM(shm, 0).buf.len);
	}

	return res; 
    } 
    
    return (IO_cl *)NULL;
}


int tcp_stack(struct tcpcb *tp, struct sockaddr_in *local, 
	      struct sockaddr_in *remote) {

    IOOffer_clp rxoffer,txoffer;
    EtherMod_cl *ethmod = NAME_FIND("modules>EtherMod", EtherMod_clp);
    IPMod_cl *ipmod = NAME_FIND("modules>IPMod", IPMod_clp);
    Net_EtherAddr *shwaddr, dhwaddr = { {0} };
    char buf[32];
    FlowMan_SAP lsap, rsap;

    /*sockets get best effort*/
    Netif_TXQoS         txqos = {SECONDS(30), 0, True}; 

    ProtectionDomain_cl *NOCLOBBER driver_pdom;
    string_t NOCLOBBER intfname;
    uint32_t gateway;
    
/* 
   BUILD THE STACK, IMPORT THE OFFERS FOR TX AND RX CHANNELS, INSTALL THE
   PACKET FILTERS, WRITE THE TX and RX channel closures into tp. These channels
   will be used by tcpu_input and tcpu_output
   All this will go directly over ethernet. For the moment...
   */
    

    if(tp->conn_st.stack_built) {
	goto stack_built;
    }

    rsap.tag = FlowMan_PT_TCP;
    lsap.tag = FlowMan_PT_TCP;
    
    rsap.u.TCP.addr.a = (remote)? remote->sin_addr.s_addr : 0;
    rsap.u.TCP.port   = (remote)? remote->sin_port : 0;
    lsap.u.TCP.addr.a = local->sin_addr.s_addr;
    lsap.u.TCP.port   = local->sin_port;

    /* If we have a remote address then this is for an established
       connection - otherwise it's for a listen connection */

    if(remote) {

	FlowMan_RouteInfo * NOCLOBBER ri;
	TRY {
	    ri = FlowMan$ActiveRoute(tp->fman, &rsap);
	    
	} CATCH_FlowMan$NoRoute() {
	    ri = NULL;
	} ENDTRY;

	if(!ri) {
	    printf("build_stack: no route to host %x\n",
		   ntohl(rsap.u.TCP.addr.a));
	    return -1;
	}

	TRC(printf("tcp_stack: routed %I to intf %s, gateway %I\n",
		   rsap.u.TCP.addr.a, ri->ifname, ri->gateway));

	intfname = ri->ifname;
	gateway = ri->gateway;
	FREE(ri->type);
	FREE(ri);
    } else {

	int i;
	FlowMan_RouteInfo *ri;
	FlowMan_RouteInfoSeq * NOCLOBBER ris = NULL;

	TRY{
	    ris = FlowMan$PassiveRoute(tp->fman, &lsap);
	} CATCH_FlowMan$NoRoute() {
	} ENDTRY;

	if (!ris)
	{
	    printf("tcp_stack: address %x not on a local interface\n",
		   ntohl(lsap.u.UDP.addr.a));
	    return -1;
	}

	/* XXX for the moment, pick the first valid interface and use that */
	ri = &SEQ_ELEM(ris, 0);
	intfname = strdup(ri->ifname);
	gateway = ri->gateway;

	TRC(printf("build_stack: %x is on intf %s\n",
		   ntohl(lsap.u.TCP.addr.a), intfname));	

	for(i = 0; i<SEQ_LEN(ris); i++) {
	    ri = &SEQ_ELEM(ris, i);
	    FREE(ri->ifname);
	    FREE(ri->type);
	}

	SEQ_FREE(ris);
    }

    sprintf(buf, "svc>net>%s>ipaddr", intfname);

    lsap.u.TCP.addr = *(NAME_FIND(buf, Net_IPHostAddrP));
    local->sin_addr.s_addr = lsap.u.TCP.addr.a;

    sprintf(buf, "svc>net>%s>mac", intfname);
    shwaddr = NAME_FIND(buf, Net_EtherAddrP);

    if (remote) {
	uint32_t NOCLOBBER addr;
	
	addr = gateway ? gateway : remote->sin_addr.s_addr;
	
	TRY {
	    FlowMan$ARP(tp->fman, addr, &dhwaddr);
	} CATCH_Context$NotFound(UNUSED name) {
	} ENDTRY;
	
    } 

    tp->conn_st.flow = 
	FlowMan$Open(tp->fman, intfname, &txqos, Netif_TXSort_Socket, 
		     &rxoffer, &txoffer);
    
    if(!tp->conn_st.flow) {
	printf("tcp_build_stack: FlowMan$Open failed\n");
	return -1;
    }

    tp->conn_st.cid = FlowMan$Attach(tp->fman, tp->conn_st.flow, &lsap, &rsap);
    if(!tp->conn_st.cid) {
	printf("tcp_build_stack: FlowMan$Attach failed\n");
	return -1;
    }

    driver_pdom = IDCOffer$PDID((IDCOffer_cl *)rxoffer);

    {
	/* Evil - see if we're using a 3c509 or other unspecified card */

	Context_clp domains;
	Context_Names *domids;
	int i;
	domains = NAME_FIND("proc>domains", Context_clp);

	domids = Context$List(domains);
	for(i = 0; i < SEQ_LEN(domids); i++) {
	    ProtectionDomain_ID pdom;
	    Context_clp dom;
	    string_t domName;
	    
	    dom = NAME_LOOKUP(SEQ_ELEM(domids, i), domains, Context_clp);
	    pdom = NAME_LOOKUP("protdom", dom, ProtectionDomain_ID);
	    domName = NAME_LOOKUP("name", dom, string_t);

	    if(pdom == driver_pdom) {
		if(!strcmp(domName, "eth3c509")) {
		    tp->conn_st.useMultipleDataRecs = True;
		} else {
		    tp->conn_st.useMultipleDataRecs = False;
		}
		break;
	    }

	}
	if(i == SEQ_LEN(domids)) {
	    eprintf("tcp_stack(): Couldn't find driver in domains list!\n");
	    tp->conn_st.useMultipleDataRecs = False;
	}    
    }

    {
	HeapMod_clp hmod = NAME_FIND("modules>HeapMod", HeapMod_clp);
	IO_clp      rxio, txio;

	rxio = bindIO(rxoffer, hmod, &tp->conn_st.rxheap);
	txio = bindIO(txoffer, hmod, &tp->conn_st.txheap);

	tp->conn_st.eth = EtherMod$New(ethmod, &dhwaddr, shwaddr, FRAME_IPv4,
				       rxio, txio);
    }

    {
	Net_IPHostAddr dst, src;
	
	dst.a = (remote)? remote->sin_addr.s_addr : 0;
	src.a = local->sin_addr.s_addr;

	tp->conn_st.ip=IPMod$Piggyback(ipmod,
				       (Protocol_clp)tp->conn_st.eth,
				       &dst,
				       &src,
				       IP_PROTO_TCP,
				       127,
				       tp->conn_st.flow,
				       tp->conn_st.txheap);

	IP$SetDF(tp->conn_st.ip,True);
    
    }

    tp->conn_st.txio     = Protocol$GetTX((Protocol_clp)tp->conn_st.ip);
    tp->conn_st.rxio     = Protocol$GetRX((Protocol_clp)tp->conn_st.ip);
    tp->conn_st.ipHdrLen = Protocol$QueryHeaderSize(
	(Protocol_clp)tp->conn_st.ip);

    {
	/* prime the RX pipe. XXX For now, don't use the Gatekeeper
           heap since it doesn't set permissions properly when a heap
           expands */
	
	IO_Rec rec;
	addr_t data;
	int i;
	int recSize = tp->conn_st.ipHdrLen + TCP_MAX_SIZE;


	data = Heap$Malloc(tp->conn_st.rxheap, (PRIME_DEPTH * recSize));
	
	for(i=0; i < PRIME_DEPTH; i++)
	{
	    rec.base = data;
	    rec.len  = recSize;
	    TRC(eprintf("tcp prime: Putting packet %p down IO channel\n", 
			rec.base));

	    IO$PutPkt(tp->conn_st.rxio, 1, &rec, 0, FOREVER);

	    data += recSize;
	}
    }

    tp->conn_st.stack_built = True;
    
stack_built:

    if (tp->wiov[0].base==0) {
	int frag0len = tp->conn_st.ipHdrLen + sizeof(struct tcphdr);
	TRC(printf("Allocating frag0 length %u\n", frag0len));
	tp->wiov[0].base = Heap$Malloc(Pvs(heap), frag0len);
	tp->wiov[0].len = frag0len;
    }


    if(!tp->t_template) {
	tp->t_template = tcp_template(tp);
    }

    return 0;
}
