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
**      Priority Queues
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Priority queues are heaps (arrays of pointers to things)
**      sorted in increasing order of some property of the things
**	pointed to.
** 
** ENVIRONMENT: 
**
**      No locking. 
** 
** FEATURES:
** 
** 	XXX - no bounds checks to prevent overflows
** 
** ID : $Id: pqueues.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _pqueues_h_
#define _pqueues_h_

/* 
 * The macros work with two types to represent a priority queue
 * of "Foo"s.  The first is the priority queue itself, and should
 * be a struct with fields as follows:
 * 
 *   struct {
 *     word_t	n;
 *     Foo     *q [ SIZE ];
 *   };
 * 
 * Such a type can be declared with the PQ_STRUCT macro.
 * eg.
 *      typedef PQ_STRUCT (domain_t, NDOMAINS) dom_q;
 *      static dom_q run, wait, blocked;
 */

#define PQ_STRUCT(_T, _sz) struct { word_t n; _T *q[_sz]; }

/* 
 * The second type is "Foo", the type of the queue's elements.
 * No constraints are made on "Foo", other than that the macro
 * "word_t PQ_VAL(addr_t f)" should be defined to give the priority
 * of the "Foo" pointed to by "(Foo *) f".
 * 
 * eg. #define PQ_VAL(_dom) (((domain_t *)(_dom))->deadline)
 *
 * It can be helpful to record in a "Foo" its position in the
 * heap structure.  Redefine the "PQ_SET_IX(addr_t _foo, word_t _ix)"
 * macro if you want this to happen.
 * 
 * eg. #undef  PQ_SET_IX
 *     #define PQ_SET_IX(_dom, _ix) (((domain_t *)(_dom))->pq_index = (_ix))
 *
 */

#define PQ_SET_IX(_t, _ix)	/* skip */


/*
 * The following priority queue manipulation macros take arguments
 * "_pq" of type PQ_STRUCT(T, sz), and "_t" of type (T *).
 */

#define PQ_HEAD(_pq)		(_pq.q[1])
#define PQ_SIZE(_pq)		(_pq.n)
#define PQ_NTH(_pq,_n)		(_pq.q[_n])
#define PQ_EMPTY(_pq,_n)	(PQ_SIZE(_pq) == 0)

#define PQ_INIT(_pq)		(PQ_SIZE(_pq) = 0)

#define PQ_REMOVE(_pq)							\
{									\
  _pq.q[1] = _pq.q[(_pq.n)--];						\
  PQ_SET_IX (_pq.q[1], 1);						\
  PQ_DOWNHEAP (_pq, 1);							\
}

#define PQ_INSERT(_pq, _t)						\
{									\
  _pq.q[++(_pq.n)] = (_t);						\
  PQ_SET_IX (_pq.q[_pq.n], _pq.n);					\
  PQ_UPHEAP (_pq, (_pq.n));						\
}

/*
 * Delete an element from the middle of heap, given its index
 */
#define PQ_DEL(_pq, _ix)			\
{						\
  word_t  k1 = (_ix);				\
  addr_t  v1 = _pq.q[k1];			\
						\
  _pq.q[k1] = _pq.q[(_pq.n)--];			\
  PQ_SET_IX (_pq.q[k1], k1);			\
  if ( PQ_VAL(_pq.q[k1]) >= PQ_VAL(v1) )	\
  {						\
    PQ_DOWNHEAP (_pq, k1);			\
  }						\
  else						\
  {						\
    PQ_UPHEAP(_pq, k1);				\
  }						\
}


/*
 * Fix "up" heap condition
 */
#define PQ_UPHEAP(_pq, _ix)						\
{									\
  word_t  k = (_ix);							\
  addr_t  v = _pq.q[k];							\
									\
  while( (k >> 1) && PQ_VAL(_pq.q[ k>>1 ]) > PQ_VAL(v) ) {		\
    _pq.q[k] = _pq.q[ k>>1 ];						\
    PQ_SET_IX (_pq.q[k], k);						\
    k >>= 1;								\
  }									\
  _pq.q[k] = v;								\
  PQ_SET_IX (_pq.q[k], k);						\
}

/*
 * Fix "_down_" heap condition
 */
#define PQ_DOWNHEAP(_pq, _ix)						\
{									\
  word_t  k = (_ix);							\
  addr_t  v = _pq.q[k];							\
									\
  while( k+k <= (_pq.n) ) {						\
    word_t j = k+k;							\
  									\
    if ( j < _pq.n && PQ_VAL(_pq.q[j]) >= PQ_VAL(_pq.q[j+1]) )		\
      j++;								\
    if ( PQ_VAL(v) < PQ_VAL(_pq.q[j]) ) 				\
      break;								\
    _pq.q[k] = _pq.q[j]; 						\
    PQ_SET_IX (_pq.q[k], k);						\
    k = j;								\
  }									\
  _pq.q[k] = v;								\
  PQ_SET_IX (_pq.q[k], k);						\
}



#endif /* _pqueues_h_ */
