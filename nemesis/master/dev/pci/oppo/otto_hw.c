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
**      otto_hw.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Hardware Support procedures for Otto driver.  
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: otto_hw.c 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
** LOG: $Log: otto_hw.c,v $
** LOG: Revision 1.1  1997/02/14 14:39:09  nas1007
** LOG: Initial revision
** LOG:
** LOG: Revision 1.3  1997/02/13 17:52:12  nas1007
** LOG: *** empty log message ***
** LOG:
** LOG: Revision 1.1  1997/02/12 13:04:36  nas1007
** LOG: Initial revision
** LOG:
 * Revision 1.1  1995/10/12  11:08:46  prb12
 * Initial revision
 *
**
*/

#include <io.h>
#include "otto_platform.h"
#include "otto_hw.h"

#define TRC(x)

/* Dummy buffer used for the final DMA fragment.   This fragment is
   used to terminate all DMA chains.   It will swallow all received
   packets that are too long and source all transmit packets that are too
   long. */
#define DUMMYLEN (1 << 8)	/* must be power of two > 2 and <= page size */
static unsigned char dummybuffer[2*DUMMYLEN];
/* allocate 2*length to allow for rounding to make physically contiguous */

#define BASIC_RESETS (OTTO_CSR_RSTXILINX | OTTO_CSR_RSTALTERA | OTTO_CSR_RSTSUNI)
/* send a single bit to the UID serial ROM */
static uidromsendbit (struct otto_hw *ohw, int bit)
{
	bit = (bit? OTTO_CSR_HOSTID_DI: 0);
	OTTO_PIOW (&ohw->csr->resets, ohw, BASIC_RESETS | OTTO_CSR_HOSTID_CS | bit);
	wbflush ();
	microdelay (1);
	OTTO_PIOW (&ohw->csr->resets, ohw,
		BASIC_RESETS | OTTO_CSR_HOSTID_CS | bit | OTTO_CSR_HOSTID_SK);
	wbflush ();
	microdelay (1);
}

/* check the UID ROM - put the UID in ohw->physaddress */
/* on entry to this routine, the device is in reset state */
static int uidok (struct otto_hw *ohw)
{
	unsigned cksum;
	static unsigned char test[8] = {
		0xff, 0x00, 0x55, 0xaa, 0xff, 0x00, 0x55, 0xaa
	};
	unsigned char romcontents[66];
	unsigned char uida[8];
	int ok = 1;
	unsigned crc;
	int i;

	/* The UID serial ROM is arranged as follows:
		there are 33 interesting 16-bit words, which
		come out in little-endian order. (i.e. low address first)

		the bits within each 16 bit word are in bigendian order
		(i.e. high-numbered bits first).  Strange, but true.
	 */
	for (i = 0; i != 33; i++) {
		unsigned w = 0;
		int j;

		OTTO_PIOW (&ohw->csr->resets, ohw,
			BASIC_RESETS);	/* turn off chip select */
		wbflush ();
		microdelay (1);
		OTTO_PIOW (&ohw->csr->resets, ohw,
		    BASIC_RESETS | OTTO_CSR_HOSTID_CS); /* turn on chip select */
		wbflush ();
		microdelay (1);

		uidromsendbit (ohw, 1);		/* send a 1 */
		uidromsendbit (ohw, 1);		/* send a 1 */
		uidromsendbit (ohw, 0);		/* send a 0 */

		/* send bits 5..0 of address */
		for (j = 5; j >= 0; j--) {
			uidromsendbit (ohw, (i >> j) & 1);	/* send bit */
		}

		for (j = 0; j != 16; j++) {
			unsigned x;
			w <<= 1;
			uidromsendbit (ohw, 0);		/* send a 0 */
			x = OTTO_PIOR (&ohw->csr->resets, ohw);
					/* read bit */
			if ((x & OTTO_CSR_HOSTID_DO) != 0) {
				w |= 1;
			}
		}
		romcontents[i*2+0] = w;
		romcontents[i*2+1] = w >> 8;

		OTTO_PIOW (&ohw->csr->resets, ohw, BASIC_RESETS);
			/* turn off chip select */
		wbflush ();
	}

	if (ohw->isoppo && romcontents[24] == test[1]) {
		/* Very early prototype OPPOs have byte swapped UID ROMs */
		printf ("Suspect prototype OPPO -- swapping UID ROM bytes\n");
		for (i = 0; i != 66; i += 2) {
			unsigned char swap = romcontents[i+0];
			romcontents[i+0] = romcontents[i+1];
			romcontents[i+1] = swap;
		}
	}

	for (i = 0; i != 6; i++) {
		uida[i] = romcontents[i];
#if 0 /* NAS */
		ohw->physaddress.ether_addr_octet[i] = romcontents[i];
#endif /* 0 */
	}

#ifdef mips
	if (forcetalc) {
		unsigned long *narom;
		unsigned int naalign;
		switch (cpu) {
                case DS_5000:		/* 3max */
			{		/* ethernet is in slot 6 on a 3max */
				caddr_t nareg = (caddr_t) PHYS_TO_K1 (tc_slotaddr[6]);
      				/* numbers from if_ln_data.c ds5000sw */
				narom = (u_long *)(((u_long)nareg) + 0x001C0000);
 				naalign = 16;
			}
                        break;
		case DS_5000_100:	/* 3min */
#ifdef DS_MAXINE
                case DS_MAXINE:		/* MAXine */
#endif
      			/* numbers from if_ln_data.c ds3minsw/dsMaxinesw */
			narom = (u_long *)PHYS_TO_K1(0x1C080000);
			naalign = 0;
			break;
#ifdef DS_5000_300
                case DS_5000_300:	/* 3max+/bigmax */
			/* numbers from if_ln_data.c ds3minsw */
			narom = (u_long *)PHYS_TO_K1(0x1F880000);
			naalign = 0;
                    break;
#endif
		}
	}
#endif

	/* old/weak checksum */
	cksum = uida[0]*(1<<10) + uida[1]*(1<<2) +
	        uida[2]*(1<<9)  + uida[3]*(1<<1) +
	        uida[4]*(1<<8)  + uida[5]*(1<<0);
	cksum %= 0xffff;
	uida[6] = cksum >> 8;
	uida[7] = cksum;

	for (i = 0; i != 8; i++) {
		if (romcontents[i] != uida[i] ||
		    romcontents[i+8] != uida[7-i] ||
		    romcontents[i+16] != uida[i] ||
		    romcontents[i+24] != test[i]) {
			ok = 0;
		}
	}

        /* check new/strong CRC */
        crc = 0;
        for (i = 0; i != 64; i++) {
                unsigned temp = romcontents[i];
                int j;
                for (j = 0; j != 8; j++) {
                        unsigned xor = ((temp >> j) ^ crc) & 1;
			crc = (crc >> 1) ^ (xor? 0xa001 : 0);
                }
        }
        if (crc != ((romcontents[65] << 8) | romcontents[64])) {
                ok = 0;
        }
	return (ok);
}

int otto_softreset (struct otto_hw *ohw, 
		    unsigned txcells, unsigned noackcells)
{
    volatile otto_uword32 *vcrings;
    int i;
    unsigned long phys;
    unsigned long phys1;
    otto_uword32 rxprevalue;
    int isoppo = ohw->isoppo;

    /* Don't want to hold the Xilinx in reset too long - bad! */
    ENTER_KERNEL_CRITICAL_SECTION();
    HIGH_IPL();
    otto_setreset (ohw);		/* enter reset state */
    microdelay (100);
    i = uidok (ohw);
    otto_unsetreset (ohw);	/* exit reset state */
    LOW_IPL();
    LEAVE_KERNEL_CRITICAL_SECTION();
    
    if (!i) {
	printf ("otto: UID ROM error\n");
	return (-1);
    }

    ENTER_BROKEN_INTEL_SECTION()
    /* Initialize FIFOs */
    OTTO_PIOW (ohw->cell, ohw, OTTO_CELL_FIFOFLAGS);
    wbflush ();
    OTTO_PIOW (ohw->fifo, ohw, OTTO_FIFO_MAGIC);
    wbflush ();
    
    OTTO_PIOW (ohw->cell, ohw, OTTO_CELL_USERIGHT);
    wbflush ();

    /*OTTO_PIOW (&ohw->suni[8*SUNI_MC], ohw, SUNI_MC_DLE );*/ /* Loopback mode */
    /* ohw->suni[8*SUNI_MC] = SUNI_MC_DLE; (Put SUNI into loopback mode) */
    OTTO_PIOW (&ohw->suni[8*SUNI_MC], ohw, 0x0);
                        /* Put SUNI into normal mode */
    wbflush();
    
    /* turn off both leds */
    OTTO_PIOW (&ohw->csr->leds, ohw, 0);
    wbflush ();
    LEAVE_BROKEN_INTEL_SECTION();
    
    /* lock microsequencer into PIO-only mode */
    otto_lockpioonly (ohw);

    if (otto_sramtest (ohw) == -1) { /* test the RAM */
	printf ("otto: RAM test failed\n");
	return (-1);
    }
    /* RAM is now zero */

    otto_setreportbase (ohw);
    
    otto_initfreelists (ohw);
    
    /* Construct final fragment */
    phys = (unsigned long)OTTO_SVTOPHY (dummybuffer);
    /* ensure physically contiguous  (DUMMYLEN <= page size) */
    phys1 = (unsigned long)OTTO_SVTOPHY (dummybuffer+DUMMYLEN);
    if (phys + DUMMYLEN != phys1) {
	phys = phys1;
    }
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->descriptors[OTTO_FINALFRAG].word0, 
	       ohw, OTTO_MAKE_FRAG0 (DUMMYLEN, OTTO_FINALFRAG));
    OTTO_PIOW (&ohw->descriptors[OTTO_FINALFRAG].word1,
	       ohw, OTTO_MAKE_FRAG1 (phys));
    
    /* make a dummy packet descriptor */
    OTTO_PIOW (&ohw->descriptors[OTTO_EMPTYPKT].word0,
	       ohw, OTTO_MAKE_RXPKT (OTTO_DONTWRAP,
				    OTTO_EMPTYPKT, OTTO_TAG1, OTTO_FINALFRAG)); 
    /* Construct empty ring-of-packets descriptor */
    OTTO_PIOW (&ohw->descriptors[OTTO_EMPTYRING].word0,
	       ohw, OTTO_MAKE_RING (OTTO_TAG0, OTTO_EMPTYPKT));
    /* Construct full ring-of-packets descriptor */
    OTTO_PIOW (&ohw->descriptors[OTTO_FULLRING].word0,
   		ohw, OTTO_MAKE_RING (OTTO_TAG1, OTTO_EMPTYPKT));
    /* point all (as yet unused) receive VC's at full ring of
       packets descriptor */
    vcrings = &ohw->ram[OTTO_VCPOS+(1*OTTO_MAXVC)];
    for (i = 0; i != OTTO_MAXVC; i++) {
	OTTO_PIOW (&vcrings[i], ohw, OTTO_MAKE_RXVCRING (OTTO_FULLRING)); 
    }
    LEAVE_BROKEN_INTEL_SECTION();
    
    /* This makes VC 0 (= junk cells) not be in packet mode */
    otto_setrxvc (ohw, 0, 1, 0, 0, OTTO_FULLRING);
    
    /* construct the cell free lists */
    otto_buildcelllists (ohw, txcells, noackcells);
    
    /* initialize the SUNI chip */
    otto_initsuni (ohw);

    /* enable DMA interrupts */
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->csr->interrupts, ohw, OTTO_CSR_DMAENABLE);
    LEAVE_BROKEN_INTEL_SECTION();
	
    otto_unlockpioonly (ohw);
    /* device can now send and receive packets */

    return (0);
}


int otto_fullreset (struct otto_hw *ohw, struct otto_xilinxdesc *xilinxcode,
		    unsigned txcells, unsigned noackcells)
{
    /* XXX NAS */
    if (otto_loadxilinx (ohw, xilinxcode) == -1) {	/* load xilinxes */
	printf ("otto: xilinx load failed\n");
	return (-1);
    }
    return (otto_softreset (ohw, txcells, noackcells));
}


/* initialize the device */
int otto_initialize (char *devicebase, struct otto_hw *ohw,
		     struct otto_xilinxdesc *xilinxcode,
		     unsigned txcells, unsigned noackcells, char *reportbase)
{
    otto_sethwaddresses (devicebase, ohw, reportbase);
    return (otto_fullreset (ohw, xilinxcode, txcells, noackcells));
}

/* initialize addresses and sizes in hardware record */
void otto_sethwaddresses (char *devicebase, struct otto_hw *ohw,
			  char *reportbase)
{

    if (ohw->isoppo) {
	ohw->sparseshift = 5;
    } else {
	ohw->sparseshift = 0;
    }

    ohw->slotaddress = devicebase;
    ohw->rom = (volatile otto_uword32 *) (devicebase + OTTO_ROM);
    ohw->ram = (volatile otto_uword32 *) (devicebase + OTTO_RAM);
    ohw->descriptors = (volatile struct otto_descriptor *) 
	&ohw->ram[OTTO_DESCPOS];
    ohw->ram_writelo = (volatile otto_uword32 *) (devicebase + OTTO_RAMB);
    ohw->ram_writehi = (volatile otto_uword32 *) (devicebase + OTTO_RAMA);
    ohw->csr = (volatile struct otto_csr *) (devicebase + OTTO_CSR);
    ohw->dma = (volatile struct otto_dma *) (devicebase + OTTO_DMA);
    ohw->fifo = (volatile otto_uword32 *) (devicebase + OTTO_FIFO);
    ohw->cell = (volatile otto_uword32 *) (devicebase + OTTO_CELL);
    ohw->suni = (volatile otto_uword32 *) (devicebase + OTTO_SUNI);
    ohw->exec = (volatile otto_uword32 *) (devicebase + OTTO_EXEC);
    ohw->txrequest = (volatile otto_uword32 *) (devicebase + OTTO_REQ);
    ohw->activate = (volatile otto_uword32 *) (devicebase + OTTO_ACTIVATE);
    ohw->ackfifoptrs =
	(volatile otto_uword32 *) (devicebase + OTTO_ACKFIFOPTRS);
    
    ohw->rom_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_ROM + OTTO_INTACK);
    ohw->ram_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_RAM + OTTO_INTACK);
    ohw->descriptors_intack = 
	(volatile struct otto_descriptor *) &ohw->ram_intack[OTTO_DESCPOS];
    ohw->ram_writelo_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_RAMB + OTTO_INTACK);
    ohw->ram_writehi_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_RAMA + OTTO_INTACK);
    ohw->csr_intack = (volatile struct otto_csr *)
	(devicebase + OTTO_CSR + OTTO_INTACK);
    ohw->dma_intack = (volatile struct otto_dma *)
	(devicebase + OTTO_DMA + OTTO_INTACK);
    ohw->fifo_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_FIFO + OTTO_INTACK);
    ohw->cell_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_CELL + OTTO_INTACK);
    ohw->suni_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_SUNI + OTTO_INTACK);
    ohw->exec_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_EXEC + OTTO_INTACK);
    ohw->txrequest_intack = (volatile otto_uword32 *)
	(devicebase + OTTO_REQ + OTTO_INTACK);

    ohw->romsize = OTTO_ROMLEN;
    ohw->ramsize = OTTO_RAMLEN;
    ohw->ncellbuffers = OTTO_MAXCELLS;
    ohw->ndescriptors = OTTO_MAXDOUBLEDESC;
    ohw->reportringsize = OTTO_REP_RING_LEN_BYTES;
    
    ohw->reportbase = (volatile struct otto_rep *) reportbase;
    
    ohw->ucodeversion = 0;
    ohw->ucodecreated = 0;
    ohw->otto_sdh = 0;		/* Default to Sonet Interface */
}

/* initialize the VC and descriptor free lists */
void otto_initfreelists (struct otto_hw *ohw)
{
    unsigned i;
    unsigned j;

    /* Set up free list for ring descriptors.  Entry 0 in the array is
       never used */
    ohw->ringfreelist = 1;	/* head of free list */
    ohw->ringmissedlist = 0;	/* head of missed list */
    for (i = 1; i != OTTO_NRINGS; i++) {
	ohw->rings[i].next = i+1;
	ohw->rings[i].desc = OTTO_RINGSSTART +
	    ((OTTO_MAXPKTSPERRING+1)*i);
	ohw->rings[i].tag = OTTO_TAG0;
	ohw->rings[i].nextfree = 0;
	ohw->rings[i].lastused = 0;
	ohw->rings[i].missed = 0;
	bzero ((char *) ohw->rings[i].pktindex,
	       sizeof (ohw->rings[i].pktindex));
    }
    ohw->rings[OTTO_NRINGS-1].next = 0; /* end of free list */

    /* Set up free list for receive packet descriptors. 
       Entry 0 in the array is never used */
    ohw->rxpktfreelist = 1;	/* head of free list */
    ohw->rxpkts[0].r = (struct rbuf *) 0;
    for (i = 1; i != OTTO_NRXPKTS; i++) {
	ohw->rxpkts[i].next = i+1;
	ohw->rxpkts[i].desc = OTTO_RXPKTSTART +
	    (OTTO_MAXFRAGSPERRXBUF*i);
	ohw->rxpkts[i].r = 0;
    }
    ohw->rxpkts[OTTO_NRXPKTS-1].next = 0; /* end of free list */


    /* Set up free list for transmit packet descriptors. 
       Entry 0 in the array is never used */
    ohw->txpktfreelist = 1;	/* head of free list */
    ohw->txpkts[0].r = (struct rbuf *) 0;
    j = 0;
    for (i = 1; i != OTTO_NTXPKTS; i++) {
	ohw->txpkts[i].next = i+1;
	ohw->txpkts[i].desc = OTTO_TXPKTSTART +
	    ((OTTO_MAXFRAGSPERTXBUF+1)*i);
	ohw->txpkts[i].r = 0;
    }
    ohw->txpkts[OTTO_NTXPKTS-1].next = 0; /* end of free list */
    /* This list is not circular.  Entry 0 is
       unused, and is being used as a list terminator */
    
    /* zero all transmit VC packet queues */
    for (i = 0; i != OTTO_MAXVC; i++) {
	ohw->vcs[i].txpkts = 0;
    }
}

/* after this call OTTO is in reset state */
void otto_setreset (struct otto_hw *ohw)
{
    OTTO_PIOW (&ohw->csr->resets, ohw,
		(OTTO_CSR_RSTXILINX|OTTO_CSR_RSTALTERA|OTTO_CSR_RSTSUNI));
    wbflush ();
}

/* after this call OTTO is no longer in reset state */
void otto_unsetreset (struct otto_hw *ohw)
{
    OTTO_PIOW (&ohw->csr->resets, ohw, 0);
    wbflush ();

    /* save the ucode version and timestamp written
       by microcode when it comes out of reset state */
    ohw->ucodeversion = OTTO_PIOR (&ohw->ram[0], ohw);
    ohw->ucodecreated = OTTO_PIOR (&ohw->ram[1], ohw);
}

/* after this call, OTTO can do only PIO activity,
   and receive interrupts are turned on */
void otto_lockpioonly (struct otto_hw *ohw)
{
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->dma->status, ohw,
	       (OTTO_DMA_STATUS_PIO_TRAN|OTTO_DMA_STATUS_PIO_RECV) &
	       ~OTTO_DMA_STATUS_PIO_UNLOCK);
    wbflush ();
    LEAVE_BROKEN_INTEL_SECTION();
}

/* after this call, OTTO can do non-PIO activity, and normal interrupts are
   enabled */
void otto_unlockpioonly (struct otto_hw *ohw)
{
    wbflush ();
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->dma->status, ohw,
	       (OTTO_DMA_STATUS_PIO_TRAN|OTTO_DMA_STATUS_PIO_RECV) |
	       OTTO_DMA_STATUS_PIO_UNLOCK);
    LEAVE_BROKEN_INTEL_SECTION();
    wbflush ();
}

/* after this call, OTTO can do non-PIO activity, normal interrupts are
   disabled, and timer inetrrupts are enabled  */
void otto_defer (struct otto_hw *ohw)
{
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->dma->status, ohw,
	       (OTTO_DMA_STATUS_PIO_TICK) | OTTO_DMA_STATUS_PIO_UNLOCK);
    LEAVE_BROKEN_INTEL_SECTION();
    wbflush ();
}

/* load the Xilinxes on the OTTO */
int otto_loadxilinx (struct otto_hw *ohw, struct otto_xilinxdesc *xilinxcode)
{
    int stepsize;
    int i;
    int len=0;
    int retry;
    unsigned char *x = (unsigned char *) 0;

    if (xilinxcode != (struct otto_xilinxdesc *) 0) {
	int j;
	int checksum;

	x = xilinxcode->data;
	stepsize = xilinxcode->stride;
	len = xilinxcode->len;
	
	checksum = 0;
	j = 0;
	for (i = 0; i != len; i++) {
	    checksum += x[j];
	    j += stepsize;
	}
	if (checksum != xilinxcode->checksum) {
	    printf ("otto_loadxilinx: checksum failed\n");
	    x = (unsigned char *) 0;
	    return (-1);
	}
    }

    if (x == (unsigned char *) 0) {
	printf ("otto: ROM xilinxcode not there yet\n");
	return (-1);
    }

    for (retry = 0; retry != 5; retry++) {
	ENTER_KERNEL_CRITICAL_SECTION();
	HIGH_IPL();
	OTTO_PIOW (&ohw->csr->resets, od,
			OTTO_CSR_RSTALTERA | OTTO_CSR_RSTSUNI);
	wbflush ();

	microdelay (100);

	OTTO_PIOW (&ohw->csr->resets, ohw,
		   OTTO_CSR_RSTXILINX | OTTO_CSR_RSTALTERA |
		   OTTO_CSR_RSTSUNI | OTTO_CSR_PROG);
	wbflush ();

	i = 1000;
	while ((OTTO_PIOR (&ohw->csr->resets, ohw) & OTTO_CSR_INIT) == 0 && 
	       i != 0) {
	    i--;
	}
	if (i == 0) {
	    printf (
		     "otto_loadxilinx: failed: INIT low too long\n");
	    microdelay (100);
	    continue;
	}

	microdelay (100);

	OTTO_PIOW (&ohw->csr->resets, ohw,
		   OTTO_CSR_RSTALTERA | OTTO_CSR_RSTSUNI);
	
	i = 1000;
	while ((OTTO_PIOR (&ohw->csr->resets, ohw) & OTTO_CSR_INIT) != 0 && 
	       i != 0) {
	    i--;
	}
	if (i == 0) {
	    printf ("otto_loadxilinx: failed: INIT high too long\n");
	    microdelay (100);
	    continue;
	}

	microdelay (100);

	for (i = 0; i != len; i++) {
	    int byte = x[i];
	    
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); byte >>= 1; wbflush ();
	    OTTO_PIOW (&ohw->csr->loadxilinx, ohw, byte); wbflush ();
	}

	if ((OTTO_PIOR (&ohw->csr->resets, ohw) & OTTO_CSR_NOT_DONE) != 0) {
	    printf ("otto_loadxilinx: programming not complete\n");
	    microdelay (100);
	    continue;
	}
	LOW_IPL();
	LEAVE_KERNEL_CRITICAL_SECTION();
	/* success */
	return (0);
    }

    printf ("otto_loadxilinx: failed: too many attempts\n");
    return (-1);
}

void otto_uselink (struct otto_hw *ohw, int link)
{
    if (link == 0) {
	OTTO_PIOW (ohw->cell, ohw, OTTO_CELL_USERIGHT);
    } else {
	OTTO_PIOW (ohw->cell, ohw, OTTO_CELL_USELEFT);
    }
    wbflush ();
    
    otto_wait_for_phaselock (ohw);
}

/* make sure we can write random values to the ram */
/* leave 0 in all the locations */
int otto_sramtest (struct otto_hw *ohw)
{
    int i;
    int n = ohw->ramsize / sizeof (otto_uword32);
    volatile otto_uword32 *ram = ohw->ram;
    otto_uword32 prime1 = 32771;
    otto_uword32 prime2 = 2147483659U;
    otto_uword32 invert;

    ENTER_BROKEN_INTEL_SECTION();
    for (invert = 0; invert != 2; invert++) {
	otto_uword32 t = 0;
	unsigned addr = 0;
	for (i = 0; i != n; i++) {
	    OTTO_PIOW (&ram[addr], ohw, t ^ (-invert));
	    addr += prime1;
	    t += prime2;
	    addr %= n;
	}
	t = 0;
	addr = 0;
	for (i = 0; i != n; i++) {
	    if (OTTO_PIOR (&ram[addr], ohw) != (t ^ (-invert))) {
		printf ("otto: RAM location %x failed: wrote %x read %x\n",
			addr, t ^ (-invert), OTTO_PIOR (&ram[addr], ohw));
		return (-1);
	    }
	    addr += prime1;
	    t += prime2;
	    addr %= n;
	}
    }
    
    /* zero the RAM */
    for (i = 0; i != n; i++) {
	OTTO_PIOW (&ram[i], ohw, 0);
    }
    LEAVE_BROKEN_INTEL_SECTION();
    return (0);
}

/* tell the hardware where the report ring is */
/* set tag bits in ring to make them all invalid */
void otto_setreportbase (struct otto_hw *ohw)
{
    int isoppo = ohw->isoppo;
    unsigned long phys = (unsigned long)OTTO_SVTOPHY ((char *) ohw->reportbase);
    otto_uword32 addr = OTTO_PHYS_TO_RPTBASE (phys);
    int i;
    TRC(printf("********** REPORTBASE phys = %08x\n", phys));
    TRC(printf("********** REPORTBASE addr = %08x\n", addr));

    for (i = 0; i != OTTO_REP_RING_LEN_REPS; i++) {
	ohw->reportbase[i].dmareport = OTTO_REP_DMA_TAG;
    }
    ohw->nextreport = 0;
    ohw->reporttag = OTTO_REP_DMA_NEGTAG;
    
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->dma->report, ohw, addr);
    LEAVE_BROKEN_INTEL_SECTION();
    wbflush ();
}

/* build the cell free lists */
void otto_buildcelllists (struct otto_hw *ohw,
			  unsigned txcells, unsigned noackcells)
{
    unsigned rxcells;		/* rxcells = # of rx cells */
    unsigned i;
    unsigned freestart;
    volatile otto_uword32 *cells = &ohw->ram[OTTO_CELLSPOS];
    
    /* make txcells be a multiple of the VRAM row size */
    i = txcells % OTTO_CELLS_PER_ROW;
    if (i != 0) {
	txcells += OTTO_CELLS_PER_ROW - i;
    }
    
    rxcells = ohw->ncellbuffers - txcells - OTTO_UNUSABLE_CELLS;
    
    ohw->rxcells = rxcells;
    ohw->txcells = txcells;
    ohw->noackcells = noackcells;
    
    if (ohw->ncellbuffers <= txcells+OTTO_PANIC_CELLS+OTTO_UNUSABLE_CELLS) {
	panic("otto_buildcelllists: too many transmit calls requested");
    }
    
    if (noackcells + OTTO_PANIC_CELLS > rxcells) {
	panic ("otto_buildcelllists: too many receive cells requested");
    }
    
    
    /* receive cells will be in RAM addresses
       OTTO_UNUSABLE_CELLS..rxcells+OTTO_UNUSABLE_CELLS-1
       transmit cells will be in RAM addresses
       rxcells+OTTO_UNUSABLE_CELLS..ohw->ncellbuffers */
     
     /* give first two receive cells to hardware directly */
    ENTER_BROKEN_INTEL_SECTION();
    i = OTTO_UNUSABLE_CELLS;
    OTTO_PIOW (&ohw->ram[0], ohw, i);
    i++;
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_cell_wr_rxbuf);
    wbflush ();
    
    OTTO_PIOW (&ohw->ram[0], ohw, i);
    i++;
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_cell_wr_rxbuf);
    wbflush ();

    freestart = i;
    /* make receive cell list */
    while (i != rxcells+OTTO_UNUSABLE_CELLS) {
	OTTO_PIOW (&cells[i], ohw, OTTO_MAKE_FREE_RXCELL (i+1));
	i++;
    }
    OTTO_PIOW (&cells[rxcells+OTTO_UNUSABLE_CELLS-1], ohw,
		OTTO_MAKE_TAIL_FREE_RXCELL);


    /* write list pointers to hardware */
    OTTO_PIOW (&ohw->ram[0], ohw, rxcells+OTTO_UNUSABLE_CELLS-1);
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_rxbuftail);
    wbflush ();
    
    OTTO_PIOW (&ohw->ram[0], ohw, freestart);
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_rxbufhead);
    wbflush ();
    
    OTTO_PIOW (&ohw->ram[0], ohw, 0);
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_rxbufpdma);
    wbflush ();
    
    OTTO_PIOW (&ohw->ram[0], ohw,
	       ((rxcells-OTTO_PANIC_CELLS) << 16) | (rxcells - noackcells));
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_rxbufctr);
    wbflush ();
    OTTO_PIOW (&ohw->ram[0], ohw, 0); /*cooked, VC 0 for OAM */
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_recvmode);
    wbflush ();
    
    /* make transmit cells */
    for (i = rxcells+OTTO_UNUSABLE_CELLS; i != ohw->ncellbuffers; i++) {
	OTTO_PIOW (&cells[i], ohw, OTTO_MAKE_FREE_TXCELL (i+1));
    }
    OTTO_PIOW (&cells[ohw->ncellbuffers-1], ohw, OTTO_MAKE_TAIL_FREE_TXCELL);
    
    OTTO_PIOW (&ohw->ram[0], ohw, rxcells+OTTO_UNUSABLE_CELLS);
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_txbufhead);
    wbflush ();
    OTTO_PIOW (&ohw->ram[0], ohw, ohw->ncellbuffers-1);
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_txbuftail);
    wbflush ();
    OTTO_PIOW (&ohw->ram[0], ohw, txcells-3); /* XXX perhaps should be -1 */
    wbflush ();
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_load_txbufctr);
    wbflush ();
    
    OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_read_rxlowbuf); 
    /* Initialize low water mark. */
    wbflush ();
    LEAVE_BROKEN_INTEL_SECTION();
}

/* initialize the SUNI chip */
void otto_initsuni (struct otto_hw *ohw)
{

    TRC(printf("oppo: initsuni\n"));
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->suni[8*SUNI_MRI], ohw, 0);
    /* take SUNI out of reset */
    OTTO_PIOW (&ohw->suni[8*SUNI_RACPMHM], ohw, 0xff); 
    /* Take cells if hdr ~0 */
    if (ohw->otto_sdh) {
	/* Enable SDH */
	OTTO_PIOW (&ohw->suni[8*SUNI_TPOPAP1], ohw, 0x98);
    } else {
	/* Enable SONET */
	OTTO_PIOW (&ohw->suni[8*SUNI_TPOPAP1], ohw, 0x90);
    }
    OTTO_PIOW (&ohw->suni[8*SUNI_RACPCS], ohw,
	       SUNI_RACPCS_DISCOR|SUNI_RACPCS_HCSADD);
    LEAVE_BROKEN_INTEL_SECTION();
    /* no header error correction*/
    wbflush();
}

/* Put suni chip into reset state */
void otto_resetsuni (struct otto_hw *ohw)
{
    OTTO_PIOW (&ohw->suni[8*SUNI_MRI], ohw, 0x80);
}

/* allocate a ring */
unsigned otto_allocring (struct otto_hw *ohw)
{
    unsigned x = ohw->ringfreelist;
    if (x != 0) {
	ohw->ringfreelist = ohw->rings[x].next;
    }
    return (x);
}

/* free a ring */
void otto_freering (struct otto_hw *ohw, unsigned index)
{
    ohw->rings[index].next = ohw->ringfreelist;
    ohw->ringfreelist = index;
}

/* allocate a receive packet */
unsigned otto_allocrxpkt (struct otto_hw *ohw)
{
    unsigned x = ohw->rxpktfreelist;
    if (x != 0) {
	struct rbuf *r = ohw->rxpkts[x].r;
	if (!r) r = RALLOC();
	if (r) {
	    ohw->rxpkts[x].r = r;
	    ohw->rxpktfreelist = ohw->rxpkts[x].next;
	} else {
	    x = 0;
	}
    }
    return (x);
}

/* free a receive packet */
void otto_freerxpkt (struct otto_hw *ohw, unsigned index)
{
    ohw->rxpkts[index].next = ohw->rxpktfreelist;
    /* XXX  ohw->rxpkts[index].r = (struct rbuf *) 0; */
    ohw->rxpktfreelist = index;
}

/* allocate a transmit packet */
unsigned otto_alloctxpkt (struct otto_hw *ohw)
{
    unsigned x = ohw->txpktfreelist;
    if (x != 0) {
	struct rbuf *r = ohw->txpkts[x].r;
	if (!r) r = RALLOC();
	if (r) {
	    ohw->txpkts[x].r = r;
	    ohw->txpktfreelist = ohw->txpkts[x].next;
	} else {
	    x = 0;
	}
    }
    return (x);
}

/* free a transmit packet */
void otto_freetxpkt (struct otto_hw *ohw, unsigned index)
{
    ohw->txpkts[index].next = ohw->txpktfreelist;
    /* XXX  ohw->txpkts[index].r = (struct rbuf *) 0; */
    ohw->txpktfreelist = index;
}

/* Set the ign, pkt, ack and ring values in a receive VC's structure.
   For each of ign, pkt, ack and ring, the value -1 indicates no change. */
void otto_setrxvc (struct otto_hw *ohw, unsigned vc,
		   int ign, int pkt, int ack, int ring)
{
    volatile otto_uword32 *rxvc_ring = ohw->ram_writelo + 
	OTTO_VCPOS+(1*OTTO_MAXVC);
    volatile otto_uword32 *rxvc_tail = ohw->ram_writelo + 
	OTTO_VCPOS+(2*OTTO_MAXVC);
    otto_uword32 x;
    volatile otto_uword32 *rxvc_head = ohw->ram_writehi + 
	OTTO_VCPOS+(2*OTTO_MAXVC);
    
    if (ring != -1) {
	ENTER_BROKEN_INTEL_SECTION();
	x = (OTTO_MASK_RXVCRING << 16) | OTTO_MAKE_RXVCRING (ring);
	OTTO_PIOW (&rxvc_ring[vc], ohw, x);
	LEAVE_BROKEN_INTEL_SECTION();
    }
    if (pkt != -1 || ack != -1) {
	x = 0;
	
	if (pkt != -1) {
	    x |= (OTTO_RXVC_PKT << 16) | (pkt? OTTO_RXVC_PKT: 0);
	}
	if (ack != -1) {
	    x |= (OTTO_RXVC_ACK << 16) | (ack? OTTO_RXVC_ACK: 0);
	}
	ENTER_BROKEN_INTEL_SECTION();
	OTTO_PIOW (&rxvc_tail[vc], ohw, x);
	LEAVE_BROKEN_INTEL_SECTION();
    }
    if (ign != -1) {
	x = (OTTO_RXVC_IGNH << 16) | (ign? OTTO_RXVC_IGNH: 0);
	ENTER_BROKEN_INTEL_SECTION();
	OTTO_PIOW (&rxvc_head[vc], ohw, x);
	LEAVE_BROKEN_INTEL_SECTION();
    }
}

void otto_printvc (struct otto_hw *ohw, unsigned vc)
{
    volatile otto_uword32 *txvc_tail = ohw->ram + 
	OTTO_VCPOS+(0*OTTO_MAXVC);
    volatile otto_uword32 *txvc_cred = ohw->ram + 
	OTTO_VCPOS+(1*OTTO_MAXVC);
    volatile otto_uword32 *rxvc_tail = ohw->ram + 
	OTTO_VCPOS+(2*OTTO_MAXVC);
#if 0
    printf ("vcget: vc%d %x %x %x\n", vc, 
	     OTTO_PIOR (&txvc_cred[vc], ohw),
	     OTTO_PIOR (&txvc_tail[vc], ohw),
	     OTTO_PIOR (&rxvc_tail[vc], ohw));
#endif
}

/* Set the usesched, enflowcntl, overflow and credits values in a transmit
   VC's structure.
   For each of usesched, enflowcntl, overflow and credits, the value -1
   indicates no change. */
void otto_settxvc (struct otto_hw *ohw, unsigned vc,
		   int usesched, int enflowcntl, int overflow, int credits)
{
    volatile otto_uword32 *txvc_cred = ohw->ram_writehi + 
	OTTO_VCPOS+(1*OTTO_MAXVC);
    volatile otto_uword32 *txvc_tail = ohw->ram_writelo + 
	OTTO_VCPOS+(0*OTTO_MAXVC);
    otto_uword32 x;

    if (enflowcntl != -1 || overflow != -1 || credits != -1) {
	x = 0;
	
	if (enflowcntl != -1) {
#if 1
	    x |= (OTTO_TXVC_ENFLOWCNTL << 16) |
		(enflowcntl? OTTO_TXVC_ENFLOWCNTL: 0);
#else
#endif
	    
	}
	if (overflow != -1) {
	    x |= (OTTO_TXVC_OVERFLOW << 16) |
	(overflow? OTTO_TXVC_OVERFLOW: 0);
	}
	if (credits != -1) {
	    x |= (OTTO_MASK_TXVCCREDITS << 16) |
		OTTO_MAKE_TXVCCREDITS (credits);
	    x |= (OTTO_TXVC_NOCREDIT << 16) | 
		((credits & 0x1000)? OTTO_TXVC_NOCREDIT: 0);
	}
	ENTER_BROKEN_INTEL_SECTION();
	OTTO_PIOW (&txvc_cred[vc], ohw, x);
	LEAVE_BROKEN_INTEL_SECTION();
    }
    if (usesched != -1) {
	x = (OTTO_TXVC_USESCHED << 16) |
	    (usesched? OTTO_TXVC_USESCHED: 0);
	ENTER_BROKEN_INTEL_SECTION();
	OTTO_PIOW (&txvc_tail[vc], ohw, x);
	LEAVE_BROKEN_INTEL_SECTION();
    }
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->activate[vc], ohw, OTTO_ACTIVATE_ONLY);
    LEAVE_BROKEN_INTEL_SECTION();
}

/* Sets schedule slot index to indicate the given vc.
   A vc of zero means no vc should use this slot. */
void otto_setsched (struct otto_hw *ohw, unsigned index, unsigned vc)
{
    volatile otto_uword32 *sched = ohw->ram_writehi + OTTO_SCHEDPOS;

    ENTER_BROKEN_INTEL_SECTION();
    if (vc == OTTO_MAXVC) {
	OTTO_PIOW (&sched[index], ohw, (0xffff0000 | 0));
    } else {
	OTTO_PIOW (&sched[index], ohw, (0xffff0000| OTTO_MAKE_SCHED (vc)));
    }
    LEAVE_BROKEN_INTEL_SECTION();
}

/* waits a while for the SUNI and PLL to acquire lock */
/* It's unclear what happens if we continue operating the device 
   when lock isn't acquired.   However, since we may need to call this
  from an interrupt routine, it waits only a limited time. */
void otto_wait_for_phaselock (struct otto_hw *ohw)
{
    int i;
    int bounce;
    otto_uword32 racp;
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->suni[8*SUNI_RACPCS], ohw, 
	       SUNI_RACPCS_FIFORST|SUNI_RACPCS_HCSADD|SUNI_RACPCS_DISCOR);
    /* reset recv FIFO */
    wbflush ();
					 /* Take cells if hdr ~0 */
    OTTO_PIOW (&ohw->suni[8*SUNI_RACPMHM], ohw, 0xff);
    wbflush ();
    
    i = 0;
    do {			/* Wait for cell sync */
	racp = OTTO_PIOR (&ohw->suni[8*0x50], ohw) & 0xff;
	i++;
    } while ((racp & 0x80) != 0 && i != 1000000);
    
    bounce = 0;
    for (i = 0; i != 5000; i++) {       /* Wait for bouncing to calm down */
	racp = OTTO_PIOR (&ohw->suni[8*SUNI_RACPCS], ohw) & 0xff;
	if ((racp & 0x80) != 0) {
	    bounce++;
	}
    }
    
    /* Gobble up RACP.OOCDI bit */
    racp = OTTO_PIOR (&ohw->suni[8*SUNI_RACPIES], ohw)& 0xff;
    OTTO_PIOW (&ohw->suni[8*SUNI_RACPCHCS], ohw, 0); /* transfer HCS counters */
    OTTO_PIOW (&ohw->suni[8*SUNI_RACPCS], ohw,
	       SUNI_RACPCS_HCSADD|SUNI_RACPCS_DISCOR);
    /* unreset FIFO */
    wbflush ();
    LEAVE_BROKEN_INTEL_SECTION();
}


#define TABLE_LEN  (OTTO_SCHEDSIZE / 2)

/* install a new schedule in the hardware from the softc */
void otto_newsched (struct otto_hw *ohw)
{
    int curr_table;
    int start_index;
    int end_index;
    int i;
    OTTO_LOCKTMP s0;
    OTTO_LOCKTMP s1;
    
    OTTO_LOCK (&ohw->schedlock, s1);

    /* find curr_table (0 or 1) */
    for (;;) {
	OTTO_LOCK (&ohw->execlock, s0);
	ENTER_BROKEN_INTEL_SECTION();
	OTTO_PIOW (ohw->exec, ohw, OTTOEXEC_sched_read);
	wbflush ();
	curr_table = (OTTO_PIOR (&ohw->ram[0],ohw) >> 12) & 1;
	LEAVE_BROKEN_INTEL_SECTION();
	wbflush ();
	OTTO_UNLOCK (&ohw->execlock, s0);
	if (curr_table == ohw->schedhalf) {
	    break;
	}
	OTTO_UNLOCK (&ohw->schedlock, s1);
	/*		sleep (&lbolt, PZERO + 1); */
	OTTO_LOCK (&ohw->schedlock, s1);
    }
    
    ohw->schedhalf = 1 - curr_table;
    start_index = (ohw->schedhalf * TABLE_LEN) + TABLE_LEN - ohw->schedlen;
    end_index = (ohw->schedhalf * TABLE_LEN) + TABLE_LEN;
    
    for (i = start_index; i < end_index; i++) {
	otto_setsched (ohw, i, ohw->sched[i - start_index]);
    }
    
    ENTER_BROKEN_INTEL_SECTION();
    OTTO_PIOW (&ohw->ram[OTTO_SCHEDULESTART], ohw, start_index);
    LEAVE_BROKEN_INTEL_SECTION();
    OTTO_UNLOCK (&ohw->schedlock, s1);
}


/* Cause "vc"  to have "bw" slots allocated in the schedule.
   Called with softc lock held. */
int otto_update_bandwidth (struct otto_hw *ohw, unsigned vc, int bw)
{
    struct otto_vcdata *vcdata = &ohw->vcs[vc];
    int change = 0;
    
    if (bw != vcdata->bandwidth) {
	if (bw >= 0 && bw <= vcdata->bandwidth + ohw->schedfree) {
	    unsigned i;
	    unsigned x = ohw->schedlen;
	    unsigned offset = (ohw->schedhalf * TABLE_LEN) +
		TABLE_LEN - ohw->schedlen;
	    
	    change = 1;
	    if (vcdata->bandwidth != 0) {
		ohw->schedfree += vcdata->bandwidth;
		for (i = 0; i != x; i++) {
		    if (ohw->sched[i] == vc) {
			ohw->sched[i] = 0;
			otto_setsched (ohw, i+offset, OTTO_MAXVC);
		    }
		}
	    }
	    ohw->schedfree -= bw;
	    vcdata->bandwidth = bw;
	    if (bw != 0) {
		unsigned every = x / bw;
		while (bw != 0 && every != 0) {
		    i = 0;
		    while (i < x && bw != 0) {
			if (ohw->sched[i] == 0) {
			    ohw->sched[i] = vc;
			    otto_setsched (ohw, i+offset, vc);
			    i += every;
			    bw--;
			} else {
			    i++;
			}
		    }
		    every >>= 1;
		}
		if (bw != 0) {
		    panic ("otto: unable to set schedule");
		}
	    }
	} else {
	    change = -1;
	}
    }
    return (change);
}

/* return non-zero iff the SUNI is receiving cells and the far end can
   receive our cells (i.e. it's not asserting far-end-receive failure)
   and this state has been true since the last call to this procedure. */
int otto_gettingcells (struct otto_hw *ohw)
{
    unsigned mis;
    unsigned racpcs;
    unsigned racpies;
    unsigned mc;
    unsigned rsopsis;
    unsigned rlopies;
    unsigned rlopcs;
    unsigned rpopsc;
    unsigned rpopis;
    unsigned tacpcs;

    ENTER_BROKEN_INTEL_SECTION();
    /* prepare to read error counts --- see code after "get error counts"*/
    OTTO_PIOW (&ohw->suni[8*SUNI_LM], ohw, 0);
    wbflush ();	/* force out the write to SUNI_LM */

    mis = OTTO_PIOR (&ohw->suni[8*SUNI_MIS], ohw);
    racpcs = OTTO_PIOR (&ohw->suni[8*SUNI_RACPCS], ohw);
    racpies = OTTO_PIOR (&ohw->suni[8*SUNI_RACPIES], ohw);
    mc = OTTO_PIOR (&ohw->suni[8*SUNI_MC], ohw);
    rsopsis = OTTO_PIOR (&ohw->suni[8*SUNI_RSOPSIS], ohw) & 0x7f;
    rlopies = OTTO_PIOR (&ohw->suni[8*SUNI_RLOPIES], ohw);
    rlopcs = OTTO_PIOR (&ohw->suni[8*SUNI_RLOPCS], ohw);
    rpopsc = OTTO_PIOR (&ohw->suni[8*SUNI_RPOPSC], ohw);
    rpopis = OTTO_PIOR (&ohw->suni[8*SUNI_RPOPIS], ohw) & 0xaf;
    tacpcs = OTTO_PIOR (&ohw->suni[8*SUNI_TACPCS], ohw);
    LEAVE_BROKEN_INTEL_SECTION();

    if ((mis & (SUNI_MIS_PFERFI|SUNI_MIS_GPINI|SUNI_MIS_GPINV)) != 0 ||
	(mc & SUNI_MC_PFERFV) != 0 ||
	rsopsis != 0 ||
	(racpcs & SUNI_RACPCS_OOCDV) != 0 ||
	(racpies & (SUNI_RACPIES_OOCDI|SUNI_RACPIES_CHCSI|SUNI_RACPIES_UHCSI|
		    SUNI_RACPIES_FOVRI|SUNI_RACPIES_FUDRI)) != 0 ||
	(rlopies & (SUNI_RLOPIES_LAISI|SUNI_RLOPIES_LAISI|
		    SUNI_RLOPIES_BIPEI|SUNI_RLOPIES_FEBEI)) != 0 ||
	(rlopcs & (SUNI_RLOPCS_FERFV|SUNI_RLOPCS_LAISV)) != 0 ||
	(rpopsc & (SUNI_RPOPSC_PYEL|SUNI_RPOPSC_PAIS|SUNI_RPOPSC_LOP)) != 0 ||
	rpopis != 0 ||
	(tacpcs & (SUNI_TACPCS_FOVRI|SUNI_TACPCS_TSOCI)) != 0) {
	
	return (0);
    }
    return (1);
}

/* set LED on link "link" (0 or 1) to:
   on if "state" is 1
   the opposite of its current state if "state" is 2
   off otherwise
   */
void otto_setled (struct otto_hw *ohw, unsigned link, unsigned state)
{
    otto_uword32 x;
    otto_uword32 led = link? OTTO_CSR_LEFTLED: OTTO_CSR_RIGHTLED;
    ENTER_BROKEN_INTEL_SECTION();
    x = OTTO_PIOR (&ohw->csr->resets, ohw);
    if (state == 2) {
	x ^= led;
    } else if (state == 1) {
	x |= led;
    } else {
	x &= ~led;
    }
    OTTO_PIOW (&ohw->csr->leds, ohw, x);
    LEAVE_BROKEN_INTEL_SECTION();
    wbflush ();
}

