/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Jumptable for stateless libc entry
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Provide an array of entry points of libc functions
** 
** ENVIRONMENT: 
**
**      Standalone module. No State.
** 
** ID : $Id: jmptbl.c 1.2 Tue, 04 May 1999 18:45:38 +0100 dr10009 $
** 
**
*/

/*************************************************************
 * NOTE WELL: All functions added to here must also be added to
 *   lib/veneers/libc.ven    The order must match!  New functions
 *   should be added to the end of the lists, to preserve backwards
 *   compatability.
 *************************************************************/

#include <nemesis.h>
#include <time.h>
#include <ecs.h>

/* suck in libc */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <bstring.h>
#include <unistd.h>

/* and the bits that people shouldn't need to know about, but are required
 * by assert.h */
extern NORETURN( badassertion(char *file, int line) );
extern NORETURN( badassertionm(char *file, int line, char *msg) );
extern NORETURN( badcompare(char *file, int line, long a) );

/* other bits we'd really like hidden */
static void _warn_libc_incompat(void);


/* The order of functions in this array must match the order they are
 * listed in libc.ven */
static const addr_t libc_jmp[] = {
    abort,
    abs,
    atoi,
    atol,
    badassertion,
    badassertionm,
    badcompare,
    bcmp,
    bcopy,
    bsearch,
    bzero,
    calloc,
    div,
    eprintf,
    fprintf,
    free,
    fscanf,
    getenv,
    isalnum,
    isalpha,
    isascii,
    iscntrl,
    isdigit,
    isgraph,
    islower,
    isprint,
    ispunct,
    isspace,
    isupper,
    isxdigit,
    labs,
    ldiv,
    malloc,
    memchr,
    memcmp,
    memcpy,
    memmove,
    memset,
    libc_printf,
    qsort,
    realloc,
    scanf,
    /* NOT setjmp - you will want to link against setjmp.o manually */
    sprintf,
    sscanf,
    strcasecmp,
    strcat,
    strchr,
    strcmp,
    strcpy,
    strcspn,
    strdup,
    strduph,
    strlen,
    strncat,
    strncmp,
    strncpy,
    strpbrk,
    strrchr,
    strspn,
    strstr,
    strto64,
    strtol,
    strtou64,
    strtoul,
    toascii,
    tolower,
    toupper,
    vfprintf,
    vprintf,
    vsprintf,
    wprintf,
    strsep,
    fopen,
    fclose,
    fread,
    fwrite,
    ferror,
    fgets,
    fputs,
    fgetc,
    fputc,
    fflush,
    exit,
    atexit,
    on_exit,
    _exit,
#ifdef __IX86__
    atof,
    strtod,
    tan,
    exp,
    __log2,
    pow,
    atan,
#endif
    /* from here on, we have some warning functions.  New real
     * libc functions should go above this line */
    _warn_libc_incompat,
    _warn_libc_incompat,
    _warn_libc_incompat,
    _warn_libc_incompat,
    _warn_libc_incompat
};

struct generic_cl {
    const addr_t op;
    const addr_t state;
};

static const struct generic_cl libc_cl = {&libc_jmp, BADPTR};
PUBLIC const struct generic_cl * const libc_clp_primal = &libc_cl;


static void _warn_libc_incompat(void)
{
    fprintf(stderr, "WARNING: program called undefined libc function: "
	    "program may have been linked against newer libc\n");
    fprintf(stderr, "     -- thread halted\n");

    PAUSE(FOREVER);

    for(;;)
	;
}


/* End of jmptbl.c */
