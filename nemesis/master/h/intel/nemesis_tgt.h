/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      ix86/nemesis_tgt.h
** 
** DESCRIPTION:
** 
**      Target-dependent standard definitions; included by <nemesis.h>
**      
**      ix86 version.
** 
** ID : $Id: nemesis_tgt.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
*/

#ifndef __nemesis_tgt_h__
#define __nemesis_tgt_h__

/* 
 * Standard (MIDDL) base types
 */

typedef unsigned char           uchar_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int   		uint32_t;
typedef unsigned long long int	uint64_t;

typedef signed char 		int8_t;
typedef signed short 		int16_t;
typedef signed int    		int32_t;
typedef signed long long int	int64_t;

typedef float 		        float32_t;
typedef double			float64_t;

typedef uint32_t		word_t;
typedef int32_t			sword_t;
typedef uint32_t                pointerval_t;


/* 
 * Constants
 */

#define __IX86__	1
#define __ix86__	1
#undef  ix86
#define ix86 		__IX86__
#define LITTLE_ENDIAN
#define BADPTR		((addr_t)0xDEADBEEF)


/*
 * Frame size definitions
 */
#define FRAME_WIDTH	(12)
#define FRAME_SIZE	(1<<FRAME_WIDTH)

/*
 * Page size definitions
 */
#define PAGE_WIDTH	(12)
#define PAGE_SIZE	(1<<PAGE_WIDTH)

/*
 * Word size definitions
 */
#if 1
#define WORD_SIZE	(sizeof(word_t))	/* bytes per word	*/
#define WORD_MASK	(3)			/* mask for alignment	*/
#else
/* With alignment checking enabled, doubles must be aligned to 8 bytes */
/* XXX as a workaround, domains which use floating point have alignment
   checking disabled automatically */
#define WORD_SIZE (8)
#define WORD_MASK (7)
#endif /* 0 */

/* 
 * Pervaisves access
 */

/* 
** XXX SMH: the below is vile, but necessary since we are about to pull
** in some middlc generated .ih files from pip.h... and we have not yet 
** typedef'd set_t or enum_t. 
** Could repeat defs in all nemesis_tgt.h files, but this'll do for now.
*/
/* We don't have the INLINE macro yet... */
static __inline__ uint32_t rpcc(void) {
    uint32_t __h,__l;
    __asm__ __volatile__ ("rdtsc" :"=a" (__l), "=d" (__h) );
    return __l&0xFFFFFFFFL;
}

#define set_t uint64_t
#define enum_t uint64_t

#include <pip.h>
#define __pvs (*((Pervasives_Rec **)(INFO_PAGE.pvsptr)))
#undef set_t
#undef enum_t
/*
 * Access to current stack pointer and return address
 */

#define SP() \
 ({ addr_t __sp; \
    __asm__ ("mov %%esp, %0" : "=r" (__sp) : /* no inputs */); \
    __sp; })

#ifdef __GNUC__
#  define RA() \
    ((addr_t) __builtin_return_address(0))
#else
#  error You need to suppy a definition for RA()
#endif /* __GNUC__ */


#define ANYI_64_READ(me) (*((int64 *) (&me.val)))
#define ANYI_64_WRITE(me,x) ((*((int64 *) (&me.val)))=x)


#define ANYI_INIT_FROM_64( c1, c2 ) { c1, (uint32_t) c2, (uint32_t) (c2 >> 32) }

#endif /* __nemesis_tgt_h__ */


