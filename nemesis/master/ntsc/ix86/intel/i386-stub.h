/*
 *      ntsc/ix86/i386-stub.h
 *      ---------------------
 *
 * Header for gdb stub code
 */

#ifndef I386_STUB_H
#define I386_STUB_H

typedef void (Function)();           /* a function */

extern void putpacket(char *buffer);
extern void set_debug_traps(void);

extern void init_debug(void);
extern void init_serial(void);
extern void init_console(void);

/* Immediately jump to the debugger */
extern void breakpoint(void);

extern void handle_exception(int exception) __attribute__((noreturn));

#define NUMREGBYTES 64
enum regnames {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
	       PC /* also known as eip */,
	       PS /* also known as eflags */,
	       CS, SS, DS, ES, FS, GS};

extern int registers[NUMREGBYTES/4];

#endif /* I386_STUB_H */
