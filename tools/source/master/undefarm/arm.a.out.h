/*
 *	arm.a.out.h
 *	-----------
 *
 * $Id: arm.a.out.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1994 University of Cambridge Computer Laboratory
 */
/*
 * Copyright (c) 1988 Acorn Computers Ltd., Cambridge, England
 */
/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)a.out.h	5.1 (Berkeley) 5/30/85
 */

#ifndef __arm_a_out_h__
#define __arm_a_out_h__

/*
 * For cross-compilation it seems to be 8K. On Riscix its 32K
 */

#define PAGESIZE 8192

/*
 * Definitions of the a.out header
 */

struct exec {
    int			a_magic;	/* magic number */
    unsigned int	a_text;		/* size of text segment */
    unsigned int	a_data;		/* size of initialized data */
    unsigned int	a_bss;		/* size of uninitialized data */
    unsigned int	a_syms;		/* size of symbol table */
    unsigned int	a_entry;	/* entry point */
    unsigned int	a_trsize;	/* size of text relocation */
    unsigned int	a_drsize;	/* size of data relocation */
};

/* Basic magic numbers */

#define OMAGIC	0407		/* old impure format */
#define NMAGIC	0410		/* read-only text */
#define ZMAGIC	0413		/* demand load format (pure sharable text) */

/*
 * Macros which take exec structures as arguments and tell whether
 * the file has a reasonable magic number or offsets to text|symbols|strings.
 */
#if 0
#define	N_BADMAG(x) \
   ( ( ((x).a_magic & ~007200) != ZMAGIC) && \
     ( ((x).a_magic & ~006000) != OMAGIC) && \
     (  (x).a_magic != NMAGIC) \
   )

#define	N_TXTOFF(x) \
   (	(x).a_magic == OMAGIC ? sizeof (struct exec) : \
	(((x).a_magic & ~007200) == ZMAGIC ? PAGESIZE : sizeof (struct exec_header)) \
   )
#endif

#define N_BADMAG(x) \
   (  ((x).a_magic != ZMAGIC) \
    && ((x).a_magic != NMAGIC) \
    && ((x).a_magic != OMAGIC) )

#define N_TXTOFF(x) \
   (	(x).a_magic == ZMAGIC ? PAGESIZE : sizeof (struct exec)  )

#define N_SYMOFF(x) \
	(N_TXTOFF(x) + (x).a_text+(x).a_data + (x).a_trsize+(x).a_drsize)
#define	N_STROFF(x) \
	(N_SYMOFF(x) + (x).a_syms)

/*
 * Format of a relocation datum.
 */
struct relocation_info {
    int			r_address;	/* address which is relocated */
    unsigned int	r_symbolnum:24,	/* local symbol ordinal */
 			r_pcrel:1, 	/* was relocated pc relative already */
			r_length:2,	/* 0=byte, 1=half, 2=word 3=jump */
			r_extern:1,	/* does not include value of sym
					   referenced */
			r_neg:1,	/* -ve relocation */
    			r_spare:3;	/* nothing, yet */
};
#define RELINFOSZ 8

/*
 * Format of a symbol table entry; this file is included by <a.out.h>
 * and should be used if you aren't interested the a.out header
 * or relocation information.
 */

struct nlist {
    union {
	char		*n_name;	/* for use when in-core */
	int		n_strx;		/* index into file string table */
    } n_un;
    unsigned char	n_type;		/* type flag, N_TEXT etc; see below */
    char		n_other;	/* unused */
    short		n_desc;		/* see <stab.h> */
    unsigned int	n_value;	/* value of this symbol
					   (or sdb offset) */
};

#define NLISTSZ 12
#define	n_hash	n_desc		/* used internally by ld */

/*
 * Simple values for n_type.
 */

#define	N_UNDF	0x0		/* undefined */
#define	N_ABS	0x2		/* absolute */
#define	N_TEXT	0x4		/* text */
#define	N_DATA	0x6		/* data */
#define	N_BSS	0x8		/* bss */
#define	N_COMM	0x12		/* common (internal to ld) */
#define	N_FN	0x1f		/* file name symbol */

#define	N_EXT	01		/* external bit, or'ed in */
#define	N_TYPE	0x1e		/* mask for all the type bits */

/*
 * Sdb entries have some of the N_STAB bits set.
 * These are given in <stab.h>
 */

#define	N_STAB	0xe0		/* if any of these bits set, a SDB entry */

/*
 * Format for namelist values.
 */
#define	N_FORMAT	"%08x"

#endif/*__a_out_h*/

/* EOF a.out.h */
