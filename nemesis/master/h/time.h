/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      time.h
** 
** DESCRIPTION:
** 
**      Handy macros for dealing with time.
** 
** ID : $Id: time.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _time_h_
#define _time_h_

#include <Time.ih>

/* 
 * Time stuff - results are "Time.T"s, ie. nanoseconds
 */

#define SECONDS(_s)			(((Time_T)(_s))  * 1000000000UL )
#define TENTHS(_ts)			(((Time_T)(_ts)) * 100000000UL )
#define HUNDREDTHS(_hs)			(((Time_T)(_hs)) * 10000000UL )
#define MILLISECS(_ms)			(((Time_T)(_ms)) * 1000000UL )
#define MICROSECS(_us)			(((Time_T)(_us)) * 1000UL )

#define NOW()				(Time$Now (Pvs(time)))

#define Time_Max			((Time_T) 0x7fffffffffffffffLL)
#define FOREVER				Time_Max

/* IN_FUTURE is a predicate to determine whether a time value is in
   the future. It will improve performance for situations where a
   timeout value is quite likely to be either 0 (don't block) or
   FOREVER (block with no timeout), since in both these situations it
   avoids invoking NOW(). It will marginally worsen performance when
   the value being checked is guaranteed to be a real time value,
   since in that case it performs a couple of useless 64-bit tests, so
   don't use it in such situations ... */

#define IN_FUTURE(_until)               			\
  ((_until) && (((_until) == FOREVER) || ((_until) > NOW())))

#endif /* _time_h_ */

