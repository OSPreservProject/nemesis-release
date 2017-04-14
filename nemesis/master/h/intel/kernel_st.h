/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Kernel state record
** 
** FUNCTIONAL DESCRIPTION:
** 
**      State kept by the scheduler - scheduling
**	queues, lists of domains, etc.
** 
** ENVIRONMENT: 
**
**      Runs as both privileged domain and SWINT handler.
** 
**
*/

#ifndef _kernel_st_h_
#define _kernel_st_h_

#include <autoconf/measure_trace.h>
#include <autoconf/callpriv.h>

#include <dcb.h>

#ifdef __LANGUAGE_C

#include <pqueues.h>
#include <Timer.ih>

#include <Mem.ih>
#include <Channel.ih>
#include <Event.ih>

#ifdef CONFIG_NTSC_SCHED_ACCOUNT
#include <Account.ih>
#endif
#ifdef CONFIG_MEASURE_TRACE
#include <Trace.ih>
#endif

/* How big is the ring buffer for the console? */
#define CONSOLEBUF_SIZE 2048

/*
 * Fifo for domains with new events pending
 */
typedef struct _DcbFifo_t DcbFifo_t;
struct _DcbFifo_t {
  uint32_t	head;
  uint32_t	tail;
  dcb_ro_t      *q[ CFG_UNBLOCK_QUEUE_SIZE ];
};

/*
 * Heap for queues of scheduling domains
 */
typedef PQ_STRUCT(SDom_t, CFG_DOMAINS) SDomHeap_t;
#define PQ_VAL(_sdom) (((SDom_t *)(_sdom))->deadline)
#undef  PQ_SET_IX
#define PQ_SET_IX(_sdom, _ix) (((SDom_t *)(_sdom))->pq_index = (_ix))
#define PQ_IX(_sdom) (((SDom_t *)(_sdom))->pq_index)

/*
 * Kernel state structure
 */
typedef struct _kernel_st kernel_st;

/* Type for the PIC interrupt mask register shadow */
typedef volatile uint32_t int_mask_reg_t;

/* Type for k_event system call defined in kernel.c */
typedef bool_t (*k_eventfn)(dcb_ro_t *rop, Channel_RX rx, Event_Val inc);

/* Interrupt handling stubs are passed their state pointer and a kernel
 * info pointer */
typedef struct {
    k_eventfn       k_event;    /* address of syscall to send an event */
    int_mask_reg_t *int_mask;   /* address of PIC interrupt mask reg shadow */
    bool_t         *reschedule; /* set this to cause reschedule on stub exit */
    /* ... etc etc */
} kinfo_t;
typedef void (*stubfn)(void *state, const kinfo_t *kinfo);
typedef int  (*callprivfn) (void *state, dcb_ro_t* rop, 
			    void *arg1, void* arg2, void* arg3);

/* The kernel state structure has a pointer to an array of N_IRQS of
 * these irq_desc structures. */
typedef struct {
    stubfn stub;
    void *state;
} irq_desc;

typedef struct {
    callprivfn cpfun;
    void *state;
} callpriv_desc;

struct _kernel_st {
    dcb_ro_t   *cur_dcb;	/* 00 address of current domain's dcb_ro    */
    SDom_t     *cur_sdom;	/* 04 Currently running sdom	            */
    
    context_t  *ccxs;		/* 08 address of current context slot	    */
    uint32_t	bad_slot;       /* 0C on error, the no of the bad ctxt slot */
    uint32_t	bad_ncxs;       /* 10 on error, the max valid ctxt slot no  */
    uint32_t    cs;		/* 14 the CS of the saved context           */

    callpriv_desc *cptable;     /* 18 Address of kernel's callpriv table */

    uint8_t    *pdom;		/* current pdom contents */
    uint32_t	ps;		/* current psmask */
    
    DcbFifo_t	domq;		/* Fifo for domains with events	*/
    
    Timer_clp	t;		/* Timer closure pointer	*/
    
    uint64_t	next;		/* Next ID to allocate		*/
    uint64_t	next_optm;	/* Next optimistic dcb to run	*/
    
    Time_ns     lastopt;        /* Time last optimistic domain ran */

    SDomHeap_t	run;		/* Runnable domains queue	*/
    SDomHeap_t	wait;		/* Waiting domains queue	*/
    SDomHeap_t	blocked;	/* Blocked domains queue	*/
    
    irq_desc	*irq_dispatch;  /* Address of kernel's irq_dispatch table */

#ifdef CONFIG_NTSC_SCHED_ACCOUNT
    Account_clp account;        /* accounting package */
#endif

#ifdef CONFIG_MEASURE_TRACE
    Trace_clp   trace;          /* meaure tracing package */
#endif

    char       *command_line;   /* Pointer to kernel cmdline, or NULL
				   if none was provided */

    word_t      memory;         /* size of memory, in bytes (for chain) */

    /* Bootstrap memory information (XXX would be nicer somewhere else) */
    Mem_PMem    allmem;         /* Pointer to physical memory region info */
    Mem_Map     memmap;         /* Pointer to bootstrap va->pa mappings   */

    /* Memory management info for ntsc_trans|map|unmap etc */
    word_t      *l1_pa;         /* paddr of the level 1 translation table */
    word_t      *l1_va;         /* vaddr of the level 1 translation table */
    word_t      *l2tab;         /* vaddr of the l2 virtual table          */
    word_t      *augtab;        /* array of "augmented" PTEs [extra prot] */
    word_t       augidx;        /* next free index into the above array   */ 
    bool_t       mmu_ok;        /* True if the misc mmgmt vals are valid  */

    uint32_t	cxxx;		/* NTSC console output (now unused) XXX */

    bool_t      reschedule;     /* Set by stubs to request a reschedule */

    struct {
	uint8_t buf[CONSOLEBUF_SIZE];
	uint32_t volatile head;
	uint32_t volatile tail;
	Domain_ID current_dom;
	bool_t buffer_output;   /* Output to console buffer? */
	bool_t local_output;    /* Output to local_vga?      */
	bool_t serial_output;   /* Output to serial?         */
    } console;                  /* Console output buffer and switches     */
    struct {
	uint32_t baseaddr;
	uint32_t width;
	uint32_t height;
    } local_vga;
    struct {
	uint32_t baseaddr;
	uint32_t params;
    } serial;
    struct {
	bool_t local;   /* Enable blue screen of death */
	bool_t serial;  /* Enable serial crashdumps and debugging */
    } crash;
};

extern kernel_st	k_st;

#endif /* __LANGUAGE_C */

#define KST_DCB         0x00
#define KST_SDOM        0x04
#define KST_CCXS	0x08
#define KST_BAD_SLOT	0x0C
#define KST_BAD_NCXS	0x10
#define KST_CS          0x14
#define KST_CALLPRIV    0x18

/* XXX this probably shouldn't be here */
#define N_CALLPRIVS 16


/*
**  k_mmgmt numbers (NB: must be in the same order as the 
**  enumeration defined in Mem.if).
*/
/* XXX this probably shouldn't be here either */
#define	MM_K_TNV  0x0
#define MM_K_UNA  0x1
#define MM_K_ACV  0x2
#define MM_K_INST 0x3
#define MM_K_FOE  0x4
#define MM_K_FOW  0x5
#define MM_K_FOR  0x6
#define MM_K_PAGE 0x7
#define MM_K_BPT  0x8



#endif /* _kernel_st_h_ */
