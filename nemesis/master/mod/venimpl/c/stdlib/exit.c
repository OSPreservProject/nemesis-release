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
**      exit.c
** 
** DESCRIPTION:
** 
**      Defines exit's friends, exit is in posixmain.c because it needs this.
**      
** 
** ID : $Id: exit.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <contexts.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>
#include <stdlib.h>
#include <stdio.h>
#include <exit.h>
#include <unistd.h>

/*****************************************************************************/
/*****************************************************************************/

/* For errors etc, these should not be here */
/*extern int  errno;
extern char *sys_errlist[];
int         errno;
char        *sys_errlist[];
#define ENOMEM          12 */     /* Out of memory */
static void __execute_atexit_stack(int status);

/*****************************************************************************/
/*****************************************************************************/

/* XXX Warning not atomic */
int on_exit(void (*function)(int , void *), void *arg)
{
    struct __atexit_stack_element *f;
    struct __atexit_stack_element *__atexit_stack;
    int                           *p__atexit_stack_length;


    TRY
    {
	__atexit_stack         = NAME_FIND("__atexit_stack", addr_t);
	p__atexit_stack_length = NAME_FIND("__atexit_stack_length", addr_t);
    }
    CATCH_ALL
    {
	printf("atexit stack missing\n");
/*	errno = ENOMEM; */
	return(-1);
    }
    ENDTRY;


    if(*p__atexit_stack_length >= __MAX_ATEXIT_FUNCS)
    {
/*	errno = ENOMEM; */
	return(-1);
    }
    (*p__atexit_stack_length)++;
    f = &(__atexit_stack[(*p__atexit_stack_length)-1]);

    f->type_of_func      = __TYPE_OF_FUNC_ON_EXIT;
    f->func.on_exit_func = function;
    f->on_exit_arg       = arg;

    return(0);
}

/*****************************************************************************/
/*****************************************************************************/
int atexit(void (*function)(void))
{
    struct __atexit_stack_element *f;
    struct __atexit_stack_element *__atexit_stack;
    int                           *p__atexit_stack_length;


    TRY
    {
	__atexit_stack         = NAME_FIND("__atexit_stack", addr_t);
	p__atexit_stack_length = NAME_FIND("__atexit_stack_length", addr_t);
    }
    CATCH_ALL
    {
	printf("atexit stack missing\n");
/* 	errno = ENOMEM; */
	return(-1);
    }
    ENDTRY;

    if(*p__atexit_stack_length >= __MAX_ATEXIT_FUNCS)
    {
/*	errno = ENOMEM; */
	return(-1);
    }
    (*p__atexit_stack_length)++;
    f = &(__atexit_stack[(*p__atexit_stack_length)-1]);

    f->type_of_func     = __TYPE_OF_FUNC_ATEXIT;
    f->func.atexit_func = function;

    return(0);
}
/*****************************************************************************/
/*****************************************************************************/
void exit(int status)
{
    __execute_atexit_stack(status);
    _exit(status);
}

/*****************************************************************************/
/*****************************************************************************/

void _exit(int status)
{
    IDCClientBinding_clp binder_binding;
    
    binder_binding = NAME_FIND("sys>Binder", IDCClientBinding_clp);

    IDCClientBinding$Destroy(binder_binding);
    /* NOTREACHED */
    PAUSE(FOREVER);
    for(;;);
}
/*****************************************************************************/
/*****************************************************************************/
static void __execute_atexit_stack(int status)
{
    struct __atexit_stack_element *f;
    struct __atexit_stack_element *__atexit_stack;
    int                           *p__atexit_stack_length;


    TRY
    {
	__atexit_stack         = NAME_FIND("__atexit_stack", addr_t);
	p__atexit_stack_length = NAME_FIND("__atexit_stack_length", addr_t);
    }
    CATCH_ALL
    {
	printf("atexit stack missing\n");
	return;
    }
    ENDTRY;


    f = &(__atexit_stack[*p__atexit_stack_length]);
    while(*p__atexit_stack_length)
    {
	(*p__atexit_stack_length)--;
	f--;
	if(f->type_of_func == __TYPE_OF_FUNC_ATEXIT)
	{
	    f->func.atexit_func();
	}
	else if(f->type_of_func == __TYPE_OF_FUNC_ON_EXIT)
	{
	    f->func.on_exit_func(status, f->on_exit_arg);
	}
	else
	{
	    printf("atexit stack corrupted\n");
	}
    };
}
/*****************************************************************************/
/*****************************************************************************/
