/*
 *	atropos.c
 *	---------
 *
 * Copyright (c) 1994 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * ID : $Id: atropos.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 * This is the "atropos" CPU scheduler. 
 */

#include <nemesis.h>
#include <kernel.h>
#include <ntsc.h>
#include <time.h>
#include <sdom.h>
#include <dcb.h>
#include <pip.h>
#include <autoconf/measure_kernel_accounting.h>
#include <autoconf/measure_trace.h>

/*
** Constants
*/
#define BESTEFFORT_QUANTUM MICROSECS(1000)

/*
** Tracing and debugging flags:
**
** Passed in from Makefile or command line:
**
** MINIMAL_TRACE  For debugging purposes.
**
** Config system options:
**
** CONFIG_MEASURE_TRACE  Keep a log of activity using the Trace module
**                       (can log reschedules for several hours)
** CONFIG_MEASURE_KERNEL_ACCOUNTING
**                       See below for description.
*/

#ifdef MINIMAL_TRACE
#define MTRC(_x) _x
#else
#define MTRC(_x)
#endif

#ifdef CONFIG_MEASURE_TRACE
#define MEASURE_TRACE(_x) _x
#else 
#define MEASURE_TRACE(_x)
#endif

#ifdef CONFIG_MEASURE_KERNEL_ACCOUNTING
#define MEASURE(_x) _x
#else
#define MEASURE(_x) 
#endif

/*
** Measure-style accounting.  
** 
** Measure periodically samples CPU usage and uses large-deviation
** theory to automatically determine the QoS guarantee necessary
** for a required probability of aplication level 'loss' e.g. queues 
** overflowing or audio breaking up, etc.  For more details see the
** Measure project web pages:
**
**   http://www.cl.cam.ac.uk/Research/SRG/measure/
**
** (don't want this code inline in the scheduler since it confuses
** an already complicated lump of code)
*/

#ifdef CONFIG_MEASURE_KERNEL_ACCOUNTING
/* 
 * Sampling preemption. For accurate accounting we measure as close as possble
 * to a defined sampling period
 */
static Time_ns ac_next = 0; /* 0 is start case... */
static Time_ns ac_period = MILLISECS(100); /* Set this to something sensible */

/* Measure style kernel accounting */
static void ac_measure(kernel_st *st, Time_ns time)
{
    SDom_t *sdom;

    /* start case */
    if (!ac_next) ac_next = time;

    if (time >= ac_next) /* we've reached the end of a measure period */
    {
	unsigned int n,nmax;
	/* Whizz around *all* sdoms copying ac_m_tm to ac_m_lm */
	nmax = PQ_SIZE(st->wait);
	for (n=1; n <= nmax; n++)
	{
	    sdom = PQ_NTH(st->wait,n);
	    /* DOIT */
	    sdom->ac_m_lm = sdom->ac_m_tm;
	    sdom->ac_m_tm = 0;
	    sdom->ac_m_lp = ac_next;
	}
	nmax = PQ_SIZE(st->run);
	for (n=1; n <= nmax; n++)
	{
	    sdom = PQ_NTH(st->run,n);
	    /* DOIT */
	    sdom->ac_m_lm = sdom->ac_m_tm;
	    sdom->ac_m_tm = 0;
	    sdom->ac_m_lp = ac_next;
	}
	nmax = PQ_SIZE(st->blocked);
	for (n=1; n <= nmax; n++)
	{
	    sdom = PQ_NTH(st->blocked,n);
	    /* DOIT */
	    sdom->ac_m_lm = sdom->ac_m_tm;
	    sdom->ac_m_tm = 0;
	    sdom->ac_m_lp = ac_next;
	}

	/* Increment ac_next */
	ac_next += ac_period;
    }
}
#endif


/* 
** SDOM Priority queues.  Ordered by deadline field.
**
** We don't forcibly INLINE these because they're big and we would
** rather not blast the I-cache. 
*/
static void sdomq_remove(SDomHeap_t *pq)
{
    PQ_REMOVE((*pq));
}

static void sdomq_insert(SDomHeap_t *pq, SDom_t *s)
{
    PQ_INSERT((*pq), s)
}

static void sdomq_del(SDomHeap_t *pq, word_t ix)
{
    PQ_DEL((*pq), ix)
}
#undef PQ_REMOVE
#define PQ_REMOVE(_pq) sdomq_remove(&(_pq))
#undef PQ_INSERT
#define PQ_INSERT(_pq,_s) sdomq_insert(&(_pq),_s)
#undef PQ_DEL
#define PQ_DEL(_pq,_ix) sdomq_del(&(_pq), _ix)


/*
 * dequeue
 * 
 * Remove a domain from whichever scheduler queue it is on.
 * Note that, if the domain is on the RUN queue then is must
 * by definition be at the head.  For BLOCK and WAIT queues,
 * atropos calls PQ_REMOVE directly when it knows the domain
 * is at the head of the queue. 
 */
static void dequeue(kernel_st *st, SDom_t *sdom)
{
    switch (sdom->state) {
    case sdom_run:
	PQ_REMOVE(st->run);	
	break;
    case sdom_block:
	PQ_DEL(st->blocked, PQ_IX(sdom));
	break;
    case sdom_wait:
	PQ_DEL(st->wait, st->next_optm);
	break;
    case sdom_unblocked:
	PQ_DEL(st->wait, PQ_IX(sdom));
	break;
    }
}

/*
 * requeue
 *
 * Moves the specified domain to the appropriate queue. 
 * The wait queue is ordered by the time at which the domain
 * will receive more CPU time.  If a domain has no guaranteed time
 * left then the domain will be placed on the WAIT queue until
 * its next period. 
 *
 * Note that domains can be on the wait queue with remain > 0 
 * as a result of being blocked for a short time.
 * These are scheduled in preference to domains with remain < 0 
 * in an attempt to 
 */
static void requeue(kernel_st *st, SDom_t *sdom)
{
    switch (sdom->state) {
    case sdom_run:
	sdom->dcb->schst = DCBST_K_RUN;
	PQ_INSERT(st->run, sdom);
	break;
    case sdom_unblocked:
    case sdom_wait:
	sdom->dcb->schst = DCBST_K_WAIT;
	PQ_INSERT(st->wait, sdom);
	break;
    case sdom_block:
	k_panic("atropos: scheduler bug\n");
    }
}


/*
 * endofperiod
 *
 * Update the various accounting parameters (described in sdom.h) 
 */
static INLINE void endofperiod(SDom_t *sdom, Time_ns time)
{
    /* The domain's period is over, so add its accumulated time to ac_g, 
       and store the current time in ac_lp. Zero ac_time ready to account 
       for the next period. */
    sdom->ac_g   += sdom->ac_time;  
    sdom->ac_time = 0;  
    sdom->ac_lp   = time;          
}

/*
 * block 
 * 
 * This function updates the sdom for a domain which is being blocked.
 * It moves the sdom to the block queue.  
 *
 * ASSERT: On entry, the sdom has already been removed from 
 * whichver queue it was on. On exit, the domain is on the block queue.
 *
 */
static void block(kernel_st *st, SDom_t *sdom)
{
    Time_ns tmp;

    /* Queues are ordered by deadline field, so swap the 
       deadline and tiemout fields */
    tmp = sdom->deadline;
    sdom->deadline = sdom->timeout;
    sdom->timeout  = tmp;
    
    /* Move to blocked queue */
    sdom->state = sdom_block;
    sdom->dcb->schst = DCBST_K_BLCKD;
    PQ_INSERT(st->blocked,sdom);
}

/*
 * unblock
 *
 * This function deals with updating the sdom for a domain
 * which has just been unblocked.  
 *
 * ASSERT: On entry, the sdom has already been removed from the block
 * queue (it can be done more efficiently if we know that it
 * is on the head of the queue) but its deadline field has not been
 * restored yet.
 */
static void unblock(kernel_st *st, SDom_t *sdom, Time_ns time)
{
    /* Restore deadline field (saved in timeout) */
    sdom->deadline = sdom->timeout;

    /* We distinguish two cases... short and long blocks */
    if ( sdom->deadline < time ) {
	/* The sdom has passed its deadline since it was blocked. 
	   Give it its new deadline based on the latency value. */
	sdom->prevddln = time; 
	sdom->deadline = time + sdom->latency;
	sdom->remain   = sdom->slice;
	sdom->state    = (sdom->remain > 0) ? sdom_run : sdom_wait;
	endofperiod(sdom, time);
    } else {
	/* We leave REMAIN intact, but put this domain on the WAIT
	   queue marked as recently unblocked.  It will be given
	   priority over other domains on the wait queue until while
	   REMAIN>0 in a generous attempt to help it make up for its
	   own foolishness. */
	sdom->state = (sdom->remain > 0) ? sdom_unblocked : sdom_wait;
    }
}

/************************************************************************
 *
 * ATROPOS
 * 
 * The scheduler deals with two different entities: the current
 * _domain_ and the current _scheduling domain_ or sdom. The sdom is what
 * CPU time usage is accounted to. When a particular sdom is selected
 * to run, it decides the domain to be executed.
 *
 * This is mainly hidden from users of the system - a better
 * abstraction from their point of view is the _service class_ of the
 * domain. A domain with a time contract with the system has its own
 * sdom, whereas the various classes of best effort domains have one
 * sdom each. 
 * 
 ************************************************************************/

void ksched_scheduler(addr_t             stp,
		      dcb_ro_t 	        *cur_dcb,
		      context_t 	*cur_ctxt )
{
    kernel_st   *st = (kernel_st *)stp;
    SDom_t	*cur_sdom;              /* Current sdom		*/
    Time_ns 	 time;			/* Current time? 		*/
    Time_ns	 itime;			/* Time on interval timer	*/
    Time_ns	 newtime;		/* What to set the itimer to	*/
    Time_ns      ranfor;	        /* How long the domain ran      */
    dcb_ro_t	*dcb;			/* tmp. domain control block	*/
    dcb_rw_t	*dcbrw;
    SDom_t	*sdom;			/* tmp. scheduling domain	*/
    bool_t       blocking;	        /* does the domain wish to block? */
    Activation_Reason   reason;
    

    /* Increment the heartbeat counter in the PIP.  This allows 
       benchmark code, etc. to see how many passes through the scheduler 
       happened as a result of certain tests - useful for spotting bugs! */
    INFO_PAGE.sheartbeat++;

    /* Squash timer and read the time */
    time = Timer$Clear(st->t, &itime);

    /* Give the Measure boys a change to do their stuff */
    MEASURE(ac_measure(st, time)); 

    /* If we were spinning in the idle loop, there is no current
     * domain to deschedule. */
    if (cur_dcb == NULL) {
	goto deschedule_done;
    }

    /*****************************
     * 
     * Deschedule the current scheduling domain
     *
     ****************************/
    
    dcbrw = DCB_RO2RW(cur_dcb);

#ifndef __IX86__
    /* We enable activations here for an RFABlock to avoid trashing
       the actctx on the way into the scheduler.  Turn on activations
       and then treat just like a block */
    if (cur_dcb->schst == DCBST_K_RFABLOCK) {
	dcbrw->mode = 0;
	cur_dcb->schst = DCBST_K_BLOCK;
    }
#endif    

#if defined(INTEL) || defined(__ARM__)
    /* Send any delayed events */
    while(cur_dcb->outevs_processed < dcbrw->outevs_pushed) {
	word_t next_event = cur_dcb->outevs_processed + 1;
	dcb_event *ev = &dcbrw->outevs[next_event % NUM_DCB_EVENTS];
#ifdef INTEL
	k_dsc_send(ev->ep, ev->val);
#else
	k_sc_send(ev->ep, ev->val);
#endif
	cur_dcb->outevs_processed++;
    }
#endif

    /* Record the time the domain was preempted and for how long it
       ran.  Work out if the domain is going to be blocked to save
       some pointless queue shuffling */
    cur_sdom = st->cur_sdom;
    cur_sdom->lastdsch = time;
    ranfor = (time - cur_sdom->lastschd);
    blocking = (cur_dcb->schst == DCBST_K_BLOCK) && EP_FIFO_EMPTY (dcbrw);

    if ((cur_sdom->state == sdom_run) ||
	(cur_sdom->state == sdom_unblocked)) {

	/* In this block, we are doing accounting for an sdom which has 
	   been running in contracted time.  Note that this could now happen
	   even if the domain is on the wait queue (i.e. if it blocked) */

	/* Deduct guaranteed time from the domain */
	cur_sdom->remain  -= ranfor;
	cur_sdom->ac_time += ranfor;
	MEASURE(cur_sdom->ac_m_tm += ranfor); 

	/* If guaranteed time has run out... */
	if (!blocking && (cur_sdom->remain <= 0)) {
	    /* Move domain to correct position in WAIT queue */
            /* XXX sdom_unblocked doesn't need this since it is 
	     already in the correct place. */
	    dequeue(st, cur_sdom);
	    cur_sdom->state = sdom_wait;
	    requeue(st, cur_sdom);
	} 

    } else {
      
	/*
	 * In this block, the sdom was running optimistically - so no
	 * accounting is necessary. The sdom is not the head of the 
	 * run queue, but st->next_optm gives its position 
	 * in the wait queue 
	 */
	cur_sdom->ac_x += ranfor;
	MEASURE(cur_sdom->ac_m_tm += ranfor);
    }

    /******************
     * 
     * If the domain wishes to block, then block it.
     *
     * We only block a domain if it has no events in the fifo - this 
     * is to avoid the "wake-up-waiting" race 
     *
     *******************/
    if (blocking) { 
	/* Remove the domain from whichever queue and block it */
	dequeue(st, cur_sdom);
	block(st, cur_sdom);
    }
    
  deschedule_done:
    
    /*****************************
     * 
     * We have now successfully descheduled the current sdom.
     * The next task is the allocate CPU time to any sdom it is due to.
     *
     ****************************/
    cur_dcb =  (dcb_ro_t *)0;
    cur_sdom = (SDom_t *)0;
    
    /*****************************
     * 
     * Allocate CPU to any waiting domains who have passed their
     * period deadline.  If necessary, move them to run queue.
     *
     ****************************/
    while(PQ_SIZE(st->wait) && 
	  (sdom = PQ_HEAD(st->wait))->deadline <= time ) {
	
	/* Remove from HEAD of wait queue */
	PQ_REMOVE(st->wait);

	/* Domain begins a new period and receives a slice of CPU 
	 * If this domain has been blocking then throw away the
	 * rest of it's remain - it can't be trusted */
	if (sdom->remain > 0) 
	    sdom->remain = sdom->slice;
    	else 
	    sdom->remain += sdom->slice;
	sdom->prevddln = sdom->deadline;
	sdom->deadline += sdom->period;
	sdom->state = (sdom->remain > 0) ? sdom_run : sdom_wait;

	/* Place on the appropriate queue */
	requeue(st, sdom);

	/* Update the accounts */
	endofperiod(sdom, time);
    }
    
    /*****************************
     * 
     * Next we must handle all domains to which new events have been
     * delivered.  These domains will have been placed in a fifo by
     * the event send code.
     *
     ****************************/
    
    while (st->domq.tail != st->domq.head)  {
	/* Let the event delivery mechanism know we've seen this one. */
	st->domq.tail = (st->domq.tail + 1) % CFG_UNBLOCK_QUEUE_SIZE;
	dcb   = st->domq.q[st->domq.tail];
	sdom  = dcb->sdomptr;

	/*
	 * Do not reschedule stopped or dying domains.
	 * They are not on ANY queue, but other domains may still
	 * send them events.
	 */
	if (dcb->schst == DCBST_K_STOPPED ||
	    dcb->schst == DCBST_K_DYING) continue;

	/* Unblock the domain if necessary */
	if(sdom->state == sdom_block) {

	    /* Remove from the MIDDLE of the blocked queue */
	    dequeue(st, sdom);

	    /* Unblock and move to the appropriate queue */
	    unblock(st, sdom, time);
	    requeue(st, sdom);
	}
    }
    
    /*****************************
     * 
     * Next we must handle all domains whose block timeouts have expired.
     * Currently we treat a timeout as though an event had arrived.  
     *
     ****************************/
    
    /* Wake up any blocked domains whose timeout has expired */
    while(PQ_SIZE(st->blocked) && 
	  (sdom = PQ_HEAD(st->blocked))->deadline <= time ) {
	
	/* Remove from HEAD of blocked queue */
	PQ_REMOVE(st->blocked);
	
	/* Unblock and move to the appropriate queue */
	unblock(st, sdom, time);
	requeue(st, sdom);
    }
    
    /*****************************
     * 
     * Next we need to pick an sdom to run.
     * If anything is actually 'runnable', we run that. 
     * If nothing is, we pick a waiting sdom to run optimistically.
     * If there aren't even any of those, we have to spin waiting for an
     * event or a suitable time condition to happen.
     *
     ****************************/
    
    newtime = FOREVER;
    reason = 0;

    /* If we have an sdom on the RUN queue, then run it. */
    if (PQ_SIZE(st->run)) {
	
	cur_sdom = PQ_HEAD(st->run);
	cur_dcb = cur_sdom->dcb;
	newtime = time + cur_sdom->remain;
	reason  = (cur_sdom->prevddln > cur_sdom->lastschd) ?
	    Activation_Reason_Allocated : Activation_Reason_Preempted;
	
    } else if (PQ_SIZE(st->wait)) {

	int i;

	/* Try running a domain on the WAIT queue - this part of the
	   scheduler isn't particularly efficient but then again, we
	   don't have any guaranteed domains to worry about. */
	
	/* See if there are any unblocked domains on the WAIT
	   queue who we can give preferential treatment to. */
	for (i = 1; i <= PQ_SIZE(st->wait); i++) {
	    sdom = PQ_NTH(st->wait, i);
	    if (sdom->state == sdom_unblocked) {
		cur_sdom = sdom;
		cur_dcb  = sdom->dcb;
		newtime  = time + sdom->remain;
		reason   = Activation_Reason_Preempted;
		goto found;
	    }
	}

	/* Last chance: pick a domain on the wait queue with the XTRA
	   flag set.  The NEXT_OPTM field is used to cheaply achieve
	   an approximation of round-robin order */
	for (i = 0; i < PQ_SIZE(st->wait); i++) {
	    /* Pick (roughly) the next domain on the wait q. */
	    if (++(st->next_optm) > PQ_SIZE(st->wait)) 
		st->next_optm = 1;
	    
	    sdom = PQ_NTH(st->wait, st->next_optm);
	    if (sdom->xtratime) {	
		cur_sdom = sdom;
		cur_dcb  = sdom->dcb;
		newtime = time + BESTEFFORT_QUANTUM;
		reason  = Activation_Reason_Extra;
		goto found;
	    }
	}

    }
    
    found:

    /**********************
     * 
     * We now have to work out the time when we next need to
     * make a scheduling decision.  We set the alarm timer
     * to cause an interrupt at that time.
     *
     **********************/
     
    /* If we might be able to run a waiting domain before this one has */
    /* exhausted its time, cut short the time allocation */
    if (PQ_SIZE(st->wait))
	newtime = MIN(newtime, PQ_HEAD(st->wait)->deadline);
    
    /* If necessary reenter the scheduler to wake up blocked domains */ 
    if (PQ_SIZE(st->blocked))
	newtime = MIN(newtime, PQ_HEAD(st->blocked)->deadline);

    MEASURE(newtime = MIN(newtime, ac_next));

    /* Set timer for newtime */
    Timer$Set(st->t, newtime);  
    
    MEASURE_TRACE(Trace$Add(st->trace, cur_dcb ? cur_dcb->id : 0, time));

    if (cur_dcb) {

	MTRC(k_putchar('0' + (cur_dcb->id & 7)));

	/* New state */
	st->cur_dcb  = cur_dcb;
	st->cur_sdom = cur_sdom;
	st->cur_sdom->lastschd = time;
	cur_dcb->schst = DCBST_K_RUN;
	
	/* Switch to the new protection domain */
	k_swppdom (cur_dcb);

	/* Activate the chosen domain */
	k_activate (cur_dcb, reason);
	
    } else {

	MTRC(k_putchar('.'));
	MTRC(k_putchar('0' + PQ_SIZE(st->run)));
	MTRC(k_putchar('0' + PQ_SIZE(st->wait)));
	MTRC(k_putchar('0' + PQ_SIZE(st->blocked)));

	st->cur_dcb = NULL; /* tested by ksched.S */

	/* Enable interrupts and spin */
	k_idle();
    }
    
}

/*
 * End atropos.c
 */
