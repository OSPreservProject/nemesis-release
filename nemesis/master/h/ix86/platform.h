/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Intel Defines
** 
** FUNCTIONAL DESCRIPTION:
** 
** 
** ENVIRONMENT: 
**
**      Kernel mode register access
** 
** ID : $Id: platform.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
**
*/

#ifndef _ix86_h_
#define _ix86_h_

/*
** Intel 82C59A Priority Interrupt Controller (PIC) Definitions:
*/

#define PIC1		0x20	/* INT0 Megacell Address */
#define PIC2		0xA0	/* INT1 Megacell Address */

#define ICW1		0	/* Initialization Control Word 1 Offset */
#define ICW2		1	/* Initialization Control Word 2 Offset */
#define ICW3		1	/* Initialization Control Word 3 Offset */
#define ICW4		1	/* Initialization Control Word 4 Offset */

#define OCW1		1	/* Operation Control Word 1 Offset */
#define OCW2		0	/* Operation Control Word 2 Offset */
#define OCW3		0	/* Operation Control Word 3 Offset */

#define PIC1_ICW1	(PIC1+ICW1)
#define PIC1_ICW2	(PIC1+ICW2)
#define PIC1_ICW3	(PIC1+ICW3)
#define PIC1_ICW4	(PIC1+ICW4)
#define PIC1_OCW1	(PIC1+OCW1)
#define PIC1_OCW2	(PIC1+OCW2)
#define PIC1_OCW3	(PIC1+OCW3)
			 
#define PIC2_ICW1	(PIC2+ICW1)
#define PIC2_ICW2	(PIC2+ICW2)
#define PIC2_ICW3	(PIC2+ICW3)
#define PIC2_ICW4	(PIC2+ICW4)
#define PIC2_OCW1	(PIC2+OCW1)
#define PIC2_OCW2	(PIC2+OCW2)
#define PIC2_OCW3	(PIC2+OCW3)

#if 0
/*
** Dallas DS1287A Real-Time Clock (RTC) Definitions:
*/

#define RTCADD     	0x70	/* RTC Address Register */
#define RTCDAT     	0x71	/* RTC Data Register */

/*
** LED for debugging
*/
#define LEDPORT     	0x80	/* LED Port */

/*
** NS 87312 SuperI/O configuration registers
*/
#define SUPERIO_INDEX 0x398
#define SUPERIO_DATA  0x399
#endif /* 0 */

/*
** Serial Port (COM) Definitions:
*/

#define COM1			0x3F8	/* COM1 Serial Line Port Address */
#define COM2			0x2F8	/* COM2 Serial Line Port Address */

#define RBR			0	/* Receive Buffer Register Offset */
#define THR			0	/* Xmit Holding Register Offset */
#define DLL			0	/* Divisor Latch (LS) Offset */
#define DLH			1	/* Divisor Latch (MS) Offset */
#define IER			0x1	/* Interrupt Enable Register Offset */
#define IIR			0x2	/* Interrupt ID Register Offset */
#define LCR			0x3	/* Line Control Register Offset */
#define MCR			0x4	/* Modem Control Register Offset */
#define LSR			0x5	/* Line Status Register Offset */
#define MSR			0x6	/* Modem Status Register Offset */
#define SCR			0x7	/* Scratch Register Offset */

#define DLA_K_BRG		12	/* Baud Rate Divisor = 9600 */

#define LSR_V_THRE		5	/* Xmit Holding Register Empty Bit */

#define LCR_M_WLS		3	/* Word Length Select Mask */
#define LCR_M_STB		4	/* Number Of Stop Bits Mask */
#define LCR_M_PEN		8	/* Parity Enable Mask */
#define LCR_M_DLAB		128	/* Divisor Latch Access Bit Mask */

#define LCR_K_INIT	      	(LCR_M_WLS | LCR_M_STB)

#define MCR_M_DTR		1	/* Data Terminal Ready Mask */
#define MCR_M_RTS		2	/* Request To Send Mask */
#define MCR_M_OUT1		4	/* Output 1 Control Mask */
#define MCR_M_OUT2		8	/* UART Interrupt Mask Enable */

#define MCR_K_INIT	      	(MCR_M_DTR  | \
				 MCR_M_RTS  | \
				 MCR_M_OUT1 | \
				 MCR_M_OUT2)

#define COM1_RBR		(COM1+RBR)
#define COM1_THR		(COM1+THR)
#define COM1_DLL		(COM1+DLL)
#define COM1_DLH		(COM1+DLH)
#define COM1_IER		(COM1+IER)
#define COM1_IIR		(COM1+IIR)
#define COM1_LCR		(COM1+LCR)
#define COM1_MCR		(COM1+MCR)
#define COM1_LSR		(COM1+LSR)
#define COM1_MSR		(COM1+MSR)
#define COM1_SCR		(COM1+SCR)
				 
#define COM2_RBR		(COM2+RBR)
#define COM2_THR		(COM2+THR)
#define COM2_DLL		(COM2+DLL)
#define COM2_DLH		(COM2+DLH)
#define COM2_IER		(COM2+IER)
#define COM2_IIR		(COM2+IIR)
#define COM2_LCR		(COM2+LCR)
#define COM2_MCR		(COM2+MCR)
#define COM2_LSR		(COM2+LSR)
#define COM2_MSR		(COM2+MSR)
#define COM2_SCR		(COM2+SCR)

#endif /* _ix86_h_ */
