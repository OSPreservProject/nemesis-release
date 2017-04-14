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
**      nemesis.h
** 
** DESCRIPTION:
** 
**      Standard definitions for Nemesis.
**      
** 
** ID : $Id: nemesis.h 1.2 Thu, 20 May 1999 14:50:44 +0100 dr10009 $
** 
*/

#ifndef _nemesis_h_
#define _nemesis_h_

#include <nemesis_types.h>	/* includes Target-dependent stuff */
#include <pervasives.h>

/*
 * Target-indep. constants
 */

#define NULL_EP			((word_t)-1L)
#define NULL_EVENT		((word_t)-1L)
#define NULL_PDID  		((word_t)-1L)
#define NULL_DOM		((uint64_t)-1LL)
#define ANY_ADDRESS             ((addr_t)-1L)  
#define NO_ADDRESS              ((addr_t)-1L)  /* synonym for error returns */

/*
 * Compiler directives
 */

#ifdef __GNUC__
#  if (__GNUC__ > 2) || ((__GNUC__ == 2) && defined(__GNUC_MINOR__))
#    define NORETURN(x) void x __attribute__ ((noreturn))
#  else
#    define NORETURN(x) volatile void x
#  endif
#  define INLINE		__inline__
#  define UNUSED                __attribute__((unused))
#else
#  define NORETURN(x) void x
#  define INLINE
#  define UNUSED
#endif

#define PUBLIC			/* useful for emphasis		*/


/* 
 * Convenient macros
 */

/* These three evaluate the result TWICE.  Beware! */

#define MIN(x, y)		(((x) < (y)) ? (x) : (y))
#define MAX(x, y)		(((x) > (y)) ? (x) : (y))
#define ABS(x)			((x) < 0 ? (-(x)) : (x))

#define OFFSETOF(_type, _field)	((word_t) &(((_type *)0)->_field))

#define HIGH32(ll)		((uint32_t) ((ll) >> 32))
#define LOW32(ll)		((uint32_t) ((ll) & 0xffffffff))

#define WORD_ALIGN(_x)		(((_x)+WORD_SIZE-1) & ~WORD_MASK)


/*
 * Set stuff
 */

#define SET_ELEM(_x)                  (((set_t)1)<<(_x))
#define SET_IN(_s,_e) 			((_s) & SET_ELEM(_e))
#define SET_ADD(_s,_e) 			(_s) |= SET_ELEM(_e)
#define SET_DEL(_s,_e) 			(_s) &= ~SET_ELEM(_e)
#define SET_INTERSECTION(_s1,_s2)	((_s1) & (_s2) )
#define SET_UNION(_s1,_s2)		((_s1) | (_s2))


/* Sequences */
#include <sequence.h>


/* 
 * Closure stuff
 */

#define CLP_INIT(_clp,_op,_st)	\
{					\
	(_clp)->op = (_op);		\
	(_clp)->st = (addr_t)(_st);	\
}
#define CL_INIT(_cl,_op,_st)	 	\
{					\
	(_cl).op = (_op);		\
	(_cl).st = (addr_t)(_st);	\
}

/* 
 * The "CL_EXPORT" macro creates symbols with names that are interpreted
 * by the module loader as requests to export the given values in the
 * "modules" context.
 * 
 * On some architectures, it is not possible to statically initialise the
 * 64-bit "val" field of an "Any" with an address calculated at link
 * time.  The "typed_ptr_t" type defined in "nemesis_types.h" is used to
 * fudge our way around this problem.
 */

#define CL_EXPORT(_type,_name,_closure)\
    const typed_ptr_t _name##_export = \
	TYPED_PTR (_type##_clp__code, &_closure); \
    const _type##_cl * const _name = &_closure;

/*
 * The "CL_EXPORT_TO_PRIMAL" macro creates symbols with names that are
 * interpreted by the kernel linked as requests to export the given values
 * in the "primal" context.
 *
 * This is similar to CL_EXPORT, except that typing is not involed, and
 * the name _primal is tacked on the end rather than _export.
 */

#define CL_EXPORT_TO_PRIMAL(_type, _name, _closure)\
    const _type##_cl * const _name##_primal = &_closure;

/*
 * "Any" stuff
 */

#define ANY_DECL(_name,_type,_val) \
	Type_Any _name = { _type##__code, (Type_Val) _type##__wordconv (_val) }
#define ANY_INIT(_anyp,_type,_val) \
        do {(_anyp)->val=(Type_Val) _type##__wordconv (_val);\
	    (_anyp)->type=(_type##__code);}while(0)
#define ANY_INITC(_anyp,_typec,_val) \
        do {(_anyp)->val=(Type_Val) (_val);\
	    (_anyp)->type=(_typec);}while(0)
#define ANY_COPY(_dest,_src) \
        do { (_dest)->val=(_src)->val; (_dest)->type= (_src)->type;} while(0)
#define ANY_CAST(_anyp,_type) \
	do { (_anyp)->type=(_type##__code);}while(0)
#define ANY_CASTC(_anyp,_typec) \
	do { (_anyp)->type=(_typec);}while(0)
#define ISTYPE(_anyp,_type) \
	(TypeSystem$IsType (Pvs (types), (_anyp)->type, _type##__code ))

#define NARROW(_anyp,_type) \
	(_type##__anytype (_type##__wordconv (TypeSystem$Narrow (Pvs (types), _anyp, _type##__code ))))

#define ANY_UNALIAS(_anyp) \
        do { ((_anyp)->type) = TypeSystem$UnAlias(Pvs(types), (_anyp)->type); } while(0)
    
/* 
 * Generic Heap stuff: all "Heap"s must make HEAP_OF (node) work.
 */

#define HEAP_OF(_node)		((Heap_clp) *( ((word_t *)(_node)) - 1 ))
#define FREE(_x)		Heap$Free (HEAP_OF (_x), (_x))
#define REALLOC(_x, _sz)	Heap$Realloc (HEAP_OF (_x), (_x), (_sz))



#endif /* _nemesis_h_ */

