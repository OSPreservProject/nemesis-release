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
**      IDE access register descriptions
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Describe the layout of the IDE control registers
** 
** ENVIRONMENT: 
**
**      IDE device driver
** 
** ID : $Id: ide.h 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#ifndef ide_h_
#define ide_h_


#define IDE0_BASE 0x1f0
#define IDE0_IRQ     14

#define IDE1_BASE 0x170
#define IDE1_IRQ     15


/* Offsets from the IO base: */
/* These registers are 8-bits wide, other than IDE_DATA, which is 16 */
#define IDE_DATA	0
#define IDE_ERR		1	/* read only */
#define IDE_PRECOMP	1	/* write only (ignored) */
#define IDE_SCNT	2	/* sector count (length of access) */
#define IDE_SNUM	3	/* sector number (start of access) */
#define IDE_CYLLO	4	/* low bits of cylinder number */
#define IDE_CYLHI	5	/* low bits of cylinder number */
#define IDE_DRHEAD	6	/* drive/head */
#define IDE_STAT	7	/* status: read only */
#define IDE_CMD		7	/* command: write only */

/* extras */
#define IDE_ALTSTAT	0x206	/* alternate status (no irq cancel): r/o */
#define IDE_DOUT	0x206	/* digital output: write only */
#define IDE_DADDR	0x207	/* drive address: read only */


/* Register contents */

/* Error */
#define BBK	0x80	/* sector marked bad by host */
#define UNC	0x40	/* uncorrectable data error */
#define MC	0x20	/* media changed */
#define NID	0x10	/* no index mark found */
#define MCR	0x08	/* media change requested (soft eject pushed) */
#define ABT	0x04	/* command aborted */
#define NT0	0x02	/* no track 0 */
#define NDM	0x01	/* no data address mark */

/* Status */
#define BSY	0x80	/* busy (all other register & bits invalid) */
#define RDY	0x40	/* ready to accept commands */
#define WFT	0x20	/* write fault */
#define SKC	0x10	/* seek complete */
#define DRQ	0x08	/* data request (sector cache valid) */
#define CORR	0x04	/* correctable data error */
#define IDX	0x02	/* track index passed */
#define ERR	0x01	/* error register valid */

/* Commands */
#define CALIBRATE	0x10	/* seek to cyl 0 (recover from seek err) */
#define READSEC		0x20	/* read sector */
#define WRITESEC	0x30	/* write sector */
#define VERIFYSEC	0x40	/* verify sector */
#define FORMATTRK	0x50	/* format track */
#define SEEK		0x70	/* seek to track & select head */
#define DIAG		0x90	/* diagnositics + slave detection */
#define SETPARAM	0x91	/* set geometry */
#define MULTREAD        0xC4    /* multi-sector read */
#define MULTWRITE       0xC5    /* multi-sector write */
#define SETMULT         0xC6    /* set multi-sector mode */
#define READBUF		0xe4	/* read sector cache */
#define WRITEBUF	0xe8	/* write sector cache */
#define IDENTIFY	0xec	/* identify */

/* digital output register */
#define RESET	0x04	/* reset controller */
#define nIEN	0x02	/* interupt enable (1 = masked, 0 = enabled) */


typedef struct {
    uint16_t conf;
    uint16_t physcyl;

    uint16_t resvd;
    uint16_t heads;

    uint16_t bytesPerTrack;
    uint16_t bytesPerSec;

    uint16_t secPerTrack;
    uint16_t resvd2[3];

    char     serialno[20];

    uint16_t bufType;
    uint16_t bufSize;  /* in 512 byte blocks */

    uint16_t ECCread;
    char     fware[8];
    char     model[40];
    uint16_t maxmult;	/* only bits 0-7 valid */

    uint16_t b32io;
    uint16_t dmalba;

    uint16_t resvd3;
    uint16_t pio;

    uint16_t dma;
    uint16_t resvd4;

    uint16_t lcyl;
    uint16_t lheads;

    uint16_t lsecPerTrack;
    uint16_t bytesPerlSec_LO;

    uint16_t bytesPerlSec_HI;

    uint16_t secPerInt;
    uint16_t secAddrLBA_LO;

    uint16_t secAddrLBA_HI;
    uint16_t singleDMA;

    uint16_t multiDMA;
    uint16_t resvd5[64];
    uint16_t manufacturer[32];
    uint16_t resvd6[96];
} id_info;


/* ------------------------------------------------------------ */
/* Partition table stuff */

typedef struct {
    uint8_t	activeflag;	/* 0x80 == active, 0x00 == inactive */
    uint8_t	SOP[3];		/* start of partition */
    uint8_t	ostype;		/* 0 == Non-DOS, 1 == FAT12, 4 == FAT16,
				   5 == Extended DOS, 6 == FAT>32Mb */
    uint8_t	EOP[3];		/* end of partition */
    uint16_t	startSecLO;
    uint16_t	startSecHI;
    uint16_t	nsecsLO;
    uint16_t	nsecsHI;
} parttbl_entry;

typedef struct {
    parttbl_entry	p[4];	/* NB: stored in reverse order */
} parttbl;

#define PARTTBL_MAGIC 0xaa55
typedef struct {
    uint8_t	MBR[446];	/* MBR boot loader */
    parttbl	ptbl;
    uint16_t	magic;
} partsect;

/* Macros to extract head, cylinder and sector number from SOP and EOP
 * entries */
#define PTBL_HEAD(x) ((x)[0])
#define PTBL_CYL(x)  ((((int)((x)[1]) & 0xc0) << 2) | ((x)[2]))
#define PTBL_SEC(x) ((x)[1] & 0x3f)

/* Memory barrier */
#define barrier()       __asm__ __volatile__("": : :"memory")


#endif /* ide_h_ */
