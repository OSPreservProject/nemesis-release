/* $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/mga/mga_dac3026.c,v 1.1.2.3 1997/08/02 13:48:22 dawes Exp $ */
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
 * Modified for MGA Millennium by Xavier Ducoin <xavier@rd.lectra.fr>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common_hw/xf86.h"
#include "../common_hw/vga.h"
#include "../common_hw/pci_stuff.h"

#include "mga_bios.h"
#include "mga_reg.h"
#include "mga.h"


extern pciTagRec MGAPciTag;

/*********************************/


/* Set to 1 if you want to set MCLK from XF86Config - AT YOUR OWN RISK! */
#define MCLK_FROM_XCONFIG 0

/*
 * exported functions
 */
void	MGA3026Restore();


/*
 * implementation
 */
 
/*
 * indexes to ti3026 registers (the order is important)
 */
static unsigned char MGADACregs[] = {
	0x0F, 0x18, 0x19, 0x1A, 0x1C,   0x1D, 0x1E, 0x2A, 0x2B, 0x30,
	0x31, 0x32, 0x33, 0x34, 0x35,   0x36, 0x37, 0x38, 0x39, 0x3A,
	0x06
};


/*
 * This is a convenience macro, so that entries in the driver structure
 * can simply be dereferenced with 'newVS->xxx'.
 * change ajv - new conflicts with the C++ reserved word.
 */
#define newVS ((vgaMGAPtr)vgaNewVideoState)

    
vgaMGAPtr vgaNewVideoState;


void FatalError(char *s)
{
fprintf(stderr, "MGA/FB: %s%");
exit(-1);
}


/*
 * direct registers
 */
static unsigned char inTi3026dreg(reg)
unsigned char reg;
{
	if (!MGAMMIOBase)
		FatalError("MGA: IO registers not mapped\n");

	return INREG8(RAMDAC_OFFSET + reg);
}

static void outTi3026dreg(reg, val)
unsigned char reg, val;
{
	if (!MGAMMIOBase)
		FatalError("MGA: IO registers not mapped\n");

	OUTREG8(RAMDAC_OFFSET + reg, val);
}

/*
 * indirect registers
 */
static unsigned char inTi3026(reg)
unsigned char reg;
{
	outTi3026dreg(TVP3026_INDEX, reg);
	return inTi3026dreg(TVP3026_DATA);
}

static void outTi3026(reg, mask, val)
unsigned char reg, mask, val;
{
	unsigned char tmp = mask? (inTi3026(reg) & mask) : 0;

	outTi3026dreg(TVP3026_INDEX, reg);
	outTi3026dreg(TVP3026_DATA, tmp | val);
}

/*
 * MGA3026Restore -- for mga2064 with ti3026
 *
 * This function restores a video mode.	 It basically writes out all of
 * the registers that have previously been saved in the vgaMGARec data 
 * structure.
 */
void 
MGA3026Restore(restore)
vgaMGAPtr restore;
{
	extern void vgaHWRestore(vgaHWPtr restore);
	int i;

	/*
	 * Code is needed to get things back to bank zero.
	 */
	for (i = 0; i < 6; i++)
		Xoutw(0x3DE, (restore->ExtVga[i] << 8) | i);

	pciWriteLong(MGAPciTag, PCI_OPTION_REG, restore->DAClong |
		(pciReadLong(MGAPciTag, PCI_OPTION_REG) & ~0x1000));

	/* select pixel clock PLL as clock source */
	outTi3026(TVP3026_CLK_SEL, 0, restore->DACreg[3]);
	
	/* set loop and pixel clock PLL PLLEN bits to 0 */
	outTi3026(TVP3026_PLL_ADDR, 0, 0x2A);
	outTi3026(TVP3026_LOAD_CLK_DATA, 0, 0);
	outTi3026(TVP3026_PIX_CLK_DATA, 0, 0);
	 
	/*
	 * This function handles restoring the generic VGA registers.
	 */
	vgaHWRestore((vgaHWPtr)restore);

	/*
	 * Code to restore SVGA registers that have been saved/modified
	 * goes here. 
	 */

	/* program pixel clock PLL */
	outTi3026(TVP3026_PLL_ADDR, 0, 0x00);
	for (i = 0; i < 3; i++)
		outTi3026(TVP3026_PIX_CLK_DATA, 0, restore->DACclk[i]);
	 
	if (restore->std.MiscOutReg & 0x08) {
		/* poll until pixel clock PLL LOCK bit is set */
		outTi3026(TVP3026_PLL_ADDR, 0, 0x3F);
		while ( ! (inTi3026(TVP3026_PIX_CLK_DATA) & 0x40) );
	}
	/* set Q divider for loop clock PLL */
	outTi3026(TVP3026_MCLK_CTL, 0, restore->DACreg[18]);
	
	/* program loop PLL */
	outTi3026(TVP3026_PLL_ADDR, 0, 0x00);
	for (i = 3; i < 6; i++)
		outTi3026(TVP3026_LOAD_CLK_DATA, 0, restore->DACclk[i]);
	
	if ((restore->std.MiscOutReg & 0x08) && ((restore->DACclk[3] & 0xC0) == 0xC0) ) {
		/* poll until loop PLL LOCK bit is set */
		outTi3026(TVP3026_PLL_ADDR, 0, 0x3F);
		while ( ! (inTi3026(TVP3026_LOAD_CLK_DATA) & 0x40) );
	}
	
	/*
	 * restore other DAC registers
	 */
	for (i = 0; i < sizeof(MGADACregs); i++)
		outTi3026(MGADACregs[i], 0, restore->DACreg[i]);

#ifdef DEBUG
	ErrorF("PCI retry (0-enabled / 1-disabled): %d\n",
		!!(restore->DAClong & 0x20000000));
#endif		 
}


/*
 * Convert the cursor from server-format to hardware-format.  The Ti3020
 * has two planes, plane 0 selects cursor color 0 or 1 and plane 1
 * selects transparent or display cursor.  The bits of these planes
 * loaded sequentially so that one byte has 8 pixels. The organization
 * looks like:
 *             Byte 0x000 - 0x007    top scan line, left to right plane 0
 *                  0x008 - 0x00F
 *                    .       .
 *                  0x1F8 - 0x1FF    bottom scan line plane 0
 *
 *                  0x200 - 0x207    top scan line, left to right plane 1
 *                  0x208 - 0x20F
 *                    .       .
 *                  0x3F8 - 0x3FF    bottom scan line plane 1
 *
 *             Byte/bit map - D7,D6,D5,D4,D3,D2,D1,D0  eight pixels each
 *             Pixel/bit map - P1P0  (plane 1) == 1 maps to cursor color
 *                                   (plane 1) == 0 maps to transparent
 *                                   (plane 0) maps to cursor colors 0 and 1
 */

#define MAX_CURS_HEIGHT 64   /* 64 scan lines */
#define MAX_CURS_WIDTH  64   /* 64 pixels     */


/*
 * LoadCursor(src)
 * src - pointer returned by RealizeCursor
 */
void
MGA3026LoadCursor(src, xorigin, yorigin)
    unsigned char *src;
    int xorigin, yorigin;
{
    register int i;
    register unsigned char *mask = src + 1;
    
    outTi3026(TVP3026_CURSOR_CTL, 0xf3, 0x00); /* reset A9,A8 */
    /* reset cursor RAM load address A7..A0 */
    outTi3026dreg(TVP3026_WADR_PAL, 0x00); 

    for (i = 0; i < 512; i++, src+=2) 
        outTi3026dreg(TVP3026_CUR_RAM, *src);    
    for (i = 0; i < 512; i++, mask+=2) 
        outTi3026dreg(TVP3026_CUR_RAM, *mask);   

}

static void
MGA3026QueryCursorSize(pwidth, pheight)
    unsigned short *pwidth, *pheight;
{
    *pwidth = MAX_CURS_WIDTH;
    *pheight = MAX_CURS_HEIGHT;
}

static Bool
MGA3026CursorState()
{
    return (inTi3026(TVP3026_CURSOR_CTL) & 0x03);
}

void 
MGA3026CursorOn()
{
    /* Enable cursor - X11 mode */
    outTi3026(TVP3026_CURSOR_CTL, 0x6c, 0x13);
}

static void
MGA3026CursorOff()
{
    /* Disable cursor */
    outTi3026(TVP3026_CURSOR_CTL, 0xfc, 0x00);
}

void
MGA3026MoveCursor(x, y)
    int x, y;
{
    x += 64;
    y += 64;
    if (x < 0 || y < 0)
        return;
        
    /* Output position - "only" 12 bits of location documented */
   
    outTi3026dreg(TVP3026_CUR_XLOW, x & 0xFF);
    outTi3026dreg(TVP3026_CUR_XHI, (x >> 8) & 0x0F);
    outTi3026dreg(TVP3026_CUR_YLOW, y & 0xFF);
    outTi3026dreg(TVP3026_CUR_YHI, (y >> 8) & 0x0F);
}

void
MGA3026RecolorCursor(red0, green0, blue0, red1, green1, blue1)
    int red0, green0, blue0, red1, green1, blue1;
{
    /* The TI 3026 cursor is always 8 bits so shift 8, not 10 */

    /* Background color */
   outTi3026dreg(TVP3026_CUR_COL_ADDR, 1);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (red0 >> 8) & 0xFF);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (green0 >> 8) &0xFF);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (blue0 >> 8) & 0xFF);

    /* Foreground color */
    outTi3026dreg(TVP3026_CUR_COL_ADDR, 2);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (red1 >> 8) & 0xFF);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (green1 >> 8) &0xFF);
    outTi3026dreg(TVP3026_CUR_COL_DATA, (blue1 >> 8) & 0xFF);
}
