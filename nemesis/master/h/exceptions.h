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
**      exceptions.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Interface to exception handling system; setjmp version.
** 
** ENVIRONMENT: 
**
**      User space. Requires ExnSetjmp interface in the
** 	pervasives 
** 
** ID : $Id: exceptions.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _exceptions_h_
#define _exceptions_h_

/* 
 * This file defines macros to support an initial implementation of
 * the Nemesis exception handling facilities.
 */

#include <nemesis.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>

#include <Pervasives.ih>
#include <ExnSetjmp.ih>

#define NOCLOBBER volatile	/* XXX - protect local vars in TRY	*/

/*
 * Constants for the state of handling in the current TRY clause.
 * 
 * The state is "none" when no exception has been raised, "active" when
 * one has been raised but has not yet been caught by a CATCH clause, and
 * "handled" after the exception has been caught by some CATCH clause.
 */

typedef enum EXN_STATE_T {
    exn_active_c	= 0,
    exn_none_c		= 1,
    exn_handled_c	= 2,
    exn_popped_c	= 3
} exn_state_t;

/*
 * Define "routine" to determine if two exceptions match.
 */

/* effectively just strcmp(), but can't use the stack (at least, not
 * much) */
#define exn_matches(e1,e2) \
({						\
    char *s=e1, *d=e2;				\
    while(*s == *d && *s != 0 && *d != 0)	\
    {						\
	s++;					\
	d++;					\
    }						\
    (*s == 0 && *d == 0)? 1 : 0;		\
})


/*
 * Structure of a context block.
 *
 * A context block is allocated in the current stack frame for each
 * TRY clause.  These context blocks are linked to form a stack of
 * all current TRY blocks in the current thread.  Each context block
 * contains a jump buffer for use by setjmp and longjmp.  
 *
 */

typedef string_t	exn_name_t;

typedef struct _exn exn_context_t;
struct _exn {
  jmp_buf		jmp;		/* Jump buffer */
  exn_context_t	       *up;		/* Link up context block stack */
  exn_context_t	       *down;		/* Link down context block stack */
  exn_state_t		exn_state;	/* State of handling for this TRY */
  exn_name_t		cur_exception;	/* name of the current exception */
  addr_t		exn_args;
  string_t		filename;	/* which file raised it */
  string_t		funcname;	/*       func           */
  uint32_t		lineno;		/*       line           */
};

/*
 * Bindings to ExnSetjmp implementation
 */

#define exn_push_ctx(ctx)\
  ExnSetjmp$PushContext (((ExnSetjmp_clp) Pvs(exns)), (ctx))

#define exn_pop_ctx(ctx) \
  ExnSetjmp$PopContext (((ExnSetjmp_clp) Pvs(exns)), (ctx), \
			__FILE__, __LINE__, __FUNCTION__)

#define exn_rec_alloc(size)\
  ExnSetjmp$AllocArgs (((ExnSetjmp_clp) Pvs(exns)), (size))

#define exn_setjmp(bf)     (setjmp(bf))
#define exn_longjmp(bf, i) (longjmp(bf, i))

#define EXN_ARGP (exn_ctx.exn_args)

/*
 * Define "statement" for clients to use to raise an exception.
 */

#define RAISE(e,args) ExnSupport$Raise (Pvs(exns), (e), (args), __FILE__, __LINE__, __FUNCTION__)

#define RERAISE \
  ExnSupport$Raise (Pvs(exns), exn_ctx.cur_exception, exn_ctx.exn_args, __FILE__, __LINE__, __FUNCTION__)

/* 
 * Start a new TRY block, which may contain exception handlers
 * 
 *   Allocate a context block on the stack to remember the current
 *   exception. Push it on the context block stack.  Initialize
 *   this context block to indicate that no exception is active. Do a SETJMP
 *   to snapshot this environment (or return to it).  Then, start
 *   a block of statements to be guarded by the TRY clause.
 *   This block will be ended by one of the following: a CATCH, CATCH_ALL,
 *   or the ENDTRY macros.
 */
#define TRY \
    { \
	exn_context_t exn_ctx; \
	exn_context_t *exn_save; \
	word_t exn_base; \
	exn_push_ctx (&exn_ctx);\
        if (!exn_setjmp (exn_ctx.jmp)) {
/*		{ user's block of code goes here } 	*/

/* 
 * Define a CATCH(e) clause (or exception handler).
 *
 *   First, end the prior block.  Then, check if the current exception
 *   matches what the user is trying to catch with the CATCH clause.
 *   If there is a match, a variable is declared to support lexical
 *   nesting of RERAISE statements, and the state of the current
 *   exception is changed to "handled".
 *
 * CATCH is split into CATCHTOP and CATCHBOT so that handler-local
 * variables (eg. args) can be introduced.
 */

#define CATCHTOP(e) \
            } \
            else if (exn_matches(exn_ctx.cur_exception, (e))) { \

#define CATCHBOT \
		exn_ctx.exn_state = exn_handled_c;
/*		{ user's block of code goes here } 	*/

#define CATCH(e) CATCHTOP(e) CATCHBOT


/* 
 * Define a CATCH_ALL clause (or "catchall" handler).
 *
 *   First, end the prior block.  Then, unconditionally,
 *   let execution enter into the catchall code.  As with a normal
 *   catch, the state of the current exception is changed to "handled".
 */
#define CATCH_ALL \
            } \
            else { \
		exn_ctx.exn_state = exn_handled_c;
/*		{ user's block of code goes here } 	*/


/* 
 * Define a FINALLY clause
 *
 *   This "keyword" starts a FINALLY clause.  It must appear before
 *   an ENDTRY.  A FINALLY clause will be entered after normal exit
 *   of the TRY block, or if an unhandled exception tries to propagate
 *   out of the TRY block.  
 *
 *			** WARNING **
 *   You should *avoid* using FINALLY with CATCH clauses, that is, use it 
 *   only as TRY {} FINALLY {} ENDTRY.
 */
#define FINALLY   } \
	if (exn_ctx.exn_state == exn_none_c) \
	    exn_pop_ctx (&exn_ctx);\
	if (exn_ctx.exn_state == exn_active_c && exn_ctx.up) \
	    (exn_ctx.up)->down = NULL; /* cauterise */ \
	{
/*		{ user's block of code goes here } 	*/

/* 
 * End the whole TRY clause
 */
#define ENDTRY \
	}						\
    if (exn_ctx.exn_state == exn_active_c)		\
    {							\
	/* preserve trace on stack */			\
	/* XXX assumes stack grows downwards */		\
	exn_save = &exn_ctx;				\
	exn_base = (word_t)&exn_ctx;			\
	while(exn_save->down)				\
	{						\
	    exn_save = exn_save->down;			\
	    exn_base = MIN(exn_base, (word_t)exn_save);	\
	}						\
	exn_base = (word_t)SP() - exn_base;		\
	if ((sword_t)exn_base > 0)			\
	    alloca(exn_base);				\
    }							\
    if (exn_ctx.exn_state == exn_none_c			\
	    || exn_ctx.exn_state == exn_active_c) {	\
	exn_pop_ctx (&exn_ctx);				\
    } \
}


/* Because the normal ENDTRY macro uses alloca(), gcc refuses to
 * inline it.  Use this one instead, however, note that it cauterises
 * the backtrace at this level. */
#define ENDTRY_INLINE \
	}							\
    /* only def these 2 variables to stop "unused" warning */	\
    exn_save = NULL;						\
    exn_base = 0;						\
    if (exn_ctx.exn_state == exn_active_c && exn_ctx.up)	\
	(exn_ctx.up)->down = NULL;				\
    if (exn_ctx.exn_state == exn_none_c				\
	    || exn_ctx.exn_state == exn_active_c) {		\
	exn_pop_ctx (&exn_ctx);					\
    } \
}

/* 
 *   We define (with apologies for the syntax)
 * 
 *          USING (Gate, obj,
 *            code;
 *          )
 * 
 *   as a shorthand for
 * 
 *          Gate_Lock (obj);
 *          TRY
 *            code;
 *          FINALLY
 *            Gate_Unlock (obj);
 *          ENDTRY;
 *
 *   This may well break with very long "code" arguments, but I can't
 *   see a better solution, short of a special pre-processor.
 */

#define USING(_gate, _obj, _code) \
  _gate##_Lock (_obj); \
  TRY \
    { _code } \
  FINALLY    \
    _gate##_Unlock (_obj); \
  ENDTRY;

#define MUTEX_Lock(_obj)	MU_LOCK(_obj)
#define MUTEX_Unlock(_obj)	MU_RELEASE(_obj)

#endif /* __exceptions_h__ */
