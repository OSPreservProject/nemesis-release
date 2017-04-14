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
**      Sequence macros
** 
** FUNCTIONAL DESCRIPTION:
** 
**      MIDDL SEQUENCEs, care and feeding of.  Loosely based on
**      CLU arrays.
** 
** ENVIRONMENT: 
**
**      Included in <nemesis.h> (needs ABS, FREE, REALLOC, memcpy and Heap).
**	No locking.
** 
** ID : $Id: sequence.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _sequence_h_
#define _sequence_h_

/* 
 * If "Foo: TYPE = SEQUENCE OF Bar", MIDDLC gives us:
 * 
 *   typedef struct {
 *     uint32_t  len;		-- "Bar"s of active data from "data"
 *     uint32_t  blen;		-- "Bar"s of allocated memory from "base"
 *     Bar      *data;
 *     Bar      *base;
 *   } Foo;
 *
 * Such a type can also be declared with the SEQ_STRUCT macro.
 * eg.
 *      typedef SEQ_STRUCT (Bar) Foo;
 *      static Foo bars;
 */

#define SEQ_STRUCT(_T)	struct { uint32_t len, blen; _T *data; _T *base; }

/*
 * In the subsequent explanations, suppose that:
 *   
 *   Foo       *foo;
 *   Bar       *bar;
 *   Bar	val;
 *   int32_t	n;
 *   uint32_t	i;
 *   Heap_clp	heap;
 */

/* 
 * The following are pretty obvious. Iterate over sequence "foo" with
 * 
 *     for (i = 0; i < SEQ_LEN(foo); i++)  Process (SEQ_ELEM (foo, i));
 * or
 *     for (bar = SEQ_START(foo); bar < SEQ_END(foo); bar++)  Process (bar);
 * 
 * Legal arguments "i" for SEQ_ELEM (foo, i) are [0..SEQ_LEN(foo) - 1].
 */

#define SEQ_LEN(_sqp)		((_sqp)->len)		/* can be an lvalue */
#define SEQ_ELEM(_sqp,_i)	((_sqp)->data[_i])	/* can be an lvalue */
#define SEQ_EMPTY(_sqp)		((_sqp)->len == 0)
#define SEQ_START(_sqp)		(& SEQ_ELEM ((_sqp), 0))
#define SEQ_END(_sqp)		(& SEQ_ELEM ((_sqp), SEQ_LEN (_sqp)))
#define SEQ_DSIZE(_sqp)		(sizeof (*((_sqp)->data)))

/* 
 * Redefine SEQ_FAIL() if you don't want exceptions raised.
 */

#define SEQ_FAIL()		RAISE_Heap$NoMemory()


/* 
 * SEQ_INIT (foo, n, heap) initialises "foo" with space for "n"
 * "Bar"s allocated from "heap".
 *
 * If "n" is strictly positive, "foo" will get a SEQ_LEN of "n".  Thus,
 * "foo" is NOT empty in this case.  It has "n" elements, each of
 * which is the all-zero "Bar" value.
 *
 * "n == 0" reserves space for a random number of elements (10), and
 * sets SEQ_LEN to 0. 
 */

#define SEQ_INIT(_sqp,_n,_h)						\
({									\
  (_sqp)->blen = (_n) ? (_n) : 10;					\
  (_sqp)->base = Heap$Calloc ((_h), (_sqp)->blen, SEQ_DSIZE (_sqp));	\
  if (!((_sqp)->base)) SEQ_FAIL();					\
  (_sqp)->len = (_n); (_sqp)->data = (_sqp)->base;			\
  (_sqp);								\
})


/* 
 * SEQ_CLEAR (foo) resets "foo" to length 0 without freeing
 * any memory allocated for "foo"'s elements.
 */

#define SEQ_CLEAR(_sq)					\
({__typeof__(_sq) _sqp = (_sq);	/* evaluate _sq once */	\
  SEQ_LEN(_sqp) = 0;					\
  (_sqp); })

/* 
 * SEQ_CLEARH (foo) is like SEQ_CLEAR (foo), except that "foo"'s data
 * pointer will be reset to its High end (ie., SEQ_END(foo)).  This
 * is useful when SEQ_ADDL operations are expected to follow (see below).
 */

#define SEQ_CLEARH(_sq)					\
({__typeof__(_sq) _sqp = (_sq);	/* evaluate _sq once */	\
  (_sqp)->data = SEQ_END(_sqp);				\
  SEQ_LEN(_sqp) = 0;					\
  (_sqp); })

/* 
 * foo = SEQ_NEW (Foo, n, heap) allocates a "Foo" sequence of "Bar"s
 * on "heap" and returns it initialised as for SEQ_INIT.
 */

#define SEQ_NEW(_ST,_n,_h)						\
({									\
  _ST *_res = (_ST *) Heap$Malloc ((_h), sizeof (_ST));			\
  if (!(_res)) SEQ_FAIL();						\
  SEQ_INIT (_res, (_n), (_h));						\
  _res;									\
})

/* 
 * SEQ_INIT_FIXED(sqp, elems, nelems) can be used to initialise a
 * sequence to a fixed set of elements. The sequence thus created is
 * read-only if elems is not a valid heap object 
 */

#define SEQ_INIT_FIXED(_sqp, _elems, _nelems) \
({\
    (_sqp)->data = (_sqp)->base = (_elems);\
    (_sqp)->len =  (_sqp)->blen = (_nelems);\
})

/* 
 * SEQ_FREE_ELEMS (foo) applies FREE to every non-NULL SEQ_ELEM(foo, i),
 * then sets SEQ_ELEM(foo, i) = NULL.  SEQ_LEN (foo) is unchanged.
 */

#define SEQ_FREE_ELEMS(_sqp)			\
({						\
  uint32_t _i;					\
  for (_i = 0; _i < (_sqp)->len; _i++)		\
    if ((_sqp)->data[_i])			\
    {						\
      FREE ((_sqp)->data[_i]);			\
      (_sqp)->data[_i] = NULL;			\
    }						\
  (_sqp);					\
})

/* 
 * SEQ_FREE_DATA (foo) frees the memory allocated for "foo"s
 * element array, but doesn't free "foo" itself.
 */

#define SEQ_FREE_DATA(_sqp)						\
({									\
  if ((_sqp)->base) FREE ((_sqp)->base);				\
  (_sqp)->len = (_sqp)->blen = 0;					\
  (_sqp)->data = (_sqp)->base = NULL;					\
  (_sqp);								\
})


/* 
 * SEQ_FREE (foo), on the other hand, does.
 */

#define SEQ_FREE(_sqp)							\
{									\
  SEQ_FREE_DATA (_sqp);							\
  FREE (_sqp);								\
}

/* 
 * SEQ_ADDH (foo, val) appends "val" to the end of the sequence "foo",
 * thus increasing SEQ_LEN(foo) by 1.
 * 
 * This may involve reallocating and copying the memory allocated for
 * "foo"s elements. If you know in advance how many SEQ_ADDH's you're
 * likely to do on "foo", you can avoid the copying by doing something
 * like
 * 
 *   foo = SEQ_CLEAR (SEQ_NEW (Foo, 25, heap));
 *
 * (The CLEAR is necessary: without it, the first ADDH would reallocate.)
 */

#define SEQ_ADDH(_sq,_val)					\
{ __typeof__(_sq) _sqp = (_sq);	/* evaluate _sq once */		\
  if (SEQ_END(_sqp) == &((_sqp)->base[(_sqp)->blen]))		\
  {								\
    int    _diff = (_sqp)->data - (_sqp)->base;			\
    (_sqp)->blen = 2 * (_sqp)->blen + 10;			\
    (_sqp)->base =						\
      REALLOC ((_sqp)->base, (_sqp)->blen * SEQ_DSIZE (_sqp));	\
    if (!((_sqp)->base)) SEQ_FAIL();				\
    (_sqp)->data = (_sqp)->base + _diff;			\
  }								\
  /* now SEQ_END < &base[blen] /\ base != NULL */		\
  *SEQ_END(_sqp) = (_val);					\
  ++SEQ_LEN(_sqp);						\
}


/* 
 * SEQ_REMH (foo) removes the last element, if present, from the
 * end of the sequence "foo", thus reducing SEQ_LEN(foo) by 1.
 */

#define SEQ_REMH(_sq)							\
{ __typeof__(_sq) _sqp = (_sq);	/* evaluate _sq once */			\
  if (SEQ_LEN (_sqp)) --SEQ_LEN(_sqp);					\
}


/* 
 * SEQ_ADDL (foo, val) prepends "val" to the start of the sequence "foo",
 * thus increasing SEQ_LEN(foo) by 1.
 * 
 * This may involve reallocating and copying the memory allocated for
 * "foo"s elements.  If you know in advance how many SEQ_ADDL's you're
 * likely to do on "foo", you can avoid the copying by doing something
 * like
 * 
 *   foo = SEQ_CLEARH (SEQ_NEW (Foo, 25, heap));
 *
 * (The CLEARH is necessary: without it, the first ADDL would reallocate.)
 */

#define SEQ_ADDL(_sq,_val)						\
{ __typeof__(_sq) _sqp = (_sq);	/* evaluate _sq once */			\
  if ((_sqp)->data == (_sqp)->base)					\
  {									\
    addr_t   _odat  = (addr_t) (_sqp)->data;				\
    addr_t   _obase = (addr_t) (_sqp)->base;				\
    uint32_t _olen  = (_sqp)->blen;					\
									\
    (_sqp)->blen    = 2*_olen + 10;					\
    (_sqp)->base    = Heap$Malloc (HEAP_OF ((_sqp)->base),		\
				   (_sqp)->blen * SEQ_DSIZE (_sqp));	\
    if (!((_sqp)->base)) SEQ_FAIL();					\
    (_sqp)->data    = (_sqp)->base + (_sqp)->blen - _olen;		\
    if (_odat)								\
      memcpy ((_sqp)->data, _odat,  SEQ_LEN(_sqp) * SEQ_DSIZE (_sqp));	\
    if (_obase)								\
      FREE (_obase);							\
  }									\
  /* now data > base != NULL */						\
  --((_sqp)->data);							\
  ++SEQ_LEN(_sqp);							\
  *SEQ_START(_sqp) = (_val);						\
}


/* 
 * SEQ_REML (foo) removes the first element, if present, from the
 * start of the sequence "foo", thus reducing SEQ_LEN(foo) by 1.
 */

#define SEQ_REML(_sq)							\
{ __typeof__(_sq) _sqp = (_sq);	/* evaluate _sq once */			\
  if (SEQ_LEN (_sqp)) { ++((_sqp)->data); --SEQ_LEN(_sqp); }		\
}


#endif /* _sequence_h_ */
