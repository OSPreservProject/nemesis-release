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
**      mod/nemesis/exception/ExnSetjmpMod.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Nemesis Exception Handling - with setjmp and longjmp
** 
** ENVIRONMENT: 
**
**      User-land;
** 
** ID : $Id: ExnSetjmpMod.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdlib.h>		/* abort */

#include <ExnSetjmp.ih>
#include <ExnSystem.ih>

#if DEBUG >= 1
#  define TRCD(_x) _x
#else
#  define TRCD(_x)
#endif

#if DEBUG >= 2
#  define TRC(_x) _x
#else
#  define TRC(_x)
#endif

/* 
 * Prototypes
 */

static ExnSupport_Raise_fn      Raise_m;

static ExnSetjmp_PushContext_fn PushContext_m;
static ExnSetjmp_PopContext_fn  PopContext_m;
static ExnSetjmp_AllocArgs_fn   AllocArgs_m;
static const struct _ExnSetjmp_op ExnSetjmp_ms = {
  Raise_m,
  PushContext_m,
  PopContext_m,
  AllocArgs_m
};

/*
 * Creator
 */
static ExnSystem_New_fn        New_m;
static ExnSystem_op mod_ms = { New_m };
static const ExnSystem_cl mod_cl = { &mod_ms, NULL };

CL_EXPORT (ExnSystem, ExnSystem, mod_cl);
CL_EXPORT_TO_PRIMAL(ExnSystem, ExnSystemCl, mod_cl);

/*---Module method to create a new exception system -------------------*/

static ExnSetjmp_clp New_m( ExnSystem_cl *self )
{
  /* We only need one pointer of state, which points to the first */
  /* context in the current chain. Thus we use the self->st pointer */
  /* directly. */

  struct _ExnSetjmp_cl *cl;

  if ( !( cl = Heap$Malloc(Pvs(heap), sizeof(*cl) ) ) ) {
    TRCD(eprintf("ExnSystem$New: failed to get memory.\n"));
    /* Not much point in raising an exception here. */
    return NULL;
  }
  
  cl->op = &ExnSetjmp_ms;
  cl->st = (addr_t)0;
  return cl;
}

/* ExnSetjmp methods */

static void INLINE
InternalRaise_m (ExnSupport_cl *self, ExnSupport_Id id, ExnSupport_Args args,
		 bool_t initialRaise,
		 string_t filename, uint32_t lineno, string_t funcname)
{
  exn_context_t   *ctx;
  exn_context_t   **handlers = (exn_context_t **)&(self->st);
  
  TRC (word_t *wargs = (word_t *) args;
	eprintf ("ExnSetjmpMod_Raise: vpp=%x ra=%x exc=%s args=%x\n",
		 Pvs(vp), RA(), id, args);
	if (args) 
	eprintf ("  args: %x %x %x %x\n",
		 wargs[0], wargs[1], wargs[2], wargs[3]);            
	)

  ctx      = *handlers;

#ifdef __arm
#  define r_ra reg[r_pc]
#endif
  TRC (
	for (; ctx; ctx = ctx->up)
	  eprintf ("ExnSetjmpMod_Raise: --> ctx=%x ra=%x\n",
		   ctx, ((jmp_buf_t *) &ctx->jmp[0])->r_ra);
	ctx = *handlers;
	)
#ifdef __arm
#  undef r_ra
#endif

  while (ctx && ctx->exn_state != exn_none_c)
  {
    eprintf("exn state=%d, can't happen?\n", ctx->exn_state);
    if (ctx->exn_args)
    {
      /* Another exception was in the middle of being processed when
       * this one happened. We have decided that we must pass higher
       * up the stack than that exception. Thus its argument
       * record is unreachable and must be freed.
       */
      FREE( ctx->exn_args );
      ctx->exn_args = BADPTR;
    }
    ctx = ctx->up;
  }
  
  if (!ctx)
  {
    TRCD(eprintf("ExnSetjmpMod_Raise: ra=%x unhandled exception `%s' arg %x\n",
	    RA(), id ? id : "(null)", args));
    abort();
    /* TODO: abort domain; threads' top-level fn should have a handler */
  }

  ctx->exn_state     = exn_active_c;
  ctx->cur_exception = id;
  ctx->exn_args      = args;
  ctx->filename	     = filename;
  ctx->lineno	     = lineno;
  ctx->funcname	     = funcname;
  if (initialRaise)
      ctx->down	     = NULL;	/* cauterise base of the backtrace */

  /* we've already longjmp'ed to this exn context, so pop it */
  *handlers = ctx->up;

  TRC (eprintf ("ExnSetjmpMod_Raise: longjmp to ctx %x\n", ctx));

  longjmp (ctx->jmp, 1);
}


static void
Raise_m (ExnSupport_cl *self, ExnSupport_Id id, ExnSupport_Args args,
	 string_t filename, uint32_t lineno, string_t funcname)
{
    /* do an internal Raise, marking this as the base of
     * a backtrace */
    InternalRaise_m(self, id, args, True, filename, lineno, funcname);
}


static void
PushContext_m (ExnSetjmp_clp self, ExnSetjmp_Context c)
{
  exn_context_t	       *ctx = (exn_context_t *) c;
  exn_context_t        **handlers = (exn_context_t **)&(self->st);

  TRC (eprintf ("ExnSetjmpMod_PushContext: c=%x handlers=%x\n",
	       c, handlers));

  ctx->exn_state = exn_none_c;
  ctx->up	 = *handlers;
  ctx->down	 = (void*)0x101;	/* non-NULL to mark as writable */
  ctx->exn_args  = NULL;

  /* only write the "down" pointer if we're not the top and
   * the bottom hasn't been cauterised by a RAISE or FINALLY */
  if (*handlers && (*handlers)->down)
      (*handlers)->down = ctx;
  *handlers      = ctx;
}


/* pre: ctx.state = none or active */

static void
PopContext_m (ExnSetjmp_clp self, ExnSetjmp_Context c,
	      string_t filename, uint32_t lineno, string_t funcname)
{
  exn_context_t   *ctx       = (exn_context_t *) c;
  exn_context_t   **handlers = (exn_context_t **)&(self->st);
  exn_state_t     prev_state = ctx->exn_state;
  
  TRC (eprintf ("ExnSetjmpMod_PopContext: c=%x\n", c));

  /* set state to popped so that FINALLY only pops once in normal case */
  
  ctx->exn_state = exn_popped_c;
  
  *handlers = ctx->up;
  
  if (prev_state == exn_active_c) {
    InternalRaise_m ((ExnSupport_clp) self, ctx->cur_exception, ctx->exn_args,
		     False, /* we're just propagating */
		     filename, lineno, funcname);
    /* NOTREACHED */
  } 
  else if (ctx->exn_args)
  {
    FREE( ctx->exn_args );
    ctx->exn_args = BADPTR;
  }
}

static Heap_Ptr
AllocArgs_m (ExnSetjmp_clp self, Heap_Size size)
{
  word_t   ra  = RA();
  Heap_Ptr res = Heap$Malloc (Pvs(heap), size);

  TRC (eprintf ("ExnSetjmpMod_AllocArgs: size=%x res=%x\n",
	       size, res));

  if (res)
    return res;
  else
  {
    /* Could be eprintf / heap etc that was raising the exception in
     * the first place. Recursion see recursion. This is why fawn had
     * a (potentially) seperate heap for exceptions. Just bail. XXX
     */
    TRC(eprintf ("ExnSetjmpMod_AllocArgs: malloc failed; ra=%x\n", ra));
    abort();
    /* TODO: might get away with raising Heap.NoMemory, which has no
       args itself.  Or just abort. */
  }
}

/* 
 * End of mod/nemesis/exception/ExnSetjmpMod.c
 */

