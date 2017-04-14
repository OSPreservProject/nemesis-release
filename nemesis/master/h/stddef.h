/*
 *	stddef.h
 *	--------
 *
 * $Id: stddef.h 1.2 Thu, 20 May 1999 15:25:49 +0100 dr10009 $
 */

#ifndef __stddef_h__
#define __stddef_h__
#include <nemesis.h> /* for the types */

/*
 * Note: This file is also included by other header files eg. stdlib.h
 */

typedef sword_t		ptrdiff_t;
typedef word_t		size_t;
typedef sword_t		ssize_t;
typedef uint16_t	wchar_t;	/* XXX: ??? */

#define offsetof(atype,member) OFFSETOF(atype,member)

#endif /* __stddef__h__ */
