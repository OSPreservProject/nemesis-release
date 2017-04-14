/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      stubs.h
** 
*/

#ifndef _stubs_h_
#define _stubs_h_
#include <nemesis.h>
#include <IDCStubs.ih>

typedef struct _stubs_entry {
    char *name;
    Type_AnyI *any;
} stubs_entry;

extern const stubs_entry stubslist[];

#endif /* _stubs_h_ */
