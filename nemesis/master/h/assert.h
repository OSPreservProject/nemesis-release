/*
 *	assert.h
 *	--------
 *
 * $Id: assert.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 * Copyright (c) 1991, 1992, 1994, 1995 University of Cambridge
 * All Rights Reserved.
 *
 * Generated from the FAWN version which was in turn ...
 *
 * Generated from Tim Wilson's Wanda version
 */

/*
 * In traditional UNIX, assert was a statement, but in ANSI it is a
 * void expression.
 *
 * This file is not strictly conforming since it defines non-standard
 * names in the user name space (badassertion etc).
 *
 * This file must allow multiple inclusions with various states of NDEBUG
 *
 */

#ifndef __assert_h__
#define __assert_h__

#ifndef __NORETURN
#ifdef __GNUC__
#if (__GNUC__ > 2) || ((__GNUC__ == 2) && defined(__GNUC_MINOR__))
#define __NORETURN(x) void x __attribute__ ((noreturn))
#else
#define __NORETURN(x) volatile void x
#endif
#else
#define __NORETURN(x) void x
#endif
#endif

extern __NORETURN(badassertion(char *file, int line));
extern __NORETURN(badcompare(char *file, int line, long a));
extern __NORETURN(badassertionm(char *file, int line, char *msg));

#endif /* __assert_h__ */

/* Undefs for multiple inclusions */

#undef assert
#undef assertm
#undef compare

#ifndef NDEBUG

#define assert(E)	((E) ? (void) 0 \
			     : badassertionm (__FILE__, __LINE__, #E))
#define assertm(E,M)	((E) ? (void) 0 \
			     : badassertionm (__FILE__, __LINE__, M))
#define compare(A,T,B)	(((A) T (B)) ? (void) 0 \
			             : badcompare (__FILE__, __LINE__, \
						   (long) (A)))

#else /* NDEBUG */

#define assert(e)	((void) 0)
#define assertm(e,m)	((void) 0)
#define compare(a,t,b)	((void) 0)

#endif /* NDEBUG */

/* End of file assert.h */
