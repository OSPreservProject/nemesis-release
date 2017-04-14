/*
*****************************************************************************
**                                                                          *
** NOTE:  This code was written by Digital Equipment Corporation.           *
**        Since this is the only hardware documentation available it is     *
**        largely unmodified (and shamelessly ripped off).                  *
**                                                                          *
**        Main changes are to the driver interface - and to remove all      *
**        traces of LAN emulation.                                          *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      otto_hw.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Hardware Support procedures for Otto driver.  
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: otto_hw.h 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
** LOG: $Log: otto_hw.h,v $
** LOG: Revision 1.1  1997/02/14 14:39:15  nas1007
** LOG: Initial revision
** LOG:
** LOG: Revision 1.1  1997/02/12 13:05:04  nas1007
** LOG: Initial revision
** LOG:
 * Revision 1.1  1995/10/12  11:08:46  prb12
 * Initial revision
 *
**
*/

#ifndef _OTTO_HW_H_
#define _OTTO_HW_H_

#include "otto_regs.h"
#include "otto_exec.h"
#include "suni.h"

/* Here's how this driver manages the descriptors in the OTTO.
   You should first read the OTTO hardware spec to understand what the 
   descriptors do.

   This code treats descriptors as 64-bit quantities.    That is
   descriptor n starts at  byte position 8*n into the descriptor
   table (which is at address OTTO_DESCPOS within the static RAM--
   see otto_regs.h)

   OTTO_FINALFRAG is a fragment descriptor for a few hundred
   bytes of otherwise unused memory.  The purpose is to 
   absorb unwanted DMA writes.

   OTTO_EMPTYPKT is pkt descriptor pointing to one fragment,
   OTTO_FINALFRAG, and looped back to itelf so it has infinite
   length.

   OTTO_EMPTYRING is a receive rignt that looks perpetually
   empty.  It can never receive a packet.  There is one packet
   descriptor, OTTO_EMPTYPKT, always disabled.

   OTTO_FULLRING is a receive ring that looks perpetually full.
   It receive an infinite number of packets of infinte length.
   There is one packet descriptor, OTTO_EMPTYPKT, always enabled.

   There are OTTO_NTXPKTS transmit packet descriptor, starting
   at descriptor OTTO_TXPKTSTART.   Each transmit descriptor
   is immediately follwed by OTTO_MAXFRAGSPERTXBUF
   fragment descriptors.  The packet descriptor always points
   to the fragment descriptor following it.  If a transmit packet has
   more fragments than  OTTO_MAXFRAGSPERTXBUF, it must be copied to avoid
   fragments.

   There are OTTO_NRINGS receive ring descriptors, starting at 
   descriptor OTTO_RINGSSTART.  Each ring descritpor is immediately
   followed by OTTO_MAXPKTSPERRING receive packet descriptors.
   The ring descriptor invariably points into the packets immediately
   following it, which are arranged in a ring.

   There are OTTO_NRXPKTS collections of fragment descriptors,
   starting at descriptor OTTO_RXPKTSTART.  Each collection
   contains OTTO_MAXFRAGSPERRXBUF fragment descriptors.  The fragments
   in such a collection are invariably used to hold the fragemnts
   of a single receive packet.
 */
/* 4 4 7 */
#define OTTO_MAXFRAGSPERTXBUF	15	/* no of frags per txpkt */
#define OTTO_MAXFRAGSPERRXBUF	5	/* no of frags per rxpkt */
#define OTTO_MAXPKTSPERRING	15	/* no of rxpkt's per ring 
					   more efficient if this 
					   is (2**n)-1 */

/* these macros allocate descriptors to various functions */
#define OTTO_FINALFRAG		0
#define OTTO_EMPTYPKT		1
#define OTTO_EMPTYRING		2
#define OTTO_FULLRING		3

#define OTTO_NTXPKTS		128
#define OTTO_TXPKTSTART		4
#define OTTO_TXPKTEND           (OTTO_TXPKTSTART+(OTTO_MAXFRAGSPERTXBUF+1)*OTTO_NTXPKTS)

#define OTTO_NRINGS		64
#define OTTO_RINGSSTART		OTTO_TXPKTEND
#define OTTO_RINGEND	        (OTTO_RINGSSTART+(OTTO_MAXPKTSPERRING+1)*OTTO_NRINGS)

#define OTTO_NRXPKTS            ((OTTO_MAXDOUBLEDESC-OTTO_RXPKTSTART)/OTTO_MAXFRAGSPERRXBUF)
#define OTTO_RXPKTSTART		OTTO_RINGEND
#define OTTO_RXPKTEND	        (OTTO_RXPKTSTART+OTTO_MAXFRAGSPERRXBUF*OTTO_NRXPKTS)

#define OTTO_TXCELLS            (OTTO_MAXCELLS/4)
#define OTTO_NOACKCELLS         (OTTO_MAXCELLS/4)

#define OTTO_PRI_PKT 2		/* number of outstanding priority packets */

#define OTTO_MAXXMTQUEUED       (OTTO_NTXPKTS-1-OTTO_PRI_PKT)
	/* max number of packets to queue for xmt on otto */
#define OTTO_MAXXMTBYTESQUEUED  (48*OTTO_TXCELLS-9180-200-512*OTTO_PRI_PKT)
	/* max number of bytes in transmit packets queued on device
	   The 200 allows space for header and trailer bytes.
	   The 9180 is a maximum length MTU.
           The 512*OTTO_PRI_PKT allows for OTTO_PRI_PKT priority packets.  */

#define OTTO_CRCLEN 4           /* size of AAL5 CRC */
#define OTTO_MINTRAILERLEN 8    /* min size of AAL5 trailer */
#define OTTO_MAXTRAILERLEN 56   /* max size of AAL5 trailer (rounded up) */

/* Ring of packets */
struct otto_ring {
	otto_uword32   tag;		/* tag to use */
	unsigned short desc;		/* index of ring descriptor */
	unsigned short next;		/* free list */
	unsigned short nextfree;	/* next free entry in ring */
	unsigned short lastused;	/* last used entry in ring */
		    /* The ring is empty when nextfree == lastused.
		       The ring can never be totally filled --- it is
		       illegal to use an entry if doing so would set
		       nextfree == lastused.  */
        unsigned short missed;		/* packets we couldn't allocate
       					   last time */
        unsigned short pktindex[OTTO_MAXPKTSPERRING];
       					/* indexes of rxpkt structures
       					   that have been queued on this 
       					   ring */
	word_t   rxclient;
};

/* receive packet buffers */
struct otto_rxpkt {
	 unsigned short next;		/* free list */
	 unsigned short desc;		/* index of pkt descriptor */
	 struct rbuf *r;		/* rbuf mapped */
};

/* transmit packet buffers */
struct otto_txpkt {
	 unsigned short next;		/* free list */
	 unsigned short desc;		/* index of pkt descriptor */
	 unsigned len;			/* packet length, to keep
	 				   track of cell usage in adapter */
	 struct rbuf   *r;		/* rbuf mapped */
};

struct otto_hw {
  OTTO_LOCKTYPE statelock;		/* lock */
  
  char    *slotaddress;			/* address of TC slot */
  unsigned sparseshift;                   /* sparse space shift */  
  volatile otto_uword32 *rom;		/* address of the ROM */
  unsigned romsize;			/* address space taken by ROM
					   in bytes */
  
  volatile otto_uword32 *ram;		/* address of the RAM */
  unsigned ramsize;			/* addres space taken by RAM
					   in bytes */
  unsigned ncellbuffers;		/* # of cell buffers in RAM */
  
  volatile struct otto_descriptor *descriptors;  /* descriptors in RAM */
  unsigned ndescriptors;		/* number of 8 byte descriptors
					   in RAM (used for pkt descriptors, 
					   ring descriptors,
					   fragment descriptors)*/
  
  unsigned reportringsize;		/* report ring size in bytes */
  
  volatile otto_uword32    *ram_writelo; /* atomic lo-half writes */
  volatile otto_uword32    *ram_writehi; /* atomic hi-half writes */
  
  volatile struct otto_csr *csr;	/* main CSR */
  volatile struct otto_dma *dma;	/* DMA chip */
  volatile otto_uword32    *fifo;	/* FIFO */
  volatile otto_uword32    *cell;	/* cell chip */
  volatile otto_uword32    *suni;	/* SUNI chip */
  volatile otto_uword32    *exec;	/* execute microcode request */
  volatile otto_uword32    *txrequest;	/* transmit request */
  volatile otto_uword32    *activate;	/* activate txvc */
  volatile otto_uword32    *ackfifoptrs; /* read ACK FIFO ptrs */
  
  /* these addresses do the same as those above except accesses also
     acknowledge a pending interrupt */
  volatile otto_uword32    *rom_intack;
  volatile otto_uword32    *ram_intack;
  volatile struct otto_descriptor *descriptors_intack;
  volatile otto_uword32    *ram_writelo_intack;
  volatile otto_uword32    *ram_writehi_intack;
  volatile struct otto_csr *csr_intack;
  volatile struct otto_dma *dma_intack;
  volatile otto_uword32    *fifo_intack;
  volatile otto_uword32    *cell_intack;
  volatile otto_uword32    *suni_intack;
  volatile otto_uword32    *exec_intack;
  volatile otto_uword32    *txrequest_intack;
  
  OTTO_LOCKTYPE execlock;		/* protects use of exec and ram[0] */
  
  otto_uword32 ucodeversion;		/* microcode version
					   Initialized by otto_unsetreset */
  otto_uword32 ucodecreated;		/* microcode creation date
					   Initialized by otto_unsetreset */
  
  int ispotto;				/* non-zero if this is a product OTTO
					   rather than a SRC OTTO */
  int isoppo;			/* non-zero if this is a PCI OTTO */
  
  /* information about how the hardware has been programmed */
  volatile struct otto_rep *reportbase;
  /* system virtual address
     of report ring */
  unsigned txcells;		/* number of transmit cells allocated */ 
  unsigned rxcells;		/* number of receive cells allocated */
  unsigned noackcells;		/* number of noack receive cells
				   allocated */ 
  
  unsigned ringfreelist;	/* head of free list of ring descriptors */
  unsigned ringmissedlist;	/* head of list of ring descriptors
				   with missing buffers */
  struct otto_ring  rings[OTTO_NRINGS];	/* array of rings */
  
  unsigned rxpktfreelist;	/* head of free list of rxpkt descriptors */
  struct otto_rxpkt rxpkts[OTTO_NRXPKTS]; /* array of receive pkts */
  
  unsigned txpktfreelist;	/* head of free list of txpkt descriptors */
  struct otto_txpkt txpkts[OTTO_NTXPKTS]; /* array of transmit packets */
  
  /* per VC data */
  struct otto_vcdata {
    unsigned flags;		/* flags---see below */
#define OTTO_VC_IGNORE	0x01	/* ignore received packets */
#define OTTO_VC_FC	0x02	/* flow controlled */
#define OTTO_VC_SCHED	0x04	/* scheduled */
#define OTTO_VC_PVC	0x08	/* a permanent virtual circuit */
#define OTTO_VC_ARPXMT	0x10	/* inARP has been sent */
#define OTTO_VC_ARPRCV	0x20	/* inARP has been received */
#define OTTO_VC_ARPRCV_RECENT 0x40 /* inARP has been received recently */
#define OTTO_VC_DONT_ARP 0x80	/* don't arp on this circuit */
#define OTTO_VC_STOPPED 0x100	/* stopped for credit resync */
#define OTTO_VC_RVC     0x200	/* a resilient virtual circuit */
#define OTTO_VC_SVC     0x400	/* a switched virtual circuit */
#define OTTO_VC_FIXED   0x800	/* allocated for internal use */
#define OTTO_VC_USED    0x1000	/* used to send a packet recently */
#define OTTO_VC_UPDATED 0x2000	/* updated by incoming packet recently*/
#define OTTO_VC_STALE 0x4000	/* address needs to be updated soon */
    /* 0x8000 is reserved for OTTOPVM */
#define OTTO_VC_NOFC 0x10000	/* must not use flow control */
#define OTTO_VC_XMTSTART 0x20000 /* VC tried to transmit recently */
#define OTTO_VC_XMTDONE 0x40000	/* VC did transmit recently */
#define OTTO_VC_XMTPEND 0x80000	/* VC has old pending transmit */
#define OTTO_VC_XMTFLUSH 0x100000 /* VC is being flushed */
#define OTTO_VC_ALLOCATED (OTTO_VC_PVC|OTTO_VC_RVC|OTTO_VC_SVC|OTTO_VC_FIXED)
    unsigned short bandwidth;	/* bandwidth for this VC in
				   cells per "schedlen" cells
				   if this VC is scheduled */
    unsigned short rxring;	/* ring associated with VC */
    unsigned txpkts;            /* Tail of list of packets queued on
                                   transmit VC.  The list is circular,
                                   so next(tail) = head. */
    unsigned short rxclient;	/* rxclient associated with VC */
    unsigned short txclient;	/* txclient associated with VC */

  } vcs[OTTO_MAXVC];
  
  unsigned nxmtvcs;		/* number of VCs transmiting */
  unsigned short xmtvcs[OTTO_NTXPKTS];	/* VCs transmitting */
  
  
  OTTO_LOCKTYPE schedlock;	/* protects sched,schedlen,schedhalf,
				   schedfree, and all the "bandwidth"
				   fields of vcs */
  unsigned schedlen;		/* current schedule length */
  unsigned sched[OTTO_SCHEDSIZE / 2]; /* schedule */
  /* the factor of two is because OTTO's schedule is
     in two halves---only one half is used at a time */
  unsigned schedhalf;		/* which half of sched is in use:0,1*/
  unsigned schedfree;		/* free slots in schedule */
  
  unsigned defaultring;		/* default receive ring */

  int nextreport;		/* index of next report */
  otto_uword32 reporttag;	/* tag expected in report */
  
  int deferred;			/* => xmt and receive
				   interrupts have been disabled,
				   and tick interrupts enabled
				   to reduce interrupts at 
				   high load */
  
  /* watchdog values */
  struct otto_watchdog {
    unsigned forcereset;	/* force a reset */
    unsigned resetsuni;		/* suni should be held reset
				   for this many seconds */
    unsigned time;		/* watchdog time */
    unsigned gotintr;		/* interrupt routine entrered */
    unsigned queuedrcvbuf;	/* queued a buffer for rcv */
    unsigned queuedxmtbuf;	/* queued a buffer for xmt */
    unsigned gotrcv;		/* received a packet */
    unsigned gotxmt;		/* transmitted a packet */
    unsigned linkworking;	/* some link working */
    unsigned reset;		/* reset called */
    unsigned notgettingcells;	/* SUNI upset on last look */
    otto_uword32 lastinterrupt;	/* ticks reading at last
				   interrupt */
  } otto_watchdog;
  
  unsigned linkinuse;		/* which link is in use */
  int otto_sdh;                 /* SDH (Not Sonet) Flag */
};

/* A xilinx code descriptor */
struct otto_xilinxdesc {
	unsigned char *data;	/* the data */
	int stride;		/* stride between bytes of data */
	int checksum;		/* sum of bytes */
	int len;		/* bytes in data */
};

int otto_initialize (char *devicebase, struct otto_hw *ohw,
	struct otto_xilinxdesc *xilinxcode,
	unsigned txcells, unsigned noackcells, char *reportbase);
/* Initialize the otto_hw structure.  Return 0 on success and -1 on failure.
   This routine calls the routines below to accomplish this. 
   This routine may print error messages on the console when it fails.

   devicebase is a system virtual address for the base
   of the device in IO space.  It will be passed to otto_sethwaddresses.

   If xilinxcode is not 0, it should point to a xilinx code descriptor.
   If it is 0, the default xilinxcode for the device is loaded.
   xilinxcode will be passed to otto_loadxilinx.

   txcells and noackcells are values passed to otto_buildcelllists.

   reportbase should point to a natually aligned, contiguous, 64Kbyte region
   for the report ring.  */

int otto_softreset (struct otto_hw *ohw, unsigned txcells, 
		    unsigned noackcells);
/* reset the device, and reinitialize the VC and descriptor free lists.
   The device is left in a state where it can send and receive packets.
   txcells and noackcells are values passed to otto_buildcelllists. */

void otto_sethwaddresses (char *devicebase, struct otto_hw *ohw,
			  char *reportbase);
/* Initialize the addresses in the otto_hw structure.
   This routine must be called before of the routines below are celled.

   devicebase is a system virtual address for the base
   of the device in IO space.
   reportbase should point to a natually aligned, contiguous, 64Kbyte region
   for the report ring. */

void otto_initfreelists (struct otto_hw *ohw);
/* initialize the VC and descriptor free lists */

void otto_setreset (struct otto_hw *ohw);
/* Put device in reset state. */

void otto_unsetreset (struct otto_hw *ohw);
/* Take device out of reset state.
   As a side effect, this routinie initializes od->ucodeversion and 
   od->ucodecreated */

void otto_uselink (struct otto_hw *ohw, int link);
/* Cause the hardware of the OTTO to use link 0 or 1 according to the
   setting of "link" */

void otto_lockpioonly (struct otto_hw *ohw);
/* Lock microsequencer to service PIO requests only. */

void otto_unlockpioonly (struct otto_hw *ohw);
/* Allow microsequencer to service non-PIO requests, and 
   turn on normal interrupts. */

void otto_defer (struct otto_hw *ohw);
/* Allow microsequencer to service non-PIO requests,
   disable normal interrupts, and enable the timer interrupt. */

int otto_loadxilinx (struct otto_hw *ohw,
		     struct otto_xilinxdesc *xilinxcode);
/* Load otto's xilinx chips.  Return 0 on success, -1 on failure.
   If xilinxcode is not 0, it should point to a xilixn code descriptor.
   If it is 0, the default xilinxcode for the device is loaded.

   This routine may print an error message on the console if it fails.
   When it reports failure, it is a hard failure --- several attempts
   have been made.
 */

int otto_sramtest (struct otto_hw *ohw);
/* Test the static RAM. Returns 0 on success, and -1 on failure.
   The static RAM is zeroed when this routine returns.

   This routine may print error messages on the console when it fails.  */

void otto_setreportbase (struct otto_hw *ohw);
/* Set the base of the report ring using the reportbase field in od.
   This field should contain the system virtual address of the report
   ring area---a physically contiguous region of size od->reportringsize.  
   The ring is initialized to have all tag bits set. */

void otto_buildcelllists (struct otto_hw *ohw,
			  unsigned txcells, unsigned noackcells);
/* Construct initial cell lists for the device.
   txcells cells are set aside for transmit.  (The driver must ensure that
   no more than txcells transmit cells could ever be in use.)

   noackcells cells are set aside for the cells that are guaranteed
   to be available for flow controlled traffic.   All other cell
   buffers will be used for receive cells.    It is good for there
   to be many receive cell buffers in addition to those strictly
   needed, because this allows the sender to transmit a burst of cells
   and to receive acknowledgements quickly, without waiting for cells
   to be given to the host processor.  */

void otto_initsuni (struct otto_hw *ohw);
/* take suni chip out of reset and initialize it */

void otto_resetsuni (struct otto_hw *ohw);
/* put suni chip into reset state */

unsigned otto_allocring (struct otto_hw *ohw);
/* allocate a ring descriptor, and return its index, or 0 if no descriptor
   exists.  */

void otto_freering (struct otto_hw *ohw, unsigned index);
/* free a ring descriptor allocated by otto_allocdesc.  It is illegal
   to free descriptor 0 */

unsigned otto_allocrxpkt (struct otto_hw *ohw);
/* allocate a receive packet descriptor, and return its index, or 0
   if no descriptor exists.  */

void otto_freerxpkt (struct otto_hw *ohw, unsigned index);
/* free a receive packet descriptor allocated by otto_allocrxpkt.  It
   is illegal to free descriptor 0 */

unsigned otto_alloctxpkt (struct otto_hw *ohw);
/* allocate a transmit packet descriptor, and return its index, or 0
   if no descriptor exists.  */

unsigned otto_allocq (struct otto_hw *ohw, unsigned bw);
/* Allocate a queue suitable for bandwidth bw, and return it.
   Called with softc lock held */

void otto_deallocq (struct otto_hw *ohw, unsigned q);
/* Deallocate queue q.
   Called with softc lock held */

void otto_freetxpkt (struct otto_hw *ohw, unsigned index);
/* free a transmit packet descriptor allocated by otto_alloctxpkt.  It
   is illegal to free descriptor 0 */

void otto_setrxvc (struct otto_hw *ohw, unsigned vc,
		   int ign, int pkt, int ack, int ring);
/* Set the ign, pkt, ack and ring values in an receive VC's structure.
   For each of ign, pkt, ack and ring, the value -1 indicates no change.
   For booleans, 0 reset the bit, and non-zero sets the bit */

void otto_settxvc (struct otto_hw *ohw, unsigned vc,
		   int usesched, int enflowcntl, int overflow, int credits);
/* Set the usesched, enflowcntl, overflow and credits values in a
   transmit VC's structure.  For each of usesched, enflowcntl,
   overflow and credits, the value -1 indicates no change.  For
   booleans, 0 reset the bit, and non-zero sets the bit */

void otto_setsched (struct otto_hw *ohw, unsigned index, unsigned vc);
/* Sets schedule slot index to indicate the given vc. */

void otto_wait_for_phaselock (struct otto_hw *ohw);
/* waits a while for the SUNI and PLL to acquire lock */
/* It's unclear what happens if we continue operating the device
  when lock isn't acquired.   However, since we may need to call this
  from an interrupt routine, it waits only a limited time. */

int otto_update_bandwidth (struct otto_hw *ohw, unsigned vc, int bw);
/* Cause "vc"  to have "bw" slots allocated in the schedule. 
   Does not update the hardware---use otto_newsched for that.
   Returns -1 if the request cannot be granted, 0 if no change was requested
   and 1 if a change was necessary to fulfil the request.
   Called with softc lock held. */

void otto_newsched (struct otto_hw *ohw);
/* install a new schedule in the hardware from the softc */

void otto_ackfifoptrs (struct otto_hw *ohw, unsigned *read, unsigned *write);
/* Atomically read the values of both the read and write pointer of
   the OTTO ACK FIFO.  The OTTO provides these values in LFSR format,
   but this routine converts them to binary. */

int otto_gettingcells (struct otto_hw *ohw);
/* return non-zero iff the SUNI is receiving cells and the far end can
   receive our cells (i.e. it's not asserting far-end-receive failure)
   and this state has been true since the last call to this procedure. */

void otto_setled (struct otto_hw *ohw, unsigned link, unsigned state);
/* set LED on link "link" (0 or 1) to: on if "state" is 1 the opposite
   of its current state if "state" is 2 off otherwise */

#endif /* _OTTO_HW_H_ */
