/* $XFree86: xc/programs/Xserver/hw/xfree86/common_hw/S3gendac.h,v 3.3 1995/01/20 04:21:21 dawes Exp $ */
/* Jon Tombs <jon@esix2.us.es>  */


#define GENDAC_INDEX	     0x3C8
#define GENDAC_DATA	     0x3C9
#define BASE_FREQ         14.31818   /* MHz */

int S3gendacSetClock( 
#if NeedFunctionPrototypes
   long freq, int clock
#endif
);     

int ICS5342SetClock( 
#if NeedFunctionPrototypes
   long freq, int clock
#endif
);     

int S3TrioSetClock( 
#if NeedFunctionPrototypes
   long freq, int clock
#endif
);     
