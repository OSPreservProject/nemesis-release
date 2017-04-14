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
**      ide
** 
** FUNCTIONAL DESCRIPTION:
** 
**      IDE disk driver
** 
** ENVIRONMENT: 
**
**      Device driver domain
** 
** ID : $Id: ide.c 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/


/* TODO: (in rough order of importance)
 *   - timeout on lack of interrupts
 *   - multiple sector mode (done: SOC)
 *   - Triton bus mastering DMA support
 *   - better (ie proper) error recovery
 *   - support Disk interface rather than BlockDev
 */

#include <nemesis.h>
#include <io.h>
#include <dev/ide.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <irq.h>

#include <Interrupt.ih>

#include <mutex.h>
#include <VPMacros.h>
#include <ecs.h>
#include <time.h>
#include <IDCMacros.h>
#include <contexts.h>


#include "ideprivate.h"

/* Control path debugging */
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/* Voluminous data path debugging */
#ifdef DATA_DEBUG
#define DATATRC(_x) _x
#else
#define DATATRC(_x)
#endif

#define MULTI_MODE                 /* enable mult-mode code */

#if defined(DEBUG) || defined(DATA_DEBUG)
static void dump(uint8_t *p, int nbytes);
#endif


static  BlockDev_BlockSize_fn           BlockSize_m;
static  BlockDev_Read_fn                Read_m;
static  BlockDev_Write_fn               Write_m;
static	Disk_GetParams_fn 		Disk_GetParams_m;
static	Disk_Translate_fn 		Disk_Translate_m;
static	Disk_Where_fn 		        Disk_Where_m;
static	Disk_ReadTime_fn 		Disk_ReadTime_m;
static	Disk_WriteTime_fn 		Disk_WriteTime_m;

static	Disk_op	disk_ms = {
    BlockSize_m,
    Read_m,
    Write_m,
    Disk_GetParams_m,
    Disk_Translate_m,
    Disk_Where_m,
    Disk_ReadTime_m,
    Disk_WriteTime_m
};







static int probe(iface_t *st);
static int probe_drive(disc_t *d);
static int timeout_wait(int io, int bit, int value, int timeout);
static void bswap(uint16_t *base, int len);
static void trim(char *base, int len);
static void show_stat(iface_t *i);
static void pr_stat(int status);
static int process_partntbl(disc_t *d);
static int polled_read_sec(disc_t *d,
			   char *buf, int nsecs,
			   int lba,
			   int cyl,
			   int head,
			   int sec);
static void InterruptThread(addr_t data);

int Main()
{
    iface_t *ide0;
    iface_t *ide1;

    TRC(printf("IDE driver entered\n"));

    ide0 = malloc(sizeof(iface_t));
    ide0->hw[0] = NULL;
    ide0->hw[1] = NULL;
    MU_INIT(&ide0->mu);
    ide0->iobase = IDE0_BASE;
    ide0->irq = IDE0_IRQ;
    ide0->ifno = 0;
    strcpy(ide0->name, "ide0");

    if (!probe(ide0))
    {
	MU_FREE(&ide0->mu);
	free(ide0);
	ide0 = NULL;
    }

    ide1 = malloc(sizeof(iface_t));
    ide1->hw[0] = NULL;
    ide1->hw[1] = NULL;
    MU_INIT(&ide1->mu);
    ide1->iobase = IDE1_BASE;
    ide1->irq = IDE1_IRQ;
    ide1->ifno = 1;
    strcpy(ide1->name, "ide1");

    if (!probe(ide1))
    {
	MU_FREE(&ide1->mu);
	free(ide1);
	ide1 = NULL;
    }

    if (ide0 == NULL && ide1 == NULL)
	printf("ide: no drives found, exiting\n");

    return 0;
}


/* ------------------------------------------------------------ */
/* Probe, initialisation, etc */


/* Probe the interface "st", returning 1 for success, 0 otherwise */
static int probe(iface_t *st)
{
    int i;
    disc_t *d;
    USDMod_clp usdmod = NULL;
    USDDrive_clp usd;
    Interrupt_clp ipt;

    for(i=0; i<=0; i++)
    {
	d = st->hw[i] = malloc(sizeof(disc_t));
	if (!d)
	{
	    printf("%s probe: out of memory for %s state\n",
		   st->name, i?"slave":"master");
	    return 0;
	}

	memset(d, 0, sizeof(disc_t));
	d->ide = st;
	d->num = i;

	if (!probe_drive(d))
	{
	    printf("%s: no %s drive present\n", st->name, i?"slave":"master");
	    free(d);
	    st->hw[i] = NULL;
	}
    }

    if (st->hw[0]==NULL && st->hw[1]==NULL)
	return 0;

    /* Allocate the interrupt */
    st->stub.dcb = RO(Pvs(vp));
    st->stub.ep  = Events$AllocChannel(Pvs(evs));
    st->event    = EC_NEW();
    Events$Attach(Pvs(evs), st->event, st->stub.ep, Channel_EPType_RX);
    ipt=IDC_OPEN("sys>InterruptOffer", Interrupt_clp);
    if (Interrupt$Allocate(ipt, st->irq, _generic_stub, &st->stub)
	!=Interrupt_AllocCode_Ok)
    {
	printf("%s: failed to allocate interrupt %d\n", st->name, st->irq);
	return 0;
    }

    /* Mark current state as idle */
    st->rq.valid = False;
    CV_INIT(&st->reqdone);

    /* Start interrupt thread */
    Threads$Fork(Pvs(thds), InterruptThread, st, 0);

    /* Create a IREF Disks and pass it to USDMod$New() */
    usdmod = NAME_FIND("modules>USDMod", USDMod_clp);
    if (usdmod)
    { 
	for(i=0; i < 1; i++)
	{
	    if (st->hw[i])
	    {
		char name[64];

		CL_INIT(st->disk[i], &disk_ms, st->hw[i]);
		sprintf(name, "dev>hd%c", DRIVE_LET(st->hw[i]));
		TRC(printf("ide%d: creating USD for %s\n", i, name));

		usd = USDMod$New(usdmod, &(st->disk[i]), name);
		IDC_EXPORT(name, USDDrive_clp, usd);
	    }
	}
    }
    else
    {
	printf("ide: couldn't find USDMod\n");
    }

    return 1;
}


static int probe_drive(disc_t *d)
{
    int		iobase = d->ide->iobase;
    uint32_t	size;
    const char *bt;

    /* Probing for ide discs seems to consist of sending some command
     * to the drive and waiting for a reply: no reply -> no device.
     * Bit grungy really. */    

    TRC(printf("ide: probing hd%c\n", DRIVE_LET(d)));

    /* wait for busy signal to be clear */
    if (timeout_wait(iobase+IDE_STAT, DRQ|BSY|ERR, 0, MAX_TRIES))
    {
	TRC(printf("ide: timeout(1) probing hd%c\n", DRIVE_LET(d)));
	return 0;
    }

    /* select the drive */
    outb(0xa0 | (d->num << 4), iobase+IDE_DRHEAD);
    outb(IDENTIFY, iobase+IDE_CMD); /* identify thyself! */

    PAUSE(MILLISECS(50)); /* give drive a chance to get itself together */

    /* wait for busy line to go low */
    if (timeout_wait(iobase+IDE_STAT, BSY|ERR, 0, MAX_TRIES))
    {
	TRC(printf("ide: timeout(2) probing hd%c\n", DRIVE_LET(d)));
	return 0;
    }

    /* wait for data to arrive */
    if (timeout_wait(iobase+IDE_STAT, DRQ|ERR|BSY, DRQ, MAX_TRIES))
    {
	TRC(printf("ide: timeout(3) probing hd%c\n", DRIVE_LET(d)));
	return 0;
    }

    insw(iobase+IDE_DATA, &d->id, 256);

    /* make sure drive is quiescent again */
    if (timeout_wait(iobase+IDE_STAT, DRQ|ERR|BSY, 0, MAX_TRIES))
    {
	TRC(printf("ide: timeout(4) probing hd%c\n", DRIVE_LET(d)));
	return 0;
    }

    DATATRC(dump((void *)&d->id, 512));

    if (d->id.heads > 256)
    {
	printf("hd%c: implausible number of heads(%d), ignoring\n",
	       DRIVE_LET(d),
	       d->id.heads);
	return 0;
    }

    /* byte-swap the ascii strings that need it */
    bswap((uint16_t*)&d->id.fware, 4);
    bswap((uint16_t*)&d->id.model, 20);
    /* trim whitespace off the end */
    trim((char*)&d->id.fware, 8);
    trim((char*)&d->id.model, 40);

    d->id.maxmult &= 0xff;  /* top bits aren't defined */

#ifdef MULTI_MODE
  
    /* try to set multiple-sector mode if it is supported */

    /* wait for busy signal to be clear */
    if (timeout_wait(iobase+IDE_STAT, DRQ|BSY|ERR, 0, MAX_TRIES))
    {
	printf("ide: timeout waiting to set multi-mode for hd%c\n", DRIVE_LET(d));
	return 0;
    }
    
    outb(d->id.maxmult, iobase+IDE_SCNT);  /* number of sectors/block */
    outb(SETMULT, iobase+IDE_CMD);         /* send set-multi command */

    PAUSE(MILLISECS(50)); /* give drive a chance to get itself together */

    /* wait for busy line to go low */
    if (timeout_wait(iobase+IDE_STAT, BSY|ERR, 0, MAX_TRIES))
    {
	printf("ide: timeout trying to set multi-mode for hd%c\n", DRIVE_LET(d));
	return 0;
    }
    d->mult_count= d->id.maxmult; /* record multiple sector value */
    printf("ide: multiple sector mode enabled, %d-sector blocks.\n",d->mult_count);
#endif /* MULTI_MODE */

    switch(d->id.bufType)
    {
    case 1:
	bt = "1 sect";
	break;
    case 2:
	bt = "DualPort";
	break;
    case 3:
	bt = "DualPortCache";
	break;
    default:
	bt = "unknown";
    }

    /* XXX this sizing code may be wrong */
    size = d->id.physcyl * d->id.heads * d->id.secPerTrack / 2048;
    TRC(printf("calcsize=%dMb\n", size));
    if (d->id.dmalba & 0x200)
    {
	size = ((d->id.secAddrLBA_HI << 16) | (d->id.secAddrLBA_LO)) / 2048;
	TRC(printf("LBAsize=%dMb\n", size));
    }

    TRC(printf("hd%c: %.40s (%.8s) %dMb, CHS=%d/%d/%d, %dKb cache (%s), MultiMode=%d, %s%s\n",
	   DRIVE_LET(d),
	   d->id.model, d->id.fware,
	   size,
	   d->id.physcyl, d->id.heads, d->id.secPerTrack,
	   d->id.bufSize / 2,
	   bt,
	   d->id.maxmult,
	   (d->id.dmalba & 0x200)?"LBA ": "",
	   (d->id.dmalba & 0x100)?"DMA ": ""));

    if (!(d->id.dmalba & 0x200))
    {
	printf("hd%c: drive does not support LBA, ignoring it\n",
	       DRIVE_LET(d));
	return 0;
    }

    /* XXX hack in bytes per sector coz we want to */
    d->id.bytesPerSec = 512;

    if (!process_partntbl(d))
	return 0;

    return 1;
}


/* Read the partition table.  Parse it.  Return the number of Amoeba
 * partitions found, or 0 for read or parse failure.  */
static int process_partntbl(disc_t *d)
{
    partsect sbuf;	/* caution: 512 bytes on stack */
    parttbl_entry *pt;
    int i;
    int npar;
    uint32_t nsecs;
    uint32_t startsec;
    char *osname;
    char buf[12];

    TRC(printf("hd%c: reading partition table\n", DRIVE_LET(d)));

    /* Read the partition table */
    if (!polled_read_sec(d,
			 (void*)&sbuf, 1, 0/*lba*/,
			 0/*cyl*/,
			 0/*head*/,
			 1/*sec*/))
    {
	printf("hd%c: read of partition table failed\n", DRIVE_LET(d));
	return 0;
    }

    DATATRC(dump((void *)&sbuf, 512));

    if (sbuf.magic != PARTTBL_MAGIC)
    {
	printf("hd%c: bad partition table magic (%04x != %04x)\n",
	       DRIVE_LET(d), sbuf.magic, PARTTBL_MAGIC);
	return 0;
    }

    /* Pointer to first partition table */
    pt = sbuf.ptbl.p;

    /* Number of partitions we're using */
    npar = 0;

    /* Work out what the maximum allowed sector number is */
    d->maxsec = 0;

    /* Dump partition table data for the curious */
    for(i=0; i<4; i++)
    {	
	startsec = pt[i].startSecHI << 16 | pt[i].startSecLO;
	nsecs = pt[i].nsecsHI << 16 | pt[i].nsecsLO;
	if (nsecs != 0)
	{
	    switch(pt[i].ostype) {
	    case 0x01: osname="FAT12"; break;
	    case 0x04: osname="FAT16"; break;
	    case 0x05: osname="extended"; break;
	    case 0x06: osname="FAT16>=32Mb"; break;
	    case 0x82: osname="Linux swap"; break;
	    case 0x83: osname="Linux native"; break;
	    case 0x93: osname="Amoeba/Nemesis"; break;
	    default:
		sprintf(buf, "ostype:%02x", pt[i].ostype);
		osname = buf;
	    }
		
	    TRC(printf("hd%c%c: %u(+%u), CHS:%d/%d/%d - %d/%d/%d, %s%s\n",
		   DRIVE_LET(d),
		   '1' + i,
		   startsec,
		   nsecs,
		   (int)PTBL_CYL(pt[i].SOP),
		   (int)PTBL_HEAD(pt[i].SOP),
		   (int)PTBL_SEC(pt[i].SOP),
		   (int)PTBL_CYL(pt[i].EOP),
		   (int)PTBL_HEAD(pt[i].EOP),
		   (int)PTBL_SEC(pt[i].EOP),
		   osname,
		   (pt[i].activeflag & 0x80) ? ", active" : ""));

	    if (pt[i].ostype == 0x93)
		npar++;

	    d->maxsec = MAX(d->maxsec, startsec + nsecs);
	}
    }

    TRC(printf("maxsec is %d\n", d->maxsec));

    return 1;
}


/* Wait for the io register at "io" to have bit "bit" set to "value".
   Returns 0 for success, stat for timeout */
static int timeout_wait(int io, int bit, int value, int tries)
{
    uint8_t stat = 0;
 /* keep intel builds not core-dumping (gcc bug) */
    volatile Time_ns wait_time = MILLISECS(50);

    for(; tries; tries--)
    {
	if (((stat=inb(io)) & bit) == value)
	    break;
	TRC(pr_stat(stat));
	if(tries%4 != 0)
	    PAUSE(wait_time);
    }

    return tries? 0 : stat;
}
    

#if defined(DEBUG) || defined(DATA_DEBUG)
static void dump(uint8_t *p, int nbytes)
{
    int i;

    for(i=0; i<nbytes; i++)
    {
	printf("%02x%s", p[i], (i&1)? " ": "");
	if ((i % 16) == 15)
	    printf("\n");
    }
}
#endif /* DEBUG */


/* Swap bytes.  "len" is the number of 16bit words to
 * process */
static void bswap(uint16_t *base, int len)
{
    while(len--)
    {
	*base = ((*base & 0xff) << 8) | ((*base & 0xff00) >> 8);
	base++;
    }
}


static void trim(char *base, int len)
{
    char *p = base + len - 1;

    while(*p == 0x20 && p != base)
    {
	*p = 0;
	p--;
    }
}


/* read one sector out of sector cache */
static void try_to_clear_fifo(iface_t *intf)
{
    int iobase = intf->iobase;
    int i;

    for(i=0; i < 512 / 2; i++)
    {
	(void) inw_p(iobase + IDE_DATA);
    }

    printf("shot fifo\n");
}

static void ide_reset(iface_t *i)
{
    int iobase = i->iobase;

    PAUSE(MILLISECS(50));
    outb(RESET, iobase + IDE_DOUT);
    PAUSE(MILLISECS(50));
    printf("reset controller:\n");
    show_stat(i);
}
    

/* Low-level raw read of a sector, polling for completion.  No
 * translation or bounds checking is performed. "d" describes the disc
 * to select. "nsecs" sectors are reads into the buffer starting at
 * "buf". The first sector read is given by "cyl", "head", and "sec".
 * "lba" specifies whether the access is to be done with the drive in
 * LBA mode.  Returns 1 for success, 0 otherwise. */
static int polled_read_sec(disc_t *d,
			   char *buf, int nsecs,
			   int lba,
			   int cyl,
			   int head,
			   int sec)
{
    int iobase = d->ide->iobase;
    int stat;
    int i;

    /* wait for drive to be ready */
    /* XXX Do we need DRQ to be clear? */
retry:
    if ((stat = timeout_wait(iobase+IDE_STAT, DRQ|BSY|ERR, 0, 100)))
    {
	pr_stat(stat);
	printf("hd%c: polled_read_sec: timeout waiting to queue command\n",
	       DRIVE_LET(d));
	ide_reset(d->ide);
	goto retry;
	return 0;
    }
    /* select the drive, head & lba mode  */
    outb(0xa0 | (lba << 6) | (d->num << 4) | head, iobase+IDE_DRHEAD);

    outb(nsecs, iobase+IDE_SCNT);	/* sector count */
    outb(sec, iobase+IDE_SNUM);	/* start sector */
    outb(cyl & 0xff, iobase+IDE_CYLLO);	/* cylinder number */
    outb((cyl>>8) & 0xff, iobase+IDE_CYLHI);
    outb(READSEC, iobase+IDE_CMD);	/* go! */

    /* wait for the drive to respond */
    if (timeout_wait(iobase+IDE_STAT, BSY|ERR, 0, MAX_TRIES))
    {
	printf("hd%c: polled_read_sec: timeout waiting for drive response\n",
	       DRIVE_LET(d));
	show_stat(d->ide);
	return 0;
    }

    /* wait for DRQ to go high */
    if (timeout_wait(iobase+IDE_STAT, DRQ|BSY|ERR, DRQ, MAX_TRIES))
    {
	printf("hd%c: polled_read_sec: timeout waiting for data\n",
	       DRIVE_LET(d));
	show_stat(d->ide);
	return 0;
    }

    /* XXX kernel critical section only needed in order to have write
     * access to the client buffers */
    ENTER_KERNEL_CRITICAL_SECTION();
    for(i=0; i < nsecs * 512 / 2; i++)
    {
	((uint16_t *)buf)[i] = inw_p(iobase + IDE_DATA);
    }
    LEAVE_KERNEL_CRITICAL_SECTION();

    return 1;
}


static void show_stat(iface_t *i)
{
    int iobase = i->iobase;
    int status = inb(iobase+IDE_STAT);
    int error = inb(iobase+IDE_ERR);	/* may not be valid */

    printf("%u:status={ ",
	   ((inb(iobase+IDE_DRHEAD) & 0xf) << 24) |
	   (inb(iobase+IDE_CYLHI) << 16) |
	   (inb(iobase+IDE_CYLLO) << 8) |
	   inb(iobase+IDE_SNUM));
#define PRST(_x)     if (status & _x) printf(#_x " ");
    PRST(BSY);
    PRST(RDY);
    PRST(WFT);
    PRST(SKC);
    PRST(DRQ);
    PRST(CORR);
    PRST(IDX);
    PRST(ERR);
    printf("}");
#define PRER(_x)     if (error & _x) printf(#_x " ");
    if ((status & (ERR|BSY)) == ERR)	/* error register is valid */
    {
	printf(", err={ ");
	PRER(BBK);
	PRER(UNC);
	PRER(MC);
	PRER(NID);
	PRER(MCR);
	PRER(ABT);
	PRER(NT0);
	PRER(NDM);
	printf("}");
    }
    printf("\n");
#undef PRST
#undef PRER
}
    
    
static void pr_stat(int status)
{
    printf("s={ ");
#define PRST(_x)     if (status & _x) printf(#_x " ");
    PRST(BSY);
    PRST(RDY);
    PRST(WFT);
    PRST(SKC);
    PRST(DRQ);
    PRST(CORR);
    PRST(IDX);
    PRST(ERR);
    printf("}\n");
#undef PRST
}


/* ------------------------------------------------------------ */
/* Interrupt handling */


/* Handle a read interrupt */
static void do_read_intr(iface_t *st, int status)
{
    int iobase = st->iobase;
    req_t *rq = &st->rq;
    disc_t *d = rq->d;
#ifdef MULTI_MODE
    unsigned int msect, nsect;
#endif /* MULTI_MODE */
    /* make sure drive has DRQ asserted and isn't having a baby */
    if ((status & (DRQ|BSY|ERR)) != DRQ)
    {
	printf("%s: read intr: ", st->name);
	show_stat(st);
	ide_reset(st); /* bit nasty, that */
	return;  /* XXX should count the errors, and fail if too many */
    }

#ifdef MULTI_MODE
    msect = d->mult_count;

read_next:

    if (msect) 
    {
        if ((nsect = rq->count) > msect)
	  nsect = msect;
	msect -= nsect;
    }
    else
      nsect = 1;
#endif /* MULTI_MODE */

    if (rq->count > 0)
    {
	/* looks ok, lets rock!  (read, even) */
	ENTER_KERNEL_CRITICAL_SECTION();
#ifdef MULTI_MODE
	insw(iobase+IDE_DATA, rq->buf, nsect*256);
#else
	insw(iobase+IDE_DATA, rq->buf, 256);
#endif
	LEAVE_KERNEL_CRITICAL_SECTION();

#ifdef MULTI_MODE
	/* step on one block of sectors */
	((char *)rq->buf) += nsect<<9;   /* move on 512 * nsect */
	rq->count-=nsect;
	rq->block+=nsect;

	if ( (rq->count > 0) && msect)
	  goto read_next;
#else
	((char *)rq->buf) += 512;
	rq->count--;
	rq->block++;
#endif /* MULTI_MODE */
	if (rq->count == 0)
	{
	    /* done all, so send request back to client */
	    SIGNAL(&st->reqdone);
	}
    }
    else
    {
	printf("%s: unexpected read intr: ");
	show_stat(st);
    }
}


/* Handle a write interrupt */
static void do_write_intr(iface_t *st, int status)
{
    int iobase = st->iobase;
    req_t *rq = &st->rq;

    /* make sure drive is happy */
    if ((status & (RDY|SKC|BSY|ERR|WFT)) != (RDY|SKC))
    {
	printf("%s: write intr: ", st->name);
	show_stat(st);
	ide_reset(st); /* bit nasty, that */
	return;  /* XXX should count the errors, and fail if too many */
    }

    /* have we finished sending all the data we wanted? */
    if (rq->count == 0)
    {
	SIGNAL(&st->reqdone); 	/* done! */
	return;
    }

    /* we need to send more data, is the drive is willing to accept it? */
    if ((status & DRQ) == 0)
    {
	printf("%s: write intr: data pending, but no DRQ, ", st->name);
	show_stat(st);
	/* XXX recover! */
	ide_reset(st);
	return;
    }

    /* looks ok, so send the sector */
    outsw(iobase+IDE_DATA, rq->buf, 256);

    rq->block++;
    rq->count--;
    ((char*)rq->buf) += 512;

    return;
}


void multwrite_data (iface_t *st, int status)
{
  int iobase = st->iobase;
  req_t *rq = &st->rq;

  int mcount = rq->d->mult_count;
  int nsect; 
  //printf("entering multwrite_data\n");
  do 
       
    {
      nsect= rq->count;
      if (nsect > mcount)
	nsect = mcount;
      mcount -= nsect;
      //printf("ide: multwrite_data - writing block %d.\n", rq->block);
      outsw(iobase+IDE_DATA, rq->buf, nsect<<8);

      rq->block+=nsect;
      rq->count-=nsect;

      ((char*)rq->buf) += nsect<<9;
      
      /* have we finished sending all the data we wanted? */
      if (rq->count == 0)
	{
	  /*  SIGNAL(&st->reqdone); */	/* done! */
	  return;
	}
      
      
    }while(mcount);
  
}

/* Handle a mult_write interrupt */
static void do_multwrite_intr(iface_t *st, int status)
{
    req_t *rq = &st->rq;

    /* make sure drive is happy  */

    if ((status & (RDY|SKC|BSY|ERR|WFT)) != (RDY|SKC)) 
    {
	printf("%s: multwrite intr: ", st->name);
	pr_stat(status);
	show_stat(st);
	ide_reset(st); /* bit nasty, that */
	return;  /* XXX should count the errors, and fail if too many */
    }  

    if ((status & DRQ))
      {
	multwrite_data(st, status);
      }
    else
      {
	if (rq->count==0)
	   SIGNAL(&st->reqdone);
	else
	  {
	    printf("%s: write intr: data pending, but no DRQ, ", st->name);
	    show_stat(st);
	    /* XXX recover! */
	    ide_reset(st);
	    return;
	  }
      }
    

  
    return;
}

/* One thread per ide interface */
static void InterruptThread(addr_t data)
{
    iface_t *st = data;
    int iobase = st->iobase;
    int status;

    st->ack = 1;

    MU_LOCK(&st->mu); /* so we can release as first thing in main loop */

    while(1)
    {
	MU_RELEASE(&st->mu);

	/* Turn on interrupts in the PICs */
	ENTER_KERNEL_CRITICAL_SECTION();
	ntsc_unmask_irq(st->irq);
	LEAVE_KERNEL_CRITICAL_SECTION();

	/* Wait for next interrupt */
	st->ack = EC_AWAIT(st->event, st->ack);
	st->ack++;

	MU_LOCK(&st->mu);

	status = inb(iobase+IDE_STAT);  /* clear pending int */

	/* Is this int in reponse to a request? */
	if (!st->rq.valid)
	{
	    /* no, so clean up whatever went wrong */
	    if (status & (BSY|ERR|DRQ))
	    {
		printf("%s: unexpected int, ", st->name);
		show_stat(st);
	    }
	    /* in addition, try to flush pending data */
	    if (status & DRQ)
		try_to_clear_fifo(st);

	    continue;
	}

	/* so, this is now very likely to be a valid interrupt */
	switch (st->rq.type) {
	case Read:
	    do_read_intr(st, status);
	    break;
	case Write:
	    do_write_intr(st, status);
	    break;
#ifdef MULTI_MODE
	case MultRead:
	    do_read_intr(st,status);
	    break;
	case MultWrite:
	    do_multwrite_intr(st, status);
	    break;
#endif /* MULTI_MODE */
	default:
	    printf("ide: unknown request type?!?\n");
	}

    }
}




/* Look at the request, and queue it at the drive */
static int ide_cmd(iface_t *st)
{
    int iobase = st->iobase;
    int stat;
    req_t *rq = &st->rq;

    if ((stat = timeout_wait(iobase+IDE_STAT, DRQ|BSY|ERR, 0, 100)))
    {
	pr_stat(stat);
	printf("hd%c: ide_cmd: timeout waiting to queue command\n",
	       DRIVE_LET(rq->d));
	ide_reset(st);
	return 0;
    }

    /* select the drive, head & lba mode  */
    outb(0xa0 |
	 (1/*lba*/ << 6) |
	 (rq->d->num << 4) |
	 (rq->block & 0xf000000) >> 24,  /* head*/
	 iobase+IDE_DRHEAD);

    outb(rq->count, iobase+IDE_SCNT);	/* sector count */
    outb(rq->block & 0xff, iobase+IDE_SNUM);	/* start sector */

    outb((rq->block & 0xff00) >> 8, iobase+IDE_CYLLO);	/* cylinder number */
    outb((rq->block & 0xff0000) >> 16, iobase+IDE_CYLHI);

    switch (rq->type) {
    case Read:
	outb(READSEC, iobase+IDE_CMD);
	break;
    case Write:
	outb(WRITESEC, iobase+IDE_CMD);
	break;
#ifdef MULTI_MODE
    case MultRead:
        outb(MULTREAD, iobase+IDE_CMD);
	break;
    case MultWrite:      
        outb(MULTWRITE, iobase+IDE_CMD);
	break;
#endif /* MULTI_MODE */
    default:
	printf("ide: unknown command %d\n", rq->type);
    }

    return 1;
}


/* ------------------------------------------------------------ */
/* BlockDev methods */



static uint32_t BlockSize_m(BlockDev_cl *self)
{
    disc_t   *st = self->st;

    return st->id.bytesPerSec;
}

static bool_t Read_m(BlockDev_cl *self,
		     BlockDev_Block b,
		     uint32_t n,
		     addr_t a)
{
    disc_t   *st = self->st;
    iface_t  *ide = st->ide;
    int ret;

    if ((b+n-1) > st->maxsec)
    {
	printf("hd%c: attempt to read off end of device (block %d > %d)\n",
	       DRIVE_LET(st), (b+n-1), st->maxsec);
	return False;
    }

    DATATRC(printf("R:%d(+%d)\n", b, n));

    MU_LOCK(&ide->mu);

    if (ide->rq.valid)
    {
	printf("%s: request still valid?!?\n", st->ide->name);
	MU_RELEASE(&ide->mu);
	return False;
    }

#ifdef CONFIG_MEMSYS_EXPT
    {
	word_t start_va, last_va; 
	word_t start_pa, last_pa;
	word_t buflen; 

	start_va = (word_t)a; 
	buflen   = n * 512 /* XXX st->params.blksz */; 
	last_va  = (word_t)((void *)a + buflen - 1);
        
	if((start_pa = virt_to_phys(start_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Read: blkno=%d, nblks=%d, buf at VA [%p, %p)\n", 
		    b, n, start_va, last_va);
	    eprintf("start va %p is bad (TNV or PAGE)\n", start_va);
	    pte = ntsc_trans(start_va >> PAGE_WIDTH); 
	    eprintf("trans of start va=%p gives pte = %lx\n", start_va, pte);
	    ntsc_dbgstop();
	}
	if((last_pa = virt_to_phys(last_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Read: blkno=%d, nblks=%d, buf at VA [%p, %p)\n", 
		    b, n, start_va, last_va);
	    eprintf("last va %p is bad (TNV or PAGE)\n", last_va);
	    pte = ntsc_trans(last_va >> PAGE_WIDTH); 
	    eprintf("trans of  last va=%p gives pte = %lx\n", last_va, pte);
	    ntsc_dbgstop();
	}

	if(last_pa != (start_pa + buflen - 1)) {
	    eprintf("BlockDev$Read: blkno=%d, nblks=%d, buf at VA [%p, %p)\n", 
		    b, n, start_va, last_va);
	    eprintf("                                 buf at PA [%p, %p)\n", 
		    start_pa, last_pa);
	    eprintf("=> not contiguous. Death.\n");
	    ntsc_dbgstop();
	}
    }
#endif



    ide->rq.type = Read;
    ide->rq.d = st;
    ide->rq.block = b;
    ide->rq.count = n;
    ide->rq.buf = a;
    ide->rq.error = 0;
    ide->rq.valid = True;

#ifdef MULTI_MODE
    if (st->mult_count > 0)
      ide->rq.type = MultRead;
#endif /* MULTI_MODE */
    /* set up the read sector command */
    ide_cmd(ide);

    /* and wait for completion */
    WAIT(&ide->mu, &ide->reqdone);

    /* any problems? */
    ret = ide->rq.error;
    ide->rq.valid = False;  /* allow re-use */

    MU_RELEASE(&ide->mu);
    
    return !ret;
}


static bool_t Write_m(BlockDev_cl *self,
		      BlockDev_Block b,
		      uint32_t n,
		      addr_t a)
{
    disc_t	*st = self->st;
    iface_t	*ide = st->ide;
    int		iobase = ide->iobase;
    int		ret;
    int		stat;

#ifdef CONFIG_MEMSYS_EXPT
    {
	word_t start_va, last_va; 
	word_t start_pa, last_pa;
	word_t buflen; 

	start_va = (word_t)a; 
	buflen   = n * 512 /* XXX st->params.blksz */; 
	last_va  = (word_t)((void *)a + buflen - 1);
        
	if((start_pa = virt_to_phys(start_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Write: blkno=%d nblks=%d, buf at VA [%p, %p)\n", 
		    b, n, start_va, last_va);
	    eprintf("start va %p is bad (TNV or PAGE)\n", start_va);
	    pte = ntsc_trans(start_va >> PAGE_WIDTH); 
	    eprintf("trans of start va=%p gives pte = %lx\n", start_va, pte);
	    ntsc_dbgstop();
	}
	if((last_pa = virt_to_phys(last_va)) == NO_ADDRESS) {
	    word_t pte; 
	    eprintf("BlockDev$Write: blkno=%d nblks=%d, buf at VA [%p, %p)\n", 
		    b, n, start_va, last_va);
	    eprintf("last va %p is bad (TNV or PAGE)\n", last_va);
	    pte = ntsc_trans(last_va >> PAGE_WIDTH); 
	    eprintf("trans of  last va=%p gives pte = %lx\n", last_va, pte);
	    ntsc_dbgstop();
	}

	if(last_pa != (start_pa + buflen - 1)) {
	    eprintf("BlockDev$Write: blkno=%d nblks=%d, buf at VA [%p, %p)\n", 
		    b, n, start_va, last_va);
	    eprintf("                                 buf at PA [%p, %p)\n", 
		    start_pa, last_pa);
	    eprintf("=> not contiguous. Death.\n");
	    ntsc_dbgstop();
	}
    }
#endif

    if ((b+n-1) > st->maxsec)
    {
	printf("hd%c: attempt to write off end of device (block %u > %u)\n",
	       DRIVE_LET(st), (b+n-1), st->maxsec);
	return False;
    }

    DATATRC(printf("W:%d(+%d)\n", b, n));

    MU_LOCK(&ide->mu);

    if (ide->rq.valid)
    {
	printf("%s: write: request still valid?!?\n", st->ide->name);
	MU_RELEASE(&ide->mu);
	return False;
    }

    ide->rq.type = Write;
    ide->rq.d = st;
    ide->rq.block = b;
    ide->rq.count = n;
    ide->rq.buf = a;
    ide->rq.error = 0;
    ide->rq.valid = True;
#ifdef MULTI_MODE
    if (st->mult_count > 0)
	ide->rq.type = MultWrite;
#endif /* MULTI_MODE */
    /* set up the write sector command */
    ide_cmd(ide);

    /* wait for drive to become ready for first sector*/    
    if ((stat = timeout_wait(iobase+IDE_ALTSTAT,
			     RDY|SKC|DRQ|BSY|ERR|WFT,
			     RDY|SKC|DRQ, 2)))
    {
	pr_stat(stat);
	printf("hd%c: write: timeout waiting to send block\n",
	       DRIVE_LET(st));
	/* XXX failure mode? */
	ide_reset(ide);
	ide->rq.valid=False;
	MU_RELEASE(&ide->mu);
	return False;
    }

    if (st->mult_count > 0)
	multwrite_data(ide, stat);
    else
      { 
	/* send the first sector */
	outsw(iobase+IDE_DATA, a, 256);
	
	/* note we've sent it */
	ide->rq.block++;
	ide->rq.count--;
	((char*)ide->rq.buf) += 512;
      }
    /* and pass off to interrupt handler to complete the transaction */
    WAIT(&ide->mu, &ide->reqdone);

    /* any problems? */
    ret = ide->rq.error;
    ide->rq.valid = False;  /* allow re-use */

    MU_RELEASE(&ide->mu);
    
    return !ret;
}

/*---------------------------------------------------- Entry Points ----*/

static void 
Disk_GetParams_m (
	Disk_cl	*self,
	Disk_Params	*p	/* OUT */ )
{
    disc_t	*st = self->st;

    p->blksz  = st->id.bytesPerSec; 
    /* XXX this sizing code may be wrong */
    p->blocks = st->id.physcyl * st->id.heads * st->id.secPerTrack / 2048;
    p->cyls   = st->id.lcyl; 
    p->heads  = st->id.lheads; 
    p->seek   = MILLISECS(8); /* XXX hacked up default */
    p->rotate = MILLISECS(8); /* XXX hacked up default */
    p->headsw = MILLISECS(8); /* XXX hacked up default */ 
    p->xfr    = MICROSECS(8); /* XXX hacked up default */
    return;
}

static void 
Disk_Translate_m (
	Disk_cl	*self,
	BlockDev_Block	b	/* IN */,
	Disk_Position	*p	/* OUT */ )
{
  /*disc_t	*st = self->st;*/

}

static void 
Disk_Where_m (
	Disk_cl	*self,
	Time_ns	t	/* IN */,
	Disk_Position	*p	/* OUT */ )
{
   /*disc_t	*st = self->st;*/

}

static Time_ns 
Disk_ReadTime_m (
	Disk_cl	*self,
	BlockDev_Block	b	/* IN */ )
{
  /*disc_t	*st = self->st;*/
  return 0;

}

static Time_ns 
Disk_WriteTime_m (
	Disk_cl	*self,
	BlockDev_Block	b	/* IN */ )
{
  /*disc_t	*st = self->st;*/
  return 0;
}

