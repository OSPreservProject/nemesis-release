#include <nemesis.h>
#include <stdio.h>

#include "xf86.h"
#include "vga.h"

extern int vgaIOBase;

static int currentGraphicsClock = -1;
static int currentExternClock = -1;

#define xf86VTSema	0	// ?????????????????

typedef int ScreenPtr;

#define TRUE  1
#define FALSE 0

#define DACDelay \
        { \
                unsigned char temp = inb(vgaIOBase + 0x0A); \
                temp = inb(vgaIOBase + 0x0A); \
        }

/*
 * vgaSaveScreen -- blank the screen.
 */

Bool
vgaSaveScreen(pScreen, on)
     ScreenPtr pScreen;
     Bool  on;
{

   unsigned char scrn;


//   if (on)
//      SetTimeSinceLastInputEvent();

   if (xf86VTSema) {
      /* the server is running on the current vt */
      /* so just go for it */

      Xoutb(0x3C4,1);
      scrn = inb(0x3C5);

      if (on) {
	 scrn &= 0xDF;			/* enable screen */
      } else {
	 scrn |= 0x20;			/* blank screen */
      }

/*      (*vgaSaveScreenFunc)(SS_START);  XXXXXXX ??? */
      Xoutw(0x3C4, (scrn << 8) | 0x01); /* change mode */
/*      (*vgaSaveScreenFunc)(SS_FINISH);   XXXXXXXX ??? */
   }

   return (TRUE);
}




/*
 * vgaHWRestore --
 *      restore a video mode
 */

void
vgaHWRestore(restore)
     vgaHWPtr restore;
{
  int i,tmp;

  tmp = inb(vgaIOBase + 0x0A);		/* Reset flip-flop */
  Xoutb(0x3C0, 0x00);			/* Enables pallete access */

  /*
   * This here is a workaround a bug in the kd-driver. We MUST explicitely
   * restore the font we got, when we entered graphics mode.
   * The bug was seen on ESIX, and ISC 2.0.2 when using a monochrome
   * monitor. 
   *
   * BTW, also GIO_FONT seems to have a bug, so we cannot use it, to get
   * a font.
   */
  
  vgaSaveScreen(NULL, FALSE);
#if defined(SAVE_TEXT) || defined(SAVE_FONT1) || defined(SAVE_FONT2)
  if(restore->FontInfo1 || restore->FontInfo2 || restore->TextInfo) {
    /*
     * here we switch temporary to 16 color-plane-mode, to simply
     * copy the font-info and saved text
     *
     * BUGALLERT: The vga's segment-select register MUST be set appropriate !
     */
    tmp = inb(vgaIOBase + 0x0A); /* reset flip-flop */
    Xoutb(0x3C0,0x30); Xoutb(0x3C0, 0x01); /* graphics mode */

#ifdef SAVE_FONT1
    if (restore->FontInfo1) {
      Xoutw(0x3C4, 0x0402);    /* write to plane 2 */
      Xoutw(0x3C4, 0x0604);    /* enable plane graphics */
      Xoutw(0x3CE, 0x0204);    /* read plane 2 */
      Xoutw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      Xoutw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy_tobus(restore->FontInfo1, vgaBase, FONT_AMOUNT);
    }
#endif
#ifdef SAVE_FONT2
    if (restore->FontInfo2) {
      Xoutw(0x3C4, 0x0802);    /* write to plane 3 */
      Xoutw(0x3C4, 0x0604);    /* enable plane graphics */
      Xoutw(0x3CE, 0x0304);    /* read plane 3 */
      Xoutw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      Xoutw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy_tobus(restore->FontInfo2, vgaBase, FONT_AMOUNT);
    }
#endif
#ifdef SAVE_TEXT
    if (restore->TextInfo) {
      Xoutw(0x3C4, 0x0102);    /* write to plane 0 */
      Xoutw(0x3C4, 0x0604);    /* enable plane graphics */
      Xoutw(0x3CE, 0x0004);    /* read plane 0 */
      Xoutw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      Xoutw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy_tobus(restore->TextInfo, vgaBase, TEXT_AMOUNT);
      Xoutw(0x3C4, 0x0202);    /* write to plane 1 */
      Xoutw(0x3C4, 0x0604);    /* enable plane graphics */
      Xoutw(0x3CE, 0x0104);    /* read plane 1 */
      Xoutw(0x3CE, 0x0005);    /* write mode 0, read mode 0 */
      Xoutw(0x3CE, 0x0506);    /* set graphics */
      slowbcopy_tobus((char *)restore->TextInfo + TEXT_AMOUNT, vgaBase, TEXT_AMOUNT);
    }
#endif
  }
#endif /* defined(SAVE_TEXT) || defined(SAVE_FONT1) || defined(SAVE_FONT2) */


  vgaSaveScreen(NULL, TRUE);

  tmp = inb(vgaIOBase + 0x0A);			/* Reset flip-flop */
  Xoutb(0x3C0, 0x00);				/* Enables pallete access */

  /* Don't change the clock bits when using an external clock program */
  if (restore->NoClock < 0)
  {
    tmp = inb(0x3CC);
    restore->MiscOutReg = (restore->MiscOutReg & 0xF3) | (tmp & 0x0C);
  }
  if (vgaIOBase == 0x3B0)
    restore->MiscOutReg &= 0xFE;
  else
    restore->MiscOutReg |= 0x01;

  Xoutb(0x3C2, restore->MiscOutReg);

  for (i=1; i<5;  i++) Xoutw(0x3C4, (restore->Sequencer[i] << 8) | i);
  
  /* Ensure CRTC registers 0-7 are unlocked by clearing bit 7 or CRTC[17] */

  Xoutw(vgaIOBase + 4, ((restore->CRTC[17] & 0x7F) << 8) | 17);

  for (i=0; i<25; i++) Xoutw(vgaIOBase + 4,(restore->CRTC[i] << 8) | i);

  for (i=0; i<9;  i++) Xoutw(0x3CE, (restore->Graphics[i] << 8) | i);

  for (i=0; i<21; i++) {
    tmp = inb(vgaIOBase + 0x0A);
    Xoutb(0x3C0,i); Xoutb(0x3C0, restore->Attribute[i]);
  }

#if 0  
  if (clgd6225Lcd)
  {
    for (i= 0; i<768; i++)
    {
      /* The LCD doesn't like white */
      if (restore->DAC[i] == 63) restore->DAC[i]= 62;
    }
  }
#endif

  Xoutb(0x3C6,0xFF);
  Xoutb(0x3C8,0x00);
  for (i=0; i<768; i++)
  {
     Xoutb(0x3C9, restore->DAC[i]);
     DACDelay;
  }

  /* Turn on PAS bit */
  tmp = inb(vgaIOBase + 0x0A);
  Xoutb(0x3C0, 0x20);

}




