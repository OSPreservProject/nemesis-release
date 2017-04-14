/*
 *	strdup.c
 *	--------
 *
 * $Id: strdup.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 */

#include <sys/types.h>
#include <stdlib.h>

char *strdup(const char *s)
{
    char *res;

    if (!s) return NULL;
    
    res = malloc(strlen(s) + 1);
    if (!res) return NULL;
    
    strcpy(res, s);
    return res;
}
