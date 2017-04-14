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
**      ttyterm
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Convert keyboard to a reader, scribble on VGA. Modify pvs.
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: ttyterm.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/
#include <nemesis.h>
#include <IDCMacros.h>
#include <Kbd.ih>
#include <time.h>
#include <ecs.h>

typedef void *Beep_clp;
static Closure_Apply_fn Init; 
static Closure_op ms = { Init }; 
static const Closure_cl cl = {&ms, NULL}; 
CL_EXPORT(Closure,ttyterm, cl); 

Rd_clp KbdReader_New(Kbd_clp kbd);

static void Init (Closure_clp self)
{
    Kbd_clp NOCLOBBER kbd;

    kbd = IDC_OPEN("dev>kbd", Kbd_clp);

    Pvs(out)=IDC_OPEN("dev>local_console",Wr_clp);
    Pvs(err)=Pvs(out);
    Pvs(in) = KbdReader_New(kbd);
}
