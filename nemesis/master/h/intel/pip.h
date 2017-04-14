/*
 *      h/ix86/pip.h
 *      ------------
 *
 * Public information page definition
 *
 * $Id: pip.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

#ifndef _pip_h
#define _pip_h

#include <processor.h>

#define INFO_PAGE_ADDRESS 0x1000

#ifdef __LANGUAGE_C

#define INFO_PAGE (((struct pip *)(INFO_PAGE_ADDRESS))[0])
#define INFO_PAGE_PTR ((struct pip *) INFO_PAGE_ADDRESS)

#include <Time.ih>
#include <nemesis_tgt.h>

/* Placeholder for the mod_data structure. We ought to be only
   declaring this if CONFIG_MODULE_OFFSET_DATA is defined, but
   including config.h in here will cause the whole world to rebuild
   on every 'make config' */
struct _mod_data_rec;

struct pip {
    volatile Time_ns	  now;	     /* 00 Current system time              */
    volatile Time_ns	  alarm;     /* 08 Alarm time                       */
    volatile uint32_t	  pcc;       /* 10 Cycle count at last tick         */
    uint32_t 		  scale;     /* 14 Cycle count scale factor         */
    uint32_t		  cycle_cnt; /* 18 Cycle time in picoseconds        */
    void                **pvsptr;    /* 1C Pointer to cur dom's PVS rec ptr */
    addr_t                ss_tab;    /* 20 SID  -> Stretch table address    */
    addr_t                pd_tab;    /* 24 PDID -> Pdom table address       */
    processor_t		  processor; /* Processor information               */
    word_t                sheartbeat;/* heartbeat -- scheduler passes       */
    word_t                iheartbeat;/* heartbeat -- interrupts             */
    word_t                pheartbeat;/* heartbeat -- memory 'patch'es       */
    word_t                nheartbeat;/* heartbeat -- ntsc calls             */
    struct _mod_data_rec *mod_data;  /* module offsets for gdb              */
    word_t               *l1_va;     /* vaddr of the l1 page table          */
    word_t               *l2tab;     /* vaddr of the l2 virtual table       */
    bool_t                mmu_ok;    /* True iff above two vals are valid   */
    uint64_t      prev_sched_time;   /* previous recorded scheduler time    */
    uint64_t      ntp_time;          /* previous time according to NTPdate  */
    uint64_t      NTPscaling_factor; /* current wall-clock scaling factor   */
};

/* Function to inline the time calculation */

static Time_T __inline__ time_inline(bool_t inkernel) {

    struct pip *st = INFO_PAGE_PTR ;
    uint32_t delta;
    Time_ns now;
    uint32_t incr[2];

    /* If we're not using timer_rdtsc, return the best approximation
       that we have */

    if(!st->scale) 
        return st->now;
    
    /* Need to read two values 'atomically'. They both change together
       (in the kernel, with interrupts disabled). st->pcc is a copy of
       the cycle counter, so is guaranteed to change (provided that
       the ticker interrupt goes off more often than every 2^32 cycles
       - this is approx 20 seconds on a P200). Therefore only check
       for delta changing, since checking for a 32-bit value changing
       is faster than checking for a 64bit value. */

again:
    delta = st->pcc;		
    now = st->now;

    if ((!inkernel ) && (st->pcc != delta)) goto again;

    delta = rpcc() - delta;

    /* Why can't bloody gcc do this for us? Here we calculate

       (st->scale * delta) >> 10

       and leave it in the two 32-bit words of "incr". */

    __asm__ ("mull %3;"
	     "shrdl $0xa, %%edx, %%eax;"
	     "sarl $0xa, %%edx;" :
	     "=d" (incr[1]), "=a" (incr[0]):
	     "a" (delta), "m" (st->scale));
    
    
    return now + *(Time_ns *)incr; 
}

#ifdef TIME_INLINE
#undef Time$Now
#define Time$Now(_time) time_inline(False)
#endif

#endif /* __LANGUAGE_C */

#define PIP_TIMELSW 0x00
#define PIP_TIMEMSW 0x04
#define PIP_PVSPTR  0x1C

#endif /* _pip_h */
