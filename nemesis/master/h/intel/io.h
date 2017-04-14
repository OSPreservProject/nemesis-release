#ifndef __IX86_IO_H
#define __IX86_IO_H

#include <ntsc.h>
#include <autoconf/memsys.h>

/*
 * Intel IO defines.  
 *
 * Heavily based on Linux version for the moment.
 */

#ifndef INLINE
#ifdef __GNUC__
#  define INLINE		__inline__
#else
#  define INLINE
#endif
#endif

/*
 * This file contains the definitions for the x86 IO instructions
 * inb/inw/inl/outb/outw/outl and the "string versions" of the same
 * (insb/insw/insl/outsb/outsw/outsl). You can also use "pausing"
 * versions of the single-IO instructions (inb_p/inw_p/..).
 *
 * This file is not meant to be obfuscating: it's just complicated
 * to (a) handle it all in a way that makes gcc able to optimize it
 * as well as possible and (b) trying to avoid writing the same thing
 * over and over again with slight variations and possibly making a
 * mistake somewhere.
 */

/*#ifdef __KERNEL__*/

/*
 * Change virtual addresses to physical addresses and vv.
 * These are trivial on the 1:1 Linux/i386 mapping (but if we ever
 * make the kernel segment mapped at 0, we need to do translation
 * on the i386 as well)
 */
extern INLINE unsigned long virt_to_phys(volatile void * address)
{
#ifdef CONFIG_MEMSYS_EXPT
    word_t pte, pwidth;

    if( (pte = ntsc_trans(((word_t)address) >> PAGE_WIDTH)) == MEMRC_BADVA)
	return (unsigned long)NO_ADDRESS;
    if(!IS_VALID_PTE(pte)) return (unsigned long)NO_ADDRESS;
    pwidth = WIDTH_PTE(pte);
    return ( (PFN_PTE(pte) << PAGE_WIDTH) |
	     ((word_t)address & ((1U << pwidth) - 1)) );
#else
    return (unsigned long) address;
#endif
}

extern INLINE void * phys_to_virt(unsigned long address)
{
    /* NB: phys_to_virt does NOT work on Nemesis (gives 121). */
    return (void *) address;
}

/* Intel's bus and physical addresses are 1 to 1, unlike the Alpha EB164.
 * So we emulate the Alpha's translation trivially: */
#define virt_to_bus(x) virt_to_phys(x)
#define bus_to_virt(x) phys_to_virt(x)

/* XXX SMH: we also do a 1:1 mapping from PCI dense mem to/from phys mem */
#define pci_dense_to_phys(x) ((void*)x)
#define phys_to_pci_dense(x) ((unsigned int)x)

#define memset_io(a,b,c)	memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)	memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)	memcpy((void *)(a),(b),(c))

/*#endif*/ /* __KERNEL__ */

/*
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the x86 architecture, we just read/write the
 * memory location directly.
 */
#define readb(addr) (*(volatile unsigned char *) (addr))
#define readw(addr) (*(volatile unsigned short *) (addr))
#define readl(addr) (*(volatile unsigned int *) (addr))

#define writeb(b,addr) ((*(volatile unsigned char *) (addr)) = (b))
#define writew(b,addr) ((*(volatile unsigned short *) (addr)) = (b))
#define writel(b,addr) ((*(volatile unsigned int *) (addr)) = (b))

/*
 * Talk about misusing macros..
 */

#define __OUT1(s,x) \
extern INLINE void __out##s(unsigned x value, unsigned short port) {

#define __OUT2(s,s1,s2) \
__asm__ __volatile__ ("out" #s " %" s1 "0,%" s2 "1"

#define __OUT(s,s1,x) \
__OUT1(s,x) __OUT2(s,s1,"w") : : "a" (value), "d" (port)); } \
__OUT1(s##c,x) __OUT2(s,s1,"") : : "a" (value), "id" (port)); } \
__OUT1(s##_p,x) __OUT2(s,s1,"w") : : "a" (value), "d" (port)); } \
__OUT1(s##c_p,x) __OUT2(s,s1,"") : : "a" (value), "id" (port)); }

#define __IN1(s) \
extern INLINE RETURN_TYPE __in##s(unsigned short port) { RETURN_TYPE _v;

#define __IN2(s,s1,s2) \
__asm__ __volatile__ ("in" #s " %" s2 "1,%" s1 "0"

#define __IN(s,s1,i...) \
__IN1(s) __IN2(s,s1,"w") : "=a" (_v) : "d" (port) ,##i ); return _v; } \
__IN1(s##c) __IN2(s,s1,"") : "=a" (_v) : "id" (port) ,##i ); return _v; } \
__IN1(s##_p) __IN2(s,s1,"w") : "=a" (_v) : "d" (port) ,##i ); return _v; } \
__IN1(s##c_p) __IN2(s,s1,"") : "=a" (_v) : "id" (port) ,##i ); return _v; }

#define __INS(s) \
extern INLINE void ins##s(unsigned short port, void * addr, unsigned long count) \
{ __asm__ __volatile__ ("cld ; rep ; ins" #s \
: "=D" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }

#define __OUTS(s) \
extern INLINE void outs##s(unsigned short port, const void * addr, unsigned long count) \
{ __asm__ __volatile__ ("cld ; rep ; outs" #s \
: "=S" (addr), "=c" (count) : "d" (port),"0" (addr),"1" (count)); }

#define RETURN_TYPE unsigned char
/* __IN(b,"b","0" (0)) */
__IN(b,"")
#undef RETURN_TYPE
#define RETURN_TYPE unsigned short
/* __IN(w,"w","0" (0)) */
__IN(w,"")
#undef RETURN_TYPE
#define RETURN_TYPE unsigned int
__IN(l,"")
#undef RETURN_TYPE

__OUT(b,"b",char)
__OUT(w,"w",short)
__OUT(l,,int)

__INS(b)
__INS(w)
__INS(l)

__OUTS(b)
__OUTS(w)
__OUTS(l)

/*
 * Note that due to the way __builtin_constant_p() works, you
 *  - can't use it inside a inline function (it will never be true)
 *  - you don't have to worry about side effects within the __builtin..
 */
#define outb(val,port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__outbc((val),(port)) : \
	__outb((val),(port)))

#define inb(port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__inbc(port) : \
	__inb(port))

#define outb_p(val,port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__outbc_p((val),(port)) : \
	__outb_p((val),(port)))

#define inb_p(port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__inbc_p(port) : \
	__inb_p(port))

#define outw(val,port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__outwc((val),(port)) : \
	__outw((val),(port)))

#define inw(port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__inwc(port) : \
	__inw(port))

#define outw_p(val,port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__outwc_p((val),(port)) : \
	__outw_p((val),(port)))

#define inw_p(port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__inwc_p(port) : \
	__inw_p(port))

#define outl(val,port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__outlc((val),(port)) : \
	__outl((val),(port)))

#define inl(port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__inlc(port) : \
	__inl(port))

#define outl_p(val,port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__outlc_p((val),(port)) : \
	__outl_p((val),(port)))

#define inl_p(port) \
((__builtin_constant_p((port)) && (port) < 256) ? \
	__inlc_p(port) : \
	__inl_p(port))

#endif /* __IX86_IO_H */
