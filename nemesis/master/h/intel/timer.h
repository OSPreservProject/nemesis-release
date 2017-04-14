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
**      Timer header
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Calibration information for timer
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: timer.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _timer_h_
#define _timer_h_

/* Timer information for rdtsc-based timer */
/* Accuracy of calculation. NB these values need to be set so that the
   difference in cycle count between two ticker interrupts multiplied by
   COUNT_MULTIPLIER doesn't overflow a 32 bit unsigned integer */
#define PROCSPD_DIVISOR (10000000)
#define COUNT_MULTIPLIER (1000000000/PROCSPD_DIVISOR)


/* Timer information for ticker-based timer */
#define TICKER_COUNT 145
#define TICKER_INTERVAL (TICKER_COUNT*838)

#endif /* _timer_h_ */
