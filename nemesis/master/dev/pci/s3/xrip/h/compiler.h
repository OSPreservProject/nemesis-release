/* no idea what this file used to do, but never mind */
#include <stdio.h>

#undef outb
#undef outw
#undef outl
#undef inb
#undef inw
#undef inl
#ifndef IOTRACE
#define outb(reg, val) port_outb(reg,val)
#define outw(reg, val) port_outw(reg,val)
#define outl(reg, val) port_outl(reg,val)
#define inb(x) port_inb(x)
#define inw(x) port_inw(x)
#define inl(x) port_inl(x)
#else
#define inb(x) ({ int p = x; int b = port_inb(p); fprintf(stderr,"%s:%d (%s) inb(0x%x=0x%x)\n",__FILE__,0,__FUNCTION__,p, b); b;})

#define outb(x,y) fprintf(stderr,"%s:%d (%s) outb(0x%x,0x%x)\n",__FILE__,0,__FUNCTION__,x,y),port_outb(x,y)


#define inw(x) ({ int p = x; int b = port_inw(p); fprintf(stderr,"%s:%d (%s) inw(0x%x=0x%x)\n",__FILE__,0,__FUNCTION__,p, b); b;})

#define outw(x,y) fprintf(stderr,"%s:%d (%s) outw(0x%x,0x%x)\n",__FILE__,0,__FUNCTION__,x,y),port_outw(x,y)

#define outl(x,y) fprintf(stderr,"%s:%d (%s) outl(0x%x,0x%x)\n",__FILE__,0,__FUNCTION__,x,y),port_outl(x,y)
#endif
void port_outb(int reg, int val);
void port_outl(int reg, int val);
void port_outw(int reg, int val);
int port_inb(int x);
int port_inw(int x);
int port_inl(int x);
#include <nemesis.h>

#ifdef SLOWMESSAGES
#define MESSAGE_RATE SECONDS(0)
#define ErrorF PAUSE(MESSAGE_RATE); ErrorF
#endif
