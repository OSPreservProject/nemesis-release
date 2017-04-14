/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.		                                    *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      h/ix86/irq.h
**
** FUNCTIONAL DESCRIPTION:
** 
**      Intel IRQ definitions
** 
** ENVIRONMENT: 
**
**      For use by privileged domains 
** 
** FILE :	irq.h
** CREATED :	Mon Sep  2 11:23:07 BST 1996
** AUTHOR :	Austin Donnelly (Austin.Donnelly@cl.cam.ac.uk),
**		 (heavily based on the eb164 one by Paul Barham)
** ID : 	$Id: irq.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
*/

#ifndef _IRQ_H_
#define _IRQ_H_

#include <io.h>  /* for outb() */
#include <kernel_st.h>

#define N_IRQS             16

#define IRQ_M_PIC1 	0x000000FF /* Master PIC */
#define IRQ_V_PIC1      0
#define IRQ_M_PIC2 	0x0000FF00 /* Slave PIC */
#define IRQ_V_PIC2      8
#define IRQ_M_ALL       (IRQ_M_PIC1|IRQ_M_PIC2)
#define IRQ_V_MAX       16

#define IRQ_V_CNT0      0
#define IRQ_V_KEYB	1
#define IRQ_V_CASC	2
#define IRQ_V_COM2      3
#define IRQ_V_COM1      4

#define IRQ_V_TICKER	8
#define IRQ_V_MATHCOPRO	13
#define IRQ_V_IDE0	14
#define IRQ_V_IDE1	15

/*
 * Bits in interrrupt controller #1:
 */
#define IRQ_V_PIC1_CASC   (IRQ_V_CASC+IRQ_V_PIC1)  
#define IRQ_V_PIC1_CNT0   (IRQ_V_CNT0+IRQ_V_PIC1)  

#define IRQ_M_PIC1_CASC   (1<<IRQ_V_PIC1_CASC)  /* PIC2 cascaded */
#define IRQ_M_PIC1_CNT0   (1<<IRQ_V_PIC1_CNT0)  /* Timer counter 0 */

/*
 * ACK an IRQ in the PICs
 * (uses PIC's rotating mode: round-robin processing of interrupts)
 */
extern INLINE void ack_irq(int irq)
{
#if 0
    if (irq > 7) {
        /*  First the slave .. */
        outb(0x60 | (irq - 8), 0xa0);
        irq = 2;
    }
    /* .. then the master */
    outb(0x60 | irq, 0x20);
#endif /* 0 */
    if (irq > 7) {
	outb(0x20, 0xa0);
    }
    outb(0x20, 0x20);
}

/*
 * libsystem.a provides device drivers with a generic IRQ stub which requires
 * a state record as below.
 */
typedef struct {
    /* Interrupt stub state */
    dcb_ro_t       *dcb;        /* DCB of destination */
    Channel_EP     ep;          /* Interrupt Channel */
} irq_stub_st;

extern void _generic_stub(irq_stub_st *st, const kinfo_t *kinfo);


#endif /* _IRQ_H_ */
