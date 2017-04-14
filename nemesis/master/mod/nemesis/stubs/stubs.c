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
**      Stubs.c
**
** FUNCTIONAL DESCRIPTION:
** 
**      Registers IDC stubs at system startup
** 
** ENVIRONMENT: 
**
**      Nemesis Primal
** 
** ID : 	$Id: stubs.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <contexts.h>
#include <stdio.h>

#include <Context.ih>
#include "stubs.h"

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif
#define DB(x)  x

static  Closure_Apply_fn                RegisterStubs;

static  Closure_op      ms = {
  RegisterStubs
};

static  const Closure_cl      cl = { &ms, NULL };

CL_EXPORT (Closure, StubsRegister, cl);
CL_EXPORT_TO_PRIMAL (Closure, StubsRegisterCl, cl);

#define TC_STRING_LEN (2*sizeof(Type_Code))

void Context_AddStubs(Context_clp c, stubs_entry *stub)
{
    const char *hex = "0123456789ABCDEF";
    char tname[TC_STRING_LEN+1];
    Type_Any *surr;
    Type_Code tc;
    int i;
    
    IDCStubs_Info stubs = (IDCStubs_Info)(stub->any->val);
    
    TRC(eprintf("IDCStubs_Info ANY = (%qx, %qx)\n", 
		stub->any->type, stub->any->val));
    
    if (stub->any->type != IDCStubs_Info__code) {
	eprintf("Bad typecode 0x%qx for '%s'!\n", 
		stub->any->type, stub->name);
    }

    surr = (Type_Any *)&stubs->surr;

    TRC(eprintf("Client Stubs ANY=(%qx, %qx)\n", surr->type, surr->val));

    /* Extract typecode from client_cl of the Info structure */
    tc = surr->type;

    TRC(eprintf("Adding %qx\n", tc));
    
    sprintf(tname, "%Q", tc);

    TRC(eprintf("Adding '%s' : %qx ('%s')\n", stub->name, tc, tname));

    /* Put stubs in context under typecode */
    TRC(eprintf("Adding stubs for Type_Code '%s' -> %p\n", tname, stub->any));
    Context$Add(c, tname, (Type_Any *)stub->any);
}



void RegisterStubs (Closure_clp self)
{
    Context_clp ctx = NAME_FIND("stubs", Context_clp);

    stubs_entry *stubs = stubslist;
    
    while(stubs->name) {
	TRC(eprintf("Adding %s\n", stubs->name));
	Context$Add(ctx, stubs->name, (Type_Any *)stubs->any);
	Context_AddStubs(ctx, stubs);
	stubs++;
    }

}
