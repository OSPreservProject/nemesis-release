/*
 *	stdlib.h
 *	--------
 *
 * $Id: stdlib.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 */

#ifndef __stdlib_h__
#define __stdlib_h__

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

/*
 * Section: ANSI Conformance
 */

#include "stddef.h"

typedef struct 
{
    int		quot;
    int		rem;
} div_t;

typedef struct 
{
    long	quot;
    long	rem;
} ldiv_t;

#define EXIT_FAILURE  1
#define EXIT_SUCCESS  0

#define MB_CUR_MAX  1

#define RAND_MAX barf_for_now

/* ----- 4.10.1	String Comversion Functions			*/
extern double		 atof		(const char *nptr);
extern int		 atoi		(const char *nptr);
extern long		 atol		(const char *nptr);
extern double		 strtod		(const char *nptr, char **endptr);
extern long		 strtol		(const char *nptr, char **endptr,
					 int base);
extern unsigned long	 strtoul	(const char *nptr, char **endptr,
					 int base);

/* Nemesis extensions */
extern int64_t		 strto64	(const char *nptr, char **endptr,
					 int base);
extern uint64_t		 strtou64	(const char *nptr, char **endptr,
					 int base);


/* ----- 4.10.2	Pseudo-Random Sequence Generation Functions	*/
extern int		 rand		(void);
extern void		 srand		(unsigned int seed);

/* ----- 4.10.3	Memory Management Functions			*/
extern void		*calloc		(size_t nmemb, size_t size);
extern void		 free		(void *ptr);
extern void		*malloc		(size_t size);
extern void		*realloc	(void *ptr, size_t size);

/* ----- 4.10.4	Communication withh the Environment		*/
extern __NORETURN(	 abort		(void) );
extern int		 atexit		(void (* fun)(void));
extern __NORETURN(	 exit		(int status) );
extern char		*getenv		(const char *name);
extern int		 system		(const char *string);

/* ----- 4.10.5	Searching and Sorting Utilities			*/
extern void		*bsearch	(const void *key, const void *base,
					 size_t nmemb, size_t size,
					 int (*compar)(const void *,
						       const void *));
extern void		 qsort		(void *base, size_t nmemb, size_t size,
					 int (*compar)(const void *,
						       const void *));

/* ----- 4.10.6	Integer Arithmetic Functions			*/
extern int		 abs		(int j);
extern div_t		 div		(int numer, int denom);
extern long		 labs		(long int j);
extern ldiv_t		 ldiv		(long int numer, long int denom);

/* ----- 4.10.7	Multibyte Character Functions			*/
extern int		 mblen		(const char *s, size_t n);
extern int		 mbtowc		(wchar_t *pwc, const char *s,
					 size_t n);
extern int		 wctomb		(char *s, wchar_t wchar);
extern size_t		 mbstowcs	(wchar_t *pwcs, const char *s,
					 size_t n);
extern size_t		 wcstombs	(char *s, const wchar_t *pwcs,
					 size_t n);
/* Local additions */
#ifdef  __GNUC__
#define __alloca(size)  __builtin_alloca(size)
#endif

#define alloca(size)    __alloca(size)

#endif /* __stdlib__h__ */
