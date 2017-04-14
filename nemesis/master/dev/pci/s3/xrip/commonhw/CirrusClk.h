/* $XConsortium: CirrusClk.h,v 1.2 95/01/27 14:46:39 kaleb Exp $ */
/* $XFree86: xc/programs/Xserver/hw/xfree86/common_hw/CirrusClk.h,v 3.1 1995/01/28 15:58:09 dawes Exp $ */

int CirrusFindClock( 
#if NeedFunctionPrototypes
   int freq, int *num_out, int *den_out, int *usemclk_out
#endif
);     

int CirrusSetClock(
#if NeedFunctionPrototypes
   int freq
#endif
);
