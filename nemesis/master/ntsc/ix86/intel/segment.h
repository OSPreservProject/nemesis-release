/*
 *	ntsc/ix86/segment.h
 *	-------------------
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory.
 * This is part of Nemesis; consult your contract for terms and conditions.
 *
 * This header contains definitions for the various segments in the GDT.
 * We may as well define the well-known address for the pervasives here too.
 *
 * $Id: segment.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
 */

#ifndef ASM_SEGMENT_H
#define ASM_SEGMENT_H

#define KERNEL_TS 0x08
#define KERNEL_CS 0x10
#define KERNEL_DS 0x18
#define USER_CS   0x23
#define USER_DS   0x2b
#define PRIV_CS   0x32
#define PRIV_DS   0x3a

#define DS(x) ((x)==PRIV_CS ? PRIV_DS : USER_DS)

#endif /* ASM_SEGMENT_H */
