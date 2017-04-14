/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/stubs/IDC_CS_ConsoleWr.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**	Custom IDC stubs for console daemon
** 
** ENVIRONMENT: 
**
**      Client side in user domains, Server side in Nemesis domain.
** 
** ID : $Id: IDC_CS_ConsoleWr.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/


#include <kernel.h>

#include <nemesis.h>
#define Type_Any typed_ptr_t
#include <ShmTransport.h>
#include <IDCMacros.h>
#include <IDCServerStubs.ih>

#include <IDC.ih>

#include <ConsoleWr.ih>

static IDCServerStubs_Dispatch_fn	       Dispatch_m;
static IDCServerStubs_op IDCServerStubs_ms = { Dispatch_m };
static const IDCServerStubs_cl server_cl = { &IDCServerStubs_ms, NULL };

#define BUF_SIZE 256

extern const ConsoleWr_cl client_cl; /* forward reference */

static const IDCStubs_Rec _rec = {
  &server_cl, 
  NULL,
  {ConsoleWr_clp__code,  &client_cl},
  BADPTR,
  BUF_SIZE, BUF_SIZE
};

PUBLIC const Type_AnyI ConsoleWr__stubs = { IDCStubs_Info__code, &_rec };

static INLINE void InnerFlush (Client_t *st);
static INLINE void InnerLPutC (Client_t *st, int8_t c);

/*---------------------------- Inline buffer management ------------*/


#define UNSENT(bd) (bd->ptr - bd->base)

/* We hijack the "space" field of the IDC_BufferDesc to hold length(wr).
   Initially, it is BUF_SIZE. */

/* LL = st->mu */
static INLINE void
InnerLPutC (Client_t *st, int8_t c)
{
  IDC_BufferDesc bd = &st->conn->txbuf;

  *((int8_t *) bd->ptr)++ = c;
  bd->space++;

  if (UNSENT (bd) == BUF_SIZE - sizeof (uint32_t))
    InnerFlush (st);
}

/* LL = st->mu */
static INLINE void
InnerFlush (Client_t *st)
{
  IDC_BufferDesc txbd = &st->conn->txbuf;
  IDC_BufferDesc rxbd;
  uint32_t	*nchars  = ((uint32_t *) ((char *) txbd->base + BUF_SIZE)) - 1;
  uint32_t	 tmp_len = txbd->space;

  /* Put the number of chars to be transmitted in the last word
     of the buffer */
  *nchars = UNSENT(txbd);

  IDCClientBinding$SendCall     (&st->binding_cl, txbd);
  IDCClientBinding$ReceiveReply (&st->binding_cl, &rxbd, NULL);

  txbd->ptr   = txbd->base;
  txbd->space = tmp_len;
}


/*----------------------------------------- IDCServerStubs Entry Points ---*/


static void
Dispatch_m (IDCServerStubs_clp self)
{
  IDCServerStubs_State *st = self->st;
  Wr_clp		wr = st->service;
  IDC_BufferDesc 	bd;
  word_t		waste;
  
  while (IDCServerBinding$ReceiveCall (st->binding, &bd, &waste, NULL))
  {
    char	*s      = (char *)(bd->base);
    uint32_t	*nchars = ((uint32_t *) (s + BUF_SIZE)) - 1;

    Wr$PutChars (wr, s, *nchars);
    
    IDCServerBinding$AckReceive     (st->binding, bd);
    bd = IDCServerBinding$InitReply (st->binding);
    IDCServerBinding$SendReply      (st->binding, bd);
  }
}


/*----------------------------------------------------- Client Stubs ---*/

static Wr_PutC_fn      ConsoleWr_PutC_m;
static Wr_PutStr_fn    ConsoleWr_PutStr_m;
static Wr_PutChars_fn  ConsoleWr_PutChars_m;
static Wr_Seek_fn      ConsoleWr_Seek_m;
static Wr_Flush_fn     ConsoleWr_Flush_m;
static Wr_Close_fn     ConsoleWr_Close_m;
static Wr_Length_fn    ConsoleWr_LengthOrIndex_m;
static Wr_Index_fn     ConsoleWr_LengthOrIndex_m;
static Wr_Seekable_fn  ConsoleWr_Seekable_m;
static Wr_Buffered_fn  ConsoleWr_Buffered_m;
static Wr_Lock_fn      ConsoleWr_Lock_m;
static Wr_Unlock_fn    ConsoleWr_Unlock_m;
static Wr_LPutC_fn     ConsoleWr_LPutC_m;
static Wr_LPutStr_fn   ConsoleWr_LPutStr_m;
static Wr_LPutChars_fn ConsoleWr_LPutChars_m;
static Wr_LFlush_fn    ConsoleWr_LFlush_m;

static ConsoleWr_op client_ms = {
  ConsoleWr_PutC_m,
  ConsoleWr_PutStr_m,
  ConsoleWr_PutChars_m,
  ConsoleWr_Seek_m,
  ConsoleWr_Flush_m,
  ConsoleWr_Close_m,
  ConsoleWr_LengthOrIndex_m,
  ConsoleWr_LengthOrIndex_m,
  ConsoleWr_Seekable_m,
  ConsoleWr_Buffered_m,
  ConsoleWr_Lock_m,
  ConsoleWr_Unlock_m,
  ConsoleWr_LPutC_m,
  ConsoleWr_LPutStr_m,
  ConsoleWr_LPutChars_m,
  ConsoleWr_LFlush_m
};

static ConsoleWr_cl client_cl = { &client_ms, NULL };

/*
 * Strictly, these should all RAISE_Wr$Failure (0) if self is closed,
 * but I can't be bothered just now.
 */

static void
ConsoleWr_PutC_m (Wr_clp self, int8_t c)
{
  Client_t *st = self->st;
  USING (STATE_MU, st->conn, InnerLPutC (st, c););
}

static void
ConsoleWr_PutStr_m (Wr_clp self, string_t s)
{
  Client_t *st = self->st;
  USING (STATE_MU, st->conn, ConsoleWr_LPutStr_m (self, s););
}

static void
ConsoleWr_PutChars_m (Wr_clp self, int8_t *s, uint64_t nb)
{
  Client_t *st = self->st;
  USING (STATE_MU, st->conn, ConsoleWr_LPutChars_m (self, s, nb););
}

static void
ConsoleWr_Seek_m (Wr_clp self, uint64_t n)
{
  RAISE_Wr$Failure (1);
}

static void
ConsoleWr_Flush_m (Wr_clp self)
{
  Client_t *st = self->st;
  USING (STATE_MU, st->conn, InnerFlush (st););
}

static void
ConsoleWr_Close_m (Wr_clp self)
{
  /* XXX - use Client_t binding_cl to close down */
}

static uint64_t
ConsoleWr_LengthOrIndex_m (Wr_clp self)
{
  Client_t *st = self->st;
  NOCLOBBER uint32_t     res;

  USING (STATE_MU, st->conn, res = st->conn->txbuf.space;); /* see below */

  if (res <= BUF_SIZE)
    RAISE_Wr$Failure (0);

  return res - BUF_SIZE;
}

static bool_t
ConsoleWr_Seekable_m (Wr_clp self)
{
  return False;
}

static bool_t
ConsoleWr_Buffered_m (Wr_clp self)
{
  return True;
}

/* 
 * Unlocked operations
 */

/* LL < st->conn->mu */
static void
ConsoleWr_Lock_m (Wr_clp self)
{
  Client_t *st = self->st;
  MU_LOCK(&st->conn->mu);
}

/* LL = st->conn->mu */
static void
ConsoleWr_Unlock_m (Wr_clp self)
{
  Client_t *st = self->st;
  MU_RELEASE(&st->conn->mu);
}

/* LL = st->conn->mu */
static void
ConsoleWr_LPutC_m (Wr_clp self, int8_t c)
{
  InnerLPutC ( self->st, c);
}

/* LL = st->conn->mu */
static void
ConsoleWr_LPutStr_m (Wr_clp self, string_t s)
{
  while (*s)
    InnerLPutC ( self->st, *s++);

  InnerFlush ( self->st );
}

/* LL = st->conn->mu */
static void
ConsoleWr_LPutChars_m (Wr_clp self, int8_t *s, uint64_t nb)
{
  for (; nb; nb--)
    InnerLPutC ( self->st, *s++);

  InnerFlush (self->st );
}

/* LL = st->conn->mu */
static void
ConsoleWr_LFlush_m (Wr_clp self)
{
  InnerFlush ( self->st );
}


/* End of File */
