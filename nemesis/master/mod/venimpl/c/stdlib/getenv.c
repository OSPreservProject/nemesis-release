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
**      getenv
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Lookup the name in the Pvs(env) context
** 
** ENVIRONMENT: 
**
**      C runtime support library
** 
** ID : $Id: getenv.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <string.h>
#include <nemesis.h> 
#include <exceptions.h>

#define ENV_PREFIX "env/"
#define BUF_SIZE 1024

extern char *
getenv(const char *name)
{
    Type_Any ret;
    string_t NOCLOBBER s = NULL;
    
/*
** XXX 
** We'd like to have the temporary buffer we use for holding the name
** to be an auto-variable, but, on alpha at least, having a partially 
** initialized array as an auto variable means that gcc inserts some 
** zero-ing code for you, which turns out to call bzero.
** Since we don't have a real bzero (just a macro), this turns out not to 
** be such a good plan ;-(
*/
    char *buf;
    
    buf= Heap$Calloc(Pvs(heap), 1, BUF_SIZE);
    strcpy(buf, ENV_PREFIX);
    strncat(buf, name, 1019);
    
    TRY {
	if (Context$Get(Pvs(root), buf, &ret)) {
	    s = NARROW(&ret, string_t);
	}
    } CATCH_ALL {
    } ENDTRY;
    
    Heap$Free(Pvs(heap), buf);
    return s;
}
