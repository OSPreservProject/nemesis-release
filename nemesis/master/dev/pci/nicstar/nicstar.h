/* nicstar.h, M. Welsh (matt.welsh@cl.cam.ac.uk)
 * Definitions for IDT77201.
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * M. Welsh, 6 July 1996
 *
 */

#ifndef _NEMESIS_NICSTAR_H
#define _NEMESIS_NICSTAR_H

#undef NICSTAR_STATS

#include <nemesis.h>
#include <stdio.h>
#include <salloc.h>
#include <exceptions.h>
#include <mutex.h>
#include <time.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <VPMacros.h>

#include <dev/pci.h>
#include <AALPod.ih>
#include <IO.ih>
#include <IOEntry.ih>
#include <PCIBios.ih>
#include <Interrupt.ih>
#include <io.h>
#include <rbuf.h>
#include <IDCCallback.ih>
#include <irq.h>

/* User definitions *********************************************************/

#define NICSTAR_HEARTBEAT

#define DEBUG_OUTPUT(stream, args...)   \
{\
     char out[256];\
     sprintf(out, args);\
     fprintf(stream, "Nicstar: " __FUNCTION__ " : %s", out);\
}
    
#define DEBUG_OUT(args...) DEBUG_OUTPUT(stdout, args)
#define DEBUG_ERR(args...) DEBUG_OUTPUT(stderr, args)
#define PRINT_RX(args...) 
#define PRINT_TX(args...) 
#define PRINT_IP(args...) 
#define PRINT_ERR(args...) DEBUG_ERR(args)
#define PRINT_DATA(args...)
#define PRINT_STATS(args...) DEBUG_OUT(args)
#define PRINT_INIT(args...) DEBUG_OUT(args)
#define PRINT_NORM(args...) DEBUG_OUT(args)

/* Argument parsing */

#include <environment.h>

#define ENTER(x) x = ntsc_ipl_high()
#define LEAVE(x) x = ntsc_swpipl(x)

typedef void* caddr_t;

#define kmalloc(size, flags) Heap$Malloc(Pvs(heap), size)
#define printk printf


#define TRC(x) x

#define EINVAL  1;
#define EIO     2;
#define EADDRINUSE 3;
#define ENOMEM 4;

struct sk_buff
{
  long len;
  void *data;
  void *end;
  struct {
    int iovcnt;
  } atm;
  struct sk_buff *next, *prev;
  struct sk_buff_head *list;
};

struct sk_buff_head 
{
        struct sk_buff  * next;
        struct sk_buff  * prev;
       int qlen;
};

struct iovec
{
        void *iov_base;         /* BSD uses caddr_t (same thing in effect) */
        int iov_len;
};


#define NICSTAR_MAX_DEVS 5
#define NICSTAR_IO_SIZE (4*1024)

#define SM_BUFSZ 48
#define NUM_SM_BUFS 32
#define LG_BUFSZ 8192
#define NUM_LG_BUFS 256
#define MIN_RESERVE_BUFS 64
#define DEFAULT_NUM_BUFS 64
#define MAX_SDU_BUFS 32  /* Max number of buffers in an AAL5 SDU */
#define NUM_SCQ_TRAIL 64 /* Number of outstanding SCQ entries */

enum nicstar_regs {
  DR0 = 0x00,
  DR1 = 0x04,
  DR2 = 0x08,
  DR3 = 0x0c,
  CMD = 0x10,
  CFG = 0x14,
  STAT = 0x18,
  RSQB = 0x1c,
  RSQT = 0x20,
  RSQH = 0x24,
  CDC = 0x28,
  VPEC = 0x2c,
  ICC = 0x30,
  RAWCT = 0x34,
  TMR = 0x38,
  TSTB = 0x3c,
  TSQB = 0x40,
  TSQT = 0x44,
  TSQH = 0x48,
  GP = 0x4c,
  VPM = 0x50
};

/* SRAM locations */
#define NICSTAR_VBR_SCD0 (0x1e7f4)
#define NICSTAR_VBR_SCD1 (0x1e7e8)
#define NICSTAR_VBR_SCD2 (0x1e7dc)
#define NICSTAR_TST_REGION (0x1c000)  /* For 32k SRAM */
#define NICSTAR_SCD_REGION (0x1d000)  /* Arbitrary value */

#define TBD_ENDPDU          0x40000000      /* 1 => last buffer of
                                                    AAL5 CS_PDU */
#define TBD_AAL0            0x00000000      /*   000 = AAL0   */
#define TBD_AAL34           0x04000000      /*   001 = AAL3/4 */
#define TBD_AAL5            0x08000000      /*   010 = AAL5   */
#define TBD_UBR_M               0x00800000      /* M value timer count, VBR */
#define TBD_UBR_N               0x00010000      /* N value timer count, VBR */

#define RCT_AAL0        (0<<16)
#define RCT_AAL34       (1<<16)
#define RCT_AAL5        (2<<16)
#define RCT_RAW         (3<<16) | (1<<15)
#define RCT_LGBUFS      (1<<21)

#define CFG_SWRST       (1<<31)
#define CFG_RXPTH       (1<<29)

#define CFG_SMBUF_48    (0<<27)
#define CFG_SMBUF_96    (1<<27)
#define CFG_SMBUF_240   (2<<27)
#define CFG_SMBUF_2048  (3<<27)

#define CFG_LGBUF_2048  (0<<25)
#define CFG_LGBUF_4096  (1<<25)
#define CFG_LGBUF_8192  (2<<25)
#define CFG_LGBUF_16384 (3<<25)

#define CFG_EFBIE       (1<<24)
#define CFG_RXSTQ_2048  (0<<22)
#define CFG_RXSTQ_4096  (1<<22)
#define CFG_RXSTQ_8192  (2<<22)

#define CFG_ICAPT       (1<<21)
#define CFG_IGGFC       (1<<20)

#define CFG_RXCNS_4096  (0<<16)
#define CFG_RXCNS_8192  (1<<16)
#define CFG_RXCNS_16384 (2<<16)

#define CFG_VPECA       (1<<15)
#define CFG_RXINT_NONE  (0<<12)
#define CFG_RXINT_IMMED (1<<12)
#define CFG_RXINT_314   (2<<12)

#define CFG_RAWIE       (1<<11)
#define CFG_RQFIE       (1<<10)
#define CFG_RXRM        (1<< 9)
#define CFG_TMOIE       (1<< 7)
#define CFG_TXEN        (1<< 5)
#define CFG_TXINT       (1<< 4)
#define CFG_TXUIE       (1<< 3)

#define CFG_UMODE_CELL  (0<< 2)
#define CFG_UMODE_BYTE  (1<< 2)
#define CFG_TXSFI       (1<< 1)
#define CFG_PHYIE       (1<< 0)
   
typedef struct _Nicstar_st Nicstar_st;
typedef struct nicstar_rcte {
  uint32_t status;
  uint32_t buf_handle;
  uint32_t dma_addr;
  uint32_t crc;
} nicstar_rcte;
#define RX_CTABLE_SIZE (16 * 1024) /* 16k words */

typedef struct nicstar_fbd {
  uint32_t buf_handle;
  uint32_t dma_addr;
} nicstar_fbd;

typedef struct nicstar_rsqe {
  uint32_t vpi_vci;
  uint32_t buf_handle;
  uint32_t crc;
  uint32_t status;
} nicstar_rsqe;
#define RX_STATQ_ENTRIES (512)

typedef struct nicstar_tbd {
  uint32_t status;
  uint32_t buf_addr;
  uint32_t ctl_len;
  uint32_t cell_hdr;
} nicstar_tbd;

#define SCQ_ENTRIES (512)  /* Variable-rate SCQ */ /* XXX 511 */
#define SCQ_SIZE (8192)    /* Number of bytes actually allocated */
#define SCQFULL_MAGIC (0xfeed0000)
#define SCQFULL_TIMEOUT (SECONDS(3))


#define NICSTAR_RXQLEN 256
#define NICSTAR_NRXCLIENTS 20
#define NICSTAR_TXQLEN 256
#define NICSTAR_NTXCLIENTS 20
 
#define NICSTAR_MAX_MISSES_ALLOWED 64

typedef struct nicstar_scd {
  uint32_t base_addr;
  uint32_t tail_addr;
  uint32_t crc;
  uint32_t rsvd;
  struct nicstar_tbd cache_a;
  struct nicstar_tbd cache_b;
} nicstar_scd;

typedef struct nicstar_tsi {
  uint32_t status;
  uint32_t timestamp;
} nicstar_tsi;
#define TX_STATQ_ENTRIES (1024)


typedef struct nicstar_scq_shadow {
    unsigned char cell[48];
    struct rbuf* rbuf;
} nicstar_scq_shadow;

typedef struct nicstar_buf_list {
  caddr_t buf_addr;
  struct nicstar_buf_list *next;
} nicstar_buf_list;

typedef struct {			/* Per client state record */
    word_t    next;		/* Free list */
    dcb_ro_t *owner;		/* DCB of client */
    IO_clp    io;		/* RX IO channel */

    int bufs_allowed;
    int pdus_missed;
    bool_t enabled;
    AALPod_Mode mode;
    AALPod_Master master;
    int vci;
    Event_Count ec;		
    Event_Val   ev;		
    int ditch_count;
    int rx_count;
    bool_t definitely_connected;

} nicstar_rxclient;

typedef  struct {
    word_t  next;		/* Free list */
    
    dcb_ro_t *owner;		/* DCB of client */
    IO_clp  io;		/* TX IO channel */
    
    AALPod_Mode mode;
    Event_Count ec;		/* Count packets TXed */
    Event_Val   ev;		/* Count packets enqueued */
    
    struct rbuf rb[NICSTAR_TXQLEN];
    
    uint32_t vci;
    Nicstar_st *st;
    
#define NICSTAR_NTXCLIENTS 20
} nicstar_txclient;

typedef struct nicstar_vcimap {
    bool_t tx;
    bool_t rx;
    
    nicstar_rxclient* rxclient;
    struct rbuf rbuf;
} nicstar_vcimap;


#define NUM_VCI (4096)  /* Size of Rx conn table, indexed by VCI only */
#define NUM_VCI_BITS (12)  /* Num sig bits */

struct _Nicstar_st {
    
    int index;                         /* Device index */
    unsigned long iobase, membase;     /* I/O and memory addresses */
    unsigned char modid, revid;        /* PCI module and revision ID */
    PCIBios_Bus pci_bus;
    PCIBios_DevFn pci_devfn;           /* PCI params */
    int irq;                           /* IRQ line */
    
    condition_t scqfull_waitc;
    
    nicstar_rsqe *rx_statq, *rx_statq_last;
    volatile nicstar_rsqe *rx_statq_next;
    caddr_t rx_statq_orig;
    nicstar_tsi *tx_statq, *tx_statq_next, *tx_statq_last;
    caddr_t tx_statq_orig;
    
    caddr_t lg_bufs, lg_bufs_limit;
    caddr_t sm_bufs, sm_bufs_limit;
    int num_reserve_bufs;
    Stretch_clp str;
    
#ifdef PARANOID_NICSTAR
    bool_t buf_table[NUM_LG_BUFS];  
#endif
    
    nicstar_tbd *scq, *scq_next, *scq_last, *scq_tail;
    caddr_t scq_orig;
    volatile int scq_full;
    
    struct atm_dev *dev;
    nicstar_vcimap vci_map[NUM_VCI];
    
    nicstar_scq_shadow scq_shadow[SCQ_ENTRIES];
    
    irq_stub_st      stub;
    uint64_t         ints;	        /* No of times stub called */
    
    Event_Count      event;	        /* Interrupt Event Count */
    Event_Val        ack;

    nicstar_rxclient rxclient[NICSTAR_NRXCLIENTS];
    word_t rxclientfreelist;

    nicstar_txclient txclient[NICSTAR_NTXCLIENTS];
    word_t txclientfreelist;

    unsigned char tmp_cell[52];
    
    uint32_t rx_count, tx_count, tx_ack_count;

    mutex_t mu;    /* For changing general state */
    mutex_t conn_mu;   /* For changing connection state */
    mutex_t reg_mu;  /* For accessing IDT registers */

    uint32_t sm_buf, lg_buf;

    uint32_t num_clients;

    IOEntry_clp entry;

    bool_t allow_client_master;
    int default_num_buffers;
    bool_t facistly_disable_slowpokes;

    bool_t rev_C3;

    IDCCallback_cl iocb_cl;
    AALPod_cl      aalpod_cl;

};

void nicstar_open(Nicstar_st* st, AALPod_Dir mode,short vpi, int vci);
int nicstar_send_aal5(Nicstar_st* st, int vci, struct rbuf* r, bool_t raw);
int nicstar_send_aal0(Nicstar_st* st, int vci, struct rbuf* r);
int nicstar_send_aal34(Nicstar_st* st, int vci, struct rbuf* r);

AALPod_clp nicstar_aal5_init(Nicstar_st *st);


void InterruptThread_m (Nicstar_st *st);
void push_rxrbuf(Nicstar_st *st, nicstar_rxclient* rclient, 
		struct rbuf* r);

nicstar_rxclient* nicstar_allocrxclient (Nicstar_st *st);

void nicstar_freerxclient (Nicstar_st *st, nicstar_rxclient*);

nicstar_txclient* nicstar_alloctxclient (Nicstar_st *st);

void nicstar_freetxclient (Nicstar_st *st, nicstar_txclient*);



#endif 
