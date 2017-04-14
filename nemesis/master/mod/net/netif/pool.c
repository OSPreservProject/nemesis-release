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
**      pool.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      FragPool implementation.
** 
** ENVIRONMENT: 
**
**      Internal to Netif.
** 
** ID : $Id: pool.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <stdlib.h>
#include <exceptions.h>
#include <time.h>
#include <netorder.h>

#include <IO.ih>
#include <TimeNotify.ih>

#include <FragPool.ih>
#include "pool.h"

#include <slab.c>

//#define POOL_TRACE

#ifdef POOL_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/* maximum number of assembly queues we're prepared to tolerate
 * simultaneously.  Limits number of timeouts to this too.  Also
 * limits the max number of allocations needed. */
#define MAX_ASMQ 5

/* Expected maximum number of fragments per assembly queue.  Can go
 * higher than this on an individual queue, but this sets the limit
 * over all queues. */
#define AVG_FRAGS 6

/* RFC1122 says this should be between 60 and 120 seconds */
#define REASSEMBLY_TIMEOUT SECONDS(60)

/* load unaligned macros */
#define LDU_32(a) ({ char *_a = a; /* eval once */  \
    (*(uint16_t*)_a) | ((*(uint16_t*)(_a+2)) << 16); })

/* PF_Handle special returns */
#define FAIL  ((PF_Handle)0)
#define DELAY ((PF_Handle)-1)


/* network-endian offset mask */
#define OFFMASK 0xff1f


static  FragPool_PutPkt_fn              PutPkt_m;
static  FragPool_TXFilt_fn              TXFilt_m;

PUBLIC FragPool_op     fragpool_ops = {
  PutPkt_m,
  TXFilt_m
};


static TimeNotify_Notify_fn        Timeout_m;
static TimeNotify_op               timeout_ops = { Timeout_m };


/* an "asmq_t" hold meta-information about a list of fragments */
typedef struct _asmq_t {
    uint32_t		hash;		/* mkhash(key) for speedy lookups */
    FragPool_key	key;		/* unique key */
    struct _asmq_t	*next;
    FragPool_fraglist	fl;		/* fraglist for this Q item */
    PF_Handle		hdl;		/* see note (1) */
    Time_ns		deadline;	/* when this times out */
    bool_t		valid;		/* True when this entry is in use */
} asmq_t;
/* note (1): The "hdl" field above can be in one of three states:
 *   a) DELAY  -  no demux in progress
 *   b) FAIL   -  packets to be unconditionally ditched
 *   c) other  -  packets are the be sent to this handle
 */


/* state for the FragPool */
typedef struct {
    FragPool_cl		fragpoolcl;
    ActivationF_clp	actf;		/* cache of our activationF */
    asmq_t		*aqmap;		/* map from IP id to assembly Q */
    to_client_fn	to_client;	/* send data to a client */
    release_fn		release;	/* fn to call on fragment release */
    void		*netifst;	/* the netif state we're with */
    TimeNotify_cl	timeoutcl;	/* timeout handler */
    slaballoc_t		*sml_alloc;	/* slab manager for asm Q & recs */
    slaballoc_t		*pkt_alloc;	/* packet buffer allocator */
} FragPool_st;


static bool_t frag_dup(FragPool_st *st, uint32_t nrecs, IO_Rec *recs,
		       FragPool_frag *new);
static void release(void *state, uint32_t nrecs, IO_Rec *recs);



/* ------------------------------------------------------------ */
/* Creating a fragpool */

FragPool_cl *fragpool_new(uint32_t mtu, void *netifst, to_client_fn to_client)
{
    FragPool_st		*st;

    st = Heap$Malloc(Pvs(heap), sizeof(FragPool_st));
    if (!st)
	RAISE_Heap$NoMemory();

    /* keep a copy of our activationF, since we can't get to our
     * pervasives during activation handlers */
    st->actf  = Pvs(actf);

    st->aqmap = NULL;		/* lazily allocate resources */

    st->netifst = netifst;
    st->to_client = to_client;
    st->release = release;	/* XXX eventually set by client */
    CL_INIT(st->timeoutcl, &timeout_ops, st);

    CL_INIT(st->fragpoolcl, &fragpool_ops, st);

    /* sml_alloc is used to allocate asmq and iorecs from.  This means
     * the largest an iorec can get is the size of an asmq_t */
    st->sml_alloc = slaballoc_new(Pvs(heap), sizeof(asmq_t),
				  MAX_ASMQ /*asmq*/ +
				  (AVG_FRAGS*MAX_ASMQ) /*iorecs*/);
    if (!st->sml_alloc)
	RAISE_Heap$NoMemory();

    /* pkt_alloc is used for packet buffers. Eventually this will
     * disappear */
    st->pkt_alloc = slaballoc_new(Pvs(heap), mtu, AVG_FRAGS*MAX_ASMQ);
    if (!st->pkt_alloc)
	RAISE_Heap$NoMemory();
	

    TRC(printf("fragpool_new, state at %p\n", st));

    return &st->fragpoolcl;
}


/* ------------------------------------------------------------ */
/* aqmap helper functions */


static INLINE uint32_t mkhash(const FragPool_key * const key)
{
    uint32_t h;

    /* the thought here is that if "src_addr" and "dst_addr" are quite
     * similar, then the xor will leave the bottom 16 bits zero (since
     * the addresses are in network endian).  These bits are ready to
     * be filled by the "id".  The "proto" goes where the subnet
     * number would be, since hopefully it won't be too different */

    h = (key->src_addr ^ key->dst_addr) ^ key->id ^ (key->proto << 16);

#if 0
    TRC(eprintf("mkhash(%x,%x,%x,%d) = %x\n",
		key->src_addr, key->dst_addr, (int)key->id, (int)key->proto,
		h));
#endif /* 0 */

    return h;
}

/* lookup "key" in "st->aqmap", and return its asmq */
static asmq_t *aq_find(FragPool_st *st, const FragPool_key * const key)
{
    uint32_t hash;
    asmq_t *f = st->aqmap;
    asmq_t *prev = NULL;

    if (!f)
	return NULL;

    hash = mkhash(key);

    while(f)
    {
	if (f->valid &&
	    f->hash == hash &&
	    f->key.src_addr == key->src_addr &&
	    f->key.dst_addr == key->dst_addr &&
	    f->key.id	    == key->id &&
	    f->key.proto    == key->proto)
	{
	    /* reset the timeout */
	    ActivationF$DelTimeout(st->actf, f->deadline, (word_t)f);
	    do {
		f->deadline = NOW() + REASSEMBLY_TIMEOUT;
	    } while(!ActivationF$AddTimeout(st->actf, &st->timeoutcl,
					    f->deadline, (word_t)f));

	    /* if there's stuff in front of us, pull to the front,
             * since we're an active asmq entry */
	    if (prev)
	    {
		prev->next = f->next;
		f->next = st->aqmap;
		st->aqmap = f;
	    }

	    TRC(eprintf("aq_find(%x) -> %p\n", hash, f));
	    return f;
	}

	prev = f;
	f = f->next;
    }

    TRC(eprintf("aq_find(%x) failed\n", hash));
    return NULL;
}




/* Delete "f" from the list of assembly queues, removing associated
 * timeout.  This version needs a "prev" pointer passed in. */
static void aq_basicdel(FragPool_st *st, asmq_t *prev, asmq_t *f)
{    
    int i;
    FragPool_frag *fr;

    TRC(eprintf("aq_basicdel(%x)\n", f->hash));

    if (prev)
	prev->next = f->next;
    else
	st->aqmap = f->next;

    f->next = NULL;
    f->valid = False;

    /* cancel the timeout (don't care if there wasn't one) */
    ActivationF$DelTimeout(st->actf, f->deadline, (word_t)f);
    
    /* release all the fragments in the fraglist */
    for(i=0; i<SEQ_LEN(&f->fl); i++)
    {
	fr = &SEQ_ELEM(&f->fl, i);
	st->release(st, fr->nrecs, fr->recs);
	slab_free(st->sml_alloc, fr->recs);
    }
    SEQ_CLEAR(&f->fl);
}


/* Same as "aq_basicdel", but friendlier (and more costly) */
static bool_t aq_del(FragPool_st *st, asmq_t *dying)
{
    asmq_t *t = st->aqmap;
    asmq_t *prev = NULL;

    /* find the "prev" entry for "f" */
    while(t)
    {
	if (t == dying)
	{
	    aq_basicdel(st, prev, dying);
	    return True;
	}
	prev = t;
	t = t->next;
    }
    return False;	/* didn't find it */
}


/* aq_new() creates a new assembly queue, possibly re-using an
 * existsing (but unused) one */
static asmq_t *aq_new(FragPool_st *st, const FragPool_key * const key)
{
    asmq_t *f = st->aqmap;
    asmq_t *pprev = NULL;
    asmq_t *prev = NULL;
    int count = 0;

    /* try to find an unuses slot to reuse */
    while(f)
    {
	if (!f->valid)
	    break;
	pprev = prev;
	prev = f;
	f = f->next;
	count++;
    }

    /* If no invalid asmq was found, then force the oldest one to be
     * reused */
    if (!f)
    {
	if (count >= MAX_ASMQ)
	{
	    TRC(eprintf("aq_new: reuseing old Q\n"));
	    aq_basicdel(st, pprev, prev);	/* blow the last item */
	    prev->next = st->aqmap;		/* and pull it to the front */
	    st->aqmap = prev;
	    f = prev;	/* now use f as if it were invalid */
	}
	else
	{
	    TRC(eprintf("aq_new: alloc Q\n"));
	    f = slab_alloc(st->sml_alloc);
	    if (!f) return NULL;
	    SEQ_INIT(&f->fl, AVG_FRAGS, Pvs(heap));
	    /* put the new item at the front */
	    f->next = st->aqmap;
	    st->aqmap = f;
	}
    }

    /* initialise new asmq */
    SEQ_CLEAR(&f->fl);
    f->hdl = DELAY;	/* marker for an untasked queue */
    f->key = *key;
    f->hash = mkhash(key);
    f->valid = True;

    /* set the timeout */
    do {
	f->deadline = NOW() + REASSEMBLY_TIMEOUT;
    } while(!ActivationF$AddTimeout(st->actf, &st->timeoutcl,
				    f->deadline, (word_t)f));

    /* up to application to fill in the fraglist */

    TRC(eprintf("aq_new(%x)\n", f->hash));

    return f;
}



/*---------------------------------------------------- Entry Points ----*/

static void
PutPkt_m(
    FragPool_cl     *self,
    uint32_t        nrecs   /* IN */,
    IO_Rec	    *recs   /* IN */,
    PF_Handle	    rxhdl   /* IN */,
    uint32_t	    hdrsz   /* IN */)
{
    FragPool_st	*st = self->st;
    uint8_t	*pkt;
    FragPool_key key;
    int		i;
    asmq_t	*aq;
    FragPool_frag *fr;
    FragPool_frag new;

    /* Since we've already packet filtered this fragment, we know we
     * have at least an IP header, otherwise it would not have been
     * flagged as a fragment */

    pkt  = recs[0].base;
    pkt += 14;  /* step over the Ethernet header */
    key.src_addr = LDU_32(pkt + 12);
    key.dst_addr = LDU_32(pkt + 16);
    key.proto = pkt[9];
    key.id    = *(uint16_t*)(pkt + 4);

    /* locking: mask timeouts while we faff around with the aqmap, so
     * we don't have the thing in our hands get deleted from under our
     * feet */
    ActivationF$MaskTimeouts(st->actf);

    /* do we already know about this IP id? */
    aq = aq_find(st, &key);

    /* if we don't have an assembly queue, then we need a new one */
    if (!aq)
    {
	aq = aq_new(st, &key);
	if (!aq)
	{
	    /* not much we can do, just ditch the packet */
	    eprintf("FragPool: out of asmq memory!\n");
	    ActivationF$UnmaskTimeouts(st->actf);
	    return;
	}
    }


    /* If we have new demux info, note it */
    if (rxhdl != DELAY)
    {
	if (aq->hdl != DELAY)
	{
	    /* Already doing demux for this id, probably bad.  Kill
	     * the assembly queue we have, and reuse it. */
	    eprintf("FragPool: head arrived(%x) while demux in progress(%x)\n",
		    rxhdl, aq->hdl);
	    aq_del(st, aq);
	    aq = aq_new(st, &key);
	    if (!aq)
	    {
		eprintf("FragPool: aq_new() failed, can't happen\n");
		ActivationF$UnmaskTimeouts(st->actf);
		/* XXX release eventually */
		return;
	    }
	}

	aq->hdl = rxhdl;
    }

    /* now deal with the data we have just been given, so that clients
     * get the head fragment first */

    switch(aq->hdl) {
    case FAIL:
	TRC(eprintf("FragPool ditching unwanted\n"));
	/* XXX release eventually */
	break;

    case DELAY:
	TRC(eprintf("FragPool <- packet\n"));
	if (!frag_dup(st, nrecs, recs, &new))
	    break;
	new.hdrsz = hdrsz;
	SEQ_ADDH(&aq->fl, new);
	break;

    default:
	TRC(eprintf("FragPool -> client\n"));
	st->to_client(st->netifst, aq->hdl, nrecs, recs, hdrsz);
	/* XXX st->release() eventually */
	break;
    }

    /* deliver any queued data there might be.  We only need to check
     * if we had new demux info. */
    if (rxhdl != DELAY)
    {
	/* clear any backlog we might have */
	TRC(eprintf("FragPool %s backlog of %d\n",
		    aq->hdl? "flush" : "ditch",
		    SEQ_LEN(&aq->fl)));

	for(i=0; i<SEQ_LEN(&aq->fl); i++)
	{
	    fr = &SEQ_ELEM(&aq->fl, i);
	    if (aq->hdl)	/* only send to client if not FAIL */
		st->to_client(st->netifst, aq->hdl, fr->nrecs, fr->recs,
			      fr->hdrsz);
	    st->release(st, fr->nrecs, fr->recs);
	    slab_free(st->sml_alloc, fr->recs);
	}
	SEQ_CLEAR(&aq->fl);
    }

    ActivationF$UnmaskTimeouts(st->actf);
}



/* Timeout_m is called when an assembly queue in the aqmap times out.  It is
 * called with activations off */
static void
Timeout_m(TimeNotify_cl *self, Time_T now, Time_T deadline, word_t handle)
{
    FragPool_st *st = self->st;
    asmq_t *dying = (void*)handle;

    TRC(eprintf("FragPool:Timeout %x\n", dying->hash));

    if (!aq_del(st, dying))
	printf("FragPool:Timeout_m: item no longer in existence, "
	       "can't happen\n");
}




static bool_t frag_dup(FragPool_st *st, uint32_t nrecs, IO_Rec *recs,
		       FragPool_frag *new)
{
    int i;

    if ((sizeof(IO_Rec) * nrecs) > sizeof(asmq_t))
    {
	eprintf("frag_dup: not enough room for %d recs (%d>%d)\n",
		nrecs, sizeof(IO_Rec)*nrecs, sizeof(asmq_t));
	return False;
    }

    /* duplicate the meta-data */
    new->nrecs = nrecs;
    new->recs = slab_alloc(st->sml_alloc);
    if (!new->recs)
    {
	eprintf("frag_dup: no sml alloc memory left!\n");
	return False;
    }

    /* duplicate the data areas */
    for(i=0; i<nrecs; i++)
    {
	new->recs[i].len = recs[i].len;
	new->recs[i].base = slab_alloc(st->pkt_alloc);
	if (!new->recs[i].base)
	    goto fail;
	memcpy(new->recs[i].base, recs[i].base, recs[i].len);
    }

    return True;

fail:
    {
	int j;
	eprintf("frag_dup: out of packet memory!\n");
	for(j=0; j<i; j++)
	    slab_free(st->pkt_alloc, new->recs[j].base);
	slab_free(st->sml_alloc, new->recs);
	return False;
    }
}


/* Free the packet buffers. */
static void release(void *state, uint32_t nrecs, IO_Rec *recs)
{
    int i;
    FragPool_st *st = state;

    for(i=0; i<nrecs; i++)
	slab_free(st->pkt_alloc, recs[i].base);

    /* NOTE: doesn't free the backing for the recs array, that's
     * considered meta-data maintained by the pool, not the pool's
     * client */
}



/*------------------------------------------------------------*/
/* Transmit functions */


static bool_t 
TXFilt_m(FragPool_cl      *self,
	 FragPool_key 	  *key     /* INOUT */,
	 addr_t		   base    /* IN */,
	 uint32_t          len     /* IN */)
{
    uint8_t *pkt = base;
    FragPool_key pk;
    uint16_t flagsOffset;

    if (len < (16+14)) return False; /* too short */

    pk.src_addr = LDU_32(pkt + 12 + 14);
    pk.dst_addr = LDU_32(pkt + 16 + 14);
    pk.proto = pkt[9 + 14];
    pk.id    = *(uint16_t*)(pkt + 4 + 14);

    flagsOffset = *(uint16_t*)(pkt + 6 + 14);

#if 0
    eprintf("TXFilt: s:%08x d:%08x p:%02x id:%04d, offset:%04x\n",
	    ntohl(pk.src_addr), ntohl(pk.dst_addr),
	    pk.proto, ntohs(pk.id), ntohs(flagsOffset));
#endif

    /* if this is a head, then set the key and allow the TX */
    if ((flagsOffset & OFFMASK) == 0)
    {
	*key = pk;
	return True;
    }

    /* if the cap fits... */
    if (key->id       == pk.id &&
	key->src_addr == pk.src_addr &&
	key->dst_addr == pk.dst_addr &&
	key->proto    == pk.proto)
    {
	return True;	/* ... wear it */
    }
    else
    {
	eprintf("fail\n");
	return False;
    }
}



/* End of pool.c */
