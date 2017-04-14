/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      ./h/sdom.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Defns and types for scheduling domains (should be mach indept)
** 
*/

#ifdef __LANGUAGE_C__

#include <Time.ih>
#include <autoconf/measure_kernel_accounting.h>

typedef enum { sdom_run, sdom_wait, sdom_block, sdom_unblocked } SDomState_t;
typedef enum { sdom_contracted, sdom_besteffort } SDomType_t;

struct _SDom_t {
    Time_ns     deadline;       /* Next deadline                */
    word_t      pq_index;       /* Index in priority queue      */
    Time_ns     timeout;        /* Block timeout                */
    Time_ns     prevddln;       /* Previous deadline            */
    
    Time_ns     remain;         /* Time remaining this period   */
    Time_ns     period;         /* Period of time allocation    */
    Time_ns     slice;          /* Length of allocation         */
    Time_ns     latency;        /* Unblocking latency           */

    SDomType_t  type;           /* Type of sdom                 */
    SDomState_t state;          /* Current running state        */
    dcb_ro_t   *dcb;            /* Contracted domain to run     */
    bool_t      xtratime;       /* Prepared to accept extra     */
    
    Time_ns     lastschd;       /* Time of last schedule        */
    Time_ns     lastdsch;       /* Time of last deschedule      */
    
    /* 
    ** The next four accounting fields are used for things such as 
    ** the 'QoSCtl' interface to the scheduler. Their purpose/meaning
    ** is as follows: 
    **   - ac_g    : total amount of guaranteed time this sdom has ever
    **               received (viz. since domain creation) upto the last 
    **  	     period but not including the current one. 
    **   - ac_x    : total amount of best effort time this sdom has ever 
    **               received; analog of ac_g really. 
    **   - ac_lp   : system time when last period ended. 
    **   - ac_time : time this sdom has received so far this period (IAP 
    **               says "(could be calculated from slice - remain, but 
    **               slice can be updated asynchronously)". 
    */
    Time_ns     ac_g;           /* Cumulative guaranteed time ever   */ 
    Time_ns     ac_x;           /* Cumulative best effort time ever  */ 
    Time_ns     ac_lp;          /* System time at end of last period */
    Time_ns     ac_time;        /* CPU time this period.             */

    /*
    ** Similar variables, but used here for measure kernel accounting
    */

#ifdef CONFIG_MEASURE_KERNEL_ACCOUNTING
     Time_ns     ac_m_lp;       /* time at end of last measure period */
     Time_ns     ac_m_lm;       /* last measure period total time used */
     Time_ns     ac_m_tm;       /* current measure period total time used */
#endif

};

#endif /* __LANGUAGE_C__ */

#ifdef __ALPHA__
#define SDOM_Q_TIMEOUT  0x0010  /* Used by PALcode (rfa_block, block) */
#endif
