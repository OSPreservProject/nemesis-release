/*-
 * Copyright (c) 1990, 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence 
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bpf.c	7.5 (Berkeley) 7/15/91
 *
 * static char rcsid[] =
 * "$Id: BPF.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $";
 */
#if 0
#if !(defined(lint) || defined(KERNEL))
static char rcsid[] =
    "@(#) $Id: BPF.c 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $ (LBL)";
#endif
#endif /* 0 */

/* This file has been heavily hacked to work with Nemesis and IO_Recs */

#include <nemesis.h>
#include <BPFMod.ih>
#include <Heap.ih>
#include <bpf.h>

/* XXX move to bpf.h */
#define BPF_MEMWORDS 32

#ifdef BPF_TRC
#define TRC(x) printf x
#else
#define TRC(x)
#endif /* BPF_TRC */

/* Module stuff -------------------------------------------------- */

static BPFMod_New_fn New_m;
static PF_Apply_fn Apply_m;
static PF_Snaplen_fn Snaplen_m;
static PF_Dispose_fn Dispose_m;
static PF_CopyApply_fn CopyApply_m;

static BPFMod_op m_op = { New_m };
static const BPFMod_cl m_cl = { &m_op, (addr_t)0xdeadbeef };

CL_EXPORT(BPFMod,BPFMod,m_cl);

static PF_op f_op = { 
    Apply_m, 
    Snaplen_m,
    Dispose_m,
    CopyApply_m,
};

typedef struct BPF_st BPF_st;
struct BPF_st {
    Heap_clp heap;
    PF_Handle handle;
    uint32_t slen;
    BPFMod_Instruction *pr;
};

/* Filter stuff -------------------------------------------------- */

/* Read up to 4 bytes out of an IO_Rec array */
/* ASSUME data in IO_Rec is all in network byte order (big endian) */
static uint32_t rbuf_get(IO_Rec *rec, int nrecs, int offset, int size,
			 int *err)
{
    int rbuf=0;
    uint32_t result=0;

    *err=0;

    while (size) {
	while (offset>rec[rbuf].len && rbuf<nrecs) {
	    offset-=rec[rbuf].len;
	    rbuf++;
	}

	if (rbuf>=nrecs) {
	    /* We're reading off the end of the packet */
	    *err=1;
	    return result;
	}

	/* XXX check the next line carefully... */
	result |= (((uint8_t *)rec[rbuf].base)[offset++]) << (--size*8);
    }

    return result;
}

/* Execute the filter program starting at pc on the packet p. buflen is
 * the amount of data present, equal to the size of the original
 * packet. */
static uint32_t bpf_filter(
    BPFMod_Instruction *pc,
    IO_Rec *p,
    int nrecs,
    uint32_t buflen)
{
    uint32_t A, X;
    int k, err;
    int32_t mem[BPF_MEMWORDS];

    A = 0;
    X = 0;

    pc--; /* Ugly, but works */

    TRC(("--- bpf:\n"));
    while (1) {
	++pc;
	switch (pc->code) {

	default:                         /* Invalid instruction */
	    printf("BPF: invalid instruction\n");
	    return 0;
	case BPF_RET|BPF_K:              /* Return constant */
	    TRC(("BPF_RET|BPF_K %d\n", pc->k));
	    return pc->k;

	case BPF_RET|BPF_A:              /* Return value in A */
	    TRC(("BPF_RET|BPF_A\n"));
	    return A;

	case BPF_LD|BPF_W|BPF_ABS:       /* Load 32 bits at abs addr to A */
	    TRC(("BPF_LD|BPF_W|BPF_ABS %d\n", pc->k));
	    k = pc->k;
	    if (k + sizeof(uint32_t) > buflen) {
		return 0; /* Off the end of the packet */
	    }
	    A = rbuf_get(p, nrecs, k, 4, &err);
	    if (err) return 0;
	    TRC((" ... [k]=%#x\n", A));
	    continue;

	case BPF_LD|BPF_H|BPF_ABS:       /* Load 16 bits at abs addr to A */
	    TRC(("BPF_LD|BPF_H|BPF_ABS %d\n", pc->k));
	    k = pc->k;
	    if (k + sizeof(uint16_t) > buflen) {
		return 0;
	    }
	    A = rbuf_get(p, nrecs, k, 2, &err);
	    if (err) return 0;
	    TRC((" ... [k]=%#x\n", A));
	    continue;

	case BPF_LD|BPF_B|BPF_ABS:       /* Load 8 bits at abs addr to A */
	    TRC(("BPF_LD|BPF_B|BPF_ABS\n"));
	    k = pc->k;
	    if (k >= buflen) {
		return 0;
	    }
	    A = rbuf_get(p, nrecs, k, 1, &err);
	    if (err) return 0;
	    continue;

	case BPF_LD|BPF_W|BPF_LEN:       /* Load packet size to A */
	    TRC(("BPF_LD|BPF_W|BPF_LEN\n"));
	    A = buflen;
	    continue;

	case BPF_LDX|BPF_W|BPF_LEN:      /* Load packet size to X */
	    TRC(("BPF_LDX|BPF_W|BPF_LEN\n"));
	    X = buflen;
	    continue;

	case BPF_LD|BPF_W|BPF_IND:       /* Load indirect 32 bits to A */
	    TRC(("BPF_LD|BPF_W|BPF_IND\n"));
	    k = X + pc->k;
	    if (k + sizeof(uint32_t) > buflen)
		return 0;
	    A = rbuf_get(p, nrecs, k, 4, &err);
	    if (err) return 0;
	    continue;

	case BPF_LD|BPF_H|BPF_IND:       /* Load indirect 16 bits to A */
	    TRC(("BPF_LD|BPF_H|BPF_IND\n"));
	    k = X + pc->k;
	    if (k + sizeof(short) > buflen) {
		return 0;
	    }
	    A = rbuf_get(p, nrecs, k, 2, &err);
	    if (err) return 0;
	    continue;

	case BPF_LD|BPF_B|BPF_IND:       /* Load indirect 8 bits to A */
	    TRC(("BPF_LD|BPF_B|BPF_IND\n"));
	    k = X + pc->k;
	    if (k >= buflen)
		return 0;
	    A = rbuf_get(p, nrecs, k, 1, &err);
	    if (err) return 0;
	    continue;

	case BPF_LDX|BPF_MSH|BPF_B:      /* X <- 4*(P[k:1]&0xf) */
	    TRC(("BPF_LDX|BPF_MSH|BPF_B\n"));
	    k = pc->k;
	    if (k >= buflen) {
		return 0;
	    }
	    X = (rbuf_get(p, nrecs, k, 1, &err) & 0xf) << 2;
	    if (err) return 0;
	    continue;

	case BPF_LD|BPF_IMM:             /* A <- k */
	    TRC(("BPF_LD|BPF_IMM %d\n", pc->k));
	    A = pc->k;
	    continue;

	case BPF_LDX|BPF_IMM:            /* X <- k */
	    TRC(("BPF_LDX|BPF_IMM\n"));
	    X = pc->k;
	    continue;

	case BPF_LD|BPF_MEM:             /* A <- mem[k] */
	    TRC(("BPF_LD|BPF_MEM\n"));
	    A = mem[pc->k];
	    continue;
			
	case BPF_LDX|BPF_MEM:            /* X <- mem[k] */
	    TRC(("BPF_LDX|BPF_MEM\n"));
	    X = mem[pc->k];
	    continue;

	case BPF_ST:                     /* mem[k] <- A */
	    TRC(("BPF_ST\n"));
	    mem[pc->k] = A;
	    continue;

	case BPF_STX:                    /* mem[k] <- X */
	    TRC(("BPF_STX\n"));
	    mem[pc->k] = X;
	    continue;

	case BPF_JMP|BPF_JA:
	    TRC(("BPF_JMP|BPF_JA\n"));
	    pc += pc->k;
	    continue;

	case BPF_JMP|BPF_JGT|BPF_K:
	    TRC(("BPF_JMP|BPF_JGT|BPF_K\n"));
	    pc += (A > pc->k) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JGE|BPF_K:
	    TRC(("BPF_JMP|BPF_JGE|BPF_K\n"));
	    pc += (A >= pc->k) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JEQ|BPF_K:
	    TRC(("BPF_JMP|BPF_JEQ|BPF_K %#x?%d:%d\n", pc->k, pc->jt, pc->jf));
	    pc += (A == pc->k) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JSET|BPF_K:
	    TRC(("BPF_JMP|BPF_JSET|BPF_K\n"));
	    pc += (A & pc->k) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JGT|BPF_X:
	    TRC(("BPF_JMP|BPF_JGT|BPF_X\n"));
	    pc += (A > X) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JGE|BPF_X:
	    TRC(("BPF_JMP|BPF_JGE|BPF_X\n"));
	    pc += (A >= X) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JEQ|BPF_X:
	    TRC(("BPF_JMP|BPF_JEQ|BPF_X\n"));
	    pc += (A == X) ? pc->jt : pc->jf;
	    continue;

	case BPF_JMP|BPF_JSET|BPF_X:
	    TRC(("BPF_JMP|BPF_JSET|BPF_X\n"));
	    pc += (A & X) ? pc->jt : pc->jf;
	    continue;

	case BPF_ALU|BPF_ADD|BPF_X:
	    TRC(("BPF_ALU|BPF_ADD|BPF_X\n"));
	    A += X;
	    continue;
			
	case BPF_ALU|BPF_SUB|BPF_X:
	    TRC(("BPF_ALU|BPF_SUB|BPF_X\n"));
	    A -= X;
	    continue;
			
	case BPF_ALU|BPF_MUL|BPF_X:
	    TRC(("BPF_ALU|BPF_MUL|BPF_X\n"));
	    A *= X;
	    continue;
			
	case BPF_ALU|BPF_DIV|BPF_X:
	    TRC(("BPF_ALU|BPF_DIV|BPF_X\n"));
	    if (X == 0)
		return 0;
	    A /= X;
	    continue;
			
	case BPF_ALU|BPF_AND|BPF_X:
	    TRC(("BPF_ALU|BPF_AND|BPF_X\n"));
	    A &= X;
	    continue;
			
	case BPF_ALU|BPF_OR|BPF_X:
	    TRC(("BPF_ALU|BPF_OR|BPF_X\n"));
	    A |= X;
	    continue;

	case BPF_ALU|BPF_LSH|BPF_X:
	    TRC(("BPF_ALU|BPF_LSH|BPF_X\n"));
	    A <<= X;
	    continue;

	case BPF_ALU|BPF_RSH|BPF_X:
	    TRC(("BPF_ALU|BPF_RSH|BPF_X\n"));
	    A >>= X;
	    continue;

	case BPF_ALU|BPF_ADD|BPF_K:
	    TRC(("BPF_ALU|BPF_ADD|BPF_K\n"));
	    A += pc->k;
	    continue;
			
	case BPF_ALU|BPF_SUB|BPF_K:
	    TRC(("BPF_ALU|BPF_SUB|BPF_K\n"));
	    A -= pc->k;
	    continue;
			
	case BPF_ALU|BPF_MUL|BPF_K:
	    TRC(("BPF_ALU|BPF_MUL|BPF_K\n"));
	    A *= pc->k;
	    continue;
			
	case BPF_ALU|BPF_DIV|BPF_K:
	    TRC(("BPF_ALU|BPF_DIV|BPF_K\n"));
	    A /= pc->k;
	    continue;
			
	case BPF_ALU|BPF_AND|BPF_K:
	    TRC(("BPF_ALU|BPF_AND|BPF_K\n"));
	    A &= pc->k;
	    continue;
			
	case BPF_ALU|BPF_OR|BPF_K:
	    TRC(("BPF_ALU|BPF_OR|BPF_K\n"));
	    A |= pc->k;
	    continue;

	case BPF_ALU|BPF_LSH|BPF_K:
	    TRC(("BPF_ALU|BPF_LSH|BPF_K\n"));
	    A <<= pc->k;
	    continue;

	case BPF_ALU|BPF_RSH|BPF_K:
	    TRC(("BPF_ALU|BPF_RSH|BPF_K\n"));
	    A >>= pc->k;
	    continue;

	case BPF_ALU|BPF_NEG:
	    TRC(("BPF_ALU|BPF_NEG\n"));
	    A = -A;
	    continue;

	case BPF_MISC|BPF_TAX:
	    TRC(("BPF_MISC|BPF_TAX\n"));
	    X = A;
	    continue;

	case BPF_MISC|BPF_TXA:
	    TRC(("BPF_MISC|BPF_TXA\n"));
	    A = X;
	    continue;
	}
    }
}

/* 
 * Return a snaplen if the 'fcode' is a valid filter program, or 0
 * otherwise.  The constraints are that each jump be forward and to a
 * valid code.  The code must terminate with either an accept or
 * reject.  'valid' is an array for use by the routine (it must be at
 * least 'len' bytes long).
 *
 * The kernel needs to be able to verify an application's filter code.
 * Otherwise, a bogus program could easily crash the system.  
 */
static uint32_t bpf_validate(struct BPFMod_Instruction *f, int len)
{
    int i;
    struct BPFMod_Instruction *p;

    for (i = 0; i < len; ++i) {
	/*
	 * Check that that jumps are forward, and within 
	 * the code block.
	 */
	p = &f[i];
	if (BPF_CLASS(p->code) == BPF_JMP) {
	    register int from = i + 1;

	    if (BPF_OP(p->code) == BPF_JA) {
		if (from + p->k >= len)
		    return 0;
	    }
	    else if (from + p->jt >= len || from + p->jf >= len)
		return 0;
	}
	/*
	 * Check that memory operations use valid addresses.
	 */
	if ((BPF_CLASS(p->code) == BPF_ST ||
	     (BPF_CLASS(p->code) == BPF_LD && 
	      (p->code & 0xe0) == BPF_MEM)) &&
	    (p->k >= BPF_MEMWORDS || p->k < 0))
	    return 0;
	/*
	 * Check for constant division by 0.
	 */
	if (p->code == (BPF_ALU|BPF_DIV|BPF_K) && p->k == 0)
	    return 0;
    }
    return (BPF_CLASS(f[len - 1].code) == BPF_RET) ? 1500 : 0;
}

static PF_Handle Apply_m(PF_cl *self, IO_Rec *p, uint32_t nrecs)
{
    BPF_st *st = self->st;
    int i;
    uint32_t bufsize;

    /* Work out how big the packet is */
    bufsize=0;
    for (i=0; i<nrecs; i++)
	bufsize+=p[i].len;
    
    if(bpf_filter(st->pr, p, nrecs, bufsize))
	return st->handle;
    
    return (PF_Handle)0;
}

/* XXX hacky: could probably do a much better job */
static PF_Handle CopyApply_m(PF_cl *self, IO_Rec *p, uint32_t nrecs,
			     addr_t dest)
{
    IO_Rec rec;

    memcpy(dest, p[0].base, p[0].len);
    rec.base = dest;
    rec.len = p[0].len;
    return Apply_m(self, &rec, 1);
}

static uint32_t Snaplen_m(PF_cl *self)
{
    BPF_st *st = self->st;

    return st->slen;
}

static void Dispose_m(PF_cl *self)
{
    BPF_st *st = self->st;

    /* Delete program */
    Heap$Free(st->heap, st->pr);
    
    /* Delete closure and state */
    Heap$Free(st->heap, self);
}

static PF_clp New_m(BPFMod_cl *self, Heap_clp heap,
			      const BPFMod_Program *prog, 
			      PF_Handle handle)
{
    PF_cl *cl;
    BPF_st *st;
    int i, slen;

    /* Allocate space for closure and state */
    cl = Heap$Malloc(heap, sizeof(*cl) + sizeof(*st));

    cl->op = &f_op;
    cl->st = st = (BPF_st *)(cl+1);

    st->heap   = heap;
    st->handle = handle;

    /* Allocate space for program */
    st->pr=Heap$Malloc(heap, sizeof(BPFMod_Instruction) * SEQ_LEN(prog));

    /* Copy program into our memory */
    for (i=0; i < SEQ_LEN(prog); i++)
	st->pr[i] = SEQ_ELEM(prog, i);

    /* Verify program */
    if (!(slen = bpf_validate(st->pr, SEQ_LEN(prog)))) {
	RAISE_BPFMod$BadProgram();
    }

    st->slen = slen;
    return cl;
}
