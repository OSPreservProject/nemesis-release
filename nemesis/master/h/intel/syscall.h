/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      h/intel
**
** FUNCTIONAL DESCRIPTION:
** 
**      Interrupts numbers, etc, for intel system calls.
** 
** ENVIRONMENT: 
**
**      Header file used by lib/static/system/syscall_intel*
** 
** FILE :	syscall.h
** CREATED :	Tue Nov  4 1997
** AUTHOR :	Steven Hand (Steven.Hand@cl.cam.ac.uk)
** ID : 	$Id: syscall.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

/* public system calls are between 0x30 and 0x3F */
#define NTSC_REBOOT_ENTRY     0x30
#define NTSC_RFA_ENTRY        0x31
#define NTSC_RFA_RESUME_ENTRY 0x32
#define NTSC_RFA_BLOCK_ENTRY  0x33
#define NTSC_BLOCK_ENTRY      0x34
#define NTSC_YIELD_ENTRY      0x35
#define NTSC_SEND_ENTRY       0x36
#define NTSC_MAP_ENTRY        0x37
#define NTSC_NAIL_ENTRY       0x38
#define NTSC_PROT_ENTRY       0x39
#define NTSC_GBLPROT_ENTRY    0x3A
#define NTSC_PUTCONS_ENTRY    0x3B 
#define NTSC_DBGSTOP_ENTRY    0x3C
		    
/* private system calls are between 0x40 and 0x4F */
#define NTSC_SWPIPL_ENTRY     0x40
#define NTSC_UNMASK_IRQ_ENTRY 0x41
#define NTSC_KEVENT_ENTRY     0x42
#define NTSC_ACTDOM_ENTRY     0x43
#define NTSC_WPTBR_ENTRY      0x44
#define NTSC_FLUSHTLB_ENTRY   0x45
#define NTSC_WRPDOM_ENTRY     0x46  
#define NTSC_MASK_IRQ_ENTRY   0x47
#define NTSC_SETDBGREG_ENTRY  0x48
#define NTSC_ENTKERN_ENTRY    0x49
#define NTSC_LEAVEKERN_ENTRY  0x4A
#define NTSC_CALLPRIV_ENTRY   0x4B
#define NTSC_CHAIN_ENTRY      0x4C
#define NTSC_EVSEL_ENTRY      0x4D

/* strings for the below (for inline stuff) */
#define STRING(_x) #_x
#define NTSC_REBOOT_STRING     STRING(0x30)
#define NTSC_RFA_STRING        STRING(0x31)
#define NTSC_RFA_RESUME_STRING STRING(0x32)
#define NTSC_RFA_BLOCK_STRING  STRING(0x33)
#define NTSC_BLOCK_STRING      STRING(0x34)
#define NTSC_YIELD_STRING      STRING(0x35)
#define NTSC_SEND_STRING       STRING(0x36)
#define NTSC_MAP_STRING        STRING(0x37)
#define NTSC_NAIL_STRING       STRING(0x38)
#define NTSC_PROT_STRING       STRING(0x39)
#define NTSC_GBLPROT_STRING    STRING(0x3A)
#define NTSC_PUTCONS_STRING    STRING(0x3B)
#define NTSC_DBGSTOP_STRING    STRING(0x3C)
		    
#define NTSC_SWPIPL_STRING     STRING(0x40)
#define NTSC_UNMASK_IRQ_STRING STRING(0x41)
#define NTSC_KEVENT_STRING     STRING(0x42)
#define NTSC_ACTDOM_STRING     STRING(0x43)
#define NTSC_WPTBR_STRING      STRING(0x44)
#define NTSC_FLUSHTLB_STRING   STRING(0x45)
#define NTSC_PGINIT_STRING     STRING(0x46)
#define NTSC_WRPDOM_STRING     STRING(0x46)
#define NTSC_MASK_IRQ_STRING   STRING(0x47)
#define NTSC_SETDBGREG_STRING  STRING(0x48)
#define NTSC_ENTKERN_STRING    STRING(0x49)
#define NTSC_LEAVEKERN_STRING  STRING(0x4A)
#define NTSC_CALLPRIV_STRING   STRING(0x4B)
#define NTSC_CHAIN_STRING      STRING(0x4C)
#define NTSC_EVSEL_STRING      STRING(0x4D)

#define NUM_SYSCALLS           0x20   /* total space for syscalls */


#endif /* _SYSCALL_H_ */
