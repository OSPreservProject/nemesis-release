/* $XFree86: xc/programs/Xserver/hw/xfree86/accel/s3/Ti3026Curs.c,v 3.1 1995/05/27 03:09:56 dawes Exp $ */
/*
 * Copyright 1994 by Robin Cutshaw <robin@XFree86.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Robin Cutshaw not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Robin Cutshaw makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ROBIN CUTSHAW DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ROBIN CUTSHAW BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * Modified for TVP3026 by Harald Koenig <koenig@tat.physik.uni-tuebingen.de>
 *
 */



#define NEED_EVENTS
#include <X.h>
#include <misc.h>
#include <input.h>
#include <scrnintstr.h>
#include <servermd.h>
#include "Ti302X.h"

#include <nemesis.h>
#include <IDCMacros.h>

#include "xrip.h"

#define MAX_CURS_HEIGHT 64   /* 64 scan lines */
#define MAX_CURS_WIDTH  64   /* 64 pixels     */

typedef void *CursorPtr;


#define BLOCK_CURSOR 
#define UNBLOCK_CURSOR

#ifndef __GNUC__
# define __inline__ /**/
#endif

#if 0
/*
 * s3OutTi3026IndReg() and s3InTi3026IndReg() are used to access the indirect
 * 3026 registers only.
 */

#ifdef __STDC__
void s3OutTi3026IndReg(unsigned char reg, unsigned char mask, unsigned char data)
#else
void s3OutTi3026IndReg(reg, mask, data)
unsigned char reg;
unsigned char mask;
unsigned char data;
#endif
{
   unsigned char tmp, tmp1, tmp2 = 0x00;

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x00);
   tmp1 = inb(0x3c8);
   outb(0x3c8, reg);
   outb(vgaCRReg, tmp | 0x02);

   if (mask != 0x00)
      tmp2 = inb(0x3c6) & mask;
   outb(0x3c6, tmp2 | data);
   
   outb(vgaCRReg, tmp | 0x00);
   outb(0x3c8, tmp1);  /* just in case anyone relies on this */
   outb(vgaCRReg, tmp);
}

#ifdef __STDC__
unsigned char s3InTi3026IndReg(unsigned char reg)
#else
unsigned char s3InTi3026IndReg(reg)
unsigned char reg;
#endif
{
   unsigned char tmp, tmp1, ret;

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x00);
   tmp1 = inb(0x3c8);
   outb(0x3c8, reg);
   outb(vgaCRReg, tmp | 0x02);

   ret = inb(0x3c6);

   outb(vgaCRReg, tmp | 0x00);
   outb(0x3c8, tmp1);  /* just in case anyone relies on this */
   outb(vgaCRReg, tmp);

   return(ret);
}

#endif /* 0 */

void 
s3Ti3026CursorOn()
{
   unsigned char tmp;

   UNLOCK_SYS_REGS;

   /* turn on external cursor */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xDF;
   outb(vgaCRReg, tmp | 0x20);

   /* Enable Ti3026 */
   outb(vgaCRIndex, 0x45);
   tmp = inb(vgaCRReg);
   outb(vgaCRReg, tmp & ~0x20);
   
   /* Enable cursor - X11 mode */
   s3OutTi3026IndReg(TI_CURS_CONTROL, 0x6c, 0x13);

   LOCK_SYS_REGS;
   return;
}

void
s3Ti3026CursorOff()
{
   UNLOCK_SYS_REGS;

   /*
    * Don't need to undo the S3 registers here; they will be undone when
    * the mode is restored from save registers.  If it is done here, it
    * causes the cursor to flash each time it is loaded, so don't do that.
    */

   /* Disable cursor */
   s3OutTi3026IndReg(TI_CURS_CONTROL, 0xfc, 0x00);

   LOCK_SYS_REGS;
   return;
}

void
s3Ti3026MoveCursor(pScr, x, y)
     ScreenPtr pScr;
     int   x, y;
{
   extern int s3AdjustCursorXPos;
   extern int s3hotX, s3hotY;
   unsigned char tmp;
#if 0
   if (!xf86VTSema)
      return;
   
   if (s3BlockCursor)
      return;
#endif /* 0 */
   x -= s3InfoRec.frameX0 - s3AdjustCursorXPos;
   x += 64 - s3hotX;
   if (x < 0)
      return;

   y -= s3InfoRec.frameY0;
   y += 64 - s3hotY;
   if (y < 0)
      return;

   UNLOCK_SYS_REGS;

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x03);

   /* Output position - "only" 12 bits of location documented */
   outb(0x3c8, x & 0xFF);
   outb(0x3c9, (x >> 8) & 0x0F);
   outb(0x3c6, y & 0xFF);
   outb(0x3c7, (y >> 8) & 0x0F);

   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp | 0x00);

   LOCK_SYS_REGS;
   return;
}

void
s3Ti3026RecolorCursor(pScr, pCurs)
     ScreenPtr pScr;
     CursorPtr pCurs;
{
   unsigned char tmp;
   UNLOCK_SYS_REGS;

   /* The TI 3026 cursor is always 8 bits so shift 8, not 10 */

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);

   /* Background color */
   outb(0x3c8, 0x01); /* cursor color write address */
   outb(0x3c9, 0xff);
   outb(0x3c9, 0xff);
   outb(0x3c9, 0xff);

   /* Foreground color */
   outb(0x3c8, 0x02); /* cursor color write address */

   outb(0x3c9, 0x00);
   outb(0x3c9, 0x00);
   outb(0x3c9, 0x00);

   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp | 0x00);

   LOCK_SYS_REGS;
   return;
}

void 
s3Ti3026LoadCursor(pScr, pCurs, x, y)
     ScreenPtr pScr;
     CursorPtr pCurs;
     int x, y;
{

#include "pegcursor2.xpm.c"
   extern int s3hotX, s3hotY;
   int index = 0;
   register int   i;
   unsigned char *ram, *p, tmp, tmp1, tmpcurs;
   extern int s3InitCursorFlag;

#if 0
   if (!xf86VTSema)
      return;

   if (!pCurs)
      return;
#endif /* 0 */
   /* turn the cursor off */
   if ((tmpcurs = s3InTi3026IndReg(TI_CURS_CONTROL)) & 0x03)
      s3Ti3026CursorOff();

   /* load colormap */
   s3Ti3026RecolorCursor(pScr, pCurs);
#if 0
   ram = (unsigned char *)pCurs->bits->devPriv[index];
#endif /* 0 */

   UNLOCK_SYS_REGS;
   BLOCK_CURSOR;

   /* position cursor off screen */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x03);

   outb(0x3c8, 0);
   outb(0x3c9, 0);
   outb(0x3c6, 0);
   outb(0x3c7, 0);

   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp | 0x00);

   s3OutTi3026IndReg(TI_CURS_CONTROL, 0xf3, 0x00); /* reset A9,A8 */
   outb(0x3c8, 0x00); /* reset cursor RAM load address A7..A0 */

   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp | 0x02);

   /* 
    * Output the cursor data.  The realize function has put the planes into
    * their correct order, so we can just blast this out.
    */
   p = ram;
#if 0
   for (i = 0; i < 1024; i++,p++)
      outb(0x3c7, (*p));
#else /* 0 */
   {
       int x, y, i, plane;
       int byte;
       int base;
       for (plane = 0; plane<2; plane++) {
	   for (y=0; y<64; y++) {
	       for (x=0; x<64; x+=8) {
		   base = 7;
		   byte = 0;
		   for (i=0; i<8; i++) {
		       switch (pegcursor2_xpm[y+4][x+i]) {
		       case ' ':
			   byte += (plane ? 0 : 0)<<base;
			   break;
		       case '.':
			   byte += (plane ? 1 : 0)<<base;
			   break;
		       case 'X':
			   byte += (plane ? 1 : 1)<<base;
			   break;
		       }
		       base--;
		   }
		   outb(0x3c7, byte);
	       }

		   
	   }
       }	   
   }
#endif	      
   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp);

   UNBLOCK_CURSOR;
   LOCK_SYS_REGS;

   /* position cursor */
   s3Ti3026MoveCursor(0, x, y);

   /* turn the cursor on */
   if ((tmpcurs & 0x03) || s3InitCursorFlag)
      s3Ti3026CursorOn();

   if (s3InitCursorFlag)
      s3InitCursorFlag = FALSE;

   return;
}

/* implementation of HWCurs.if */
#include <nemesis.h>
#include <HWCur.ih>

static HWCur_ShowCursor_fn Ti3026_ShowCursor_m;
static HWCur_MoveCursor_fn Ti3026_MoveCursor_m;
static HWCur_op Ti3026_ms = {
    Ti3026_ShowCursor_m,
    Ti3026_MoveCursor_m
};

static HWCur_cl HWCur_Ti3026_cl  = { &Ti3026_ms, BADPTR};

void Ti3026register(void) {
    IDC_EXPORT("svc>hwcur", HWCur_clp, &HWCur_Ti3026_cl);
}

void Ti3026_ShowCursor_m( 
    HWCur_cl *self,
    bool_t show) {
    if (show) {
	s3Ti3026CursorOn();
	s3Ti3026LoadCursor(0,0,0,0);
    } else {
	s3Ti3026CursorOff();
    }
}

void Ti3026_MoveCursor_m(
    HWCur_cl *self,
    int32_t x,
    int32_t y) {    
    s3Ti3026MoveCursor(0, x, y);
}

