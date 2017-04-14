/* $XFree86: xc/programs/Xserver/hw/xfree86/common_hw/Ti3026clk.c,v 3.2 1995/07/01 10:49:07 dawes Exp $ */
/*
 * Copyright 1995 The XFree86 Project, Inc
 *
 * programming the on-chip clock on the Ti3026, derived from the
 * S3 gendac code by Jon Tombs
 * Harald Koenig <koenig@tat.physik.uni-tuebingen.de>
 */

#include "xrip.h"
#include "Ti302X.h" 
#include "compiler.h"
#define NO_OSLIB_PROTOTYPES
#include "xf86_OSlib.h"


#if NeedFunctionPrototypes
static void
s3ProgramTi3026Clock(int clk, unsigned char n, unsigned char m, 
		     unsigned char p, unsigned char ln, unsigned char lm, 
		     unsigned char lp, unsigned char lq)
#else
static void
s3ProgramTi3026Clock(clk, n, m, p, ln, lm, lp, lq)
int clk;
unsigned char n;
unsigned char m;
unsigned char p;
unsigned char ln;
unsigned char lm;
unsigned char lp;
unsigned char lq;
#endif
{
   int tmp;
   /*
    * Reset the clock data index
    */
   s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x00);

   if (clk != TI_MCLK_PLL_DATA) {
      /*
       * Now output the clock frequency
       */
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, (n & 0x3f) | 0x80);
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, (m & 0x3f) );
      tmp = s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA) & 0x40;
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, (p & 3) | tmp | 0xb0);

      /* wait until PLL is locked */
      for (tmp=0; tmp<10000; tmp++) 
	 if (s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA) & 0x40) 
	    break;

      /*
       * And now set up the loop clock for RCLK
       */
      s3OutTi3026IndReg(TI_LOOP_CLOCK_PLL_DATA, 0x00, (ln & 0x3f) | 0x80);
      s3OutTi3026IndReg(TI_LOOP_CLOCK_PLL_DATA, 0x00, (lm & 0x3f));
      s3OutTi3026IndReg(TI_LOOP_CLOCK_PLL_DATA, 0x00, (lp & 3) | 0xf0);
      s3OutTi3026IndReg(TI_MCLK_LCLK_CONTROL, 0xf8, lq);

      /* select pixel clock PLL as dot clock soure */
      s3OutTi3026IndReg(TI_INPUT_CLOCK_SELECT, 0x00, TI_ICLK_PLL);
   }
   else {
      int pn, pm, pp, csr;

      /* select pixel clock PLL as dot clock soure */
      csr = s3InTi3026IndReg(TI_INPUT_CLOCK_SELECT);
      s3OutTi3026IndReg(TI_INPUT_CLOCK_SELECT, 0x00, TI_ICLK_PLL);

      /* save the old dot clock PLL settings */
      s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x00);
      pn = s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA);
      s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x01);
      pm = s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA);
      s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x02);
      pp = s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA);

      /* programm dot clock PLL to new MCLK frequency */
      s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x00);
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, (n & 0x3f) | 0x80);
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, (m & 0x3f) );
      tmp = s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA) & 0x40;
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, (p & 3) | tmp | 0xb0);

      /* wait until PLL is locked */
      for (tmp=0; tmp<10000; tmp++)
	 if (s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA) & 0x40) 
	    break;

      /* switch to output dot clock on the MCLK terminal */
      s3OutTi3026IndReg(0x39, 0xe7, 0x00);
      s3OutTi3026IndReg(0x39, 0xe7, 0x08);
      
      /* Set MCLK */
      s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x00);
      s3OutTi3026IndReg(TI_MCLK_PLL_DATA, 0x00, (n & 0x3f) | 0x80);
      s3OutTi3026IndReg(TI_MCLK_PLL_DATA, 0x00, (m & 0x3f));
      s3OutTi3026IndReg(TI_MCLK_PLL_DATA, 0x00, (p & 3) | 0xb0);

      /* wait until PLL is locked */
      for (tmp=0; tmp<10000; tmp++) 
	 if (s3InTi3026IndReg(TI_MCLK_PLL_DATA) & 0x40) 
	    break;

      /* switch to output MCLK on the MCLK terminal */
      s3OutTi3026IndReg(0x39, 0xe7, 0x10);
      s3OutTi3026IndReg(0x39, 0xe7, 0x18);

      /* restore dot clock PLL */
      s3OutTi3026IndReg(TI_PLL_CONTROL, 0x00, 0x00);
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, pn);
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, pm);
      s3OutTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA, 0x00, pp);

      /* wait until PLL is locked */
      for (tmp=0; tmp<10000; tmp++) 
	 if (s3InTi3026IndReg(TI_PIXEL_CLOCK_PLL_DATA) & 0x40) 
	    break;
      s3OutTi3026IndReg(TI_INPUT_CLOCK_SELECT, 0x00, csr);
   }
}

#if NeedFunctionPrototypes
void Ti3026SetClock(long freq, int clk, int bpp)
#else
void
Ti3026SetClock(freq, clk, bpp)
long freq;
int clk;
int bpp;
#endif
{
#if 1

   int ifreq;
   int n, p, m;
   int ln, lp, lm, lq, lk, z;
   int best_n=32, best_m=32;
   int  diff, mindiff;
   
#define FREQ_MIN   13767  /* ~110000 / 8 */
#define FREQ_MAX  220000

   if (freq < FREQ_MIN)
      ifreq = FREQ_MIN; 
   else if (freq > FREQ_MAX)
      ifreq = FREQ_MAX;
   else
      ifreq = freq;
  


   /* work out suitable timings */

   /* pick the right p value */

   for(p=0; p<4 && ifreq < 110000; p++)
      ifreq *= 2;
   
   /* now 110.0 <= ffreq <= 220.0 */   
   
   ifreq = ifreq*1000 / TI_REF_FREQ_INT;
   
   /* now 7.6825 <= ffreq <= 15.3650 */
   /* the remaining formula is  ffreq = (65-m)*8 / (65-n) */
   
   
   mindiff = ifreq;
   for (n=63; n >= 65 - (int)(TI_REF_FREQ*2); n--) {
      m = 65 - (int)(ifreq * (65-n) / 8000 );
      if (m < 1)
	 m = 1;
      else if (m > 63) 
	 m = 63;
      diff = ((65000-m*1000) * 8) / (65-n) - (ifreq);
      if (diff<0)
	 diff = -diff;
      if (diff < mindiff) {
	 mindiff = diff;
	 best_n = n;
	 best_m = m;
      }
   }

   ln = 65 - 2 * 64 / 8 / bpp;
   lm = 63;
   lk = 1;
   z = (100 * 110000 * (65-ln)) / (lk * freq);
   if (z > 1600) {
      lp = 3;
      lq = (z-1600) / 1600 + 1; /* smallest q greater (z-16)/16 */
   }
   else { /* largest p less then log2(z) */
      for (lp=0; z > (200 << lp); lp++) ;
      lq = 0;
   }

#else

   double ffreq;
   int n, p, m;
   int ln, lp, lm, lq, lk, z;
   int best_n=32, best_m=32;
   double  diff, mindiff;
   
#define FREQ_MIN   13767  /* ~110000 / 8 */
#define FREQ_MAX  220000

   if (freq < FREQ_MIN)
      ffreq = FREQ_MIN / 1000.0;
   else if (freq > FREQ_MAX)
      ffreq = FREQ_MAX / 1000.0;
   else
      ffreq = freq / 1000.0;
   


   /* work out suitable timings */

   /* pick the right p value */

   for(p=0; p<4 && ffreq < 110.0; p++)
      ffreq *= 2;
   
   /* now 110.0 <= ffreq <= 220.0 */   
   
   ffreq /= TI_REF_FREQ;
   
   /* now 7.6825 <= ffreq <= 15.3650 */
   /* the remaining formula is  ffreq = (65-m)*8 / (65-n) */
   
   
   mindiff = ffreq;
   
   for (n=63; n >= 65 - (int)(TI_REF_FREQ/0.5); n--) {
      m = 65 - (int)(ffreq * (65-n) / 8.0 + 0.5);
      if (m < 1)
	 m = 1;
      else if (m > 63) 
	 m = 63;

      
      diff = ((65-m) * 8) / (65.0-n) - ffreq;
      if (diff<0)
	 diff = -diff;

      
      if (diff < mindiff) {
	 mindiff = diff;
	 best_n = n;
	 best_m = m;
      }
   }


   ln = 65 - 2 * 64 / 8 / bpp;
   lm = 63;
   lk = 1;
   z = (100 * 110000 * (65-ln)) / (lk * freq);
   if (z > 1600) {
      lp = 3;
      lq = (z-1600) / 1600 + 1; /* smallest q greater (z-16)/16 */
   }
   else { /* largest p less then log2(z) */
      for (lp=0; z > (200 << lp); lp++) ;
      lq = 0;
   }
#endif
   s3ProgramTi3026Clock(clk, best_n, best_m, p, ln, lm, lp, lq);
}
