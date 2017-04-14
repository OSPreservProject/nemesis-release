/*
*****************************************************************************
**                                                                          *
** NOTE:  This is substantiall similar to code written by Digital Equipment *
**        Corporation.                                                      *
**        Since this is the only hardware documentation available it is     *
**        largely unmodified (and shamelessly ripped off).                  *
**                                                                          *
*****************************************************************************
**
**             Copyright (C) 1994,1995 Digital Equipment Corporation
**                          All Rights Reserved
**
** This software is a research work and is provided "as is." Digital disclaims 
** all express and implied warranties with regard to such software, including 
** the warranty of fitness for a particular purpose.  In no event shall Digital
** be liable for any special, direct, indirect, or consequential damages or any
** other damages whatsoever, including any loss or any claim for loss of data 
** or profits, arising out of the use or inability to use the software even if 
** Digital has been advised of the possibility of such damages. 
**
** FACILITY:
**
**      otto.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Otto Driver for DEC 3000/400 series and up. 
**      See section 9.5.2 of DEC3000 H/W Spec Manual
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: otto.c 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
** LOG: $Log: otto.c,v $
** LOG: Revision 1.11  1998/06/23 11:31:57  smh22
** LOG: global param to NewAt
** LOG:
** LOG: Revision 1.10  1998/06/18 08:57:06  rn212
** LOG: alloca PCI I/O memory
** LOG:
** LOG: Revision 1.9  1998/04/16 14:28:40  smh22
** LOG: fix includes
** LOG:
** LOG: Revision 1.8  1997/10/06 13:09:05  dr10009
** LOG: less
** LOG: less tracing
** LOG:
** LOG: Revision 1.7  1997/07/07 11:32:05  pbm1001
** LOG: Don't crash if otto probe fails
** LOG:
** LOG: Revision 1.6  1997/06/27 14:50:32  and1000
** LOG: w13mode support
** LOG: move backpressure to when cells leave the card, rather than DMA complete
** LOG:
** LOG: Revision 1.5  1997/05/14 18:33:38  smh22
** LOG: Event->Events
** LOG:
** LOG: Revision 1.4  1997/04/11 11:22:43  rjb17
** LOG: dont crash machine if no oppo present
** LOG:
** LOG: Revision 1.3  1997/04/07 15:31:08  prb12
** LOG: use generic stubs
** LOG:
** LOG: Revision 1.2  1997/03/21 13:34:17  sde1000
** LOG: Minor fix for Intel (kernel critical sections are not nestable)
** LOG:
** LOG: Revision 1.1  1997/02/14 14:38:41  nas1007
** LOG: Initial revision
** LOG:
** LOG: Revision 1.3  1997/02/13 17:52:12  nas1007
** LOG: *** empty log message ***
** LOG:
** LOG: Revision 1.1  1997/02/12 13:04:36  nas1007
** LOG: Initial revision
** LOG:
 * Revision 1.2  1995/12/06  14:25:56  prb12
 * *** empty log message ***
 *
 * Revision 1.1  1995/10/12  11:08:46  prb12
 * Initial revision
 *
 * Revision 1.1  1995/03/01  11:56:40  prb12
 * Initial revision
 *
**
*/

#include <nemesis.h>
#include <platform.h>
#include <irq.h>
#include <ntsc.h>
#include <time.h>
#include <ecs.h>
#include <PCIBios.ih>
#include <VPMacros.h>

/*****************************************************************/

#include <stdio.h>
#include <Interrupt.ih>
#include <pci.h>
#include <irq.h>
#include <io.h>
#include <IDCMacros.h>
#include <IOMacros.h>


/* Common includes */

#include "state.h"
#include "otto_platform.h"
#include "otto_hw.h"
#include "rbuf.h"
#include <Interrupt.ih>


#ifdef BUS_PCI
pci_dev_t *pci_config;
#endif

/*****************************************************************/

#define WTRC(x)

#ifdef OTTO_TRACE
#define TRC(x)	x
#else /* OTTO_TRACE */
#define TRC(x)
#endif /* OTTO_TRACE */

#define DB(x) x


static const int ottodebug = 0;

#define OTTO_FS_VCI 16          /* VCI these packets go on */
#define OTTO_PKTROUNDUP 48      /* round up packets to a cell size */
/* This works for x < 32721 and gccarm -O2 turns it into very neat code
* viz:
*	add	r3, r0, #47
*	mov	r3, r3, lsr #4
*	add	r0, r3, r3, asl #2
*	add	r0, r3, r0, asl #1
*	rsb	r0, r0, r0, asl #5
*	add	r0, r3, r0, asl #1
*	mov	r0, r0, lsr #11
*
* XXX On Alpha Make sure you use OTTO_NCELLS(uint64_t len)
*
*	srl     t1, 0x4, t1
*	s4addq  t1, t1, t0
*	s4subq  t0, t1, t0
*	s8addq  t0, t0, t0
*	s4subq  t0, t1, t0
*	srl     t0, 0xb, t0
*	addl    t0, zero, t3
*
* COOL EH?
*/
#define OTTO_NCELLS(x)	((((x+47)/16)*683)/2048)

#define OTTO_RCVPKTS    (OTTO_MAXPKTSPERRING-1) /* no. rcv pkts outstanding */

/*****************************************************************/
/* Private */
/*****************************************************************/

static int  ottoprobe (Otto_st *st, addr_t reg);
static void ottoinit  (Otto_st *st);
static int  ottointr  (Otto_st *st);
static void initstate (Otto_st *st);
static int  pci_probe (pci_dev_t *pci_dev);

void initring (Otto_st *st, uint32_t q);
void queuercvbuffers (Otto_st *st, unsigned q, int n);

/*****************************************************************/
/*
 * Probe the OTTO to see if it's there 
 */
static int ottoprobe(Otto_st *st, addr_t reg)
{
    addr_t addr, paddr, opaddr;
    int ispotto = 0;
    int isoppo;
    int i;
    struct otto_xilinxdesc xilinxcode;
  
    /* allocate space for the report ring */
    addr = Heap$Malloc(Pvs(heap), OTTO_REP_RING_LEN_BYTES * 2);
    if (addr == 0) {
	printf ("Otto: probe failed: couldn't allocate memory "
		"(%x bytes) for report ring\n", OTTO_REP_RING_LEN_BYTES * 2);
	return (0);
    }
    memset(addr, 0xaaaaaaaa, OTTO_REP_RING_LEN_BYTES * 2);

    /* See if we have an Otto, Potto or Oppo in our slot */
    pci_config = (pci_dev_t*) reg;
    TRC(printf("oppo: pci_config->irq = %d,"
	       " st->irq = %d\n",pci_config->irq,st->irq));
    ispotto = 0;
    isoppo = 1;
    reg = (addr_t) pci_config->base_addr; /* XXX NAS */

    ENTER_KERNEL_CRITICAL_SECTION();
    {
	unsigned w;
	int rc,rc2,rc3,rc4;

/*	ENTER_BROKEN_INTEL_SECTION(); */
	rc = PCIBios$ReadConfigDWord(pci_config->pcibios, 0, 
				     pci_config->bridgedevno, 0x40, &w);
      
	if (rc) eprintf("Req Msk Timer write return code %d\n", rc);
	if (w & 0xc) {
	    eprintf ("oppo: req msk timer: was %x\n",w);
	    w &= ~0xc;
	    rc = PCIBios$WriteConfigDWord(pci_config->pcibios, 
					  0, 
					  pci_config->bridgedevno,
					  0x40, w);
	}
/*	LEAVE_BROKEN_INTEL_SECTION(); */

	if (rc) eprintf("Req Msk Timer write return code %d\n", rc);
  
    }
    LEAVE_KERNEL_CRITICAL_SECTION();

    bzero ((char *)st->otto_hw, sizeof (struct otto_hw));
	
    /* force natural alignment of physical address */
    opaddr = paddr = (addr_t) OTTO_SVTOPHY (addr);
    paddr += OTTO_REP_RING_LEN_BYTES;
    paddr  = (addr_t)
	(((unsigned long) paddr) & ~(OTTO_REP_RING_LEN_BYTES - 1));
    addr  += paddr - opaddr;
    if (isoppo) {
	extern uchar_t otto_pxilinxdata[];
	extern int otto_pxilinxdatalen;
	extern int otto_pxilinxchecksum;
	xilinxcode.data = otto_pxilinxdata;
	xilinxcode.stride = 1;
	xilinxcode.checksum = otto_pxilinxchecksum;
	xilinxcode.len = otto_pxilinxdatalen;
    } else {
	extern uchar_t otto_xilinxdata[];
	extern int otto_xilinxdatalen;
	extern int otto_xilinxchecksum;
      
	TRC(printf("oppo: PANIC!!!\n"));
	PAUSE(FOREVER);
	xilinxcode.data = otto_xilinxdata;
	xilinxcode.stride = 1;
	xilinxcode.checksum = otto_xilinxchecksum;
	xilinxcode.len = otto_xilinxdatalen;
    }

    TRC(printf("oppo: disabling irq %x\n", st->irq));
    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_mask_irq(st->irq);
    LEAVE_KERNEL_CRITICAL_SECTION();

    st->otto_hw->isoppo = isoppo;
    st->otto_hw->ispotto = ispotto;
   
    if (otto_initialize (reg, st->otto_hw, &xilinxcode,
			 OTTO_TXCELLS, OTTO_NOACKCELLS, addr) != 0) {
	printf ("oppo: init failed\n"); 
	return (0);
    }
  
    /* initialize the locks */
    OTTO_LOCKINIT (&st->otto_hw->statelock);
    OTTO_LOCKINIT (&st->otto_hw->schedlock);
    OTTO_LOCKINIT (&st->otto_hw->execlock);
  
    st->otto_hw->schedlen = OTTO_SCHEDSIZE / 2;
    st->otto_hw->schedfree = st->otto_hw->schedlen;
  
    st->otto_hw->vcs[0].flags = OTTO_VC_FIXED|OTTO_VC_NOFC;
    for (i = 1; i != OTTO_MAXVC; i++) {
	st->otto_hw->vcs[i].flags = OTTO_VC_IGNORE;
    }
    st->otto_hw->ispotto = ispotto;
  
    return (sizeof (Otto_st));
}

/*****************************************************************/
/* 
 * Initialize interface. 
 */
static void ottoinit(Otto_st *st)
{
    OTTO_LOCKTMP s;

    TRC(printf("oppo: Init HW\n"));

    /* Lock the state record */
    OTTO_LOCK (&st->otto_hw->statelock, s);

    /* reinitialize the device and the free lists for its resources */
    if (otto_softreset (st->otto_hw, OTTO_TXCELLS, OTTO_NOACKCELLS) != 0) {
	panic ("otto_softreset failed");
    }

    /* lock microsequencer into PIO-only mode */
    otto_lockpioonly (st->otto_hw);	

    /* Initialise the state */
    initstate (st);

    /* Allow otto to send and receive packets */
    otto_unlockpioonly (st->otto_hw);

    /* unlock state record, drop IPL */
    OTTO_UNLOCK (&st->otto_hw->statelock, s);

}


/*****************************************************************/
/* 
 * RX Buffer management
 */

static INLINE void replenishrcvbuffer (st, rxpkt)
    Otto_st    *st;
    struct otto_rxpkt *rxpkt;
{
    unsigned rxfragdescindex;
    volatile struct otto_descriptor *rxfragdesc;
    struct rbuf *r = rxpkt->r;
    IO_Rec *rec;
    int i;
    int isoppo = st->otto_hw->isoppo;

    rxfragdescindex = rxpkt->desc;
    rxfragdesc = &st->otto_hw->descriptors[rxfragdescindex];
  
    /* Set up the frags to cover the IORecs */
    rec = r->r_recs;
    
    ENTER_BROKEN_INTEL_SECTION();
    for (i = r->r_nrecs-1; i>=0; i--, rec++) {
	otto_uword32 physaddr = (otto_uword32) OTTO_SVTOPHY(rec->base);
	otto_uword32 physlen  = (otto_uword32) rec->len;
    
	TRC(printf("repl: %x rec[%x]=(%p, %x) %x %x\n", 
		   rxfragdescindex, i, 
		   rec->base, rec->len,
		   physaddr, physlen)); 

	if (i)
	{
	    OTTO_PIOW (&rxfragdesc->word0, &st->otto_hw, 
		       OTTO_MAKE_FRAG0 (physlen, rxfragdescindex));
	}
	else
	{
	    OTTO_PIOW (&rxfragdesc->word0, &st->otto_hw, 
		       OTTO_MAKE_FRAG0 (physlen,OTTO_FINALFRAG));
	}
    
	OTTO_PIOW (&rxfragdesc->word1, &st->otto_hw,
		   OTTO_MAKE_FRAG1 (physaddr));

	rxfragdesc++;
	rxfragdescindex++;
    }
    LEAVE_BROKEN_INTEL_SECTION();
    NTSC_MB;
}

/* Queue "n" receive buffers from channel "io" on ring "q". */
void queuercvbuffers (st, q, n)
    Otto_st *st;
    unsigned q;
    int n;
{
    struct otto_ring *ring = &st->otto_hw->rings[q];
    volatile struct otto_descriptor *rcvring =
	&st->otto_hw->descriptors[ring->desc+1];
    unsigned nextfree = ring->nextfree;
    unsigned lastused = ring->lastused;
    unsigned tag      = ring->tag;
    volatile struct otto_descriptor *rxfragdesc;
    unsigned rxfragdescindex;
    unsigned rxpktindex;
    word_t rxclient;
    struct rbuf *r;
    IO_clp io;
    IO_Rec *rec;
    int nnfree;
    int nexttag;
    int i;
    int isoppo = st->otto_hw->isoppo;

    /* Work out where to get the buffers from */
    rxclient = st->otto_hw->rings[q].rxclient;
    io  = st->rxclient[rxclient].io;

    /* catchup with missed buffers */
    n += ring->missed;		

    while (n > 0) {
	nnfree = nextfree+1;
	nexttag = tag;

	if (nnfree == OTTO_MAXPKTSPERRING) {
	    nnfree = 0;
	    nexttag = OTTO_TAG_FLIP (tag);
	}

	/* if the next entry would fill the ring, give up */
	if (nnfree == lastused) {
	    limitprintf (5, ("oppo: queuercvbuffers: ring full\n"));
	    printf("FULL\n");
	    break;
	}

	/* Allocate an rxpkt */
	rxpktindex = otto_allocrxpkt (st->otto_hw);
	if (rxpktindex == 0) {
	    limitprintf (30, ("queuercvbuffers: no free buf descs\n"));
	    OTTO_STATS_INC (st->otto_hw, no_free_desc);
	    break;
	}

	r = st->otto_hw->rxpkts[rxpktindex].r;
	if (!r) {
	    printf("No RBUF!\n");
	    ntsc_dbgstop();
	}

	/* Try to fill the RBuf for this packet from this VCI's IO channel */
	if (RGET(r, io, R_DONTWAIT)) {
	    r->r_type = RT_EMPTY;
	} else {
	    otto_freerxpkt(st->otto_hw, rxpktindex);
	    break;
	}
 
/*    printf("RXPKT:%d  nextfree:%d  RBuf:%p  nrecs:%d\n", 
      rxpktindex, nextfree, r, r->r_nrecs); */

	rxfragdescindex = st->otto_hw->rxpkts[rxpktindex].desc;
	tag = OTTO_MAKE_RXPKT (0, 0, tag, rxfragdescindex);
	rxfragdesc = &st->otto_hw->descriptors[rxfragdescindex++];

	/* Set up the frags to cover the IORecs */
	rec = r->r_recs;
	for (i = r->r_nrecs-1; i>=0; i--, rec++) {
	    otto_uword32 physaddr = (otto_uword32) OTTO_SVTOPHY(rec->base);
	    otto_uword32 physlen  = (otto_uword32) rec->len;

	    TRC(printf("qbuf: %x/%x/%x rec[%x]=(%p, %x)\n", 
			nextfree, rxpktindex, rxfragdescindex-1, 
			i, rec->base, rec->len)); 

	    ENTER_BROKEN_INTEL_SECTION();
	    if (i)
	    {
		OTTO_PIOW(&rxfragdesc->word0, &st->otto_hw, 
			  OTTO_MAKE_FRAG0 (physlen, rxfragdescindex));
	    }
	    else
	    {
		OTTO_PIOW(&rxfragdesc->word0, &st->otto_hw, 
			  OTTO_MAKE_FRAG0 (physlen,OTTO_FINALFRAG));
	    }

	    OTTO_PIOW(&rxfragdesc->word1, &st->otto_hw,
		      OTTO_MAKE_FRAG1 (physaddr));
	    LEAVE_BROKEN_INTEL_SECTION();
	    rxfragdesc++;
	    rxfragdescindex++;
	}

	/* Update the ring's next free pkt descriptor */
	wbflush ();
	ENTER_BROKEN_INTEL_SECTION();
	OTTO_WRITE_LOW (&rcvring[nextfree].word0, tag, 0xffff, st->ottohw);
	LEAVE_BROKEN_INTEL_SECTION();
	wbflush ();

	ring->pktindex[nextfree] = rxpktindex;

	tag = nexttag;
	nextfree = nnfree;
	n--;

	/* dead man's handle */
	st->otto_hw->otto_watchdog.queuedrcvbuf =
	    st->otto_hw->otto_watchdog.time;
    }

    /* If we couldn't get enough buffers, add the ring to the missed 
       list for refreshing at a later date. */
    if (n != 0 && ring->missed == 0) {
	ring->next = st->otto_hw->ringmissedlist;
	st->otto_hw->ringmissedlist = ring - st->otto_hw->rings;
    } else if (ring->missed != 0 && n==0 && st->otto_hw->ringmissedlist != 0) {
	struct otto_ring *x = &st->otto_hw->rings[st->otto_hw->ringmissedlist];
	if (ring == x) {
	    st->otto_hw->ringmissedlist = ring->next;
	} else {
	    while (x->next != 0) {
		struct otto_ring *nx = &st->otto_hw->rings[x->next];
		if (nx == ring) {
		    x->next = ring->next;
		    break;
		}
		x = nx;
	    }
	}
    }
    ring->missed   = n;
    ring->tag      = tag;
    ring->nextfree = nextfree;
}

/* initialize ring q. */
void initring (Otto_st *st, uint32_t q)
{
    struct otto_ring *ring = &st->otto_hw->rings[q];
    volatile struct otto_descriptor *ringdesc =
	&st->otto_hw->descriptors[ring->desc];
    int i;

    ring->tag = OTTO_TAG0;
    ring->nextfree = 0;
    ring->lastused = 0;
    ring->missed = 0;
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ringdesc[0].word0,
	       &st->data,
	       OTTO_MAKE_RING (OTTO_TAG0, ring->desc+1));
    for (i = 1; i != OTTO_MAXPKTSPERRING; i++) {
	OTTO_PIOW (&ringdesc[i].word0,
		   &st->data,
		   OTTO_MAKE_RXPKT (OTTO_DONTWRAP,
				    ring->desc+i+1, OTTO_TAG1, OTTO_FINALFRAG));
	ring->pktindex[i] = 0;
    }
    OTTO_PIOW (&ringdesc[OTTO_MAXPKTSPERRING].word0,
	       &st->data,
	       OTTO_MAKE_RXPKT (OTTO_DOWRAP,
				ring->desc+1, OTTO_TAG1, OTTO_FINALFRAG));
    LEAVE_BROKEN_INTEL_SECTION();
    ring->pktindex[0] = 0;
}


/* initialize state after a device soft reset */
/* should be called with softc lock held */
static void initstate (Otto_st *st)
{
    unsigned ring;
    int i;
  
    TRC(printf("TXFRAGS:%d RXFRAGS:%d PKTSPERRING:%d TXPKTS:%d RINGS:%d RXPKTS:%d\n",
	       OTTO_MAXFRAGSPERTXBUF,
	       OTTO_MAXFRAGSPERRXBUF,
	       OTTO_MAXPKTSPERRING,
	       OTTO_NTXPKTS,
	       OTTO_NRINGS,
	       OTTO_NRXPKTS));
	 
    ring = otto_allocring (st->otto_hw);	/* allocate default ring */

    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&st->otto_hw->ram[OTTO_DEFAULTRING], /* tell OTTO default ring */
	       &st->otto_hw,
	       OTTO_MAKE_DEFAULTRING (st->otto_hw->rings[ring].desc));
    LEAVE_BROKEN_INTEL_SECTION();
    initring (st, ring);
    st->otto_hw->defaultring = ring; 
  
    st->otto_hw->schedhalf = 0;
    otto_newsched (st->otto_hw);
    for (i = 1; i != OTTO_MAXVC; i++) {
	struct otto_vcdata *vcdata = &st->otto_hw->vcs[i];
    
	otto_setrxvc (st->otto_hw, i,
		      vcdata->flags & OTTO_VC_IGNORE, 1,
		      0, st->otto_hw->rings[ring].desc);
	vcdata->rxring = ring;
	otto_settxvc (st->otto_hw, i, vcdata->flags & OTTO_VC_SCHED,
		      vcdata->flags & OTTO_VC_FC, -1, -1);
    }
    st->otto_hw->otto_watchdog.notgettingcells = 2;
    otto_setled (st->otto_hw, st->otto_hw->linkinuse, 0);
}

/*****************************************************************/

static volatile uint64_t ints = 0;
static volatile uint64_t rx = 0;
static volatile uint64_t dma = 1;	/* Avoid div by zero */
static volatile uint64_t sent = 1;
static volatile uint64_t bogus = 0;
static volatile uint64_t dmatime = 0;
static volatile uint64_t sendtime = 0;

#undef PROFILE
#ifdef PROFILE

#define PVAR(var)				\
static uint64_t PROFILE_##var = 0;		\
static uint64_t PROFILE_##var##_n = 1

#define PROFILE_START \
uint32_t entered = rpcc()

#define PROFILE_END(var) \
    PROFILE_##var += (uint64_t)rpcc()-entered; PROFILE_##var##_n++

#define PROFILE_DUMP(var) 					\
    printf("PROFILE_" #var ":\t%0lx (%lx)\n", 			\
	   PROFILE_##var / PROFILE_##var##_n, PROFILE_##var##_n);	\
    PROFILE_##var = 0;						\
	   PROFILE_##var##_n = 1

    PVAR(ottointr);
PVAR(ottointr1);
PVAR(ottointr2);
PVAR(ottointr3);
PVAR(ottointr4);
PVAR(ottoqtxbuf);
PVAR(rx);
PVAR(sent);
PVAR(dmad);

#else  PROFILE

#define PROFILE_START uint32_t entered
#define PROFILE_END(x)
#define PROFILE_DUMP(x)

#endif PROFILE

static uint32_t rxstart = 0;

/*
 * Interrupt service routine 
 */
static int ottointr (Otto_st *st)
{
    volatile struct otto_rep *reportbase = st->otto_hw->reportbase;
    OTTO_LOCKTMP s;
    otto_uword32 reporttag;
    int nextreport;
    int link = 0;		/*XXX*/
    int retry = 0;
    int i;
    uint32_t ticks;

    int ddmad = 0;
    int dsent = 0;
    int reps  = 0;

    PROFILE_START;

    if ((ints & 0x7fff) == 0) { 

	TRC(printf("Otto: ints:%qx sent %qx bogus:%qx rx:%qx dma:%qx\n",
		   ints, sent, bogus, rx, dma));

	PROFILE_DUMP(ottointr);
	PROFILE_DUMP(ottointr1);
	PROFILE_DUMP(ottointr2);
	PROFILE_DUMP(ottointr3);
	PROFILE_DUMP(ottointr4);
	PROFILE_DUMP(ottoqtxbuf);
	PROFILE_DUMP(rx);
	PROFILE_DUMP(sent);
	PROFILE_DUMP(dmad);

    }

    PROFILE_END(ottointr4);

    OTTO_LOCK (&st->otto_hw->statelock, s);

    if (st->otto_hw->deferred) {
	st->otto_hw->deferred = 0;
	otto_unlockpioonly (st->otto_hw);
    }
  
    reporttag = st->otto_hw->reporttag;
    nextreport = st->otto_hw->nextreport;
  
    PROFILE_END(ottointr1);

    /* process each report */
    do {
	otto_uword32 seqreport;
	/* pick up the DMA report word (point 1) */
	otto_uword32 dmareport = reportbase[nextreport].dmareport; /*1*/
	unsigned bytes;
	unsigned vc;
 
	/* XXX NAS */
	NAS_DEBUG(eprintf("REPORT =%08x\n", dmareport));

	if ((dmareport & OTTO_REP_DMA_TAGS) != reporttag) {

	    /* turn off the interrupt condition */
	    wbflush ();
#if defined(__alpha)
	    /* On Alpha, we acknowlegde the interrupt with a read to avoid
	       write-buffer flush problems. */
	    ENTER_BROKEN_INTEL_SECTION();
	    { volatile int readback = OTTO_PIOR (&st->otto_hw->ram_intack[0],
						 &st->otto_hw); }
	    LEAVE_BROKEN_INTEL_SECTION(); 
#else
	    ENTER_BROKEN_INTEL_SECTION();
	    st->otto_hw->ram_intack[0] = 0; 
	    LEAVE_BROKEN_INTEL_SECTION();
#endif
	    wbflush ();

	    /* save time when we last handled a report.  This allows us to
	       fake transmit interrupts in ottooutput(). */
	    /* st->otto_hw->otto_watchdog.lastinterrupt = NOW(); */

	    /* Check the DMA report word again to avoid losing interrupt for
	       any packet delivered between points *1* and *2* above */
	    dmareport = reportbase[nextreport].dmareport;
	    if ((dmareport & OTTO_REP_DMA_TAGS) != reporttag) {
		break;
	    }
	}
    another:
	seqreport = reportbase[nextreport].seqreport;
	bytes = OTTO_REP_DMA_BYTECOUNT (dmareport);
	vc = OTTO_REP_SEQ_VCI (seqreport);
	ticks = reportbase[nextreport].ticks;

	reps++;

	if ((OTTO_REP_SEQ_ISXMT & seqreport) == 0) {      /* receive */
	    PROFILE_START;

	    struct otto_ring *ring =
		&st->otto_hw->rings[st->otto_hw->vcs[vc].rxring];
	    volatile struct otto_descriptor *rcvring = 
		&st->otto_hw->descriptors[ring->desc+1];
	    unsigned nextfree = ring->nextfree;
	    unsigned lastused = ring->lastused+1;
	    unsigned tag      = ring->tag;
	    struct otto_rxpkt *rxpkt;
	    unsigned rxpktindex;
	    unsigned rxfragdescindex;
	    unsigned pktdesc;
	    struct rbuf *r;

	    unsigned tmp;
      
	    /* XXX NAS */
	    NAS_DEBUG(printf("rcv interrupt \n"));

	    rx++;

	    pktdesc = OTTO_REP_SEQ_PKT (seqreport);
	    if (pktdesc == OTTO_EMPTYPKT) {
		printf("ottointr: rcv %d bytes into empty buffer on vc %d\n",
		       bytes, vc);
		goto next_report;
	    }

	    tmp = (pktdesc - OTTO_RINGSSTART) / (OTTO_MAXPKTSPERRING+1);
      
	    if (0) printf("otto: rcv into %x - entry %d of ring %d"
			  " vc=%d savedring=%d pkt=%x lastused=%d\n",
			  pktdesc,
			  pktdesc - ring->desc - 1, tmp, vc,
			  st->otto_hw->vcs[vc].rxring,
			  OTTO_REP_SEQ_PKT (seqreport),
			  ring->lastused);
      
	    if (&st->otto_hw->rings[tmp] != ring) {
		limitprintf (5,("ottointr: ring number isn't right %u %ld\n",
				tmp, (long) (st->otto_hw->rings-ring)));
		ring = &st->otto_hw->rings[tmp];
		rcvring = &st->otto_hw->descriptors[ring->desc+1];
		nextfree = ring->nextfree;
		lastused = ring->lastused+1;
		tag = ring->tag;
	    }

	    tmp = pktdesc - ring->desc - 1;

	    rxpktindex = ring->pktindex[tmp];
	    rxpkt = &st->otto_hw->rxpkts[rxpktindex];

	    r = rxpkt->r; 

	    rxfragdescindex = rxpkt->desc;
	    tag = OTTO_MAKE_RXPKT (0, 0, tag, rxfragdescindex);
      
	    if (0) printf("vc:%x:%x  ring:%x:%x pkt:%x:%x frag:%x:%x %x\n",
			  OTTO_PIOA(&st->otto_hw->ram[OTTO_VCPOS+OTTO_MAXVC+vc], 
				    &st->otto_hw),
			  OTTO_PIOR((otto_uword32 *)&st->otto_hw->ram[OTTO_VCPOS+OTTO_MAXVC+vc],
				    &st->otto_hw),
		    
			  OTTO_PIOA(&st->otto_hw->descriptors[ring->desc],
				    &st->otto_hw),
			  OTTO_PIOR((otto_uword32 *)&st->otto_hw->descriptors[ring->desc],&st->otto_hw),
			  OTTO_PIOA(&rcvring[ring->lastused],
				    &st->otto_hw),
			  OTTO_PIOR((otto_uword32 *)&rcvring[ring->lastused],
				    &st->otto_hw),
			  OTTO_PIOA(&st->otto_hw->descriptors[rxfragdescindex],
				    &st->otto_hw),
			  OTTO_PIOR((otto_uword32 *)&st->otto_hw->descriptors[rxfragdescindex],
				    &st->otto_hw),
			  OTTO_PIOR(((otto_uword32 *)&st->otto_hw->descriptors[rxfragdescindex])+1,
				    &st->otto_hw)
		);
    
	    if (r->r_type != RT_EMPTY) {
		int kk;
		printf ("pkt_in_ring=%d ring: tag=%x desc=%x next=%d nextfree=%d lastused=%d missed=%d\n",
			tmp, ring->tag, ring->desc,
			ring->next, ring->nextfree,
			ring->lastused, ring->missed);
		for (kk = 0; kk != OTTO_MAXPKTSPERRING; kk++) {
		    printf ("%x ", ring->pktindex[kk]);
		}
		printf ("\n");
		printf ("ottointr: receive RBUF not of type RT_EMPTY");
	    }

	    if (lastused == OTTO_MAXPKTSPERRING) {
		lastused = 0;
	    }
	    ring->lastused = lastused;

	    /* packet must be at least a cell, less than 
	       64Kbytes, 0mod4 bytes long, and no error bits
	       should be set */
	    if (bytes >= OTTO_PKTROUNDUP &&
		(bytes & 3) == 0 &&
		bytes < 0x10000 &&
		(dmareport & (OTTO_REP_DMA_CRCERROR|
			      OTTO_REP_DMA_DMAERR)) == 0) {

		st->otto_hw->otto_watchdog.gotrcv = st->otto_hw->otto_watchdog.time;

		{
		    static uint64_t bits = 0;
		    static uint64_t dropped = 0;
		    struct rbuf rtmp;
		    bool_t ok;

		    if (0) printf("RX: pkt %x rbuf %x\n", rxpktindex, r);

#if 0
		    /* Try to allocate new buffer space */
		    if (!replenishrcvbuffer (st, rxpkt)) {
			/* XXX  - shouldn't have to do this! 
			   if we can't get enough buffers
			   defer interrupts for a while */
			if (ring->missed == 0) {
			    ring->next =
				st->otto_hw->ringmissedlist;
			    st->otto_hw->ringmissedlist = 
				ring - st->otto_hw->rings;
			}
			ring->missed++;
			printf("*********************** defer!!\n");
			st->otto_hw->deferred = 1;
			otto_defer (st->otto_hw);
			goto next_report;
		    }
#endif /* 0 */
	  
		    /* Attempt to get a new RBuf for this packet*/
		    ok = RGET(&rtmp, r->r_io, R_DONTWAIT);  
	  
		    /* If we managed to get a new RBuf then send the old one
		       to the client */
		    if (ok) { 
			if (rtmp.r_nrecs == 0) 
			    printf("Otto: Got zero recs\n");

			/* Adjust len  */
			RADJ(r, bytes);
			bits += bytes<<3;

			/* Passup the old buffer to the client */
			RPUT(r);

			/* XXX Copy the new buffer information */
			RCOPY(&rtmp, r);

			/* Set up the RX packet fragment descriptors */
			replenishrcvbuffer (st, rxpkt);

			st->otto_hw->otto_watchdog.queuedrcvbuf =
			    st->otto_hw->otto_watchdog.time;

		    } else {
			dropped += bytes<<3;
		    }
		    if (bits > 1<<24) {
			uint32_t now = rpcc();
			uint32_t time = (now-rxstart) + 1;
#ifdef DEBUG
			printf ("OPPO receive %ld bps\n", bits * 275000000 / time);
			printf ("OPPO dropped %ld bps\n", dropped * 275000000 / time);
#endif
			rxstart = now;
			bits = 0;
			dropped = 0;
		    }
		}

	    } else {
		NAS_DEBUG(printf("ERROR OCCURED\n"));
		/* packet had an error */
		OTTO_STATS_INC_L (st->otto_hw, rcv_error, link);
		if (dmareport & OTTO_REP_DMA_DMAERR) {
		    /* this error will probably never be seen.  If there was a
		       DMA error, how did the DMA engine DMA the report tell us?
		       We shutdown the device, and get the watchdog to
		       restart. */
		    otto_setreset (st->otto_hw);
		    st->otto_hw->otto_watchdog.forcereset = 1;
		    limitprintf (5, ("ottointr: rcv dma error on %d\n", link));
		    OTTO_STATS_INC_L (st->otto_hw, rcv_dma_error, link);
		    OTTO_UNLOCK (&st->otto_hw->statelock, s);
		    return retry;
		}
		if (dmareport & OTTO_REP_DMA_CRCERROR) {
		    OTTO_STATS_INC_L (st->otto_hw, rcv_crc_error, link);
		    ottodprintf (2, ("crc error\n"));
		}
		if ((bytes % 4) != 0) {
		    printf ("ottointr: byte not 0mod4\n");
		    OTTO_STATS_INC_L (st->otto_hw, rcv_len_err, link);
		}
		if (bytes > 0x10000) {
		    printf ("ottointr: rcv pkt too long\n");
		    OTTO_STATS_INC_L (st->otto_hw, 
				      rcv_pkt_too_large, link);
		}
		if (bytes < OTTO_PKTROUNDUP) {
		    printf ("ottointr: rcv pkt too short bytes=%d rep=%x\n",
			    bytes, dmareport);
		    OTTO_STATS_INC_L (st->otto_hw,
				      rcv_pkt_too_short, link);
		}
	    } /* Error */

	    /* Mark packet as free for RX */
	    wbflush ();
	    ENTER_BROKEN_INTEL_SECTION();
	    OTTO_WRITE_LOW (&rcvring[nextfree].word0, tag, 0xffff, st->otto_hw);
	    LEAVE_BROKEN_INTEL_SECTION();
	    wbflush ();

	    ring->pktindex[nextfree] = rxpktindex;
	    nextfree++;
	    if (nextfree == OTTO_MAXPKTSPERRING) {
		nextfree = 0;
		ring->tag = OTTO_TAG_FLIP (ring->tag);
	    }
	    ring->nextfree = nextfree;

	    /* try to get back any rbufs we were not able to allocate before */
	    if (ring->missed != 0) {
		queuercvbuffers (st, (unsigned) (ring - st->otto_hw->rings), 0);
	    }

	    PROFILE_END(rx);

	} else {      /* transmit */

	    PROFILE_START;

	    unsigned *txvcpkt = &st->otto_hw->vcs[vc].txpkts;
	    unsigned lastpkt = *txvcpkt;
	    struct otto_txpkt *lasttxpkt= &st->otto_hw->txpkts[lastpkt];
	    unsigned firstpkt = lasttxpkt->next;
	    struct otto_txpkt *firsttxpkt = &st->otto_hw->txpkts[firstpkt];
	    word_t txclient;

	    NAS_DEBUG(printf("trans interrupt \n"));

	    if (lastpkt == 0) {
		printf ("ottointr: no TX pkt");
	    }
	    if (0) {
		struct otto_txpkt *curtxpkt = firsttxpkt;
		printf("first %lx last %lx\n", firsttxpkt, lasttxpkt);
		printf("TXVCPKT");
		do {
		    printf(" -> %lx", curtxpkt);
		    if (curtxpkt->r->r_type == RT_FREE) 
			printf("(E)");
		    else
			printf("(F)");
		    curtxpkt = &st->otto_hw->txpkts[curtxpkt->next];
		} while (curtxpkt != firsttxpkt);
		printf("\n");
	    }

	    if (seqreport & OTTO_REP_SEQ_SENT) {
		PROFILE_START;

		if (0) printf("SENT %x\n", firsttxpkt);

		/* Check for the rate control hack VCI! */
		if (!vc) {
		    /* repeatedly transmit the same packet */
		    ENTER_BROKEN_INTEL_SECTION();
		    OTTO_PIOW(st->otto_hw->txrequest, &st->otto_hw,
			      OTTO_MAKE_TXREQ (firsttxpkt->desc, 256 * 48));
		    LEAVE_BROKEN_INTEL_SECTION();
		    NTSC_MB;
		    goto next_report;
		}

		/* packet has gone down the wire */
		if (0) printf("otto: pkt sent\n");

		sent++;	dsent++;

		if (!st->w13mode && firsttxpkt->r->r_type != RT_FREE) {
		    printf ("otto: tx didn't free rbuf");
		}

		st->otto_hw->vcs[vc].flags |= OTTO_VC_XMTDONE;
		st->otto_hw->otto_watchdog.gotxmt = st->otto_hw->otto_watchdog.time;

		/* remove first packet from list of outstanding
		   transmit packets on this VC */
		if (firstpkt == lastpkt) {
		    /* there was only one pkt outstanding */
		    *txvcpkt = 0;
		} else {
		    lasttxpkt->next = firsttxpkt->next;
		}

		/* Packet has been TXed so TX Packet descriptor is now free */
		txclient = st->otto_hw->vcs[vc].txclient;
		OTTO_UNLOCK (&st->otto_hw->statelock, s);/* XXX */
		EC_ADVANCE(st->txclient[txclient].ec, 1); 
		OTTO_LOCK (&st->otto_hw->statelock, s);/* XXX */

		{
		/* packet is now in the adapter */
		struct otto_txpkt *curtxpkt = firsttxpkt;

		/* find first packet not yet reported DMAed
		   that is on VC's transmit queue (i.e. first non-empty RBUF) */
		while (curtxpkt->r->r_type != RT_FULL) {
		    if (curtxpkt == lasttxpkt) {
			ottodprintf (0, ("seqreport=%x dmareport=%x vc=%d\n",
					 seqreport, dmareport, vc));
			/* there must be such a packet*/
			printf ("otto: TX queue broken");
		    }
		    curtxpkt = &st->otto_hw->txpkts[curtxpkt->next];
		}

		if (0) printf("DMAD %x\n", curtxpkt);
		dma++; ddmad++;

		/* retry = 1; */

		/* DMA Complete - send ACK */
		{ 
		    struct rbuf *r = curtxpkt->r;

		    OTTO_UNLOCK (&st->otto_hw->statelock, s);/* XXX */
		    RPUT(r);
		    OTTO_LOCK (&st->otto_hw->statelock, s);/* XXX */
		    r->r_type = RT_FREE;	/* Mark the RBUF as FREE */
		}
		}


		PROFILE_END(sent);
	    } else {
		PROFILE_START;

		/* Check for the rate control hack VCI! */
		if (!vc) goto next_report;

		ottodprintf (3, ("otto: pkt dmaed\n"));
	
		if (dmareport & OTTO_REP_DMA_DMAERR) {
		    /* a DMA error here means serious
		       trouble.  Shutdown the device and
		       get the watchdog to restart. */
		    otto_setreset (st->otto_hw);
		    st->otto_hw->otto_watchdog.forcereset = 1;
		    limitprintf (5, ("ottointr: xmt dma error on %d\n", link));
		    OTTO_STATS_INC_L (&st->otto_hw, xmt_dma_error, link);
		    OTTO_UNLOCK (&st->otto_hw->statelock, s);
		    return retry;
		}

		PROFILE_END(dmad);
	    }
	    PROFILE_END(ottointr3);
	}

    next_report:
	/* step on to the next report */
	nextreport++;
	if (nextreport == OTTO_REP_RING_LEN_REPS) {
	    nextreport = 0;
	    reporttag = OTTO_REP_TAGFLIP (reporttag);
	}
    } while (!st->otto_hw->deferred);

finished:  
    st->otto_hw->nextreport = nextreport;
    st->otto_hw->reporttag = reporttag;
  
    if (!reps) bogus++;

    PROFILE_END(ottointr2);

    st->otto_hw->otto_watchdog.gotintr = st->otto_hw->otto_watchdog.time;
    OTTO_UNLOCK (&st->otto_hw->statelock, s);

    PROFILE_END(ottointr);

    return retry;
}

/*****************************************************************/
/* 
 * Enqueue an rxpkt for receive
 */
word_t otto_enqueue_rxpkt(Otto_st *st, struct otto_rxpkt *rxpkt)
{
    volatile struct otto_descriptor *rxfragdesc;
    word_t rxfragdescindex;
    struct rbuf *r = rxpkt->r;
    IO_Rec *rec;
    word_t i;
    int isoppo = st->otto_hw->isoppo;

    rxfragdescindex = rxpkt->desc;
    rxfragdesc = &st->otto_hw->descriptors[rxfragdescindex];
  
    /* Set up the frags to cover the IORecs */
    rec = r->r_recs;
    for (i = 0; i < r->r_nrecs; i++, rec++) {
	otto_uword32 physaddr = (otto_uword32) OTTO_SVTOPHY(rec->base);
	otto_uword32 physlen  = (otto_uword32) rec->len;
    
	TRC(printf("QRX: %x rec[%x]=(%p, %x) %x %x\n", 
		   rxfragdescindex, i, 
		   rec->base, rec->len,
		   physaddr, physlen));
	ENTER_BROKEN_INTEL_SECTION();
	if(i != r->r_nrecs-1)
	{   
	    OTTO_PIOW(&rxfragdesc->word0, &st->otto_hw,
		      OTTO_MAKE_FRAG0 (physlen, rxfragdescindex)); 
	}
	else
	{
	    OTTO_PIOW(&rxfragdesc->word0, &st->otto_hw,
		      OTTO_MAKE_FRAG0 (physlen, OTTO_FINALFRAG));
	}

	OTTO_PIOW(&rxfragdesc->word1, &st->otto_hw,
		  OTTO_MAKE_FRAG1 (physaddr));
	LEAVE_BROKEN_INTEL_SECTION();
	rxfragdesc++;
	rxfragdescindex++;
    }
    NTSC_MB;

    /* dead man's handle */
    st->otto_hw->otto_watchdog.queuedrcvbuf = st->otto_hw->otto_watchdog.time;
}

/*
 * Prepare a TX packet for use on a specified VCI 
 */
void otto_prepare_txpkt(Otto_st *st, struct otto_txpkt *txpkt, word_t vc)
{
    word_t pktdescindex;
    volatile struct otto_descriptor *desc;  /* transmit descriptor followed
					       by OTTO_MAXFRAGSPERTXBUF fragment
					       descriptors */

    pktdescindex = txpkt->desc;
    desc = &st->otto_hw->descriptors[pktdescindex]; /* pkt desc */
  
    ENTER_BROKEN_INTEL_SECTION();
    /* Set up packet descriptor (1st FRAG desc follows PKT desc) */

    if (!st->w13mode)
	OTTO_PIOW(&desc->word0, &st->otto_hw,
		  OTTO_MAKE_TXPKT0 (OTTO_TXPKT_DOCRC, vc, pktdescindex+1));
    else
	OTTO_PIOW(&desc->word0, &st->otto_hw,
		  OTTO_MAKE_TXPKT0 (OTTO_TXPKT_W13, vc, pktdescindex+1));

    OTTO_PIOW(&desc->word1, &st->otto_hw,
	      OTTO_MAKE_TXPKT1 (OTTO_TXACTION_SEND));
    LEAVE_BROKEN_INTEL_SECTION();
    NTSC_MB;
}

/* 
 * Enqueue a packet for transmission
 */
word_t otto_enqueue_txpkt(Otto_st *st, struct otto_txpkt *txpkt, word_t vc)
{
    struct rbuf *r = txpkt->r;
    volatile struct otto_descriptor *desc; 
    word_t pktdescindex;
    word_t fragdescindex;

    word_t totallen;
    IO_Rec *rec;
    word_t i;
    int isoppo = st->otto_hw->isoppo;

    /* The fragment descriptors immediately follow the packet
       descriptor.   The variable descindex is the index of the 
       *next* fragment descriptor (its the bit pattern to be put into
       the current fragment to make it point to the next one).
       The variable desc points to the *current* fragment
       descriptor on each iteration of the loop below. */
  
    pktdescindex  = txpkt->desc;
    fragdescindex = pktdescindex+1;	/* index of 1st FRAG desc */
    desc = &st->otto_hw->descriptors[fragdescindex]; /* FRAG desc */
    fragdescindex++;		/* Index of next FRAG desc */

    totallen = 0;
  
    /* Set up the frags to cover the IORecs */
    rec = r->r_recs;
    for (i = 0; i < r->r_nrecs; i++) {
	otto_uword32 physaddr = (otto_uword32) OTTO_SVTOPHY(rec->base);
	otto_uword32 physlen  = rec->len;

	TRC(printf("QTX: %x rec[%x]=(%p, %x) %x %x\n", 
		   fragdescindex, i, 
		   rec->base, rec->len,
		   physaddr, physlen)); 

	ENTER_BROKEN_INTEL_SECTION();
	if (i != r->r_nrecs-1)
	{
	    OTTO_PIOW(&desc->word0, &st->otto_hw,
		      OTTO_MAKE_FRAG0 (physlen, fragdescindex));
	}
	else
	{
	    OTTO_PIOW(&desc->word0, &st->otto_hw,
		      OTTO_MAKE_FRAG0 (physlen, OTTO_FINALFRAG));
	}
    
	OTTO_PIOW(&desc->word1, &st->otto_hw, OTTO_MAKE_FRAG1 (physaddr)); 
	LEAVE_BROKEN_INTEL_SECTION();

	totallen += physlen;
	rec++; desc++; fragdescindex++;
    }
  
    /* Link packet to end of chain of those to be transmitted on this
       VCI.  This allows us to find them given the VCI in
       the transmit report. */
    OTTO_LOCK (&st->otto_hw->statelock, s);
    {
	unsigned pktindex = txpkt - st->otto_hw->txpkts;
	unsigned *txvcpkt = &st->otto_hw->vcs[vc].txpkts;
	unsigned  lastpkt = *txvcpkt;
	if (lastpkt == 0) {
	    txpkt->next = pktindex;	/* Only one pkt in queue */
	} else {
	    struct otto_txpkt *lasttxpkt = &st->otto_hw->txpkts[lastpkt];
	    txpkt->next = lasttxpkt->next;
	    lasttxpkt->next = pktindex; /* Add after lasttxpkt */
	}
	*txvcpkt = pktindex;	/* Update ptr to last pkt */
    }
    OTTO_UNLOCK (&st->otto_hw->statelock, s);

    if (0) { 
	printf("TX %lx\n", txpkt); 
    }

    /* Need only memory barriers here */
    NTSC_MB;
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW(st->otto_hw->txrequest , &st->otto_hw,
	      OTTO_MAKE_TXREQ (pktdescindex, totallen - OTTO_CRCLEN));
    LEAVE_BROKEN_INTEL_SECTION();
    NTSC_MB;
  
    /* dead-man's handle */
    st->otto_hw->otto_watchdog.queuedxmtbuf = st->otto_hw->otto_watchdog.time;

    return 1;
}

/*****************************************************************/
/* 
 * WatchThread
 */
static void WatchThread_m (Otto_st *st)
{
    int reset = 0;
    int fakeinterrupt = 0;
    unsigned t;
    OTTO_LOCKTMP s;

    while (1) {
	unsigned rcvpkt = 1;
	
	WTRC(printf("OttoWatch: "));

	t = st->otto_hw->otto_watchdog.time;
	st->otto_hw->otto_watchdog.time = t+1;
	
	if (t != st->otto_hw->otto_watchdog.gotintr) {
	    WTRC(printf(" nointr"));
	    fakeinterrupt = 1;
	}
	if (t - st->otto_hw->otto_watchdog.queuedrcvbuf > 1) {
	    WTRC(printf(" norxq"));
	    fakeinterrupt = 1;
	}
	if (t - st->otto_hw->otto_watchdog.queuedxmtbuf > 1) {
	    WTRC(printf(" notxq"));
	    fakeinterrupt = 1;
	}
	if (t - st->otto_hw->otto_watchdog.gotrcv > 1) {
	    WTRC(printf(" norx"));
	    fakeinterrupt = 1;
	    rcvpkt = 0;
	}
	if (t - st->otto_hw->otto_watchdog.gotxmt > 5) {
	    WTRC(printf(" notx"));
	}
	if (st->otto_hw->otto_watchdog.forcereset) {
	    WTRC(printf(" forcereset"));
	    reset = 1;
	}
	OTTO_LOCK (&st->otto_hw->statelock, s);

/* Do everything that we can to avoid reads on Pentium */

#ifndef INTEL
	if (OTTO_PIOR (&st->otto_hw->ram[OTTO_CRASHFLAG],st->otto_hw)
	    == OTTO_CRASHVALUE) {
	    unsigned i;
	    reset = 1;
	    printf ("otto crash:\n");
	    for (i = 16; i < 60; i++) {
		unsigned reg0 = st->otto_hw->ram[0x7000+i] & 0xFFFF;
		if (reg0 != 0) {
		    printf("C %2d: %4x %6d\n",
			   i, reg0, reg0 & 0x1FFF);
		}
	    }
	    printf("Transmit DMA Queue:\n");
	    for (i = OTTO_DMAFFPOS;
		 i < OTTO_DMAFFPOS+OTTO_DMAFFSIZE; i++) {
		otto_uword32 temp = st->otto_hw->ram[i];
		if (temp != 0) {
		    printf("%4x/ %8x\n", i, temp);
		}
	    }
	}
	

	if (st->otto_hw->otto_watchdog.resetsuni != 0 ||
	    !otto_gettingcells (st->otto_hw)) {
	    
	    WTRC(if (st->otto_hw->otto_watchdog.resetsuni) 
		 printf(" reset");
		 else printf(" !cells"));
	    
	    if (!st->otto_hw->otto_watchdog.notgettingcells) {
		otto_setled (st->otto_hw, st->otto_hw->linkinuse, 0);
		st->otto_hw->otto_watchdog.notgettingcells = 1;
	    }
	    /* turn on SUNI after a reset */
	    if (st->otto_hw->otto_watchdog.resetsuni != 0) {
		st->otto_hw->otto_watchdog.resetsuni--;
		if (st->otto_hw->otto_watchdog.resetsuni == 0) {
		    WTRC(printf(" initsuni"));
		    otto_initsuni (st->otto_hw);
		}
	    }
	} else {
	    if (st->otto_hw->otto_watchdog.notgettingcells) {
		st->otto_hw->otto_watchdog.notgettingcells = 0;
	    }
	    if (rcvpkt) {
		otto_setled (st->otto_hw, st->otto_hw->linkinuse, 1);
	    } else {
		otto_setled (st->otto_hw, st->otto_hw->linkinuse, 2);
	    }
	}
#endif

	{
	    unsigned lasttime = 0;
	    unsigned nexttime;
	    while ((nexttime = st->otto_hw->ringmissedlist) != 0 &&
		   nexttime != lasttime) {
		WTRC(printf(" queuercvbuffers"));
		queuercvbuffers (st, nexttime, 0); 
		lasttime = nexttime;
	    }
	}
	
	OTTO_UNLOCK (&st->otto_hw->statelock, s);
	
	if (reset) {
	    WTRC(printf(" reset"));
	    if (t - st->otto_hw->otto_watchdog.reset > 25) {
		WTRC(printf(" restart"));
		/* XXX  ottorestart (unit); */
		st->otto_hw->otto_watchdog.reset = t;
		st->otto_hw->otto_watchdog.forcereset = 0;
	    }
	} else if (fakeinterrupt) {
	    WTRC(printf(" fake"));
	    /* EC_ADVANCE(st->event, 1);       ottointr (st); */
	}
	
	WTRC(printf("\n"));
	PAUSE(SECONDS(1));
    }
}


/*****************************************************************/
/* 
 * InterruptThread
 */
static void InterruptThread_m (Otto_st *st)
{
    TRC(printf("oppo: InterruptThread started...\n"));
    
    while (1)  {
        Event_Val val;
        
        /* Wait for next interrupt event */
	ENTER_KERNEL_CRITICAL_SECTION();
        ntsc_unmask_irq(st->irq);
	LEAVE_KERNEL_CRITICAL_SECTION();
        
        val = EC_AWAIT_UNTIL(st->event, st->ack, NOW()+SECONDS(2)); 
	if (val < st->ack) {
	    TRC(printf("Timeout await %d\n", st->ack));
	} else {
	    if (val > st->ack) 
		TRC(printf("Took %d interrupts %lx %lx\n",
			   (val - st->ack)+1, val, st->ack));
	}
	
        st->ack = ++val;

        /* Service the interrupt */
        ottointr(st);

        ints++;
    }
}

/*****************************************************************/

/* allocate an rxclient structure */
word_t otto_allocrxclient (Otto_st *st)
{
    word_t x = st->rxclientfreelist;
    if (x != 0) {
	st->rxclientfreelist = st->rxclient[x].next;
    }
    return (x);
}

/* free an rxclient */
void otto_freerxclient (Otto_st *st, word_t index)
{
    st->rxclient[index].next = st->rxclientfreelist;
    st->rxclientfreelist = index;
}

/* allocate a txclient structure */
word_t otto_alloctxclient (Otto_st *st)
{
    word_t x = st->txclientfreelist;
    if (x != 0) {
	st->txclientfreelist = st->txclient[x].next;
	if (st->txclient[x].ec == NULL) {
	    st->txclient[x].ec = EC_NEW();
	    TRC(printf("Got EC %lx\n", st->txclient[x].ec));
	}
    }
    return (x);
}

/* free a txclient */
void otto_freetxclient (Otto_st *st, word_t index)
{
    st->txclient[index].next = st->txclientfreelist;
    st->txclientfreelist = index;
}

/*****************************************************************/

/*
 * Main thread
 */
Otto_st *otto_driver_init ()
{
    Domain_ID    dom;
    Channel_RX   rx;
    Interrupt_clp       ipt;
    Interrupt_AllocCode rc;

    Otto_st *st;
    pci_dev_t *pci_config;
    uint32_t i;

    /* 
     * Create local state 
     */
    TRC(printf("oppo: Allocating driver state.\n"));
    st = Heap$Malloc(Pvs(heap), sizeof(Otto_st) + sizeof(struct otto_hw));
    if (!st) {
	DB(printf("oppo: malloc(%x) failed\n", 
		  sizeof(*st) + sizeof(struct otto_hw)));
	return NULL;
    }

    pci_config = Heap$Malloc(Pvs(heap), sizeof(pci_dev_t));
    if (!pci_config) {
	DB(printf("oppo: malloc(%x) failed\n", 
		  sizeof(*pci_config)));
	return NULL;
    }
    
    TRC(printf("oppo: Initialising driver state\n"));
    
    /* Hardware regs */
    st->otto_hw = (struct otto_hw *)(st + 1);

    /* Probe for Otto device */
    TRC(printf("Call pci_probe..."));
    if (pci_probe(pci_config) < 0)
	return NULL;

    st->irq = pci_config->irq;
    TRC(printf("...returned %d\n",st->irq));
    TRC(printf("Call ottoprobe..."));
    if (ottoprobe(st, pci_config))
    {
	TRC(printf("...returned\n"));
	/* Install the interrupt stubs */
	TRC(printf("oppo: Installing interrupt stub... "));

	TRC(printf("oppo: Grabbing interrupt allocator\n"));
	ipt = IDC_OPEN("sys>InterruptOffer", Interrupt_clp);
      
	TRC(printf("Allocating interrupt... "));
	rc = Interrupt$Allocate(ipt, st->irq, _generic_stub, &st->stub);
	TRC(printf(" ... allocate returns %x\n", rc));
      
	TRC(printf("done\n"));
    } else {
	TRC(printf("oppo: probe failed\n", i));
	return NULL;
    }

    /* Interrupt event channels */
    dom = RO(Pvs(vp));
    st->stub.dcb   = dom;
    st->stub.ep    = Events$AllocChannel (Pvs(evs));
    TRC(printf("oppo: Interrupt channel %lx, %lx\n", st->stub.dcb, st->stub.ep));
    st->event = EC_NEW();
    st->ack   = 1;
    Events$Attach(Pvs(evs), st->event, st->stub.ep, Channel_EPType_RX);

    /* Init the per-client state */
    st->rxclientfreelist = 1;
    for (i = 1; i < OTTO_NRXCLIENTS; i++) {
	st->rxclient[i].next = i+1;
	st->rxclient[i].io   = NULL;
	st->rxclient[i].ring = 0;
    }
    st->rxclient[OTTO_NRXCLIENTS-1].next = 0;

    st->txclientfreelist = 1;
    for (i = 1; i < OTTO_NTXCLIENTS; i++) {
	st->txclient[i].next = i+1;
	st->txclient[i].io = NULL;
	st->txclient[i].ec = NULL;
	st->txclient[i].ev = 0;
    }
    st->txclient[OTTO_NTXCLIENTS-1].next = 0;

    /* Fork the watch thread */
    TRC(printf("oppo: Forking WatchThread... "));
    Threads$Fork(Pvs(thds), WatchThread_m, st, 0 ); 
    TRC(printf("Done\n"));

#if 0 /* XXX NAS move to later */
    /* Fork the interrupt thread */
    TRC(printf("oppo: Forking InterruptThread... "));
    Threads$Fork(Pvs(thds), InterruptThread_m, st, 0 ); 
    TRC(printf("Done\n"));
#endif /* 0 */

    /* no-one using the device at present */
    st->w13mode = False;
    st->normalmode = False;

    /* 
     * Init the device 
     */
    TRC(printf("oppo: Starting otto...\n"));
    ottoinit(st);
    TRC(printf("oppo: Done Init\n"));

    /* Fork the interrupt thread */
    TRC(printf("oppo: Forking InterruptThread... "));
    Threads$Fork(Pvs(thds), InterruptThread_m, st, 0 ); 
    TRC(printf("Done\n"));

    return st;
}

/*
 * Below ripped from de4x5 driver
 */

#define PCI_DEVICE    (dev_num << 3)
#define PCI_BRIDGE    (bridge_dev_num <<3)
#define PCI_LAST_DEV  32
/*
** PCI Configuration Base I/O Address Register (PCI_CBIO)
*/
#define CBIO_MASK   0xffffff80       /* Base I/O Address Mask */
#define CBIO_IOSI   0x00000001       /* I/O Space Indicator (RO, value is 1) */
#define PCI_MAX_BUS_NUM      8
/*#define OPPO_PCI_TOTAL_SIZE 0x00*/       /* I/O address extent */

static int pci_probe(pci_dev_t *pci_dev)
{
    uchar_t irq;
    uchar_t pb, pbus, dev_num, bridge_dev_num, dnum, dev_fn;
    uint16_t vendor, device, index, status;
    uint64_t class = PCI_CLASS_NETWORK_OTHER;
    uint32_t val;
    uint32_t iobase = 0;

    TRY {
	pci_dev->pcibios = NAME_FIND("sys>PCIBios", PCIBios_clp);
    } 
    CATCH_Context$NotFound(name) {
	printf("pci_probe: PCIBios not present\n");
	return;
    }
    ENDTRY;


#define ENTER_BROKEN_INTEL_SECTION() ENTER_KERNEL_CRITICAL_SECTION()
#define LEAVE_BROKEN_INTEL_SECTION() LEAVE_KERNEL_CRITICAL_SECTION()

    ENTER_BROKEN_INTEL_SECTION();
    val = PCIBios$FindDevice(pci_dev->pcibios, PCI_VENDOR_ID_DEC, 
			     PCI_DEVICE_ID_DEC_OPPO, 0, &pb, &dev_fn);
    LEAVE_BROKEN_INTEL_SECTION();
    if(!val)
    {
	
	dev_num = PCI_SLOT(dev_fn);
       
	/* Set the device number information */
	pci_dev->dev_num = dev_num;
	pci_dev->bus_num = pb;

	/* Get the board I/O address */
	ENTER_BROKEN_INTEL_SECTION();
	PCIBios$ReadConfigDWord(pci_dev->pcibios,pb, PCI_DEVICE, 
				PCI_BASE_ADDRESS_0, &iobase);
	LEAVE_BROKEN_INTEL_SECTION();
	if (!iobase)
	{
	    printf("oppo: No IO base\n");
	    goto bad;
	}

#if 0
	pci_dev->base_addr = iobase & PCI_BASE_ADDRESS_MEM_MASK;
	TRC(printf("Base address at: 0x%lx.\n",pci_dev->base_addr));
#endif 
	/* Fetch the IRQ to be used */
	ENTER_BROKEN_INTEL_SECTION();
	PCIBios$ReadConfigByte(pci_dev->pcibios, pb, PCI_DEVICE, 
			       PCI_INTERRUPT_LINE, &irq);
	LEAVE_BROKEN_INTEL_SECTION();
	TRC(printf("IRQ:%d\n",irq));
	if ((irq == 0) || (irq == (uchar_t) 0xff)) 
	{
	    printf("oppo: Dodgy IRQ\n");
	    goto bad;
	}

	pci_dev->irq = irq;

	printf("oppo on irq %d\n", irq);
	pci_dev->devno = PCI_DEVICE;

	/* Get the bridge device number */
	ENTER_BROKEN_INTEL_SECTION();
	PCIBios$FindDevice(pci_dev->pcibios, PCI_VENDOR_ID_DEC, 
			   PCI_DEVICE_ID_DEC_BRD, 0, &pb, &dev_fn);
	LEAVE_BROKEN_INTEL_SECTION();
	bridge_dev_num = PCI_SLOT(dev_fn);
	pci_dev->bridgedevno = PCI_BRIDGE;


	/* 
	** neugebar: Hacks for the PCI ala SMHs hack for s3 and mga
	*/
	{
	  Mem_PMemDesc pmem;
	  Stretch_clp str; 
	  Stretch_Size size, sz; 
	  Frames_clp frames; 
	  addr_t pbase, chk; 
	  uint32_t fwidth; 
	  Mem_Attrs attr; 
	  bool_t free; 
	  word_t oppo_phys; 

	  oppo_phys = pci_dense_to_phys(iobase);
	  /* 
	  ** XXX neugebar: should sort out size from pcibios
	  ** size derived from otto documentation.
	  */
	  size = 0x400000;
	  eprintf("oppo: iobase = %p, oppo_phys = %p, size = %lx\n", 
		  iobase, oppo_phys, size);
	  frames = NAME_FIND("sys>Frames", Frames_clp);
	  pbase  = Frames$AllocRange(frames, size, FRAME_WIDTH, oppo_phys, 0); 
	  fwidth = Frames$Query(frames, pbase, &attr);
	  eprintf("Got memory (pbase is %p): width is 0x%x, attr 0x%x\n", 
		  pbase, fwidth, attr);
	  pmem.start_addr  = pbase; 
	  pmem.nframes     = BYTES_TO_LFRAMES(size, fwidth); 
	  pmem.frame_width = fwidth; 
	  pmem.attr        = 0;
	  
	  str = StretchAllocator$NewAt(Pvs(salloc), size, 0, 
					 oppo_phys, 0, &pmem);
	  pci_dev->base_addr = oppo_phys;
	  TRC(printf("Base address at: 0x%lx.\n",pci_dev->base_addr));
	}
    }
    else
    {
	printf("oppo: Device Not found!!!\n");
    bad:
	return -1;
    }

    return 0;
}

