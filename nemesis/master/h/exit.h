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
**      exit.h
** 
** DESCRIPTION:
** 
**      Internal structure definitions for use by mod/venimpl/c/stdlib/exit.c
**      
** 
** ID : $Id: exit.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _exit_h_
#define _exit_h_

/* exit stacks */
#define __TYPE_OF_FUNC_ATEXIT  0
#define __TYPE_OF_FUNC_ON_EXIT 1
struct __atexit_stack_element
{
    union
    {
	void (*atexit_func)(void);
	void (*on_exit_func)(int, void *);
    }func;
    int  type_of_func;
    void *on_exit_arg;
};
/* POSIX say we only have to be able to do 32 functions (according
 * to Austin).
 */
#define __MAX_ATEXIT_FUNCS 32

#endif
