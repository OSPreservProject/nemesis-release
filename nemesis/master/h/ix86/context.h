/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      h/ix86/context.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Structure of saved machine context for Intel ix86
** 
** ENVIRONMENT: 
**
**      Used inside Domain implementation and NTSC as a
**      part of DCB data structure. Where registers are saved.
** 
** ID : $Id: context.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _context_h_
#define _context_h_

#ifdef __LANGUAGE_C

/*
 * Names for registers
 */
typedef enum _reg {
    r_eax, r_ebx, r_ecx, r_edx, r_esi, r_edi, r_ebp
} reg_t;


/*
 * Format of saved machine context
 */
/* NB pervasives isn't really a register, but it is saved from and
   restored to dcb_ro on every save/restore. This is becaue threads
   can change the value of the pervasives pointer at any time, and
   sometimes threads are preempted and restored with RFAR rather than
   with setjmp/longjmp */

typedef struct {
    uint32_t	gpreg[7];		/* Machine registers		*/
    uint32_t    eip;                    /* PC                           */
    uint32_t    eflag;                  /* Flag register                */
    uint32_t    esp;                    /* Stack pointer                */
    uint32_t    pvs;                    /* Pervasives pointer           */
    uint32_t    cxflag;                 /* Context flags                */
    uint32_t    fpreg[27];              /* Floating point registers     */
    uint32_t    pad[25];                /* Rhubarb                      */
} context_t;

/* The context free list in dcb_rw_t is chained through gpreg[0].
   It is maintained by VPMacros.h and should be initialised at
   domain creation. 

   This macro must be defined as something of meaning in all instances
   of context.h */

#define CX_FREELINK gpreg[0]

#endif /* __LANGUAGE_C */

/* If CX_SIZE changes, change the calculation for slot addr in idt.S	*/

#define	CX_SIZE		((64)*4)	/* Bytes in a context		*/
#define	CX_NREGS	((64))	        /* Words in a context		*/

#define CX_EAX          0               /* EAX                          */
#define CX_EBX          1               /* EBX                          */
#define CX_ECX          2               /* ECX                          */
#define CX_EDX          3               /* EDX                          */
#define CX_ESI          4               /* ESI                          */
#define CX_EDI          5               /* EDI                          */
#define CX_EBP          6               /* EBP                          */
#define CX_EIP          7               /* EIP: instruction pointer     */
#define CX_EFLAG        8               /* EFLAG: processor status      */
#define CX_ESP          9               /* ESP: stack pointer           */
#define CX_PVS         10               /* Pervasives, from DCB         */
#define CX_CXFLAG      11               /* Flags for this context       */
#define CX_FPREG       12               /* Floating point registers     */

/* Context flags: information about this context */
#define CXFLAG_FPVALID 1                /* Set if fp context is valid   */

#endif /* _context_h_ */
