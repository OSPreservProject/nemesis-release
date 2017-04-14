
#include <io.h>

typedef int INT32;
typedef int Bool;
typedef unsigned int CARD32;
typedef unsigned short CARD16;
typedef unsigned char CARD8;
typedef void * pointer;

#if 1
#define Xoutb(port,val) outb(val,port)
#define Xoutw(port,val) outw(val,port)

#else

#define Xoutb(port,val) {printf("Outb %x, %x\n",port,val); outb(val,port);}
#define Xoutw(port,val) {printf("Outw %x, %x\n",port,val); outw(val,port);}
#endif 
