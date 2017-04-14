/*
 *
 *	timer.c
 *	-------
 *
 * Copyright (c) 1997 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * $Id: timer.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 *
 * Ticker driver for PCs
 */

#include <nemesis.h>
#include <mc146818rtc.h>
#include <Timer.ih>
#include <asm.h>
#include <pip.h>
#include <kernel.h>
#include <irq.h>
#include <platform.h>

#include <timer.h>

static Timer_Val_fn	Val_m;
static Timer_Set_fn	Set_m;
static Timer_Clear_fn	Clear_m;
static Timer_Enable_fn	Enable_m;

/*
 * Module stuff
 */
static  Timer_op        Timer_ms = { Val_m, Set_m, Clear_m, Enable_m };
static  Timer_cl        t_cl     = { &Timer_ms, (void *)0xdeadbeef };
static  Timer_clp	Timer    = &t_cl;

typedef struct Timer_st {
    Time_ns alarm;
} Timer_st;

Timer_st tst;

#if 0
static void do_ticker_irq(void *state, const kinfo_t *kinfo)
{
    uint32_t cc, lastcc;
    /* Unmask the ticker interrupt */
    int_mask &= ~(1<<IRQ_V_TICKER);
    if (IRQ_V_TICKER < IRQ_V_PIC2)
	outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);
    else
	outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);

    /* Ack this interrupt to the RTC */
    CMOS_READ(RTC_REG_C);
    CMOS_READ(RTC_REG_C); /* twice for mystic unknown reasons */

    INFO_PAGE.now+=122070;
    cc=rpcc();
    /* How many cycles since the last ticker? */
    lastcc=cc-tst.stamp;
    tst.stamp=cc;
    tst.count=lastcc;
    INFO_PAGE.count=lastcc;
}
#endif /* 0 */

static void do_timer_irq(void *state, const kinfo_t *kinfo)
{
    /* Unmask irq */
    int_mask &= ~(1<<IRQ_V_CNT0);
    if (IRQ_V_CNT0 < IRQ_V_PIC2)
	outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);
    else
	outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);

    INFO_PAGE.now+=TICKER_INTERVAL;
    if (INFO_PAGE.now>=tst.alarm)
	k_st.reschedule=True;
}

/*
 * RTC initialization 
 */
Timer_clp Timer_init(void)
{
    /* Indicate that the rdtsc timer is not in use */
    INFO_PAGE.scale=0;

    /* Register the ticker interrupt handler */

/*    k_st.irq_dispatch[IRQ_V_TICKER].stub=do_ticker_irq; */

    /* Register the countdown timer interrupt handler */
    k_st.irq_dispatch[IRQ_V_CNT0].stub=do_timer_irq;

#if 0
    /*
     * Enable RTC periodic interrupt as the rate of 8192Hz(122.07 us)
     */
    CMOS_WRITE(0x23, RTC_REG_A); /* Base 010 rate 0011 */
    CMOS_READ(RTC_REG_C);	/* reset IRQF */
#endif /* 0 */

#if 0
    /* Disable the interval timer for now */
    outb(0x3a, 0x43); /* binary, mode 5, ch 0 */
    outb(0x01, 0x40);
    outb(0x01, 0x40); /* A bogus count */
#else
    /* Set up the interval timer for periodic interrupt */
    outb(0x34, 0x43);
    outb(TICKER_COUNT & 0xff, 0x40);
    outb(TICKER_COUNT >> 8, 0x40);
#endif /* 0 */

    /* Now is zero */
    INFO_PAGE.now=0;
    tst.alarm=-1LL;

    Timer->st=&tst;

#if 0
    /* Enable the ticker */
    CMOS_WRITE(CMOS_READ(RTC_REG_B) | RTC_PIE, RTC_REG_B); /* Enable */
    CMOS_READ(RTC_REG_C);	/* reset IRQF */
#endif /* 0 */


    /* Unmask the timer interrupt */
    int_mask &= ~(1<<IRQ_V_CNT0);
    int_mask |= (1<<IRQ_V_TICKER);
    outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);
    outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);

    return Timer;
}

/*
 * Read timer value
 */
static Time_ns Val_m(Timer_cl *self)
{
    return INFO_PAGE.now;
}

/*
 * Set timer and enable interrupt
 * This version is for use with ticker interrupt only
 */
static void Set_m( Timer_cl *self, Time_ns time)
{
#if 0
    Time_ns	delay;
    uint16_t	counter;
#endif /* 0 */
    Timer_st *st = (Timer_st *)self->st;

#if 0
    /* Start the interval timer */
    delay = time - XNOW();
/*    k_printf("\nd=%x ",delay); */
    /*
     * One timer tick is 838ns. If you multiply the delay by 313 and
     * divide by 2^18, you get the number of 837.5ns intervals
     * necessary to make up the delay.
     */
    if (delay > 54000000) delay=54000000; /* MAX */
/*    k_printf("(actually %x)\n",delay); */
    counter = delay < 0 ? 1 : ( delay * 313 ) >> 18;
    outb_p(0x38, 0x43);           /* binary, mode 4, LSB/MSB, ch 0 */
    outb_p(counter & 0xff, 0x40); /* LSB */
    outb_p(counter >> 8, 0x40);   /* MSB */
/*    k_printf("Timer_Set: %t\n",time);*/
#endif /* 0 */
    st->alarm=time;
}

/*
 * Disable timer interrupt and read timer 
 */
static Time_ns Clear_m(Timer_cl *self, Time_ns *itime)
{
/*    uint16_t	counter; */
    Timer_st *st=(Timer_st *)self->st;

#if 0
    /* Deal with the interval timer */

    /* Issue counter latch instruction */
    outb_p(0x00, 0x43);

    counter = inb(0x42);	/* read current timer value */
    counter |= inb(0x42) << 8;

    /* Squash timer */
    /* Counter mode 5 is similar to mode 4, but the countdown will only
       start on a low->high transition of the gate pin. Fortunately, this
       pin is connected directly to logic 1 in PCs, so the count will never
       start */
    outb(0x3a, 0x43); /* binary, mode 5, ch 0 */
    outb(0x01, 0x40);
    outb(0x01, 0x40); /* A bogus count */
    
    *itime = counter * 838;	/* counter 1 = 838ns */
    if (*itime > 0 && *itime <= 2*838) {
	k_printf("small itime\n");
    }

#else

    *itime=st->alarm-INFO_PAGE.now;
    st->alarm=-1LL;

#endif /* 0 */
    return INFO_PAGE.now;
}

/*
 * Register Timer interrupt 
 */
static void Enable_m(Timer_cl *self, uint32_t sirq)
{
    /* Doesn't do anything under Intel */
}
