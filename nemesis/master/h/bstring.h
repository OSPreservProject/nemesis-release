#ifndef __bstring_h__
#define __bstring_h__

#include <stddef.h>

/*
 *  prototypes
 */

extern void bzero(void *, int);
extern void bcopy(void *, void*,  int);
extern int bcmp(const void *s1, const void *s2, int n);
extern char *strsep( char **stringp, const char *delimset);

#endif /* __bstring_h__ */
/*
 * end bstring.h
 */
