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
**      mod/nemesis/sgen/canned_stubs.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Pre-generated stubs for some simple IDC operations for use
**      with generic dispatcher
** 
** ID : $Id: canned_stubs.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
** */

#include <stubgen.h>
#include <IDCMarshal.h>

#define STUB_CODE(name, code...) const inst_t name##_Code[] = { code };

#define STUB(r, p1, p2, p3) {				\
    (inst_t *) STUB_NAME(r, p1, p2, p3, Code),			\
    code_canned,					\
    sizeof(STUB_NAME(r, p1, p2, p3, Code)),		\
    (client_fn *) STUB_NAME(r, p1, p2, p3, Stub_C),	\
    STUB_NAME(r, p1, p2, p3, Stub_S),			\
    0,							\
    NULL },

#define Void__plist(parm)
#define Void__decl(parm) 
#define Void__rtype void
#define Void__code(mode)
#define Void__marshal(parm) True
#define Void__unmarshal(parm) True
#define Void__arg(parm)
#define Void__getres(parm) 
#define Void__return(parm)

#define Int32__plist(parm) , uint32_t parm
#define Int32__decl(parm)  uint32_t parm;
#define Int32__rtype uint32_t
#define Int32__code(mode) (SOP(SETVAL) | BYVAL | mode | SETTYPE(TYPE_INT32) | FORMAT_STRUCT ),
#define Int32__marshal(parm)  IDC_mCardinal(idc, bd, parm)
#define Int32__unmarshal(parm) IDC_uCardinal(idc, bd, &parm)
#define Int32__arg(parm) , parm
#define Int32__getres(parm) parm =
#define Int32__return(parm) parm

#define Int64__plist(parm) , uint64_t parm
#define Int64__decl(parm)  uint64_t parm;
#define Int64__rtype  uint64_t
#define Int64__code(mode) (SOP(SETVAL) | BYVAL | mode | SETTYPE(TYPE_INT64) | FORMAT_STRUCT ),
#define Int64__marshal(parm) IDC_mLongCardinal(idc, bd, parm)
#define Int64__unmarshal(parm) IDC_uLongCardinal(idc, bd, &parm)
#define Int64__arg(parm), parm
#define Int64__getres(parm) parm =
#define Int64__return(parm) parm

#define String__plist(parm) ,string_t parm
#define String__decl(parm)   string_t parm;
#define String__rtype        string_t
#define String__code(mode)   (SOP(SETVAL) | BYVAL | mode | SETTYPE(TYPE_STRING) | FORMAT_STRUCT ),
#define String__marshal(parm) marshalString(bd, parm)
#define String__unmarshal(parm) unmarshalString(bd, &parm, False, NULL)
#define String__arg(parm)   , parm

#define CLIENT_START()							\
    IDCClientBinding_clp  bnd;						\
    IDC_BufferDesc        bd;						\
    stub_operation_info  *info;						\
    string_t              exnname = NULL;				\
    word_t res;								\
									\
    bnd = op->cs->binding;						\
									\
    info = op->info;							\
									\
    bd = IDCClientBinding$InitCall(bnd, info->opnum, info->name);

#define CLIENT_SEND()						\
    IDCClientBinding$SendCall(bnd, bd);				\
								\
    res = IDCClientBinding$ReceiveReply(bnd, &bd, &exnname);	\
								\
    if(res) {							\
								\
	HandleClientException(info, res, exnname,		\
			      bd, bnd, op->cs->marshal);	\
    }

#define CLIENT_FINISH() IDCClientBinding$AckReceive(bnd, bd);

#define STUB_NAME(r, p1, p2, p3, suffix) r##_##p1##p2##p3##_##suffix	

#ifdef DELAYED_ACK
#define MAYBE_SERVER_ACK_RECEIVE()
#else
#define MAYBE_SERVER_ACK_RECEIVE() IDCServerBinding$AckReceive(bnd, bd)
#endif

#define DECLARE_STUB(ret, parm1, parm2, parm3)				\
STUB_CODE(ret##_##parm1##parm2##parm3,					\
	  parm1##__code(MODE_IN)					\
	  parm2##__code(MODE_IN)					\
	  parm3##__code(MODE_IN)					\
	  ret##__code(MODE_RESULT)					\
	  SOP(SETVAL) | EOP(SETVAL))					\
									\
ret##__rtype STUB_NAME(ret,parm1,parm2,parm3,Stub_C) (			\
    generic_op_st *op							\
    parm1##__plist(p1)							\
    parm2##__plist(p2)							\
    parm3##__plist(p3)){						\
									\
    ret##__decl(retval)							\
									\
    CLIENT_START();							\
    TRC(eprintf("Using " __FUNCTION__ ", '%s'\n",			\
		info->name));						\
    if(!(parm1##__marshal(p1) &&					\
	 parm2##__marshal(p2) &&					\
	 parm3##__marshal(p3))) {					\
	RAISE_IDC$Failure();						\
    }									\
									\
    CLIENT_SEND();							\
    if(!(ret##__unmarshal(retval))) {					\
	RAISE_IDC$Failure();						\
    }									\
									\
    CLIENT_FINISH();							\
    return ret##__return(retval);					\
}									\
									\
typedef ret##__rtype STUB_NAME(ret, parm1, parm2, parm3, Func) (	\
    _generic_cl *cl							\
    parm1##__plist(p1)							\
    parm2##__plist(p2)							\
    parm3##__plist(p3));						\
									\
void STUB_NAME(ret, parm1, parm2, parm3, Stub_S)(			\
    generic_stub_st *st,						\
    _generic_cl *clp,							\
    word_t opnum,							\
    IDCServerBinding_clp bnd,						\
    IDC_clp idc,							\
    IDC_BufferDesc bd) {						\
									\
    ret##__decl(retval)							\
    parm1##__decl(p1)							\
    parm2##__decl(p2)							\
    parm3##__decl(p3)							\
									\
    if(!(parm1##__unmarshal(p1) &&					\
	 parm2##__unmarshal(p2) &&					\
	 parm3##__unmarshal(p3))) {					\
	RAISE_IDC$Failure();						\
    }									\
    MAYBE_SERVER_ACK_RECEIVE();						\
									\
									\
    ret##__getres(retval)						\
	((STUB_NAME(ret, parm1, parm2, parm3, Func) *)			\
	 (((addr_t *)clp->op)[opnum]))(clp				\
				       parm1##__arg(p1)			\
				       parm2##__arg(p2)			\
				       parm3##__arg(p3));		\
									\
    bd = IDCServerBinding$InitReply(bnd);				\
									\
    if(!(ret##__marshal(retval))) { RAISE_IDC$Failure(); }		\
									\
    IDCServerBinding$SendReply(bnd, bd);				\
}

/* Pregenerate stubs for all combinations of void/int32/int64 return
   type and up to 3 int32/int64 parameters of same type */

DECLARE_STUB(Void, Int32, Void, Void)
DECLARE_STUB(Void, Int64, Void, Void)
DECLARE_STUB(Void, Int32, Int32, Void)
DECLARE_STUB(Void, Int64, Int64, Void)
DECLARE_STUB(Void, Int32, Int32, Int32)
DECLARE_STUB(Void, Int64, Int64, Int64)

DECLARE_STUB(Int32, Void, Void, Void)
DECLARE_STUB(Int32, Int32, Void, Void)
DECLARE_STUB(Int32, Int64, Void, Void)
DECLARE_STUB(Int32, Int32, Int32, Void)
DECLARE_STUB(Int32, Int64, Int64, Void)
DECLARE_STUB(Int32, Int32, Int32, Int32)
DECLARE_STUB(Int32, Int64, Int64, Int64)

DECLARE_STUB(Int64, Void, Void, Void)
DECLARE_STUB(Int64, Int32, Void, Void)
DECLARE_STUB(Int64, Int64, Void, Void)
DECLARE_STUB(Int64, Int32, Int32, Void)
DECLARE_STUB(Int64, Int64, Int64, Void)
DECLARE_STUB(Int64, Int32, Int32, Int32)
DECLARE_STUB(Int64, Int64, Int64, Int64)

DECLARE_STUB(Void, String, Int32, Void)

DECLARE_STUB(Void, Void, Void, Void)

const code_entry canned_table[] = {

    STUB(Void, Void, Void, Void)
    STUB(Void, Int32, Void, Void)
    STUB(Void, Int64, Void, Void)
    STUB(Void, Int32, Int32, Void)
    STUB(Void, Int64, Int64, Void)
    STUB(Void, Int32, Int32, Int32)
    STUB(Void, Int64, Int64, Int64)
    
    STUB(Int32, Void, Void, Void)
    STUB(Int32, Int32, Void, Void)
    STUB(Int32, Int64, Void, Void)
    STUB(Int32, Int32, Int32, Void)
    STUB(Int32, Int64, Int64, Void)
    STUB(Int32, Int32, Int32, Int32)
    STUB(Int32, Int64, Int64, Int64)
    
    STUB(Int64, Void, Void, Void)
    STUB(Int64, Int32, Void, Void)
    STUB(Int64, Int64, Void, Void)
    STUB(Int64, Int32, Int32, Void)
    STUB(Int64, Int64, Int64, Void)
    STUB(Int64, Int32, Int32, Int32)
    STUB(Int64, Int64, Int64, Int64)

    STUB(Void, String, Int32, Void)
    {0, 0, 0, 0, 0, 0, 0},
};
	
	      
