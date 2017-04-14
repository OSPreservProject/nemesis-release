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
**      i8254.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Definitions for the Intel i8254 Programmable Interval Timer
**      There is one of these on the VL82C486 System Controller on the
**	EB64.
**
** ENVIRONMENT: 
**
**      Used by the EB64 timer module for scheduling. Can also be used
**	for playing tunes!
** 
** ID : $Id: i8254.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _i8254_h_
#define _i8254_h_

/*
 * This file contains a bunch of handy definitions for the 
 * Intel 8254 timer cell found on the EB64 SC486 System Controller.
 */

/* Registers */
#define i8254_C0	0x00	/* counter 0 	*/
#define i8254_C1	0x01	/* counter 1 	*/
#define i8254_C2	0x02	/* counter 2	*/
#define i8254_CW	0x03	/* control word	*/

/*
The control word to the 8254 is composed of four fields:
 bits 6-7: select the counter
 bits 4-5: select read/write
 bits 1-3: mode to use
 bit  0  : BCD mode
*/

/* bits 6-7 select the counter */
#define	i8254_CW_SEL0	0x00	/* select counter 0	*/
#define	i8254_CW_SEL1	0x40	/* select counter 1	*/
#define	i8254_CW_SEL2	0x80	/* select counter 2	*/
#define	i8254_CW_RBC	0xC0	/* read-back command	*/

/* bits 4-5 select transaction type */
#define	i8254_CW_CLC	0x00	/* counter latch comm.	*/
#define	i8254_CW_LSB	0x10	/* r/w lsb only		*/
#define	i8254_CW_MSB	0x20	/* r/w msb only		*/
#define	i8254_CW_BOTH	0x30	/* r/w lsb, then msb	*/

/* bits 1-3 select the mode. bit 3 is sometimes a don't care */
#define	i8254_CW_MODE0	0x00	/* int. on term. count	*/
#define	i8254_CW_MODE1	0x02	/* h/w retrig. one-shot	*/
#define	i8254_CW_MODE2	0x04	/* rate generator	*/
#define	i8254_CW_MODE3	0x06	/* square wave mode	*/
#define	i8254_CW_MODE4	0x08	/* s/w trig. strobe	*/
#define	i8254_CW_MODE5	0x0A	/* h/w trig. strobe	*/

/* bit 0 sets BCD mode, if you really must */
#define	i8254_CW_BCD	0x01	/* set BCD operation	*/
 
/* read-back commands use bits 4 and 5 to return status */
/*  these are the wrong way round since the bits are    */
/*  inverted (RTFDS).					*/
#define i8254_RB_COUNT	0x10	/* latch count value 	*/
#define i8254_RB_STAT	0x20	/* latch status value 	*/

/* read-back commands use bits 3-1 to select counter */
#define i8254_RB_SEL0	0x02	/* counter 0 		*/
#define i8254_RB_SEL1	0x04	/* counter 1 		*/
#define i8254_RB_SEL2	0x08	/* counter 2 		*/

/* status from read-back is returned in bits 6-7 */
#define i8254_RB_OUT	0x80	/* out pin value 	*/
#define i8254_RB_NULL	0x40	/* 1 = count null	*/

#endif /* _i8254_h_ */
