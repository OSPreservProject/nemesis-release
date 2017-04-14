/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      netcons.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Network console.  List on a port number related to the domain ID, 
**	and if we get a UDP packet, redirect our future output to the
**	source of the packet.
** 
** ENVIRONMENT: 
**
**      Initialised by the Builder
** 
** ID : $Id: netcons.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
** */


#include <nemesis.h>
#include <socket.h>
#include <VPMacros.h>

#include <VP.ih>
#include <Rd.ih>
#include <Wr.ih>
#include <WrRedir.ih>
#include <WrRedirMod.ih>
#include <RdRedir.ih>
#include <RdRedirMod.ih>
#include <Heap.ih>
#include <Threads.ih>

#include <stdio.h>
#undef EOF		/* grr! */

#ifdef NETCON_RXTRACE
#define RXTRC(x) x
#else
#define RXTRC(x)
#endif

#ifdef NETCON_TXTRACE
#define TXTRC(x) x
#else
#define TXTRC(x)
#endif

/* port number to start console output from */
#define NETCONS_BASE  4000

/* how many charaters we're prepared to rx in a single packet */
#define RXBUFSZ       1024
#define TXBUFSZ	      1024

/* forward decl */
struct _netcon_st;

typedef struct {
    mutex_t	txmu;
    bool_t	buffered;
    int8_t 	*txbuf;
    int		txlen;
    struct sockaddr_in	dstaddr;
    struct _netcon_st *netcon;
} netwr_st;

typedef struct {
    mutex_t	mu;
    condition_t	newdata;	/* signalled when new data arrives in buffer */
    int		rxblocked;	/* number of threads blocked on cond var */
    condition_t	spaceavail;	/* signalled when free space arrived */
    int		txblocked;	/* number of threads blocked on tx */
    int8_t 	*rxbuf;
    int		rxfree;		/* chars free in buffer */
    int		head;		/* new data arrives at rxbuf[head] */
    int		tail;		/* client reads from rxbuf[tail] */
    struct _netcon_st *netcon;
} netrd_st;

/* Idea here is to have a thin layer of a writer, allowing the
 * rx_thread to switch which real writer is used by changing the state
 * ->wr field to be something else.  Maybe there should be a special
 * type of writer, which does just this. It seems to be a fairly
 * common requirement. */

/* Thread state */
typedef struct _netcon_st {
    int		sock;	/* socket to monitor for activity */
    char	*rxbuf; /* buffer to receive into */
    /* net cons stub closures */
    Wr_cl	netout;
    Wr_cl	neterr;
    Rd_cl	netin;
    /* net cons state */
    netwr_st	outst;
    netwr_st	errst;
    netrd_st	inst;
    /* redirectors */
    WrRedir_cl	*out;
    WrRedir_cl	*err;
    RdRedir_cl	*in;
    /* serial outputs */
    Wr_cl	*serialout;
    Wr_cl	*serialerr;
    Rd_cl	*serialin;
    char	errtxbuf;  /* single char tx hold `buffer' for stderr */
} netcon_st;

/* prototypes */
static void rx_thread(netcon_st *st);


/* This is the network output writer */
/*************************************************************/
static  Wr_PutC_fn              NetWr_PutC_m;
static  Wr_PutStr_fn            NetWr_PutStr_m;
static  Wr_PutChars_fn          NetWr_PutChars_m;
static  Wr_Seek_fn              NetWr_Seek_m;
static  Wr_Flush_fn             NetWr_Flush_m;
static  Wr_Close_fn             NetWr_Close_m;
static  Wr_Length_fn            NetWr_Length_m;
static  Wr_Index_fn             NetWr_Index_m;
static  Wr_Seekable_fn          NetWr_Seekable_m;
static  Wr_Closed_fn            NetWr_Closed_m;
static  Wr_Buffered_fn          NetWr_Buffered_m;
static  Wr_Lock_fn              NetWr_Lock_m;
static  Wr_Unlock_fn            NetWr_Unlock_m;
static  Wr_LPutC_fn             NetWr_LPutC_m;
static  Wr_LPutStr_fn           NetWr_LPutStr_m;
static  Wr_LPutChars_fn         NetWr_LPutChars_m;
static  Wr_LFlush_fn            NetWr_LFlush_m;

static  Wr_op   netwr_ms = {
    NetWr_PutC_m,
    NetWr_PutStr_m,
    NetWr_PutChars_m,
    NetWr_Seek_m,
    NetWr_Flush_m,
    NetWr_Close_m,
    NetWr_Length_m,
    NetWr_Index_m,
    NetWr_Seekable_m,
    NetWr_Closed_m,
    NetWr_Buffered_m,
    NetWr_Lock_m,
    NetWr_Unlock_m,
    NetWr_LPutC_m,
    NetWr_LPutStr_m,
    NetWr_LPutChars_m,
    NetWr_LFlush_m
};


static Rd_GetC_fn	NetRd_GetC_m;
static Rd_EOF_fn 	NetRd_EOF_m;
static Rd_UnGetC_fn 	NetRd_UnGetC_m;
static Rd_CharsReady_fn NetRd_CharsReady_m;
static Rd_GetChars_fn	NetRd_GetChars_m;
static Rd_GetLine_fn 	NetRd_GetLine_m;
static Rd_Seek_fn 	NetRd_Seek_m;
static Rd_Close_fn 	NetRd_Close_m;
static Rd_Length_fn 	NetRd_Length_m;
static Rd_Index_fn 	NetRd_Index_m;
static Rd_Intermittent_fn NetRd_Intermittent_m;
static Rd_Seekable_fn 	NetRd_Seekable_m;
static Rd_Closed_fn	NetRd_Closed_m;
static Rd_Lock_fn	NetRd_Lock_m;
static Rd_Unlock_fn 	NetRd_Unlock_m;
static Rd_LGetC_fn 	NetRd_LGetC_m;
static Rd_LEOF_fn 	NetRd_LEOF_m;
Rd_op netrd_ms = {
    NetRd_GetC_m,
    NetRd_EOF_m,
    NetRd_UnGetC_m,
    NetRd_CharsReady_m,
    NetRd_GetChars_m,
    NetRd_GetLine_m,
    NetRd_Seek_m,
    NetRd_Close_m,
    NetRd_Length_m,
    NetRd_Index_m,
    NetRd_Intermittent_m,
    NetRd_Seekable_m,
    NetRd_Closed_m,
    NetRd_Lock_m,
    NetRd_Unlock_m,
    NetRd_LGetC_m,
    NetRd_LEOF_m,
};

static Closure_Apply_fn Init; 
static Closure_op ms = { Init }; 
static const Closure_cl cl = {&ms, NULL}; 
CL_EXPORT(Closure, netcons, cl); 

/* -- Init ------------------------------------------------------- */
/* Called to set up the network console.  Standard console is in
 * Pvs(out), Pvs(err), etc.  Replace them with custom writers that use
 * original ones but can switch to network on net activity. */

static void Init(Closure_clp self)
{
    struct sockaddr_in	srcaddr;
    netcon_st		*st;
    NOCLOBBER WrRedirMod_clp  wrredirmod = NULL; /* keep gcc happy */
    NOCLOBBER RdRedirMod_clp  rdredirmod = NULL; /* keep gcc happy */
    NOCLOBBER bool_t	ok = True;

    /* find the redirector */
    TRY
    {
	wrredirmod = NAME_FIND("modules>WrRedirMod", WrRedirMod_clp);
	rdredirmod = NAME_FIND("modules>RdRedirMod", RdRedirMod_clp);
    }
    CATCH_Context$NotFound(x)
    {
	printf("Builder: couldn't find %s: won't do network console\n", x);
	ok = False;
    }
    ENDTRY
    if (!ok) return;
    
    st = Heap$Malloc(Pvs(heap), sizeof(netcon_st) + sizeof(char) * RXBUFSZ);
    if (!st)
    {
	printf("Builder: Not enough memory to do network console\n");
	return;
    }

    st->rxbuf = (char *)(st + 1);

    srcaddr.sin_family = AF_INET;
    srcaddr.sin_port   = htons(NETCONS_BASE + (VP$DomainID(Pvs(vp)) & 0xffff));
    srcaddr.sin_addr.s_addr = 0;  /* all addresses, please */

    st->sock = socket(srcaddr.sin_family, SOCK_DGRAM, IP_PROTO_UDP);
    if (st->sock == -1)
    {
	printf("Builder: socket() for network console rx failed\n");
	Heap$Free(Pvs(heap), st);
	return;
    }

    if (bind(st->sock, (struct sockaddr *)&srcaddr, sizeof(srcaddr))==-1)
    {
	printf("Builder: bind() for network console rx failed\n");
	close(st->sock);  /* XXX currently a nop! */
	Heap$Free(Pvs(heap), st);
	return;
    }

    /* initialise network writer's state */
    MU_INIT(&st->outst.txmu);
    st->outst.buffered = True;
    st->outst.txbuf = Heap$Malloc(Pvs(heap), TXBUFSZ);
    st->outst.txlen = 0;
    st->outst.netcon = st;

    MU_INIT(&st->errst.txmu);
    st->errst.buffered = False;
    /* XXX don't set ^^^^ this to True unless you grow the buffer below */
    st->errst.txbuf = &st->errtxbuf;
    st->errst.txlen = 0;
    st->errst.netcon = st;

    /* initialise network reader's state */
    MU_INIT(&st->inst.mu);
    CV_INIT(&st->inst.newdata);
    CV_INIT(&st->inst.spaceavail);
    st->inst.rxblocked = 0;
    st->inst.txblocked = 0;
    st->inst.rxbuf = Heap$Malloc(Pvs(heap), RXBUFSZ);
    st->inst.rxfree = RXBUFSZ;
    st->inst.head = 0;
    st->inst.tail = 0;
    st->inst.netcon = st;

    /* initialise the network writers & reader */
    CL_INIT(st->netout, &netwr_ms, &st->outst);
    CL_INIT(st->neterr, &netwr_ms, &st->errst);
    CL_INIT(st->netin,  &netrd_ms, &st->inst);

    /* save the current Pvs(out) etc, in case we need to back
     * off to them */
    st->serialout = Pvs(out);
    st->serialerr = Pvs(err);
    st->serialin  = Pvs(in);

    /* create the redirectors, initially going to the current Pvs() */
    TRY
    {
	st->out = WrRedirMod$New(wrredirmod, Pvs(out));
	st->err = WrRedirMod$New(wrredirmod, Pvs(err));
	st->in  = RdRedirMod$New(rdredirmod, Pvs(in));
    }
    CATCH_ALL
    {
	printf("Builder: Out of memory creating redirectors. "
	       "Won't do network console.\n");
	ok = False;
    }
    ENDTRY
    if (!ok)
    {
	close(st->sock);
	MU_FREE(&st->inst.mu);
	MU_FREE(&st->outst.txmu);
	MU_FREE(&st->errst.txmu);
	CV_FREE(&st->inst.newdata);
	Heap$Free(Pvs(heap), st->inst.rxbuf);
	Heap$Free(Pvs(heap), st->outst.txbuf);
	Heap$Free(Pvs(heap), st->errst.txbuf);
	Heap$Free(Pvs(heap), st);
	return;
    }

    /* now move over to our redirectors */
    Pvs(out) = (Wr_clp) st->out;
    Pvs(err) = (Wr_clp) st->err;
    Pvs(in)  = (Rd_clp) st->in;

    /* start receiver */
    TRY
    {
	Threads$Fork(Pvs(thds), rx_thread, st, 0);
    }
    CATCH_Threads$NoResources()
    {
	/* Looks bad: this would have been the first fork. Ok,
	 * maybe we're using a Threads package which doesn't have a thread
	 * scheduler?
	 */

	/* restore Pvs() */
	Pvs(out) = st->serialout;
	Pvs(err) = st->serialerr;
	Pvs(in)  = st->serialin;

	printf("Builder: no thread support => not doing network console\n");

	/* clean up */
	close(st->sock);
	MU_FREE(&st->outst.txmu);
	MU_FREE(&st->errst.txmu);
	MU_FREE(&st->inst.mu);
	CV_FREE(&st->inst.newdata);
	Heap$Free(Pvs(heap), st->inst.rxbuf);
	Heap$Free(Pvs(heap), st->outst.txbuf);
	Heap$Free(Pvs(heap), st->errst.txbuf);
	Heap$Free(Pvs(heap), st);
    }
    ENDTRY
}


static void rx_thread(netcon_st *st)
{
    struct sockaddr_in	remote;
    int			remote_len;
    int			nbytes;
    int			did;

    did = VP$DomainID(Pvs(vp)) & 0xffff;

    RXTRC(printf("Netcons: listening on port %d\n", NETCONS_BASE + did));

    /* wait for someone to contact us, then switch to network console */
    remote_len = sizeof(remote);
    nbytes = recvfrom(st->sock, st->rxbuf, RXBUFSZ-1, 0 /*(flags)*/, 
		      (struct sockaddr *)&remote,
		      &remote_len);
    if (nbytes == -1)
    {
	printf("Netcons: problem receiving from network\n");
	return; /* thread dies */
    }

    printf("Netcons: domain %#x switching to network console on port %d\n",
	   did, NETCONS_BASE + did);

    /* set the socket destination */
    remote.sin_port = htons(NETCONS_BASE + did);
    st->outst.dstaddr = remote;
    st->errst.dstaddr = remote;

    /* switch redirectors over */
    WrRedir$SetWr(st->out, &st->netout);
    WrRedir$SetWr(st->err, &st->neterr);
    RdRedir$SetRd(st->in,  &st->netin);


    /* now continue running, shovelling net data into the rx buffer */
    do /* while(1) */
    {
	MU_LOCK(&st->inst.mu);

	/* If we don't have enough space, block until there's
         * some. Need to leave 1 byte free as minumum ammount of
         * pushback, hence wait till got nbytes+1 space free */
	while (st->inst.rxfree < nbytes+1)
	{
	    RXTRC(wprintf(st->serialerr, "Netcons: waiting for space..\n"));
	    st->inst.txblocked++;
	    WAIT(&st->inst.mu, &st->inst.spaceavail);
	    st->inst.txblocked--;
	}

	/* do we need to do 2 memcpy()s to cope with wrap-around? */
	if (nbytes > (RXBUFSZ - st->inst.head))
	{
	    int startlen = RXBUFSZ - st->inst.head;
	    memcpy(st->inst.rxbuf + st->inst.head, st->rxbuf, startlen);
	    memcpy(st->inst.rxbuf, st->rxbuf + startlen, nbytes - startlen);
	    st->inst.head = nbytes - startlen;
	    RXTRC(wprintf(st->serialerr,
			"Netcons: added %d to rx (2 memcpy)\n",
			nbytes));
	}
	else
	{
	    memcpy(st->inst.rxbuf + st->inst.head, st->rxbuf, nbytes);
	    st->inst.head += nbytes;
	    if (st->inst.head == RXBUFSZ)
		st->inst.head = 0;
	    RXTRC(wprintf(st->serialerr,
			"Netcons: added %d to rx (1 memcpy)\n",
			nbytes));

	}
	st->inst.rxfree -= nbytes;

	if (st->inst.rxblocked)
	    SIGNAL(&st->inst.newdata);

	RXTRC(wprintf(st->serialerr,
		      "Stat: rxfree:%d  head:%d  tail:%d  rxblocked:%d  txblocked:%d\n",
		      st->inst.rxfree, st->inst.head, st->inst.tail,
		      st->inst.rxblocked, st->inst.txblocked));

	MU_RELEASE(&st->inst.mu);


	/* ok, now block waiting for some more network input */
	RXTRC(wprintf(st->serialerr, "Netcons: waiting for rx\n"));
	nbytes = recvfrom(st->sock, st->rxbuf, RXBUFSZ-1, 0 /*(flags)*/, 
			  (struct sockaddr *)&remote,
			  &remote_len);
	if (nbytes == -1)
	{
	    printf("Netcons: problem receiving from network\n");
	    /* XXX back off to serial input? */
	    return; /* thread dies */
	}
    } while (1);
}




/* ------------------------------------------------------------ */
/* Network writer methods */

static void
NetWr_PutC_m(Wr_cl *self, int8_t ch) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    USING(MUTEX, &st->txmu, {NetWr_LPutC_m(self, ch);});
}

static void
NetWr_PutStr_m(Wr_cl *self, string_t s) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    USING(MUTEX, &st->txmu, {NetWr_LPutStr_m(self, s);});
}

static void
NetWr_PutChars_m(Wr_cl *self, int8_t *s, uint32_t nb) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    USING(MUTEX, &st->txmu, {NetWr_LPutChars_m(self, s, nb);});
}

static void
NetWr_Seek_m(Wr_cl *self, uint32_t n) {
    /* illegal operation */
}

static void
NetWr_Flush_m(Wr_cl *self) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    USING(MUTEX, &st->txmu, {NetWr_LFlush_m(self);});
}

static void
NetWr_Close_m(Wr_cl *self) {
    /* XXX garbage collect resources */
}

static uint32_t
NetWr_Length_m(Wr_cl *self) {
    return 0;  /* not really meaningful anyway */
}

static uint32_t
NetWr_Index_m(Wr_cl *self) {
    return 0;  /* not really meaningful anyway */
}

static bool_t
NetWr_Seekable_m(Wr_cl *self) {
    return False;
}

static bool_t
NetWr_Closed_m(Wr_cl *self) {
    return False;  /* if we got here, we're clearly not closed! */
}

static bool_t
NetWr_Buffered_m(Wr_cl *self) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return st->buffered;
}

static void
NetWr_Lock_m(Wr_cl *self) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    MU_LOCK(&st->txmu);
}

static void
NetWr_Unlock_m(Wr_cl *self) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    MU_RELEASE(&st->txmu);
}

static void
NetWr_LPutC_m(Wr_cl *self, int8_t ch) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    st->txbuf[st->txlen++] = ch;
    if (!st->buffered || ch == '\n' || st->txlen == TXBUFSZ)
	NetWr_LFlush_m(self);
}

static void
NetWr_LPutStr_m(Wr_cl *self, string_t s) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    while(*s)
    {
	st->txbuf[st->txlen++] = *s;
	if (!st->buffered || *s == '\n' || st->txlen == TXBUFSZ)
	    NetWr_LFlush_m(self);
	s++;
    }
}

static void
NetWr_LPutChars_m(Wr_cl *self, int8_t *s, uint32_t nb) {
    netwr_st *st = (netwr_st *)self->st;
    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    while(nb)
    {
	st->txbuf[st->txlen++] = *s;
	if (!st->buffered || *s == '\n' || st->txlen == TXBUFSZ)
	    NetWr_LFlush_m(self);
	nb--;
	s++;
    }
}

static void
NetWr_LFlush_m(Wr_cl *self) {
    netwr_st *st = (netwr_st *)self->st;
    int       nbytes;

    TXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    
    if (st->txlen != 0)
    {
	TXTRC(wprintf(st->netcon->serialerr,
		"NetWr_LFlush: about to sendto %d bytes\n`%.*s'\n",
		st->txlen, st->txlen, st->txbuf));

	/* spit out the data */
	nbytes = sendto(st->netcon->sock, st->txbuf, st->txlen, 0,
			(struct sockaddr *)&st->dstaddr,
			sizeof(struct sockaddr));
	if (nbytes != st->txlen)
	{
	    /* oops, something bad happened.  Let's back off to serial
	     * output again */
	    WrRedir$SetWr(st->netcon->out, st->netcon->serialout);
	    WrRedir$SetWr(st->netcon->err, st->netcon->serialerr);
	    /* announce the bad news */
	    printf("netcons: sendto() returned nbytes=%d, expecting %d\n",
		   nbytes, st->txlen);
	    printf("netcons: returned to serial output\n");
	    TXTRC(wprintf(st->netcon->serialerr, "NetWr_LFlush: error\n"));
	    return;
	}

	TXTRC(wprintf(st->netcon->serialerr, "NetWr_LFlush: ok\n"));

	st->txlen = 0;
    }
    else
    {
	TXTRC(wprintf(st->netcon->serialerr, "NetWr_LFlush: no need to flush\n"));
    }	

}



/* ------------------------------------------------------------ */
/* Network reader methods */

/* The setup: the network reader thread blocks reading data from the
 * network into the its contiguous buffer space.  Reader client
 * threads block on the condition variable "newdata" waiting for data
 * to arrive.  Data arrives, the reader thread punts it to the
 * circular buffer, possibly splitting it. "head" gets advanced, and
 * the condition variable is signalled. */

#define BUFFER_EMPTY (st->rxfree == RXBUFSZ)


static int8_t
NetRd_GetC_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    int8_t res;

    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));

    MU_LOCK(&st->mu);

    while BUFFER_EMPTY
    {
	/* wait until there's some more data been put in */
	RXTRC(wprintf(st->netcon->serialerr,
		      "GetC: buffer empty, waiting...\n"));
	st->rxblocked++;
	WAIT(&st->mu, &st->newdata);
	st->rxblocked--;
    }

    res = st->rxbuf[st->tail];
    st->tail++;
    if (st->tail == RXBUFSZ)
	st->tail = 0;

    st->rxfree++;

    if (st->txblocked)
	SIGNAL(&st->spaceavail);

    RXTRC(wprintf(st->netcon->serialerr,
		  "Stat: rxfree:%d  head:%d  tail:%d  rxblocked:%d  txblocked:%d\n",
		  st->rxfree, st->head, st->tail,
		  st->rxblocked, st->txblocked));

    MU_RELEASE(&st->mu);

    RXTRC(wprintf(st->netcon->serialerr, "%s done\n", __FUNCTION__));

    return res;
}

/* EOF is always false for the network console */
static bool_t
NetRd_EOF_m(Rd_cl *self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return False;
}

/* UnGetC pushes the last char read into the buffer. */
static void
NetRd_UnGetC_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    int		t;

    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));

    MU_LOCK(&st->mu);

    /* All that really needs to be done is to move the "tail" pointer
     * back one position, if it wouldn't clash with the newly arrived
     * data. */
    t = st->tail - 1;
    if (t < 0)
	t = RXBUFSZ-1;

    if (t != st->head)
    {
	st->tail = t;
	st->rxfree--;
    }

    /* we've effectively "added" some more data, so tell people about
     * it, if there's anyone interested */
    if (st->rxblocked)
	SIGNAL(&st->newdata);
	   
    MU_RELEASE(&st->mu);
}

/*
 * Simple read of the buffer size.  */
static uint32_t
NetRd_CharsReady_m(Rd_cl	*self)
{
    netrd_st *st = self->st;

    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return RXBUFSZ - st->rxfree;
}

/*
 * Get a number of characters into the buffer. 
 * Under the lock we still maintain the invariants concerning UnGetC,
 * since it is possible someone else could come in when we do the WAIT.
 *
 */
static uint32_t
NetRd_GetChars_m(Rd_cl	*self,
	      Rd_anon0	buf	/* IN */,
	      uint32_t	nb	/* IN */)
{
    netrd_st *st = self->st;
    int i = 0;

    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));

    MU_LOCK(&(st->mu));
	 
    for(i = 0; i < nb; i++)
    {
	while BUFFER_EMPTY
	{
	    st->rxblocked++;
	    WAIT(&st->mu, &st->newdata);
	    st->rxblocked--;
	}

	buf[i] = st->rxbuf[st->tail];
	st->tail++;
	if (st->tail == RXBUFSZ)
	    st->tail = 0;

	st->rxfree++;
    }

    if (st->txblocked)
	SIGNAL(&st->spaceavail);

    MU_RELEASE(&st->mu);
    return i;
}

/*
 * GetLine is very similar to GetChars.
 */
static uint32_t
NetRd_GetLine_m(Rd_cl	*self,
		Rd_anon0	buf	/* IN */,
		uint32_t	nb	/* IN */)
{
    netrd_st *st = self->st;
    bool_t nocr = True;
    int i;

    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));

    MU_LOCK(&st->mu);

    /* very similar to above, but check for '\n' too */
    for(i = 0; i < nb && nocr; i++)
    {
	while BUFFER_EMPTY
	{
	    st->rxblocked++;
	    WAIT(&st->mu, &st->newdata);
	    st->rxblocked--;
	}

	buf[i] = st->rxbuf[st->tail];
	st->tail++;
	if (st->tail == RXBUFSZ)
	    st->tail = 0;

	st->rxfree++;

	if (buf[i] == '\n')
	    nocr = False;
    }

    if (st->txblocked)
	SIGNAL(&st->spaceavail);

    MU_RELEASE(&st->mu);
    return i;
}

/*
 * We are not seekable.
 */
static void
NetRd_Seek_m(Rd_cl	*self,
	     uint32_t	nb	/* IN */)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    RAISE_Rd$Failure(1); /* 1 == not seekable */
}

/*
 * We can't close this either. 
 */
static void
NetRd_Close_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
}

/* 
 * Length returns -1, since the length is unknown (potentially
 * infinite).
 */
static int32_t
NetRd_Length_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return -1;
}

/*
 * Index is probably not correctly implemented, but returns -1.
 */
static uint32_t
NetRd_Index_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return -1;
}

/*
 * We are intermittent.
 */
static bool_t
NetRd_Intermittent_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return True;
}

/*
 * We are not seekable
 */
static bool_t
NetRd_Seekable_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return False;
}

/* 
 * We are open all hours.
 */
static bool_t
NetRd_Closed_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return False;
}

/*
 * Acquire the lock
 */
static void
NetRd_Lock_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    MU_LOCK(&st->mu);
}

static void
NetRd_Unlock_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    MU_RELEASE(&st->mu);
}

static int8_t
NetRd_LGetC_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    int8_t res;

    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
  
    while BUFFER_EMPTY
    {
	/* wait until there's some more data been put in */
	st->rxblocked++;
	WAIT(&st->mu, &st->newdata);
	st->rxblocked--;
    }

    res = st->rxbuf[st->tail];
    st->tail++;
    if (st->tail == RXBUFSZ)
	st->tail = 0;

    st->rxfree++;

    if (st->txblocked)
	SIGNAL(&st->spaceavail);

    return res;
}

static bool_t
NetRd_LEOF_m(Rd_cl	*self)
{
    netrd_st *st = self->st;
    RXTRC(wprintf(st->netcon->serialerr, "%s called\n", __FUNCTION__));
    return False;
}
