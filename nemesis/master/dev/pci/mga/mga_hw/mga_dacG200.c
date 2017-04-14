/* $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/mga/mga_dacG200.c,v 1.1.2.11 1998/11/01 07:51:18 dawes Exp $ */
/*
 * Millennium G200 RAMDAC driver
 */

#include <stdio.h>
#include <string.h>

#include "../common_hw/xf86.h"
#include "../common_hw/vga.h"
#include "../common_hw/pci_stuff.h"

#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"

extern pciTagRec MGAPciTag;

/*
 * exported functions
 */
void MGAG200Restore();

/*
 * implementation
 */
 
/*
 * Read/write to the DAC via MMIO 
 */

/*
 * direct registers
 */
static unsigned char inMGAdreg(reg)
unsigned char reg;
{
	if (!MGAMMIOBase)
		FatalError("MGA: IO registers not mapped\n");

	return INREG8(RAMDAC_OFFSET + reg);
}

static void outMGAdreg(reg, val)
unsigned char reg, val;
{
	if (!MGAMMIOBase)
		FatalError("MGA: IO registers not mapped\n");

	OUTREG8(RAMDAC_OFFSET + reg, val);
}

/*
 * indirect registers
 */
static void outMGAdac(reg, val)
unsigned char reg, val;
{
	outMGAdreg(MGA1064_INDEX, reg);
	outMGAdreg(MGA1064_DATA, val);
}

static unsigned char inMGAdac(reg)
unsigned char reg;
{
	outMGAdreg(MGA1064_INDEX, reg);
	return inMGAdreg(MGA1064_DATA);
}

/*
 * MGAG200Restore
 *
 * This function restores a video mode.	 It basically writes out all of
 * the registers that have previously been saved in the vgaMGARec data 
 * structure.
 */
void 
MGAG200Restore(restore)
vgaMGAG200Ptr restore;
{
	int i;
	int vgaIOBase, temp;

	/*
	 * Code is needed to get things back to bank zero.
	 */

	/* restore DAC regs */
	for (i = 0; i < 0x50; i++)
	    if (i < MGA1064_SYS_PLL_M || i > MGA1064_SYS_PLL_STAT)
		outMGAdac(i, restore->DACreg[i]);

	pciWriteLong(MGAPciTag, PCI_OPTION_REG, restore->DAClong );

	/* restore CRTCEXT regs */
	for (i = 0; i < 6; i++)
		Xoutw(0x3DE, (restore->ExtVga[i] << 8) | i);

	/*
	 * This function handles restoring the generic VGA registers.
	 */
	vgaHWRestore((vgaHWPtr)restore);

	/*
	 * this is needed to properly restore start address (???)
	 */
	Xoutw(0x3DE, (restore->ExtVga[0] << 8) | 0);
}

/*********************************************************************/



void MGA1064SetCursorColors(bg, fg)
	int bg, fg;
{

	/* Background color */
	outMGAdac(MGA1064_CURSOR_COL0_RED,   (bg & 0x00FF0000) >> 16);
	outMGAdac(MGA1064_CURSOR_COL0_GREEN, (bg & 0x0000FF00) >> 8);
	outMGAdac(MGA1064_CURSOR_COL0_BLUE,  (bg & 0x000000FF));

	/* Foreground color */
	outMGAdac(MGA1064_CURSOR_COL1_RED,   (fg & 0x00FF0000) >> 16);
	outMGAdac(MGA1064_CURSOR_COL1_GREEN, (fg & 0x0000FF00) >> 8);
	outMGAdac(MGA1064_CURSOR_COL1_BLUE,  (fg & 0x000000FF));

}

void MGA1064SetCursorPosition(x, y, xorigin, yorigin)
	int x, y, xorigin, yorigin;
{
	unsigned long curpos;

	x += 64-xorigin;
	y += 64-yorigin;

	curpos = ((y & 0xFFF) << 16) | (x & 0xFFF);

	/* Wait for end of vertical retrace */
	while (INREG8(MGAREG_Status) & 0x08)
	{
		/* Wait */
	}

	/* Set new position */
	OUTREG(RAMDAC_OFFSET + MGA1064_CUR_XLOW, curpos);

}

void MGA1064LoadCursorImage(cardbase, src, xorigin, yorigin)
	unsigned char * cardbase;
	register unsigned char *src;
	int xorigin, yorigin;
{
        register int   i;


/*** ARRGHH XXXXX fix me ! fix me ! 
        unsigned char *dest = (unsigned char *)vgaLinearBase
                - (MGAydstorg * vgaBitsPerPixel >> 3) 	
		+ ((vga256InfoRec.videoRam-1)*1024);
		*****/

	unsigned char *dest = cardbase
                - (0 * 16 >> 3) 	
		+ ((8192-1)*1024);



        for (i=0; i<1024; i+=16)
        {
                dest[i]    = src[i+14];
                dest[i+1]  = src[i+12];
                dest[i+2]  = src[i+10];
                dest[i+3]  = src[i+8];
                dest[i+4]  = src[i+6];
                dest[i+5]  = src[i+4];
                dest[i+6]  = src[i+2];
                dest[i+7]  = src[i];
                dest[i+8]  = src[i+15];
                dest[i+9]  = src[i+13];
                dest[i+10] = src[i+11];
                dest[i+11] = src[i+9];
                dest[i+12] = src[i+7];
                dest[i+13] = src[i+5];
                dest[i+14] = src[i+3];
                dest[i+15] = src[i+1];
        }

}

void MGA1064HideCursor()
{

	/* Disable cursor */
	outMGAdac(MGA1064_CURSOR_CTL, 0);

}

void MGA1064ShowCursor()
{

	/* Enable cursor, X11 mode */
	outMGAdac(MGA1064_CURSOR_CTL, 0x03);

}
