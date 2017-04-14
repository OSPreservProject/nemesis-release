#ifndef _RELOCATION_H_
#define _RELOCATION_H_
/*
 * $Id: relocation_aout.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 *
 * $Log: relocation_aout.h,v $
 * Revision 1.2  1996/11/01 14:56:10  rjb17
 * rcs id fixes
 *
 * Revision 1.1  1996/09/23 12:48:08  smh22
 * see updates
 *
 * Revision 1.1  1995/06/15  16:11:53  prb12
 * Initial revision
 *
 * Revision 1.2  1993/07/01  12:46:29  rwe11
 * from prb12
 *  Loader support for LITERAL sections, and general tidying up.
 *
 * Revision 1.1  1993/02/17  12:08:25  eah
 * Initial revision
 *
 * Revision 1.1  1993/01/26  15:28:51  prb12
 * Initial revision
 *
 *
 *   The following are relocation macros for a.out files.  They were
 * originally part of gcc-ld.  They have been extracted so that they
 * may be used to write a relocating loader for use by the Wanda ProcSvr.
 *
 *   relocation_info: This must be typedef'd (or #define'd) to the type
 * of structure that is stored in the relocation info section of your
 * a.out files.  Often this is defined in the a.out.h for your system.
 *
 *   RELOC_ADDRESS (rval): Offset into the current section of the
 * <whatever> to be relocated.  *Must be an lvalue*.
 *
 *   RELOC_EXTERN_P (rval):  Is this relocation entry based on an
 * external symbol (1), or was it fully resolved upon entering the
 * loader (0) in which case some combination of the value in memory
 * (if RELOC_MEMORY_ADD_P) and the extra (if RELOC_ADD_EXTRA) contains
 * what the value of the relocation actually was.  *Must be an lvalue*.
 *
 *   RELOC_TYPE (rval): If this entry was fully resolved upon
 * entering the loader, what type should it be relocated as?
 *
 *   RELOC_SYMBOL (rval): If this entry was not fully resolved upon
 * entering the loader, what is the index of it's symbol in the symbol
 * table?  *Must be a lvalue*.
 *
 *   RELOC_MEMORY_ADD_P (rval): This should return true if the final
 * relocation value output here should be added to memory, or if the
 * section of memory described should simply be set to the relocation
 * value.
 *
 *   RELOC_ADD_EXTRA (rval): (Optional) This macro, if defined, gives
 * an extra value to be added to the relocation value based on the
 * individual relocation entry.  *Must be an lvalue if defined*.
 *
 *   RELOC_PCREL_P (rval): True if the relocation value described is
 * pc relative.
 *
 *   RELOC_VALUE_RIGHTSHIFT (rval): Number of bits right to shift the
 * final relocation value before putting it where it belongs.
 *
 *   RELOC_TARGET_SIZE (rval): log to the base 2 of the number of
 * bytes of size this relocation entry describes; 1 byte == 0; 2 bytes
 * == 1; 4 bytes == 2, and etc.  This is somewhat redundant (we could
 * do everything in terms of the bit operators below), but having this
 * macro could end up producing better code on machines without fancy
 * bit twiddling.  Also, it's easier to understand/code big/little
 * endian distinctions with this macro.
 *
 *   RELOC_TARGET_BITPOS (rval): The starting bit position within the
 * object described in RELOC_TARGET_SIZE in which the relocation value
 * will go.
 *
 *   RELOC_TARGET_BITSIZE (rval): How many bits are to be replaced
 * with the bits of the relocation value.  It may be assumed by the
 * code that the relocation value will fit into this many bits.  This
 * may be larger than RELOC_TARGET_SIZE if such be useful.
 *
 * (comment starting here by rjb17)
 *
 *   RELOC_MEMORY_SUB_P (rval): This was already defined by the time I got here
 * and only being used for the sequent. It causes the relocation value to have
 * the current value of the memory at the address to be subtracted from it.
 * There isn't a lot of point in this to my mind since the assembler might as
 * well have stored the negative value, with the exception that it gives
 * one more bits worth of significance (I assume that the direction bit for
 * the cpu is non-adjacent in the instruction word)
 *
 *   RELOC_MEMORY_NEG_P (rval): This is the one that I added for the ARM.
 * it allows relocation with the negative value of symbols. eg. ".word - _main"
 * this uses the r_neg bit which is an extension to the standard a.out.h
 * which acorn have added. The funny thing is that their own linker fails
 * completly when this bit is set, although the assembler uses it ok.
 * it works by subtracting the other way round, ie. the relocaion value is
 * subtracted from the current value at the address.
 *
 *
 * Also added for cross_arm only is the case that size is 3. By convention
 * this would indicate a double word wide relocation, but as it says above
 * this has never been implemented. It is used on the arm to refer to the
 * branch opcodes relcoation, cause they are 24bits wide and result shifted.
 * see the relevant code for more details.
 *
 * (end comment rjb17)
 */


/* Macros for ARM Cross linker */
#ifdef arm
#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)		((r)->r_extern)
#define RELOC_TYPE(r)		        ((r)->r_symbolnum)
#define RELOC_SYMBOL(r)		        ((r)->r_symbolnum)
#define RELOC_MEMORY_SUB_P(r)	        0
#define RELOC_MEMORY_ADD_P(r)	        1
#define RELOC_MEMORY_NEG_P(r)           ((r)->r_neg)
#undef RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)		((r)->r_pcrel)
#define RELOC_VALUE_RIGHTSHIFT(r)	(((r)->r_length==3)? 2 : 0)
#define RELOC_TARGET_SIZE(r)		((r)->r_length)
#define RELOC_TARGET_BITPOS(r)	        0
#define RELOC_TARGET_BITSIZE(r)	        (reloc_target_bitsize[(r)->r_length])
static int reloc_target_bitsize[] = {8,16,32,24};
#endif

/* relocation macros for mips */
#ifdef mips
#define REFLO_SIGN_BIT 0x00008000

#define relocation_info reloc

#define RELOC_ADDRESS(r)    ((r)->r_vaddr)
#define RELOC_EXTERN_P(r)   ((r)->r_extern)
#define RELOC_TYPE(r)	    ((r)->r_type)
#define RELOC_SYMBOL(r)	    ((r)->r_symndx)

#define RELOC_MEMORY_SUB_P(r)	   0
#define RELOC_MEMORY_ADD_P(r)	   1
#undef  RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)	   0

#define RELOC_VALUE_RIGHTSHIFT(r)  (reloc_target_rightshift[(r)->r_type])
#define RELOC_TARGET_SIZE(r)       (reloc_target_size[(r)->r_type])
#define RELOC_TARGET_BITPOS(r)     0
#define RELOC_TARGET_BITSIZE(r)    (reloc_target_bitsize[(r)->r_type])

/* 
 * Note that these are very dependent on the order of the relocation
 * types in the file reloc.h included from a.out.h. 
 */

/* ABS     RHALF    RWOR      JMPAD    REFHI     REFLO     GPREL     LIT */
static int reloc_target_rightshift[] = {
   0,      0,       0,         2,       16,        0,        0,        0
};
static int reloc_target_size[] = {
   2,      1,       2,         2,        1,        1,        1,        1
};
static int reloc_target_bitsize[] = {
   32,     16,     32,        26,       16,       16,       16,       16
};
#endif /* mips */


/* relocation macros for alpha */
#ifdef alpha
#define REFLO_SIGN_BIT 0x00008000

#define relocation_info reloc

#define RELOC_ADDRESS(r)    ((r)->r_vaddr)
#define RELOC_EXTERN_P(r)   ((r)->r_extern)
#define RELOC_TYPE(r)	    ((r)->r_type)
#define RELOC_SYMBOL(r)	    ((r)->r_symndx)

#define RELOC_MEMORY_SUB_P(r)	   0
#define RELOC_MEMORY_ADD_P(r)	   1
#undef  RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)	   0

#define RELOC_VALUE_RIGHTSHIFT(r)  (reloc_target_rightshift[(r)->r_type])
#define RELOC_TARGET_SIZE(r)       (reloc_target_size[(r)->r_type])
#define RELOC_TARGET_BITPOS(r)     0
#define RELOC_TARGET_BITSIZE(r)    (reloc_target_bitsize[(r)->r_type])

/* 
 * Note that these are very dependent on the order of the relocation
 * types in the file reloc.h included from a.out.h. 
 */

/* ABS   RLONG  RQUAD  GPREL32  LITERAL  LITUSE  GPDISP  BRADDR  HINT*/
static const int reloc_target_rightshift[] = {
   0,    0,     0,     0,       0,       0,      0,      2,      2
};
static const int reloc_target_size[] = {
   2,    2,     3,     2,       1,       0,      1,      2,      1
};
static const int reloc_target_bitsize[] = {
   32,   32,    64,    32,      16,      0,      16,     21,     14
};
#endif /* alpha */


/* Default macros */
#ifndef RELOC_ADDRESS
#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)		((r)->r_extern)
#define RELOC_TYPE(r)		        ((r)->r_symbolnum)
#define RELOC_SYMBOL(r)		        ((r)->r_symbolnum)
#define RELOC_MEMORY_SUB_P(r)	        0
#define RELOC_MEMORY_ADD_P(r)	        1
#undef RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)		((r)->r_pcrel)
#define RELOC_VALUE_RIGHTSHIFT(r)	0
#define RELOC_TARGET_SIZE(r)		((r)->r_length)
#define RELOC_TARGET_BITPOS(r)	        0
#define RELOC_TARGET_BITSIZE(r)	        32
#endif

#ifndef RELOC_MEMORY_NEG_P
#define RELOC_MEMORY_NEG_P(r)           0
#endif

#endif /* _RELOCATION_H_ */


