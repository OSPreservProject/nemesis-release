/* $XFree86: xc/programs/Xserver/hw/xfree86/accel/s3/IBMRGBCurs.c,v 3.0 1995/06/29 13:30:39 dawes Exp $ */

#define NEED_EVENTS

#include <X.h>

#include <misc.h>
#include <input.h>


#include <scrnintstr.h>
#include <servermd.h>

#include <nemesis.h>
#include <IDCMacros.h>

#include "xrip.h"


#define MAX_CURS_HEIGHT 64   /* 64 scan lines */
#define MAX_CURS_WIDTH  64   /* 64 pixels     */


#ifndef __GNUC__
# define __inline__ /**/
#endif

typedef void * CursorPtr;

#define BLOCK_CURSOR 
#define UNBLOCK_CURSOR

void 
s3IBMRGBCursorOn()
{
   unsigned char tmp;

   UNLOCK_SYS_REGS;

   /* turn on external cursor */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xDF;
   outb(vgaCRReg, tmp | 0x20);

   /* Enable IBMRGB */
   outb(vgaCRIndex, 0x45);
   tmp = inb(vgaCRReg);
   outb(vgaCRReg, tmp & ~0x20);
   
   /* Enable cursor - X11 mode */
   s3OutIBMRGBIndReg(IBMRGB_curs, 0, 0x27);

   LOCK_SYS_REGS;
   return;
}

void
s3IBMRGBCursorOff()
{
   UNLOCK_SYS_REGS;

   /*
    * Don't need to undo the S3 registers here; they will be undone when
    * the mode is restored from save registers.  If it is done here, it
    * causes the cursor to flash each time it is loaded, so don't do that.
    */

   /* Disable cursor */
   s3OutIBMRGBIndReg(IBMRGB_curs, ~3, 0x00);

   LOCK_SYS_REGS;
   return;
}

void
s3IBMRGBMoveCursor(pScr, x, y)
     void * pScr;
     int   x, y;
{
   extern int s3AdjustCursorXPos;
#if 0
   extern 

#endif /* 0 */
int s3hotX, s3hotY;



   
   unsigned char tmp;
#if 1
   s3hotX = 1;
   s3hotY = 1;
#endif
#if 0   
   if (!xf86VTSema)
      return;

   if (s3BlockCursor)
      return;
#endif /* 0 */
#if 0    
   x -= s3InfoRec.frameX0 - s3AdjustCursorXPos;
/*   x += 64 - s3hotX; */
#endif /* 0 */
   if (x < 0)
      return;
#if 0
   y -= s3InfoRec.frameY0;
#endif /* 0 */
/*   y += 64 - s3hotY; */
   if (y < 0)
      return;

   UNLOCK_SYS_REGS;

#if 0
   s3OutIBMRGBIndReg(IBMRGB_curs_hot_x, 0, s3hotX);
   s3OutIBMRGBIndReg(IBMRGB_curs_hot_y, 0, s3hotY);
   s3OutIBMRGBIndReg(IBMRGB_curs_xl, 0, x);
   s3OutIBMRGBIndReg(IBMRGB_curs_xh, 0, x>>8);
   s3OutIBMRGBIndReg(IBMRGB_curs_yl, 0, y);
   s3OutIBMRGBIndReg(IBMRGB_curs_yh, 0, y>>8);
#else /* 0 */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_hot_x);  outb(IBMRGB_INDEX_DATA, s3hotX);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_hot_y);  outb(IBMRGB_INDEX_DATA, s3hotY);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_xl);     outb(IBMRGB_INDEX_DATA, x);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_xh);     outb(IBMRGB_INDEX_DATA, x>>8);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_yl);     outb(IBMRGB_INDEX_DATA, y);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_yh);     outb(IBMRGB_INDEX_DATA, y>>8);
   outb(vgaCRReg, tmp);
#endif

   LOCK_SYS_REGS;
   return;
}

void
s3IBMRGBRecolorCursor(ScreenPtr pScr, CursorPtr pCurs)
{
   unsigned char tmp;
   UNLOCK_SYS_REGS;

   /* The IBM RGB52x cursor is always 8 bits so shift 8, not 10 */

#if 0
   /* Background color */
   s3OutIBMRGBIndReg(IBMRGB_curs_col1_r, 0, (pCurs->backRed   >> 8));
   s3OutIBMRGBIndReg(IBMRGB_curs_col1_g, 0, (pCurs->backGreen >> 8));
   s3OutIBMRGBIndReg(IBMRGB_curs_col1_b, 0, (pCurs->backBlue  >> 8));

   /* Foreground color */
   s3OutIBMRGBIndReg(IBMRGB_curs_col2_r, 0, (pCurs->foreRed   >> 8));
   s3OutIBMRGBIndReg(IBMRGB_curs_col2_g, 0, (pCurs->foreGreen >> 8));
   s3OutIBMRGBIndReg(IBMRGB_curs_col2_b, 0, (pCurs->foreBlue  >> 8));
#else /* 0 */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_col1_r); 
   outb(IBMRGB_INDEX_DATA, /* (pCurs->backRed   >> 8) */ 0xff);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_col1_g); 
   outb(IBMRGB_INDEX_DATA, /* (pCurs->backGreen >> 8) */ 0xff);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_col1_b); 
   outb(IBMRGB_INDEX_DATA, /* (pCurs->backBlue  >> 8) */ 0xff);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_col2_r); 
   outb(IBMRGB_INDEX_DATA, /* (pCurs->foreRed   >> 8) */ 0x00);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_col2_g); 
   outb(IBMRGB_INDEX_DATA, /* (pCurs->foreGreen >> 8) */ 0x00);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_col2_b); 
   outb(IBMRGB_INDEX_DATA, /* (pCurs->foreBlue  >> 8) */ 0x00);
   outb(vgaCRReg, tmp);
#endif

   LOCK_SYS_REGS;
   return;
}

void 
s3IBMRGBLoadCursor(pScr, pCurs, x, y)
     ScreenPtr pScr;
     CursorPtr pCurs;
     int x, y;
{
#if 0   
    extern int s3hotX, s3hotY;
    
    int   index = 0 pScr->myNum;
#endif /* 0 */
#include "pegcursor2.xpm.c"
   register int   i;
   unsigned char *ram, *p, tmp, tmp2, tmpcurs;
   extern int s3InitCursorFlag;

#if 0
   if (!xf86VTSema)
      return;

   if (!pCurs)
      return;
#endif /* 0 */
   /* turn the cursor off */
   if ((tmpcurs = s3InIBMRGBIndReg(IBMRGB_curs)) & 0x03)
      s3IBMRGBCursorOff();

   /* load colormap */
   s3IBMRGBRecolorCursor(pScr, pCurs);
#if 0
   ram = (unsigned char *)pCurs->bits->devPriv[index];
#endif /* 0 */
   UNLOCK_SYS_REGS;

   BLOCK_CURSOR;
#if 0
   /* position cursor off screen */
   s3OutIBMRGBIndReg(IBMRGB_curs_hot_x, 0, 0);
   s3OutIBMRGBIndReg(IBMRGB_curs_hot_y, 0, 0);
   s3OutIBMRGBIndReg(IBMRGB_curs_xl, 0, 0xff);
   s3OutIBMRGBIndReg(IBMRGB_curs_xh, 0, 0x7f);
   s3OutIBMRGBIndReg(IBMRGB_curs_yl, 0, 0xff);
   s3OutIBMRGBIndReg(IBMRGB_curs_yh, 0, 0x7f);

   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);

   tmp2 = inb(IBMRGB_INDEX_CONTROL) & 0xfe;
   outb(IBMRGB_INDEX_CONTROL, tmp2 | 1);  /* enable auto-increment */
   
   outb(IBMRGB_INDEX_HIGH, IBMRGB_curs_array >> 8);
   outb(IBMRGB_INDEX_LOW,  IBMRGB_curs_array);

   p = ram;
   for (i = 0; i < 1024; i++,p++)
      outb(IBMRGB_INDEX_DATA, (*p));

   outb(IBMRGB_INDEX_HIGH, 0);
   outb(IBMRGB_INDEX_CONTROL, tmp2);  /* disable auto-increment */
   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp);
#else /* 0 */
   outb(vgaCRIndex, 0x55);
   tmp = inb(vgaCRReg) & 0xFC;
   outb(vgaCRReg, tmp | 0x01);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_hot_x);  outb(IBMRGB_INDEX_DATA, 0);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_hot_y);  outb(IBMRGB_INDEX_DATA, 0);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_xl);     outb(IBMRGB_INDEX_DATA, 0xff);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_xh);     outb(IBMRGB_INDEX_DATA, 0x7f);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_yl);     outb(IBMRGB_INDEX_DATA, 0xff);
   outb(IBMRGB_INDEX_LOW,IBMRGB_curs_yh);     outb(IBMRGB_INDEX_DATA, 0x7f);

   /* 
    * Output the cursor data.  The realize function has put the planes into
    * their correct order, so we can just blast this out.
    */
   tmp2 = inb(IBMRGB_INDEX_CONTROL) & 0xfe;
   outb(IBMRGB_INDEX_CONTROL, tmp2 | 1);  /* enable auto-increment */
   
   outb(IBMRGB_INDEX_HIGH, IBMRGB_curs_array >> 8);
   outb(IBMRGB_INDEX_LOW,  IBMRGB_curs_array);

#if 0
   p = ram;
   for (i = 0; i < 1024; i++,p++)
      outb(IBMRGB_INDEX_DATA, (*p));
#else /* 0 */
   {
       int x, y, i;
       for (y=0; y<64; y++) {
	   for (x=0; x<64; x+=4) {
	       int byte;
	       int base;
	       base = 1;
	       byte = 0;
	       for (i=0; i<4; i++) {
		   switch (pegcursor2_xpm[y+4][x+3-i]) {
		   case ' ':
		       byte = (byte & (255 - (3*base)));
		       break;
		   case '.':
		       byte = (byte & (255 - (3*base))) | 2*base;
		       break;
		   case 'X':
		       byte = (byte & (255 - (3*base))) | 3*base;
		       break;
		   }
		   base = base << 2;
	       }
	       outb(IBMRGB_INDEX_DATA, byte);
	   }
       }
   }	   
#endif	       
   outb(IBMRGB_INDEX_HIGH, 0);
   outb(IBMRGB_INDEX_CONTROL, tmp2);  /* disable auto-increment */
   outb(vgaCRIndex, 0x55);
   outb(vgaCRReg, tmp);
#endif
   UNBLOCK_CURSOR;
   LOCK_SYS_REGS;

   /* position cursor */
   s3IBMRGBMoveCursor(0, x, y);

   /* turn the cursor on */
   if ((tmpcurs & 0x03) || s3InitCursorFlag)
      s3IBMRGBCursorOn();

   if (s3InitCursorFlag)
      s3InitCursorFlag = FALSE;

   return;
}


/* implementation of HWCurs.if */
#include <nemesis.h>
#include <HWCur.ih>

static HWCur_ShowCursor_fn IBMRGB_ShowCursor_m;
static HWCur_MoveCursor_fn IBMRGB_MoveCursor_m;
static HWCur_op IBMRGB_ms = {
    IBMRGB_ShowCursor_m,
    IBMRGB_MoveCursor_m
};

static HWCur_cl HWCur_IBMRGB_cl  = { &IBMRGB_ms, BADPTR};

void IBMRGBregister(void) {
    IDC_EXPORT("svc>hwcur", HWCur_clp, &HWCur_IBMRGB_cl);
}

void IBMRGB_ShowCursor_m( 
    HWCur_cl *self,
    bool_t show) {
    if (show) {
	s3IBMRGBCursorOn();
	s3IBMRGBLoadCursor(0,0,0,0);
    } else {
	s3IBMRGBCursorOff();
    }
}

void IBMRGB_MoveCursor_m(
    HWCur_cl *self,
    int32_t x,
    int32_t y) {    
    s3IBMRGBMoveCursor(0, x, y);
}
