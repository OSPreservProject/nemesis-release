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
**      Nemesis Device Driver
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Driver for NS16550 devices
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** ID : $Id: serial.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <string.h>
#include <ntsc.h>

#include <irq.h>
#include <io.h>

#include <dev/ns16550.h>

#include <ecs.h>
#include <time.h>
#include <mutex.h>
#include <exceptions.h>

#include <IDCMacros.h>
#include <VPMacros.h>             /* XXX for RO(_vp) */

#include <Context.ih>
#include <Interrupt.ih>
#include <Rd.ih>
#include <Serial.ih>
#include <SerialMod.ih>

#include <EntryMod.ih>

#include "Serial_st.h"
#ifdef INTEL
#  include <autoconf/serial_split.h>
#endif

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define DB(_x) _x

#ifdef CONFIG_SERIAL_SPLIT
#define SET_TOP_BIT(c) (c) = (c) | 0x80
#else
#define SET_TOP_BIT(c) do { } while(0)
#endif


/* Serial interface definitions */
static Serial_SetSpeed_fn Serial_SetSpeed_m;
static Serial_SetProtocol_fn Serial_SetProtocol_m;
static Serial_SetControlLines_fn Serial_SetControlLines_m;
static Serial_GetControlLines_fn Serial_GetControlLines_m;
static Serial_SendBreak_fn Serial_SendBreak_m;

static Serial_op serial_op = {
    Serial_SetSpeed_m,
    Serial_SetProtocol_m,
    Serial_SetControlLines_m,
    Serial_GetControlLines_m,
    Serial_SendBreak_m,
};

static SerialMod_New_fn SerialMod_New_m;
static SerialMod_op serialmod_op = {
    SerialMod_New_m
};

static  Context_List_fn                 Context_List_m;
static  Context_Get_fn          Context_Get_m;
static  Context_Add_fn          Context_Add_m;
static  Context_Remove_fn               Context_Remove_m;
static  Context_Dup_fn          Context_Dup_m;
static  Context_Destroy_fn              Context_Destroy_m;

static  Context_op      context_ms = {
  Context_List_m,
  Context_Get_m,
  Context_Add_m,
  Context_Remove_m,
  Context_Dup_m,
  Context_Destroy_m
};


static const SerialMod_cl scl = { &serialmod_op, NULL };
CL_EXPORT(SerialMod, SerialMod, scl);

/*****************************************************************/
/* Prototypes */ 

static void SerialSetSpeed(Serial_st *st, uint32_t divisor);
static void SerialSetProto(Serial_st *st, uchar_t databits,
			   Serial_Parity parity, uchar_t stopbits);
static void SerialRx_m(Serial_st *st);

static void InterruptThread_m(Serial_st *st);

STAT(static void      SerialStatInit( Serial_st *st ));
STAT(static void      StatThread_m( Serial_st *st));
static void SerialTransmit_m (Serial_st *st);

/******************************************************************/
/* Auto-detect routine for serial ports */

static uart_type detect(uint32_t baseaddr)
{
    int x, olddata;

    /* check if a UART is present */
    olddata=inb(baseaddr+NS16550_MCR);
    outb(0x10,baseaddr+NS16550_MCR);
    if ((inb(baseaddr+NS16550_MSR)&0xf0)) return uart_type_none;
    outb(0x1f,baseaddr+NS16550_MCR);
    if ((inb(baseaddr+NS16550_MSR)&0xf0)!=0xf0) return uart_type_none;;
    outb(olddata,baseaddr+NS16550_MCR);

    /* next thing to do is look for the scratch register */
    olddata=inb(baseaddr+NS16550_SCR);
    outb(0x55,baseaddr+NS16550_SCR);
    if (inb(baseaddr+NS16550_SCR)!=0x55) return uart_type_8250;
    outb(0xaa,baseaddr+NS16550_SCR);
    if (inb(baseaddr+NS16550_SCR)!=0xAA) return uart_type_8250;
    outb(olddata,baseaddr+NS16550_SCR);

    /* then check whether there's a FIFO */
    outb(1,baseaddr+NS16550_IIR);
    x=inb(baseaddr+NS16550_IIR);
    /* some software relies on this! */
    outb(0x0,baseaddr+NS16550_IIR);
    if ((x&0x80)==0) return uart_type_16450;
    if ((x&0x40)==0) return uart_type_16550;
    return uart_type_16550a;
}

/******************************************************************/
/* Routine to initialise a serial port */

static Context_clp InitialiseSerialPort(uint32_t base, uint32_t irq,
					string_t name, bool_t halt)
{
    Serial_st *st;
    Interrupt_clp ipt;
    Interrupt_AllocCode rc;
    uart_type type;
    bool_t fifo=False;

    if (!name) {
	printf("No name provided for serial port base %x\n",base);
	return NULL;
    }

    /* Work out what we've got */
#ifdef __ALPHA__
    ENTER_KERNEL_CRITICAL_SECTION();
    ntsc_mask_irq(irq);
    LEAVE_KERNEL_CRITICAL_SECTION();
#endif /* __ALPHA__ */
    type=detect(base);

    switch (type) {
    case uart_type_none:
	return 0;
	break;
    case uart_type_8250:
	TRC(printf("Port %s is a 8250. This is unsupported.\n", name));
	return 0;
	break;
    case uart_type_16450:
	TRC(printf("Port %s is a 16450 at %x.\n", name, base));
	fifo=False;
	break;
    case uart_type_16550:
	TRC(printf("Port %s is a 16550 at 0x%x.\n", name, base));
	fifo=False;
	break;
    case uart_type_16550a:
	TRC(printf("Port %s is a 16550A at 0x%x.\n", name, base));
	fifo=True;
	break;
    }

    /* Allocate space for state */
    TRC(printf("serial: %s: allocating state\n",name));
    st=Heap$Malloc(Pvs(heap),sizeof(*st));
    if (!st) {
	printf("serial: %s: couldn't allocate state record\n",
		name);
	return NULL;
    }

    CL_INIT(st->serial_cl, &serial_op, st);
    CL_INIT(st->rd_cl, &rd_op, st);
    CL_INIT(st->wr_cl, &wr_op, st);

    strncpy(st->name,name,15);
    st->name[15]=0;

    /* Things for interrupts */
    st->stub.dcb = RO(Pvs(vp));
    st->stub.ep  = Events$AllocChannel(Pvs(evs));
    st->event    = EC_NEW();
    Events$Attach(Pvs(evs), st->event, st->stub.ep, Channel_EPType_RX);

    /* Details of the UART */
    st->uart.base=base;
    st->uart.irq=irq;
    st->uart.type=type;
    st->uart.fifo=fifo;
    st->uart.lcr=0;
    st->uart.mcr=0;
    st->uart.ier=0;

    st->stalled=True;
    st->halt = halt;
    
    /* Mutex to protect access to the hardware and buffers */
    MU_INIT(&st->mu);

    /* Buffers */
    st->rxbuf.head = 0;
    st->rxbuf.tail = 0;
    st->rxbuf.free = SERIAL_BUFSIZE-1;
    st->rxbuf.size = SERIAL_BUFSIZE;
    
    st->txbuf.head = 0;
    st->txbuf.tail = 0;
    st->txbuf.free = SERIAL_BUFSIZE-1;
    st->txbuf.size = SERIAL_BUFSIZE;

    SerialInitRd(st);
    SerialInitWr(st);
    STAT(SerialStatInit(st));

    TRC(printf("%s: grabbing interrupt %d\n",st->name,st->uart.irq));
    ipt = IDC_OPEN("sys>InterruptOffer", Interrupt_clp);
    rc  = Interrupt$Allocate(ipt, st->uart.irq, _generic_stub, &st->stub);

    /* Initialise the port hardware */
    TRC(printf("%s: got interrupt; initialising port hardware\n",st->name));

    SerialSetProto(st, 8, Serial_Parity_None, 1);
    SerialSetSpeed(st, 0x0c); /* 9600 bps */
    if (fifo) {
	/* Clear FIFOs, enable, trigger at 1 byte */
	outb(NS16550_FCR_TRG1 | NS16550_FCR_ENABLE |
	     NS16550_FCR_CLRX  | NS16550_FCR_CLTX, st->uart.base+NS16550_FCR);
    }
    st->uart.mcr|=NS16550_MCR_OUT2;
    st->uart.ier|=NS16550_IER_ETHREI | NS16550_IER_ERDAI;
    outb(st->uart.mcr, st->uart.base + NS16550_MCR); /* Modem control */
    outb(st->uart.ier, st->uart.base + NS16550_IER ); /* Interrupts */


    /* XXX this must happen _after_ initialising all the mutices and
       condition variables */
    Threads$Fork(Pvs(thds), InterruptThread_m, st, 0);

    /* From this point onwards access to the hardware must be done using
       the mutex */

    CL_INIT(st->cx, &context_ms, st);
    /* The Context code at the end of this file knows about the three offers
       in Serial_st */

    /* Some IDCs can be blocking, which is not a Good Thing for multiple
       people. We give it its own entry (i.e. thread) and only allow
       one binding at a time.

       XXX SDE: this needs more thought. For now I've left the reader with
       its own thread, and the writer and control services share the
       pervasive entry. */
    {
	IDCTransport_clp shmt; 
	Type_Any         any;
	IDCService_clp   service;
	EntryMod_clp     emod;
	Entry_clp        entry;
	extern void      RdSrvThread();
	IDCCallback_cl  *rd_cb;
	
	/* Get the transport mechanism */
	shmt = NAME_FIND ("modules>ShmTransport", IDCTransport_clp);
	
	/* Initialise the server any */
	ANY_INIT(&any, Rd_clp, &st->rd_cl); 
	
	rd_cb     = Heap$Malloc(Pvs(heap),sizeof(*rd_cb));
	rd_cb->op = &rd_cb_op;
	rd_cb->st = st;
	
	/* Create a new entry for the reader service */
	emod  = NAME_FIND("modules>EntryMod", EntryMod_clp);
	entry = EntryMod$New (emod, Pvs(actf), Pvs(vp), 
			      VP$NumChannels (Pvs(vp)));
	
	/* Fork the thread to deal with the reader */
	Threads$Fork (Pvs(thds), RdSrvThread, Entry$Closure (entry), 0);
	
	/* Make an offer with a callback => can ensure max one
	   binding at a time */
	st->rd_offer = IDCTransport$Offer (shmt, &any, rd_cb,
					   Pvs(heap), Pvs(gkpr),
					   entry, &service);
	
	ANY_INIT(&any, Wr_clp, &st->wr_cl);
	st->wr_offer = IDCTransport$Offer (shmt, &any, NULL,
					   Pvs(heap), Pvs(gkpr),
					   Pvs(entry), &service);
	ANY_INIT(&any, Serial_clp, &st->serial_cl);
	st->serial_offer = IDCTransport$Offer (shmt, &any, NULL,
					       Pvs(heap), Pvs(gkpr),
					       Pvs(entry), &service);
    }

    return &st->cx;
}

static Context_clp SerialMod_New_m(SerialMod_clp self,
				   string_t name,
				   uint32_t base,
				   uint32_t interrupt, bool_t halt)
{
    Context_clp s;

    s=InitialiseSerialPort(base, interrupt, name, halt);

    return s;
}

/*--------------------------------------------------------- Server Side ---*/

#define EVTRC(_x) 
/*
 * simply put a character into the buffer at the next spot. If the buffer is
 * full, discard.
 */
void SerialPutcBuffer( buf_t *buf, uchar_t c)
{
    EVTRC(printf("SerialPutcBuffer: char is %x\n", c));
    if (buf->free) {
	/* Put the character in the circular buffer */
	buf->buf[buf->head++] = c;
	
	/* Wrap the pointer if necessary */
	if (buf->head == buf->size)
	    buf->head = 0;
	
	buf->free--;
    } else {
	/* Discard for now; maybe block later */
    }
}

/* Get a character from the buffer.  Returns zero if buffer empty */
uchar_t SerialGetcBuffer (buf_t *buf) 
{
    uchar_t c;

    if (!BUF_IS_EMPTY(buf))  {

	c = buf->buf[buf->tail++];
    
	/* Wrap around */
	if (buf->tail == buf->size) {
	    buf->tail = 0;
	}
	buf->free++;

	return c;
    } 

    return 0;
}

/*------------------------------------------------ ConsoleWr Methods ---*/

void
Serial_PutChars (Serial_st *st, uchar_t *s, uint32_t n)
{
    uchar_t c;
    uchar_t *str = s;

    MU_LOCK(&st->mu);

    while (n) {
	ENTER_KERNEL_CRITICAL_SECTION();
	c = *str++;
	LEAVE_KERNEL_CRITICAL_SECTION();
	SerialPutcBuffer (&st->txbuf, c);
	n--;
    }
/*    if (st->stalled) { */
    if (inb(st->uart.base+NS16550_LSR)&NS16550_LSR_THRE) {
	SerialTransmit_m(st);
    }
    MU_RELEASE(&(st->mu));
}

/* ----------------------------------------------------- SerialRx method - */

/* XXX update to use FIFO */
static void SerialRx_m (Serial_st *st)
{
    uint32_t c;

    /* Check whether there is space in the transmitter; if so, call the
       transmit routine. This is a bug in the 16550. */
    if (inb(st->uart.base+NS16550_LSR)&NS16550_LSR_THRE)
	SerialTransmit_m(st);

    /* XXX do this for as many characters as we can read */

    /* clear the interrupt by reading the character */
    c = inb(st->uart.base + NS16550_RBR );
    if (st->halt && c==0x04) {
	ENTER_KERNEL_CRITICAL_SECTION();
	eprintf("Serial: got EOT on port %s. Halting machine.\n",
		st->name);
	ntsc_dbgstop();
	LEAVE_KERNEL_CRITICAL_SECTION();
    }

    STAT(++(st->n_rx));

    USING(MUTEX, &(st->rd.mu),
	  SerialPutcBuffer ( &st->rxbuf, c);
	);
    SIGNAL(&st->rd.cv);
}

/* -------------------------------------------------- Line Status method - */

/* Called with mutex locked whenever the line status register changes
 */
static void SerialLStat_m (Serial_st *st)
{
    uint32_t c;
  
    /* clear the interrupt by reading line status register */
    c = inb(st->uart.base + NS16550_LSR );
    STAT(++(st->n_misc));
}
/* ----------------------------------------------------- MODEM method - */

/* Called with device mutex locked whenever the modem control signals change
 */
static void SerialModem_m (Serial_st *st)
{
    uint32_t c;
    
    /* clear the interrupt by reading the MODEM status register */
    c = inb(st->uart.base + NS16550_MSR );
    STAT(++(st->n_misc));
}

/* -------------------------------------------------- Transmit Method - */

/* This function must be called with the port mutex locked */
/* Called from the interrupt thread and also when transmission needs to
   be kicked */
static void SerialTransmit_m (Serial_st *st)
{
    uint8_t c;
    uint32_t count;

    if (BUF_IS_EMPTY(&st->txbuf)) {
	SIGNAL(&st->wr.cv); /* Tell writer that flush is complete */
	st->stalled=True;
	return;
    }


    /* If we have a FIFO we can push up to 16 bytes */
    if (st->uart.fifo) count=16; else count=1;
    while (count-- && !BUF_IS_EMPTY(&st->txbuf)) {
	c=SerialGetcBuffer(&st->txbuf);
	SET_TOP_BIT(c);
	outb(c, st->uart.base + NS16550_THR);
	STAT((st->n_tx)++);
    }
    /* If we think we're finished, make sure that we can't push a further
       byte otherwise we won't get the LSR_THRE interrupt */
    if ((!BUF_IS_EMPTY(&st->txbuf)) &&
	(inb(st->uart.base+NS16550_LSR)&NS16550_LSR_THRE)) {
	/* There's room for one more byte */
	c=SerialGetcBuffer(&st->txbuf);
	SET_TOP_BIT(c);
	outb(c, st->uart.base + NS16550_THR);
	STAT((st->n_tx)++);
    }
    st->stalled=False;
}
      
/* ------------------------------------------------- Interrupt Thread - */

static void InterruptThread_m (Serial_st *st)
{
    uchar_t ip;
    Event_Val cur;

    cur=0;

    while(1)  {

	/* Turn on interrupts in the PIC's */
	ENTER_KERNEL_CRITICAL_SECTION();
	ntsc_unmask_irq(st->uart.irq);
	LEAVE_KERNEL_CRITICAL_SECTION();

	/* Wait for next interrupt */
	cur = EC_AWAIT(st->event, cur);
	cur++;

	/* Read pending register */
	MU_LOCK(&st->mu);
	{
	    while (((ip=inb(st->uart.base+NS16550_IIR))&NS16550_IIR_PEND)
		   == 0) {
		/* Use only low four bits if using fifo */
		switch (ip & 0x0f) {
		case NS16550_IIR_RLS:
		    SerialLStat_m(st);
		    break;
		
		case NS16550_IIR_RDA: /* FIFO full */
		    SerialRx_m(st);
		    break;
		
		case NS16550_IIR_FIFO: /* FIFO not empty */
		    SerialRx_m(st);
		    break;

		case NS16550_IIR_THRE:
		    SerialTransmit_m(st);
		    break;
		
		case NS16550_IIR_MODEM: /* Modem control signals changed */
		    SerialModem_m(st);
		    break;
		}
	    } 
	}
	MU_RELEASE(&st->mu);
    }
}

/*--------------------------------------------------- Hardware ------------*/

static void SerialSetSpeed(Serial_st *st, uint32_t divisor)
{
    outb(NS16550_LCR_DLAB | st->uart.lcr, st->uart.base+NS16550_LCR);
    outw(divisor, st->uart.base+NS16550_DDL);
    outb(st->uart.lcr, st->uart.base+NS16550_LCR);
}

static void SerialSetProto(Serial_st *st, uchar_t databits,
			   Serial_Parity parity, uchar_t stopbits)
{
    uchar_t word;
    switch (databits) {
    case 5: word=0; break;
    case 6: word=1; break;
    case 7: word=2; break;
    case 8: word=3; break;
    default: word=3; break;
    }

    switch (parity) {
    case Serial_Parity_None: break;
    case Serial_Parity_Odd: word |= 0x088; break;
    case Serial_Parity_Even: word |= 0x18; break;
    case Serial_Parity_Mark: word |= 0x28; break;
    case Serial_Parity_Space: word |= 0x38; break;
    }

    switch (stopbits) {
    case 1: break;
    case 2: word |= 4; break;
    }

    st->uart.lcr=word;
    outb(st->uart.lcr, st->uart.base+NS16550_LCR);
}

/* Serial control methods */
static void Serial_SetSpeed_m(Serial_cl *self, uint32_t speed)
{
    uint32_t divisor=0xc;
    Serial_st *st=self->st;

    switch (speed) {
    case     50: divisor=0x900; break;
    case     75: divisor=0x600; break;
    case    110: divisor=0x417; break;
    case    150: divisor=0x300; break;
    case    300: divisor=0x180; break;
    case    600: divisor=0x0c0; break;
    case   1200: divisor=0x060; break;
    case   1800: divisor=0x040; break;
    case   2000: divisor=0x03a; break;
    case   2400: divisor=0x030; break;
    case   3600: divisor=0x020; break;
    case   4800: divisor=0x018; break;
    case   7200: divisor=0x010; break;
    case   9600: divisor=0x00c; break;
    case  19200: divisor=0x006; break;
    case  38400: divisor=0x003; break;
    case  57600: divisor=0x002; break;
    case 115200: divisor=0x001; break;
    default: RAISE_Serial$BadSpeed(); break;
    }

    MU_LOCK(&st->mu);
    SerialSetSpeed(st, divisor);
    MU_RELEASE(&st->mu);
}

static void Serial_SetProtocol_m(Serial_cl *self, uint32_t databits,
				 Serial_Parity parity, uint32_t stopbits)
{
    Serial_st *st=self->st;

    MU_LOCK(&st->mu);
    SerialSetProto(st, databits, parity, stopbits);
    MU_RELEASE(&st->mu);
}

static void Serial_SetControlLines_m(Serial_cl *self,
				     Serial_ControlLines cl)
{
    Serial_st *st=self->st;

    MU_LOCK(&st->mu);
    st->uart.mcr &= ~0x3;
    if (SET_IN(cl, Serial_ControlLine_DTR)) st->uart.mcr |= 1;
    if (SET_IN(cl, Serial_ControlLine_RTS)) st->uart.mcr |= 2;

    outb(st->uart.mcr, st->uart.base+NS16550_MCR);
    MU_RELEASE(&st->mu);
}

static Serial_ControlLines Serial_GetControlLines_m(Serial_cl *self)
{
    Serial_st *st=self->st;
    uchar_t w;
    Serial_ControlLines r=0;

    MU_LOCK(&st->mu);
    w=inb(st->uart.base+NS16550_MSR);
    MU_RELEASE(&st->mu);
    if (w&0x10) SET_ADD(r, Serial_ControlLine_CTS);
    if (w&0x20) SET_ADD(r, Serial_ControlLine_DSR);
    if (w&0x40) SET_ADD(r, Serial_ControlLine_RI);
    if (w&0x80) SET_ADD(r, Serial_ControlLine_DCD);
    return r;
}

static void Serial_SendBreak_m(Serial_cl *self)
{
    Serial_st *st=self->st;

    MU_LOCK(&st->mu);
    st->uart.lcr |= NS16550_LCR_SBRK;
    outb(st->uart.lcr, st->uart.base+NS16550_LCR);
    PAUSE(MILLISECS(250));
    st->uart.lcr &= ~NS16550_LCR_SBRK;
    outb(st->uart.lcr, st->uart.base+NS16550_LCR);
    MU_RELEASE(&st->mu);
}

/*---------------------------------------------------- Entry Points ----*/

static Context_Names *Context_List_m (
        Context_cl      *self )
{
    Context_Names *r;

    r=SEQ_NEW(Context_Names, 3, Pvs(heap));
    SEQ_CLEAR(r);
    SEQ_ADDH(r, strdup("rd"));
    SEQ_ADDH(r, strdup("wr"));
    SEQ_ADDH(r, strdup("control"));

    return r;
}

static bool_t Context_Get_m (
        Context_cl      *self,
        string_t        name    /* IN */,
        Type_Any        *obj    /* OUT */ )
{
    Serial_st    *st = self->st;

    if (strcmp(name,"rd")==0) {
	ANY_INIT(obj, IDCOffer_clp, st->rd_offer);
	return True;
    }

    if (strcmp(name,"wr")==0) {
	ANY_INIT(obj, IDCOffer_clp, st->wr_offer);
	return True;
    }

    if (strcmp(name,"control")==0) {
	ANY_INIT(obj, IDCOffer_clp, st->serial_offer);
	return True;
    }

    return False;
}

static void Context_Add_m (
        Context_cl      *self,
        string_t        name    /* IN */,
        const Type_Any  *obj    /* IN */ )
{
    RAISE_Context$Denied();
}

static void Context_Remove_m (
        Context_cl      *self,
        string_t        name    /* IN */ )
{
    RAISE_Context$Denied();
}

static Context_clp Context_Dup_m (
        Context_cl      *self,
        Heap_clp        h       /* IN */,
        WordTbl_clp     xl      /* IN */ )
{
    RAISE_Context$Denied();
    return NULL;
}

static void Context_Destroy_m (
        Context_cl      *self )
{
    /* XXX May eventually shut down the serial driver */
    RAISE_Context$Denied();
}

#ifdef SERIAL_STATS
/**************************************************************************/
/* Statistics functions	*/

/*
 * Initialise statistics
 */
static void
SerialStatInit( Serial_st *st )
{
  st->n_ints 	= 0;
  st->n_tx	= 0;
  st->n_rx 	= 0;
  st->n_misc 	= 0;
  st->stat_thread = Threads$Fork(Pvs(thds), StatThread_m, st, 0 );
}

/*
 * Statistics thread
 */
static void
StatThread_m( Serial_st *st)
{
  TRC(printf("Serial: stats thread up.\n"));
  for(;;) {

    /* Wait for 5 seconds */
    PAUSE(SECONDS(5));
    
    /* Print information */
    TRC(printf("UART: I:%x  TX:%x  RX:%x  MISC:%x\n",
	    st->n_ints, st->n_tx, st->n_rx, st->n_misc));
  }
}

#endif /* SERIAL_STATS */
