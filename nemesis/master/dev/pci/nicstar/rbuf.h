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
**      Nemesis includes
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Structures and macros for RBuf manipulation
** 
** ENVIRONMENT: 
**
**      Anywhere
** 
** ID : $Id: rbuf.h 1.1 Thu, 18 Feb 1999 15:09:21 +0000 dr10009 $
** 
**
*/

#ifndef _RBUF_H_
#define _RBUF_H_

#include <IO.ih>
#include <sequence.h>

/* These should come from some sort of config file */
#define MAX_IORECS      4	/* Max frags per pkt */

/* RBuf header */
struct r_hdr {
  struct   rbuf *rh_next;	/* next rbuf in queue */
  IO_clp   rh_io;		/* IO from which Recs came */
  uint64_t rh_st;		/* Additional state */
  uint32_t rh_len;		/* amount of data in this rbuf */
  uint16_t rh_type;		/* type of data in this rbuf */
  uint16_t rh_flags;		/* flags; see below */
};

/* RBuf header flags */
#define R_EXT           0x0001  /* has associated external storage */
#define R_PKTHDR        0x0002  /* start of record */
#define R_EOR           0x0004  /* end of record */

/* RBuf header types */
#define RT_FREE         0       /* IORecs not valid  */
#define RT_EMPTY        1	/* IORecs valid, buf empty */
#define RT_FULL         3	/* IORecs valid, buf full */

/* rbuf structure */
struct rbuf {
        struct  r_hdr r_hdr;
	struct {
	  uint32_t  rd_nrecs; 
	  word_t    rd_value;
	  IO_Rec    rd_recs [MAX_IORECS];
	} r_data;
};

/* Abbreviations for RBuf fields */
#define r_next          r_hdr.rh_next
#define r_io            r_hdr.rh_io
#define r_st            r_hdr.rh_st
#define r_len           r_hdr.rh_len
#define r_type          r_hdr.rh_type
#define r_flags         r_hdr.rh_flags
#define r_nrecs         r_data.rd_nrecs
#define r_value         r_data.rd_value
#define r_recs          r_data.rd_recs

/*
 * XXX Note: all these macros assume single produce/consumer so that
 * the increments of head/tail work OK.
 * (Hence no locking)
 *
 * RBuf allocation/deallocation macros:
 *
 *      struct rbuf * RALLOC()
 * Try to allocate and initialise an RBuf.
 *
 *      RGET(struct rbuf *r, IO_clp, int how)
 * Try to fill RBuf "r" with IO_Recs from IO stream "io".  
 * If "how" is R_WAIT then block attempting to get IO_Recs.
 *
 *      RPUT(struct rbuf *r)
 * Send the IO_Recs from "r" back down its associated IO stream.
 *
 */

#define R_DONTWAIT      0
#define R_WAIT          1

#define RALLOC()						\
({								\
  struct rbuf *_r;						\
  _r = (struct rbuf *)Heap$Malloc(Pvs(heap), sizeof(struct rbuf));\
  if (_r) {							\
    _r->r_next  = NULL;						\
    _r->r_io    = NULL;						\
    _r->r_len   = 0;						\
    _r->r_type  = RT_FREE;					\
    _r->r_flags = 0;						\
  }								\
  _r;								\
})

#define RGET(r, io, how)				\
({							\
  bool_t ok = True;					\
  (r)->r_io    = (io);					\
   ok = IO$GetPkt ((r)->r_io, MAX_IORECS, (r)->r_recs,	\
		   ((how) == R_WAIT) ? FOREVER : 0,	\
		   &(r)->r_nrecs, &(r)->r_value);	\
  (ok);							\
})

/*
 * RADJ(struct rbuf *r, uint32_t len)
 */
#define RADJ(r, bytes)							   \
{									   \
  uint32_t left = (bytes);						   \
  IO_Rec *rec = (r)->r_recs;						   \
  int i = 0;								   \
									   \
  while (i < ((r)->r_nrecs)-1 && left > rec->len) {			   \
    left -= rec->len; rec++; i++;					   \
  }									   \
  if (left < rec->len)	{						   \
    rec->len = left;							   \
    (r)->r_len = (bytes);						   \
  } 									   \
}


/*
 * RCOPY(struct rbuf *r1, struct rbuf *r2)
 */
#define RCOPY(r1, r2)				\
{						\
  uint32_t i;					\
  (r2)->r_io    = (r1)->r_io;			\
  (r2)->r_nrecs = (r1)->r_nrecs;		\
  (r2)->r_value = (r1)->r_value;		\
  (r2)->r_len   = (r1)->r_len;			\
  for (i = 0; i < (r1)->r_nrecs; i++) 		\
    (r2)->r_recs[i] = (r1)->r_recs[i];		\
}

/*
 * RPUT(struct rbuf *r)
 * Send a single rbuf down its IO channel.
 */
#define RPUT(r) \
IO$PutPkt((r)->r_io, (r)->r_nrecs, (r)->r_recs, (r)->r_value, FOREVER);
  
#endif /* _RBUF_H_ */


