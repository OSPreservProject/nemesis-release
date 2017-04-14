/* $XFree86: xc/programs/Xserver/hw/xfree86/vga256/drivers/mga/mga.h,v 3.8.2.3 1997/07/26 06:30:53 dawes Exp $ */
/*
 * MGA Millennium (MGA2064W) functions
 *
 * Copyright 1996 The XFree86 Project, Inc.
 *
 * Authors
 *		Dirk Hohndel
 *			hohndel@XFree86.Org
 *		David Dawes
 *			dawes@XFree86.Org
 */

#ifndef MGA_H
#define MGA_H

typedef struct {
	vgaHWRec std;                           /* good old IBM VGA */
	unsigned int DAClong;
	unsigned char DACclk[6];
	unsigned char DACreg[28];
	unsigned char ExtVga[6];
} vgaMGARec, *vgaMGAPtr;

typedef struct {
	vgaHWRec std;                           /* good old IBM VGA */
	unsigned long DAClong;
	unsigned char DACreg[0x50];
	unsigned char ExtVga[6];
} vgaMGAG200Rec, *vgaMGAG200Ptr;

/* This is not a pointer, it is a PCI value argument for readb etc. */
extern unsigned int MGAMMIOBase;

/* RJB & IAP & Rolf, 28th Apr 1999.  We need 'outb' semantics on
 * 'readb' style memory translation.  The 'fix' below is not ideal,
 * but at least it works. */

#if 1
#ifndef NTSC_MB /* ought to be defined for all architectures */
#define NTSC_MB (void)0
#endif
#define INREG8(addr) readb(MGAMMIOBase+addr)
#define INREG16(addr) readw(MGAMMIOBase+addr)
#define OUTREG8(addr, val) do { writeb(val, MGAMMIOBase+addr); NTSC_MB; } while(0)
#define OUTREG16(addr, val) do { writew(val, MGAMMIOBase+addr); NTSC_MB; } while(0)
#define OUTREG(addr, val) do { writel(val, MGAMMIOBase+addr); NTSC_MB; } while(0)

#else

#define INREG8(addr) (printf("readb %x\n",MGAMMIOBase+addr),readb(MGAMMIOBase+addr))
#define INREG16(addr) (printf("readw %x\n",MGAMMIOBase+addr),readw(MGAMMIOBase+addr))}
#define OUTREG8(addr, val) {printf("writeb %x %x\n",val, MGAMMIOBase+addr);writeb(val, MGAMMIOBase+addr);}
#define OUTREG16(addr, val) {printf("writew %x %x\n",val, MGAMMIOBase+addr);writew(val, MGAMMIOBase+addr);}
#define OUTREG(addr, val) {printf("write %x %x\n",val, MGAMMIOBase+addr);write(val, MGAMMIOBase+addr);}
#endif


#if 0
#if defined(__alpha__)
#define mb() __asm__ __volatile__("mb": : :"memory")
#define INREG8(addr) xf86ReadSparse8(MGAMMIOBase, (addr))
#define INREG16(addr) xf86ReadSparse16(MGAMMIOBase, (addr))
#define OUTREG8(addr,val) do { xf86WriteSparse8((val),MGAMMIOBase,(addr)); \
				mb();} while(0)
#define OUTREG16(addr,val) do { xf86WriteSparse16((val),MGAMMIOBase,(addr)); \
				mb();} while(0)
#define OUTREG(addr, val) do { xf86WriteSparse32((val),MGAMMIOBase,(addr)); \
				mb();} while(0)
#else /* __alpha__ */
#define INREG8(addr) *(volatile CARD8 *)(MGAMMIOBase + (addr))
#define INREG16(addr) *(volatile CARD16 *)(MGAMMIOBase + (addr))
#define OUTREG8(addr, val) *(volatile CARD8 *)(MGAMMIOBase + (addr)) = (val)
#define OUTREG16(addr, val) *(volatile CARD16 *)(MGAMMIOBase + (addr)) = (val)
#define OUTREG(addr, val) *(volatile CARD32 *)(MGAMMIOBase + (addr)) = (val)
#endif /* __alpha__ */

#endif

#define MGAISBUSY() (INREG8(MGAREG_Status + 2) & 0x01)
#define MGAWAITFIFO() while(INREG16(MGAREG_FIFOSTATUS) & 0x100);
#define MGAWAITFREE() while(MGAISBUSY());
#define MGAWAITFIFOSLOTS(slots) while ( ((INREG16(MGAREG_FIFOSTATUS) & 0x3f) - (slots)) < 0 );

//extern MGARamdacRec MGAdac;
//extern pciTagRec MGAPciTag;
extern int MGAchipset;
extern int MGArev;
extern int MGAinterleave;
extern int MGABppShft;
extern int MGAusefbitblt;
extern int MGAydstorg;
#ifdef __alpha__
extern unsigned char *MGAMMIOBaseDENSE;
#endif

/*
 * ROPs
 *
 * for some silly reason, the bits in the ROPs are just the other way round
 */

/*
 * definitions for the new acceleration interface
 */
#define WAITUNTILFINISHED()	MGAWAITFREE()
#define SETBACKGROUNDCOLOR(col)	OUTREG(MGAREG_BCOL, (col))
#define SETFOREGROUNDCOLOR(col)	OUTREG(MGAREG_FCOL, (col))
#define SETRASTEROP(rop)	mga_cmd |= (((rop & 1)==1)*8 | \
					    ((rop & 2)==2)*4 | \
					    ((rop & 4)==4)*2 | \
					    ((rop & 8)==8)) << 16;
#define SETWRITEPLANEMASK(pm)	OUTREG(MGAREG_PLNWT, (pm))
#define SETBLTXYDIR(x,y)	OUTREG(MGAREG_SGN, ((-x+1)>>1)+4*((-y+1)>>1))

#endif
