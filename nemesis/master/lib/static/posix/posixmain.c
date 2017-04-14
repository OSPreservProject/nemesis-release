/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      posixmain.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Defines a last-ditch Main() function for posix main() compatability
** 
** ENVIRONMENT: 
**
**      libsystem.a - ie optionally added to all PROGRAM and MODULEs
** 
** ID : $Id: posixmain.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <contexts.h>
#include <exceptions.h>
#include <exit.h>
#include <time.h>
#include <ecs.h>

#undef SPEC

/*****************************************************************************/
/*****************************************************************************/

#ifdef SPEC
#include <Time.ih>
#endif

/* Internal functions */
#if 0
static void __execute_atexit_stack(int status);
#endif
#ifdef SPEC
void __print_time(void);
static int posixmain_atexit(void (*function)(void));
#endif
/*****************************************************************************/
/* External functions                                                        */
/*****************************************************************************/
/* We expect POSIX programs to define at least a main() function! */
extern int main(int argc, char **argv);

/* Define a Nemesis-style Main() function which will serve as the
 * entry point of the user program if they haven't defined one.  This
 * allows the user to have a posix-style main() function, and have it
 * called in the traditional manner by this piece of code.  Since this
 * is in an ar-archive, it will only be used if Main() is undefined -
 * precisely the semantics we want. */
void Main(Closure_clp cl)
{
    NOCLOBBER uint32_t            argc;
    char ** NOCLOBBER             argv;
    char                          *null_arg = NULL;
    int                           ret;
    struct __atexit_stack_element __atexit_stack[__MAX_ATEXIT_FUNCS];
    int                           __atexit_stack_length;
#ifdef SPEC
    Time_T NOCLOBBER              start_time;
#endif

#ifdef SPEC
    start_time = NOW();
#endif

    __atexit_stack_length = 0;

    /*
     * Argument handling
     */
    TRY {
      uint32_t       i;
      Context_clp    ctx;
      uint32_t       num;
      
      ctx = NAME_LOOKUP ("arguments", Pvs(root), Context_clp);
      num = SEQ_LEN( Context$List( ctx ) );
      argv = Heap$Malloc (Pvs(heap), (num + 1) * sizeof (char *));
      for (i = 0, argc=0; i < num; i++) {
	uchar_t name[32];
	sprintf (name, "a%d", i);
	TRY {
	  argv[argc] = NAME_LOOKUP (name, ctx, string_t);
	  argc++;
	} CATCH_ALL {
	  /* one argument didn't have the right form, ignore it */
	} ENDTRY;
      }
      argv[argc] = (char *)NULL;
    } CATCH_ALL {
      argc = 0;
      argv = &null_arg;
    } ENDTRY;
    
    /* Run the program with a correctly setup exit. */
    CX_ADD("__atexit_stack", __atexit_stack, addr_t);
    CX_ADD("__atexit_stack_length", &__atexit_stack_length, addr_t);


#ifdef SPEC
    /* For timing stuff */
    CX_ADD("__spec_start_time", start_time, Time_T);
    (void)posixmain_atexit(__print_time);
#endif
    ret = main(argc, argv);
    if (ret)
	printf("main() exited with non-zero return code %d\n", ret);
}


/*****************************************************************************/
/* Internal functions                                                        */
/*****************************************************************************/
#ifdef SPEC
void __print_time(void)
{
    Time_T start_time;

    TRY
    {
	start_time = NAME_FIND("__spec_start_time", Time_T);
	printf("Main finished in time %ld\n", NOW()-start_time);
    }
    CATCH_ALL
    {
    }
    ENDTRY;
}
#endif

#if 0
/*****************************************************************************/
/*****************************************************************************/

static void exit(int status)
{
    __execute_atexit_stack(status);
    _exit(status);
}

/*****************************************************************************/
/*****************************************************************************/

static void _exit(int status)
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
#endif
/*****************************************************************************/
/*****************************************************************************/
#ifdef SPEC /* Note: this is a cut down version of atexit */
static int posixmain_atexit(void (*function)(void))
{
    struct __atexit_stack_element *f;
    struct __atexit_stack_element *__atexit_stack;
    int                           *p__atexit_stack_length;


    __atexit_stack         = NAME_FIND("__atexit_stack", addr_t);
    p__atexit_stack_length = NAME_FIND("__atexit_stack_length", addr_t);

    (*p__atexit_stack_length)++;
    f = &(__atexit_stack[(*p__atexit_stack_length)-1]);

    f->type_of_func     = __TYPE_OF_FUNC_ATEXIT;
    f->func.atexit_func = function;

    return(0);
}
#endif
/*****************************************************************************/
/*****************************************************************************/
