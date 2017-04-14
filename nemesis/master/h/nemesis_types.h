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
**      nemesis_types.h
** 
** DESCRIPTION:
** 
**      Standard type definitions for Nemesis.
**      
** 
** ID : $Id: nemesis_types.h 1.2 Thu, 20 May 1999 14:50:44 +0100 dr10009 $
** 
*/

#ifndef _nemesis_types_h_
#define _nemesis_types_h_

/*
 * Target-indep base types not dependant on target dependant types
 */

typedef void		       *addr_t;
typedef char		       *string_t;
typedef enum {False=0, True=1}	bool_t;

#include <nemesis_tgt.h>		/* Target-dependent stuff	*/

/* 
 * Target-indep. base types dependant on target dependant types
 */

typedef uint32_t		enum_t;
typedef uint64_t		set_t;

/* Cardinals in network endian; XXX - should be distinct types */
typedef uint16_t		nint16_t;
typedef uint32_t		nint32_t;
typedef uint64_t		nint64_t;

/* IO registers */
typedef volatile uint32_t      *const ioreg32_t;
typedef volatile uint64_t      *const ioreg64_t;
typedef volatile word_t	       *const ioreg_t;


/*
 * Target-indep. constants
 */

#define NULL			((addr_t)0)
#define WORD_MAX		((word_t)(-1))	/* Biggest unsigned int	*/

/*
 * Type codes and wordconversions (mostly blank) for all predefined types
 */
#define uint8_t__code	(1ull)
#define uint8_t__wordconv
#define uint8_t__anytype (uint8_t)
#define uint16_t__code	(2ull)
#define uint16_t__wordconv
#define uint16_t__anytype (uint16_t)
#define uint32_t__code	(3ull)
#define uint32_t__wordconv
#define uint32_t__anytype (uint32_t)
#define uint64_t__code	(4ull)
#define uint64_t__wordconv
#define uint64_t__anytype (uint64_t)

#define int8_t__code	(5ull)
#define int8_t__wordconv
#define int8_t__anytype (int8_t)
#define int16_t__code	(6ull)
#define int16_t__wordconv
#define int16_t__anytype (int16_t)
#define int32_t__code	(7ull)
#define int32_t__wordconv
#define int32_t__anytype (int32_t)
#define int64_t__code	(8ull)
#define int64_t__wordconv
#define int64_t__anytype (int64_t)

#define float32_t__code	(9ull)
#define float32_t__wordconv
#define float32_t__anytype (float32_t)
#define float64_t__code	(10ull)
#define float64_t__wordconv
#define float64_t__anytype (float64_t)

#define bool_t__code	(11ull)
#define bool_t__wordconv
#define bool_t__anytype  (bool_t)

#define string_t__code	(12ull)
#define string_t__wordconv  (pointerval_t)
#define string_t__anytype  (string_t)

#define word_t__code	(13ull)
#define word_t__wordconv
#define word_t__anytype    (word_t)

#define addr_t__code	(14ull)
#define addr_t__wordconv   (pointerval_t)
#define addr_t__anytype   (addr_t)
  

/*
 * On some architectures, it is not possible to statically initialise the
 * 64-bit "val" field of an "Any" with an address calculated at link
 * time.  The "typed_ptr_t" type is used to fudge our way around this problem.
 */
#include <Type.ih>			/* Basic typesystem defns	*/

typedef union {
  struct {				/* MUST be first for u initialisers  */
    Type_Code	type;
#if defined (BIG_ENDIAN)		/* ASSUME problem arch's are 32-bit */
    uint32_t	pad;
#endif
    addr_t	val;
  }			ptr;
  Type_Any		any;
} typed_ptr_t;

#if defined (BIG_ENDIAN)
#  define TYPED_PTR(tc,val) { { tc, 0, (addr_t) (val) } }
#else
#  define TYPED_PTR(tc,val) { { tc, (addr_t) (val) } }
#endif



#endif /* _nemesis_types_h_ */

