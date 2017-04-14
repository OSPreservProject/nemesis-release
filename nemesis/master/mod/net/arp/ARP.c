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
**      ARP.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of the Address Resolution Protocol
** 
** ENVIRONMENT: 
**
**      Used internally in the FlowManager
** 
** ID : $Id: ARP.c 1.2 Thu, 18 Feb 1999 15:09:28 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <mutex.h>
#include <time.h>
#include <stdio.h>

#include <ARPMod.ih>
#include <WordTblMod.ih>
#include <HeapMod.ih>
#include <Threads.ih>
#include <IO.ih>
#include <Context.ih>
#include <IOMacros.h>

#include <netorder.h>
#include <iana.h>


/* light tracing of calls */
#define ETRC(x)

/* packet dumping tracing */
#define TRC(x)

/* ARP packet format:
 *  0: Ethernet destination address
 *  6: Ethernet sender address
 * 12: Ethernet packet type
 * 14: Hardware address type
 * 16: Protocol type
 * 18: Hardware address length
 * 19: IP address length
 * 20: Operation code
 * 22: Sender hardware address
 * 28: Sender IP address
 * 32: Target hardware address
 * 38: Target IP address
 */

/* Definitions */

/* Entry expiry time: 60 seconds */
#define ARP_ENTRY_EXPIRY_TIME SECONDS(60)
#define ARP_RETRIES 4
/* rfc1122 (2.3.2.1 ARP Cache Validation) says:
 *           A mechanism to prevent ARP flooding (repeatedly sending an
 *           ARP Request for the same IP address, at a high rate) MUST be
 *           included.  The recommended maximum rate is 1 per second per
 *           destination.
 */
#define ARP_INITIAL_RETRY_DELAY SECONDS(1)

#define ARP_HARDWARE_ETHER10 htons(1)

#define ARP_REQUEST htons(1)
#define ARP_REPLY   htons(2)


/* Module stuff */

static ARPMod_New_fn New_m;

static ARPMod_op a_op = { New_m };
static const ARPMod_cl a_cl = { &a_op, (addr_t)0xdeadbeef };

CL_EXPORT(ARPMod,ARPMod,a_cl);

static Context_List_fn		List;
static Context_Get_fn		Get;
static Context_Add_fn		Add;
static Context_Remove_fn	Remove;
static Context_Dup_fn		Dup;
static Context_Destroy_fn	DestroyCtxt;

static Context_op c_op = { List, Get, Add, Remove, Dup, DestroyCtxt };

/* State */

struct ARPwait_st {
    Event_Count signal; /* XXX only ever used once */
    struct ARPwait_st *next; /* More than one thread may wait on a request */
};

struct ARPentry {
    Net_EtherAddr address; /* Ethernet address corresponding to IP address */
    Time_ns time;          /* Time at which address was added to cache */
};

typedef struct ARP_st ARP_st;
struct ARP_st {
    Heap_clp rxheap;
    Heap_clp txheap;
    Net_IPHostAddr ipaddr;
    const Net_EtherAddr *hwaddr;
    IO_clp tx;
    IO_clp rx;
    uint8_t *txbuf;
    uint8_t *rxbuf;
    WordTbl_clp arptable;
    SRCThread_Mutex arptable_mu;
    SRCThread_Mutex tx_mu;
    WordTbl_clp waittable;
    SRCThread_Mutex waittable_mu;
};

/* Function declarations */
static bool_t arp_update_cache(ARP_st *st, const Net_IPHostAddr *ip,
			       const Net_EtherAddr *hw);
static void arp_encache(ARP_st *st, const Net_IPHostAddr *ip,
			const Net_EtherAddr *hw);
static bool_t arp_find(ARP_st *st, const Net_IPHostAddr *ip,
		       Net_EtherAddr **hw);
static void arp_send(ARP_st *st, uint16_t type, 
		     const Net_IPHostAddr *ip, const Net_EtherAddr *hwaddr);
static bool_t string_to_addr(string_t s, Net_IPHostAddr *ip);

/* Utility functions */

/* Update a pre-existing ARP entry */
static bool_t arp_update_cache(ARP_st *st, const Net_IPHostAddr *ip,
			       const Net_EtherAddr *hw)
{
    struct ARPentry *entry;
    bool_t present;

    MU_LOCK(&st->arptable_mu);

    if ((present=WordTbl$Get(st->arptable, ip->a, (addr_t)&entry))) {
	entry->time=NOW();
	memcpy(&entry->address,hw,6);
    }
    MU_RELEASE(&st->arptable_mu);
    return present;
}

/* Create a new ARP entry */
static void arp_encache(ARP_st *st, const Net_IPHostAddr *ip,
			const Net_EtherAddr *hw)
{
    struct ARPentry *entry;
    struct ARPwait_st *wait;
    bool_t present;

    ETRC(printf("arp_encache\n"));

    MU_LOCK(&st->arptable_mu);

    present=WordTbl$Get(st->arptable, ip->a, (addr_t)&entry);
    ETRC(printf(" - %x not present\n", ip->a));

    if (!present) {
	entry=Heap$Malloc(Pvs(heap), sizeof(*entry));
    }
    entry->time=NOW();
    memcpy(&entry->address, hw, 6);
    WordTbl$Put(st->arptable, ip->a, entry);
    MU_RELEASE(&st->arptable_mu);

    /* If threads are waiting for this ARP entry, wake them up here */
    MU_LOCK(&st->waittable_mu);
    present=WordTbl$Get(st->waittable, ip->a, (addr_t)&wait);
    if (present) {
	while (wait) {
	    ETRC(printf(" - waking %p\n", wait));
	    Events$Advance(Pvs(evs), wait->signal, 1);
	    wait=wait->next;
	}
    }
    MU_RELEASE(&st->waittable_mu);
    ETRC(printf("done encache\n"));
}

static bool_t arp_find(ARP_st *st, const Net_IPHostAddr *ip,
		       Net_EtherAddr **hw)
{
    struct ARPentry *entry;
    bool_t present;

    /* See if the entry is in the cache */
    MU_LOCK(&st->arptable_mu);
    present=WordTbl$Get(st->arptable, ip->a, (addr_t)&entry);
    MU_RELEASE(&st->arptable_mu);

    if (present) {
	/* Is the entry still valid? */
	if ((NOW()-entry->time)<=ARP_ENTRY_EXPIRY_TIME) {
	    *hw=&entry->address;
	} else {
	    present=False;
	}
    }
    return present;
}

/* Send an ARP packet */
static void arp_send(ARP_st *st, uint16_t type, 
		     const Net_IPHostAddr *ip, const Net_EtherAddr *hwaddr)
{
    IO_Rec rec;
    word_t value;
    uint32_t nrecs; 

    rec.base= st->txbuf; 
    rec.len=60; /* XXX hack? */

    /* We have to lock _before_ assembling the packet in the buffer */
    MU_LOCK(&st->tx_mu);

    /* Put in our hardware address and IP address */
    memcpy(st->txbuf+6,  st->hwaddr, 6);
    memcpy(st->txbuf+22, st->hwaddr, 6);

    /* XXX SMH -- the txbuf is long word aligned, so below is ok */
    *(uint32_t *)(st->txbuf+28) = st->ipaddr.a;

    /* Put in their IP address and maybe hardware address too */

    /* XXX SMH -- however, must memcpy the below (since (38&4)!=0) */
    memcpy((char *)(st->txbuf+38), &ip->a, 4);

    if (hwaddr) memcpy(st->txbuf+32,hwaddr,6);
    /* Fill in operation codes */
    *(uint16_t *)(st->txbuf+12) = FRAME_ARP;
    *(uint16_t *)(st->txbuf+14) = ARP_HARDWARE_ETHER10;
    *(uint16_t *)(st->txbuf+16) = FRAME_IPv4;
    *(st->txbuf+18) = 6; /* Hardware address length */
    *(st->txbuf+19) = 4; /* Protocol address length */
    *(uint16_t *)(st->txbuf+20) = type; /* Opcode */

    if (type == ARP_REQUEST) {
	memset(st->txbuf, 0xff, 6); /* Broadcast requests */
    } else {
	memcpy(st->txbuf+0,hwaddr,6); /* Direct replys */
    }

    TRC(dump(rec.base, rec.len));

    ETRC(printf("TXPutPkt: %p(%d)\n", rec.base, rec.len));
    IO$PutPkt(st->tx, 1, &rec, 0, FOREVER);
    IO$GetPkt(st->tx, 1, &rec, FOREVER, &nrecs, &value);
    ETRC(printf("TXGetPkt: %p(%d)\n", rec.base, rec.len));
    MU_RELEASE(&st->tx_mu);
}

static bool_t hexchar(uint8_t c)
{
    return (c<='f' && c>='a') || (c<='9' && c>='0');
}

static uint8_t hexdigit(uint8_t c)
{
    if (c<='9' && c>='0')
	return c-'0';

    return c-'a'+10;
}

/* Convert a string of the form "Eth10:aabbccdd" into an IP address */
static bool_t string_to_addr(string_t s, Net_IPHostAddr *ip)
{
    int i;
    int u, v;
    uint32_t o;

    /* Check the length is 14 */
    i = strlen(s);
    if (i != 14)
    {
	printf("ARP: Badly formed request: `%s' has length %d != 14", s, i);
	return False;
    }

    if (strncmp(s, "Eth10:", 6)!=0)
    {
	printf("ARP: Badly formed request: unrecognised header `%.6s'\n", s);
	return False;
    }
    s+=6;

    /* We expect the IP address to be in network order (i.e. big endian) */

    /* Check that the next 8 characters are hex digits */
    for (i=0; i<8; i++)
	if (!hexchar(s[i]))
	{
	    printf("ARP: Badly formed request: "
		    "`%c' is not a valid hex char\n", s[i]);
	    return False;
	}

    o=0;
    for (i=0; i<8; i+=2) {
	u=hexdigit(s[i]);
	v=hexdigit(s[i+1]);
	o |=( ((u<<4)+v) <<((6-i)*4) );
    }

    ETRC(printf("string_to_addr: string was %s, result is %x\n", s, o));

    ip->a=o;

    return True;
}

/* Work threads */

TRC(static void dump(uint8_t *p, int len)
{
    int i;
    char buf[80];
    int l=0;

    for (i = 0; i < len; i++)
    {
	l += sprintf(buf+l, "%02x ", p[i]);
	if (i % 16 == 15)
	{
	    printf("%s\n", buf);
	    l = 0;
	}
    }
    if (l)
	printf("%s\n", buf);
	
    printf ("\n");
    
    printf ("------\n");
})

static void RXThread(addr_t data)
{
    ARP_st *st=data;
    IO_Rec rec;
    word_t value;
    uint32_t nrecs;
    uint8_t *packet;
    Net_IPHostAddr sender_ip, target_ip;
    Net_EtherAddr *sender_hw, *target_hw;
    uint16_t opcode;
    bool_t merge;

    /* prime the RX */
    rec.base= st->rxbuf;
    rec.len=1512; /* XXX */
    ETRC(printf("RXPutPkt: %p(%d)\n", rec.base, rec.len));
    IO$PutPkt(st->rx, 1, &rec, 0, FOREVER);

    rec.base= st->rxbuf + 2048;


    TRY {
        while(True)
	{
	    /* prime rx */
            rec.len=1512; /* XXX */
	    ETRC(printf("RXPutPkt: %p(%d)\n", rec.base, rec.len));
	    IO$PutPkt(st->rx, 1, &rec, 0, FOREVER);

	    ETRC(printf("arp:RXThread:here we go again\n"));

	    IO$GetPkt(st->rx, 1, &rec, FOREVER, &nrecs, &value);
	    ETRC(printf("RXGetPkt: %p(%d)\n", rec.base, rec.len));
	    if (nrecs!=1) continue;

	    /* Check the ARP packet header; we are only interested in
	       Ethernet and IP */
	    packet=rec.base;
	    if (*(uint16_t *)(packet+14) != ARP_HARDWARE_ETHER10 ||
		*(uint16_t *)(packet+16) != FRAME_IPv4) {
		/* We aren't interested in this packet */
		continue;
	    }

	    TRC(dump(packet, rec.len));
	    /* Read the information from the packet */

	    /* XXX SMH -- rxbuf is long word aligned:
	       => can copy one, must memcpy other */
	    sender_ip.a = *(uint32_t *)(packet+28);
	    memcpy(&(target_ip.a), (char *)(packet+38), 4);

	    sender_hw=(Net_EtherAddrP)(packet+22);
	    target_hw=(Net_EtherAddrP)(packet+32);
	    opcode = *(uint16_t *)(packet+20);

	    /* Cache the sender information if necessary */
	    ETRC(printf("arp:RXThread:updating cache\n"));
	    merge=arp_update_cache(st, &sender_ip, sender_hw);

	    /* Look at the target IP address; give up if it isn't us */
	    ETRC(printf("arp:RXThread:target IP is %x\n",ntohl(target_ip.a)));
	    if (target_ip.a != st->ipaddr.a)
		continue;

	    /* If the merge flag is false, add the entry to the table */
	    ETRC(printf("arp:RXThread:someone is calling for us\n"));
	    if (!merge) arp_encache(st, &sender_ip, sender_hw);

	    /* If the packet is a request, reply to it */

	    if (opcode == ARP_REQUEST) {
		ETRC(printf("arp:RXThread:replying to request\n"));
		arp_send(st, ARP_REPLY, &sender_ip, sender_hw);
	    }
	}
    } CATCH_Thread$Alerted() {
	/* the device driver has probably vanished. Oh well, guess
	 * we won't need to do ARP anymore */
	printf("ARPthread: exiting\n");
    } ENDTRY;
}

/* Implementation - methods */

static Context_Names *List(Context_cl *self)
{
    ARP_st *st=self->st;
    Context_Names *res;
    WordTblIter_clp i;
    int ind;
    addr_t junk;
    uint32_t key;
    uint8_t buff[16];

    MU_LOCK(&st->arptable_mu);

    res=SEQ_NEW(Context_Names, WordTbl$Size(st->arptable), Pvs(heap));

    i=WordTbl$Iterate(st->arptable);

    ind=0;
    while (WordTblIter$Next(i, (word_t *)&key, (addr_t *)&junk)) {
	sprintf(buff,"Eth10:%08x",key);
	SEQ_ELEM(res,ind)= strdup(buff);
    }

    WordTblIter$Dispose(i);
    MU_RELEASE(&st->arptable_mu);

    return res;
}

static bool_t Get(Context_cl *self, string_t name, Type_Any *obj)
{
    ARP_st *st=self->st;
    struct ARPwait_st *wait, *wn, **wp= NULL;
    Net_IPHostAddr ip;
    Net_EtherAddr *hw;
    bool_t present;
    uint32_t retry;
    Event_Val ack;
    Time_ns retry_delay;

    ETRC(printf("ARP Get for `%s'\n", name));

    /* Convert the name to a number */
    if (!string_to_addr(name, &ip))
	return False;
    
    /* XXX SMH: check if it is our own IP address. */
    if(ip.a == st->ipaddr.a)
    {
	printf("ARPing for my own address??\n");
	ANY_INIT(obj, Net_EtherAddrP, st->hwaddr);
	return True;
    }

    present = arp_find(st, &ip, &hw);

    if (!present) {
	/* The entry isn't valid or isn't present; send an ARP packet */

	/* Add ourselves to the 'waiting' table */
	wait = Heap$Malloc(Pvs(heap), sizeof(*wait));
	wait->signal = Events$New(Pvs(evs));
	ack = 0;
	MU_LOCK(&st->waittable_mu);
	if (!WordTbl$Get(st->waittable, ip.a, (addr_t)&wait->next))
	    wait->next=NULL;
	WordTbl$Put(st->waittable, ip.a, wait);
	MU_RELEASE(&st->waittable_mu);

	retry=ARP_RETRIES;
	retry_delay=ARP_INITIAL_RETRY_DELAY;

	while (retry) {
	    arp_send(st, ARP_REQUEST, &ip, NULL);

	    Events$AwaitUntil(Pvs(evs), wait->signal, ++ack,
			     NOW()+retry_delay);

	    present=arp_find(st, &ip, &hw);

	    if (present) {
		/* Remove ourselves from wait queue */
		/* XXX do it */
		MU_LOCK(&st->waittable_mu);
		WordTbl$Get(st->waittable, ip.a, (addr_t)&wn);
		if (wn==wait) {
		    if (!wait->next)
#if 0
			WordTbl$Delete(st->waittable, ip.a, (addr_t)&wn);
#endif
			WordTbl$Delete(st->waittable, ip.a, (addr_t)wn);
		    else
			WordTbl$Put(st->waittable, ip.a, (addr_t)wait->next);
		} else {
		    /* The record must be further down the list */
		    while (wn && wn!=wait) {
			wp=&wn->next;
			wn=wn->next;
		    }
		    *wp=wait->next;
		    Events$Free(Pvs(evs), wn->signal);
		    Heap$Free(Pvs(heap), wn);
		}
		retry=0;
		MU_RELEASE(&st->waittable_mu);
		continue;
	    }
	    retry--;
	    retry_delay*=2;
	}
    }

    if (!present)
    {
	printf("ARP for %I got no answer\n", ip.a);
	return False;
    }

    ANY_INIT(obj, Net_EtherAddrP, hw);
    return True;
}
    

/* Eventually this could be made to do proxyARP... */
static void Add(Context_cl *self, string_t name, const Type_Any *obj)
{
    RAISE_Context$Denied();
}

/* Remove an entry from the cache; presumably used to invalidate one */
static void Remove(Context_cl *self, string_t name)
{
    RAISE_Context$Denied();
}

static Context_clp Dup(Context_cl *self, Heap_clp heap, WordTbl_clp xl)
{
    RAISE_Context$Denied();

    return NULL;
}

/* Eventually make this tidy up */
static void DestroyCtxt(Context_cl *self)
{
    RAISE_Context$Denied();
}


static Context_clp New_m(ARPMod_cl *self, IO_clp rxio, IO_clp txio,
			 Heap_clp rxheap, Heap_clp txheap, 
			 const Net_IPHostAddr *myip, 
			 const Net_EtherAddr *myether)
{
    ARP_st *st;
    Context_cl *cl;
    WordTblMod_clp wt;

    /* Allocate memory for closure and state */
    cl          = Heap$Malloc(Pvs(heap), sizeof(*cl) + sizeof(*st));
    cl->op      = &c_op;
    cl->st = st = (ARP_st *)(cl+1);

    /* Fill in state */
    st->rxheap = rxheap;
    st->txheap = txheap;
    st->ipaddr = *myip;

    /* dbs20: If we arp for our own addr apps need to be able to read
     * this, so it must be allocated on a heap they can read.  
     */
    st->hwaddr = myether;

    st->rx = rxio;
    st->tx = txio;


#define ALIGN4(_x) (((word_t)(_x)+3)&(~3))

    st->txbuf = Heap$Calloc(st->txheap, 3000, sizeof(char));
    st->txbuf = (uint8_t *)ALIGN4(st->txbuf);
    TRC(fprintf(stderr, "ARP: txbuf at %p\n", st->txbuf));

    st->rxbuf = Heap$Malloc(st->rxheap, 4096);
    st->rxbuf = (uint8_t *)ALIGN4(st->rxbuf);
    TRC(fprintf(stderr, "ARP: rxbuf at %p\n", st->rxbuf));

    wt=NAME_FIND("modules>WordTblMod", WordTblMod_clp);

    st->arptable  = WordTblMod$New(wt, Pvs(heap));
    st->waittable = WordTblMod$New(wt, Pvs(heap));

    /* Initialise mutices */
    MU_INIT(&st->arptable_mu);
    MU_INIT(&st->tx_mu);
    MU_INIT(&st->waittable_mu);

    /* Fork thread to receive ARP packets */
    Threads$Fork(Pvs(thds), RXThread, st, 0);

    return cl;
}
