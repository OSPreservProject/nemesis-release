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
**      dev/pci/mga/mga_hw.c
**
** FUNCTIONAL DESCRIPTION:
** 
**      Device driver for PMAGBBA framestore 
** 
** ENVIRONMENT: 
**
**      Privileged domain
** 
**
*/

#include <ntsc.h>
#include <nemesis.h>
#include <stdio.h>
#include <contexts.h>
#include <exceptions.h>
#include <frames.h>

#include <Frames.ih>
#include <PCIBios.ih>
#include <pci.h>

#include "../common_hw/xf86.h"
#include "../common_hw/vga.h"
#include "../common_hw/pci_stuff.h"

#include "mga.h"

#define TRC(_x)

pciTagRec MGAPciTag;
unsigned int MGAMMIOBase = 0;
uint32_t vgaIOBase;
PCIBios_clp pcibios;

extern vgaMGARec MGAmodes[];     // defined in modes.c
extern vgaMGARec MGAmodesG200[];     // defined in modes.c

extern void MGA3026Restore( vgaMGAPtr restore);

extern pointer MGA3026RealizeCursor(pSource, pMask, w, h, ram);
extern void MGA3026LoadCursor(src);
extern void MGA3026QueryCursorSize(pwidth, pheight);
extern Bool MGA3026CursorState();
extern void MGA3026CursorOn();
extern void MGA3026CursorOff();
extern void MGA3026MoveCursor(x, y);
extern void MGA3026RecolorCursor(red0, green0, blue0, red1, green1, blue1);


extern void MGAG200Restore(restore);
extern void MGA1064SetCursorColors(bg, fg);
extern void MGA1064SetCursorPosition(x, y, xorigin, yorigin);
extern void MGA1064LoadCursorImage(cardbase, src, xorigin, yorigin);
extern void MGA1064HideCursor();
extern void MGA1064ShowCursor();


typedef enum { DAC_unknown, DAC_MGA3026, DAC_MGAG200 } dac_type_t;

dac_type_t dac_type = DAC_unknown;

#define PCI_VGA_CLASS            0x00030000 

#define PCI_VENDOR_MATROX        0x102B

#define PCI_CHIP_MGA2164         0x051B
#define PCI_CHIP_MGA2164_AGP	 0x051F

#define PCI_CHIP_MGAG200_PCI     0x0520
#define PCI_CHIP_MGAG200         0x0521




unsigned char * MGAInit(int Width, int Height, int Depth, int Mode)
{
    uchar_t bus, dev_fn;
    PCIBios_Error retcode;
    uint32_t MMIOBase0, MMIOBase1, temp;
    unsigned char * ScreenBase_phys;
    word_t ScreenBase_pci; 


    TRC(printf("MGA/FB: Init entered\n"));

    TRY {
	pcibios = NAME_FIND("sys>PCIBios", PCIBios_clp);
    } 
    CATCH_Context$NotFound(UNUSED name) {
	printf("pci_probe: PCIBios not present\n");
	return NULL;
    }
    ENDTRY;

    ENTER_KERNEL_CRITICAL_SECTION();
    retcode = PCIBios$FindDevice(pcibios, 
				 PCI_VENDOR_MATROX, PCI_CHIP_MGA2164, 0,
				 &bus, &dev_fn);
    LEAVE_KERNEL_CRITICAL_SECTION();
  
    if (retcode == PCIBios_Error_OK)
    {
	printf("MGA: Matrox Millennium II PCI (MGA2164) found.\n");
	dac_type = DAC_MGA3026;
	goto CardFound;
    }

    ENTER_KERNEL_CRITICAL_SECTION();
    retcode = PCIBios$FindDevice(pcibios,
                                 PCI_VENDOR_MATROX, PCI_CHIP_MGA2164_AGP, 0,
                                 &bus, &dev_fn);
    LEAVE_KERNEL_CRITICAL_SECTION();
 
    if (retcode == PCIBios_Error_OK)
    {
        printf("MGA: Matrox Millennium II AGP (MGA2164) found.\n");
	dac_type = DAC_MGA3026;
        goto CardFound;
    }


    ENTER_KERNEL_CRITICAL_SECTION();
    retcode = PCIBios$FindDevice(pcibios,
                                 PCI_VENDOR_MATROX, PCI_CHIP_MGAG200, 0,
                                 &bus, &dev_fn);
    LEAVE_KERNEL_CRITICAL_SECTION();
 
    if (retcode == PCIBios_Error_OK)
    {
        printf("MGA: Matrox G200 AGP found!\n");
	dac_type = DAC_MGAG200;
        goto CardFound;
    }

    ENTER_KERNEL_CRITICAL_SECTION();
    retcode = PCIBios$FindDevice(pcibios,
                                 PCI_VENDOR_MATROX, PCI_CHIP_MGAG200_PCI, 0,
                                 &bus, &dev_fn);
    LEAVE_KERNEL_CRITICAL_SECTION();
 
    if (retcode == PCIBios_Error_OK)
    {
        printf("MGA: Matrox G200 PCI found.\n");
	dac_type = DAC_MGAG200;
        goto CardFound;
    }


    printf(
            "MGA: Couldn't find a Matrox Millennium II or G200. (Vend %x)\n",
            PCI_VENDOR_MATROX);

    return NULL;

CardFound:

	/* Do magic stuff to initilise the card */
	MGAPciTag.tag = ((uint32_t)bus<<16) | ((uint32_t)dev_fn<<8);
	
	/* read out out Mem base */

	ENTER_KERNEL_CRITICAL_SECTION();
	PCIBios$ReadConfigDWord(pcibios, bus, dev_fn,
				PCI_BASE_ADDRESS_0,
				&MMIOBase0);
	LEAVE_KERNEL_CRITICAL_SECTION();

	ScreenBase_pci  = MMIOBase0 & 0xff800000;
	ScreenBase_phys = pci_dense_to_phys(ScreenBase_pci);
	TRC(printf("MGA: Screen Base PCI %x, PHYS %p\n", 
		   ScreenBase_pci, ScreenBase_phys));	
	
	ENTER_KERNEL_CRITICAL_SECTION();
	PCIBios$ReadConfigDWord(pcibios, bus, dev_fn,
				PCI_BASE_ADDRESS_1,
				&MMIOBase1);
	LEAVE_KERNEL_CRITICAL_SECTION();
	
	MGAMMIOBase=(unsigned int)(word_t)(MMIOBase1 & 0xffffc000);
	TRC(printf("MGA: IO Base PCI %x\n", MGAMMIOBase));
	
	vgaIOBase = (inb(0x3CC) & 0x01) ? 0x3D0 : 0x3B0;
	TRC(printf("MGA: vgaIO Base %x\n",vgaIOBase));

	/* SMH: allocate stretches over our framebuffer/register memory */
	{
	    Mem_PMemDesc pmem;
	    Stretch_clp fbstr, iostr; 
	    Stretch_Size size;
	    Frames_clp frames; 
	    addr_t pbase; 
	    uint32_t fwidth; 
	    Mem_Attrs attr; 
	    
	    /* size = (Width * Height * Depth) >> 3; */
	    
	    size  = 8*1024*1024; /* HACK for G200 cursor */

	    frames = NAME_FIND("sys>Frames", Frames_clp);
	    pbase  = Frames$AllocRange(frames, size, FRAME_WIDTH, 
				       ScreenBase_phys, 0); 
	    fwidth = Frames$Query(frames, pbase, &attr);
	    pmem.start_addr  = pbase; 
	    pmem.nframes     = BYTES_TO_LFRAMES(size, fwidth); 
	    pmem.frame_width = fwidth; 
	    pmem.attr        = attr | SET_ELEM(Mem_Attr_NoCache);
	    
	    fbstr = StretchAllocator$NewAt(Pvs(salloc), size, 0,
					   ScreenBase_phys, 0, &pmem);

	    size   = 0x4000;  /* XXX SMH: should sort out size from pcibios */
	    pbase  = Frames$AllocRange(frames, size, FRAME_WIDTH, 
				       pci_dense_to_phys(MGAMMIOBase),
				       0); 
	    fwidth           = Frames$Query(frames, pbase, &attr);
	    pmem.start_addr  = pbase; 
	    pmem.nframes     = BYTES_TO_LFRAMES(size, fwidth); 
	    pmem.frame_width = fwidth; 
	    pmem.attr        = attr | SET_ELEM(Mem_Attr_NoCache) | 
		               SET_ELEM(Mem_Attr_NonMemory);
	    
	    iostr = StretchAllocator$NewAt(Pvs(salloc), size, 0, 
					   pci_dense_to_phys(MGAMMIOBase),
					   0, &pmem);
	    
	}



	
	/* Unprotect CRTC[0-7] */
	Xoutb(vgaIOBase + 4, 0x11); temp = inb(vgaIOBase + 5);
	Xoutb(vgaIOBase + 5, temp & 0x7F);


	switch (dac_type)
	{
	case DAC_MGAG200:
	    MGAG200Restore( &MGAmodesG200[Mode] );
	    break;
	case DAC_MGA3026:
	    MGA3026Restore( &MGAmodes[Mode] );
	    break;

	default:
	    printf(
		"MGA: Major Error - shouldn't get here!\n");	   
	}

	/* support only one card for now. */
	return ScreenBase_phys; 
}



#define CURSOR_WIDTH  64
#define CURSOR_HEIGHT 64

void MGALoadCursor(char *cardbase, int index, char *cursor)
{

    int x,y,i,ss,mm;
    uchar_t Xcur[2048], *Xcp;
    char ** curs;

    Xcp = Xcur;


/* Stage 0: Turn cursor off */
/*    MGA3026CursorOff(); */

/* Stage 1: Parse the .xpm into standard X Cursor format */

    curs = (char **)cursor; /* its in xpm format */

    for (y=0; y < CURSOR_HEIGHT; y++) {
        for (x=0; x < CURSOR_WIDTH; x+=8) {
	    ss=mm=0;
	    for(i=0;i<8;i++)
	    {
		mm<<=1; ss<<=1;
                switch (curs[y+4][x+i]) {
                case ' ': /* transparent => planes 0, 1 both have '0' */
                    break;
                case '.': /* cursor color 0; plane 0@1, plane 1@0 */
		    mm|=0x1;
                    break;
                case 'X': /* cursor color 1; plane 0@1, plane 1@1 */
		    mm|=0x1; ss|=0x1;
                    break;
                default: 
                    printf("got bogus char '%c'\n", curs[y+4][x+i]);
                }
	    }
	    *Xcp++ = ss; *Xcp++ = mm;
        }
    }

    if ( dac_type == DAC_MGA3026 )
    {

/* Stage 3: Load the MGA format cursor */
	MGA3026LoadCursor( Xcur, 0, 0 );
    
/* Stage 4: Set Sursor Colour (should pass xpm...) */
	MGA3026RecolorCursor( 0xffff, 0xffff, 0xffff, 0, 0, 0);

/* Stage 5: Turn Cursor On */
	MGA3026CursorOn();

    }

    if ( dac_type == DAC_MGAG200 )
    {

/* Stage 3: Load the MGA format cursor */
	MGA1064LoadCursorImage( cardbase, Xcur , 0, 0 );
    
/* Stage 4: Set Sursor Colour (should pass xpm...) */
	MGA1064SetCursorColors( 0xffffff, 0 );

/* Stage 5: Turn Cursor On */
	MGA1064ShowCursor();
    }

}

void MGAMoveCursor(char *scr, int idx, int x, int y)
{
    if ( dac_type == DAC_MGA3026 ) MGA3026MoveCursor(x, y);
    if ( dac_type == DAC_MGAG200 ) MGA1064SetCursorPosition(x, y, 0, 0);
}
