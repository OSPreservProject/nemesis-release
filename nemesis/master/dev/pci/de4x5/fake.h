/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      dev/pci/de4x5
**
** FUNCTIONAL DESCRIPTION:
** 
**      Supplies defines and types that the driver code expects to 
**      have got from the Linux header files.
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
** FILE :	fake.h
** CREATED :	Wed Sep  4 1996
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: fake.h 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
** 
**
*/

#ifndef _FAKE_H_
#define _FAKE_H_


/* Typedefs needed by this driver */
#define s16 	int16_t
#define u16 	uint16_t
#define s32 	int32_t
#define u32 	uint32_t
#define u_char 	unsigned char
#define u_short	unsigned short
#define u_int 	unsigned int
#define u_long 	unsigned long

/*
 * Replacements for "Kernel" routines
 */
#include <compat.h>
#define printk  	printf
#if defined(CONFIG_MEMSYS_EXPT)
#define kmalloc(_x,_y)  Heap$Malloc(nailedHeap,_x)
#elif defined(CONFIG_MEMSYS_STD) || defined(CONFIG_MEMSYS_121P)
#define kmalloc(s,f) 	malloc(s)
#else
#error You must include autoconf/memsys.h before this file
#endif
#define kfree   	free
#define jiffies         NOW()
#define request_region(b,s,a) 
#define release_region(b,s) 
#define check_region(b,s) 0
#define barrier()       __asm__ __volatile__("": : :"memory")
#define cli()           Threads$EnterCS(Pvs(thds), False)
#define sti()           Threads$LeaveCS(Pvs(thds))
#define request_irq(irq,func,shared,adapter,dev) 0
#define free_irq(irq, dev)


/*#define pcibios_present() 0*/

/* size of an Ethernet address, in bytes */
#define ETH_ALEN 6

/* drag in the needed bit-related operations */
#include <bitops.h>

/* endianness */
#if defined(LITTLE_ENDIAN)
#define le32_to_cpu(x) x
#define le16_to_cpu(x) x
#elif defined(BIG_ENDIAN)
#define le32_to_cpu(x) \
  ({ uint32_t l = x;	(( ((l) & 0x000000ff) << 24 ) | \
			( (((l) & 0xff000000) >> 24 ) & 0x000000ff) | \
			( (((l) & 0x00ff0000) >> 8 ) & 0x0000ff00)  | \
			( ( (l) & 0x0000ff00) << 8 )); })
#define le16_to_cpu(x) \
  ({ uint16_t l = x;   \
	((l & 0xff00) >> 8) | \
	((l & 0x00ff) << 8)   \
  })
#else
#error You havent defined the endianness!
#endif
#define cpu_to_le32(x) le32_to_cpu(x)
#define cpu_to_le16(x) le16_to_cpu(x)


/* unaligned accesses
 * The linux function copes with unaligned reads of 1,2,4,8 byte
 * quantities. This one only does 2 bytes. */
#define get_unaligned(x) ({uint8_t *t = (void*)x; (*t | ((*(t+1))<<8)); })


/*
 * A few errnos */
#define   ENXIO            6      /* No such device or address */
#define   EIO              5      /* I/O error */
#define   ENODEV          19      /* No such device */
#define   ENOMEM          12      /* Out of memory */
#define   EAGAIN          35      /* Try again */

/*
 * Better not ever look at these!
 */
struct pt_regs;
struct ifreq;
struct ifmap;
struct sk_buff {
    bool_t	valid;
    uint32_t	nrecs;
    IO_Rec	rec[5];
    IO_clp	io;
};

/*
 *      Ethernet statistics collection data. 
 */
 
struct net_device_stats
{
        int     rx_packets;             /* total packets received       */
        int     tx_packets;             /* total packets transmitted    */
        int     rx_errors;              /* bad packets received         */
        int     tx_errors;              /* packet transmit problems     */
        int     rx_dropped;             /* no space in linux buffers    */
        int     tx_dropped;             /* no space available in linux  */
        int     multicast;              /* multicast packets received   */
        int     collisions;

        /* detailed rx_errors: */
        int     rx_length_errors;
        int     rx_over_errors;         /* receiver ring buff overflow  */
        int     rx_crc_errors;          /* recved pkt with crc error    */
        int     rx_frame_errors;        /* recv'd frame alignment error */
        int     rx_fifo_errors;         /* recv'r fifo overrun          */
        int     rx_missed_errors;       /* receiver missed packet       */

        /* detailed tx_errors */
        int     tx_aborted_errors;
        int     tx_carrier_errors;
        int     tx_fifo_errors;
        int     tx_heartbeat_errors;
        int     tx_window_errors;

	/* Nemesis specific */
	int	tx_queued;		/* number of queue_pkt() calls */
};

/* XXX PRB Fake struct device for now */
/*
 * The DEVICE structure.
 * Actually, this whole structure is a big mistake.  It mixes I/O
 * data with strictly "high-level" data, and it has to know about
 * almost every data structure used in the INET module.  
 */
struct device 
{

  /*
   * This is the first field of the "visible" part of this structure
   * (i.e. as seen by users in the "Space.c" file).  It is the name
   * the interface.
   */
  char                    *name;

  /* I/O specific fields - FIXME: Merge these and struct ifmap into one */
  unsigned long           rmem_end;             /* shmem "recv" end     */
  unsigned long           rmem_start;           /* shmem "recv" start   */
  unsigned long           mem_end;              /* shared mem end       */
  unsigned long           mem_start;            /* shared mem start     */
  unsigned long           base_addr;            /* device I/O address   */
  unsigned char           irq;                  /* device IRQ number    */

  /* Low-level status flags. */
  volatile unsigned char  start,                /* start an operation   */
                          interrupt;            /* interrupt arrived    */
  unsigned long           tbusy;                /* transmitter busy must be long for bitops */

  struct device           *next;

  /* The device initialization function. Called only once. */
  int                     (*init)(struct device *dev);

  /* Some hardware also needs these fields, but they are not part of the
     usual set specified in Space.c. */
  unsigned char           if_port;              /* Selectable AUI, TP,..*/
  unsigned char           dma;                  /* DMA channel          */

  struct  net_device_stats * (*get_stats)(struct device *dev);

  /*
   * This marks the end of the "visible" part of the structure. All
   * fields hereafter are internal to the system, and may change at
   * will (read: may be cleaned up at will).
   */
  /* These may be needed for future network-power-down code. */
  unsigned long           trans_start;  /* Time (in jiffies) of last Tx */
  unsigned long           last_rx;      /* Time of last Rx              */

  unsigned short          flags;        /* interface flags (a la BSD)   */
  unsigned short          family;       /* address family ID (AF_INET)  */
  unsigned short          metric;       /* routing metric (not used)    */
  unsigned short          mtu;          /* interface MTU value          */
  unsigned short          type;         /* interface hardware type      */
  unsigned short          hard_header_len;      /* hardware hdr length  */
  void                    *priv;        /* pointer to private data      */

  /* Interface address info. */
#define MAX_ADDR_LEN 7
  unsigned char           broadcast[MAX_ADDR_LEN];      /* hw bcast add */
  unsigned char           pad;                          /* make dev_addr aligned
 to 8 bytes */
  unsigned char           dev_addr[MAX_ADDR_LEN];       /* hw address   */
  unsigned char           addr_len;     /* hardware address length      */
  unsigned long           pa_addr;      /* protocol address             */
  unsigned long           pa_brdaddr;   /* protocol broadcast addr      */
  unsigned long           pa_dstaddr;   /* protocol P-P other side addr */
  unsigned long           pa_mask;      /* protocol netmask             */
  unsigned short          pa_alen;      /* protocol address length      */

  struct dev_mc_list     *mc_list;      /* Multicast mac addresses      */
  int                    mc_count;      /* Number of installed mcasts   */
  
#if 0
  struct ip_mc_list      *ip_mc_list;   /* IP multicast filter chain    */
  __u32                 tx_queue_len;   /* Max frames per queue allowed */
#endif 0

  PCIBios_clp             pcibios;

  /* Pointers to interface service routines. */
  int                     (*open)(struct device *dev);
  int                     (*stop)(struct device *dev);
  int                     (*hard_start_xmit) (struct sk_buff *skb,
                                              struct device *dev);
  int                     (*hard_header) (struct sk_buff *skb,
                                          struct device *dev,
                                          unsigned short type,
                                          void *daddr,
                                          void *saddr,
                                          unsigned len);
  int                     (*rebuild_header)(void *eth, struct device *dev,
                                unsigned long raddr, struct sk_buff *skb);

  Event_Count		cardReady;
};

struct timeout {
    Event_Count ec;
    Event_Val   waitfor;
    Thread_clp  thread;
    Time_ns     time;
    void        (*fn)();
    void        *arg;
};


/* stuff from linux/init.h - just disable the initfunc stuff */
#define __init
#define __initdata
#define __initfunc(__arginit) __arginit
/* For assembly routines */
#define __INIT
#define __FINIT
#define __INITDATA


/* module stuff */
#define MOD_INC_USE_COUNT
#define MOD_DEC_USE_COUNT

#endif /* _FAKE_H_ */
