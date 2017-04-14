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
**      Interrupt allocator
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implementation of Interrupt.if
** 
** ENVIRONMENT: 
**
**      Nemesis domain. Privileged to make interrupt allocate syscalls
** 
** ID : $Id: IntAlloc.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <stdio.h>
#include <kernel_st.h>
#include <ntsc.h>
#include <irq.h>

#include <Interrupt.ih>
#include <Heap.ih>
#include <Domain.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static Interrupt_Allocate_fn  Allocate_m;
static Interrupt_Free_fn      Free_m;
static Interrupt_op int_ops = {Allocate_m, Free_m};

/* IRQ ownership and state structure */
/* Note the memory access issues: st->owner is in user space, and
 * might at some point be pageable. This is a bad idea if we're
 * disabling interrupts. Also, st->irq_dispatch[] is in kernel memory,
 * and so this domain will require write access to kernel mem. */
typedef struct {
    Interrupt_cl cl;
    irq_desc	*irq_dispatch;   /* pointer to kernel dispatch table */
    Domain_ID	 owner[N_IRQS];  /* owner of each irq                */
    /* id=0 -> no owner
       id=1 -> kernel owned */
} int_st;

Interrupt_clp InterruptInit(kernel_st *kst)
{
    int i;
    int_st *st;

    st = Heap$Malloc(Pvs(heap), sizeof(*st));
    if (!st) {
	fprintf(stderr,"InterruptInit: no memory\n");
	return NULL;
    }
    CL_INIT(st->cl, &int_ops, st);

    st->irq_dispatch = ((kernel_st *)kst)->irq_dispatch;
    TRC(eprintf("Interrupt$New: IRQ Dispatch table at %p\n", 
		st->irq_dispatch));
    for(i=0; i<N_IRQS; i++)
	st->owner[i] = 0;

    /* now fill in the kernel reserved interrupts */
#ifdef __ix86__
    st->owner[0] = 1;  /* interval timer */
    st->owner[8] = 1;  /* RTC timer */
#elif defined(EB64)
    /* on eb64, we've already installed one devint, and one swint */
    st->owner[IRQ_V_CNT0]= 1;
    st->owner[IRQ_V_SCHED]= 1;
#elif defined(EB164)
    /* Why isn't this owner thing part of the kernel state so that 
       mask/unmask IRQ can check it? */
    st->owner[IRQ_V_CNT0] = 1;	/* Interval timer */
    st->owner[IRQ_V_CASC] = 1;	/* PIC cascade */
#elif defined(SRCIT)
    /* So far the only interrupt is the alarm going off. Also tie down
     * the bits in the CSR which ought not to be touched. Note that
     * IRQ_BIT_ALARM == IRQ_BIT_RAM. */
    st->owner[IRQ_BIT_ALARM] = 1;
    st->owner[IRQ_BIT_RAM] = 1;
    st->owner[IRQ_BIT_RESET] = 1;
    st->owner[IRQ_BIT_GREEN] = 1;
    st->owner[IRQ_BIT_RED] = 1;
    st->owner[IRQ_BIT_YELLOW] = 1;
#elif defined(RISCPC)
    st->owner[IRQ_BIT_ALARM] = 1;
    st->owner[IRQ_BIT_PERIODIC] = 1;
#elif defined(SHARK)
    st->owner[IRQ_TIMER0] = 1;	/* Interval timer */
    st->owner[IRQ_CASCADE] = 1;	/* PIC cascade */
#else
#warning You probably want to fill in some reserved IRQs for your arch here
#endif

    return &st->cl;
}   


/* --- Interrupt.if implementation ------------------------------ */


Interrupt_AllocCode Allocate_m(Interrupt_cl     *self,
			       Interrupt_IrqDesc irq,			       
			       Interrupt_StubFn  stub,
			       Interrupt_StateT  state)
{
    int_st *st = self->st;
    word_t ipl;
    irq_desc newirq;
    Domain_ID clientid = 2; /* XXX I can't believe this is still here!
			       We need to add an IDCCallback so we can
			       find out who is calling. */

    if (irq < 0 || irq >= N_IRQS)
	return Interrupt_AllocCode_InvalidIRQ;

    newirq.stub = stub;
    newirq.state = state;

    ipl = ntsc_ipl_high(); /* before checking owner state */

    if (st->owner[irq] != 0)
    {
	ntsc_swpipl(ipl);
	return Interrupt_AllocCode_AlreadyClaimed;
    }

    /* all ok, so let's do it! */
    st->owner[irq] = clientid;
    st->irq_dispatch[irq] = newirq;  /* NOTE: this is a write to NTSC mem */

    ntsc_swpipl(ipl);

    TRC(eprintf("Interrupt$Allocate: allocated IRQ %d to Dom %d\n", 
		irq, clientid));

    return Interrupt_AllocCode_Ok;
}


Interrupt_FreeCode Free_m(Interrupt_cl     *self,
			  Interrupt_IrqDesc irq)
{
    word_t ipl;
    int_st *st = self->st;
    Domain_ID clientid = 2; /* XXX should be dynamically detected
			       (see earlier comment) */

    if (irq < 0 || irq >= N_IRQS)
	return Interrupt_FreeCode_InvalidIRQ;

    ipl = ntsc_ipl_high();

    if (st->owner[irq] == 0)
    {
	ntsc_swpipl(ipl);
	return Interrupt_FreeCode_NoStub;
    }

    if (st->owner[irq] != clientid)
    {
	ntsc_swpipl(ipl);
	return Interrupt_FreeCode_NotOwner;
    }

    /* ok, lets go! */
    st->irq_dispatch[irq].stub  = BADPTR; /* NOTE: a write to kernel mem */
    st->irq_dispatch[irq].state = BADPTR;
    st->owner[irq] = 0;

    ntsc_swpipl(ipl);

    return Interrupt_FreeCode_Ok;
}
