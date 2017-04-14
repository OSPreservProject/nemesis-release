/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      h/frames.h
**
** FUNCTIONAL DESCRIPTION:
** 
**      Shared structures and definitions for Frames, NTSC & MMU.
** 
** ENVIRONMENT: 
**
**      Nemesis domain or NTSC.
** 
** FILE :	frames.h
** CREATED :	Wed Mar 11 1998
** AUTHOR :	Steven Hand (Steven.Hand@cl.cam.ac.uk)
** ID : 	$Id: frames.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _FRAMES_H_
#define _FRAMES_H_

#ifdef __LANGUAGE_C

#include <RamTab.ih>

typedef struct _flink_t flink_t;
struct _flink_t {
    flink_t *next; 
    flink_t *prev; 
};

typedef struct {
    flink_t  link;              /* XXX Currently below is defined in dcb.h */
    addr_t   base;              /* Start of physical memory region         */
    uint32_t npf;               /* No of *physical* frames it extends      */
    uint32_t fwidth;            /* Logical frame width within region       */
} flist_t;

typedef struct _rtab_t RamTable_t; 
struct _rtab_t {
    uint32_t owner;         /* PHYSICAL address of owning domain's DCB   */
    uint16_t fwidth;        /* Logical width of the frame                */
    uint16_t state;         /* Misc bits, e.g. is_mapped, is_nailed, etc */
};

/* Some general macros for physical addresses and phyiscal/logical frames */

/* Convert a physical address into a PFN */
#define PFN(_paddr)  ((word_t)(_paddr)  >> FRAME_WIDTH)

/* Determine if a value "_x" is aligned to the frame width "_fw" */
#define ALIGNED(_x,_fw)          (!((word_t)(_x) & ((1UL<<(_fw))-1)))

/* Roundup a value "_x" to an intergral number of frames of width "_fw" */
#define ROUNDUP(_x,_fw)       	 (((word_t)_x + (1UL<<(_fw))-1) & 	\
				  ~((1UL<<(_fw))-1))

/* Convert "bytes" into a number of frames of logical width "fwidth" */
#define BYTES_TO_LFRAMES(_x,_fw) (ROUNDUP((word_t)(_x), (_fw)) >> (_fw))

/* And vice versa... */
#define LFRAMES_TO_BYTES(_x,_fw) (_x << (_fw))

#endif /* __LANGUAGE_C */


/* definitions for owner field of ramtab */
#define OWNER_NONE    0x0     /* pfn is unused by anyone        */
#define OWNER_SYSTEM  0x1     /* pfn is owned by us (mmgmt etc) */

/* definitions for state field of ramtab */
#define RAMTAB_UNUSED 0x0
#define RAMTAB_MAPPED 0x1
#define RAMTAB_NAILED 0x2

#endif /* _FRAMES_H_ */
