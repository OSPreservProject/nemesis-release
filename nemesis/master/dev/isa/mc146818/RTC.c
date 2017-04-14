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
**	RTC.c
**	
**
** FUNCTIONAL DESCRIPTION:
**	Reads and sets the Real-time clock
**	
**
** ENVIRONMENT: 
**	Kernel
**
**
** CREATED : January 1998
** AUTHOR :  Ian McDonald <ian@dcs.gla.ac.uk>
**
** ID : $Id: RTC.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/
/*
 *  get_cmos_time is based on the function of the same
 *  name in  linux/arch/i386/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 */


#include <nemesis.h>
#include <ntsc.h>
#include <stdio.h>
#include <stdlib.h>
#include <contexts.h>
#include <exceptions.h>
#include <mc146818rtc.h>
#include <RTC.ih>
#include <WTime.ih>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED
", __FUNCTION__);


static	RTC_GetCMOSTime_fn 		RTC_GetCMOSTime_m;
static	RTC_SetCMOSTime_fn 		RTC_SetCMOSTime_m;

static	RTC_op	ms = {
    RTC_GetCMOSTime_m,
    RTC_SetCMOSTime_m
};

static	const RTC_cl	cl = { &ms, NULL };

CL_EXPORT (RTC, RTC, cl);


/* Bit 1 of status register B on the MC146818
 * tells us whether the time is in 12 or 24 hour mode.
 * If we OR its contents with this, we should get the
 * mode. => 0xFD = 12 hr, 0xFF = 24 hr.
 */
#define TIME_MODE_MASK 0xFD /* => 11111101 */

static WTime_clp getTimeClosure();


/*
 * Gets closure pointer for WTime
 */
static WTime_clp getTimeClosure()
{
    WTime_clp tcl;

    TRY{
	tcl = (WTime_clp)malloc(sizeof(WTime_cl));
	tcl = NAME_FIND("modules>WTime", WTime_clp);
    }
    CATCH_ALL
	{
	    printf("### Error: Failed to attain WTime Closure in RTC.c ###\n");
	    return NULL;
	}
    ENDTRY;
    return tcl;
}

/*
 * Reads the time from the real-time clock.
 * Here (In i386 land) it is from the Motorola
 * (or compatible) MC146818.
 * It then returns the time in seconds since 1/1/70
 * This is a priveleged operation and is not available
 * to the plebs.
 */
static WTime_Time_T RTC_GetCMOSTime_m (
    RTC_cl	*self )
{
    uint16_t year, mon, mday, hour, min, sec;
    uint16_t i;
    WTime_clp tcl;

    TRC(printf("In get_cmos_time()\n"));

    ENTER_KERNEL_CRITICAL_SECTION();
    /* read RTC exactly on falling edge of update flag */
    for (i = 0 ; i < 1000000 ; i++)	/* may take up to 1 second... */
	if (CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP)
	    break;
    for (i = 0 ; i < 1000000 ; i++)	/* must try at least 2.228 ms */
	if (!(CMOS_READ(RTC_FREQ_SELECT) & RTC_UIP))
	    break;
    do { /* This'll catch updates in the middle - as if! */
	sec = CMOS_READ(RTC_SECONDS);
	min = CMOS_READ(RTC_MINUTES);
	hour = CMOS_READ(RTC_HOURS);
	mday = CMOS_READ(RTC_DAY_OF_MONTH);
	mon = CMOS_READ(RTC_MONTH);
	year = CMOS_READ(RTC_YEAR);
/*		regB = CMOS_READ(RTC_REG_B); */
    } while (sec != CMOS_READ(RTC_SECONDS));
    if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY) || RTC_ALWAYS_BCD)
    {
	BCD_TO_BIN(sec);
	BCD_TO_BIN(min);
	BCD_TO_BIN(hour);
	BCD_TO_BIN(mday);
	BCD_TO_BIN(mon);
	BCD_TO_BIN(year);
    }
    LEAVE_KERNEL_CRITICAL_SECTION();

    TRC(printf("Left critical section.\n"));
    if ((year += 1900) < 1970)
	year += 100;
/*    	TRC(printf("Register B of RTC CMOS chip equals %d\n",regB));

	if((regB | TIME_MODE_MASK) == 0xFF)
	current_mode = HOUR_24;
	else
	current_mode = HOUR_12;
	*/
    if((tcl = getTimeClosure()) == NULL)
	return 0;

    return(WTime$MakeTime(tcl,mday, mon, year, hour, min, sec));
}


/*
 * Sets the real-time clock with the values
 * from the tm struct.
 * This is a priveleged operation and not available
 * to the plebs.
 */
static void RTC_SetCMOSTime_m (
    RTC_cl	*self,
    const WTime_TM	*tmptr	/* IN */ )
{

    UNIMPLEMENTED;
}

/*
 * End 
 */
