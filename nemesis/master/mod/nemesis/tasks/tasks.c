/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      mod/nemesis/tasks/tasks.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Experimental ANSA-style task scheduler, modification of Threads.c
**	Multiplexes threads over a Nemesis virtual processor.
** 
** ENVIRONMENT: 
**
**      User-land;
**
*/

#include <nemesis.h>

static void foo() { 
    return; 
}


