/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.					            *
**                                                                          *
*****************************************************************************
**
** FACILITY:
**
**      mod/nemesis/rdwr/MemRd.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Readers on areas of memory.
** 
** ENVIRONMENT: 
**
**      Static, reentrant, anywhere
** 
** ID : $Id: MemRd.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
*/

#include <nemesis.h>
#include <exceptions.h>
#include <mutex.h>

#ifdef MEMRD_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif

static Rd_GetC_fn	  GetC_m;
static Rd_EOF_fn	  EOF_m;
static Rd_UnGetC_fn	  UnGetC_m;
static Rd_CharsReady_fn	  CharsReady_m;
static Rd_GetChars_fn	  GetChars_m;
static Rd_GetLine_fn	  GetLine_m;
static Rd_Seek_fn	  Seek_m;
static Rd_Close_fn	  Close_m;
static Rd_Length_fn	  Length_m;
static Rd_Index_fn	  Index_m;
static Rd_Intermittent_fn Intermittent_m;
static Rd_Seekable_fn	  Seekable_m;
static Rd_Lock_fn	  Lock_m;
static Rd_Unlock_fn	  Unlock_m;
static Rd_LGetC_fn	  LGetC_m;
static Rd_LEOF_fn	  LEOF_m;

static Rd_op rd_ops = { 
    GetC_m, 
    EOF_m, 
    UnGetC_m, 
    CharsReady_m,
    GetChars_m, 
    GetLine_m, 
    Seek_m, 
    Close_m,
    Length_m, 
    Index_m, 
    Intermittent_m,
    Seekable_m, 
    Lock_m, 
    Unlock_m,
    LGetC_m, 
    LEOF_m 
};

/* 
 * Data structures
 */
typedef struct MemRd_t MemRd_t;
struct MemRd_t {
    struct _Rd_cl cl;
    int8_t       *base;
    uint64_t	bytes;
    
    mutex_t	mu;
    
    /* LL >= mu */
    uint64_t	ptr;
};


Rd_clp CreateMemRd(addr_t base, uint64_t bytes, Heap_clp h)
{
    MemRd_t *st = Heap$Malloc (h, sizeof (*st));
    
    if (!st) RAISE_Heap$NoMemory();

    st->base  = base;
    st->bytes = bytes;
    st->ptr   = 0;

    MU_INIT (&st->mu);
    CL_INIT (st->cl, &rd_ops, st);

    return &st->cl;
}

/*********************************************************************
 *
 * Methods for File reader interface
 *
 *********************************************************************/

static int8_t GetC_m( Rd_cl *self ) 
{
    MemRd_t          *st = self->st;
    NOCLOBBER int8_t  res = 0;

    USING (MUTEX, &st->mu,
       {
	   if (st->ptr >= st->bytes)
	   {
	       DB(printf("MemRd: End of file!\n"));
	       RAISE_Rd$EndOfFile();
	   }
	   res = st->base [st->ptr++];
       });

    TRC (printf ("MemRd$GetC: returning %02x\n", res));

    return res;
}

static bool_t EOF_m( Rd_cl *self )
{
    MemRd_t          *st  = self->st;
    NOCLOBBER bool_t  res = False;

    USING (MUTEX, &st->mu, res = (st->ptr >= st->bytes); );
    TRC (printf ("MemRd$EOF: %x returns %d\n", self, res));
    return res;
}

static void UnGetC_m( Rd_cl *self )
{
    MemRd_t *st = self->st;

    USING (MUTEX, &st->mu,
       {
	   if ( st->ptr > 0 )
	       --(st->ptr);
	   else
	       RAISE_Rd$Failure (3);
       });
}

static uint64_t CharsReady_m( Rd_cl *self )
{
    MemRd_t            *st  = self->st;
    NOCLOBBER uint64_t  res = 0;
    USING (MUTEX, &st->mu, res = MAX (st->bytes - st->ptr, 1); );
    return res;
}

static uint64_t GetChars_m( Rd_cl *self, Rd_Buffer buf, uint64_t NOCLOBBER nb )
{
    MemRd_t *st = self->st;

    USING (MUTEX, &st->mu,
       {
	   if (nb > st->bytes - st->ptr)
	       nb = st->bytes - st->ptr;
    
	   memcpy (buf, st->base + st->ptr, nb);
    
	   st->ptr += nb;
       });
    return nb;
}

static uint64_t GetLine_m (Rd_cl *self, Rd_Buffer buf, uint64_t nb )
{
    MemRd_t            *st  = self->st;
    uint64_t NOCLOBBER res = 0;

    USING (MUTEX, &st->mu,
       {
	   while ((res != nb) &&
		  (res == 0 || buf[res-1] != '\n') &&
		  (st->ptr < st->bytes))
	   {
	       buf[res++] = st->base [st->ptr++];
	   }
       });
    return res;
}

static void Seek_m ( Rd_cl *self, uint64_t nb )
{
    MemRd_t *st = self->st;
    USING (MUTEX, &st->mu, st->ptr = MIN (nb, st->bytes); );
}

static void Close_m (  Rd_cl   *self )
{
    MemRd_t *st = self->st;
    MU_FREE (&st->mu);
    FREE(self->st);
}

static uint64_t Length_m ( Rd_cl   *self )
{
    MemRd_t *st = self->st;
    return st->bytes;
}

static uint64_t Index_m ( Rd_cl   *self )
{
    MemRd_t *st = self->st;
    return st->ptr;
}

static bool_t Intermittent_m ( Rd_cl   *self )
{
    return False;
}

static bool_t Seekable_m ( Rd_cl   *self )
{
    return True;
}
static void Lock_m ( Rd_cl   *self )
{
    MemRd_t *st = self->st;
    MUTEX_Lock (&st->mu);
}

static void Unlock_m (Rd_cl *self)
{
    MemRd_t *st = self->st;
    MUTEX_Unlock (&st->mu);
}

static int8_t LGetC_m (Rd_cl   *self)
{
    MemRd_t *st = self->st;
    if (st->ptr >= st->bytes)
    {
	DB (printf ("MemRd: End of file!\n"));
	RAISE_Rd$EndOfFile();
    }
    return st->base [st->ptr++];
}

static bool_t LEOF_m (Rd_cl *self)
{
    MemRd_t *st = self->st;
    return (st->ptr >= st->bytes);
}

