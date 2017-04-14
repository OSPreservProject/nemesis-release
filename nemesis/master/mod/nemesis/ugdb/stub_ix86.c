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
**      mod/nemesis/ugdb
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ix86-specific part of user-space stub for remote debugging.
** 
** ENVIRONMENT: 
**
**      User space.
**
*/

#include <nemesis.h>
#include <context.h>
#include <stdio.h>
#include <string.h>


#include "GDB_st.h"


#define NUMREGBYTES 64
enum regnames {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
	       PC /* also known as eip */,
	       PS /* also known as eflags */,
	       CS, SS, DS, ES, FS, GS};

/* BUFMAX defines the max number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX (NUMREGBYTES*4)


PUBLIC bool_t arch_init_state(GDB_st *st, context_t *ctx)
{
    char *buf;

    if(!(st->registers = Heap$Malloc(Pvs(heap), NUMREGBYTES)))
	return False;

    bzero(st->registers, NUMREGBYTES);
    st->numregbytes = NUMREGBYTES;

    if(!(buf = Heap$Malloc(Pvs(heap), 2 * BUFMAX))) 
	return False;

    st->inbuf  = buf; 
    st->outbuf = ((void *)buf) + BUFMAX;
    st->bufmax = BUFMAX;

    /* 
    ** Registers here are in a weird order compared to context, 
    ** so copy 'em all individually (i.e. not block copy) 
    */
    st->registers[EAX] = ctx->gpreg[CX_EAX];
    st->registers[EBX] = ctx->gpreg[CX_EBX];
    st->registers[ECX] = ctx->gpreg[CX_ECX];
    st->registers[EDX] = ctx->gpreg[CX_EDX];

    st->registers[ESP] = ctx->esp;
    st->registers[EBP] = ctx->gpreg[CX_EBP];

    st->registers[ESI] = ctx->gpreg[CX_ESI];
    st->registers[EDI] = ctx->gpreg[CX_EDI];

    st->registers[PC] =  ctx->eip;
    st->registers[PS] =  ctx->eflag;

    /* 
    ** We don't have a handy way of getting the segment registers at 
    ** the time of the fault. But in the current implementation, the 
    ** seg registers are the same for all threads (and user domains!) 
    ** so the below will do fine. 
    */
    __asm__ __volatile__ ("
        movl %%cs, 0(%0);
        movl %%ss, 4(%0);
        movl %%ds, 8(%0);
        movl %%es, 12(%0);
        movl %%fs, 16(%0);
        movl %%gs, 20(%0);
     " : /* no outputs */ : "r" (&st->registers[CS]));

    return True;
}
