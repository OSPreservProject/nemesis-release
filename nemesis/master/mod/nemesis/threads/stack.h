/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**      mod/nemesis/threads/stack.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Stack related definitions for various architectures.
** 
** ENVIRONMENT: 
**
**      User-land;
**
*/

#include <autoconf/memsys.h>


#define SWIZZLE_STACK
/* 
** If SWIZZLE_STACK is defined, entry to RunNext from a previously 
** running thread also switches to stack pointer to the activation 
** stack. This is probably the correct behaviour (or at least more
** correct than not doing it).
** The macro NEWSP(_vpp) computes the correct stack pointer to 
** swizzle too - this is essentially the activation stack pointer
** offset big enough to allow a successful _longjmp() back to 
** ActivationGo() from ActivationF$Reactivate(). Do not meddle
** with its definition unless you know what you are doing.
** the magic number is found by inspecting the compiler output in 
** mod/nemesis/activations/ for ActivationGo in activations.c
** looking for the number of things on the stack
*/
#ifdef SWIZZLE_STACK
#warning swizzling stack is ON
#if   defined(__ALPHA__)
#define NEWSP(_vpp) 							\
    ((word_t *)((word_t )RW(_vpp)+DCBRW_W_STKSIZE-6*sizeof(word_t)))
#elif defined(__IX86__)
#define NEWSP(_vpp) 							\
    ((word_t *)((word_t )RW(_vpp)+DCBRW_W_STKSIZE-8*sizeof(word_t)))
#elif defined(__ARM__)
#define NEWSP(_vpp)                                                     \
    ((word_t *)((word_t )RW(_vpp)+DCBRW_W_STKSIZE-8*sizeof(word_t)))
#else
#error you must define a NEWSP macro for your architecure
#endif
#endif


/*
** CTXTSP() and JMPBUFSP() are simply macros to hide the various ways 
** in which architectures store their stack pointers within ctxts.
*/
#if defined(__ALPHA__) 
#define CTXTSP(_ctxt)   (void *)(_ctxt->gpreg[r_sp])
#define JMPBUFSP(_jbf)  (void *)(((jmp_buf_t *)_jbf)->r_sp)
#elif defined(__IX86__)
#define CTXTSP(_ctxt)   (void *)(_ctxt->esp)
#define JMPBUFSP(_jbf)  (void *)(((jmp_buf_t *)_jbf)->__sp)
#elif defined(__ARM__)
#define CTXTSP(_ctxt)   (void *)(_ctxt->reg[r_sp])
#define JMPBUFSP(_jbf)  (void *)(((jmp_buf_t *)_jbf)->reg[r_sp])
#else
#error You need to provide jmpbuf/context to stack pointer macros
#endif


/*
** STKTOP() returns the 'top' of the stack for a given architecture; 
** i.e. the start address of the first page of the stack which 
** will be used. 
*/
#if defined(__ALPHA__) || defined(__IX86__) || defined(__ARM__)
/* grows downward in memory */ 
#define STKTOP(_va, _sz) ((void *)(_va) + (_sz) - PAGE_SIZE)
#else
/* grows upward in memory */ 
#define STKTOP(_va, _sz) (_va)
#endif



