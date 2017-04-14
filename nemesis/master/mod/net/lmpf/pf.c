/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/net/lmpf/pf.c
**
** FUNCTIONAL DESCRIPTION:
** 
**      Lean, Mean, Packet filtering.
** 
** ENVIRONMENT: 
**
**      User space.
** 
** FILE :	 pf.c
** CREATED :	 Mon Jun  2 1997
** AUTHOR :	 Steven Hand (Steven.Hand@cl.cam.ac.uk)
** ORGANISATION: University of Cambridge Computer Laboratory
** ID : 	 $Id: pf.c 1.2 Tue, 04 May 1999 18:46:16 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <mutex.h>
#include <ether.h>
#include <iana.h>
#include <string.h>
#include <bstring.h>

#include <IO.ih>
#include <Net.ih>
#include <PF.ih>
#include <LMPFMod.ih>
#include <LMPFCtl.ih>


#ifdef DEBUG
#define TX_DEBUG
#define RX_DEBUG
#define GEN_DEBUG
#endif

#ifdef GEN_DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef TX_DEBUG
#define TXTRC(_x) _x
#else
#define TXTRC(_x)
#endif

#ifdef RX_DEBUG
#define RXTRC(_x) _x
#else
#define RXTRC(_x)
#endif

#define DB(x)  x


#ifdef __ALPHA__
#if 0
#define LDU_32(_addr) \
({						\
    uint32_t _res;                              \
    __asm__ volatile (" ldq_u %0, 0(%1)" 	\
		      : "=r" (_res) 		\
		      : "r"  (_addr));		\
    _res; 					\
})
#endif
#endif
#define LDU_32(_addr) \
({									\
    addr_t _a = (_addr);						\
  *((uint16_t *)(_a)) | ((*((uint16_t *)((char *)(_a) + 2))) << 16);	\
})


#define ASSERT_LEN(_x) \
if (len < _x)								   \
{									   \
    printf(__FUNCTION__ " failed: runt packet (%d < " #_x "\n", len); \
    return FAIL;							   \
}



#define TAB_SIZE 1024 /* random guess for now */




/* PF_Handle special returns */
#define FAIL  ((PF_Handle)0)
#define DELAY ((PF_Handle)-1)


/* both of these are in network endian */
#define MF	0x0020	/* More Fragments bit */
#define OFFMASK 0xff1f	/* fragment offset mask */


/*
** Three different tables for udp packet filtering, called in order of 
** specificity and lean mean. ETc. 
*/

    
/* tcp flag bits */
#define TCP_FIN  0x01
#define TCP_SYN  0x02
#define TCP_RST  0x04
#define TCP_PUSH 0x08
#define TCP_ACK  0x10
#define TCP_URG  0x20


typedef struct _pf_st {
    PF_cl	cl;
    LMPFCtl_cl	ctlcl;
    uint32_t  slen; 
    PF_Handle arp;
#if 0
    PF_Handle bootp;
    Net_EtherAddr hwaddr; /* hwaddr for bootp */
#endif
    PF_Handle icmp;
    PF_Handle msnl;
    PF_Handle def_handle;   /* thiemo's default handle, greps all ip */
    /* three udp tables for the various possibilities */
    /* lport, fport, faddr */
    struct {
	mutex_t   lock;
	uint32_t  sz;
	PF_Handle hdl[TAB_SIZE];
	struct {
	    uint32_t port_pair;
	    uint32_t src_addr;
	} tab[TAB_SIZE];
    } udptab3;
    /* lport, fport */ 
    struct {
	mutex_t   lock;
	uint32_t  sz;
	PF_Handle hdl[TAB_SIZE];
	struct {
	    uint32_t port_pair;
	} tab[TAB_SIZE];
    } udptab2;
    /* lport */
    struct {
	mutex_t   lock;
	uint32_t  sz;
	PF_Handle hdl[TAB_SIZE];
	struct {
	    uint16_t dest_port;
	} tab[TAB_SIZE];
    } udptab1;
    /* + two tcp tables: one for connected things, and one for listens. */
    /* connected */
    struct {
	mutex_t   lock;
	uint32_t  sz;
	PF_Handle hdl[TAB_SIZE];
	struct {
	    uint32_t port_pair;
	    uint32_t src_addr;
	} tab[TAB_SIZE];
    } tcpconns;
    /* listening */
    struct {    
	mutex_t   lock;
	uint32_t  sz;
	PF_Handle hdl[TAB_SIZE];
	struct {
	    uint16_t dest_port;
	} tab[TAB_SIZE];
    } tcplistens;
} PF_st;



/* Packet template used for transmit filtering */
struct ethip {
    uint8_t	pad[2];
    /* ethernet header */
    uint8_t	destmac[6];
    uint8_t	srcmac[6];
    uint16_t	frametype;
    /* IP header */
    uint8_t	vershlen;
    uint8_t	servicetype;
    uint16_t	totlen;
    uint16_t	ident;
    uint16_t	frag;
    uint8_t	ttl;
    uint8_t	protocol;
    uint16_t	chksum;
    uint32_t	srcip;
    uint32_t	destip;
    /* UDP or TCP (they're conveniently the same) */
    uint16_t	srcport;
    uint16_t	destport;
    /* ... etc etc ... */
};
    
#define MAX_FILTER_LEN 128

typedef struct {
    uint32_t		snaplen;	/* minimum required number of bytes */
    /* if allowFrags is True, the snaplen actually checked is slightly
     * lower (the ports aren't checked) if this looks like its an IP
     * fragment body */
    bool_t		allowFrags;
    union {
	struct ethip	template;
	uint8_t		pad[MAX_FILTER_LEN];
    } u;
    PF_cl		cl;
} txpf_st;


#ifdef DEBUG 
#undef INLINE
#define INLINE
#endif

#define ACTOFF() VP$ActivationsOff(Pvs(vp))
#define ACTON() VP$ActivationsOn(Pvs(vp))


INLINE PF_Handle eth_filter(PF_st *sef, struct ether_hdr *epkt, word_t len,
			    bool_t *frag, uint32_t *hdrsz);



/* ------------------------ module things ------------------------- */

/* PF datapath interface */
static PF_Apply_fn Apply_m;
static PF_Snaplen_fn Snaplen_m;
static PF_Dispose_fn Dispose_m;
static PF_CopyApply_fn CopyApplyRX_m;
static PF_op pf_ops = {
    Apply_m,
    Snaplen_m,
    Dispose_m,
    CopyApplyRX_m,
};

/* Transmit PF datapath interface */
static PF_Apply_fn ApplyTX_m;
static PF_Snaplen_fn SnaplenTX_m;
static PF_Dispose_fn DisposeTX_m;
static PF_CopyApply_fn CopyApplyTX_m;
static PF_op txpf_ops = {
    ApplyTX_m,
    SnaplenTX_m,
    DisposeTX_m,
    CopyApplyTX_m,
};

/* LMPFCtl control interface */
static LMPFCtl_SetDefault_fn    SetDefault_m;
static LMPFCtl_SetARP_fn	SetARP_m;
static LMPFCtl_SetICMP_fn	SetICMP_m;
static LMPFCtl_SetMSNL_fn	SetMSNL_m;
static LMPFCtl_AddUDP_fn	AddUDP_m;
static LMPFCtl_DelUDP_fn	DelUDP_m;
static LMPFCtl_AddTCP_fn	AddTCP_m;
static LMPFCtl_DelTCP_fn	DelTCP_m;
static  LMPFCtl_op ctl_ops = {
    SetDefault_m,
    SetARP_m,
    SetICMP_m,
    SetMSNL_m,
    AddUDP_m,
    DelUDP_m,
    AddTCP_m,
    DelTCP_m
};

/* LMPFMod parent interface */
static LMPFMod_NewRX_fn LMPFMod_NewRX_m;
static LMPFMod_NewTX_fn LMPFMod_NewTX_m;
static LMPFMod_NewTXICMP_fn LMPFMod_NewTXICMP_m;
static LMPFMod_NewTXARP_fn LMPFMod_NewTXARP_m;
static LMPFMod_NewTXDef_fn LMPFMod_NewTXDef_m;
static LMPFMod_op mod_ms = {
    LMPFMod_NewRX_m,
    LMPFMod_NewTX_m,
    LMPFMod_NewTXICMP_m,
    LMPFMod_NewTXARP_m,
    LMPFMod_NewTXDef_m
};
static const LMPFMod_cl mod_cl = { &mod_ms, NULL };
CL_EXPORT (LMPFMod, LMPFMod, mod_cl);



/* ------------------------------------------------------------ */
/* Initialisation */

static PF_clp LMPFMod_NewRX_m(LMPFMod_cl *self, /* RETURNS */LMPFCtl_clp  *ctl)
{
    PF_st *st; 

    st = Heap$Malloc(Pvs(heap), sizeof(*st));
    if (!st)
    {
	*ctl = NULL;
	return NULL;
    }

    bzero(st, sizeof(*st));

    CL_INIT(st->cl, &pf_ops, st);
    CL_INIT(st->ctlcl, &ctl_ops, st); /* same state, different methods */

    *ctl = &st->ctlcl;
    return &st->cl;    
}


#define TX_SNAPLEN (sizeof(struct ethip) - 2) /* -2 for pad */

static PF_clp LMPFMod_NewTX_m(LMPFMod_cl *self,
			      const Net_EtherAddr *src_mac,
			      uint32_t src_ip,
			      uint8_t protocol,
			      uint16_t src_port)
{
    txpf_st *st;

    st = Heap$Malloc(Pvs(heap), sizeof(txpf_st));
    if (!st)
	return NULL;

    TXTRC(printf("NewTX filter: srcip=%08x, proto=%02x, srcport=%04x\n",
		 ntohl(src_ip), protocol, ntohs(src_port)));

    memset(st, 0, sizeof(txpf_st));

    /* write the template out */
    memcpy(st->u.template.srcmac, src_mac, 6);
    st->snaplen = TX_SNAPLEN;	/* full size of the structure */
    st->allowFrags = True;	/* relax snaplen check a little */
    st->u.template.frametype = FRAME_IPv4;
    st->u.template.srcip = src_ip;
    st->u.template.protocol = protocol;
    st->u.template.srcport = src_port;

    CL_INIT(st->cl, &txpf_ops, st);

    return &st->cl;
}

static PF_clp LMPFMod_NewTXDef_m(LMPFMod_cl *self,
			      const Net_EtherAddr *src_mac,
			      uint32_t src_ip)
{
    txpf_st *st;

    st = Heap$Malloc(Pvs(heap), sizeof(txpf_st));
    if (!st)
	return NULL;

    TXTRC(printf("NewTXDef filter: srcip=%08x, "
		 "mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
		 ntohl(src_ip),
		 src_mac->a[0], src_mac->a[1], src_mac->a[2], 
		 src_mac->a[3], src_mac->a[4], src_mac->a[5]));
    

    memset(st, 0, sizeof(txpf_st));

    /* write the template out */
    memcpy(st->u.template.srcmac, src_mac, 6);
    st->snaplen = 20;	/* IP header XXX does this work ? */
    st->u.template.frametype = FRAME_IPv4;
    st->u.template.srcip = src_ip;


    CL_INIT(st->cl, &txpf_ops, st);

    return &st->cl;
}


static PF_clp LMPFMod_NewTXICMP_m(LMPFMod_cl *self,
				  const Net_EtherAddr *src_mac,
				  uint32_t src_ip)
{
    txpf_st    *st = self->st;

    st = Heap$Malloc(Pvs(heap), sizeof(txpf_st));
    if (!st)
	return NULL;

    memset(st, 0, sizeof(txpf_st));

    /* write the template out */
    memcpy(st->u.template.srcmac, src_mac, 6);
    st->snaplen = 34;  /* size of everything in the IP header */
    st->allowFrags = False;
    st->u.template.frametype = FRAME_IPv4;
    st->u.template.srcip = src_ip;
    st->u.template.protocol = IP_PROTO_ICMP;
    st->u.template.srcport = 0; /* not important for ICMP */

    CL_INIT(st->cl, &txpf_ops, st);

    return &st->cl;
}


static PF_clp LMPFMod_NewTXARP_m(LMPFMod_cl *self,
				 const Net_EtherAddr *src_mac)
{
    txpf_st    *st = self->st;

    st = Heap$Malloc(Pvs(heap), sizeof(txpf_st));
    if (!st)
	return NULL;

    memset(st, 0, sizeof(txpf_st));

    /* write the template out */
    memcpy(st->u.template.srcmac, src_mac, 6);
    st->snaplen = 28; /* size of an ARP packet */
    st->allowFrags = False;
    st->u.template.frametype = FRAME_ARP;    

    CL_INIT(st->cl, &txpf_ops, st);

    return &st->cl;
}


/* ------------------------------------------------------------ */
/* PF datapath operations */

static PF_Handle Apply_m(PF_cl *self, IO_Rec *p, uint32_t nrecs, bool_t *frag,
			 uint32_t *hdrsz)
{
    /* should switch on type of interface or something */
    /* XXX deal with multi recs */
    RXTRC(printf("PF$Apply called: p[0].base is %p, p[0].len is %x\n", 
		 p[0].base, p[0].len));
    (*frag) = False;  /* default */
    return eth_filter(self->st, (struct ether_hdr *)(p[0].base), p[0].len,
		      frag, hdrsz);
}

static uint32_t Snaplen_m(PF_cl *self)
{
    PF_st *st = (PF_st *)self->st;
    return st->slen;
}

static void Dispose_m(PF_cl *self)
{
    /* erm */
    printf("pf: %s: UNIMPLEMENTED", __FUNCTION__);
    return;
}

static PF_Handle CopyApplyRX_m(PF_cl *self, IO_Rec *p, uint32_t nrecs,
			       addr_t dest, bool_t *frag, uint32_t *hdrsz)
{
    printf("pf: %s: UNIMPLEMENTED", __FUNCTION__);
    return FAIL;
}

/* ------------------------------------------------------------ */
/* Transmit PF datapath operations */

static PF_Handle ApplyTX_m(PF_cl *self, IO_Rec *p, uint32_t nrecs,
			   bool_t *frag, uint32_t *hdrsz)
{
    printf("pf: %s: UNIMPLEMENTED", __FUNCTION__);
    (*frag) = False;
    return FAIL;
}

static PF_Handle CopyApplyTX_m(PF_cl *self, IO_Rec *p, uint32_t nrecs,
			       addr_t dest, bool_t *frag, uint32_t *hdrsz)
{
    txpf_st *st = self->st;
    uint8_t *d = dest;
    uint8_t *t = st->u.template.destmac; /* start of real data */
    uint8_t *s;  /* source */
    int packetlen;
    int checklen;	/* where to finish checking */
    int copylen;

    TXTRC(printf("CopyApplyTX\n"));

    if (nrecs < 1)
    {
	printf("PF$ApplyTX: no data? (nrecs=%d)\n", nrecs);
	return FAIL;
    }

    packetlen = p[0].len;
    s         = p[0].base;

    (*frag) = False;  /* default */

    /* normally, we want to check the entire template */
    checklen = st->snaplen;

    /* but if we're going to allow frags, then if this is a body
     * fragment, let them get away with a smaller checklen (ie, miss out
     * the check on the ports) */
    if (st->allowFrags)
    {
	uint16_t flags;

	/* make sure we're allowed to look into the IP header */
	if (packetlen < 22)
	{
	    printf("PF$ApplyTX: not enough IP data (%d)\n", packetlen);
	    return FAIL;
	}

	/* +20 bytes gets to IP fragflags and offset */
	flags = *(uint16_t*)(s+20);
	if ((flags & (OFFMASK|MF)) != 0)
	    (*frag) = True;

	/* if this is a body, then don't check the UDP/TCP ports */ 
	if (flags & OFFMASK)
	    checklen -= 4;
    }    

    /* Have we enough data? */
    if (packetlen < checklen)
    {
	printf("PF$ApplyTX: not enough data (%d < %d)\n", packetlen, checklen);
	return FAIL;
    }

    /* but not too much */
    if (packetlen > MAX_FILTER_LEN)
    {
	printf("PF$CopyApplyTX: rec[0] is too long (%d bytes)\n", packetlen);
	return FAIL;
    }

    /* if the client supplied more data than we're going to check,
     * just copy the rest */
    copylen = packetlen - checklen;

    while(checklen--)
    {
	TXTRC(printf(": %02x = %02x?\n", *s, *t));
	if ((*d=*s) != *t && *t)
	{
	    TXTRC(printf(": FAIL\n"));
	    return FAIL;
	}
	d++;
	s++;
	t++;
    }
    while(copylen--)
    {
	TXTRC(printf(": %02x\n", *s));
	*d = *s;
	d++;
	s++;
    }


    /* check the IP checksum if this is IP */
#ifdef IPHDR_PARANOIA
    {
	uint16_t *b;
	int i;
	uint32_t sum = 0;
	b = (uint16_t*)(((char *)(p[0].base)) + 14);
	for(i=0; i<10; i++)
	{
//	    eprintf("%04x ", (int)htons(b[i]));
	    sum += htons(b[i]);
	}
	sum = (sum & 0xffff) | (sum >> 16);
	sum = (sum & 0xffff) | (sum >> 16);
	if (sum != 0xffff)
	    eprintf("pf: failed chksum %04x\n", sum);
    }
#endif

    return (PF_Handle)1;
}


#if 0
static PF_Handle ApplyTX_m(PF_cl *self, IO_Rec *p, uint32_t nrecs)
{
    txpf_st *st = self->st;
    struct ethip *pkt;
    uint32_t	t32;
    uint32_t	t32b;
    uint16_t	t16;

    TXTRC(printf("TXPF$Apply called: p[0].base is %p, p[0].len is %x\n", 
		 p[0].base, p[0].len));

    if (nrecs < 1)
    {
	printf("PF$ApplyTX: no data? (nrecs=%d)\n", nrecs);
	return FAIL;
    }

    /* Have we enough data? */
    if (p[0].len < TX_SNAPLEN)
    {
	TXTRC(printf("PF$ApplyTX: not enough data (%d < %d)\n",
		     p[0].len, TX_SNAPLEN));
	return FAIL;
    }

    pkt = p[0].base - 2;

    /* check the source MAC */
    t16 = *(uint16_t *)(pkt->srcmac);
    if (t16 != *(uint16_t*)st->mac)
    {
	TXTRC(printf("PF$ApplyTX: bad source mac, first 2 bytes "
		     "(%02x != %02x)\n", t16, *(uint16_t*)st->mac));
	return FAIL;
    }

    /* check last 4 bytes of src mac */
    t32 = *(uint32_t *)(&pkt->srcmac[2]);
    t32b = LDU_32( (((char*)st->mac)+2) );
    if (t32 != t32b)
    {
	TXTRC(printf("PF$ApplyTX: bad source mac or frametype "
		     "(%04x != %04x)\n",
		     t32, t32b));
	return FAIL;
    }

    /* IP version and header length */
    if (pkt->vershlen != 0x45) /* IPv4, 20bytes of header */
    {
	TRC(printf("PF$ApplyTX: bad IP version or header length "
		   "(%02x != 45)\n", pkt->vershlen));
	/* XXX should really bail to BPF if the header length is the
         * only thing that's wrong */
	return FAIL;
    }

    if (pkt->protocol != st->protocol)
    {
	TXTRC(printf("PF$ApplyTX: bad protocol (%#02x != %#02x)\n",
		     pkt->protocol, st->protocol));
	return FAIL;
    }

    t32 = LDU_32(&pkt->srcip);
    if (t32 != st->src_ip)
    {
	TXTRC(printf("PF$ApplyTX: bad src IP (%08x != %08x)\n",
		     pkt->srcip, st->src_ip));
	return FAIL;
    }

    if (pkt->srcport != st->src_port)
    {
	TXTRC(printf("PF$ApplyTX: bad src port (%#04x != %#04x)\n",
		     pkt->srcport, st->src_port));
	return FAIL;
    }

    return (PF_Handle)1; /* Accept */
}
#endif /* 0 */


static uint32_t SnaplenTX_m(PF_cl *self)
{
    txpf_st *st = self->st;
    return st->snaplen;
}

static void DisposeTX_m(PF_cl *self)
{
    Heap$Free(Pvs(heap), self->st);
    return;
}


/* ------------------------------------------------------------ */
/* Control functions */

bool_t SetDefault_m(LMPFCtl_cl *self, PF_Handle def)
{   
    PF_st *st = (PF_st *)self->st;

    st->def_handle = def;
    st->slen = 128;
    TRC(printf("PF: setting length for default %d\n", st->slen));
    return True;
}

bool_t SetARP_m(LMPFCtl_cl *self, PF_Handle arp)
{
    PF_st *st = (PF_st *)self->st;
    st->arp = arp;
    st->slen = MAX(st->slen, 14);
    return True;
}


bool_t SetICMP_m(LMPFCtl_cl *self, PF_Handle icmp, uint32_t ipaddr)
{
    PF_st *st = (PF_st *)self->st;
    st->icmp = icmp;
    st->slen = MAX(st->slen, 14);
    /* XXX single ICMP handler only: ipaddr is ignored! */
    return True;
}
    

bool_t SetMSNL_m(LMPFCtl_cl *self, PF_Handle msnl)
{
    PF_st *st = (PF_st *)self->st;
    st->msnl = msnl;
    st->slen = MAX(st->slen, 14);
    return True;
}

/*
** add_udp() is used to insert a receive filter for one of the three forms
** of udp packets; for each param a '0' means "don't care". 
** Note that the names of the params are as defn'd in terms of the rxed
** packet, and hence 'src_addr' means the IP address of the sender. Clear?
*/
bool_t AddUDP_m(LMPFCtl_cl *self, uint16_t dest_port, uint16_t src_port, 
		uint32_t src_addr, PF_Handle hdl)
{
    PF_st *st = (PF_st *)self->st;
    uint16_t *ppp;

    st->slen = MAX(st->slen, 54); /* XXX encaps/tunnel XXX */
    if(dest_port) {
	if(src_port) {
	    if(src_addr) {  /* tab 3 */
		if(st->udptab3.sz == TAB_SIZE)
		    return False;
		st->udptab3.hdl[st->udptab3.sz] = hdl;
		ppp = (uint16_t*)&(st->udptab3.tab[st->udptab3.sz].port_pair);
		ppp[0] = src_port;
		ppp[1] = dest_port;
		TRC(printf("add_udp: port_pair=%x, src_addr=%x\n", 
			st->udptab3.tab[st->udptab3.sz].port_pair, 
			    ntoh32(src_addr)));
		st->udptab3.tab[st->udptab3.sz].src_addr = src_addr;
		st->udptab3.sz++;
		return True;
	    } else {
		/* tab 2 */
		if(st->udptab2.sz == TAB_SIZE)
		    return False;
		st->udptab2.hdl[st->udptab2.sz] = hdl;
		ppp = (uint16_t*)&(st->udptab2.tab[st->udptab2.sz].port_pair);
		TRC(printf("add_udp: port_pair=%x\n", 
			st->udptab3.tab[st->udptab3.sz].port_pair));
		ppp[0] = src_port;
		ppp[1] = dest_port;
		st->udptab2.sz++;
		return True;
	    }
	} else {
	    /* tab 1 */
	    if(st->udptab1.sz == TAB_SIZE)
		return False;
	    st->udptab1.hdl[st->udptab1.sz] = hdl;
	    st->udptab1.tab[st->udptab1.sz].dest_port = dest_port;
	    TRC(printf("add_udp: dest_port=%x\n", ntoh16(dest_port)));
	    st->udptab1.sz++;
	    return True;
	}
    }

    /* die : NB have potentially trashed slen now */
    return False;
}


bool_t DelUDP_m(LMPFCtl_cl *self, uint16_t dest_port, uint16_t src_port, 
		uint32_t src_addr, PF_Handle hdl)
{
    PF_st *st = (PF_st *)self->st;
    uint32_t port_pair;
    bool_t found = False;
    int i;

    TRC(printf("LMPF$DelUDP: src:%08x/%04x dest:%04x\n",
	       ntohl(src_addr), ntohs(src_port), ntohs(dest_port)));

    if(dest_port) {
	if(src_port) {

	    /* XXX endianness */
	    port_pair = (dest_port << 16) | src_port;

	    if(src_addr) {  /* tab 3 */

		if(!st->udptab3.sz) 
		    return False;

		src_addr  = src_addr;
		for(i=0; i < st->udptab3.sz ; i++) {
		    if((st->udptab3.tab[i].src_addr  == src_addr) && 
		       (st->udptab3.tab[i].port_pair == port_pair) &&
		       (st->udptab3.hdl[i] == hdl) ) {
			found= True;
			break;
		    }
		}

		if(!found) 
		    return False;
		
		st->udptab3.sz--;
		for(; i < st->udptab3.sz; i++) {
		    st->udptab3.tab[i].src_addr  = 
			st->udptab3.tab[i+1].src_addr;
		    st->udptab3.tab[i].port_pair = 
			st->udptab3.tab[i+1].port_pair; 
		    st->udptab3.hdl[i] = st->udptab3.hdl[i+1];
		}

		return True;

	    } else {
		/* tab 2 */
		if(!st->udptab2.sz) 
		    return False;

		for(i=0; i < st->udptab2.sz ; i++) {
		    if((st->udptab2.tab[i].port_pair == port_pair) &&
		       (st->udptab2.hdl[i] == hdl) ) {
			found= True;
			break;
		    }
		}
		
		if(!found) 
		    return False;
		
		st->udptab2.sz--;
		for(; i < st->udptab2.sz; i++) {
		    st->udptab2.tab[i].port_pair = 
			st->udptab2.tab[i+1].port_pair; 
		    st->udptab2.hdl[i] = st->udptab2.hdl[i+1];
		}
		
		return True;


	    }
	} else {
	    /* tab 1 */
	    if(!st->udptab1.sz)
		return False;

	    for(i=0; i < st->udptab1.sz ; i++) {
		if((st->udptab1.tab[i].dest_port == dest_port) &&
		   (st->udptab1.hdl[i] == hdl) ) {
		    found= True;
		    break;
		}
	    }
	    
	    if(!found) 
		return False;
	    
	    st->udptab1.sz--;
	    for(; i < st->udptab1.sz; i++) {
		st->udptab1.tab[i].dest_port = st->udptab1.tab[i+1].dest_port; 
		st->udptab1.hdl[i] = st->udptab1.hdl[i+1];
	    }
	    
	    return True;
	}
    }
    
    return False;
}


bool_t AddTCP_m(LMPFCtl_cl *self, uint16_t dest_port, uint16_t src_port, 
		uint32_t src_addr, PF_Handle hdl)
{
    PF_st *st = (PF_st *)self->st;
    uint16_t *ppp;

    st->slen = MAX(st->slen, 66); /* XXX encaps/tunnel XXX */

    if(dest_port) {
	if(src_port && src_addr) { /* tcpconn */
	    if(st->tcpconns.sz == TAB_SIZE)
		return False;

	    st->tcpconns.hdl[st->tcpconns.sz] = hdl;

	    ppp = (uint16_t*)&(st->tcpconns.tab[st->tcpconns.sz].port_pair);
	    ppp[0] = src_port;
	    ppp[1] = dest_port;
	    TRC(printf("add_tcp (conn): dstprt=%x, srcprt=%x, srcaddr=%x\n", 
		    ntoh16(dest_port), ntoh16(src_port), ntoh32(src_addr)));
	    TRC(printf("              : port_pair is %x && handle is %p\n", 
		    st->tcpconns.tab[st->tcpconns.sz].port_pair, hdl));
	    st->tcpconns.tab[st->tcpconns.sz].src_addr = src_addr;
	    st->tcpconns.sz++;

	    return True;
	} else { /* tcplisten */
	    if(st->tcplistens.sz == TAB_SIZE)
		return False;
	    st->tcplistens.hdl[st->tcplistens.sz] = hdl;
	    st->tcplistens.tab[st->tcplistens.sz].dest_port= dest_port;
	    st->tcplistens.sz++;
	    TRC(printf("add_tcp (listen): dstprt=%x && handle is %p\n", 
		    ntoh16(dest_port), hdl));
	    return True;
	}
    }

    /* die : nb have potentially trashed slen */
    return False;
}


bool_t DelTCP_m(LMPFCtl_cl *self, uint16_t dest_port, uint16_t src_port, 
		uint32_t src_addr, PF_Handle hdl)
{
    PF_st *st = (PF_st *)self->st;
    bool_t found = False;
    uint32_t port_pair;
    int i;

    if(dest_port) {
	if(src_port && src_addr) { /* tcpconn */

	    if(!st->tcpconns.sz)
		return False;

	    port_pair = (dest_port << 16) | src_port;

	    for(i=0; i < st->tcpconns.sz; i++) {
		if((st->tcpconns.tab[i].port_pair == port_pair) &&
		   (st->tcpconns.tab[i].src_addr  == src_addr) && 
		   (st->tcpconns.hdl[i] == hdl) ) {
		    found= True;
		    break;
		}
	    }

	    if(!found) 
		return False;
		
	    st->tcpconns.sz--;
	    for(; i < st->tcpconns.sz; i++) {
		st->tcpconns.tab[i].port_pair = 
		    st->tcpconns.tab[i+1].port_pair;
		st->tcpconns.tab[i].src_addr = 
		    st->tcpconns.tab[i+1].src_addr;
		st->tcpconns.hdl[i] = st->tcpconns.hdl[i+1];
	    }

	    return True;
	} else { /* tcplisten */

	    if(!st->tcplistens.sz)
		return False;


	    for(i=0; i < st->tcplistens.sz; i++) {
		if((st->tcplistens.tab[i].dest_port == dest_port) &&
		   (st->tcplistens.hdl[i] == hdl) ) {
		    found= True;
		    break;
		}
	    }

	    if(!found) 
		return False;
		
	    st->tcplistens.sz--;
	    for(; i < st->tcplistens.sz; i++) {
		st->tcplistens.tab[i].dest_port = 
		    st->tcplistens.tab[i+1].dest_port;
		st->tcplistens.hdl[i] = st->tcplistens.hdl[i+1];
	    }

	    return True;
	}
    }

    return False;
}




/* ----------------------- internal (aka filtering) stuff ---------- */

					    
INLINE PF_Handle udp_filter(PF_st *st, char *udp_pkt, uint32_t src_addr,
			    word_t len, uint32_t *hdrsz)
{
    int i;
    uint32_t port_pair;
    uint16_t dest_port;

    ASSERT_LEN(8);

    *hdrsz += 8;

    port_pair  = LDU_32(udp_pkt);
    dest_port  = *((uint16_t *)(udp_pkt+2));

    /* check full matches */
    for(i= 0; i < st->udptab3.sz; i++) 
    {
	RXTRC(printf("=3: pair=%08x, src_addr=%08x\n",
		     st->udptab3.tab[i].port_pair,
		     st->udptab3.tab[i].src_addr));
	if(st->udptab3.tab[i].port_pair == port_pair &&
	   st->udptab3.tab[i].src_addr == src_addr) {
	    RXTRC(printf("udp_filter3: dest_port=%x returning handle at %p\n",
			 dest_port, st->udptab3.hdl[i]));
	    return st->udptab3.hdl[i];
	}
    }

    /* check port pair things */
    for(i= 0; i < st->udptab2.sz; i++) 
	if(st->udptab2.tab[i].port_pair == port_pair) {
	    RXTRC(printf("udp_filter2: dest_port=%x returning handle at %p\n",
			 dest_port, st->udptab2.hdl[i]));
	    return st->udptab2.hdl[i];
	}
    
    /* check just lport stuff */
    for(i= 0; i < st->udptab1.sz; i++)
    {
	RXTRC(printf("=1: %04x\n", st->udptab1.tab[i].dest_port));
	if(st->udptab1.tab[i].dest_port == dest_port) {
	    RXTRC(printf("udp_filter1: dest_port=%x returning handle at %p\n",
			 dest_port, st->udptab1.hdl[i]));
	    return st->udptab1.hdl[i];
	}
    }
    
    RXTRC(printf("udp_filter failed: port_pair=%x, dest_port=%x, src_addr=%x\n",
		 port_pair, dest_port, src_addr));
    return FAIL;
}


INLINE PF_Handle tcp_filter(PF_st *st, char *tcp_pkt, uint32_t src_addr,
			    word_t len, uint32_t *hdrsz)
{
    int i;
    uint32_t port_pair;
    uint16_t dest_port;
    uint8_t  flags;
    uint32_t seq;
    uint32_t ack;

    ASSERT_LEN(20);

    /* add in the tcp header length */
    *hdrsz += ((*(uint8_t*)(tcp_pkt+12)) & 0xf0) >> 2;

    port_pair  = LDU_32(tcp_pkt);
    dest_port  = *((uint16_t *)(tcp_pkt+2));
    flags      = tcp_pkt[13];
    seq        = ntoh32(LDU_32(tcp_pkt+4));
    ack        = ntoh32(LDU_32(tcp_pkt+8));

    RXTRC(printf("tcp_filter, seq=%d, ack=%d\n", seq, ack));
    /* check connected things */
    for(i= 0; i < st->tcpconns.sz; i++) 
	if(st->tcpconns.tab[i].port_pair == port_pair &&
	   st->tcpconns.tab[i].src_addr == src_addr) {
	    RXTRC(printf(
		"tcp_filter: got connection port_pair=%x, src_addr=%x\n", 
		port_pair, src_addr));
	    RXTRC(printf("          : returning handle at %p\n", 
			 st->tcpconns.hdl[i]));
	    return st->tcpconns.hdl[i];
	}

    /* check listening things: want SYN=1,ACK=0 */
    for(i= 0; i < st->tcplistens.sz; i++)
#if 1
	if(st->tcplistens.tab[i].dest_port == dest_port) {
#else
	if(st->tcplistens.tab[i].dest_port == dest_port &&
	   (flags & (TCP_SYN|TCP_ACK)) == TCP_SYN) {
#endif
	    RXTRC(printf("tcp_filter: listen succeeds on port %x\n", 
			 dest_port));
	    RXTRC(printf("          : returning handle at %p\n", 
			 st->tcplistens.hdl[i]));
	    return st->tcplistens.hdl[i];
	}
    /* prob should punt packet up a 'junk' io or sim */
    printf("tcp_filter failed: port_pair=%x, dest_port=%x, src_addr=%x\n",
	    port_pair, dest_port, src_addr);
    

    for(i=0; i<len; i++)
    {
	printf("%02x ", (unsigned char)(tcp_pkt[i]));
	if ((i & 15) == 15)
	    printf("\n");
    }
    printf("\n");

    return FAIL;
}







INLINE PF_Handle ip_filter(PF_st *st, char *ip_pkt, word_t len, bool_t *frag,
			   uint32_t *hdrsz)
{
    uint32_t	src_addr;
    uint16_t	flagsOffset;
    int		hdsz;

#if 0
    printf("ip_filter: ip packet id is %d\n", 
	    hton16(*((uint16_t *)(ip_pkt + 4))) );
#endif

    ASSERT_LEN(20);

    /* check this is IPv4 */
    if ((ip_pkt[0] & 0xf0) != 0x40)
    {
	printf("ip_filter failed: not IPv4\n");
	return FAIL;
    }

    /* calc IP header size */
    hdsz = ((ip_pkt[0] & 0xF)<<2);

    /* update our running count */
    *hdrsz += hdsz;

    src_addr = LDU_32(ip_pkt+12);

    /* is this a fragmented datagram? */
    flagsOffset = *(uint16_t*)(ip_pkt+6);
    if (flagsOffset & (MF|OFFMASK))
    {
	(*frag) = True; 
	if (flagsOffset & OFFMASK)
	    return DELAY;
    }


    /* Here have some sort of IP packet */
    switch(ip_pkt[9]) {
    case IP_PROTO_ICMP:
	*hdrsz += 8/*ICMP header*/ + 20/*opt. old IP*/ + 8/*opt. data*/;
	/* check that hrdsz is not off the end of the buffer */
	*hdrsz = MIN(*hdrsz, ETHER_HDR_SIZE + len);
        TRC(printf("icmp in pf with header size  %d\n", *hdrsz));
	return st->def_handle;
	break;

    case IP_PROTO_TCP:
	return tcp_filter(st, ip_pkt + hdsz, src_addr, len - hdsz, hdrsz);
	break;

    case IP_PROTO_UDP:
	return udp_filter(st, ip_pkt + hdsz, src_addr, len - hdsz, hdrsz);
	break;

    default:
	printf("ip_filter failed: (type is %x)\n", ip_pkt[9]);
	return FAIL;
	break;
    }
}

INLINE PF_Handle eth_filter(PF_st *st, struct ether_hdr *epkt, word_t len,
			    bool_t *frag, uint32_t *hdrsz)
{
    PF_Handle ret;

    ASSERT_LEN(14);

    switch(ntoh16(epkt->proto)) {
    case ETHER_TYPE_IP:
	*hdrsz = 14;  /* ethernet header for starters */
	ret = ip_filter(st, ((char *)epkt) + ETHER_HDR_SIZE,
			len - ETHER_HDR_SIZE, frag, hdrsz);
	RXTRC(printf("%x\n", ret));
	/* update by Thiemo: the default channel stuff */
	if (ret == FAIL) {
	  /* headersize is already correct for UDP and TCP */
	  RXTRC(printf("PF: something for default channel, "
		       "protocol type %d hdrsz %d\n",
		       *(((char *)epkt)+ETHER_HDR_SIZE+9), *hdrsz));
	  return st->def_handle;
	}
	else
	  return ret;
	break;
	
    case ETHER_TYPE_ARP:
	RXTRC(printf("%x\n", st->arp));
	*hdrsz = 64;  /* treat it as a single large header */
	return st->arp;
	break;
	
    case ETHER_TYPE_MSDL1:
    case ETHER_TYPE_MSDL2:
	RXTRC(printf("%x\n", st->msnl));
	*hdrsz = 64;  /* XXX no idea, really */
	return st->msnl;
	break;

    case ETHER_TYPE_XNS:
    case ETHER_TYPE_RARP:
    default:
	return FAIL;
	break;
    }
}


/* End of pf.c */
