/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1998, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.					            *
 **                                                                          *
 *****************************************************************************
 **
 ** FACILITY:
 **
 **      lib/static/debug
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Gets output out without using IDC.
 ** 
 ** ENVIRONMENT: 
 **
 **      Bizarre limbo-land.
 ** 
 */

#include <nemesis.h>
#include <Wr.ih>

/* All architectures need to define uart_putch() which does the obvious
   (except that it isn't obvious any more: it should really go through the
   ntsc rather than bashing the hardware directly. Some of the hardware
   bashing is left in for now, though). */

/*  Intel */
#ifdef __ix86

/* ====================================================================== */

#include <ntsc.h>

static void uart_putch(unsigned char c)
{
    ntsc_putcons(c);
}

/* ====================================================================== */

#elif defined(__alpha)

/* ====================================================================== */

#  if defined(EB64) || defined(EB164)
#    include <nemesis.h>


#    define __IO_INLINE__
#    include <io.h>
#    include <platform.h>

#ifdef EB164
static void vga_putch(unsigned char c, int *xpos /* in-out */) { }
#endif /* EB164 */ 

static void uart_putch(unsigned char c)
{
    if(c=='\n') /* need to map LF to CRLF */
	uart_putch('\r');

    while(!(inb(COM1+LSR) & 0x20));
    outb(c, COM1+THR);
    while(!(inb(COM1+LSR) & 0x20));
}

#  elif defined(AXP3000)

#    include <nemesis.h>
#    include <ntsc.h>

#    include <platform.h>
#    include <z8530.h>

#    define SCC_WRITE_RDP(val) 			\
{ 						\
	volatile uint32_t *rdp = (volatile uint32_t *)(IOCTL_SCC1B + 8); \
    mb(); *rdp=(((uint32_t)(val))<<8); mb(); 	\
}

#    define SCC_STAT(val)			\
{ 						\
	volatile uint32_t *rap = (volatile uint32_t *)(IOCTL_SCC1B + 0); \
    (val)=(uchar_t)((*rap)>>8); mb(); 		\
}

/*
 * Write the character c in polled mode. Note that this can block with
 * interrupts disabled for approx 1ms.
 */
static void uart_putch(uchar_t c)
{
    uint32_t stat;
    
    if(c=='\n') { /* need to map LF to CR LF */
	/* Wait until the last character has been TX'ed */
	do {
	    SCC_STAT(stat);
	} while (! (stat & SCC_RREG_STAT_TBUF_EMPTY));
	
	SCC_WRITE_RDP('\r');
    }

    /* Wait until the last character has been TX'ed */
    do {
	SCC_STAT(stat);
    } while (! (stat & SCC_RREG_STAT_TBUF_EMPTY));
    
    SCC_WRITE_RDP(c);
    return;
}
#  else
#    error Strange alpha platform (!=eb64/eb164/axp3000) needs serial output.
#  endif

/* ====================================================================== */

#elif defined(arm)

/* ====================================================================== */

#  if defined(RISCPC)

#    include <riscpc.h>

static void uart_putch(unsigned char c)
{
    if (c=='\n') uart_putch('\r');
    while (! ((*FDC3_LINE_STAT) & 0x40));
    *FDC3_BASE = c;
}

#  elif defined(SRCIT)

static void uart_putch(unsigned char c)
{
    /* cant access the hardware here directly in case it competes with
     * the kernel */
    if (c=='\n') uart_putch('\r');
    while(ntsc_uart_putch(c));
}

#  elif defined(SHARK)

#include <io.h>
#include <shark.h>

static void uart_putch(unsigned char c)
{
    if(c=='\n') { /* need to map LF to CRLF */
	while(!(inb(COM1+COM_LSR) & 0x20));
	outb('\r', COM1+COM_THR);
	while(!(inb(COM1+COM_LSR) & 0x20));
    }

    while(!(inb(COM1+COM_LSR) & 0x20));
    outb(c, COM1+COM_THR);
    while(!(inb(COM1+COM_LSR) & 0x20));
}

#  else
#    error Strange arm platform (!=srcit/riscpc) needs serial output
#  endif

/* ============================================================ */

#else
#  error You need to write a uart_putch() for your architecture
#endif /* architecture stuff */

void dbg_putc(char c)
{
    uart_putch(c);
}

void dbg_puts(const char *s)
{
    while(*s)
    {
	uart_putch(*s);
	s++;
    }
}

static const char hex[16] = "0123456789abcdef";

void dbg_hex(word_t x, uint16_t w)
{
    int i;

    for(i=0; i < w; i++, (x)<<=4)
	uart_putch(hex[((x) >> (4 * w - 4)) & 0xf]);
}

/* Export a writer, available through C linkage as triv_wr_cl */

static bool_t           Nop(Wr_cl *self);
static Wr_PutC_fn       TrivPutC_m;
static Wr_PutStr_fn     TrivPutStr_m;
static Wr_PutChars_fn   TrivPutChars_m;

static Wr_op TrivWr_ms = {
    (Wr_PutC_fn *)      TrivPutC_m,
    (Wr_PutStr_fn *)    TrivPutStr_m,
    (Wr_PutChars_fn *)  TrivPutChars_m,
    (Wr_Seek_fn *)      Nop,
    (Wr_Flush_fn *)     Nop,
    (Wr_Close_fn *)     Nop,
    (Wr_Length_fn *)    Nop,
    (Wr_Index_fn *)     Nop,
    (Wr_Seekable_fn *)  Nop,
    (Wr_Buffered_fn *)  Nop,
    (Wr_Lock_fn *)      Nop,
    (Wr_Unlock_fn *)    Nop,
    (Wr_LPutC_fn *)     TrivPutC_m,
    (Wr_LPutStr_fn *)   TrivPutStr_m,
    (Wr_LPutChars_fn *) TrivPutChars_m,
    (Wr_LFlush_fn *)    Nop
};

const Wr_cl triv_wr_cl = { &TrivWr_ms, NULL };

static void TrivPutC_m(Wr_cl *self, int8_t c)
{
    uart_putch(c);
}

static void TrivPutStr_m(Wr_cl *self, string_t s)
{
    while(*s)
    {
        TrivPutC_m(self, *s);
        s++;
    }
}

static void TrivPutChars_m(Wr_cl *self, Wr_Buffer s, uint64_t nr)
{
    uint64_t i;

    for (i=0; i<nr; i++) {
        TrivPutC_m(self, s[i]);
    }
}

static bool_t Nop(Wr_cl *self)
{
    return False;
}

/* End */
