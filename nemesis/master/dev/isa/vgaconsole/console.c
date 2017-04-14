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
**      vgaconsole.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Text-mode output for VGA cards
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: console.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

/* Tested so far on Intel and SRCIT */

#include <nemesis.h>
#include <time.h>
#include <ecs.h>
#include <exceptions.h>
#include <mutex.h>
#include <VGATextMod.ih>

#include <ansicolour.h>
#include <io.h>

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef __IX86__
#define ACCESSHACK_START ENTER_KERNEL_CRITICAL_SECTION()
#define ACCESSHACK_END   LEAVE_KERNEL_CRITICAL_SECTION()
#else
#define ACCESSHACK_START
#define ACCESSHACK_END
#endif

static Wr_PutC_fn Wr_PutC_m;
static Wr_PutStr_fn Wr_PutStr_m;
static Wr_PutChars_fn Wr_PutChars_m;
static Wr_Seek_fn Wr_Seek_m;
static Wr_Flush_fn Wr_Flush_m;
static Wr_Close_fn Wr_Close_m;
static Wr_Length_fn Wr_Length_m;
static Wr_Index_fn Wr_Index_m;
static Wr_Seekable_fn Wr_Seekable_m;
static Wr_Buffered_fn Wr_Buffered_m;
static Wr_Lock_fn Wr_Lock_m;
static Wr_Unlock_fn Wr_Unlock_m;
static Wr_LPutC_fn Wr_LPutC_m;
static Wr_LPutStr_fn Wr_LPutStr_m;
static Wr_LPutChars_fn Wr_LPutChars_m;
static Wr_LFlush_fn Wr_LFlush_m;
static Wr_op wr_op = {
    Wr_PutC_m,
    Wr_PutStr_m,
    Wr_PutChars_m,
    Wr_Seek_m,
    Wr_Flush_m,
    Wr_Close_m,
    Wr_Length_m,
    Wr_Index_m,
    Wr_Seekable_m,
    Wr_Buffered_m,
    Wr_Lock_m,
    Wr_Unlock_m,
    Wr_LPutC_m,
    Wr_LPutStr_m,
    Wr_LPutChars_m,
    Wr_LFlush_m,
};

static VGATextMod_New_fn New_m;
static VGATextMod_op mod_op = { New_m };
static const VGATextMod_cl mod_cl = { &mod_op, NULL };
CL_EXPORT(VGATextMod, VGAConsole, mod_cl);

typedef struct {
    Wr_cl cl;
    uint32_t cx;       /* Current X position */
    uint32_t cy;       /* Current Y position */
    uint32_t woffset;  /* Window offset in screen memory */
    uint32_t base;     /* Base of window in ISA space */
    uint32_t height;   /* Number of lines in window */
    uint32_t width;    /* Number of columns in window */
    uint32_t stride;   /* Real width of screen */
    uint8_t attrib;    /* Current attribute byte */
    bool_t in_control; /* Are we processing a multi-byte control code? */
    uint32_t control_state; /* Current state in state machine */
    bool_t autolf;     /* Do we do automatic LF after CR? */
    Beep_clp beeper;
    mutex_t mu;        /* Every writer has a mutex */
} vga_st;

static Wr_clp New_m(VGATextMod_clp self,
		    uint32_t displaybase,
		    uint32_t displayheight,
		    uint32_t displaywidth,
		    uint32_t windowheight,
		    uint32_t windowwidth,
		    uint32_t windowx,
		    uint32_t windowy,
		    Beep_clp beeper)
{
    Wr_clp wr;
    vga_st *st;
#if 0
    string_t test=(ANSI_BLACK   "Black "
		   ANSI_RED     "Red "
		   ANSI_GREEN   "Green "
		   ANSI_YELLOW  "Yellow "
		   ANSI_BLUE    "Blue "
		   ANSI_MAGENTA "Magenta "
		   ANSI_CYAN    "Cyan "
		   ANSI_WHITE   "White\n");
#endif /* 0 */

    st=Heap$Malloc(Pvs(heap), sizeof(*st));

    CL_INIT(st->cl, &wr_op, st);
    st->cx=0;
    st->cy=0;
    st->woffset=windowy*displaywidth+windowx;
    st->base=displaybase+2*displaywidth*windowy+2*windowx;
    st->height=windowheight;
    st->width=windowwidth;
    st->stride=displaywidth;
    st->attrib=0x0f;
    st->in_control=False;
    st->autolf=False;
    st->beeper=beeper;
    MU_INIT(&st->mu);

    wr=&st->cl;

    Wr$PutC(wr, 12); /* Clear window */

#if 0
    Wr$PutStr(wr, ANSI_NORMAL);
    Wr$PutStr(wr, "Normal: ");
    Wr$PutStr(wr, test);
    Wr$PutStr(wr, ANSI_BOLD);
    Wr$PutStr(wr, "Bold: ");
    Wr$PutStr(wr, test);
    Wr$PutStr(wr, ANSI_USCORE);
    Wr$PutStr(wr, "Bold underscore: ");
    Wr$PutStr(wr, test);
    Wr$PutStr(wr, ANSI_NORMAL ANSI_USCORE);
    Wr$PutStr(wr, "Underscore: ");
    Wr$PutStr(wr, test);
#endif /* 0 */

    Wr$PutStr(wr, ANSI_NORMAL);

    return wr;
}

static void setcurs(vga_st *st)
{
    uint16_t offset;

    offset=st->woffset+st->cy*st->stride+st->cx;

    outb(14, 0x3d4);
    outb(offset>>8, 0x3d5);
    outb(15, 0x3d4);
    outb(offset&0xff, 0x3d5);
}

static void bell(vga_st *st)
{
#ifdef __IX86__
    VP_clp vp = Pvs(vp);
    bool_t mode = VP$ActivationMode(vp);
#endif

    if (st->beeper) {
	Beep$On(st->beeper, 880);
#ifdef __IX86__
	ntsc_leavekern();
	VP$ActivationsOn(vp);
#endif
	PAUSE(MILLISECS(200));
#ifdef __IX86__
	if (!mode) VP$ActivationsOff(vp);
	ntsc_entkern();
#endif
	Beep$Off(st->beeper);
    }
}

static void putchar(vga_st *st, uint32_t x, uint32_t y, uint8_t c)
{
    uint32_t base=st->base;
    writeb(c,base+y*st->stride*2+x*2);
    writeb(st->attrib,base+y*st->stride*2+x*2+1);
}

static void scroll_up(vga_st *st)
{
    uint32_t x,y;
    uint32_t base=st->base;
    for (y=0; y<st->height-1; y++)
	for (x=0; x<st->width; x++) {
	    writeb(readb(base+(y+1)*st->stride*2+x*2),
		   base+y*st->stride*2+x*2);
	    writeb(readb(base+(y+1)*st->stride*2+x*2+1),
		   base+y*st->stride*2+x*2+1);
	}
    for (x=0; x<st->width; x++)
	putchar(st, x, st->height-1, ' ');
}

static void backspace(vga_st *st)
{
    if (st->cx)
	st->cx--;
    else if (st->cy) {
	st->cy--;
	st->cx=st->width-1;
    }
}

static void delete(vga_st *st)
{
    backspace(st);
    putchar(st, st->cx, st->cy, ' ');
}

static void tab(vga_st *st)
{
    /* Move cx to the next multiple of eight */
}

static void inc_cy(vga_st *st)
{
    st->cy++;
    if (st->cy>=st->height) {
	scroll_up(st);
	st->cy=st->height-1;
    }
}

static void inc_cx(vga_st *st)
{
    st->cx++;
    if (st->cx>=st->width) {
	st->cx=0;
	inc_cy(st);
    }
}

static void newline(vga_st *st)
{
    inc_cy(st);
    st->cx=0;
}

static void cr(vga_st *st)
{
    st->cx=0;
    if (st->autolf) newline(st);
}

static void clear(vga_st *st)
{
    int x, y;

    st->cx=0;
    st->cy=0;

    for (y=0; y<st->height; y++)
	for (x=0; x<st->width; x++)
	    putchar(st, x, y, ' ');
}

static void curs_up(vga_st *st)
{
    if (st->cy>0)
	st->cy--;
}

static void curs_down(vga_st *st)
{
    if (st->cy<st->height)
	st->cy++;
}

static void curs_right(vga_st *st)
{
    if (st->cx<st->width)
	st->cx++;
}

static void curs_left(vga_st *st)
{
    if (st->cx>0)
	st->cx--;
}

static void func_normal(vga_st *st)
{
    st->attrib &= ~0x88;
}

static void func_bold(vga_st *st)
{
    st->attrib |= 0x08;
}

/* Underline represented by blink - ugh */
static void func_uline(vga_st *st)
{
    st->attrib |= 0x80;
}

#define COL(_x, _y) \
static void col_##_x(vga_st *st) { st->attrib&=0xf8; st->attrib|=(_y); }

COL(black, 0);
COL(red, 4);
COL(green, 2);
COL(yellow, 6);
COL(blue, 1);
COL(magenta, 5);
COL(cyan, 3);
COL(white, 7);

/* use FUNC when a state is an accepting state, but can accept further input */
#define STATE(_x) case (_x): switch (ch)
#define ENDSTATE break;
#define INPUT(_x,_y) case (_x): next=(_y); break;
#define FUNC(_x) default: (_x)(st); break;
#define TERMSTATE(_x, _y) case (_x): (_y)(st); break;

static bool_t process_control(vga_st *st, int8_t ch)
{
    uint32_t next=-1;

    switch (st->control_state) {
	STATE(0) {
	    INPUT('[', 1);
	} ENDSTATE;

	STATE(1) {
	    INPUT('0', 2);
	    INPUT('3', 8);
	    INPUT('A', 25);
	    INPUT('B', 26);
	    INPUT('C', 27);
	    INPUT('D', 28);
	} ENDSTATE;

	STATE(2) {
	    INPUT('m', 3);
	    INPUT('1', 4);
	    INPUT('4', 6);
	} ENDSTATE;

	TERMSTATE(3, func_normal);
	
	STATE(4) {
	    INPUT('m', 5);
	} ENDSTATE;

	TERMSTATE(5, func_bold);

	STATE(6) {
	    INPUT('m', 7);
	} ENDSTATE;

	TERMSTATE(7, func_uline);

	STATE(8) {
	    INPUT('0', 9);
	    INPUT('1', 10);
	    INPUT('2', 11);
	    INPUT('3', 12);
	    INPUT('4', 13);
	    INPUT('5', 14);
	    INPUT('6', 15);
	    INPUT('7', 16);
	} ENDSTATE;

	STATE(9) {
	    INPUT('m', 17);
	} ENDSTATE;

	STATE(10) {
	    INPUT('m', 18);
	} ENDSTATE;

	STATE(11) {
	    INPUT('m', 19);
	} ENDSTATE;

	STATE(12) {
	    INPUT('m', 20);
	} ENDSTATE;

	STATE(13) {
	    INPUT('m', 21);
	} ENDSTATE;

	STATE(14) {
	    INPUT('m', 22);
	} ENDSTATE;

	STATE(15) {
	    INPUT('m', 23);
	} ENDSTATE;

	STATE(16) {
	    INPUT('m', 24);
	} ENDSTATE;

	TERMSTATE(17, col_black);
	TERMSTATE(18, col_red);
	TERMSTATE(19, col_green);
	TERMSTATE(20, col_yellow);
	TERMSTATE(21, col_blue);
	TERMSTATE(22, col_magenta);
	TERMSTATE(23, col_cyan);
	TERMSTATE(24, col_white);

	TERMSTATE(25, curs_up);
	TERMSTATE(26, curs_down);
	TERMSTATE(27, curs_right);
	TERMSTATE(28, curs_left);
    }

    if (next==-1) {
	st->control_state=0;
	st->in_control=False;
	return False; /* Didn't process it */
    }
    st->control_state=next;
    return True; /* Processed character */
}

static void screen_out(vga_st *st, int8_t ch)
{
    if (st->in_control) {
	if (process_control(st, ch))
	    return;
    }

    switch (ch) {
	/* Ignore some control codes */
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
	break;
    case 7: bell(st); break;
    case 8: backspace(st); break;
    case 9: tab(st); break;
    case 10: newline(st); break;
    case 11: break;
    case 12: clear(st); break;
    case 13: cr(st); break;
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
	break;
    case 27: st->in_control=True; st->control_state=0; break;
    case 28:
    case 29:
    case 30:
    case 31:
	break;
    case 127: delete(st); break;
    default: putchar(st, st->cx, st->cy, ch); inc_cx(st); break;
    }

    return;
}

static void Wr_PutC_m(Wr_cl *self, int8_t ch)
{
    vga_st *st=self->st;

    MU_LOCK(&st->mu);
    ACCESSHACK_START;
    screen_out(st, ch);
    setcurs(st);
    ACCESSHACK_END;
    MU_RELEASE(&st->mu);
}

static void Wr_PutStr_m(Wr_cl *self, string_t string)
{
    Wr_PutChars_m(self, string, strlen(string));
}

static void Wr_PutChars_m(Wr_cl *self, int8_t *string, uint64_t nb)
{
    vga_st *st=self->st;
    int i;

    MU_LOCK(&st->mu);
    ACCESSHACK_START;
    for (i=0; i<nb; i++)
	screen_out(st, string[i]);
    setcurs(st);
    ACCESSHACK_END;
    MU_RELEASE(&st->mu);
}

static void Wr_Seek_m(Wr_cl *self, uint64_t n)
{
    RAISE_Wr$Failure(1);
}

static void Wr_Flush_m(Wr_cl *self)
{
}

static void Wr_Close_m(Wr_cl *self)
{
    vga_st *st=self->st;
    
    MU_FREE(&st->mu);
    FREE(st);
}

static uint64_t Wr_Length_m(Wr_cl *self)
{
    return -1; /* Infinite length */
}

static uint64_t Wr_Index_m(Wr_cl *self)
{
    return -1; /* No index */
}

static bool_t Wr_Seekable_m(Wr_cl *self)
{
    return False;
}

static bool_t Wr_Buffered_m(Wr_cl *self)
{
    return True;
}

static void Wr_Lock_m(Wr_cl *self)
{
    vga_st *st=self->st;

    MU_LOCK(&st->mu);
}

static void Wr_Unlock_m(Wr_cl *self)
{
    vga_st *st=self->st;

    MU_RELEASE(&st->mu);
}

static void Wr_LPutC_m(Wr_cl *self, int8_t ch)
{
    vga_st *st=self->st;

    ACCESSHACK_START;
    screen_out(st, ch);
    setcurs(st);
    ACCESSHACK_END;
}

static void Wr_LPutStr_m(Wr_cl *self, string_t s)
{
    Wr_LPutChars_m(self, s, strlen(s));
}

static void Wr_LPutChars_m(Wr_cl *self, int8_t *string, uint64_t nb)
{
    vga_st *st=self->st;
    int i;

    ACCESSHACK_START;
    for (i=0; i<nb; i++)
	screen_out(st, string[i]);
    setcurs(st);
    ACCESSHACK_END;
}

static void Wr_LFlush_m(Wr_cl *self)
{
}
