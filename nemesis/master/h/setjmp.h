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
**      setjmp.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      setjmp and longjmp.  Includes target-dependent jmp_buf.h
**      N.B. these do not yet save/restore floating point registers.
** 
** ID : $Id: setjmp.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _setjmp_h_
#define _setjmp_h_

#include <jmp_buf.h>

typedef word_t		jmp_buf [JB_LEN];

extern int	setjmp   (jmp_buf env);
extern NORETURN (longjmp  (jmp_buf env, int val));
extern NORETURN (_longjmp (jmp_buf env)); /* returns non-zero from setjmp */

#endif /* _setjmp_h_ */
