/*  -*- Mode: C;  -*-
 * File: memcopy.h
 * Author: James Hall (jch1003@cl.cam.ac.uk)
 * Copyright (C) University of Cambridge Computer Laboratory, 1994
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ** PACKAGE: Nemesis C Library
 **
 ** FUNCTION: Definitions for memory copy functions.  Generic C version.
 **
 ** HISTORY:
 ** Created: Tue Apr 19 14:32:20 1994 (jch1003)
 **~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */


#ifndef __MEMCOPY_H__
#define  __MEMCOPY_H__

#include <string.h>


/* The strategy of the memory functions is:

     1. Copy bytes until the destination pointer is aligned.

     2. Copy words in unrolled loops.  If the source and destination
     are not aligned in the same way, use word memory operations,
     but shift and merge two read words before writing.

     3. Copy the few remaining bytes.

   This is fast on processors that have at least 10 registers for
   allocation by GCC, and that can access memory at reg+const in one
   instruction.

   I made an "exhaustive" test of this memmove when I wrote it (him that is,
   not me, James), exhaustive in the sense that I tried all alignment and 
   length combinations, with and without overlap.  */


/* The macros defined in this file are:

   BYTE_COPY_FWD(dst_beg_ptr, src_beg_ptr, nbytes_to_copy)

   BYTE_COPY_BWD(dst_end_ptr, src_end_ptr, nbytes_to_copy)

   WORD_COPY_FWD(dst_beg_ptr, src_beg_ptr, nbytes_remaining, nbytes_to_copy)

   WORD_COPY_BWD(dst_end_ptr, src_end_ptr, nbytes_remaining, nbytes_to_copy)

   MERGE(old_word, sh_1, new_word, sh_2)
     [I fail to understand.  I feel stupid.  --roland]
*/


/* Type to use for aligned memory operations.
   This should normally be the biggest type supported by a single load
   and store.  */
#define	op_t	word_t
#define OPSIZ	(sizeof(op_t))

/* Type to use for unaligned operations.  */
typedef unsigned char byte;

/* Optimal type for storing bytes in registers.  */
#define	reg_char	char

#ifdef LITTLE_ENDIAN
#define MERGE(w0, sh_1, w1, sh_2) (((w0) >> (sh_1)) | ((w1) << (sh_2)))
#endif
#ifdef BIG_ENDIAN
#define MERGE(w0, sh_1, w1, sh_2) (((w0) << (sh_1)) | ((w1) >> (sh_2)))
#endif

/* Copy exactly NBYTES bytes from SRC_BP to DST_BP,
   without any assumptions about alignment of the pointers.  */
#define BYTE_COPY_FWD(dst_bp, src_bp, nbytes)				      \
  do									      \
    {									      \
      size_t __nbytes = (nbytes);					      \
      while (__nbytes > 0)						      \
	{								      \
	  byte __x = ((byte *) src_bp)[0];				      \
	  src_bp += 1;							      \
	  __nbytes -= 1;						      \
	  ((byte *) dst_bp)[0] = __x;					      \
	  dst_bp += 1;							      \
	}								      \
    } while (0)

/* Copy exactly NBYTES_TO_COPY bytes from SRC_END_PTR to DST_END_PTR,
   beginning at the bytes right before the pointers and continuing towards
   smaller addresses.  Don't assume anything about alignment of the
   pointers.  */
#define BYTE_COPY_BWD(dst_ep, src_ep, nbytes)				      \
  do									      \
    {									      \
      size_t __nbytes = (nbytes);					      \
      while (__nbytes > 0)						      \
	{								      \
	  byte __x;							      \
	  src_ep -= 1;							      \
	  __x = ((byte *) src_ep)[0];					      \
	  dst_ep -= 1;							      \
	  __nbytes -= 1;						      \
	  ((byte *) dst_ep)[0] = __x;					      \
	}								      \
    } while (0)

/* Copy *up to* NBYTES bytes from SRC_BP to DST_BP, with
   the assumption that DST_BP is aligned on an OPSIZ multiple.  If
   not all bytes could be easily copied, store remaining number of bytes
   in NBYTES_LEFT, otherwise store 0.  */
extern void _wordcopy_fwd_aligned(long int dstp,
				  long int srcp, size_t len);
extern void _wordcopy_fwd_dest_aligned(long int dstp,
				       long int srcp, size_t len);
#define WORD_COPY_FWD(dst_bp, src_bp, nbytes_left, nbytes)		      \
  do									      \
    {									      \
      if (src_bp % OPSIZ == 0)						      \
	_wordcopy_fwd_aligned (dst_bp, src_bp, (nbytes) / OPSIZ);	      \
      else								      \
	_wordcopy_fwd_dest_aligned (dst_bp, src_bp, (nbytes) / OPSIZ);	      \
      src_bp += (nbytes) & -OPSIZ;					      \
      dst_bp += (nbytes) & -OPSIZ;					      \
      (nbytes_left) = (nbytes) % OPSIZ;					      \
    } while (0)

/* Copy *up to* NBYTES_TO_COPY bytes from SRC_END_PTR to DST_END_PTR,
   beginning at the words (of type op_t) right before the pointers and
   continuing towards smaller addresses.  May take advantage of that
   DST_END_PTR is aligned on an OPSIZ multiple.  If not all bytes could be
   easily copied, store remaining number of bytes in NBYTES_REMAINING,
   otherwise store 0.  */
extern void _wordcopy_bwd_aligned(long int dstp,
				  long int srcp, size_t len);
extern void _wordcopy_bwd_dest_aligned(long int dstp,
				       long int srcp, size_t len);
#define WORD_COPY_BWD(dst_ep, src_ep, nbytes_left, nbytes)		      \
  do									      \
    {		    						      \
      if (src_ep % OPSIZ == 0)						      \
	_wordcopy_bwd_aligned (dst_ep, src_ep, (nbytes) / OPSIZ);	      \
      else								      \
	_wordcopy_bwd_dest_aligned (dst_ep, src_ep, (nbytes) / OPSIZ);	      \
      src_ep -= (nbytes) & -OPSIZ;					      \
      dst_ep -= (nbytes) & -OPSIZ;					      \
      (nbytes_left) = (nbytes) % OPSIZ;					      \
    } while (0)


/* Threshold value for when to enter the unrolled loops.  */
#define	OP_T_THRES	16


#endif /*  __MEMCOPY_H__  */


/*
 * end memcopy.h
 */















