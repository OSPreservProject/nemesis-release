/*
*****************************************************************************
**                                                                          *
**  Copyright 1996, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      Inline assembler functions
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Useful inline functions
** 
** ENVIRONMENT: 
**
**      Intel NTSC
** 
** ID : $Id: inline.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef inline_h
#define inline_h

/* Invalidate a TLB entry */

static INLINE void invlpg(addr_t address) {
    char *x=address;
    __asm__ __volatile__ ("invlpg %0" : : "m" (*x) );
}

/* Invalidate all the TLB entries */
static INLINE void load_cr3(addr_t address) {
    __asm__ __volatile__ ("movl %0, %%cr3"
			  : /* no outputs */
			  : "r" (address) );
}

static INLINE void set_cr4_bit(uint32_t val) {
    __asm__ __volatile__ ("movl %%cr4, %%eax; orl %0, %%eax; movl %%eax, %%cr4"
			  : /* no outputs */
			  : "g" (val)
			  : "eax" );
}

static INLINE void set_cr0_bit(uint32_t val) {
    __asm__ __volatile__ ("movl %%cr0, %%eax; orl %0, %%eax; movl %%eax, %%cr0"
			  :
			  : "g" (val)
			  : "eax" );
}

static INLINE void disable_fpu(void) {
    __asm__ __volatile__
	("movl %%cr0, %%eax; orl $0x8, %%eax; movl %%eax, %%cr0"
	 :
	 :
	 : "eax" );
}

static INLINE void enable_fpu(void) {
    __asm__ __volatile__ ("clts");
}

static INLINE uint16_t fetch_ds(void) {
    uint16_t r;
    __asm__ __volatile__ ("movw %%ds, %0"
			  : "=q" (r) : );
    return r;
}

static INLINE uint16_t fetch_es(void) {
    uint16_t r;
    __asm__ __volatile__ ("movw %%es, %0"
			  : "=q" (r) : );
    return r;
}

static INLINE uint16_t fetch_fs(void) {
    uint16_t r;
    __asm__ __volatile__ ("movw %%fs, %0"
			  : "=q" (r) : );
    return r;
}

static INLINE uint16_t fetch_gs(void) {
    uint16_t r;
    __asm__ __volatile__ ("movw %%gs, %0"
			  : "=q" (r) : );
    return r;
}

#endif /* inline_h */
