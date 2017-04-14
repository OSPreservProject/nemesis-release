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
**      ns16550.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Definitions for the NS16550 UART. 
** 
** ENVIRONMENT: 
**
**      Serial line driver and primitive kernel output.
** 
** ID : $Id: ns16550.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _ns16550_h_
#define _ns16550_h_

/* Register offsets */
#define NS16550_RBR	0x00	/* receive buffer	*/
#define NS16550_THR	0x00	/* transmit holding	*/
#define NS16550_IER	0x01	/* interrupt enable	*/
#define NS16550_IIR	0x02	/* interrupt identity	*/
#define NS16550_FCR     0x02    /* FIFO control         */
#define NS16550_LCR	0x03	/* line control		*/
#define NS16550_MCR	0x04	/* MODEM control	*/
#define NS16550_LSR	0x05	/* line status		*/
#define NS16550_MSR	0x06	/* MODEM status		*/
#define NS16550_SCR	0x07	/* scratch		*/
#define NS16550_DDL	0x00	/* divisor latch (ls) ( DLAB=1)	*/
#define NS16550_DLM	0x01	/* divisor latch (ms) ( DLAB=1)	*/

/* Interrupt enable register */
#define NS16550_IER_ERDAI	0x01	/* rx data recv'd	*/
#define NS16550_IER_ETHREI	0x02	/* tx reg. empty	*/
#define NS16550_IER_ELSI	0x04	/* rx line status	*/
#define NS16550_IER_EMSI	0x08	/* MODEM status		*/

/* Interrupt identify register */
#define NS16550_IIR_PEND	0x01	/* NOT pending		*/
#define NS16550_IIR_ID		0x06	/* ID bits		*/
#define NS16550_IIR_RLS		0x06	/* Line status 		*/
#define NS16550_IIR_RDA		0x04	/* Rx data available	*/
#define NS16550_IIR_FIFO        0x0c    /* Old Rx data in fifo  */
#define NS16550_IIR_THRE	0x02	/* Tx holding empty	*/
#define NS16550_IIR_MODEM	0x00	/* Modem status		*/

/* FIFO control register */
#define NS16550_FCR_ENABLE      0x01    /* enable FIFO          */
#define NS16550_FCR_CLRX        0x02    /* clear Rx FIFO        */
#define NS16550_FCR_CLTX        0x04    /* clear Tx FIFO        */
#define NS16550_FCR_DMA         0x10    /* enter DMA mode       */
#define NS16550_FCR_TRG1        0x00    /* Rx FIFO trig lev 1   */
#define NS16550_FCR_TRG4        0x40    /* Rx FIFO trig lev 4   */
#define NS16550_FCR_TRG8        0x80    /* Rx FIFO trig lev 8   */
#define NS16550_FCR_TRG14       0xc0    /* Rx FIFO trig lev 14  */

/* Line control register */
#define NS16550_LCR_WLSEL	0x03	/* word length select	*/
#define NS16550_LCR_STB 	0x04	/* stop bits		*/
#define NS16550_LCR_PEN 	0x08	/* parity enable	*/
#define NS16550_LCR_EPS 	0x10	/* even parity select 	*/
#define NS16550_LCR_STICK	0x20	/* stick parity		*/
#define NS16550_LCR_SBRK	0x40	/* set break		*/
#define NS16550_LCR_DLAB	0x80	/* divisor latch access	*/

/* MODEM control register */
#define NS16550_MCR_DTR 	0x01	/* Data Terminal Ready	*/
#define NS16550_MCR_RTS 	0x02	/* Request to Send	*/
#define NS16550_MCR_OUT1        0x04    /* OUT1: unused         */
#define NS16550_MCR_OUT2        0x08    /* OUT2: interrupt mask */
#define NS16550_MCR_LOOP	0x10	/* Loop			*/

/* Line Status Register */
#define NS16550_LSR_DR  	0x01	/* Data ready		*/
#define NS16550_LSR_OE  	0x02	/* overrun error	*/
#define NS16550_LSR_PE  	0x04	/* parity error		*/
#define NS16550_LSR_FE  	0x08	/* frame error		*/
#define NS16550_LSR_BI  	0x10	/* break interrupt	*/
#define NS16550_LSR_THRE	0x20	/* tx holding empty	*/
#define NS16550_LSR_TEMT	0x40	/* transmitter empty	*/

/* MODEM status */
#define NS16550_MSR_DCTS	0x01	/* Delta CTS		*/
#define NS16550_MSR_DDSR	0x02	/* Delta DSR		*/
#define NS16550_MSR_TERI	0x04	/* trailing edge ring	*/
#define NS16550_MSR_DDCD	0x08	/* Delta DCD		*/
#define NS16550_MSR_CTS 	0x10	/* Clear to send	*/
#define NS16550_MSR_DSR 	0x20	/* Data Set Ready	*/
#define NS16550_MSR_RI		0x40	/* Ring Indicator	*/
#define NS16550_MSR_DCD		0x80	/* Data Carrier Detect	*/

#endif /* _ns16550_h_ */
