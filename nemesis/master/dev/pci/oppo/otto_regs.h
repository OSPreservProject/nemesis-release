/* Copyright 1995 Digital Equipment Corporation.               */
/* See file COPYRIGHT.  All rights reserved.                             */
/* Last modified on Fri Sep  8 11:12:30 1995 by burrows        */
/*
  $Id: otto_regs.h 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $

  OTTO is a PCI/TURBOchannel network adapter for ATM/AN2.
  (AN2 differs from standard ATM in three ways:
  	per virtual circuit flow control on its variable bit rate channels

  	guaranteed bandwidth channels on which cells are forwarded
  	according to a schedule.
  OTTO is intened to work with either standard ATM or with the AN2
  extensions.   

  ATM adapters use "pacing" to transmit cells on variable bit rate
  circuits.  That is, they will transmit only m cells in n slot times
  for m and n values specified for each circuit.)

  			-------------------------

  OTTO contains an Altera microsequencer.  References to microcode below
  refer to the code for this device.   It is used to coordinate 
  operations on the OTTO.

  OTTO also contains several Xilinx field programmable gate arrays
  "Xilinx chips".   These are dynamically reprogrammable gate-arrays that
  must be loaded with appropriate code on power-on.   The section
  INITIALIZATION gives the necessary algorithm.
 */

/* scalar types  - all are really unsigned 32-bit ints */
typedef unsigned otto_uword32;		/* a 32 bit word */
typedef unsigned otto_cellindex;	/* cell index */
typedef unsigned otto_vcindex;		/* a virtual circuit id */
typedef unsigned otto_fragindex;	/* a fragment index */
typedef unsigned otto_txpktindex;	/* a transmit packet index */
typedef unsigned otto_rxpktindex;	/* a receive packet index */
typedef unsigned otto_ringindex;	/* a receive ring index */
typedef unsigned otto_schedindex;	/* a schedule index */


/*
  ADDRESS MAP

  The OTTO implements registers, RAM, and ROM that are addressible
  by the host.

  All the addresses given below are relative to the base of the bus slot
  occupied by the device.  
  All accesses are assumed to be 32-bits wide, and 32-bit aligned.
  There are no side-effects on reads.  (Thus, Flamingo/Sandpiper "dense"
  space can be used.)
*/

#define OTTO_ROM	0x000000	/* offset of FLASH ROM in bus slot */
#define OTTO_ROMLEN	(128*1024*4)	/* byte length of host address space
					   taken by FLASH ROM in bus slot */
	/* A FLASH ROM.  Accessed by aligned 32-bit reads to addresses a:
			OTTO_ROM <= a < OTTO_ROM+OTTO_ROMLEN
	   Only the low order byte of each 32-bit word contains data.
	   This ROM contains bus-specific device information, the
	   adapter ID, the Xilinx code to be loaded into the ROM, and
	   the boot code needed to boot a machine via the OTTO.

	   The ROM is accessible at any time.

	   The ROM is programmed via the procedure set out in the section
	   ROM PROGRAMMING.

	   Useful offsets within the ROM are: -tbd-
        */
#define OTTO_ROM_XILINXCODE	(0x0)		/* -tbd- index of otto_uwords
						   of start of xilinx code
						   in ROM */
#define OTTO_ROM_XILINXCODESIZE	(40113)	        /* 5 xilinxes, about 8Keach*/

/*-------------------------------------------------------------------------*/

/* OTTO has a RAM.  It's mapped into the address space densely.  That is
   32-bit reads and writes affect 32-bits of data in the RAM.
   Each byte in the RAM is mapped into the address space 
   twice---once for 32 bit accesses, and once for atomic bit updates.
   RAM reads are always 32-bits wide.
   The RAM has OTTO_RAMLEN bytes, numbered 0 to OTTO_RAMLEN-1.
 */
#define OTTO_RAMLEN	(64*1024*4)	/* length of RAM in bytes */

#define OTTO_RAM	0x100000	/* offset of 32-bit RAM region
					   in bus slot */
	/*  The RAM is read/write.  Accessible only after the Xilinxes have
	    been loaded, and the device is not in reset.

	    32-bit RAM words are accessed by aligned 32-bit reads and writes
  	    to addresses a:  OTTO_RAM <= a < OTTO_RAM+OTTO_RAMLEN.
  	    RAM bytes i..i+3 are accessed at address OTTO_RAM+i where i=0 mod 4.
         */

#define OTTO_RAMA	0x180000 	/*  16-bit atomic writes to
                                            high half of each RAM word */
	/* Write only region.  Accessible only after the Xilinxes have
	   been loaded, and the device is not in reset.
	   An aligned 32-bit write wr(a, X) of value X to address a
	   (where a = OTTO_RAMA + i and 0 <= i < OTTO_RAMLEN/4)
	   is equivalent to:
	   	wr (OTTO_RAM + i,
	   		(rd (OTTO_RAM + i) & (0xffff | ~X)) ^ (X << 16))
	  performed atomically, where rd(a) reads 32 bits from address a.

	  That is, the high 16-bits of the RAM word at address OTTO_RAM + i
	  is modified by masking it with the high order 16-bits of the
	  written value, and xoring the result with the low-order 16-bit of
	  the written value.
         */

#define OTTO_WRITE_HIGH(addr,flip,mask,_od)	\
	OTTO_PIOW ((volatile otto_uword32 *)(((char *)(addr))+OTTO_RAMA-OTTO_RAM), (_od), (((flip) >> 16) | (mask)))
	/* This macro writes the high half of a word in the RAM using the
	   OTTO_RAMA locations.
	   If A is the high half of the word at addr, it does:
	   	A = (A & ~mask) ^ flip
	   flip must have no bits other than in the bottom 16.
	 */
#define OTTO_MASK_ALLBITS_HI	0xffff0000	/* mask to update all 
						   the high bits in a word */

#define OTTO_RAMB	0x140000	/*  16-bit atomic writes to
					    low half of each RAM word */
	/* Write only region.  Accessible only after the Xilinxes have
	   been loaded, and the device is not in reset.
	   An aligned 32-bit write wr(a, X) of value X to address a
	   (where a = OTTO_RAMB + i and 0 <= i < OTTO_RAMLEN/4)
	   is equivalent to:
	   	wr (OTTO_RAM + i,
	   		(rd (OTTO_RAM + i) & (0xffff0000 | ~(X >> 16)))
	   	      ^ (X & 0xffff))
	  performed atomically, where rd(a) reads 32 bits from address a.

	  That is, the low 16-bits of the RAM word at address OTTO_RAM + i
	  is modified by masking it with the high order 16-bits of the
	  written value, and xoring the result with the low-order 16-bit of
	  the written value.
         */

#define OTTO_WRITE_LOW(addr,flip,mask,_od)	\
	OTTO_PIOW ((volatile otto_uword32 *)(((char *)(addr))+OTTO_RAMB-OTTO_RAM), (_od), ((flip) | ((mask) << 16)))
	/* This macro writes the low half of a word in the RAM using the
	   OTTO_RAMB locations.
	   If B is the low half of the word at addr, it does:
	   	B = (B & ~mask) ^ flip
	   flip must have no bits other than in the bottom 16.
	 */
#define OTTO_MASK_ALLBITS_LO 0xffff  /* Mask to update all of the lo half word */
#define OTTO_MASK_VC_LO 0x07ff  /* Mask to update a vc in lo half of a word */
/*-------------------------------------------------------------------------*/

#define OTTO_CSR	0x80000		/* offset of CSR in OTTO bus slot */
	/* 4 32 bit read/write registers.   Some bits are read-only, some
	   some write-only.  The CSR is accessible at any time except
	   during the bus setup delay after a reset.

	   This register is used to reset the device, load the Xilinx chips,
	   read the HostID ROM, enable/disable interrupts, and turn on the LEDs.
	   
	   There is also a CSR in the DMA Xilinx.
	 */
struct otto_csr {
	volatile otto_uword32	resets;
	volatile otto_uword32	loadxilinx;
	volatile otto_uword32	interrupts;
	volatile otto_uword32	leds;
};

	/* Readable bits within CSR.resets are: */
#define OTTO_CSR_INIT		(1 << 0)	/* 1 => xilinx not ready for
							loading */
#define OTTO_CSR_RSTALTERA	(1 << 1)	/* 1 => reseting altera */
#define OTTO_CSR_RSTSUNI	(1 << 2)	/* 1 => reseting suni */
#define OTTO_CSR_NOT_DONE	(1 << 3)	/* 1 => xilinx not yet loaded */
#define OTTO_CSR_DMAINT		(1 << 4)	/* 1 => DMA interrupt request
						   if OTTO_CSR_HOSTID_CS=0 */
#define OTTO_CSR_HOSTID_DO	(1 << 4)	/* Host UID ROM data out
						   if OTTO_CSR_HOSTID_CS=1 */
#define OTTO_CSR_SUNIINT	(1 << 5)	/* 1 => suni intr. request */
#define OTTO_CSR_LEFTLED	(1 << 6)	/* 1 => left LED lit */
#define OTTO_CSR_RIGHTLED	(1 << 7)	/* 1 => right LED lit */

	/* writeable bits within CSR.resets are: */
#define OTTO_CSR_RSTXILINX	(1 << 0)	/* 1 => reset Xilinxes */
#define OTTO_CSR_RSTALTERA	(1 << 1)	/* 1 => reset altera */
#define OTTO_CSR_RSTSUNI	(1 << 2)	/* 1 => reset suni */
#define OTTO_CSR_PROG		(1 << 3)	/* 1 => programme Xilinx */
#define OTTO_CSR_HOSTID_CS	(1 << 5)	/* Host UID ROM chip select */
#define OTTO_CSR_HOSTID_SK	(1 << 6)	/* Host UID ROM serial clock */
#define OTTO_CSR_HOSTID_DI	(1 << 7)	/* Host UID ROM data in */

	/* writeable bits within CSR.loadxilinx are: */
#define OTTO_CSR_DATA		(1 << 0)	/* Xilinx load data */
						/* Writes automatically send
						   one config. clock */

	/* writeable bits within CSR.interrupts are: */
#define OTTO_CSR_DMAENABLE	(1 << 4)	/* 1 => DMA intr. enabled */
#define OTTO_CSR_SUNIENABLE	(1 << 5)	/* 1 => SUNI intr. enabled */

	/* writeable bits within CSR.leds are: */
#define OTTO_CSR_LEFTLED	(1 << 6)	/* 1 => light left LED */
#define OTTO_CSR_RIGHTLED	(1 << 7)	/* 1 => light right LED */

/*-------------------------------------------------------------------------*/

#define OTTO_SUNI	0xa0000		/* the SUNI chip */
	/* At this location is an array of SUNI registers
	   Each register is 8 bits wide, and occupies the low-order
	   8 bits of every 8th 32-bit word.
	   ---need to look at SUNI documentation---
	   NB: The SUNI addressing is shifted by 3 bits, so each word
	   is replicated 8 times.  Thus to access location X in the SUNI,
	   you must refer to (word) offset 8*X.
         */

/*-------------------------------------------------------------------------*/

#define OTTO_CELL	0xc0000		/* the cell chip address */
/* The cell chip is connected to AD bits 16-19.  It provides the address
lines to both FIFOs.  They are copied from AD[16-18] on a PIO write to
the cell chip.  They must be 7 for normal operations, and 5 when
loading the FIFOoffset registers. */

/* values that can be written to the cell chip address */
#define OTTO_CELL_FIFOFLAGS    	0x50000		/* initialize FIFO offsets */
#define OTTO_CELL_USELEFT	0x70000		/* use the left link */
#define OTTO_CELL_USERIGHT	0xf0000		/* use the right link */

/* values that can be read from the cell chip address */
#define OTTO_CELL_NOSIGRIGHT	0x10000		/* No signal on right link */
#define OTTO_CELL_NOSIGLEFT	0x20000		/* No signal on left link */
#define OTTO_CELL_TICKER        0x40000         /* Clock that can be
       locked to the network.  This bit generates a square
       wave with frequency OTTO_CELL_TICK_HZ.  Normally, the
       frequency is locked to our local oscillator.  An option on the
       SUNI to lock the transmitter to the receive clock causes
       this bit to lock to the network clock.  */
#define OTTO_CELL_TICK_HZ    	(8000/16)	/* 1/2 KHz */
#define OTTO_CELL_TROUBLE       0x80000         /* No signal from
						   selected link */
/*     The TROUBLE bit is also connected to the GPIN pin on the SUNI. */
/*     The SUNI can be programmed to make an interrupt when GPIN changes. */

/*-------------------------------------------------------------------------*/

#define OTTO_DMA	0xe0000		/* address of the DMA chip struct */
struct otto_dma {		        /* layout of the DMA chip registers */
        volatile otto_uword32 ticks;       /* read-only ticks register */
		/* counts once per TC cycle */
        volatile otto_uword32 report;      /* report queue addr (TC format) */
	    /* a 32-bit read/write register.  Accessible only after the
	     Xilinxes have been loaded, and the device is not in reset.

	     The driver should write the base address of the report ring
	     (see IN-MEMORY DATA STRUCTURES) to this register during
	     initialization (see INITIALIZATION).  Use the macro
	     OTTO_PHYS_TO_RPTBASE(a) to convert physical byte address (a)
	     to the the correct format for writing to this register.  Use
	     the macro OTTO_RPTBASE_TO_PHYS(a) to convert a value read from
	     this register to a physical address.

	     The report ring must be a physically contiguous block of memory
	     of length OTTO_REP_RING_LEN_BYTES aligned on a
	     OTTO_REP_RING_LEN_BYTES physical boundary.

	     It may be read to determine the current position of the ring
	     pointer. 
	     */
        volatile otto_uword32 status;            /* bytes and flags */
	    /* This word is copied into the dmareport word of each pkt report.*/

        volatile otto_uword32 address;          /* DMA address (TC format) */
	    /* current or last DMA address used */
};

#define OTTO_DMA_STATUS_PIO_UNLOCK (1 << 0)	/* PIO-only unlock bit */
#define OTTO_DMA_STATUS_PIO_TICK (1 << 11)	/* Enable timer interrupts */
    /* The timer goes off every 65K bus clocks. */
    /* That takes 2.621 ms on full speed machines.  Slower on others. */ 
#define OTTO_DMA_STATUS_PIO_RECV (1 << 12)	/* Enable receive interrupts */
#define OTTO_DMA_STATUS_PIO_TRAN (1 << 13)	/* Enable transmit interrupts */

/*-------------------------------------------------------------------------*/

#define OTTO_FIFO	0xf0000		/* address of an internal OTTO FIFO */
	/* This is used only during initialization. */
#define OTTO_FIFO_MAGIC	0x20828220	/* magic number to load into the FIFO
					   location at startup */

/*-------------------------------------------------------------------------*/

#define OTTO_EXEC	0x1c0000	/* execute a microcode instruction */
	/* This location may only be written.   Accessible only after the
	   Xilinxes have been loaded, and the device is not in reset.

	   When a 32-bit word is written to this location, the it is
	   interpretted as an instruction by the microsequencer on the next
	   instruction cycle.   This allows the host to execute arbitrary
	   instructions, one at a time.  Routines can be executed by 
	   writing "call" instructions.

	   This is used for adapter debugging, and in initialization.
	   In normal operation, only the bit patterns described in 
	   the INITIALIZATION section should be used.
         */

/*-------------------------------------------------------------------------*/

#define OTTO_ACKFIFOPTRS	0x1c0000	/* Read the ACK FIFO pointers */
	/* This location may only be read.   Accessible only after the
	   Xilinxes have been loaded, and the device is not in reset.

           This returns the in and out pointers to the ACK FIFO.  
           They are not counters, but LFSR sequencers with a period of 8191.

           LFSR sequence is given by:
           	x' = ((x << 1) & 0x1fff) ^ ((x & 0x1000)? 0 : 0x1601)
         */
#define OTTO_ACKFIFO_OUT(ptrs)   ((ptrs) & 0x1FFF)
#define OTTO_ACKFIFO_IN(ptrs)  (((ptrs) >> 16) & 0x1FFF)
#define OTTO_ACKFIFO_MOD 8191	/* ack fifo values are mod 8191 */

/*-------------------------------------------------------------------------*/

#define OTTO_REQ	0x1d0000		/* enqueue a transmit request */
	/* 32-bit write only.   Accessible only after the
	   Xilinxes have been loaded, and the device is not in reset.

	   When written, OTTO enqueues a packet buffer for sending.
	   The written value contains the packet descriptor number
	   in the high 16 bits and a byte count in the low 16 bits.
	   The byte count must be 0 mod 4.   See the TRANSMIT section.
        */
#define OTTO_MAKE_TXREQ(pkt,size)	(((pkt) << 17) | (size))

/*-------------------------------------------------------------------------*/

#define OTTO_ENQACK	0x1e0000		/* enqueue an ACK request */
	/* 16-bit write only.   Accessible only after the
	   Xilinxes have been loaded, and the device is not in reset.

	   When written, OTTO enqueues an ACK for sending.
	   The written value is a VC to be ACKed.

	   There is no overflow checking.
        */

/*-------------------------------------------------------------------------*/

#define OTTO_EVICT	0x1F0000		/* Evict a partial packet */
	/* 32-bit write only.   Accessible only after the
	   Xilinxes have been loaded, and the device is not in reset.

	   When the constant below is written to address OTTO_EVICT+(rxvc*4), 
           OTTO moves any partially assembled packet for rxvc to 
           the receive dma queue and eventually to memory. 
        */

#define OTTO_EVICT_PKT	0x00004000

/*-------------------------------------------------------------------------*/

#define OTTO_ACTIVATE	0x1F0000		/* Activate a transmit VC */
	/* 32-bit write only.   Accessible only after the
	   Xilinxes have been loaded, and the device is not in reset.

	   When the constants below are written to address
	   OTTO_ACTIVATE+(tcvc*4), OTTO optionally increments or
	   decrements tcvc's credit balance and then considers
	   appending it to the ready queue for opportunistic transmit.

        */

#define OTTO_ACTIVATE_ONLY	0x00008000
#define OTTO_ACTIVATE_INC	0x40000000
#define OTTO_ACTIVATE_DEC	0x40004000

/*-------------------------------------------------------------------------*/
#define OTTO_INTACK	0x200000	/* interrupt acknowledge */
	/* if this bit pattern is masked into any address used to read
	   or write the board, and pending interrupt is acknowlegded */


#define OTTO_PHYS_TO_RPTBASE(a)	OTTO_PHYS_TO_BUS(a)
	/* converts a physical byte address to the format used by the 
	   report ring pointer register
         */
#define OTTO_RPTBASE_TO_PHYS(a)	OTTO_BUS_TO_PHYS(a)
	/* converts from the format used by the report ring pointer register
	   to a physical byte address
         */

/* XXX NAS */

#ifdef BUS_PCI
  #ifdef EB164
  #define OTTO_PHYS_TO_BUS(phys) (isoppo?(phys+ALCOR_DMA_WIN_BASE):OTTO_PHYS_TO_TC(phys))
  #define OTTO_BUS_TO_PHYS(busa) (isoppo?(busa-ALCOR_DMA_WIN_BASE):OTTO_TC_TO_PHYS(busa))
  #else
  #define OTTO_PHYS_TO_BUS(phys) (isoppo?(phys):OTTO_PHYS_TO_TC(phys))
  #define OTTO_BUS_TO_PHYS(busa) (isoppo?(busa):OTTO_TC_TO_PHYS(busa))
  #endif
#define OTTO_PHYSW_TO_BUS(physw) (isoppo?(physw):OTTO_PHYSW_TO_TC(physw))
#define OTTO_BUS_TO_PHYSW(busa) (isoppo?(busa):OTTO_TC_TO_PHYSW(busa))
#else
#define OTTO_PHYS_TO_BUS OTTO_PHYS_TO_TC
#define OTTO_BUS_TO_PHYS OTTO_TC_TO_PHYS
#define OTTO_PHYSW_TO_BUS OTTO_PHYSW_TO_TC
#define OTTO_BUS_TO_PHYSW OTTO_TC_TO_PHYSW
#endif

/* TURBOchannel specific macros */
#define OTTO_PHYS_TO_TC(phys) OTTO_PHYSW_TO_TC((phys) >> 2)
	/* converts a physical byte address to a bus address */
#define OTTO_TC_TO_PHYS(tcaddr) (OTTO_TC_TO_PHYSW (tcaddr) << 2)
	/* converts a physical byte address to a bus address */
#define OTTO_PHYSW_TO_TC(physw)	(((physw) << 5) | (((physw) >> 27) & 0x1f))
	/* converts a physical WORD address to a bus address. */
#define OTTO_TC_TO_PHYSW(tcaddr)   ((((tcaddr) & 0x1f) << 27) | \
				     (((tcaddr) >> 5) & 0x7ffffff))
	/* converts a bus address to a physical word address.  */


/* IN-MEMORY DATA STRUCTURES
   The adapter accesses these via DMA.
   The report ring a physically contiguous piece of memory
   OTTO_REP_RING_LEN_BYTES (== 2**OTTO_REP_RING_LOGLEN) bytes long.
   It is aligned on a OTTO_REP_RING_LEN_BYTES boundary in physical memory.

   The report ring should be allocated at startup by the operating system.
   Its address is given to the adapter by writing to the OTTO_RPT register.
   	wr (OTTO_RPT, OTTO_RPTBASE (phys_address))

   The report ring is an array OTTO_REP_RING_LEN_REPS of otto_rep structures.
   Each otto_rep structure is 16 bytes long.  It consists of 4 32-bit words.

   An otto_rep is written to the report ring each time a packet is DMAed
   into the OTTO in preparation for transmit, every time a packet is
   sent on the wire, and a packet (or partial packet) is received into
   host memory.  Each otto_rep is written at the position pointed at by
   the OTTO_RPT register, which is than advanced by 16 bytes (wrapping
   to the beginning of the ring if the end of the ring is reached).

   For a transmit report only the ticks, VCI (in seqreport word)
   and TX bit (in dmareport word) are valid.

   The driver can determine whether a new report has been delivered by:
   	read the ring pointer register from struct otto_dma  (== slow PIO read)

   	check the tag bit:  it is written as (i mod 2), where i is the
   	number of times the report ring has wrapped.    (in this technique,
   	the tags bits should be initialized to 1)

	check the tag and complement-tag bits:  one of these is bound to
	be set when a report is written.  The driver should zero
	reports after use when using this technique.
 */
struct otto_rep {
	otto_uword32 crcresult;         /* The result of computing the
					   CRC on the data transmitted or
					   delivered to memory. */
	otto_uword32 ticks;		/* value of cycle counter when
				           report was written.
				           For receive, that's just after
				           the packet is written into memory.
					   For transmit, that's at the
					   next time the DMA engine is
					   free after the last cell of
					   a packet has been transmitted.  */
	otto_uword32 seqreport;		/* contains pkt desc index, and VCI */
					/* see macros below */
	otto_uword32 dmareport;		/* see macros below */
};

/* the type field in seqreport indicates whether a transmit or a receive */
#define OTTO_REP_SEQ_ISXMT		(1 << 30)	/* 0 for RX, 1 for TX*/
/* this macro extracts the VCI the cell was sent/received on */
#define OTTO_REP_SEQ_VCI(seqreport)	(((seqreport) >> 16) & 0x7ff)

/* For received packets: */
#define OTTO_REP_SEQ_PKT(seqreport) (((seqreport) & 0x7fff) >> 1) /* pkt desc */
#define OTTO_REP_SEQ_ANYCNG	(1 << 28)	/* some cell saw congestion */
#define OTTO_REP_SEQ_CNG	(1 << 29)	/* congestion encountered on
						   more than half the cells in
						   this packet. */
#define OTTO_REP_SEQ_EOP	(1 << 31)	/* EOP cell */

/* for transmitted packets */
#define OTTO_REP_SEQ_SCHED(seqreport)	((seqreport) & 0x1fff)	/* sched time */
#define OTTO_REP_SEQ_SENT              	(1 << 31)    /* 0 if DMAed, 1 if sent */


#define OTTO_REP_DMA_BYTECOUNT(dmareport)	((dmareport) & 0xfffff)
	/* extract the byte count from a dmareport word in an otto_rep */
	/* The byte count should be 0 mod 4 and less than 0x10000 */

/* these bits are flags inside the dmareport word */
#define OTTO_REP_DMA_DMAACTIVE	(1 << 20)	/* DMA engine is processing
						   a packet or report */
#define OTTO_REP_DMA_NEEDFRAG	(1 << 21)	/* DMA engine is waiting for
						   Address/length (or Report
						   info) from Altera */
#define OTTO_REP_DMA_FIFOWAIT	(1 << 22)	/* DMA engine is waiting
						   for the FIFO to fill/empty */
#define OTTO_REP_DMA_EOP	(1 << 23)	/* DMA engine is waiting
						   for transmit FIFO to drain */
#define OTTO_REP_DMA_FIFORDY	(1 << 24)	/* DMA-to-VRAM FIFO is ready */
#define OTTO_REP_DMA_REPORT	(1 << 25)	/* Current/last DMA op was rep*/
#define OTTO_REP_DMA_TRANSMIT	(1 << 26)	/* Current/last DMA op was tx */
#define OTTO_REP_DMA_RECEIVE	(1 << 27)	/* Current/last DMA op was rx */
#define OTTO_REP_DMA_DMAERR	(1 << 28)	/* TC error during DMA-fatal */
#define OTTO_REP_DMA_CRCERROR	(1 << 29)	/* rcv pkt has CRC error */
#define OTTO_REP_DMA_TAG	(1 << 30)	/* tag bit */
#define OTTO_REP_DMA_NEGTAG	(1 << 31)	/* complement of tag bit */
#define OTTO_REP_DMA_TAGS	(3 << 30)	/* both tag bits */

				    /* return the tag-flipped version of x */
#define OTTO_REP_TAGFLIP(x)	((x) ^ OTTO_REP_DMA_TAGS)

#define OTTO_REP_RING_LOGLEN 16
#define OTTO_REP_RING_LEN_BYTES (1 << OTTO_REP_RING_LOGLEN)
#define OTTO_REP_RING_LEN_REPS (OTTO_REP_RING_LEN_BYTES / sizeof (struct otto_rep))

/* Packet buffers are the only other in-memory data structure.
   Each packet buffer consists of a number of fragments.   Each fragment
   is a multiple of 4 bytes in length and aligned on a 4 byte boundary.

   Packet buffer are described by packet buffer descriptors,
   which are linked lists of fragment descriptors.
   See the STATIC RAM DATA STRUCTURES section.
 */

/* STATIC RAM DATA STRUCTURES
	The data structures below are expressed as little-endian
	C bit-fields.   Since bit-fields are not portable between compilers,
	these are useful mostly for documentation.   When accessing
	fields of these record, the driver should use the macros
	provided.

	Location OTTO_DEFAULTRING holds the location of the default
	receive ring.   The value put into this field should be 
	obtained from OTTO_MAKE_DEFAULTRING(ring).

	Location OTTO_SCHEDULESTART is used to reloed the shedule each time
	the schedule counter wraps.

	Location OTTO_CRASHFLAG is written with the value OTTO_CRASHVALUE
	if the interface crashes.

	The value in OTTO_RXPRERELOAD is used to reload the reassembly
	timeout register whenever the value in that register decrements
	to zero.   The location OTTO_RXPRERELOAD should be set by software
	to OTTO_RXPREVALUE(x), where x = t/2.8 and t is the desired
	timeout in milliseconds, or zero if the timeout is to be
	disabled (the default).   When the device is reset, the reassembly
	timeout register should initialized to OTTO_RXPREVALUE(x)
	using the OTTOEXEC_load_rxprectr exec code.

	The RAM data structures include several interleaved parallel arrays.

	The RAM contains:
		An array of cell descriptors.
		An array that implements the acknowledgement fifo.
		An array that implements the transmit schedule.
		An array of transmit VC records (INTERLEAVED with receive desc)
		An array of receive VC records (INTERLEAVED with transmit desc)
		An array of descriptor slots. Each slot may describe:
			A ring-of-packets
			a transmit packet
			a receive packet
			a DMA fragment from a packet

	The transmit and receive VC record arrays are actually not simple
	arrays.  Instead they each consist of several parallel arrays that 
	are interleaved with one another.     Similarly, the acknowledgement
	fifo and transmit schedule are interleaved.
	Fortunately, the driver needs only initialize most of these
	data structres.  Only the descriptors are changed by the driver
	in normal operation.

	In more detail:
	    The array of cell descriptors is:
		struct otto_cell cell[OTTO_MAXCELLS];
	    Each cell descriptor may be for a receive or transmit
	    cell---the distinciton is made by which list it's on.   The
	    actual data in the cell is held in a parallel array in a
	    video RAM.   The start address of the array is 4*OTTO_CELLSPOS
	    bytes from the start of the RAM.  A "cellindex" is an index into
	    this array.  The first two cell buffers (0 and 1) must not be used.

	    The acknowledgement fifo array consists of the ackffvc
	    field of each element of the array:
		struct otto_schedackff sched_ackff[OTTO_SCHEDSIZE];
	    The transmit schedule array consists of the schedvc field of each
	    element of the same array.   The start address of the array is
	    4*OTTO_SCHEDPOS bytes from the start of the RAM.   A "schedindex"
	    is an index into this array.

	    The array of transmit VC records is actually split across
	    two parallel arrays, one of which is interleaved with 
	    receive VC information:
		struct otto_txvc_headtail txvchead_txvctail[OTTO_MAXVC];
		struct otto_txvccred_rxvcring txvccred_rxvcring[OTTO_MAXVC];
	    These start at 4*OTTO_VCPOS and 4*(OTTO_VCPOS+OTTO_MAXVC) bytes
	    from the start of the RAM, repectively.  The "credits",
	    "overflow", "nocredit", and "enflowcntl" fields are
	    associated with the transmit VC.   A transmit "vcindex" is
	    an index into these two arrays.  The first VC (VC 0) must
	    not be used.

	    The array of receive VC records is actually split across three
	    parallel arrays, one of which is interleaved with 
	    transmit VC information:
		struct otto_txvccred_rxvcring txvccred_rxvcring[OTTO_MAXVC];
		struct otto_rxvc_headtail rxvchead_rxvctail[OTTO_MAXVC];
		struct otto_rxvc_oldernewer txvcolder_rxvcnewer[OTTO_MAXVC];
	    These start at 4*(OTTO_VCPOS+OTTO_MAXVC),
	    4*(OTTO_VCPOS+2*OTTO_MAXVC), and 4*(OTTO_VCPOS+3*OTTO_MAXVC) bytes
	    from the start of the RAM, repectively.  The "ring" and
	    "reserved0" fields are associated with the receive VC.   A
	    receive "vcindex" is an index into these three arrays.  The first
	    VC (VC 0) must not be used.

	    The DMA fifo:
		struct otto_dmaff dmaff[OTTO_DMAFFSIZE];
	    starts at 4*OTTO_DMAFFPOS bytes from the start of the RAM.

	    The ready fifo:
		struct otto_rdyff rdyff[OTTO_RDYFFSIZE];
	    starts at 4*OTTO_RDYFFPOS bytes from the start of the RAM.

	    The report fifo:
		struct otto_rptff rptff[OTTO_RPTFFSIZE];
	    starts at 4*OTTO_RPTFFPOS bytes from the start of the RAM.

	    A region of reserved space starting at byte position
	    4*OTTO_RESERVEDPOS of length 4*OTTO_RESERVEDSIZE bytes
	    appears next in RAM.


	    The descriptor array is:
		union otto_desc desc[OTTO_MAXDESC];
	    which starts at byte 4*OTTO_DESCPOS after the start of the RAM.
	    A "fragindex", "txpktindex", "rxpktindex", and "ringindex" 
	    values are all indexes into this array.  A fragindex must
	    always be even---a fragment descriptor takes two elements in the
	    array, the first one always being even.
 */

#define OTTO_SCHEDULESTART 0x7000
		/* location where schedule gets reloaded from */
#define OTTO_DEFAULTRING 0x7001	/* location where default ring id is stored */
#define OTTO_MAKE_DEFAULTRING(ring) ((ring) << 1)
#define OTTO_CRASHFLAG 0x7002
#define OTTO_CRASHVALUE 0xf0f0f0f0
#define OTTO_RXPRERELOAD 0x7003
#define OTTO_RXPREVALUE(x) ((x) == 0? 0: (((x) << 1) | 1))

struct otto_cell {
	otto_vcindex vc: 11;		/* txvc or rxvc of this cell */
					/* This field is not used when this
				   	cell is on the receive or transmit
		                   	free list */
	otto_uword32 reserved0: 4;	/* reserved */
	otto_uword32 eop: 1;		/* 1 => this cell is marked eop */
					/* This field is not used when this
				   	cell is on the receive or transmit
				   	free list. */
	otto_cellindex next: 13;	/* index of next cell */
	otto_uword32 reserved1: 1;	/* reserved */
	otto_uword32 samegroup: 1;	/* 1 => next is same group */
					/* a cell group is the unit of
					   delivery to the host --it's
					   normally a packet, unless the 
					   adapter has run out of room.
					   This field must be 1 when this 
					   cell is on a free list.  */
	otto_uword32 valid: 1;		/* 1 => next is valid */
};
/* sizeof (struct otto_cell) == 4 */

/* a normal free receive cell has valid and samegroup  bit=1 and
   valid next field--- everything else is "don't care"  */
#define OTTO_MAKE_FREE_RXCELL(next)	((1 << 31) | (1 << 30) | ((next) << 16))
/* the last free receive cell has valid bit=0 and samegroup bit 1 --- 
   Every thing else is "don't care" */
#define OTTO_MAKE_TAIL_FREE_RXCELL	(1 << 30)

/* a normal free transmit cell has valid bit=1, valid next field and 
   samegroup field = 1 - everything else is "don't care" */
#define OTTO_MAKE_FREE_TXCELL(next)	((1 <<31) | (1 << 30) | ((next) << 16))
/* the last free receive cell has valid bit=0, and samegroup field=1
   - everything else is "don't care" */
#define OTTO_MAKE_TAIL_FREE_TXCELL	(1 << 30)

struct otto_schedackff {
					/* these two fields are associated with
					   the acknowledgement fifo */
	otto_vcindex ackffvc: 11;	/* rxvc on ack fifo */
					/* the driver should never touch
					   this field */
	otto_uword32 reserved0: 5;	/* reserved */

					/* these two fields are associated with
					   the transmit schedule */
	otto_vcindex schedvc: 11;	/* txvc that owns this slot in
					   the schedule */
	otto_uword32 reserved1: 4;	/* reserved */
	otto_uword32 valid: 1;	        /* 0 => nothing scheduled in this slot */
};
/* sizeof (struct otto_schedackff) == 4 */
#define OTTO_MAKE_SCHED(x)	(0x8000 | (x))

struct otto_rxvc_headtail {
	otto_cellindex tail: 13;	/* index of tail cell on assembly
					   queue */
	otto_uword32 reserved0: 1;	/* reserved */
	otto_uword32 ack: 1;		/* 1 => generate acks for received
					   cells on this vc */
	otto_uword32 pkt: 1;		/* 1 => AAL5 packet mode,
				           0 => every cell is a packet */
	otto_cellindex head: 13;	/* index of head cell on assbly queue */
	otto_uword32 reserved1: 1;	/* reserved */
	otto_uword32 ign: 1;		/* 1 => ignore cells arriving on this VC */
	otto_uword32 valid: 1;		/* 1 => head is valid
					   i.e. assembly queue is non-empty */
};
/* sizeof (struct otto_rxvc_headtail) == 4 */
#define OTTO_RXVC_ACK	(1 << 14)
#define OTTO_RXVC_PKT	(1 << 15)
#define OTTO_RXVC_IGN	(1 << 30)
#define OTTO_RXVC_IGNH	(1 << 14)

struct otto_rxvc_oldernewer {
	otto_vcindex newer: 11;		/* next newer partial packet vc */
	otto_uword32 reserved0: 5;	/* reserved */
	otto_vcindex older: 11;		/* next older partial packet vc */
	otto_uword32 reserved1: 4;	/* reserved */
	otto_uword32 ancient: 1;	/* 1 => marked as ancient partial
					   packet */
};
/* sizeof (struct otto_rxvc_oldernewer) == 4 */

struct otto_txvc_headtail {
	otto_cellindex tail: 13;	/* index of tail cell on disassembly
				           queue */
	otto_uword32 reserved0: 1;	/* reserved */
	otto_uword32 usesched: 1;	/* 1 => send according to schedule */
	otto_uword32 ready: 1;		/* 1 => this txvc is on the ready
					   queue */
	otto_cellindex head: 13;	/* index of head cell on disassembly
					   queue */
	otto_uword32 reserved1: 2;	/* reserved */
	otto_uword32 valid: 1;		/* 1 => head is valid
					   i.e. the disassembly queue is
					   non-empty */
};
/* sizeof (struct otto_txvc_headtail) == 4 */
#define OTTO_TXVC_USESCHED 		(1 << 14)

struct otto_txvccred_rxvcring {
					/* these two fields are accociated with
					   the receive VC */
	otto_ringindex ring: 15;	/* ring-of-packets descriptor for
					   this VC */
	otto_uword32 w13: 1;		/* 13-word mode */

					/* these four fields are associated with
					   the transmit VC */
	otto_uword32 credits: 13;	/* current credit balance minus 1 */
	otto_uword32 overflow: 1;	/* 1 => overflow has occurred */
	otto_uword32 nocredit: 1;	/* 1 => no credits are available */
				/* must be same is top bit of credit field */
	otto_uword32 enflowcntl: 1;	/* 1 => enable credit flow-control
					   for this vc */
};
/* sizeof (struct otto_txvccred_rxvcring) == 4 */
#define OTTO_RXVC_W13	(1 << 15)

#define OTTO_MAKE_RXVCRING(ring)	((ring) << 1)
#define OTTO_MASK_RXVCRING		0x7fff
#define OTTO_MAKE_TXVCCREDITS(credits)	(credits)
#define OTTO_MASK_TXVCCREDITS		0x1fff
#define OTTO_TXVC_OVERFLOW 		(1 << 13)
#define OTTO_TXVC_NOCREDIT 		(1 << 14)
#define OTTO_TXVC_ENFLOWCNTL 		(1 << 15)

struct otto_dmaff {
	otto_uword32 len: 16;		/* byte count */
	otto_txpktindex pktd: 15;	/* index of tx packet descriptor */
	otto_uword32 reserved1: 1;	/* reserved */
};
/* sizeof (struct otto_dmaff) == 4 */

struct otto_rdyff {
	otto_uword32 reserved0: 16;	/* reserved */
	otto_vcindex vc: 11;		/* txvc with credit and cell to send */
	otto_uword32 reserved1: 5;	/* reserved */
};
/* sizeof (struct otto_rdyff) == 4 */

struct otto_rptff {
	otto_uword32 reserved0: 16;	/* reserved */
	otto_vcindex vc: 11;		/* txvc to generate sent report for */
	otto_uword32 reserved1: 5;	/* reserved */
};
/* sizeof (struct otto_rptff) == 4 */

struct otto_ringdesc {
	otto_rxpktindex pktd: 15;	/* index of current rx packet desc */
	otto_uword32 tag: 1;		/* current free buffer polarity */
	otto_uword32 reserved1: 16;	/* reserved */
};
/* sizeof (struct otto_ringdesc) == 4 */
#define OTTO_TAG0	0
#define OTTO_TAG1	(1 << 15)
#define OTTO_MAKE_RING(tag,pktd)	((tag) | ((pktd) << 1))
#define OTTO_TAG_FLIP(x)	((x) ^ OTTO_TAG1)

struct otto_rxpktdesc {
	otto_fragindex frag: 15;	/* index of head fragment descriptor */
	otto_uword32 tag: 1;		/* full/empty tag (based on current
				           ring polarity) */
	otto_rxpktindex pktd: 15;	/* index of next rx packet descriptor */
	otto_uword32 wrap: 1;		/* 1 => next pktd wraps the ring.
					   The tag bit in the ring descriptor
					   is xored with wrap after the
					   packet descriptor is used.
				         */
 /* XXX congestion stuff goes here XXX */
};
/* sizeof (struct otto_rxpktdesc) == 4 */
#define OTTO_DOWRAP	(1 << 31)
#define OTTO_DONTWRAP	0
#define OTTO_MAKE_RXPKT(wrap,pktd,tag,frag) ((wrap) | ((pktd) << 17) | (tag) | ((frag) << 1))

struct otto_txpktdesc0 {
	otto_fragindex frag: 15;	/* index of head fragment descriptor */
	otto_uword32 reserved0: 1;	/* reserved */
	otto_vcindex vc: 11;		/* txvc to use for this packet */
	otto_uword32 reserved1: 1;	/* reserved */
	otto_uword32 clp: 1;		/* 1 => Set CLP on cells of this pkt */
	                                /*  CLP => lower priority */
	                                /*  CLP is masked off by EOP on
	                                	last cell */
	otto_uword32 nocrc: 1;		/* 1 => suppress CRC */
	otto_uword32 w13: 1;		/* 1 => Cell header comes from DMA */
	otto_uword32 eop: 1;		/* 1 => Set EOP on last cell of pkt */
	                                /*      This should normally be on. */
};
/* sizeof (struct otto_txpktdesc0) == 4 */
#define OTTO_TXPKT_DOCLP	(1 << 28)
#define OTTO_TXPKT_DOCRC	0
#define OTTO_TXPKT_DONTCRC	(1 << 29)
#define OTTO_TXPKT_W13		(1 << 30)
#define OTTO_TXPKT_DOEOP	(1 << 31)
#define OTTO_MAKE_TXPKT0(crc,vc,frag) ((crc) | ((vc) << 16) | ((frag)<<1) | OTTO_TXPKT_DOEOP)

struct otto_txpktdesc1 {
	otto_uword32 reserved: 30;	/* reserved */
	otto_uword32 action: 2;		/* action to take after DMAing pkt */
};
/* sizeof (struct otto_txpktdesc1) == 4 */
#define OTTO_MAKE_TXPKT1(actioncode)	((actioncode) << 30)
#define OTTO_TXACTION_SEND	0		/* send the packet */
#define OTTO_TXACTION_DROP	1		/* drop the packet */
#define OTTO_TXACTION_ECHO	2		/* echo the packet
					       (internal OTTO loopback)
					       Only useful for h'ware debugging.
					       Don't use in a normal driver. */

struct otto_txpktdesc {
	struct otto_txpktdesc0 word0;
	struct otto_txpktdesc1 word1;
};

struct otto_fragdesc0 {
	otto_uword32 size: 16;		/* fragment size in bytes */
	otto_uword32 next: 15;		/* index of next fragment descriptor */
	otto_uword32 reserved: 1;	/* reserved */
};
/* sizeof (struct otto_fragdesc0) == 4 */
#define OTTO_MAKE_FRAG0(size,next)	((size) | (next << 17))

struct otto_fragdesc1 {
	otto_uword32 addr: 32;	/* fragment address in bus format */
};
/* sizeof (struct otto_fragdesc1) == 4 */
#define OTTO_MAKE_FRAG1(addr)	OTTO_PHYS_TO_BUS(addr)

struct otto_fragdesc {
	struct otto_fragdesc0 word0;
	struct otto_fragdesc1 word1;
};

/* SRAM structure definition */
/* (struct otto_sram) is a C description of all of OTTO's RAM */

/* a generic two-word OTTO descriptor */
struct otto_descriptor {
	otto_uword32 word0;
	otto_uword32 word1;
};

union otto_desc {
	struct otto_ringdesc ring;
	struct otto_rxpktdesc rxpktd;
	struct otto_txpktdesc txpktd;
	struct otto_fragdesc0 frag0;
	struct otto_fragdesc1 frag1;
};

#define OTTO_CELLSPOS	0x0000
#define OTTO_MAXCELLS	0x2000
#define OTTO_SCHEDPOS	(OTTO_CELLSPOS+OTTO_MAXCELLS)
#define OTTO_SCHEDSIZE	0x2000
#define OTTO_VCPOS	(OTTO_SCHEDPOS+OTTO_SCHEDSIZE)
#define OTTO_MAXVC	0x0800
#define OTTO_VCARRAYS	5
#define OTTO_DMAFFPOS	(OTTO_VCPOS+OTTO_VCARRAYS*OTTO_MAXVC)
#define OTTO_DMAFFSIZE	0x0200
#define OTTO_RDYFFPOS	(OTTO_DMAFFPOS+OTTO_DMAFFSIZE)
#define OTTO_RDYFFSIZE	0x0200
#define OTTO_RPTFFPOS	(OTTO_RDYFFPOS+OTTO_RDYFFSIZE)
#define OTTO_RPTFFSIZE	0x0200
#define OTTO_RESERVEDPOS (OTTO_RPTFFPOS+OTTO_RPTFFSIZE)
#define OTTO_RESERVEDSIZE (OTTO_DESCPOS-OTTO_RESERVEDPOS)
#define OTTO_DESCPOS	0x8000
#define OTTO_MAXDESC	0x8000
#define OTTO_MAXDOUBLEDESC (OTTO_MAXDESC/2)

#define OTTO_CELLS_PER_ROW	16	/* cells per VRAM row */
	/* this value is needed because it's illegal to allocate
	   a transmit cell and a receive cell in the same VRAM row */

#define OTTO_PANIC_CELLS 6	/* number of panic receive cells.
			Must be at least as big as the receive pipeline. */
#define OTTO_UNUSABLE_CELLS 2	/* the first two cells are not usable */

struct otto_sram {
	struct otto_cell cell[OTTO_MAXCELLS];
	struct otto_schedackff sched_ackff[OTTO_SCHEDSIZE];
	struct otto_txvc_headtail txvchead_txvctail[OTTO_MAXVC];
	struct otto_txvccred_rxvcring txvccred_rxvcring[OTTO_MAXVC];
	struct otto_rxvc_headtail rxvchead_rxvctail[OTTO_MAXVC];
	struct otto_rxvc_oldernewer txvcolder_rxvcnewer[OTTO_MAXVC];
	struct otto_dmaff dmaff[OTTO_DMAFFSIZE];
	struct otto_rdyff rdyff[OTTO_RDYFFSIZE];
	struct otto_rptff rptff[OTTO_RPTFFSIZE];
	otto_uword32 reserved1[OTTO_RESERVEDSIZE];
	union otto_desc desc[OTTO_MAXDESC];
};


/*
  TRANSMIT
 */

/*
  INITIALIZATION
 */


/*
  ROM PROGRAMMING
 */
