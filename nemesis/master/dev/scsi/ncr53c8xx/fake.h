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
**      dev/scsi/ncr53c8xx
**
** FUNCTIONAL DESCRIPTION:
** 
**      What does it do?
** 
** ENVIRONMENT: 
**
**      Where does it do it?
** 
** FILE :	fake.h
** CREATED :	Mon Oct  7 1996
** AUTHOR :	Paul Barham (Paul.Barham@cl.cam.ac.uk)
** ID : 	$Id: fake.h 1.1 Thu, 18 Feb 1999 14:19:15 +0000 dr10009 $
** 
**
*/

#ifndef _FAKE_H_
#define _FAKE_H_

/* XXX PRB Typedefs needed by this driver */
#define s16 	int16_t
#define u16 	uint16_t
#define s32 	int32_t
#define u32 	uint32_t
#define u_char 	unsigned char
#define u_short	unsigned short
#define u_int 	unsigned int
#define u_long 	unsigned long

/*
** Define the BSD style type u_int32.
*/
typedef uint32_t u_int32;


/*
 * Replacements for "Kernel" routines
 */
#include <compat.h>
#define printk  	printf
#define jiffies         (NOW())
#define udelay(us)      PAUSE(MICROSECS(us))
#define HZ              SECONDS(1)
#define request_region(b,s,a) 
#define release_region(b,s) 
#define check_region(b,s) 0
#define barrier()       __asm__ __volatile__("": : :"memory")

#if 0
#define cli()            VP_ActivationsOff(Pvs(vp))
#define sti()            VP_ActivationsOn(Pvs(vp))
#define save_flags(mode) 	 mode = VP_ActivationMode(Pvs(vp))
#define restore_flags(mode) 			\
do {						\
  if (mode)					\
  {						\
    VP_ActivationsOn (Pvs(vp));			\
    if (VP_EventsPending (Pvs(vp)))		\
      VP_RFA (Pvs(vp));				\
  }						\
} while (0)
#endif 

#define cli()
#define sti()
#define save_flags(m)
#define restore_flags(m)

#define panic(why)           ({eprintf(why) ; ntsc_dbgstop(); })


#define request_irq(irq,func,shared,adapter,dev) 0

/*
**  	This is the minimal amount of Linux SCSI gloop to get THIS 
**	driver to go... this'll go in a module eventually
*/


#define DID_OK          0x00 /* NO error                                */
#define DID_NO_CONNECT  0x01 /* Couldn't connect before timeout period  */
#define DID_BUS_BUSY    0x02 /* BUS stayed busy through time out period */
#define DID_TIME_OUT    0x03 /* TIMED OUT for other reason              */
#define DID_BAD_TARGET  0x04 /* BAD target.                             */
#define DID_ABORT       0x05 /* Told to abort for some other reason     */
#define DID_PARITY      0x06 /* Parity error                            */
#define DID_ERROR       0x07 /* Internal error                          */
#define DID_RESET       0x08 /* Reset by somebody.                      */
#define DID_BAD_INTR    0x09 /* Got an interrupt we weren't expecting.  */ 
#define DRIVER_OK       0x00 /* Driver status                           */ 

/*
 *  These indicate the error that occurred, and what is available.
 */

#define DRIVER_BUSY         0x01
#define DRIVER_SOFT         0x02
#define DRIVER_MEDIA        0x03
#define DRIVER_ERROR        0x04    

#define DRIVER_INVALID      0x05
#define DRIVER_TIMEOUT      0x06
#define DRIVER_HARD         0x07
#define DRIVER_SENSE	    0x08

#define SUGGEST_RETRY       0x10
#define SUGGEST_ABORT       0x20 
#define SUGGEST_REMAP       0x30
#define SUGGEST_DIE         0x40
#define SUGGEST_SENSE       0x80
#define SUGGEST_IS_OK       0xff

#define DRIVER_MASK         0x0f
#define SUGGEST_MASK        0xf0

#define MAX_COMMAND_SIZE    12

/*
 *  SCSI command sets
 */

#define SCSI_UNKNOWN    0
#define SCSI_1          1
#define SCSI_1_CCS      2
#define SCSI_2          3

/*
 *  Every SCSI command starts with a one byte OP-code.
 *  The next byte's high three bits are the LUN of the
 *  device.  Any multi-byte quantities are stored high byte
 *  first, and may have a 5 bit MSB in the same byte
 *  as the LUN.
 */

/*
 *      Manufacturers list
 */

#define SCSI_MAN_UNKNOWN     0
#define SCSI_MAN_NEC         1
#define SCSI_MAN_TOSHIBA     2
#define SCSI_MAN_NEC_OLDCDR  3
#define SCSI_MAN_SONY        4
#define SCSI_MAN_PIONEER     5

/*
 *  As the scsi do command functions are intelligent, and may need to
 *  redo a command, we need to keep track of the last command
 *  executed on each one.
 */

#define WAS_RESET       0x01
#define WAS_TIMEDOUT    0x02
#define WAS_SENSE       0x04
#define IS_RESETTING    0x08
#define IS_ABORTING     0x10
#define ASKED_FOR_SENSE 0x20
/*
 * These are the return codes for the abort and reset functions.  The mid-level
 * code uses these to decide what to do next.  Each of the low level abort
 * and reset functions must correctly indicate what it has done.
 * The descriptions are written from the point of view of the mid-level code,
 * so that the return code is telling the mid-level drivers exactly what
 * the low level driver has already done, and what remains to be done.
 */

/* We did not do anything.  
 * Wait some more for this command to complete, and if this does not work, 
 * try something more serious. */ 
#define SCSI_ABORT_SNOOZE 0

/* This means that we were able to abort the command.  We have already
 * called the mid-level done function, and do not expect an interrupt that 
 * will lead to another call to the mid-level done function for this command */
#define SCSI_ABORT_SUCCESS 1

/* We called for an abort of this command, and we should get an interrupt 
 * when this succeeds.  Thus we should not restore the timer for this
 * command in the mid-level abort function. */
#define SCSI_ABORT_PENDING 2

/* Unable to abort - command is currently on the bus.  Grin and bear it. */
#define SCSI_ABORT_BUSY 3

/* The command is not active in the low level code. Command probably
 * finished. */
#define SCSI_ABORT_NOT_RUNNING 4

/* Something went wrong.  The low level driver will indicate the correct
 * error condition when it calls scsi_done, so the mid-level abort function
 * can simply wait until this comes through */
#define SCSI_ABORT_ERROR 5

/* We do not know how to reset the bus, or we do not want to.  Bummer.
 * Anyway, just wait a little more for the command in question, and hope that
 * it eventually finishes.  If it never finishes, the SCSI device could
 * hang, so use this with caution. */
#define SCSI_RESET_SNOOZE 0

/* We do not know how to reset the bus, or we do not want to.  Bummer.
 * We have given up on this ever completing.  The mid-level code will
 * request sense information to decide how to proceed from here. */
#define SCSI_RESET_PUNT 1

/* This means that we were able to reset the bus.  We have restarted all of
 * the commands that should be restarted, and we should be able to continue
 * on normally from here.  We do not expect any interrupts that will return
 * DID_RESET to any of the other commands in the host_queue, and the mid-level
 * code does not need to do anything special to keep the commands alive. 
 * If a hard reset was performed then all outstanding commands on the
 * bus have been restarted. */
#define SCSI_RESET_SUCCESS 2

/* We called for a reset of this bus, and we should get an interrupt 
 * when this succeeds.  Each command should get its own status
 * passed up to scsi_done, but this has not happened yet. 
 * If a hard reset was performed, then we expect an interrupt
 * for *each* of the outstanding commands that will have the
 * effect of restarting the commands.
 */
#define SCSI_RESET_PENDING 3

/* We did a reset, but do not expect an interrupt to signal DID_RESET.
 * This tells the upper level code to request the sense info, and this
 * should keep the command alive. */
#define SCSI_RESET_WAKEUP 4

/* The command is not active in the low level code. Command probably
   finished. */
#define SCSI_RESET_NOT_RUNNING 5

/* Something went wrong, and we do not know how to fix it. */
#define SCSI_RESET_ERROR 6

#define SCSI_RESET_SYNCHRONOUS          0x01
#define SCSI_RESET_ASYNCHRONOUS         0x02
#define SCSI_RESET_SUGGEST_BUS_RESET    0x04
#define SCSI_RESET_SUGGEST_HOST_RESET   0x08
/*
 * This is a bitmask that is ored with one of the above codes.
 * It tells the mid-level code that we did a hard reset.
 */
#define SCSI_RESET_BUS_RESET 0x100
/*
 * This is a bitmask that is ored with one of the above codes.
 * It tells the mid-level code that we did a host adapter reset.
 */
#define SCSI_RESET_HOST_RESET 0x200
/*
 * Used to mask off bits and to obtain the basic action that was
 * performed.  
 */
#define SCSI_RESET_ACTION   0xff

/*
 *  Use these to separate status msg and our bytes
 */

#define status_byte(result) (((result) >> 1) & 0x1f)
#define msg_byte(result)    (((result) >> 8) & 0xff)
#define host_byte(result)   (((result) >> 16) & 0xff)
#define driver_byte(result) (((result) >> 24) & 0xff)
#define suggestion(result)  (driver_byte(result) & SUGGEST_MASK)

#define sense_class(sense)  (((sense) >> 4) & 0x7)
#define sense_error(sense)  ((sense) & 0xf)
#define sense_valid(sense)  ((sense) & 0x80);


typedef struct scsi_device {
    struct scsi_device * next;      /* Used for linked list */

    unsigned char id, lun, channel;

    unsigned int manufacturer;      /* Manufacturer of device, for using 
                                     * vendor-specific cmd's */
    int attached;                   /* # of high level drivers attached to 
                                     * this */
    int access_count;               /* Count of open channels/mounts */
    struct wait_queue * device_wait;/* Used to wait if device is busy */
    struct Scsi_Host * host;
    void (*scsi_request_fn)(void);  /* Used to jumpstart things after an 
                                     * ioctl */
    struct scsi_cmnd *device_queue; /* queue of SCSI Command structures */
    void *hostdata;                 /* available to low-level driver */
    char type;
    char scsi_level;
    char vendor[8], model[16], rev[4];
    unsigned char current_tag;      /* current tag */
    unsigned char sync_min_period;  /* Not less than this period */
    unsigned char sync_max_offset;  /* Not greater than this offset */
    unsigned char queue_depth;      /* How deep a queue to use */

    unsigned writeable:1;
    unsigned removable:1; 
    unsigned random:1;
    unsigned has_cmdblocks:1;
    unsigned changed:1;             /* Data invalid due to media change */
    unsigned busy:1;                /* Used to prevent races */
    unsigned lockable:1;            /* Able to prevent media removal */
    unsigned borken:1;              /* Tell the Seagate driver to be 
                                     * painfully slow on this device */ 
    unsigned tagged_supported:1;    /* Supports SCSI-II tagged queuing */
    unsigned tagged_queue:1;        /* SCSI-II tagged queuing enabled */
    unsigned disconnect:1;          /* can disconnect */
    unsigned soft_reset:1;          /* Uses soft reset option */
    unsigned sync:1;                /* Negotiate for sync transfers */
    unsigned single_lun:1;          /* Indicates we should only allow I/O to
                                     * one of the luns for the device at a 
                                     * time. */
    unsigned was_reset:1;           /* There was a bus reset on the bus for 
                                     * this device */
    unsigned expecting_cc_ua:1;     /* Expecting a CHECK_CONDITION/UNIT_ATTN
                                     * because we did a bus reset. */
} Scsi_Device;

struct Scsi_Device_Template
{
    struct Scsi_Device_Template * next;
    const char * name;
    const char * tag;
    long * usage_count;		  /* Used for loadable modules */
    unsigned char scsi_type;
    unsigned char major;
    unsigned char nr_dev;	  /* Number currently attached */
    unsigned char dev_noticed;	  /* Number of devices detected. */
    unsigned char dev_max;	  /* Current size of arrays */
    unsigned blk:1;		  /* 0 if character device */
    int (*detect)(Scsi_Device *); /* Returns 1 if we can attach this device */
    int (*init)(void);		  /* Sizes arrays based upon number of devices
		   *  detected */
    void (*finish)(void);	  /* Perform initialization after attachment */
    int (*attach)(Scsi_Device *); /* Attach devices to arrays */
    void (*detach)(Scsi_Device *);
};

struct scatterlist {
    char *  address;    /* Location data is to be transferred to */
    char * alt_address; /* Location of actual if address is a 
                         * dma indirect buffer.  NULL otherwise */
    unsigned int length;
};

typedef struct scsi_pointer {
    char * ptr;                     /* data pointer */
    int this_residual;              /* left in this buffer */
    struct scatterlist *buffer;     /* which buffer */
    int buffers_residual;           /* how many buffers left */
    
    volatile int Status;
    volatile int Message;
    volatile int have_data_in;
    volatile int sent_command;
    volatile int phase;
} Scsi_Pointer;


typedef struct scsi_cmnd {
    struct Scsi_Host * host;
    Scsi_Device * device;
    unsigned char target, lun, channel;
    struct scsi_cmnd *next, *prev;

    Event_Count ec;
    SCSICallback_clp scb;

    /* These elements define the operation we are about to perform */
    unsigned char cmnd[12];
    unsigned char cmd_len;

    unsigned request_bufflen;   /* Actual request size */
    void * request_buffer;      /* Actual requested buffer */

    /* These elements define the operation we ultimately want to perform */
    unsigned char data_cmnd[12];
    unsigned bufflen;           /* Size of data buffer */
    void *buffer;               /* Data buffer */
    unsigned short use_sg;      /* Number of pieces of scatter-gather */
    unsigned short sglist_len;  /* size of malloc'd scatter-gather list */
 
    unsigned char sense_buffer[16];  /* Sense for this command, if needed */
    
    Time_ns timeout_per_command, timeout_total, timeout;

    unsigned volatile char internal_timeout;

    /* Low-level done function - can be used by low-level driver to point
     *  to completion function.  Not used by mid/upper level code. */
    void (*scsi_done)(struct scsi_cmnd *);  
    void (*done)(struct scsi_cmnd *);  /* Mid-level done function */
 
    Scsi_Pointer SCp;		/* Scratchpad used by some host adapters */
    
    unsigned char * host_scribble; /* The host adapter is allowed to
                                    * call scsi_malloc and get some memory
                                    * and hang it here.  The host adapter
                                    * is also expected to call scsi_free
                                    * to release this memory.  (The memory
                                    * obtained by scsi_malloc is guaranteed
                                    * to be at an address < 16Mb). */
    
    int result;                    /* Status code from lower level driver */
    
    unsigned char tag;             /* SCSI-II queued command tag */

} Scsi_Cmnd;

typedef struct	SHT
{
    struct SHT * next;
    const char *name;
    int (* detect)(struct SHT *); 
    int (*release)(struct Scsi_Host *);
    int (* command)(Scsi_Cmnd *);
    int (* queuecommand)(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
    int (* abort)(Scsi_Cmnd *);
    int (* reset)(Scsi_Cmnd *, unsigned int);
    int can_queue;
    int this_id;
    short unsigned int sg_tablesize;
    short cmd_per_lun;
    unsigned char present;  
    unsigned unchecked_isa_dma:1;
    unsigned use_clustering:1;

} Scsi_Host_Template;

struct Scsi_Host
{
    struct Scsi_Host * next;
    Scsi_Cmnd        * host_queue;
    Scsi_Host_Template * hostt;
    unsigned long last_reset;

    /*
     *	These three parameters can be used to allow for wide scsi,
     *	and for host adapters that support multiple busses 
     *	The first two should be set to 1 more than the actual max id
     *	or lun (i.e. 8 for normal systems).
     */
    unsigned int max_id;
    unsigned int max_lun;
    unsigned int max_channel;

    /*
     * Pointer to a circularly linked list - this indicates the hosts
     * that should be locked out of performing I/O while we have an active
     * command on this host. 
     */
    struct Scsi_Host * block;
    unsigned wish_block:1;

    /* These parameters should be set by the detect routine */
    unsigned char *base;
    unsigned int  io_port;
    unsigned char n_io_port;
    unsigned char irq;
    unsigned char dma_channel;

    /*
     * The rest can be copied from the template, or specifically
     * initialized, as required.
     */
    int this_id;
    int can_queue;
    short cmd_per_lun;
    short unsigned int sg_tablesize;
    unsigned unchecked_isa_dma:1;
    unsigned use_clustering:1;

    unsigned long hostdata[0];  /* Used for storage of host specific stuff */
};


struct timer_list {
    struct timer_list *next;
    Event_Count        ec;
    Thread_clp         thread;
    Time_ns            expires;
    void               (*function)(void *data);
    void              *data;
};


#endif /* _FAKE_H_ */
