/* Nemesis 3c509 driver program */

/* This driver is based on code from Linux. However, very little of
 * the original structure remains.
 */

/* $Id: 3c509.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $ */

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG_RX
#define TRCRX(_x) _x
#else
#define TRCRX(_x)
#endif

#ifdef DEBUG_TX
#define TRCTX(_x) _x
#else
#define TRCTX(_x)
#endif

#include <nemesis.h>
#include <stdio.h>
#include <mutex.h>
#include <io.h>
#include <time.h>
#include <irq.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <VPMacros.h>

#include "3c509_st.h"
#include "3c509.h"
#include <kernel.h>
#include <IOMacros.h>
#include <SimpleIO.ih>
#include <Interrupt.ih>
#include <Threads.ih>
#include <NetifMod.ih>

/* Closure things */
static const Netcard_Up_fn Up_m;
static const Netcard_Down_fn Down_m;
static Netcard_op Netcard_ms = { Up_m, Down_m };

static const SimpleIO_Punt_fn SendPacket_m;
static SimpleIO_op TxSimpleIO_ms = { SendPacket_m };

/* Threads */
static void InterruptThread(addr_t data);

/* Driver constants */
#define EL3_TXMAX 1514
#define EL3_TXMIN 60
#define EL3_RXMAX 1514

#define RATE_10MBIT 10000000

#define MAX_ETHNUM 10

#define QOSCTL_NAME "qosctl"

/* Driver strings */
static const char *port_desc[]={"10baseT","AUI","undefined","BNC"};

/* Utility function */

static INLINE unsigned short byteswap(unsigned short val)
{
    return ((val&0xff)<<8) | ((val&0xff00)>>8);
}

/* Read a word from the EEPROM when in the ISA ID probe state. */
/* XXX this routine provokes a bug in gcc if you use PAUSE() after
 * outb().  The workaround is to expand the macro by hand and use a
 * temporary variable.  */
static unsigned short id_read_eeprom(short id_port, int index)
{
    int bit, word;
    Time_ns wibble;

    /* Issue read command, and pause for at least 162 us. for it to complete.
       Assume extra-fast 16Mhz bus. */
    /* Except we actually pause 300us. *shrug* */
    outb(EEPROM_READ + index, id_port);

    wibble=NOW();
    wibble+=300000;
    Events$AwaitUntil(Pvs(evs), NULL_EVENT, 1, wibble);
/*  PAUSE(162000);  XXX provokes gcc bug */

    word=0;
    for (bit = 15; bit >= 0; bit--)
	word = (word << 1) + (inb(id_port) & 0x01);

    return word;
}

/* Read a word from the EEPROM using the regular EEPROM access register.
   Assume that we are in register window zero.
 */
static unsigned short read_eeprom(short ioaddr, int index)
{
    outw(EEPROM_READ + index, ioaddr + 10);
    /* Pause for at least 162 us. for the read to take place. */
    /* Except we actually pause 300us. *shrug* */
    PAUSE(300000);
    return inw(ioaddr + 12);
}

static int probe(int tag, short *ret_address, short *ret_irq, short *ret_media)
{
#ifndef SRCIT
    short lrs_state = 0xff; /* Mystic PnP runes */
    short i;
    unsigned short ioaddr, irq, if_port, iobase, id_port;
    unsigned short phys_addr[3];

    /* Reset the ISA PnP mechanism */
    outb(0x02, 0x279); /* Mystic: Select PnP config control register */
    outb(0x02, 0xA79); /* Mystic: Return to WaitForKey state */

    /* Mystic: Select an open I/O location at 0x1*0 to do contention select */
    for (id_port=0x100; id_port < 0x200; id_port+=0x10) {
	outb(0x00, id_port);
	outb(0xff, id_port);
	if (inb(id_port) & 0x01) /* XXX MYSTIC */
	    break;
    }
    if (id_port >= 0x200) {  /* XXX orig. comment: GCC optimises test out */
	/* Output: No I/O port available for 3c509 activation */
	return False;
    }

    /* Next check for all ISA bus boards by sending the ID sequence to the
       ID_PORT.  We find cards past the first by setting the 'current_tag'
       on cards as they are found.  Cards with their tag set will not
       respond to subsequent ID sequences. */
    /* XXX need to check for reservation on id_port with Nemesis domain */

    outb(0x00, id_port);
    outb(0x00, id_port); /* Mystic */
    for(i=0; i < 255; i++) {
	outb(lrs_state, id_port);
	/* Extremely mystic PnP stuff */
	lrs_state <<= 1;
	lrs_state = lrs_state & 0x100 ? lrs_state ^ 0xcf : lrs_state;
    }

    if (tag==0)
	outb(0xd0, id_port); /* Clear all boards tag registers */
    else
	outb(0xd8, id_port); /* Kill off already-found boards */

    if (id_read_eeprom(id_port, 7) != TAG_3COM) {
	return False; /* No more boards */
    }
    /* Read in EEPROM data, which does contention-select.
       Only the lowest address board will stay "on-line".
       3Com got the byte order backwards. */
    /* XXX do we want to throw away this info now? */
    for (i = 0; i < 3; i++) {
	phys_addr[i] = byteswap(id_read_eeprom(id_port, i));
    }
    TRC(printf(" + serially read address: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       phys_addr[0]&0xff, phys_addr[0]>>8, phys_addr[1]&0xff,
	       phys_addr[1]>>8, phys_addr[2]&0xff, phys_addr[2]>>8));

    iobase=id_read_eeprom(id_port, 8);
    if_port = iobase >> 14; /* Media type */
    ioaddr = 0x200 + ((iobase & 0x1f) << 4); /* Base address */
    irq = id_read_eeprom(id_port, 9) >> 12; /* IRQ */

    /* Mystic: Set the adaptor tag so that the next card can be found */
    outb(0xd0 + tag + 1, id_port);

    /* Mystic: Activate the adaptor at the EEPROM location */
    outb((ioaddr >> 4) | 0xe0, id_port);

    EL3WINDOW(0);
    if (inw(ioaddr) != TAG_3COM) {
	/* Something went wrong - complain */
	return False;
    }

    /* Disable the interrupt on the card for now */
    outw(0x0f00, ioaddr + WN0_IRQ); /* ??? */

    /* Return the card details */
    *ret_address=ioaddr;
    *ret_irq=irq;
    *ret_media=if_port;
#else /* SRCIT */
    if (1) 
    {
	unsigned short i,j, ioaddr;

	*ret_address = ioaddr = 0x200;

	printf("pcmcia hack %02X %02X\n", 
	       (outb(0x03, 0x03e0), inb(0x03e1)),
	       (outb(0x43, 0x03e0), inb(0x03e1)) );
	outb(0x03, 0x3e0);
	outb(((inb(0x3e1) & 0xf0) | 0x03), 0x3e1);
	outb(0x43, 0x3e0);
	outb(((inb(0x3e1) & 0xf0) | 0x03), 0x3e1);
	printf("pcmcia hack %02X %02X\n", 
	       (outb(0x03, 0x03e0), inb(0x03e1)),
	       (outb(0x43, 0x03e0), inb(0x03e1)) );
	*ret_irq     = 1;

	EL3WINDOW(0);
	i = inw(ioaddr + 0x04);
	j = inw(ioaddr + 0x06);
	printf("i=0x%04X j=0x%04X\n", i,j);
	*ret_media   = (j>>14);
    }
#endif /* SRCIT */

    return True;
}

static Netcard_st *initialise_card(unsigned short address, unsigned short irq,
				   unsigned short media, NOCLOBBER int tag)
{
    int ioaddr=address;
    int i;
    unsigned short hwaddr;
    char buff[32];
    Netcard_cl *cl;
    Netcard_st *st;
    NetifMod_clp netifmod;
    Netif_clp netif;
    Interrupt_clp icl;
    SimpleIO_clp txio;
    ProtectionDomain_ID driver_pdom;

    /* Create closure and state */
    st = Heap$Malloc(Pvs(heap), sizeof(*st) + sizeof(*cl) + sizeof(*txio));
    if (!st)
	printf("3c509: out of mem allocating closure\n");
    cl=(Netcard_cl *)(st+1);
    cl->st=st;
    cl->op=&Netcard_ms;
    txio = (SimpleIO_cl *)(cl + 1);
    txio->st = st;
    txio->op = &TxSimpleIO_ms;

    /* Fill in state as much as possible */
    st->address=st->cfg_address=address;
    st->irq=st->cfg_irq=irq;
    st->media=st->cfg_media=media;
    st->active=False;

    /* Allocate 60 bytes of padding */
    st->padbuf = Heap$Malloc(Pvs(heap), EL3_TXMIN);
    if (!st->padbuf)
	printf("3c509: out of mem allocating pad buffer\n");
    memset(st->padbuf, 0xe5, EL3_TXMIN);

    /* Allocate a receive buffer */
    st->rxbuf = Heap$Malloc(Pvs(heap), EL3_RXMAX);
    if (!st->rxbuf)
	printf("3c509: out of mem allocating rx buffer\n");

    EL3WINDOW(0);
    for (i=0; i<3; i++) {
	hwaddr=byteswap(read_eeprom(ioaddr, i));
	st->cfg_hwaddr.a[i*2]=st->hwaddr.a[i*2]=hwaddr&0xff;
	st->cfg_hwaddr.a[i*2+1]=st->hwaddr.a[i*2+1]=hwaddr>>8;
    }    
    EL3WINDOW(1);

    /* Output MAC address of card */
    TRC(printf(" + address %02x:%02x:%02x:%02x:%02x:%02x\n",
	       st->hwaddr.a[0], st->hwaddr.a[1], st->hwaddr.a[2],
	       st->hwaddr.a[3], st->hwaddr.a[4], st->hwaddr.a[5]));

    /* Initialise mutex */
    MU_INIT(&st->mu);

    /* Event channel for interrupt stub */
    st->stub.dcb   = RO(Pvs(vp));
    st->stub.ep    = Events$AllocChannel (Pvs(evs));
    st->event = EC_NEW();
    Events$Attach(Pvs(evs), st->event, st->stub.ep, Channel_EPType_RX);

    /* Condition for transmitters. If SendPacket is called but the
       FIFO is full, the thread will wait on this condition. */
    SRCThread$InitCond(Pvs(srcth), &st->txready);

    /* Register interrupt stub */
    icl=IDC_OPEN("sys>InterruptOffer", Interrupt_clp);
    if (Interrupt$Allocate(icl, irq, _generic_stub, &st->stub)
	!=Interrupt_AllocCode_Ok)
    {
	printf("3c509: failed to allocate interrupt\n");
	return NULL;
    }

    /* Create top half of driver */
    netifmod=NAME_FIND("modules>NetifMod", NetifMod_clp);
    st->rxio = NetifMod$New(netifmod, cl, txio,
			    1, /* tx queue depth */
			    EL3_TXMAX, RATE_10MBIT,
			    &st->hwaddr,
			    &st->txfree,
			    &netif);

    /* Thread to service interrupts */
    Threads$Fork(Pvs(thds), InterruptThread, st, 0);

    /* Export context */
    sprintf(buff,"dev>3c509-%d", tag);
    st->cx=CX_NEW(buff);
    TRC(printf(" + created context %s\n",buff));

    /* Export Ethernet address */
    TRC(printf(" + exporting hardware address\n"));
    CX_ADD_IN_CX(st->cx, "addr", &st->hwaddr, Net_EtherAddrP);

    TRC(printf(" + exporting protection domain\n"));
    driver_pdom = VP$ProtDomID(Pvs(vp));
    CX_ADD_IN_CX(st->cx, "pdom", driver_pdom, ProtectionDomain_ID);

    /* Export Netif closure */
    TRC(printf(" + exporting control interface\n"));
    IDC_EXPORT_IN_CX(st->cx, "control", Netif_clp, netif);

    /* If a qosctl interface was created by qosentry (via netif),
     * then export that too */
    {
	Type_Any any;
	if (Context$Get(Pvs(root), QOSCTL_NAME, &any))
	{
	    Context$Add(st->cx, QOSCTL_NAME, &any);
	    Context$Remove(Pvs(root), QOSCTL_NAME);
	}
    }
	    

    TRC(printf("eth%d is a 3c509\n", tag));
    
    return st;
}

/* XXX This function must be called with the mutex LOCKED */
static void update_statistics(Netcard_st *st)
{
#if 0
    int ioaddr=st->address;

    /* Turn off statistics updates while reading */
    outw(StatsDisable, ioaddr + EL3_CMD);
    /* Switch to the stats window and read everything */
    EL3WINDOW(6);
    st->stats.tx_carrier_errors += inb(ioaddr + 0);
    st->stats.tx_heartbeat_errors += inb(ioaddr + 1);
    st->stats.multiple_collisions += inb(ioaddr + 2);
    st->stats.collisions += inb(ioaddr + 3);
    st->stats.tx_window_errors += inb(ioaddr + 4);
    st->stats.rx_fifo_errors += inb(ioaddr + 5);
    st->stats.tx_packets += inb(ioaddr + 6);
    st->stats.rx_packets += inb(ioaddr + 7); /* XXX do we update this
						somewhere else
						instead?  Be careful
						not to count packets
						twice. */
    st->stats.tx_deferrals += inb(ioaddr + 8);
    st->stats.rx_octets += inw(ioaddr + 10);
    st->stats.tx_octets += inw(ioaddr + 12);

    EL3WINDOW(1);
    outw(StatsEnable, ioaddr + EL3_CMD);
#endif /* 0 */
    return;
}

static void ReceivePacket(Netcard_st *st)
{
    short ioaddr=st->address;
    short rx_status, pkt_len;
    IO_Rec rxrec;

    TRCRX(printf("ReceivePacket: entered\n"));

    rx_status = inw(ioaddr + RX_STATUS) & 0x7fff;
    TRCRX(printf("status: %x\n",rx_status));

    /* There's a packet. Deal with it. */
    /* Mystic constant coming up... */
    if (rx_status & 0x4000) /* Error, update stats */
    {
	/* We don't keep stats at the moment */
	short error = rx_status & 0x3800; /* More mysticism */
	TRCRX(printf("3c509: rx error? (status=%x)\n", rx_status));
#if 0
	st->stats.rx_errors++;
	/* XXX if this is done here, should we avoid updating these
	   in update_statistics()? */
	switch (error)
	{
	case 0x0000: st->stats.rx_over_errors++; break;
	case 0x0800: st->stats.rx_length_errors++; break;
	case 0x1000: st->stats.rx_frame_errors++; break;
	case 0x1800: st->stats.rx_length_errors++; break;
	case 0x2000: st->stats.rx_frame_errors++; break;
	case 0x2800: st->stats.rx_crc_errors++; break;
	}
#endif /* 0 */
	switch (error)
	{
	case 0x0000: 
	    printf("3c509: RX overrun\n");
	    break;
	case 0x0800:
	case 0x1800:
	    printf("3c509: RX length error\n");
	    break;
	case 0x1000:
	case 0x2000:
	    printf("3c509: RX frame error\n");
	    break;
	case 0x2800:
	    printf("3c509: RX CRC error\n");
	    break;
	}
    }
    else
    {
	TRCRX(printf("Got packet\n"));
	/* Congratulations - it's a packet */
	pkt_len = rx_status & 0x7ff;

	if (pkt_len <= EL3_RXMAX)
	{
	    insl(ioaddr+RX_FIFO, st->rxbuf, (pkt_len+3)>>2);
	    rxrec.len  = pkt_len;
	    rxrec.base = st->rxbuf;
	    /* Run packet filter over it, and deliver it to the
	     * correct domain. */
	    SimpleIO$Punt(st->rxio, 1, &rxrec, 0, 0);
	    /* now safe to re-use rxbuf */
#if 0
	    st->stats.rx_packets++;
	    printf("T: %02x:%02x:%02x:%02x:%02x:%02x\n"
		    "F: %02x:%02x:%02x:%02x:%02x:%02x\n",
		    packet[0],packet[1],packet[2],
		    packet[3],packet[4],packet[5],
		    packet[6],packet[7],packet[8],
		    packet[9],packet[10],packet[11]);
#endif /* 0 */
	}
	else
	{
	    printf("3c509: rx'ed overlong packet (%d > %d)\n",
		    pkt_len, EL3_RXMAX);
	}
    }

    /* We discard the packet no matter what */
    outw(RxDiscard, ioaddr + EL3_CMD);

    TRCRX(printf("ReceivePacket: returning\n"));
    return;
}

static void
InterruptThread(addr_t data)
{
    Netcard_st *st=data;
    short ioaddr=st->address;
    int status;
    int i;
    bool_t receive_active=False, busy;

    st->ack = 1;

    while(1)  {
	ENTER_KERNEL_CRITICAL_SECTION();
	ntsc_unmask_irq(st->irq);
	LEAVE_KERNEL_CRITICAL_SECTION();

	/* Wait for next interrupt */
	st->ack = EC_AWAIT(st->event, st->ack);
	st->ack++;

	i=0;
	/* XXX There must be a corresponding release */
	MU_LOCK(&st->mu);
	busy=True;
	while ((status=inw(ioaddr + EL3_STATUS)) &
	       (IntLatch | RxComplete | StatsFull) && busy) {
	    busy=False;
	    if ((status & RxComplete) && !receive_active)
	    {
		TRCRX(printf("3c509: Int: receive\n"));
		receive_active=True;
		busy=True;
		ReceivePacket(st);
	    }
		
	    if (status & TxAvailable)
	    {
		busy=True;
		/* There's room in the FIFO for a full-sized packet;
		   send an event to the transmitter thread */
		SRCThread$Signal(Pvs(srcth), &st->txready);
		outw(AckIntr | TxAvailable, ioaddr + EL3_CMD);
	    }

	    /* XXX next two if's are a bit bogus! and1000 */
	    if (status & (AdapterFailure | RxEarly | StatsFull))
	    {
		busy=True;
		/* Uncommon interrupts */
		if (status & StatsFull)
		    update_statistics(st);
		if (status & RxEarly)
		    ReceivePacket(st);
		outw(AckIntr | RxEarly, ioaddr + EL3_CMD);
	    }
	    if (status & AdapterFailure)
	    {
		busy=True;
		/* Adapter failure requires Rx reset and reinit */
		outw(RxReset, ioaddr + EL3_CMD);
		/* Set the filter up */
		outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
		outw(RxEnable, ioaddr + EL3_CMD);
		outw(AckIntr | AdapterFailure, ioaddr + EL3_CMD);
	    }
	    
	    if (++i>=10)
	    {
		printf("Infinite loop in interrupt thread, status=%x\n",
			status);
		outw(AckIntr | 0x7ff, ioaddr + EL3_CMD);
		continue;
	    }
	    /* Acknowledge the IRQ */
	    outw(AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD);
	}
	busy=False;
	receive_active=False;
	MU_RELEASE(&st->mu);
    }
}


static void
SendPacket_m(SimpleIO_cl *self, uint32_t nrecs, IO_Rec *recs, word_t value,
	     addr_t owner)
{
    Netcard_st *st=self->st;
    uint32_t length;
    int padlen;    /* used to need to be signed (still true?) */
    uint8_t *overptr;
    
    short ioaddr=st->address;
    int free, i;

    /* Find length of packet */
    length=0;
    for (i=0; i<nrecs; i++)
	length += recs[i].len;

    if (length < EL3_TXMIN)
	padlen = EL3_TXMIN - length;
    else
	padlen = 0;

    /* is the packet oversize? */
    if (length > EL3_TXMAX)
    {
	printf("3c509: frame length %d too long (> %d)\n", length, EL3_TXMAX);
	/* send the packet back to Netif to recycle the buffers */
	SimpleIO$Punt(st->txfree, nrecs, recs, value, owner);
	return;
    }

    TRCTX(printf("tx for %d bytes (%d frags), pad=%d\n",
		   length, nrecs, padlen));

    /* Wait until there's some free space in the TX fifo */
    SRCThread$Lock(Pvs(srcth), &st->mu);
    do {
	free = inw(ioaddr + TX_FREE);
	if (free < length + padlen)
	{
	    /* interrupt when fifo has enough space */
	    outw(SetTxThreshold + length + padlen + 36, ioaddr + EL3_CMD);
	    TRCTX(printf("3c509: waiting for fifo space\n"));
	    if (!SRCThread$WaitUntil(Pvs(srcth), &st->mu, &st->txready,
				     NOW() + SECONDS(1)))
	    {
		printf("3c509: timed out waiting for fifo space\n");
	    }
	}
    } while (free < length + padlen);
    
    /* Now there should be space in the fifo for the packet */
    /* Output doubleword header */
    TRCTX(printf("TX header: expect %d+%d=%d bytes\n",
		 length, padlen, length+padlen));
    outw(length + padlen, ioaddr + TX_FIFO);
    outw(0x0000, ioaddr + TX_FIFO);

    /* Output packet */
    for (i=0; i<nrecs; i++)
    {
	/* send out as many complete words as possible */
	outsl(ioaddr + TX_FIFO, recs[i].base, (recs[i].len)>>2);
	TRCTX(printf("outsl()ed %d words\n", (recs[i].len)>>2));

	/* If non word-length rec then we havn't transmitted the last
	 * 1 to 3 bytes */
	if (recs[i].len & 3)
	{
	    TRCTX(printf("got %d bytes left over, outb()ing them\n",
			 recs[i].len & 3));
	    overptr = ((char *)recs[i].base) + (recs[i].len & (~3));
	    switch(recs[i].len & 3) {
	    case 3: outb(*overptr++, ioaddr + TX_FIFO);
	    case 2: outb(*overptr++, ioaddr + TX_FIFO);
	    case 1: outb(*overptr++, ioaddr + TX_FIFO);
	    }
	}
    }

    /* Output pad */
    TRCTX(printf("length=%d, padlen=%d\n", length, padlen));
    /* round down, and do whole words */
    if ((padlen>>2) > 0)
	outsl(ioaddr + TX_FIFO, st->padbuf, padlen>>2);
    /* put out single bytes to make up exact pad */
    if (padlen & 3)
    {
	switch(padlen & 3) {
	case 3: outb(0x44, ioaddr + TX_FIFO);
	case 2: outb(0x44, ioaddr + TX_FIFO);
	case 1: outb(0x44, ioaddr + TX_FIFO);
	}
    }

    /* That's what will hit the wire.  Now pad to next whole word to trigger
     * the 3c509 to transmit */
    if ((length + padlen) & 3)
    {
	switch(4 - ((length + padlen) & 3)) {
	case 3: outb(0x45, ioaddr + TX_FIFO);
	case 2: outb(0x45, ioaddr + TX_FIFO);
	case 1: outb(0x45, ioaddr + TX_FIFO);
	}
    }

    /* clear up the TX status stack */
    {
	short tx_status;
	int i = 4;
	
	while (--i > 0	&&	(tx_status = inb(ioaddr + TX_STATUS)) > 0) {
	    if (tx_status & 0x38)
		printf("3c509: tx aborted\n");
	    if (tx_status & 0x30)
	    {
		TRC(printf("3c509: TxReset\n"));
		outw(TxReset, ioaddr + EL3_CMD);
	    }
	    if (tx_status & 0x3C)
	    {
		TRC(printf("3c509: TxEnable\n"));
		outw(TxEnable, ioaddr + EL3_CMD);
	    }
	    outb(0x00, ioaddr + TX_STATUS); /* Pop the status stack. */
	}
    }

    /* Send the packet back to Netif to acknowledge the transmit */
    SimpleIO$Punt(st->txfree, nrecs, recs, value, owner);

    /* We're finished */
    SRCThread$Release(Pvs(srcth), &st->mu);
}

static bool_t Up_m(Netcard_cl *self)
{
    Netcard_st *st=self->st;
    int ioaddr, i;

    if (st->active) return False; /* Already open */

    TRC(printf("Netcard_Up called\n"));

    SRCThread$Lock(Pvs(srcth), &st->mu);
    st->active=True;
    ioaddr=st->address;
	      
    outw(TxReset, ioaddr + EL3_CMD);
    outw(RxReset, ioaddr + EL3_CMD);
    outw(SetStatusEnb | 0x00, ioaddr + EL3_CMD); /* ??? */

    EL3WINDOW(0);
    /* Activate board: probably unnecessary */
    outw(0x0001, ioaddr + 4); /* Mystic runes */

    /* Set the IRQ for the board */
    outw((st->irq<<12) | 0x0f00, ioaddr + WN0_IRQ);
	      
    /* Set the station address */
    EL3WINDOW(2);
    for (i=0; i<6; i++)
	outb(st->hwaddr.a[i], ioaddr+i);
	      
    if (st->media==PORT_BNC) {
	/* Start the thinnet transceiver. We should really wait 50ms... XXX */
	outw(StartCoax, ioaddr + EL3_CMD);
	PAUSE(MILLISECS(50));
    }
    else if (st->media==PORT_10BASET) {
	/* 10baseT interface: enable link beat and jabber check */
	EL3WINDOW(4);
	outw(inw(ioaddr + WN4_MEDIA) | MEDIA_TP, ioaddr + WN4_MEDIA);
    }
	      
    /* Switch to the stats window, and clear all stats by reading */
    EL3WINDOW(6);
    for (i=0; i<9; i++)
	inb(ioaddr + i);
    inw(ioaddr + 10);
    inw(ioaddr + 12);
	      
    /* Switch to register set 1 for normal use */
    EL3WINDOW(1);
	      
    /* Accept broadcasts and properly addressed packets only */
    outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
#if 0
    outw(StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics */
#endif /* 0 */
    /* Enable the receiver */
    outw(RxEnable, ioaddr + EL3_CMD);
    /* Enable the transmitter */
    outw(TxEnable, ioaddr + EL3_CMD);
	      
    /* Allow status bits to be seen */
    outw(SetStatusEnb | 0xff, ioaddr + EL3_CMD);
    /* Ack all pending events, and set active indicator mask */
    outw(AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
	 ioaddr + EL3_CMD);
    outw(SetIntrEnb | IntLatch | TxAvailable |
	 RxComplete | StatsFull,
	 ioaddr + EL3_CMD);
	      
    /* Unmask the interrupt */
    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_unmask_irq(st->irq);
    LEAVE_KERNEL_CRITICAL_SECTION();
    SRCThread$Release(Pvs(srcth), &st->mu);
    TRC(printf("Netcard_Up finished\n"));

    return True;
}

static bool_t Down_m(Netcard_cl *self)
{
    Netcard_st *st=self->st;
    int ioaddr;

    if (!st->active) return False;

    printf("Netcard_Down: called\n");

    ioaddr=st->address;

    USING(MUTEX, &st->mu, {
	outw(StatsDisable, ioaddr + EL3_CMD);
	outw(RxDisable, ioaddr + EL3_CMD);
	outw(TxDisable, ioaddr + EL3_CMD);

	if (st->media==PORT_BNC) {
	    outw(StopCoax, ioaddr + EL3_CMD);
	}
	else if (st->media==PORT_10BASET) {
	    /* Disable link beat and jabber */
	    EL3WINDOW(4);
	    outw(inw(ioaddr + WN4_MEDIA) & ~MEDIA_TP, ioaddr + WN4_MEDIA);
	}

	/* Switching to window 0 disables IRQ... */
	EL3WINDOW(0);
	/* ...but we do it explicitly anyway */
	outw(0x0f00, ioaddr + WN0_IRQ);

	/* Update statistics here */
	update_statistics(st);
    });

    /* XXX Unregister the interrupt stub here perhaps */

    st->active=False;

    printf("Netcard_Down: finished\n");

    return True;
}

void Main(Closure_clp *self)
{
    int tag=0;
    bool_t found;
    Netcard_st *st;
    unsigned short addr, irq, media;

    TRC(printf("3c509: searching for cards...\n"));

    found = False;
    for (tag=0; probe(tag, &addr, &irq, &media); tag++)
    {
	found = True;
	st = initialise_card(addr, irq, media, tag);
	printf("3c509-%d at 0x%x irq %d media %s, "
	       "h/w addr %02x:%02x:%02x:%02x:%02x:%02x\n",
	       tag, addr, irq, port_desc[media],
	       st->hwaddr.a[0], st->hwaddr.a[1], st->hwaddr.a[2], 
	       st->hwaddr.a[3], st->hwaddr.a[4], st->hwaddr.a[5]);
#ifdef SRCIT
	break;
#endif
    }

    if (found)
	TRC(printf("3c509: initialisation complete\n"));
    else {
	printf("3c509: didn't find any cards.\n");
    }
}
