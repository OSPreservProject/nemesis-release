/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Glasgow                                   *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**	Wall_Clock.c
**	
**
** FUNCTIONAL DESCRIPTION:
**	Implements two interfaces : WTime.if which provides the functions 
**      for getting the time and for manipulating time values; and
**      WTimeUpdate.if which provides the functions for updating the
**      variables used to calculate wall-clock time.
**      **ONLY** deals in UTC time.
**	
**
** ENVIRONMENT: 
**	User for WTime.if implementation.
**	Kernel for WTimeUpdate implementation.
**
**
** CREATED : January 1998
** AUTHOR :  Ian McDonald <ian@dcs.gla.ac.uk>
**
** ID : $Id: Wall_Clock.c 1.2 Wed, 19 May 1999 10:57:27 +0100 dr10009 $
**
**
*/



/*
 * Copyright (c) 1987, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Arthur David Olson of the National Cancer Institute.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <ntsc.h>
#include <stdio.h>
#include <stdlib.h>
#include <WTime.ih>
#include <WTimeUpdate.ih>


#if defined(__IX86__) || defined(EB164)
#include <pip.h>
#include <RTC.ih>
#endif

#if 0
#ifndef WTIME_DEBUG
#define WTIME_DEBUG
#endif
#endif

#ifdef WTIME_DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED", __FUNCTION__);


static	WTime_GetTimeOfDay_fn 		WTime_GetTimeOfDay_m;
static	WTime_MkTime_fn 		WTime_MkTime_m;
static	WTime_AscTime_fn 		WTime_AscTime_m;
static	WTime_LocalTime_fn 		WTime_LocalTime_m;
static	WTime_CTime_fn 			WTime_CTime_m;
static	WTime_GMTime_fn 		WTime_GMTime_m;
static	WTime_DayOfWeek_fn 		WTime_DayOfWeek_m;
static	WTime_MakeTime_fn 		WTime_MakeTime_m;

static	WTimeUpdate_InitTime_fn 	WTimeUpdate_InitTime_m;
static	WTimeUpdate_AdjustOffset_fn 	WTimeUpdate_AdjustOffset_m;
static	WTimeUpdate_ShallowUpdate_fn 	WTimeUpdate_ShallowUpdate_m;

static	WTimeUpdate_op	ums = {
  WTimeUpdate_InitTime_m,
  WTimeUpdate_AdjustOffset_m,
  WTimeUpdate_ShallowUpdate_m
};


static	WTime_op	ms = {
  WTime_GetTimeOfDay_m,
  WTime_MkTime_m,
  WTime_AscTime_m,
  WTime_LocalTime_m,
  WTime_CTime_m,
  WTime_GMTime_m,
  WTime_DayOfWeek_m,
  WTime_MakeTime_m,
};


static const	WTime_cl	cl = { &ms, NULL };
static	const WTimeUpdate_cl	ucl = { &ums, NULL };

CL_EXPORT (WTimeUpdate, WTimeUpdate, ucl);
CL_EXPORT (WTime, WTime, cl);

#define SECSPERMIN         60
#define MINSPERHOUR        60
#define HOURSPERDAY        24
#define DAYSPERWEEK        7
#define MONSPERYEAR        12
#define DAYSPERNYEAR       365
#define DAYSPERLYEAR       366
#define SECSPERHOUR        (SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY         ((long) SECSPERHOUR * HOURSPERDAY)
#define PM_BASE            0x80
#define TM_YEAR_BASE       1900

#define TM_SUNDAY       0
#define TM_MONDAY       1
#define TM_TUESDAY      2
#define TM_WEDNESDAY    3
#define TM_THURSDAY     4
#define TM_FRIDAY       5
#define TM_SATURDAY     6

#define EPOCH_YEAR      70
#define EPOCH_WDAY      TM_THURSDAY
#define ITIMER_REAL     0


#define FAILURE -1
#define SUCCESS 0
#define POSITIVE 0
#define NEGATIVE 1
#define HOUR_12 0


#define HOUR_24 1
#define isleap(y) (((((y) % 4) == 0) && (((y) % 100) != 0)) || 	     \
		   (((y) % 400) == 0))
#define ERROR(x) printf("### ERROR : %s in %s (%s) , line %d ###\n", \
			x,__FUNCTION__,__FILE__,__LINE__)

typedef struct {
    uint32_t Xl_ui;
    uint32_t Xl_uf;
} u_fixedpoint;

static char *const wdays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"};
static char *const month[] = {
    "January", "February", "March", "April",
    "May", "June", "July", "August", "September", "October",
    "November", "December"};
static int16_t const mon_lengths[2][MONSPERYEAR] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};
static int const year_lengths[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

static char *const tz_names[] = {
  "UTC"
};



/* Our function declarations */
static void normalise(int16_t *const, int16_t *const, const int16_t);
static void
extract_time(const WTime_Time_T *timep, const int32_t offset, WTime_TM *tmp);
static void cget_time(WTime_TimeVal*);
/* multiplies value by scaling factor */
static uint64_t multSF(const uint64_t);
/* calculates scaling factor */
static uint64_t FPdiv(const uint64_t, const uint64_t);
/* retrieves closure pointer for RTC */
static RTC_clp getRTCClosure();




/*
 * Divides 2 64-bit quantities producing a 64-bit fixed-point
 * result - the scaling factor.
 * How it works: First you divide num1 by num2 and store
 * that in the integer part of the result. This becomes the 
 * the top 32-bits of our result. Then, we take the
 * remainder, shift it left 32-bits to keep our accuracy
 * and divide it by num2 as well. This becomes the bottom 32-bits
 * of our result.
 * There is a potential problem in that the remainder could be
 * greater than 2^32. To combat this, we check to see if this
 * is the case and, if it is, we shift the remainder as far to
 * the left as we can and shift num2 in the other direction by
 * 32 - the number of left shifts of the remainder.
 * 
 */
static uint64_t FPdiv(const uint64_t num1, const uint64_t num2)
{
    uint64_t tmp, rem, div;
    uint64_t int_part = 0;
    int count = 0;
    const uint64_t big_num = 0x0000000100000000LL; /* 2^32 */
    const uint64_t  max = 0x8000000000000000LL; 

    TRC(printf("In FPdiv \n"));

    int_part = (num1 / num2); /* quotient */
    rem = (num1 - (int_part * num2)); /* remainder */
    div = num2;

    /* check that integer part has not overflowed */
    if(int_part >= big_num){
	ERROR("DIVISION OVERFLOW");
	TRC(printf("int_part was larger than 2^32\n"));
	TRC(printf("num1 is %qx, num2 is %qx \n",num1,num2));
	TRC(printf("int_part is %qx \n",int_part));
    }
    if(rem >= big_num){
	tmp = rem;
	if(!(tmp && 0xFFFF000000000000LL)){
	    tmp <<= 16;
	    count += 16;
	}
	if(!(tmp && 0xFF00000000000000LL)){
	    tmp <<= 8;
	    count += 8;
	}
	if(!(tmp && 0xF000000000000000LL)){
	    tmp <<= 4;
	    count += 4;
	}
	if(!(tmp && 0xB000000000000000LL)){
	    tmp <<= 2;
	    count += 2;
	}
	if(!(tmp && max)){
	    tmp <<= 1;
	    count += 1;
	}
	/* now the divisor */
	div >>= (32 - count);
	TRC(printf("rem was greater than big_num \n"));
    }
    else
	tmp = (rem << 32);

    TRC(printf("num1 is %qx, num2 is %qx \n",num1,num2));
    TRC(printf("int_part is %qx \n",int_part));
    TRC(printf("count is %d \n", count));
    TRC(printf("remainder is %qx \n", rem));
    TRC(printf("temp is %qx \n"));


    int_part <<= 32;
    return (int_part + (tmp / div));
}

/*
 * Performs the multiplication of a 64-bit value
 * with a 64-bit fixed-point value (the scaling factor).
 * Particular attention should be paid to the imaginitive
 * variable names.
 */
static uint64_t multSF(const uint64_t value)
{
    uint64_t lo, hi;
    uint64_t a, b, c, d, e, f;
    u_fixedpoint sf;
    uint64_t hi_mask = 0xFFFFFFFF00000000LL;
    uint64_t lo_mask = 0x00000000FFFFFFFFLL;


    sf.Xl_uf = INFO_PAGE.NTPscaling_factor; /* throw top 32-bits away */
    sf.Xl_ui = INFO_PAGE.NTPscaling_factor >> 32; 

    TRC(printf("The scaling factor is %qx \n", INFO_PAGE.NTPscaling_factor));
    TRC(printf("Mult val is %qx \n", value));

    lo = (value & lo_mask);
    hi = (value & hi_mask) >> 32;
    a = ((uint64_t)sf.Xl_uf) * lo;
    b = ((uint64_t)sf.Xl_uf) * hi;
    c = ((uint64_t)sf.Xl_ui) * lo;
    d = ((uint64_t)sf.Xl_ui) * hi;
    e = (a >> 32) + (b & lo_mask) + (c & lo_mask);
    
    if(a && 0x0000000080000000LL) /* bit 32 */
	e++;

    f = (d & lo_mask) + (b >> 32) + (c >> 32);
    if((f >> 32) || (d & hi_mask)) /* catch overflow */
	ERROR("MULTIPLICATION OVERFLOW");
    
    return ((f << 32) + e);
}


/*****************************************************************************
**
** Implementation of WTime.if
******************************************************************************/


/*
 * Calculates current time, splits it into seconds
 * and micro-seconds and inserts it into
 * a timeval struct for your pleasure.
 * The time sponsored by nemesis is:
 * the
 * previous ntp_time + (current sched_time - prev_sched_time) * scaling factor
 */
static void cget_time(WTime_TimeVal *tv)
{
    uint64_t current;
    uint64_t now;

    now = NOW();
    current = (now - INFO_PAGE.prev_sched_time);
    current = multSF(current) + INFO_PAGE.ntp_time;

    tv->tv_sec = current / SECONDS(1);
    tv->tv_usec = (current % SECONDS(1)) / 1000;
}


/*
 * Returns the time from the system clock after applying
 * a scaling factor to it.
 * Note that gettimeofday deals only in wall-clock time,
 * not scheduler time (although this is used in the calculation). 
 */
static int16_t WTime_GetTimeOfDay_m (
	WTime_cl	*self,
	WTime_TimeVal	*tval	/* OUT */ )
{
  uint64_t current;

  TRC(printf("In gettimeofday().\n"));
  if(tval == NULL){
      ERROR("NULL pointer to timeval struct");
      return FAILURE;
  }
  /* we use ntp_time as the method of concurrency control
   * in preference to the scaling factor as the latter is
   * not updated during a `shallow update' while the former is.
   */
  current  = INFO_PAGE.ntp_time; 
  cget_time(tval);
  if(current != INFO_PAGE.ntp_time) /* if we get the time during an update */
      cget_time(tval);
  return SUCCESS;
}



/*
 * A handy wee function for normalising values.
 */
static void
normalise(int16_t *const tensptr, int16_t *const unitsptr, const int16_t base)
{
    if (*unitsptr >= base) {
	*tensptr += *unitsptr / base;
	*unitsptr %= base;
    } else if (*unitsptr < 0) {
	*tensptr -= 1 + (-(*unitsptr + 1)) / base;
	*unitsptr = base - 1 - (-(*unitsptr + 1)) % base;
    }
}



/*
 * Takes a pointer to a tm struct and returns the seconds since
 * 1/1/1970. Also, we are supposed to assume that the fields
 * are awry and therefore we have to fix them.
 * tm_wday and tm_yday are ignored for computations but fixed
 * within the tm structure to the correct values determined from
 * the other values.
 *
 * NOTE: This will not cope with 12 hour mode - far too disgusting.
 * My advice is that if you are using 12 hour mode then don't touch this.
 *
 * Based on the BSD timesub function in ctime.c
 */
static WTime_Time_T WTime_MkTime_m (
	WTime_cl	*self,
	WTime_TM	*tmptr	/* OUT */ )
{
  int16_t i, index;
  if(tmptr == NULL)
      return FAILURE;
  TRC(printf("In mktime().\n"));
  normalise(&tmptr->tm_min, &tmptr->tm_sec, SECSPERMIN);      /* fix seconds */
  normalise(&tmptr->tm_hour,&tmptr->tm_min, MINSPERHOUR);     /* fix minutes */
  normalise(&tmptr->tm_mday, &tmptr->tm_hour, HOURSPERDAY);   /* fix hours */
  normalise(&tmptr->tm_year, &tmptr->tm_mon, MONSPERYEAR);    /* fix months */

	
  while (tmptr->tm_mday <= 0) {
      --tmptr->tm_year;
      tmptr->tm_mday +=
	  year_lengths[isleap(tmptr->tm_year)];
  }

  while (tmptr->tm_mday > DAYSPERLYEAR) {
      tmptr->tm_mday -=
	  year_lengths[isleap(tmptr->tm_year)];
      ++tmptr->tm_year;
  } 
    
  for ( ; ; ) {      /* for mday > days in month */
      i = mon_lengths[isleap(tmptr->tm_year)][tmptr->tm_mon - 1];
      if (tmptr->tm_mday <= i)
	  break;	
      tmptr->tm_mday -= i;
      if (++tmptr->tm_mon > MONSPERYEAR) {
	  tmptr->tm_mon = 1;
	  ++tmptr->tm_year;
      }
  }
  tmptr->tm_yday = 0;
  tmptr->tm_wday = WTime_DayOfWeek_m(self,tmptr->tm_mday,
				     tmptr->tm_mon, tmptr->tm_year);
  index = isleap(tmptr->tm_year);
  for(i = 0; i < (int)tmptr->tm_mon; i++)
      tmptr->tm_yday += mon_lengths[index][i];

  return(WTime_MakeTime_m(self,tmptr->tm_mday, tmptr->tm_mon, tmptr->tm_year,
		  tmptr->tm_hour,tmptr->tm_min,tmptr->tm_sec));

  TRC(printf("The number of months is %d \n",tmptr->tm_mon));
}


/* number of chars */
#define MAX_DATE_LEN 40
/*
 * Returns string rep'n of time and date or NULL.
 */
static string_t WTime_AscTime_m (
	WTime_cl	*self,
	const WTime_TM	*tmptr	/* IN */ )
{
  char	*timestr;
  TRC(printf("In asctime().\n"));
  if(tmptr == NULL)
      return NULL;
  timestr = (char *)malloc(sizeof(char) * MAX_DATE_LEN);
  
  /* 
   * Because there is no out of bounds checking for
   * arrays in C - the very idea - the wday and the mon 
   * have to be checked before we continue, otherwise
   * we get undefined behaviour.
   */
  if((tmptr->tm_wday < 0) || (tmptr->tm_wday >= 7)){
      ERROR("Day of week is not in range");
      return NULL;
  }
  if((tmptr->tm_mon < 1) || (tmptr->tm_mon > 12)){
      ERROR("Month value is not in range");
      return NULL;
  }

  /* format text for single digit numbers in time */
  if((sprintf(timestr, "%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n",
	      wdays[tmptr->tm_wday], month[tmptr->tm_mon - 1], tmptr->tm_mday, 
	      tmptr->tm_hour, tmptr->tm_min, tmptr->tm_sec,
	      tmptr->tm_year)) < 0){
      ERROR("sprintf() error");
      return NULL;
  }
  return timestr;
}

/* 
 * converts seconds since 1/1/70 to TM struct in UTC time
 */
static WTime_TM *WTime_GMTime_m (
	WTime_cl	*self,
	WTime_Time_T	time	/* IN */ )
{
  WTime_TM *tmp;
  TRC(printf("In gmtime().\n"));

  tmp = (WTime_TM *)malloc(sizeof(WTime_TM));

  extract_time(&time, 0L, tmp);
  return tmp;
}


/*
 * XXX At the moment this is exactly the same as gmtime.
 * This will have to be expanded as we grow to
 * incorporate proper timezone information.
 */
static WTime_TM *WTime_LocalTime_m (
	WTime_cl	*self,
	WTime_Time_T	time	/* IN */ )
{
  WTime_TM  *tmp;
  TRC(printf("In localtime().\n"));

  tmp = (WTime_TM *)malloc(sizeof(WTime_TM));
  extract_time(&time, 0L, tmp);
  return tmp;
}


/*
 * Takes seconds since 1/1/1970 and converts it to 
 * calendar time. The offset is used for timezone
 * deviations.
 * From BSD timesub function in ctime.c
 */
static void
extract_time(const WTime_Time_T *timep, const int32_t offset, WTime_TM *tmp)
{
    register int32_t days;
    register int32_t rem;
    register int16_t y;
    register int16_t yleap;
    register const int16_t *ip;

    days = *timep / SECSPERDAY;
    rem = *timep % SECSPERDAY;
    rem += (offset);
    while (rem < 0) {
	rem += SECSPERDAY;
	--days;
    }
    while (rem >= SECSPERDAY) {
	rem -= SECSPERDAY;
	++days;
    }
    tmp->tm_hour = (int) (rem / SECSPERHOUR);
    rem %= SECSPERHOUR;
    tmp->tm_min = (int) (rem / SECSPERMIN);
    tmp->tm_sec = (int) (rem % SECSPERMIN);
    tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYSPERWEEK);
    if (tmp->tm_wday < 0)
	tmp->tm_wday += DAYSPERWEEK;
    y = EPOCH_YEAR;
    if (days >= 0)
	for ( ; ; ) {
	    yleap = isleap(y);
	    if (days < (long) year_lengths[yleap])
		break;
	    ++y;
	    days -= (long) year_lengths[yleap];
	}
    else do {
	--y;
	yleap = isleap(y);
	days +=  (long) year_lengths[yleap];
    } while (days < 0);

    tmp->tm_year = y;
    tmp->tm_yday = (int) days;
    ip = mon_lengths[yleap];
    for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; tmp->tm_mon++){
	days = days - (long) ip[tmp->tm_mon];
    }
    //    tmp->tm_mon++;
    tmp->tm_mday = (int) (days + 1);
    tmp->tm_isdst = 0;
    tmp->tm_gmtoff = offset;
    /* XXX RN: just do UTC */
    tmp->tm_zone = tz_names[0];
}



/* 
 * => asctime(localtime(timer)) 
 */
static string_t WTime_CTime_m (
	WTime_cl	*self,
	WTime_Time_T	time	/* IN */ )
{
  TRC(printf("In ctime().\n"));
  return WTime_AscTime_m(self,WTime_LocalTime_m(self,time));
}


static int32_t WTime_DayOfWeek_m (
	WTime_cl	*self,
	uint16_t	mday	/* IN */,
	uint16_t	mon	/* IN */,
	uint16_t	year	/* IN */ )
{
  int16_t cent, rem;

  if (0 >= (((int16_t)mon) -= 2)) {	/* 1..12 -> 11,12,1..10 */
      mon += 12;	/* Puts Feb last since it has leap day */
      year -= 1;
  }
  return((mday + (rem = year % 100) + 5 * (cent = year / 100) +
	  cent / 4 + rem / 4 + (31 * mon / 12)) % 7);
}

/* Courtesy of Linux 
 * Converts Gregorian date to seconds since 01-01-1970 00:00:00.
 * Assumes input in normal date format, i.e. 31-12-1980 23:59:59
 * => day=31, mon=12, year=1980, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * XXX WARNING: this function will overflow on 07-02-2106 06:28:16 on
 * machines were long is 32-bit! (However, as time_t is signed, we
 * will already get problems at other places on 19-01-2038 03:14:08)
 */
static WTime_Time_T WTime_MakeTime_m (
	WTime_cl	*self,
	uint16_t	mday	/* IN */,
	uint16_t	mon	/* IN */,
	uint16_t	year	/* IN */,
	uint16_t	hour	/* IN */,
	uint16_t	min	/* IN */,
	uint16_t	sec	/* IN */ )
{

  TRC(printf("In WTime_MakeTime_m.\n"));
  if (0 >= (int) (mon -= 2)) {	/* 1..12 -> 11,12,1..10 */
      mon += 12;	/* Puts Feb last since it has leap day */
      year -= 1;
  }
  return (((
      (uint32_t)(year/4 - year/100 + year/400 + 367*mon/12 + mday) +
      year*365 - 719499
      )*24 + hour /* now have hours */
      )*60 + min /* now have minutes */
      )*60 + sec; /* finally seconds */

}

/*****************************************************************************
**
** Implementation of WTimeUpdate.if
**
******************************************************************************/


/*
 * Gets closure pointer for RTC
 */
static RTC_clp getRTCClosure()
{
  RTC_clp NOCLOBBER rtc = NULL;
  
  TRY{
    rtc = NAME_FIND("modules>RTC", RTC_clp);
  }
  CATCH_ALL
    {
      printf("### Error: Failed to attain RTC Closure in Wall_Clock.c ###\n");
    }
  ENDTRY;
  
  return rtc;
}


/*
 * Initialises timing data structures and values.
 */
static void WTimeUpdate_InitTime_m (
	WTimeUpdate_cl	*self )
{
#if defined(__IX86__) || defined(EB164)
  RTC_clp rtc;
  uint64_t ntptime;
#endif
  
  TRC(printf("### Initialising Wall-Clock Time ###\n"));
  
#if defined(__IX86__) || defined(EB164)
  rtc = getRTCClosure();
  if(rtc != NULL)
    ntptime = ((uint64_t)SECONDS((uint64_t)RTC$GetCMOSTime(rtc)));
  else
    ntptime = NOW() + ((uint64_t)SECONDS(883612800)); /* seconds since 1998 */ 
#else
  ntptime = NOW() + ((uint64_t)SECONDS(883612800)); /* seconds since 1998 */ 
#endif
    ENTER_KERNEL_CRITICAL_SECTION();
    INFO_PAGE.NTPscaling_factor = 0x0000000100000000LL; /* 1.0 */
    INFO_PAGE.prev_sched_time = NOW();
    INFO_PAGE.ntp_time = ntptime;
    LEAVE_KERNEL_CRITICAL_SECTION();
}


/*
 * Adjusts scaling factor. This is a priveleged operation
 * and is not available to the plebs.
 * The formula is based only on the previous values. It
 * goes like this:
 * t_ntp_1 = a * t_sched_1
 * t_ntp_2 = a * t_sched_2
 * Therefore:
 * a = (t_ntp_2 - t_ntp_1) / (t_sched_2 - t_sched_1)
 * XXX Assumes the time variable used to calculate the wall-clock
 * time have been initialised via a call to InitTime.
 * XXX Also assumes that ShallowUpdate has been used to
 * allow the NTP time variable to converge on a sensible value.
 * See ShallowUpdate for the reasoning behind this.
 */
static int16_t WTimeUpdate_AdjustOffset_m (
	WTimeUpdate_cl	*self,
	WTime_clp	tcl	/* IN */,
	WTimeUpdate_TimeVal_ptr	delta	/* IN */,
	int16_t	sign	/* IN */ )
{
    uint64_t new_sch;
    uint64_t stage1, stage2;
    uint64_t newtime;
    uint64_t newSF;
    WTime_TimeVal ntpnew;
    WTime_TimeVal *ndelta;

    TRC(printf("adjustOffset called.\n"));
    
    if((ndelta = (WTime_TimeVal *)malloc(sizeof(WTime_TimeVal))) == NULL){
	ERROR("No heap space available");
	return FAILURE;
    }

    ndelta->tv_sec = delta->tv_sec;
    ndelta->tv_usec = delta->tv_usec;
    new_sch = NOW();
    WTime_GetTimeOfDay_m(tcl, &ntpnew);

    if(sign == NEGATIVE){
	TRC(printf("ndelta->secs = -%lu; ndelta->usecs = -%lu \n",
		   ndelta->tv_sec, ndelta->tv_usec));
	if(ndelta->tv_usec > ntpnew.tv_usec){
	    ntpnew.tv_usec += 1000000;
	    ntpnew.tv_sec--;
	}
	ntpnew.tv_sec -= ndelta->tv_sec;
	ntpnew.tv_usec -= ndelta->tv_usec;
    }
    else{
	TRC(printf("ndelta->secs = %lu; ndelta->usecs = %lu \n",
		   ndelta->tv_sec, ndelta->tv_usec));
	ntpnew.tv_sec += ndelta->tv_sec;
	ntpnew.tv_usec += ndelta->tv_usec;
	if(ntpnew.tv_usec >= 1000000){
	    ntpnew.tv_usec -= 1000000;
	    ntpnew.tv_sec++;
	}
    }
    TRC(printf("NTP new secs = %lu; usecs = %lu \n",ntpnew.tv_sec,
	       ntpnew.tv_usec));

    stage1 = (((uint64_t)SECONDS(ntpnew.tv_sec)) +
	      ((uint64_t)MICROSECS(ntpnew.tv_usec))) - 
                         INFO_PAGE.ntp_time;  /* t_ntp_2 - t_ntp_1 */
    stage2 = new_sch - INFO_PAGE.prev_sched_time; /* t_sched_2 - t_sched_1 */

    TRC(printf("stage 1 is %qx + %qx - %qx \n",
	       ((uint64_t)SECONDS(ntpnew.tv_sec)), 
	       ((uint64_t)MICROSECS(ntpnew.tv_usec)),
	       INFO_PAGE.ntp_time));
    

    newSF = FPdiv(stage1, stage2);
    
    newtime = SECONDS(ntpnew.tv_sec) + MICROSECS(ntpnew.tv_usec);

    ENTER_KERNEL_CRITICAL_SECTION();
    INFO_PAGE.prev_sched_time = new_sch;
    INFO_PAGE.ntp_time  = newtime;
    INFO_PAGE.NTPscaling_factor = newSF;
    LEAVE_KERNEL_CRITICAL_SECTION();

    return SUCCESS;
}

/*
 * This is used to update the variables used to calculate the
 * wall-clock time without recalculating the scaling factor.
 * This is important as it allows the variables to converge
 * on sensible values before we start adjusting the scaling factor.
 * For instance, if the RTC time is greater than the time we get
 * from NTPdate, the stage 1 calculation in AdjustOffset (ntp2 - ntp1)
 * will generate a negative value. Also, if there is no RTC, then the 
 * stage 1 calculation will yield a very high result, causing the 
 * scaling factor to be wildly upset, resulting in a multiplication
 * overflow on the next calculation of the wall-clock time.
 */
static void WTimeUpdate_ShallowUpdate_m (
	WTimeUpdate_cl	*self,
	WTime_clp	tcl,	/* IN */ 
	WTimeUpdate_TimeVal_ptr	delta,	/* IN */
	int16_t	sign	/* IN */ )
{
    uint64_t new_sch;
    WTime_TimeVal ntpnew;
    uint64_t newtime;

    new_sch = NOW();
    WTime_GetTimeOfDay_m(tcl, &ntpnew);

    if(sign == NEGATIVE){
	if(delta->tv_usec > ntpnew.tv_usec){
	    ntpnew.tv_usec += 1000000;
	    ntpnew.tv_sec--;
	}
	ntpnew.tv_sec -= delta->tv_sec;
	ntpnew.tv_usec -= delta->tv_usec;
    }
    else{
	ntpnew.tv_sec += delta->tv_sec;
	ntpnew.tv_usec += delta->tv_usec;
	if(ntpnew.tv_usec >= 1000000){
	    ntpnew.tv_usec -= 1000000;
	    ntpnew.tv_sec++;
	}
    }
    newtime = SECONDS(ntpnew.tv_sec) + MICROSECS(ntpnew.tv_usec);
    
    ENTER_KERNEL_CRITICAL_SECTION();
    INFO_PAGE.prev_sched_time = new_sch;
    INFO_PAGE.ntp_time  = newtime;
    LEAVE_KERNEL_CRITICAL_SECTION();
}


/*
 * End of Wall_Clock.c
 */
