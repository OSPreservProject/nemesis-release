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
**      Dallas Semiconductor DS1287A Real Time Clock chip information
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Provides real time clock, time-of-year, and NVRAM.
**      This is also found as a macrocell on the VL82C113 SCAMP. 
** 
** ENVIRONMENT: 
**
**      Hardware
** 
** ID : $Id: ds1287a.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
** LOG: $Log: ds1287a.h,v $
** LOG: Revision 1.1  1995/01/17 14:12:14  prb12
** LOG: Initial revision
** LOG:
 * Revision 1.1  1994/11/04  13:53:27  tr
 * Initial revision
 *
# Revision 1.1  1994/09/09  14:41:28  tr
# Initial revision
#
**
*/

#ifndef _ds1287a_h_
#define _ds1287a_h_

/* Real-time clock module registers. */

#define DS1287_TIME_SEC		0x00	/* Seconds (time)	*/
#define DS1287_ALRM_SEC		0x01	/* Seconds (alarm)	*/
#define DS1287_TIME_MIN		0x02	/* Minutes (time)	*/
#define DS1287_ALRM_MIN		0x03	/* Minutes (alarm)	*/
#define DS1287_TIME_HRS		0x04	/* Hours (time) 	*/
#define DS1287_ALRM_HRS		0x05	/* Hours (alarm) 	*/
#define DS1287_DOW		0x06	/* Day of the week	*/
#define DS1287_DATE		0x07	/* Day of the month	*/
#define DS1287_MONTH		0x08	/* Month  		*/
#define DS1287_YEAR		0x09	/* Year			*/
#define DS1287_A		0x0a	/* RTC Register A (r/w)	*/
#define DS1287_B		0x0b	/* RTC Register B (r/w)	*/
#define DS1287_C		0x0c	/* RTC Register C (r/o)	*/
#define DS1287_D		0x0d	/* RTC Register D (r/o)	*/

/* RTC Register A definitions */
#define DS1287_A_RATE		0x0f	/* Mask for periodic int. rate	*/
#define DS1287_A_DIVON		0x20	/* Divider on and running 	*/
#define DS1287_A_DIVRES		0x60	/* Divider resetting	 	*/
#define DS1287_A_UIP		0x80	/* Update in progress		*/

/* Periodic interrupt timings */
#define DS1287_A_2 		0x0f	/* 500 ms 	*/
#define DS1287_A_4 		0x0e	/* 250 ms 	*/
#define DS1287_A_8 		0x0d	/* 125 ms 	*/
#define DS1287_A_16		0x0c	/* 62.5 ms 	*/
#define DS1287_A_32		0x0b	/* 31.25 ms 	*/
#define DS1287_A_64		0x0a	/* 15.625 ms 	*/
#define DS1287_A_128		0x09	/* 7.8125 ms 	*/
#define DS1287_A_256		0x08	/* 3.90625 ms 	*/
#define DS1287_A_512		0x07	/* 1.953125 ms 	*/
#define DS1287_A_1024		0x06	/* 976.562 us 	*/
#define DS1287_A_2048		0x05	/* 448.281 us 	*/
#define DS1287_A_4096		0x04	/* 244.141 us 	*/
#define DS1287_A_8192		0x03	/* 122.070 us 	*/

/* RTC Register B definitions */
#define DS1287_B_DSE		0x01	/* Daylight savings enable	*/
#define DS1287_B_24OR12		0x02	/* 1 = 24-hour mode	 	*/
#define DS1287_B_DM		0x04	/* Data mode, 1=binary, 0=BCD 	*/
#define DS1287_B_UIE		0x10	/* Update-end interrupt enable	*/
#define DS1287_B_AIE		0x20	/* Alarm interrupt enable	*/
#define DS1287_B_PIE		0x40	/* Periodic interrupt enable	*/
#define DS1287_B_SET		0x80	/* Set Command			*/

/* RTC Register C definitions : this register is read-only */
#define DS1287_C_UF		0x10	/* Update-end interrupt pending	*/
#define DS1287_C_AF		0x20	/* Alarm interrupt pending	*/
#define DS1287_C_PF		0x40	/* Periodic interrupt pending	*/
#define DS1287_C_IRQF		0x80	/* Any interrupt pending	*/

/* RTC Register D definitions : this register is read-only */
#define DS1287_D_VRT		0x80	/* Valid RAM data and time	*/

#endif /* _ds1287a_h_ */
