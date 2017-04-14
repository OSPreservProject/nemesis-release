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
**      NTSC.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Allows user space control over some NTSC parameters.
** 
** ENVIRONMENT: 
**
**      Privileged user space.
** 
** ID : $Id: NTSC.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <kernel_st.h>
#include <ntsc.h>

#include <NTSC.ih>

static  NTSC_SetCrashdump_fn            NTSC_SetCrashdump_m;
static  NTSC_SetConsole_fn              NTSC_SetConsole_m;

static  NTSC_op ms = {
    NTSC_SetCrashdump_m,
    NTSC_SetConsole_m
};


NTSC_clp NTSC_access_init(kernel_st *st)
{
    NTSC_clp clp;

    clp=Heap$Malloc(Pvs(heap), sizeof(*clp));
    CLP_INIT(clp, &ms, st);

    return clp;
}

/*---------------------------------------------------- Entry Points ----*/

static void NTSC_SetCrashdump_m (
    NTSC_cl *self,
    bool_t  local   /* IN */,
    uint32_t        serial  /* IN */ )
{
    kernel_st       *st = self->st;

    ENTER_KERNEL_CRITICAL_SECTION();
    st->crash.serial=(serial>0);
    st->crash.local=local;
    if (serial) st->serial.baseaddr = serial;
    LEAVE_KERNEL_CRITICAL_SECTION();
}

static void NTSC_SetConsole_m (
    NTSC_cl *self,
    bool_t  buffer  /* IN */,
    bool_t  local   /* IN */,
    uint32_t        serial  /* IN */ )
{
    kernel_st       *st = self->st;

    ENTER_KERNEL_CRITICAL_SECTION();
    st->console.buffer_output=buffer;
    st->console.local_output=local;
    if (serial) {
	st->console.serial_output=True;
	st->serial.baseaddr = serial;
    } else {
	st->console.serial_output=False;
    }
    LEAVE_KERNEL_CRITICAL_SECTION();
}

