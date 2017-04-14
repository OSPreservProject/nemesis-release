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
**      mutex.h
** 
** DESCRIPTION:
** 
**      Handy macros for dealing with event counts & sequencers.
**      Includes the PAUSE() macro. 
** 
** ID : $Id: mutex.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#ifndef _mutex_h_
#define _mutex_h_

#include <SRCThread.ih>

/* 
 * Mutexes and Condition variables
 */

typedef SRCThread_Mutex		mutex_t;
typedef SRCThread_Condition	condition_t;

#define MU_INIT(mu)		SRCThread$InitMutex (Pvs(srcth), mu)
#define MU_FREE(mu)		SRCThread$FreeMutex (Pvs(srcth), mu)
#define MU_LOCK(mu)		SRCThread$Lock      (Pvs(srcth), mu)
#define MU_RELEASE(mu)		SRCThread$Release   (Pvs(srcth), mu)
#define CV_INIT(cond)		SRCThread$InitCond  (Pvs(srcth), cond)
#define CV_FREE(cond)		SRCThread$FreeCond  (Pvs(srcth), cond)

#define WAIT(mu,cond)		SRCThread$Wait      (Pvs(srcth), mu, cond)
#define SIGNAL(cond)		SRCThread$Signal    (Pvs(srcth), cond)
#define BROADCAST(cond)		SRCThread$Broadcast (Pvs(srcth), cond)



#endif /* _mutex_h_ */

