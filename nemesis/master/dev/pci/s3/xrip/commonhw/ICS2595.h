/* $XFree86: xc/programs/Xserver/hw/xfree86/common_hw/ICS2595.h,v 3.1 1994/09/23 13:39:43 dawes Exp $ */

/* Norbert Distler ndistler@physik.tu-muenchen.de */

typedef int Bool;
#define TRUE 1
#define FALSE 0
#define QUARZFREQ	        14318
#define MIN_ICS2595_FREQ        79994 /* 85565 */
#define MAX_ICS2595_FREQ       145000     

Bool ICS2595SetClock( 
#if NeedFunctionPrototypes
   long
#endif
);     
