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
**      What is it?
** 
** FUNCTIONAL DESCRIPTION:
** 
**      What does it do?
** 
** ENVIRONMENT: 
**
**      Where does it do it?
** 
** ID : $Id: processor.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef processor_h
#define processor_h

#define PROCESSOR_FEATURE_FPU    0x0001
#define PROCESSOR_FEATURE_VME    0x0002
#define PROCESSOR_FEATURE_DE     0x0004
#define PROCESSOR_FEATURE_PSE    0x0008
#define PROCESSOR_FEATURE_TSC    0x0010
#define PROCESSOR_FEATURE_MSR    0x0020
#define PROCESSOR_FEATURE_PAE    0x0040
#define PROCESSOR_FEATURE_MCE    0x0080
#define PROCESSOR_FEATURE_CX8    0x0100
#define PROCESSOR_FEATURE_APIC   0x0200
#define PROCESSOR_FEATURE_MTRR   0x1000
#define PROCESSOR_FEATURE_PGE    0x2000
#define PROCESSOR_FEATURE_MCA    0x4000
#define PROCESSOR_FEATURE_CMOV   0x8000

#ifdef __LANGUAGE_C

typedef enum {
    processor_type_unknown,
    processor_type_486,
    processor_type_pentium,
    processor_type_ppro
} processor_type_t;

typedef struct _processor {
    processor_type_t type;
    uint8_t          vendor[16];
    unsigned int     stepping : 4;
    unsigned int     model : 4;
    unsigned int     family: 4;
    
    uint32_t         features;
} processor_t;

#endif /* __LANGUAGE_C */

#define CR0_PE 0x00000001
#define CR0_MP 0x00000002
#define CR0_EM 0x00000004
#define CR0_TS 0x00000008
#define CR0_ET 0x00000010
#define CR0_NE 0x00000020
#define CR0_WP 0x00010000
#define CR0_AM 0x00040000
#define CR0_NW 0x20000000
#define CR0_CD 0x40000000
#define CR0_PG 0x80000000

#define CR3_PWT 0x04
#define CR3_PCD 0x10

#define CR4_VME 0x001
#define CR4_PVI 0x002
#define CR4_TSD 0x004
#define CR4_DE  0x008
#define CR4_PSE 0x010
#define CR4_PAE 0x020
#define CR4_MCE 0x040
#define CR4_PGE 0x080
#define CR4_PCE 0x100

#define EFLAG_CF  0x00000001
#define EFLAG_PF  0x00000004
#define EFLAG_AF  0x00000010
#define EFLAG_ZF  0x00000040
#define EFLAG_SF  0x00000080
#define EFLAG_TF  0x00000100
#define EFLAG_IF  0x00000200
#define EFLAG_DF  0x00000400
#define EFLAG_OF  0x00000800
#define EFLAG_NT  0x00004000
#define EFLAG_RF  0x00010000
#define EFLAG_VM  0x00020000
#define EFLAG_AC  0x00040000
#define EFLAG_VIF 0x00080000
#define EFLAG_VIP 0x00100000
#define EFLAG_ID  0x00200000

#endif /* processor_h */
