/*
*****************************************************************************
**                                                                          *
**  Copyright 1994, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      jmp_buf.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Jump buffer: Intel version
** 
** ID : $Id: jmp_buf.h 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/

#ifndef _jmp_buf_h_
#define _jmp_buf_h_

/* 
 * A full context_t is far too big for a jmp_buf, which need only
 * contain callee-saves.  For ANSI, need jmp_buf to be an array, as
 * in setjmp.h; here we define a struct jmp_buf_t.
 */

#ifdef __LANGUAGE_C__

/* XXX - floating point */

typedef struct jmp_buf_t jmp_buf_t;
struct jmp_buf_t {
    long int __bx, __si, __di;
    long int __bp, __sp, __pc; /* XXX pointers? */
    long int pvs;
    long int jbflags; /* Set to 1 if fpreg contains valid info */
    long int fpreg[27];
};


/*
** User-level thread schedulers like to use jmp_buf's to store 
** useful things such as pc, sp, pvs, ra, and a number of args.  
** To keep the code as simple and generic-looking as possible, every
** arch. must provide an init_jb() routine which does the right thing.
** This includes setting of misc. flags if relevant.
**
** XXX The format used here _must_ coincide with what the arch-specific
**     _thead() stub (pointed to by ra) expects.
*/
static void INLINE init_jb(jmp_buf_t *jb, addr_t pc, 
			   addr_t ra, word_t sp, word_t pvs,
			   word_t a0, word_t a1, word_t a2)
{
    /* on ix86, we store the pc and args in the first four slots */
    jb->__bx = (word_t)pc;
    jb->__si = a0;
    jb->__di = a1;
    jb->__bp = a2;
    
    /* we set pvs to point to the jmp_buf itself: used by _thead() */
    jb->pvs = (word_t)jb;
    
    /* ra is set to the _thread() routine, with sp being standard */ 
    jb->__pc = (word_t)ra; 
    jb->__sp = sp;

    /* We also indicate that the jmp_buf doesn't contain fp state */
    jb->jbflags=0;

    /* that's it! */
    return;
}



#endif /* __LANGUAGE_C__ */

/* XXX- JB_LEN referenced in setjmp.h, so it better be defined */
#define JB_LEN     (35)

#endif /* _jmp_buf_h_ */
