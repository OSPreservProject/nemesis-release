/*
 *
 *	timer_rdtsc.c
 *	-------------
 *
 * Copyright (c) 1997 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * $Id: timer_rdtsc.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 *
 * Timer code for PCs. Uses rdtsc instruction.
 */

#include <nemesis.h>
#include <Timer.ih>
#include <asm.h>
#include <pip.h>
#include <kernel.h>
#include <irq.h>
#include <platform.h>
#include <timer.h>
#include <time.h>


/* Real-time clock */
#include <ds1287a.h>

/* Interval timer */
#include <i8254.h>

/* PIC */
#include <i8259.h>

#ifdef DEBUG_TIMER
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


/* On some Intel SMP machines, only overwriting the lower-order 5 bits of
   RTCADDR causes strange things to happen - the timer is not programmed
   correctly, and the CMOS will fail its checksum on the next reboot. */
#define SMP_TIMER_HACK

/* Number of calibration loops to perform */
#define CAL 5

#define TIMER0 0x40
#define TIMER_PERIOD_FLAG DS1287_A_2 /* 8192 */
#define TIMER_ALARM_OFF 0x7fffffffffffffffLL

#define RTCADD 0x70
#define RTCDAT 0x71

typedef struct pip Timer_st;

/*
 * Statics */
static Timer_Val_fn	val;
static Timer_Set_fn	set;
static Timer_Clear_fn	clear;
static Timer_Enable_fn	enable;

Timer_clp Timer_rdtsc_init();

/*
 * Module stuff
 */
static  Timer_op        Timer_ms = { val, set, clear, enable };
static  Timer_cl        t_cl     = { &Timer_ms, (void *)INFO_PAGE_ADDRESS };
static  Timer_clp	Timer    = &t_cl;

/*
 * Note on the i8254 timer:
 *
 * The clock is driven by a 14.318 MHz clock through a divide-by-12 counter.
 * This gives a clock frequency of 1.193 MHz.
 * Each tick is therefore 0.838 microseconds long.
 * Thus a 16 bit timer gives us a maximum delay of 0.05 seconds.
 *
 */

/* Interrupt service routines */

/*
 * Inline function to read a register
 */
INLINE uint8_t read_rtc( uint8_t addr )
{
    uint8_t b;
#ifdef SMP_TIMER_HACK
    b=addr;
#else
    b=inb(RTCADD);
    b=(b&0xe0)|addr;
#endif
    outb(b, RTCADD);
    return inb(RTCDAT);
}


/*
 * Inline function to write a register
 */
INLINE void write_rtc( uint8_t addr, uint8_t val )
{
    uint8_t b;
#ifdef SMP_TIMER_HACK
    b=addr;
#else
    b=inb(RTCADD);
    b=(b&0xe0)|addr;
#endif
    outb(b, RTCADD);
    outb(val, RTCDAT);
}

/* This is called when the interval timer expires, i.e. we want to
   reschedule */
static void do_itimer_irq(void *state, const kinfo_t *kinfo)
{
    k_st.reschedule=True;
}

static void do_ticker_irq(void *state, const kinfo_t *kinfo)
{
    Timer_st *st=state;
    uint32_t delta;

    delta=rpcc();

    /* Unmask the ticker interrupt */
    int_mask &= ~(1<<IRQ_V_TICKER);
    outb((int_mask & IRQ_M_PIC2) >> IRQ_V_PIC2, PIC2_OCW1);

    /* Ack this interrupt to the RTC */
    read_rtc(DS1287_C);
    read_rtc(DS1287_C); /* twice for mystic unknown reasons */

    st->now = st->now + (((delta - st->pcc) * (uint64_t)st->scale) / 1024);
    st->pcc=delta;
}

/* Calibration routine - returns processor cycles per 1/8 second */
static uint32_t calibrate_once(void)
{
    uint32_t start, end;
    uint32_t i;

    /* Switch off interrupts */
    write_rtc(DS1287_B, 0x02);

    /* Set the periodic interrupt interval to 1.953125ms */
    write_rtc(DS1287_A, 0x27);

    /* Wait for ten ticks to let it settle */
    for (i=0; i<10; i++) {
	while (!(read_rtc(DS1287_C)&0x40));
	while ((read_rtc(DS1287_C)&0x40));
    }

    /* At this point the flag is high */
    /* Now wait for 64 ticks, timing it */
    start=rpcc();
    for (i=0; i<64; i++) {
	while (!(read_rtc(DS1287_C)&0x40)); /* Wait for low */
	while ((read_rtc(DS1287_C)&0x40));  /* Wait for high */
    }
    end=rpcc();

    return end-start;
}

/* Returns number of picoseconds per processor cycle */
static uint32_t calibrate(void)
{
    uint32_t results[CAL];
    uint32_t tot;
    uint32_t i;
    uint64_t p;

    TRC(k_printf("Calibrating system timer... "));

    for (i=0; i<CAL; i++)
	results[i]=calibrate_once();

    /* Average the results */
    tot=0;
    for (i=0; i<CAL; i++)
	tot+=results[i];
    tot = (tot / CAL) * 8;

    TRC(k_printf("\n"));

    /* (10**12)/(cycles per second) = CPU clock period in picoseconds */
    p=1000000000000LL / (uint64_t)tot;

    return (uint32_t)p;
}

/*
 * Return the current value in ns of the timer.
 */
static Time_ns val( Timer_cl *self )
{
    /* Use function from pip.h */
    return time_inline(True);
}

/*
 * Set the timer to go off some time into the future.
 */
static void set( Timer_cl *self, Time_ns time )
{
    Timer_st *st = (Timer_st *)(self->st);
    Time_ns delay;
    word_t value;

    delay = time - val(self);

    st->alarm = time;

    if(delay < MICROSECS(1)) delay = MICROSECS(1);

    /* Limit value to something sensible */
    value = delay <= 0 ? 1 : ( delay * 313 ) >> 18;
    if ( value > 0xffff ) value = 0;  /* 0 is maximum value */

    TRC(k_printf("  timer_set: %i (%t)\n", value, delay));

    /*
     * Write the count into the registers on the i8254. We use mode 0
     * which pulses the output for one cycle when the timer reaches
     * zero. 
     */
    outb((i8254_CW_SEL0 | i8254_CW_BOTH | i8254_CW_MODE0), TIMER0 + i8254_CW);
    outb(value&0xFF, 		TIMER0 + i8254_C0);
    outb((value >> 8 ) & 0xFF, 	TIMER0 + i8254_C0);

/* XXX seems to be needed - forces out the writes? */
    inb(TIMER0 + i8254_C0);	

    /*
     * The timer is now off and running. Enable the interrupt.
     */
    int_mask &= ~(1<<IRQ_V_CNT0);
    outb((int_mask & IRQ_M_PIC1), PIC1_OCW1);
}

/*
 * Stop the timer from doing anything unexpected in the near future.
 * Return the value at about the time it was squashed.
 */
static Time_ns clear( Timer_cl *self, Time_ns *itime )
{
    Timer_st *st=self->st;
    word_t msb, lsb;
    word_t ticks;
    Time_ns now;
    
    /* Disable the timer and read remaining count value */
    outb((i8254_CW_RBC | i8254_RB_COUNT | i8254_RB_SEL0), TIMER0 + i8254_CW);
    lsb = inb(TIMER0 + i8254_C0);
    msb = inb(TIMER0 + i8254_C0);

    ticks = lsb | (msb << 8);
    *itime = ticks * 838;

    now = time_inline(True);

    *itime = (st->alarm>now)?st->alarm-now:0;

    return now;
}

/*
 * Enable interrupts and start the ball rolling
 */
static void enable( Timer_cl *self, uint32_t sirq )
{
    /* Install an interrupt handler for this clock (we're in the kernel) */
    k_st.irq_dispatch[IRQ_V_TICKER].stub=do_ticker_irq;
    k_st.irq_dispatch[IRQ_V_CNT0].stub=do_itimer_irq;
    k_st.irq_dispatch[IRQ_V_TICKER].state=self->st;
    k_st.irq_dispatch[IRQ_V_CNT0].state=self->st;

    /* Unmask the ticker */
    int_mask &= ~(1<<IRQ_V_TICKER);
    outb((int_mask & IRQ_M_PIC2)>>IRQ_V_PIC2, PIC2_OCW1);

    /* Enable ticker interrupts */
    write_rtc(DS1287_B, read_rtc(DS1287_B) | DS1287_B_PIE);
}

/*
 * RTC initialization 
 */
Timer_clp Timer_rdtsc_init(void)
{
    Timer_st *st = (Timer_st *)(Timer->st);
    word_t dummy;

    st->cycle_cnt=calibrate();

    st->now   = 0;
    st->alarm = TIMER_ALARM_OFF;
    st->pcc   = rpcc();
    st->scale = (st->cycle_cnt * 1024) / 1000;
    TRC(k_printf("SCALE: %w\n", st->scale));

    /* Set up periodic clock interrupt for 122.5us period */
    write_rtc(DS1287_A, DS1287_A_DIVON | TIMER_PERIOD_FLAG );

    /* This section _really_ upsets PC BIOSes */
#if 0
    /* Disable daylight savings & interrupts, select 24-hour mode/binary */
    write_rtc(DS1287_B, 
	      DS1287_B_PIE | DS1287_B_24OR12 | DS1287_B_DM );
#endif /* 0 */
  
    /* Now read register C to clear any outstanding interrupt condition */
    dummy = read_rtc( DS1287_C );

    return Timer;
}
