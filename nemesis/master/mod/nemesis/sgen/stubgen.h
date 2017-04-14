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
**      mod/nemesis/sgen/stubgen.h
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Type declarations and macros for stub generator
** 
** ID : $Id: stubgen.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _stubgen_h_
#define _stubgen_h_



#include <nemesis.h>
#include <ntsc.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <stackframe.h>
#include <StubGen.ih>
#include <ShmTransport.h>
#include <IDCMacros.h>
#include <IDCServerStubs.ih>
#include <LongCardTblMod.ih>
#include <LongCardTbl.ih>

#ifdef __ALPHA__
#define SMALL_IS_64BIT
#endif

#define DELAYED_ACK

typedef StubGen_Instruction   inst_t;
typedef inst_t   **buf_t;

#define INSTSIZE_OFFSET  0
#define SOP_OFFSET      (INSTSIZE_OFFSET + 1)
#define EOP_OFFSET      (SOP_OFFSET + 1)
#define MODE_OFFSET     (EOP_OFFSET + 1)
#define VALREF_OFFSET   (MODE_OFFSET + 2)
#define FORMAT_OFFSET   (VALREF_OFFSET + 1)
#define BASETYPE_OFFSET (FORMAT_OFFSET + 1)
#define SOS_OFFSET      (BASETYPE_OFFSET + 4)
#define EOS_OFFSET      (SOS_OFFSET + 1)
#define SOC_OFFSET      (EOS_OFFSET + 1)
#define EOC_OFFSET      (SOC_OFFSET + 1)
#define NC_OFFSET       (EOC_OFFSET + 1)
#define ALIGN_OFFSET    (NC_OFFSET + 1)
#define USESIDC_OFFSET  (ALIGN_OFFSET + 3)
#define SIZE_OFFSET     (USESIDC_OFFSET + 1)

/* SIZE_OFFSET is the position in the instruction at which small size
   values start - anything too large to fit in here will be put in the
   next instruction, and the INSTSIZE bit of this instruction will be
   set */

#define MAX_SMALL_SIZE ((1 << (32 - SIZE_OFFSET)) - 1)

#define LARGE_INST(c) ((1<<INSTSIZE_OFFSET) & (c))
#define SOP(c)        ((1<<SOP_OFFSET) & (c))
#define EOP(c)        ((1<<EOP_OFFSET) & (c))
#define MODE(c)       ((3<<MODE_OFFSET) & (c))
#define MODE_IN        (0<<MODE_OFFSET)
#define MODE_OUT       (1<<MODE_OFFSET)
#define MODE_INOUT     (2<<MODE_OFFSET)
#define MODE_RESULT    (3<<MODE_OFFSET)
#define VALREF(c)     ((1<<VALREF_OFFSET) & (c))
#define BYVAL          (0<<VALREF_OFFSET)
#define BYREF          (1<<VALREF_OFFSET)
#define FORMAT(c)     ((1<<FORMAT_OFFSET) & (c))
#define FORMAT_COPY    (0<<FORMAT_OFFSET)
#define FORMAT_STRUCT  (1<<FORMAT_OFFSET)
#define BASETYPE(c)   (((c)>>BASETYPE_OFFSET) & 15)
#define SETTYPE(t)      (((t)&15)<<BASETYPE_OFFSET)
#define TYPE_ALLOCSIZE  0
#define TYPE_INT8       1
#define TYPE_INT16      2
#define TYPE_INT32      3
#define TYPE_INT64      4
#define TYPE_FLT32      5
#define TYPE_FLT64      6
#define TYPE_STRING     7
#define TYPE_BOOL       8
#define TYPE_CUSTOM     9
#define TYPE_ANY       10
#define TYPE_WORD      ((sizeof(word_t) == 8)?TYPE_INT64:TYPE_INT32)
#define SOS(c)        ((1<<SOS_OFFSET) & (c))
#define EOS(c)        ((1<<EOS_OFFSET) & (c))
#define SOC(c)        ((1<<SOC_OFFSET) & (c))
#define EOC(c)        ((1<<EOC_OFFSET) & (c))
#define NC(c)         ((1<<NC_OFFSET) & (c))
#define ALIGNVAL(c)   (((c) >> ALIGN_OFFSET) & 7)
#define USESIDC(c)    ((1<<USESIDC_OFFSET) & (c))
#define USEIDC         (1<<USESIDC_OFFSET)
#define DATASIZE(c)   (((c) >> SIZE_OFFSET) & MAX_SMALL_SIZE)

#define SETVAL  0xffffffff
#define CLEARVAL 0

#define Boolean__ctype         bool_t
#define ShortCardinal__ctype   uint16_t
#define Cardinal__ctype        uint32_t
#define LongCardinal__ctype    uint64_t
#define ShortInteger__ctype    int16_t
#define Integer__ctype         int32_t
#define LongInteger__ctype     int64_t
#define Real__ctype            float32_t
#define LongReal__ctype        float64_t
#define Octet__ctype           uint8_t
#define Char__ctype            int8_t
#define Address__ctype         addr_t
#define Word__ctype            word_t

#define Boolean__spec          "u"
#define ShortCardinal__spec    "u"
#define Cardinal__spec         "u"
#define LongCardinal__spec     "qx"
#define ShortInteger__spec     "d"
#define Integer__spec          "d"
#define LongInteger__spec      "qx"
#define Real__spec             "f"
#define LongReal__spec         "f"
#define Octet__spec            "u"
#define Char__spec             "u"
#define Address__spec          "p"
#define Word__spec             "x"


#ifdef DEBUG_MUF_ALL
#define DEBUG_MUF
#endif

#ifdef DEBUG_MUF
#ifdef INTEL
#define TRCMUF(_x) { if (debugmuf) { int ipl = ntsc_ipl_high(); _x; ntsc_swpipl(ipl); }  }
#else
#define TRCMUF(_x) { if (debugmuf) { _x; } }
#endif
#define NO_MUF_INLINE
#else 
#define TRCMUF(_x)
#endif

#ifdef DEBUG_SYN
#define TRCSYN(_x) _x
#else
#define TRCSYN(_x)
#endif

#ifdef DEBUG_TS
#define TRCTS(_x...) eprintf(_x)
#else
#define TRCTS(_x...)
#endif

#ifdef DEBUG
#define TRC(x) x
#define DB(x) x
#define NO_MUF_INLINE
#else
#define TRC(x) 
#define DB(x)  x
#endif

#ifdef NO_MUF_INLINE
#define MUF_INLINE 
#else
#define MUF_INLINE __inline__
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED\n", __FUNCTION__);

#define DO_ASSERTS

#if (defined(DEBUG) || defined(DO_ASSERTS))
#define ASSERT(_x) if(!(_x)) { eprintf("Stubgen: Assertion '" #_x "'  (%s:%d) failed\n", __FILE__, __LINE__); ntsc_dbgstop(); }
#else
#define ASSERT(_x)
#endif

#define TROUNDUP(p,TYPE) ( (((word_t)(p)) + sizeof(TYPE)-1) & -sizeof(TYPE) )

#define MAX_OPS 40

typedef struct _generic_stub_st      generic_stub_st;
typedef struct _stub_operation_info  stub_operation_info;
typedef struct _generic_op_st        generic_op_st;

/* The stub state is laid out as follows:

   A generic_stub_st structure

   The stub_operation_info structure for each operation
   The stub_exception_info structure for each exception
   The various chunks of byte code

   */

typedef uint64_t client_fn(generic_op_st *info, ...);
typedef void     server_fn(generic_stub_st *st,
			   _generic_cl *clp,
			   word_t opnum,
			   IDCServerBinding_clp bnd,
			   IDC_clp idc,
			   IDC_BufferDesc bd);
			   
/* IF YOU CHANGE THESE STRUCTURES, CHANGE THE ASM STUBS TOO ... */

/* A generic_op_st is an element in the table hung off the per-binding
   IDCClientStubs_State - it contains a pointer to the full
   (per-stubs) operation info, a back pointer to the
   IDCClientStubs_State and a pointer to the function to be executed
   for this method */

struct _generic_op_st {
    stub_operation_info  *info;
    IDCClientStubs_State *cs;
    client_fn            *func;
};

/* A stub_operation_info describes an operation */
	
struct _stub_operation_info {
    word_t     opnum;
    client_fn *client_fn;
    server_fn *server_fn;
    string_t   name;
    inst_t    *code;
    word_t     codelen;
    bool_t     has_exns;
    bool_t     announcement;
};

typedef struct _stub_exception_info {
    
    string_t  name;    /* The name of the exception */
    inst_t   *code;    /* Pointer to the bytecode */
    int       codelen;
    set_t     procs;   /* Procedures allowed to raise this exn (bitset) */

} stub_exception_info;

/* The generic_stub_st contains the optables for both client and
   server. The server optable is immediately preceded by a back
   pointer to the generic_stub_st, so that the server can find its
   state (This is per-stub state, not per-binding state. Maybe there
   ought to be a handle field in the IDCStubs_Rec structure */

/* The server optable is needed in here, since the word before it is
   the pointer to this state record, used by the server to locate the
   state */


struct _generic_stub_st {
    IDCStubs_Rec                    rec;
    string_t                        intfname; 
    IDCServerStubs_cl               server_cl;
    IDCClientStubs_cl               client_cl;
    _generic_cl                     surr_cl;
    uint32_t                        maxargssize;
    generic_stub_st                *srv_backptr;
    IDCServerStubs_op               srv_optable;
    uint32_t                        num_ops;
    stub_operation_info            *ops;
    uint32_t                        num_exns;
    stub_exception_info            *exns;
};

#define STUB_ST_FROM_CLP(_clp) ((generic_stub_st **)((_clp)->op))[-1]

void pretty_print_inst(inst_t *code);
void pretty_print_prog(inst_t *code);
IDCServerStubs_Dispatch_fn Generic_Dispatch;

extern client_fn Generic_Stub_C;
extern server_fn Generic_Stub_S;

extern client_fn Fast_Stub_C;
extern server_fn Fast_Stub_S;

extern void HandleClientException(stub_operation_info *info, 
				  int exn_num, 
				  string_t exnname,
				  IDC_BufferDesc bd,
				  IDCClientBinding_clp bnd,
				  IDC_clp idc);

extern bool_t marshalString(IDC_BufferDesc bd, string_t s);
extern bool_t unmarshalString(IDC_BufferDesc bd,
			      string_t *res,
			      bool_t doCopy,
			      Heap_clp h);

extern StubGen_Marshal_fn StubGen_Marshal_m;
extern StubGen_Unmarshal_fn StubGen_Unmarshal_m;

typedef struct _code_entry code_entry;

typedef enum {
    code_canned,
    code_synth,
    code_fast,
    code_slow
} code_entry_type;

struct _code_entry {
    inst_t          *code;
    code_entry_type  type;
    int              len;
    client_fn       *client;
    server_fn       *server;
    int              refCount;
    code_entry      *next;
};

extern const code_entry canned_table[];

/* Synthesised stubs stuff */

typedef struct _synth_func synth_func;

typedef struct _reloc_info {
    word_t src;
    word_t dest;
    bool_t rel;
    word_t size;
} reloc_info;

typedef SEQ_STRUCT(reloc_info) reloc_seq;

struct _synth_func {
    uint8_t* buffer;
    uint32_t buffersize, offset;
    word_t arginfo, argbase;
    word_t staticinfo, staticbase;
    word_t status;
    word_t buffp_offset;
    word_t argp_offset;
    word_t argp_align;
    word_t framesize;
    reloc_seq relocs;
    word_t space_return;
};



extern synth_func *StartClient(word_t args, word_t space_call, 
			       word_t space_return);

extern void DoClientCall(synth_func *func);
extern client_fn *FinishClient(synth_func *func);

extern synth_func *StartServer(word_t statics, word_t args, 
			       word_t space_call, word_t space_return);
extern void DoServerInvocation(synth_func *func);
extern server_fn * FinishServer(synth_func *func);

extern void SkipArg(synth_func *func, word_t argsize, word_t staticsize);
extern void StartIndirect(synth_func *func, word_t align, word_t staticsize);

extern void MarshalArg(synth_func *func, word_t argsize);
extern void MarshalArgIDC(synth_func *func, Type_Code argtype);
extern void MarshalIndirect(synth_func *func, word_t align, word_t argsize);
extern void MarshalIndirectIDC(synth_func *func, 
			       word_t align, 
			       Type_Code argtype);

extern void MarshalRet(synth_func *func, word_t argsize);
extern void MarshalRetIDC(synth_func *func, Type_Code tc);
extern void MarshalString(synth_func *func, bool_t indirect);

extern void UnmarshalArg(synth_func *func, word_t argsize);
extern void UnmarshalArgIDC(synth_func *func, Type_Code argtype);
extern void UnmarshalIndirect(synth_func *func, word_t align, word_t argsize);
extern void UnmarshalIndirectIDC(synth_func *func, word_t align, 
				 Type_Code argtype);
extern void UnmarshalRet(synth_func *func, word_t argsize);
extern void UnmarshalRetIDC(synth_func *func, Type_Code argtype);
extern void UnmarshalString(synth_func *func, bool_t indirect);
extern void UnmarshalArgInPlace(synth_func *f, word_t datasize);
extern void RaiseIDCFailure(void);

extern bool_t SynthesiseFunctions(inst_t *codebase, word_t args, 
				  client_fn **client_code, server_fn **server_code);

#endif /* _stubgen_h_ */

