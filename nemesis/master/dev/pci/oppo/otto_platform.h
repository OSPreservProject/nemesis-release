/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      otto_platform.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Platform specific definitions for the Otto driver
** 
** ENVIRONMENT: 
**
**      Internal to module dev/pci/otto
** 
** ID : $Id: otto_platform.h 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
**
*/

#ifndef _OTTO_PLATFORM_H_
#define _OTTO_PLATFORM_H_


#include <nemesis.h>
#include <ntsc.h>
#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>
#include <kernel.h>
#include <stdio.h>
#include <rbuf.h>
#include <ntsc.h>

#define NAS_DEBUG(x)

#define cprintf  printf
#define panic(f, _args...) {eprintf(f);ntsc_dbgstop();}

#ifndef EB164
#define NTSC_MB
#endif

#ifdef EB164
#define BUS_PCI
#define ENTER_BROKEN_INTEL_SECTION() 
#define LEAVE_BROKEN_INTEL_SECTION() 
#endif


#ifdef __IX86__
#define BUS_PCI
#define ENTER_BROKEN_INTEL_SECTION()  ENTER_KERNEL_CRITICAL_SECTION()
#define LEAVE_BROKEN_INTEL_SECTION()  LEAVE_KERNEL_CRITICAL_SECTION()
#endif


/*
 * HIGH kernel critical sections turn off interupts as well as setting supervisor
 * bit in processor
 */

#define HIGH_IPL() \
{                                            \
  word_t old_ipl;                                           \
  old_ipl = ntsc_ipl_high();          
#define LOW_IPL() \
  ntsc_swpipl(old_ipl);                      \
}

#ifdef __alpha__
#define rpcc() \
({ uint32_t __t; __asm__ volatile ("rpcc\t%0" : "=r" (__t) :); __t & 0xFFFFFFFFL; })
#endif
#ifdef __IX86__
#define rpcc() \
({uint32_t __h,__l; __asm__(".byte 0x0f,0x31" :"=a" (__l), "=d" (__h) );__l& 0xFFFFFFFFL; })
#endif

#ifdef __alpha__
#define microdelay(usecs) 			\
{						\
  uint32_t start = rpcc();			\
  uint32_t cycles = usecs<<7;			\
  while ((rpcc()-start) < cycles) ;		\
}
#else 
/* Would be better to use BogoMips here */
#define microdelay(usecs)			\
{						\
    uint32_t i;					\
    i = usecs;					\
    i = i*300;					\
    while (i>0) i--;				\
}
#endif

#if 0
#define limitprintf(l, x) 			\
do {                                  		\
  static Time_ns lasttime;                   	\
  static int limitcount;                  	\
  Time_ns now = NOW();				\
  limitcount++;					\
  if (now > lasttime+(SECONDS(l))) {       	\
    cprintf ("otto: count=%d ", limitcount);	\
    cprintf x;                      		\
    lasttime = now;         			\
  }                                       	\
} while (0)

#define ottodprintf(lev, x)			\
do { 						\
  if (ottodebug >= (lev)) {			\
    cprintf x;					\
  } 						\
} while (0)
#endif /* 0 */

#define limitprintf(l,x)
#define ottodprintf(lev,x)

/* Address munging and memory allocation */
typedef unsigned long vm_offset_t;
#define PHYS_TO_K1(x)   (x)
#define PHYS_TO_K0(x)   (x) 
#define OTTO_SVTOPHY(v) (v)
#define DENSE(a)        ((vm_offset_t)(a) - 0x10000000)

#define wbflush()     NTSC_MB
#define OTTO_CLEAN_DCACHE(addr, len)

/* Memory allocation */
#include <StretchAllocator.ih>
#define KM_ALLOC(addr, type, length) 				\
{								\
  StretchAllocator_clp  salloc;					\
  Stretch_clp           str;					\
  Stretch_Size          size = length;				\
								\
  salloc = NAME_LOOKUP("sys>StretchAllocator",			\
		       Pvs(root),				\
		       StretchAllocator_clp);			\
  str    = StretchAllocator_New(salloc, size);			\
  if (!str) {eprintf("Couldn't get stretch");ntsc_dbgstop();}			\
  (addr) = (type) Stretch_Range(str, &size);			\
  if (!(addr) || size < length) {eprintf("Stretch too small");ntsc_dbgstop();}	\
  Stretch_SetProt(str, VP_ProtDom(Pvs(vp)), 			\
		  SET_ELEM(Stretch_Right_Read) |		\
		  SET_ELEM(Stretch_Right_Write));		\
}

/*
 * Driver Critical sections.
 *
 * Protect shared state by disabling activations and hence preventing
 * ULTS context switches. 
 *
 * N.B.+1
 * 1. ENTER/LEAVE sections are NOT nestable so any call to the ULTS which 
 * is implemented using ENTER/LEAVE will cause BAD results. (e.g. You will
 * suddenly be running with Activations ON)
 *
 * 2. This is not safe with a pre-emptive thread scheduler.  e.g. If
 * you do an EC_ADVANCE in the critical section another thread may run
 * (Murphy's Law says that it is guaranteed to be one which shares the
 * datastructure!)
 * 
 */

#define ENTER() VP_ActivationsOff (Pvs(vp))
#define LEAVE()                         \
{                                       \
  VP_ActivationsOn (Pvs(vp));           \
  if (VP_EventsPending (Pvs(vp)))       \
    VP_RFA (Pvs(vp));                   \
}

/* Locking of state record */
#if 1
#define OTTO_LOCKTMP           int
#define OTTO_LOCKTYPE          mutex_t
#define OTTO_LOCKINIT(lock)    MU_INIT(lock)
#define OTTO_LOCK(lock,prev)   MU_LOCK(lock)
#define OTTO_UNLOCK(lock,prev) MU_RELEASE(lock)
#else /* 0 */
#define OTTO_LOCKTMP           int
#define OTTO_LOCKTYPE          int
#define OTTO_LOCKINIT(lock)    
#define OTTO_LOCK(lock,prev)   ENTER()
#define OTTO_UNLOCK(lock,prev) LEAVE()
#endif /* 0 */

/* Driver stats */
#define OTTO_STATS_INC(x,y)       
#define OTTO_STATS_INC_L(x,y,l)   

#define OTTO_PIOA(_a,_od)    (_a)
#define OTTO_PIOR(_a,_od)    (readl(_a))       /* 32 bit PIO read */
#define OTTO_PIOW(_a,_od,_d) (writel(_d,_a)) /* 32 bit PIO write */

/* NAS Handy PCI structure */
#include <PCIBios.ih>
typedef struct {
    uchar_t irq;
    uchar_t dev_num;
    uchar_t bus_num;
    uchar_t devno;
    uchar_t bridgedevno;
    addr_t base_addr;
    PCIBios_clp pcibios;
} pci_dev_t;

extern pci_dev_t *pci_config;
extern uint32_t dummy;

#define BRIDGE_DEVICE (14<<3)
#define OPPO_DEVICE (0<<3)

#endif /* _OTTO_PLATFORM_H_ */






