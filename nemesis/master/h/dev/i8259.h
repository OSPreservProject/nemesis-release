/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      i8259.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Definitions for the Intel i8259 Programmable Interrupt Controller.
**	(There are two of these on the VL82C486 System Controller on
**	the EB64).
** 
** ENVIRONMENT: 
**
**      Most device drivers need this, as does PALcode.
** 
** ID : $Id: i8259.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _i8259_h_
#define _i8259_h_

/*
 * This file only really talks about 8088/8086 mode. 
 */

/* Registers */
#define i8259_ICW1	0x00	/* initialisation control word 1	*/
#define i8259_ICW2	0x01	/* initialisation control word 2	*/
#define i8259_ICW3	0x01	/* initialisation control word 3	*/
#define i8259_ICW4	0x01	/* initialisation control word 4	*/
#define i8259_OCW1	0x01	/* operation control word 1	*/
#define i8259_OCW2	0x00	/* operation control word 2	*/
#define i8259_OCW3	0x00	/* operation control word 3	*/
#define i8259_IRR	0x00	/* interrupt request register	*/
#define i8259_ISR	0x00	/* in-service register		*/
#define i8259_PC	0x00	/* poll command			*/
#define i8259_IMR	0x01	/* interrupt mask register	*/

/* Masks for disambiguating accesses to 0x00 */
#define i8259_ICW1_MASK	0x10	/* Mask for ICW1 		*/
#define i8259_OCW2_MASK	0x00	/* Mask for OCW2 		*/
#define i8259_OCW3_MASK	0x08	/* Mask for OCW3 		*/

/* ICW1 format - options are 1/0 */
#define i8259_ICW1_LTIM	0x08	/* Level/edge triggered mode	*/
#define i8259_ICW1_ADI	0x04	/* call address interval 4/8	*/
#define i8259_ICW1_SNGL	0x02	/* single/cascade mode 		*/
#define i8259_ICW1_IC4	0x01	/* ICW4 needed/not needed	*/

/* ICW2/3 formats are trivial */

/* ICW4 format */
#define i8259_ICW4_SFNM	0x10	/* Level/edge triggered mode	*/
#define i8259_ICW4_NBUF	0x00	/* non-buffered mode		*/
#define i8259_ICW4_BFSL	0x08	/* buffered mode slave		*/
#define i8259_ICW4_BFMS	0x0C	/* buffered mode master		*/
#define i8259_ICW4_AEOI	0x02	/* Auto/normal EOI 		*/
#define i8259_ICW4_uPM	0x01	/* 8086/MCS-85 mode		*/

/* OCW2 format is more complex that this : this is biased towards */
/*  signalling end-of-interrupts on devices */
#define i8259_OCW2_R	0x80	/* Rotate			*/
#define i8259_OCW2_SL	0x40	/* Specific EOI			*/
#define i8259_OCW2_EOI	0x20	/* End-of-interrupt command	*/


/* OCW3 format */
#define i8259_OCW3_RIR	0x02	/* Read IRR			*/
#define i8259_OCW3_RIS	0x03	/* Read ISR			*/
#define i8259_OCW3_P	0x04	/* Poll command if set		*/
#define i8259_OCW3_RSM	0x20	/* Reset special mask		*/
#define i8259_OCW3_SSM	0x30	/* Set special mask		*/

#endif /* _i8259_h_ */
