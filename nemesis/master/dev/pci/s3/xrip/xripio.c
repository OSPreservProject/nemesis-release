
#include <io.h>



void port_outw(int reg, int val) {
  outw(val,reg);
}

void outw32(int reg, int val) {
  outl(val,reg);
}

void port_outb(int reg, int val) {
  outb(val,reg);
}

void port_outl(int reg, long val) {
  outl(val,reg);
}

int port_inb(int reg) {
  return inb(reg);
}

int port_inw(int reg) {
    return inw(reg);
}

int port_inl(int reg) {
    return inl(reg);
}
