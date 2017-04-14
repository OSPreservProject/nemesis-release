/* $XConsortium: SC11412.h,v 1.1 94/03/28 21:25:22 dpw Exp $ */

/* Norbert Distler ndistler@physik.tu-muenchen.de */

typedef int Bool;
#define TRUE 1
#define FALSE 0
#define QUARZFREQ	        14318
#define MIN_SC11412_FREQ        45000
#define MAX_SC11412_FREQ       100000

Bool SC11412SetClock( 
#if NeedFunctionPrototypes
   long
#endif
);     
