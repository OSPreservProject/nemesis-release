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
**      mod/nemesis/sgen/ix86_synth.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      ix86 specific backend to dynamic stub function synthesiser
** 
** ID : $Id: ix86_synth.c 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
** 
**
*/


#include "stubgen.h"

#define IDC_IN_EDI 1
#define BUFFP_IN_EDI 2
#define ARGP_IN_EBX 4
#define ARGP_IN_EDX 8
#define ARGP_IN_EBP 16
#define BND_IN_EBX 32
#define IS_SERVER 64
#define IS_MARSHALLING 128

/* Misc useful functions */

#define RESIZABLE
#ifdef RESIZABLE
#define GROW_FUNC(f)					\
   do {							\
        f->buffersize += 512;				\
        f->buffer = REALLOC(f->buffer, f->buffersize);	\
   } while(0)
#define ALLOC_FUNC(size) Heap$Malloc(Pvs(heap), size)
#define COMPACT_FUNC(f) REALLOC(f->buffer, f->offset)
#else
#include <salloc.h>
__inline__ addr_t GetStretch(word_t size) {
    Stretch_clp str;
    Stretch_Size s;
    str = STR_NEW(size);
    STR_SETGLOBAL(str, Stretch_Right_Read|Stretch_Right_Execute);
    return Stretch$Range(str, &s);
}

#define GROW_FUNC(f) ntsc_dbgstop()
#define ALLOC_FUNC(size) GetStretch(size)
#define COMPACT_FUNC(f) f->buffer
#endif

void AddInst1(synth_func *f, uint8_t inst1) {

    if(f->offset == f->buffersize) {
	GROW_FUNC(f);
    }

    f->buffer[f->offset++] = inst1;
}

void AddInst2(synth_func *f, uint8_t inst1, uint8_t inst2) {
    if(f->offset > f->buffersize-2) {
	GROW_FUNC(f);
    }

    f->buffer[f->offset++] = inst1;
    f->buffer[f->offset++] = inst2;
}
void AddInst3(synth_func *f, uint8_t inst1, uint8_t inst2, uint8_t inst3) {
    if(f->offset > f->buffersize-3) {
	GROW_FUNC(f);
    }

    f->buffer[f->offset++] = inst1;
    f->buffer[f->offset++] = inst2;
    f->buffer[f->offset++] = inst3;
}

void AddInst4(synth_func *f, uint8_t inst1, uint8_t inst2, uint8_t inst3, uint8_t inst4) {
    if(f->offset > f->buffersize - 4) {
	GROW_FUNC(f);
    }

    f->buffer[f->offset++] = inst1;
    f->buffer[f->offset++] = inst2;
    f->buffer[f->offset++] = inst3;
    f->buffer[f->offset++] = inst4;
}

void AddImm32(synth_func *f, uint32_t val) {
    AddInst4(f, val, val >> 8, val >> 16, val >> 24);
}

/* Add an instruction that uses the ModR/M form of
   addressing. Automatically slect the optimal form for the given
   displacement. op2 should have the Mod portion set to 0
   (i.e. indirect through register without displacement. It will be
   adjusted accordingly). If the desired indirect register is ebp, set
   R/M to 5 - this would normally indicate a 32-bit absolute address,
   but it will be adjusted. */

void AddInst2Disp(synth_func *f, uint8_t op1, uint8_t op2, int disp) {
    
    AddInst1(f, op1);

    if(disp == 0 && ((op2 & 7) != 5)) {
	/* Use [reg] addressing, unless we're trying to disp from ebp
           (i.e. the R/M portion of op2 is 5) in which case we have to
           use an 8-bit displacement of 0 */
	AddInst1(f, op2);
    } else if((disp < 128) && (disp > -127)) {
	/* Use disp8[reg] addressing */
	AddInst2(f, op2 | 0x40, disp);
    } else {
	/* Use disp32[reg] addressing */
	AddInst1(f, op2 | 0x80);
	AddImm32(f, disp);
    }
}
	
	

void AddBlock(synth_func *f, const uint8_t *block, word_t blocksize) {
    if(f->offset > f->buffersize - blocksize) {
	GROW_FUNC(f);
    }
    memcpy(f->buffer + f->offset, block, blocksize);
    f->offset += blocksize;
}

void AddReloc(synth_func *f, word_t src, word_t dest, uint32_t size, bool_t relative) {
    
    reloc_info i = {src, dest, relative, size};

    SEQ_ADDH(&f->relocs, i);
}

void SaveBUFFP(synth_func *f) {
    if(f->status & BUFFP_IN_EDI) {
	TRCSYN(eprintf("Saving bufp (code = %p)\n", f->buffer + f->offset));

	/* Save buffp in bd */
	/* lea  buffpoffset(edi), edi */
	if(f->buffp_offset == 0) {
	    /* No need to change edi */ 
	} else {

	    /* lea  buffpoffset(edi), edi */
	    AddInst2Disp(f, 0x8d, 0x3f, f->buffp_offset);

	    /* movl edi, bd:esi->ptr */
	    AddInst3(f, 0x89, 0x7e, OFFSETOF(IDC_BufferRec, ptr));
	}
	f->status &= ~BUFFP_IN_EDI;
    }
}
	
void LoadIDCintoEDI(synth_func *f) {
    if(f->status & IDC_IN_EDI) return;

    SaveBUFFP(f);

    TRCSYN(eprintf("Loading IDC (code = %p)\n", f->offset + f->buffer));
    
    if(f->status & IS_SERVER) {
	/* movl idc ( ==0x18(ebp)), edi */
	AddInst3(f, 0x8b, 0x7d, 0x18);
    } else {
	/* movl op (==0x8(ebp)), edi */
	AddInst3(f, 0x8b, 0x7d, 0x08);
	/* movl op->cs (== 0x4(edi)), edi */
	AddInst3(f, 0x8b, 0x7f, (OFFSETOF(generic_op_st, cs)));
	/* movl op->cs->idc (== 0x10(edi)), edi */
	AddInst3(f, 0x8b, 0x7f, (OFFSETOF(IDCClientStubs_State, marshal)));
    }

    f->status |= IDC_IN_EDI;
}
    
void LoadBUFFPintoEDI(synth_func *f) {
    if(f->status & BUFFP_IN_EDI) return;

    f->status &= ~IDC_IN_EDI;
    
    TRCSYN(eprintf("Loading buffp (code = %p)\n", f->buffer + f->offset));
    /* movl bd:esi->ptr, edi */
    AddInst3(f, 0x8b, 0x7e, OFFSETOF(IDC_BufferRec, ptr));
    f->status |= BUFFP_IN_EDI;
    f->buffp_offset = 0;
}

void LoadBndIntoEBX(synth_func *f) {
    
    if(f->status & BND_IN_EBX) {
	TRCSYN(eprintf("LoadBnd: Bnd available (code = %p)\n",
		       f->offset + f->buffer));

	return;
    }

    /* No need to spill argp, since if we need bnd, we've finished
       with it */
    
    f->status &= ~ARGP_IN_EBX;
    
    TRCSYN(eprintf("Loading Bnd into EBX (code = %p)\n", 
		   f->offset + f->buffer));
    /* Load bnd */

    if(f->status & IS_SERVER) {
	/* movl 0x14(ebp), ebx */
	AddInst3(f, 0x8b, 0x5d, 0x14);
    } else {
	/* movl -4(ebp), ebx */
	AddInst3(f, 0x8b, 0x5d, -4);
    }
    f->status |= BND_IN_EBX;
}

void SaveArgp(synth_func *f) {
    
    /* If argp doesn't need saving, don't bother */
    if(!(f->status & ARGP_IN_EDX)) {
	return;
    }

    TRCSYN(eprintf("Saving ARGP to EBX\n"));

    f->status &= ~(BND_IN_EBX|ARGP_IN_EDX);
    f->status |= ARGP_IN_EBX;

    /* movl edx, ebx */
    AddInst2(f, 0x8b, 0xda);

}


void DoRelocations(synth_func *f) {
    
    reloc_info r;
    int i;

    for(i = 0; i < SEQ_LEN(&f->relocs); i++) {
	r = SEQ_ELEM(&f->relocs, i);
	if(!r.rel) {
	    uint32_t jmpval;
	    ASSERT(r.size == 32);
	    
	    jmpval = r.dest - ((word_t)(f->buffer + r.src + 4));
	    f->buffer[r.src] = jmpval;
	    f->buffer[r.src + 1] = jmpval >> 8;
	    f->buffer[r.src + 2] = jmpval >> 16;
	    f->buffer[r.src + 3] = jmpval >> 24;
	} else {
	    ASSERT(r.size == 8);
	    
	    f->buffer[r.src] = (r.dest - r.src) - 1;
	}
    }
}

void AddCall(synth_func *f, addr_t dest) {
    AddInst1(f, 0xe8);
    AddReloc(f, f->offset, (word_t) dest, 32, False);
    AddImm32(f, 0);
}

void AddEprintf(synth_func *f, string_t s, int num_words) {

    SaveArgp(f);
    /* pushl s */
    AddInst1(f, 0x68);
    AddImm32(f, (uint32_t)s);
    AddCall(f, eprintf);
    /* addl 0x04 + num_words * 4, esp */
    AddInst3(f, 0x83, 0xc4, 0x04 + num_words * 4);
}

void CheckSpace(synth_func *f, word_t space) {
    word_t src;

    if(space) {
	/* cmpl bd (==esi)->space, <space> */
	if(space < 128) {
	    AddInst4(f, 0x83, 0x7e, OFFSETOF(IDC_BufferRec, space), space); 
	} else {
	    AddInst3(f, 0x81, 0x7e, OFFSETOF(IDC_BufferRec, space));
	    AddImm32(f, space);
	}

	/* jge pc + ?? */
	AddInst1(f, 0x7d);
	src = f->offset;
	AddInst1(f, 0xff);
	AddInst1(f, 0xcc);
	/* pushl bd->space */
	AddInst3(f, 0xff, 0x76, OFFSETOF(IDC_BufferRec, space));
	/* pushl space */
	AddInst1(f, 0x68);
	AddImm32(f, space);
	AddEprintf(f, "Out of space! Needed %u, found %u \n", 2);
	/* RaiseIDCFailure(); */
	AddCall(f, RaiseIDCFailure);
	
	/* Continue */
	AddReloc(f, src, f->offset, 8, True);
	
	/* subl bd:esi->space, <space> */
	if(space < 128) {
	    AddInst4(f, 0x83, 0x6e, OFFSETOF(IDC_BufferRec, space), space);
	} else {
	    AddInst3(f, 0x81, 0x6e, OFFSETOF(IDC_BufferRec, space));
	    AddImm32(f, space);
	}
    }
}
#define SIB(s, i, b) ((((s)&3)<<6)|(((i)&7)<<3)|((d)&7))
#define MODRM(m, ro, rm) ((((m)&3)<<6)|(((ro)&7)<<3)|((rm)&7))

/* Client register/stack usage in ix86 synth funcs:

   Stack:

   ebp - 20 result (edx)
   ebp - 16 result (eax)
   ebp - 12 bd returned
   ebp - 8  exception_name
   ebp - 4  bnd
   ebp      saved stack pointer
   ebp + 4  return address
   ebp + 8  generic_op_st *
   ebp + 12 arg1
   ebp + 16 arg2 ...

   Registers:

   ebx      bnd, or argp
   esi      info, then bd
   edi      op, then buffp/idc
   edx      argp sometimes
   */   

const uint8_t client_prologue[] = {

    /* pushl ebp */
    0x55,
    /* movl esp, ebp */
    0x89, 0xe5,
    /* subl 0x18, esp */ /* local var space == 24 ... */
    0x83, 0xec, 0x18,
    
    /* pushl edi */
    0x57,
    /* pushl esi */
    0x56,
    /* pushl ebx */
    0x53,
    
    /* movl op (==0x8(ebp)), edi */
    0x8b, 0x7d, 0x08,
    
    /* movl op->cs (== 0x4(edi)), eax */
    0x8b, 0x47, (OFFSETOF(generic_op_st, cs)),
    /* movl op->cs->binding (== 0x10(eax)), ebx */
    0x8b, 0x58, (OFFSETOF(IDCClientStubs_State, binding)),
    /* movl op->info (== (edi)), esi */
    0x8b, 0x37,
    /* movl ebx, -4(ebp) */
    0x89, 0x5d, -4,

    /* bd:esi = IDCClientBinding$InitCall(ebx, 0xc(esi), 0(esi)) */
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl 0xc(esi) */
    0xff, 0x76, 0x0c,
    /* pushl (esi) */
    0xff, 0x36,
    /* pushl ebx */
    0x53,
    /* movl InitCall(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCClientBinding_op, InitCall),
    /* call *eax */
    0xff, 0xd0,
    /* movl eax, esi */
    0x89, 0xc6,
    /* addl 0xc, esp */
    0x83, 0xc4, 0x0c,


};
    
    
    
synth_func *StartClient(word_t args, word_t space_call, word_t space_return) {
    
    synth_func *f = Heap$Malloc(Pvs(heap), sizeof(*f));
    
    TRCSYN(eprintf("StartClient(%u, %u)\n", space_call, space_return));

    f->buffersize = 1024;
    f->buffer = ALLOC_FUNC(f->buffersize);
    f->offset = 0;
    f->arginfo = f->argbase = 12; 
    f->staticinfo = f->staticbase = 0;
    f->status = IS_MARSHALLING;
    f->space_return = space_return;

    SEQ_INIT(&f->relocs, 0, Pvs(heap));

    /* Add the client prologue, as far as the 

       bd = IDCClientBinding$InitCall() */

    AddBlock(f, client_prologue, sizeof(client_prologue));

#ifdef DEBUG_SYN
    /* edi currently holds op */
    /* movl op:edi->info, eax */
    AddInst2(f, 0x8b, 0x07);
    /* pushl info->name */
    AddInst3(f, 0xff, 0x70, OFFSETOF(stub_operation_info, name));

    AddEprintf(f, "Client: Started function '%s'\n", 1);
#endif
    /* Check there's enough space in the buffer */

    CheckSpace(f, space_call);
    
    /* Now ready for actual marshalling */
    
    TRCSYN(AddEprintf(f, "Client: Ready to marshal\n", 0));
    return f;
}

const uint8_t client_call[] = {
    
    /* IDCClientBinding$SendCall(bnd (==ebx), bd (==esi)) */
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl esi */
    0x56,
    /* pushl ebx */
    0x53,
    /* movl SendCall(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCClientBinding_op, SendCall),
    /* call *eax */
    0xff, 0xd0,
    /* addl 0x08, esp */
    0x83, 0xc4, 0x08,

    /* res = IDCClientBinding$ReceiveReply(bnd (==ebx), &bd:0xf4(ebp), &exnname:0xf8(ebp)) */
    
    /* lea  0xf8(ebp), eax */
    0x8d, 0x45, 0xf8,
    /* pushl eax */
    0x50,
    /* lea  0xf4(ebp), eax */
    0x8d, 0x45, 0xf4,
    /* pushl eax */
    0x50,
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl ebx */
    0x53,
    /* movl ReceiveReply(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCClientBinding_op, ReceiveReply),
    /* call *eax */
    0xff, 0xd0,
    /* addl 0x0c, esp */
    0x83, 0xc4, 0x0c,

    /* Load returned bd into esi */
    /* movl bd(== 0xf4(ebp)), esi */
    0x8b, 0x75, 0xf4,
};
    
const uint8_t handle_exception[] = {
    /* HandleClientException(info, res, exnname, bd, bnd, idc) */
    /* pushl edi */
    0x57,
    /* pushl ebx */
    0x53,
    /* pushl esi */
    0x56,
    /* pushl 0xf8(ebp) */
    0xff, 0x75, 0xf8,
    /* pushl eax */
    0x50,
    /* movl op (==0x8(ebp)), eax */
    0x8b, 0x45, 0x08,
    /* movl op->info, eax */
    0x8b, 0x00,
    /* pushl eax */
    0x50,

    /* Actual call is done later */

};
void DoClientCall(synth_func *f) {

    word_t src;
    word_t status;

    TRCSYN(AddEprintf(f, "Client: Done marshalling\n", 0));
    SaveBUFFP(f);

    LoadBndIntoEBX(f);
    AddBlock(f, client_call, sizeof(client_call));

    f->status &= ~(BUFFP_IN_EDI | IS_MARSHALLING);

    /* cmpl res:eax, 0 */
    AddInst1(f, 0x3d);
    AddImm32(f, 0);

    status = f->status;
    /* je ??? */
    AddInst1(f, 0x74);
    src = f->offset;
    AddInst1(f, 0xff);

    LoadIDCintoEDI(f);

    AddBlock(f, handle_exception, sizeof(handle_exception));
    AddCall(f, HandleClientException);

    /* addl 0x18, esp */
    AddInst3(f, 0x83, 0xc4, 0x18);
    AddReloc(f, src, f->offset, 8, True);

    f->status = status;

    CheckSpace(f, f->space_return);
    f->arginfo = f->argbase;

    TRCSYN(AddEprintf(f, "Client: Ready to unmarshal\n", 0));

}

const uint8_t client_epilogue[] = {
    
    /* IDCClientBinding$AckReceive(bnd:ebx, bd:esi) */
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl esi */
    0x56,
    /* pushl ebx */
    0x53,
    /* movl AckReceive(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCClientBinding_op, AckReceive),
    /* call *eax */
    0xff, 0xd0,
    /* addl 0x08, esp */
    0x83, 0xc4, 0x08,

    /* return result */
    /* movl 0xf0(ebp), eax */
    0x8b, 0x45, 0xf0,
    /* movl 0xec(ebp), edx */
    0x8b, 0x55, 0xec,
    
    /* pop ebx */
    0x5b,
    /* pop esi */
    0x5e,
    /* pop edi */
    0x5f,
    /* leave */
    0xc9,
    /* ret */
    0xc3
};
    
client_fn *FinishClient(synth_func *f) {
    
    client_fn *res;
    
    TRCSYN(AddEprintf(f, "Client: Done unmarshalling\n", 0));

    LoadBndIntoEBX(f);

    AddBlock(f, client_epilogue, sizeof(client_epilogue));
    
    f->buffer = COMPACT_FUNC(f);
    DoRelocations(f);

    res = (client_fn *)f->buffer;
    
    TRCSYN(eprintf("Finishing client : %p %p\n", 
		   f->buffer, f->offset + f->buffer));
    SEQ_FREE_DATA(&f->relocs);
    FREE(f);
    return res;
}  

/* Server register/stack usage in ix86 synth funcs:

   Stack:

   ebp - ?? start of args
   ebp - ?? arg/static boundary
   ebp - 24 end of static space
   ebp - 20 retval (edx)
   ebp - 16 retval (eax)
   ebp - 12 saved ebx
   ebp - 8  saved esi
   ebp - 4  saved edi
   ebp      saved stack pointer
   ebp + 4  return address
   ebp + 8  generic_stub_st *
   ebp + 12 generic_clp
   ebp + 16 opnum
   ebp + 20 bnd
   ebp + 24 idc
   ebp + 28 bd

   Registers:

   ebx      bnd
   esi      bd
   edi      buffp/idc
   */   

        
synth_func *StartServer(word_t statics, word_t args, word_t space_call,
			word_t space_return) {
    
    synth_func *f = Heap$Malloc(Pvs(heap), sizeof(*f));
    int framesize;
    
    TRCSYN(eprintf("StartServer(%u, %u, %u, %u)\n", statics, args, 
		   space_call, space_return));

    f->buffersize = 1024;
    f->buffer = ALLOC_FUNC(f->buffersize);
    f->offset = 0;
    f->staticinfo = f->staticbase = -(20 + statics);
    f->arginfo = f->argbase = f->staticbase - args;
    f->status = IS_SERVER;
    f->space_return = space_return;

    SEQ_INIT(&f->relocs, 0, Pvs(heap));

    /* pushl ebp */
    AddInst1(f, 0x55);
    /* movl esp, ebp */
    AddInst2(f, 0x89, 0xe5);
    
    /* pushl edi */
    AddInst1(f, 0x57);
    /* pushl esi */
    AddInst1(f, 0x56);
    /* pushl ebx */
    AddInst1(f, 0x53);
    /* Deal with local space - edi, ebx and esi are already on the stack */
    framesize = 8 + statics + args;
    /* subl framesize, esp */
    if(framesize < 128) {
	AddInst3(f, 0x83, 0xec, framesize);
    } else {
	AddInst2(f, 0x81, 0xec);
	AddImm32(f, framesize);
    }

    /* movl bnd:0x14(ebp), ebx */
    AddInst3(f, 0x8b, 0x5d, 0x14);
    /* movl bd:0x1c(ebp), esi */
    AddInst3(f, 0x8b, 0x75, 0x1c);

    /* Check there is enough data in the buffer */
    
    CheckSpace(f, space_call);


    /* Ready to unmarshal data */

    return f;
}

const uint8_t server_invocation[] = {

#ifndef DELAYED_ACK
    /* IDCServerBinding$AckReceive(bnd:ebx, bd:esi) */
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl esi */
    0x56,
    /* pushl ebx */
    0x53,
    /* movl AckReceive(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCServerBinding_op, AckReceive),
    /* call *eax */
    0xff, 0xd0,
    /* addl 0x08, esp */
    0x83, 0xc4, 0x08,
#endif

    /* Actually do the invocation: eax = clp->op[opnum]() */
    /* movl clp:0x0c(ebp), ecx */
    0x8b, 0x4d, 0x0c,
    /* movl optable:(ecx), eax */
    0x8b, 0x01,
    /* movl opnum:0x10(ebp), edx */
    0x8b, 0x55, 0x10,
    /* pushl clp:ecx */
    0x51,
    /* movl (eax + edx * 4), eax */
    0x8b, 0x04, 0x90,
    /* call *eax */
    0xff, 0xd0,
    
    /* Don't bother removing the args from the stack */
    /* Saved returned values */
    /* movl edx, -20(ebp) */
    0x89, 0x55, -20,
    /* movl eax, -16(ebp) */
    0x89, 0x45, -16,

    /* bd = IDCServerBinding$InitReply(bnd:ebx) */
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl ebx */
    0x53,
    /* movl SendCall(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCServerBinding_op, InitReply),
    /* call *eax */
    0xff, 0xd0,
    /* Store returned bd */
    /* movl eax, esi */
    0x8b, 0xf0,
    /* addl 0x08, esp */
    0x83, 0xc4, 0x08,
    
};

void DoServerInvocation(synth_func *f) {

    /* At this point, the arguments should be set up correctly on the
       stack */

    LoadBndIntoEBX(f);

    AddBlock(f, server_invocation, sizeof(server_invocation));
    
    f->status |= IS_MARSHALLING;

    f->status &= ~(BUFFP_IN_EDI|ARGP_IN_EDX);

    f->arginfo = f->argbase;
    f->staticinfo = f->staticbase;

    CheckSpace(f, f->space_return);

    /* Ready to marshal returned parameters */

}

const uint8_t server_epilogue[] = {
    
    /* IDCServerBinding$SendReply(bnd:ebx, bd:esi) */
    /* movl (ebx), eax */
    0x8b, 0x03,
    /* pushl esi */
    0x56,
    /* pushl ebx */
    0x53,
    /* movl AckReceive(eax), eax */
    0x8b, 0x40, OFFSETOF(IDCServerBinding_op, SendReply),
    /* call *eax */
    0xff, 0xd0,
    /* addl 0x08, esp */
    0x83, 0xc4, 0x08,

    /* Tidy up */
    /* movl -4(ebp), edi */
    0x8b, 0x7d, -4,
    /* movl -8(ebp), esi */
    0x8b, 0x75, -8,
    /* movl -12(ebp), ebx */
    0x8b, 0x5d, -12,
    /* movl -20(ebp), edx */
    0x8b, 0x55, -20,
    /* movl -16(ebp), eax */
    0x8b, 0x45, -16,
    /* leave */
    0xc9,
    /* ret */
    0xc3,
};

server_fn *FinishServer(synth_func *f) {
    server_fn *res;
    
    SaveBUFFP(f);
    
    LoadBndIntoEBX(f);

    AddBlock(f, server_epilogue, sizeof(server_epilogue));

    f->buffer = COMPACT_FUNC(f);
    DoRelocations(f);
    
    res = (server_fn *) f->buffer;

    TRCSYN(eprintf("Finishing server : %p %p\n", 
		   f->buffer, f->offset + f->buffer));

    SEQ_FREE_DATA(&f->relocs);
    FREE(f);
    return res;
}

void MoveEAXintoBuffer(synth_func *f, int buffpoffset) {

    /* movl eax, buffpoffset(edi) */
    AddInst2Disp(f, 0x89, 0x07, buffpoffset);
}
    
void MarshalArg(synth_func *f, word_t argsize) {
    
    int reps;
    int tmpoffset;

    TRCSYN(eprintf("MarshalArg(%p, %u)\n", f, argsize));

    ASSERT(!(f->status & IS_SERVER));
    ASSERT(f->status & IS_MARSHALLING);

    LoadBUFFPintoEDI(f);

    tmpoffset = f->buffp_offset;

    /* Move the current argument into the buffer.  If this is 64 bits,
       do two moves */

    ASSERT(argsize <= 8);
    reps = (argsize > 4) ? 2 : 1;

    /* movl arginfo(ebp), eax */
    AddInst2Disp(f, 0x8b, 0x05, f->arginfo);
    f->arginfo += 4;
    MoveEAXintoBuffer(f, tmpoffset);

    if(argsize > 4) {
	/* movl arginfo(ebp), eax */
	AddInst2Disp(f, 0x8b, 0x05, f->arginfo);
	f->arginfo += 4;
	MoveEAXintoBuffer(f, tmpoffset+4);
    }

    f->buffp_offset += 8;
} 

void MarshalStackTopIDC(synth_func *f, Type_Code tc) {

    int op_offset;
    word_t src;

    LoadIDCintoEDI(f);
    
    switch(tc) {
    case uint8_t__code:
	op_offset = OFFSETOF(IDC_op, mOctet);
	break;

    case uint16_t__code:
	op_offset = OFFSETOF(IDC_op, mShortCardinal);
	break;

    case uint32_t__code:
	op_offset = OFFSETOF(IDC_op, mCardinal);
	break;

    case uint64_t__code:
	op_offset = OFFSETOF(IDC_op, mLongCardinal);
	break;

    case bool_t__code:
	op_offset = OFFSETOF(IDC_op, mBoolean);
	break;

    default:
	ASSERT(False);
	
    }

    /* movl (idc:edi), eax */
    AddInst2(f, 0x8b, 0x07);
    /* pushl bd:esi */
    AddInst1(f, 0x56);
    /* movl op_offset(eax), eax */
    AddInst2Disp(f, 0x8b, 0x00, op_offset);
    /* pushl edi */
    AddInst1(f, 0x57);
    /* call *eax */
    AddInst2(f, 0xff, 0xd0);
    if((tc == uint64_t__code) || (tc == float64_t__code)) {
	/* addl 0x10, esp */
	AddInst3(f, 0x83, 0xc4, 0x10);
    } else {
	/* addl 0x0c, esp */
	AddInst3(f, 0x83, 0xc4, 0x0c);
    }

    /* Return OK? */
    /* test eax, eax */
    AddInst2(f, 0x85, 0xc0);
    /* je ?? */
    AddInst1(f, 0x75);
    src = f->offset;
    AddInst1(f, 0xff);
    
    /* RaiseIDCFailure() */
    AddCall(f, RaiseIDCFailure);

    /* Continue */
    AddReloc(f, src, f->offset, 8, True);

}

void MarshalArgIDC(synth_func *f, Type_Code tc) {

    TRCSYN(eprintf("MarshalArgIDC(%p, %qx)\n", f, tc));

    ASSERT(!(f->status & IS_SERVER));
    ASSERT(f->status & IS_MARSHALLING);

    if((tc == uint64_t__code) || (tc == float64_t__code)) {
	/* pushl arginfo+4(ebp) */
	AddInst2Disp(f, 0xff, 0x35, f->arginfo + 4);
	/* pushl arginfo(ebp) */
	AddInst2Disp(f, 0xff, 0x35, f->arginfo);
	f->arginfo += 8;
    } else {
	/* pushl arginfo(ebp) */
	AddInst2Disp(f, 0xff, 0x35, f->arginfo);
	f->arginfo += 4;
    }
    
    MarshalStackTopIDC(f, tc);
}

void MoveBufferIntoEAX(synth_func *f, int buffp_offset) {

    /* movl buffpoffset(edi), eax */
    AddInst2Disp(f, 0x8b, 0x07, buffp_offset);
}

void UnmarshalArg(synth_func *f, word_t argsize) {
    
    int reps, tmpoffset;

    TRCSYN(eprintf("UnmarshalArg(%p, %u)\n", f, argsize));

    ASSERT(f->status & IS_SERVER);
    ASSERT(!(f->status & IS_MARSHALLING));

    LoadBUFFPintoEDI(f);

    tmpoffset = f->buffp_offset;

    /* Move the current argument on to the stack. We can't just push
       it, since we are getting the args in reverse order. If this is
       64 bits, do it twice */
    
    ASSERT(argsize <= 8);
    reps = (argsize > 4) ? 2 : 1;

    MoveBufferIntoEAX(f, tmpoffset);
    /* movl eax, arginfo(ebp) */
    AddInst2Disp(f, 0x89, 0x05, f->arginfo);
    f->arginfo += 4;

    if(argsize > 4) {
	MoveBufferIntoEAX(f, tmpoffset + 4);
	/* movl eax, arginfo(ebp) */
	AddInst2Disp(f, 0x89, 0x05, f->arginfo);
	f->arginfo += 4;
    }

    f->buffp_offset += 8;
}

void UnmarshalArgInPlace(synth_func *f, word_t datasize) {

    TRCSYN(eprintf("UnmarshalArgInPlace(%p, %u)\n", f, datasize));
    ASSERT(f->status & IS_SERVER);
    ASSERT(!(f->status & IS_MARSHALLING));
    
    LoadBUFFPintoEDI(f);

    if(f->buffp_offset) {
	/* lea buffp_offset(edi), eax */
	AddInst2Disp(f, 0x8d, 0x07, f->buffp_offset);
	/* movl eax, argpinfo(ebp) */ 
	AddInst2Disp(f, 0x89, 0x05, f->arginfo);
    } else {
	/* movl edi, argp_offset(ebp) */
	AddInst2Disp(f, 0x89, 0x3d, f->arginfo);
    }
    f->arginfo += 4;

    f->buffp_offset += TROUNDUP(datasize, uint64_t);
}

void UnmarshalViaStackTopIDC(synth_func *f, Type_Code tc) {

    int op_offset;
    word_t src;

    LoadIDCintoEDI(f);

    switch(tc) {
    case uint8_t__code:
	op_offset = OFFSETOF(IDC_op, uOctet);
	break;

    case uint16_t__code:
	op_offset = OFFSETOF(IDC_op, uShortCardinal);
	break;

    case uint32_t__code:
	op_offset = OFFSETOF(IDC_op, uCardinal);
	break;

    case uint64_t__code:
	op_offset = OFFSETOF(IDC_op, uLongCardinal);
	break;

    case bool_t__code:
	op_offset = OFFSETOF(IDC_op, uBoolean);
	break;

    default:
	ASSERT(False);
	
    }
    /* movl (idc:edi), eax */
    AddInst2(f, 0x8b, 0x07);
    /* pushl bd:esi */
    AddInst1(f, 0x56);
    /* movl op_offset(eax), eax */
    AddInst2Disp(f, 0x8b, 0x00, op_offset);
    /* pushl edi */
    AddInst1(f, 0x57);
    /* call *eax */
    AddInst2(f, 0xff, 0xd0);
    /* addl 0x0c, esp */
    AddInst3(f, 0x83, 0xc4, 0x0c);
    /* Return OK? */
    /* test eax, eax */
    AddInst2(f, 0x85, 0xc0);
    /* je ?? */
    AddInst1(f, 0x75);
    src = f->offset;
    AddInst1(f, 0xff);
    
    /* RaiseIDCFailure() */
    AddCall(f, RaiseIDCFailure);

    /* Continue */
    AddReloc(f, src, f->offset, 8, True);

}

void UnmarshalArgIDC(synth_func *f, Type_Code tc) {

    TRCSYN(eprintf("UnmarshalArgIDC(%p, %qx)\n", f, tc));
    
    ASSERT(f->status & IS_SERVER);
    ASSERT(!(f->status & IS_MARSHALLING));
    /* lea arginfo(ebp), eax */
    AddInst2Disp(f, 0x8d, 0x05, f->arginfo);
    /* pushl eax */
    AddInst1(f, 0x50);

    UnmarshalViaStackTopIDC(f, tc);

    if((tc == uint64_t__code) || (tc == float64_t__code)) {
	f->arginfo += 8;
    } else {
	f->arginfo += 4;
    }

}
    
void SkipArg(synth_func *f, word_t argsize, word_t staticsize) {

    TRCSYN(eprintf("SkipArg(%p, %u, %u)\n", f, argsize, staticsize));

    ASSERT(argsize <= 8);

    if(f->status & IS_SERVER) {
	if(!(f->status & IS_MARSHALLING)) {
	    /* Need to set the argument value here */
	    /* lea staticinfo(ebp), eax */
	    AddInst2Disp(f, 0x8d, 0x05, f->staticinfo);
	    /* movl eax, arginfo(ebp) */
	    AddInst2Disp(f, 0x89, 0x05, f->arginfo);
	}
	f->staticinfo += TROUNDUP(staticsize, uint64_t);
    } else {
	ASSERT(staticsize == 0);
    }

    if(argsize > 4) {
	f->arginfo += 8;
    } else {
	f->arginfo += 4;
    }

}


void MarshalRet(synth_func *f, word_t argsize) {
    
    int tmpoffset;
    TRCSYN(eprintf("MarshalRet(%p, %u)\n", f, argsize));

    ASSERT(f->status & IS_SERVER);
    ASSERT(f->status & IS_MARSHALLING);
    ASSERT(argsize <= 8);

    LoadBUFFPintoEDI(f);

    tmpoffset = f->buffp_offset;

    /* movl eaxretval, eax */
    AddInst3(f, 0x8b, 0x45, -16);
    MoveEAXintoBuffer(f, tmpoffset);

    if(argsize > 4) {
	/* movl edxretval, eax */
	AddInst3(f, 0x8b, 0x45, -20);
	MoveEAXintoBuffer(f, tmpoffset + 4);
    }

    f->buffp_offset += 8;
}
    
void UnmarshalRet(synth_func *f, word_t argsize) {
	
    int tmpoffset;

    TRCSYN(eprintf("UnmarshalRet(%p, %u)\n", f, argsize));

    ASSERT(!(f->status & IS_SERVER));
    ASSERT(!(f->status & IS_MARSHALLING));
    ASSERT(argsize <= 8);
    
    LoadBUFFPintoEDI(f);

    tmpoffset = f->buffp_offset;

    MoveBufferIntoEAX(f, tmpoffset);
    /* mov eax, eaxretval */
    AddInst3(f, 0x89, 0x45, -16);
    
    if(argsize > 4) {
	MoveBufferIntoEAX(f, tmpoffset + 4);
	/* mov eax, edxretval */
	AddInst3(f, 0x89, 0x45, -20);
    }
    
    f->buffp_offset += 8;
}

void MarshalRetIDC(synth_func *f, Type_Code tc) {

    TRCSYN(eprintf("MarshalRetIDC(%p, %qx)\n", f, tc));

    ASSERT(f->status & IS_SERVER);
    ASSERT(f->status & IS_MARSHALLING);


    if((tc == uint64_t__code) || (tc == float64_t__code)) {
	/* pushl edxretval */
	AddInst3(f, 0xff, 0x75, -20);
	/* pushl eaxretval */
	AddInst3(f, 0xff, 0x75, -16);
    } else {
	/* pushl eaxretval */
	AddInst3(f, 0xff, 0x75, -16);
    }
    
    MarshalStackTopIDC(f, tc);
}

void UnmarshalRetIDC(synth_func *f, Type_Code tc) {

    TRCSYN(eprintf("UnmarshalRetIDC(%p, %qx)\n", f, tc));

    ASSERT(!(f->status & IS_SERVER));
    ASSERT(!(f->status & IS_MARSHALLING));
    /* lea eaxretval:-16(ebp), edx */
    AddInst3(f, 0x8d, 0x55, -16);
    /* push edx */
    AddInst1(f, 0x52);

    UnmarshalViaStackTopIDC(f, tc);
}

void StartIndirect(synth_func *f, word_t align, word_t staticsize) {
    
    TRCSYN(eprintf("StartIndirect(%p, %u, %u)\n", f, align, staticsize));
    
    f->argp_align = align;
    
    f->status &= ~ARGP_IN_EBX;
    f->status |= ARGP_IN_EDX;

    if(f->status & IS_SERVER) {
	/* lea staticinfo(ebp), edx */
	AddInst2Disp(f, 0x8d, 0x15, f->staticinfo);
	/* If unmarshalling, store value in argument */
	if(!(f->status & IS_MARSHALLING)) {
	    /* movl edx, arginfo(ebp) */
	    AddInst2Disp(f, 0x89, 0x15, f->arginfo);
	}
	f->staticinfo += TROUNDUP(staticsize, uint64_t);
    } else {
	ASSERT(staticsize == 0);
	/* client just needs to move argp into edx */
	/* movl arginfo(ebp), edx */
	AddInst2Disp(f, 0x8b, 0x15, f->arginfo);
    }
    f->argp_offset = 0;
    f->arginfo += 4;
}

void MarshalIndirect(synth_func *f, word_t align, word_t argsize) {

    word_t copylen;
    bool_t use_memcpy = False;
    int pos;
	
    TRCSYN(eprintf("MarshalIndirect(%p, %u, %u)\n", f, align, argsize));

    ASSERT(argsize > 0);
    ASSERT(f->status & IS_MARSHALLING);
    /* We should have argp loaded somewhere */
    ASSERT(f->status & (ARGP_IN_EBX|ARGP_IN_EDX));

    /* We need to make sure we are correctly aligned on the parameter */
    
    ASSERT(align <= f->argp_align);
    
    f->argp_offset = (f->argp_offset + align) & ~align;

    LoadBUFFPintoEDI(f);

    /* Copy the required number of words - we don't mind copying too
       much data, since we're padding to 8 bytes anyway */
	
    copylen = argsize;
    
    switch(align) {
    case 7:
    case 3:
	/* 8- or 4-byte aligned. */

	if(copylen > 256) {
	    use_memcpy = True;
	} else {
	    /* Use movl */
	    for(pos = 0; pos < copylen; pos += 4) {
		
		/* Load word from memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movl argpoffset(ebx), eax */
		    AddInst2Disp(f, 0x8b, 0x03, f->argp_offset + pos);
		} else {
		    /* movl argpoffset(edx), eax */
		    AddInst2Disp(f, 0x8b, 0x02, f->argp_offset + pos);
		}
		/* Write into buffer */
		MoveEAXintoBuffer(f, f->buffp_offset);
		f->buffp_offset += 4;
	    }
	}
	break;

    case 1:
	/* 2-byte aligned */

	if(copylen > 40) {
	    use_memcpy = True;
	} else {
	    /* Use movw for reads and movl for writes */
	    for(pos = 0; pos < copylen; pos += 4) {
		
		if(copylen > pos + 2) {
		    /* Load high word from memory */
		    if(f->status & ARGP_IN_EBX) {
			/* movw argpoffset(ebx), ax */
			AddInst1(f, 0x66);
			AddInst2Disp(f, 0x8b, 0x03, f->argp_offset + pos + 2);
		    } else {
			/* movw argpoffset(edx), ax */
			AddInst1(f, 0x66);
			AddInst2Disp(f, 0x8b, 0x02, f->argp_offset + pos + 2);
		    }
		    /* shl eax, 16 */
		    AddInst3(f, 0xc1, 0xe0, 16);
		}

		/* Load low word from memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movw argpoffset(ebx), ax */
		    AddInst1(f, 0x66);
		    AddInst2Disp(f, 0x8b, 0x03, f->argp_offset + pos);
		} else {
		    /* movw argpoffset(edx), ax */
		    AddInst1(f, 0x66);
		    AddInst2Disp(f, 0x8b, 0x02, f->argp_offset + pos);
		}
	    
		/* Write into buffer */
		MoveEAXintoBuffer(f, f->buffp_offset);
		f->buffp_offset += 4;
	    }
	}
	break;

    case 0:
	/* Completely unaligned */
	if(copylen > 20) {
	    use_memcpy = True;
	} else {
	    for(pos = 0; pos < copylen; pos += 4) {
	    /* Use movb for reads and movl for writes */

		if(copylen > pos + 2) {
		    /* Load first bytes */
		    if(f->status & ARGP_IN_EBX) {
			/* movb argpoffset + 2(ebx), al */
			AddInst2Disp(f, 0x8a, 0x03, f->argp_offset + pos + 2);
			/* movb argpoffset + 3(ebx), ah */
			AddInst2Disp(f, 0x8a, 0x23, f->argp_offset + pos + 3);
		    } else {
			/* movb argpoffset + 2(edx), al */
			AddInst2Disp(f, 0x8a, 0x02, f->argp_offset + pos + 2);
			/* movb argpoffset + 3(edx), ah */
			AddInst2Disp(f, 0x8a, 0x22, f->argp_offset + pos + 3);
		    }
		    /* shl eax, 16 */
		    AddInst3(f, 0xc1, 0xe0, 16);
		}

		/* Load low word */
		if(f->status & ARGP_IN_EBX) {
		    /* movb argpoffset + 2(ebx), al */
		    AddInst2Disp(f, 0x8a, 0x03, f->argp_offset + pos);
		    /* movb argpoffset + 3(ebx), ah */
		    AddInst2Disp(f, 0x8a, 0x23, f->argp_offset + pos + 1);
		} else {
		    /* movb argpoffset + 2(edx), al */
		    AddInst2Disp(f, 0x8a, 0x02, f->argp_offset + pos);
		    /* movb argpoffset + 3(edx), ah */
		    AddInst2Disp(f, 0x8a, 0x22, f->argp_offset + pos + 1);
		}

		/* Write into buffer */
		MoveEAXintoBuffer(f, f->buffp_offset);
		f->buffp_offset += 4;
	    }
	}		
	break;
	
    default: 
	ASSERT(False);
    }
    
    if(use_memcpy) {
	
	SaveArgp(f);
	/* Use memcpy(buffp, argp, copylen) */
	/* pushl copylen */
	if(copylen < 128) {
	    AddInst2(f, 0x6a, copylen);
	} else {
	    AddInst1(f, 0x68);
	    AddImm32(f, copylen);
	}
	/* pushl argp */
	if(f->status & ARGP_IN_EBX) {
	    /* lea argpoffset(ebx), eax */
	    AddInst2Disp(f, 0x8d, 0x03, f->argp_offset);
	} else {
	    /* lea argpoffset(edx), eax */
	    AddInst2Disp(f, 0x8d, 0x02, f->argp_offset);
	}
	/* pushl eax */
	AddInst1(f, 0x50);
	/* lea buffp_offset(edi), eax */
	AddInst2Disp(f, 0x8d, 0x07, f->buffp_offset);
	/* pushl eax */
	AddInst1(f, 0x50);
	AddCall(f, memcpy);
	/* addl 0x0c, esp */
	AddInst3(f, 0x83, 0xc4, 0x0c);

	f->buffp_offset += copylen;
    } 

    f->buffp_offset = (f->buffp_offset + 7) & ~7;
    f->argp_offset += argsize;
}

void UnmarshalIndirect(synth_func *f, word_t align, word_t argsize) {

    word_t copylen;
    bool_t use_memcpy = False;
    int pos;

    TRCSYN(eprintf("UnmarshalIndirect(%p, %u, %u)\n", f, align, argsize));
    
    ASSERT(argsize > 0);
    ASSERT(!(f->status & IS_MARSHALLING));
    /* We should have argp loaded somewhere */
    ASSERT(f->status & (ARGP_IN_EBX|ARGP_IN_EDX));

    /* We need to make sure we are correctly aligned on the parameter */
    
    ASSERT(align <= f->argp_align);
    
    f->argp_offset = (f->argp_offset + align) & ~align;

    LoadBUFFPintoEDI(f);

    /* Copy the required number of words - we mustn't copy too much
       data */
	
    copylen = argsize;

    switch(align) {

    case 7:
    case 3:
	/* 8- or 4-byte aligned */

	if(copylen > 256) {
	    use_memcpy = True;
	} else {
	    /* Use movl */
	    for(pos = 0; pos < copylen; pos += 4) {
	    
		/* Load from buffer */
		MoveBufferIntoEAX(f, f->buffp_offset);
		f->buffp_offset += 4;
		
		/* Write word into memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movl eax, argpoffset(ebx) */
		    AddInst2Disp(f, 0x89, 0x03, f->argp_offset + pos);
		} else {
		    /* movl eax, argpoffset(edx) */
		    AddInst2Disp(f, 0x89, 0x02, f->argp_offset + pos);
		}
	    }
	}
	break;
	
    case 1:
	/* 2-byte aligned */
	if(copylen > 40) {
	    use_memcpy = True;
	} else {
	    /* use movw */
	    for(pos = 0; pos < copylen; pos += 4) {
		
		/* Load from buffer */
		MoveBufferIntoEAX(f, f->buffp_offset);
		f->buffp_offset += 4;

		/* Write low word to memory */
		AddInst1(f, 0x66);
		if(f->status & ARGP_IN_EBX) {
		    /* movw ax, argpoffset(ebx) */
		    AddInst2Disp(f, 0x89, 0x02, f->argp_offset + pos);
		} else {
		    /* movw ax, argpoffset(edx) */
		    AddInst2Disp(f, 0x89, 0x03, f->argp_offset + pos);
		}
		
		if(copylen > pos + 2) {
		    /* shr eax, 16 */
		    AddInst3(f, 0xc1, 0xe8, 16);
		    
		    /* Write high word to memory */
		    if(f->status & ARGP_IN_EBX) {
			/* movw ax, argpoffset(ebx) */
			AddInst2Disp(f, 0x89, 0x02, f->argp_offset + pos + 2);
		    } else {
			/* movw ax, argpoffset(edx) */
			AddInst2Disp(f, 0x89, 0x03, f->argp_offset + pos + 2);
		    }
		}
	    }
	}
	break;

    case 0:
	/* Unaligned */
	if(copylen > 20) {
	    use_memcpy = True;
	} else {
	    /* use movb */
	    for(pos = 0; pos < copylen; pos += 4) {
		
		/* Load from buffer */
		MoveBufferIntoEAX(f, f->buffp_offset);
		f->buffp_offset += 4;

		/* Write byte 1 to memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movb al, argpoffset(ebx) */
		    AddInst2Disp(f, 0x88, 0x03, f->argp_offset + pos);
		} else {
		    /* movb al, argpoffset(edx) */
		    AddInst2Disp(f, 0x88, 0x02, f->argp_offset + pos);
		}

		if(copylen == pos + 1) goto done;

		/* Write byte 2 to memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movb ah, argpoffset + 1(ebx) */
		    AddInst2Disp(f, 0x88, 0x23, f->argp_offset + pos + 1);
		} else {
		    /* movb ah, argpoffset + 1(edx) */
		    AddInst2Disp(f, 0x88, 0x22, f->argp_offset + pos + 1);
		}
		
		if(copylen == pos + 2) goto done;

		/* shr eax, 16 */
		AddInst3(f, 0xc1, 0xe8, 16);

		/* Write byte 3 to memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movb al, argpoffset + 2(ebx) */
		    AddInst2Disp(f, 0x88, 0x03, f->argp_offset + pos + 2);
		} else {
		    /* movb al, argpoffset + 2(edx) */
		    AddInst2Disp(f, 0x88, 0x02, f->argp_offset + pos + 2);
		}

		if(copylen == pos + 3) goto done;

		/* Write byte 4 to memory */
		if(f->status & ARGP_IN_EBX) {
		    /* movb ah, argpoffset + 3(ebx) */
		    AddInst2Disp(f, 0x88, 0x23, f->argp_offset + pos + 3);
		} else {
		    /* movb ah, argpoffset + 3(edx) */
		    AddInst2Disp(f, 0x88, 0x22, f->argp_offset + pos + 3);
		}
	    done:
	    }
	}
	break;
    default:
	ASSERT(False);
    }

    if(use_memcpy) {
	SaveArgp(f);
	/* Use memcpy(argp, buffp, copylen) for more than 80 bytes */
	/* pushl copylen */
	if(copylen < 128) {
	    AddInst2(f, 0x6a, copylen);
	} else {
	    AddInst1(f, 0x68);
	    AddImm32(f, copylen);
	}
	/* lea buffp_offset(edi), eax */
	AddInst2Disp(f, 0x8d, 0x07, f->buffp_offset);
	/* pushl eax */
	AddInst1(f, 0x50);

	/* pushl argp */
	if(f->status & ARGP_IN_EBX) {
	    /* lea argpoffset(ebx), eax */
	    AddInst2Disp(f, 0x8d, 0x03, f->argp_offset);
	} else {
	    /* lea argpoffset(edx), eax */
	    AddInst2Disp(f, 0x8d, 0x02, f->argp_offset);
	}
	/* pushl eax */
	AddInst1(f, 0x50);

	AddCall(f, memcpy);
	/* addl 0x0c, esp */
	AddInst3(f, 0x83, 0xc4, 0x0c);

	f->buffp_offset += copylen;
    } 

    f->buffp_offset = (f->buffp_offset + 7) & ~7;
    f->argp_offset += argsize;
}

void MarshalIndirectIDC(synth_func *f, word_t align, Type_Code tc) {

    TRCSYN(eprintf("MarshalDirectIDC(%p, %u, %qx)\n", f, align, tc));
    
    ASSERT(f->status & IS_MARSHALLING);
    /* We should have argp loaded somewhere */
    ASSERT(f->status & (ARGP_IN_EBX|ARGP_IN_EDX));
    /* We need to make sure we are correctly aligned on the parameter */
    
    ASSERT(align <= f->argp_align);
    
    f->argp_offset = (f->argp_offset + align) & ~align;

    /* If argp is currently in edx, we want to save it into ebx
       (callee saves) so that the call to the IDC doesn't trash
       it. This will cause bnd to be flushed out, and reloaded later */

    SaveArgp(f);

    /* Argp is definitely now in ebx */

    switch(tc) {
    case uint64_t__code:
    case float64_t__code:
	/* Push two words from memory */
	/* pushl argpoffset+4(ebx) */
	AddInst2Disp(f, 0xff, 0x33, f->argp_offset + 4);
	/* pushl argpoffset(ebx) */
	AddInst2Disp(f, 0xff, 0x33, f->argp_offset);

	f->argp_offset += 8;
	break;

    case uint32_t__code:
    case float32_t__code:
    case bool_t__code:
	/* Push one word from memory */
	/* pushl argpoffset(ebx) */
	AddInst2Disp(f, 0xff, 0x33, f->argp_offset);
	f->argp_offset += 4;
	break;

    case uint16_t__code:
	/* movzxs argpoffset(ebx), eax */
	AddInst3(f, 0x0f, 0xb7, 0x83);
	AddImm32(f, f->argp_offset);
	/* pushl eax */
	AddInst1(f, 0x50);
	f->argp_offset += 2;
	break;

    case uint8_t__code:
	/* movzxb argpoffset(ebx), eax */
	AddInst3(f, 0x0f, 0xb6, 0x83);
	AddImm32(f, f->argp_offset);
	/* pushl eax */
	AddInst1(f, 0x50);
	f->argp_offset += 1;
	break;
	

    default:
	ASSERT(False);

    }

    MarshalStackTopIDC(f, tc);
}

void UnmarshalIndirectIDC(synth_func *f, word_t align, Type_Code tc) {

    TRCSYN(eprintf("UnmarshalDirectIDC(%p, %u, %qx)\n", f, align, tc));
    
    ASSERT(!(f->status & IS_MARSHALLING));
    /* We should have argp loaded somewhere */
    ASSERT(f->status & (ARGP_IN_EBX|ARGP_IN_EDX));
    
    /* We need to make sure we are correctly aligned on the parameter */
    
    ASSERT(align <= f->argp_align);
    
    f->argp_offset = (f->argp_offset + align) & ~align;

    /* If argp is currently in edx, we want to save it into ebx
       (callee saves) so that the call to the IDC doesn't trash
       it. This will cause bnd to be flushed out, and reloaded later */

    SaveArgp(f);

    /* Argp is definitely now in ebx */

    /* lea argpoffset(ebx), eax */
    AddInst2Disp(f, 0x8d, 0x03, f->argp_offset);
    /* pushl eax */
    AddInst1(f, 0x50);

    switch(tc) {
    case uint64_t__code:
    case float64_t__code:
	f->argp_offset += 8;
	break;
	
    case uint32_t__code:
    case float32_t__code:
    case bool_t__code:
	f->argp_offset += 4;
	break;

    case uint16_t__code:
	f->argp_offset += 2;
	break;

    case uint8_t__code:
	f->argp_offset += 1;
	break;

    default:
	ASSERT(False);
	break;
    }

    UnmarshalViaStackTopIDC(f, tc);

}    

void MarshalString(synth_func *f, bool_t indirect) {

    word_t src;
    TRCSYN(eprintf("MarshalString(%p, %u)\n", f, indirect));

    ASSERT(f->status & IS_MARSHALLING);
    ASSERT(!(f->status & IS_SERVER));

    /* Flush the value of bd->ptr */
    SaveBUFFP(f);

    if(indirect) {
	ASSERT(f->status & (ARGP_IN_EBX|ARGP_IN_EDX));
	/* We need to make sure we are correctly aligned on the parameter */
    
	ASSERT(3 <= f->argp_align);
    
	f->argp_offset = TROUNDUP(f->argp_offset, string_t);
	
	/* If argp is currently in edx, we want to save it into ebx
	   (callee saves) so that the call to the IDC doesn't trash
	   it. This will cause bnd to be flushed out, and reloaded later */
	
	SaveArgp(f);
	
	/* Argp is definitely now in ebx */

	/* movl argpoffset(ebx), eax */
	AddInst2Disp(f, 0x8b, 0x03, f->argp_offset);
	/* pushl eax */
	AddInst1(f, 0x50);
	f->argp_offset += sizeof(string_t);
	
    } else {
	/* Push source param on to stack */
	/* pushl arginfo(ebp) */
	AddInst2Disp(f, 0xff, 0x35, f->arginfo);
	f->arginfo += 4;
    }

    /* push bd on to stack */
    /* pushl esi */
    AddInst1(f, 0x56);
    AddCall(f, marshalString);
    /* addl 0x08, esp */
    AddInst3(f, 0x83, 0xc4, 0x08);

    /* Check the return code */
    /* cmpl res:eax, 0 */
    AddInst1(f, 0x3d);
    AddImm32(f, 0);

    /* If return code is OK, jump round call to RaiseIDCFailure() */
    /* jne ??? */
    AddInst1(f, 0x75);
    src = f->offset;
    AddInst1(f, 0xff);

    AddInst1(f, 0xcc);
    AddCall(f, RaiseIDCFailure);
    
    /* Finish the target of the jump */
    AddReloc(f, src, f->offset, 8, True);
}

void UnmarshalString(synth_func *f, bool_t indirect) {

    word_t src;
    TRCSYN(eprintf("UnmarshalString(%p, %u)\n", f, indirect));

    ASSERT(!(f->status & IS_MARSHALLING));
    ASSERT(f->status & IS_SERVER);

    /* Flush bd->ptr */
    SaveBUFFP(f);

    /* Push (Heap_clp) NULL */
    AddInst2(f, 0x6a, 0x00);
    /* Push False */
    AddInst2(f, 0x6a, 0x00);

    if(indirect) {
	ASSERT(f->status & (ARGP_IN_EBX|ARGP_IN_EDX));
	/* We need to make sure we are correctly aligned on the parameter */
	
	ASSERT(3 <= f->argp_align);
    
	f->argp_offset = (f->argp_offset + 3) & ~3;
	
	/* If argp is currently in edx, we want to save it into ebx
	   (callee saves) so that the call to the IDC doesn't trash
	   it. This will cause bnd to be flushed out, and reloaded later */
	
	SaveArgp(f);
	
	/* Argp is definitely now in ebx */

	/* Note crucial difference with equivalent code in
           MarshalString : we do an lea here, rather than a move */

	/* lea argpoffset(ebx), eax */
	AddInst2Disp(f, 0x8d, 0x03, f->argp_offset);
	/* pushl eax */
	AddInst1(f, 0x50);
	f->argp_offset += 4;
    } else {
	/* Push address of source param on to stack */
	/* lea arginfo(ebp), eax */
	AddInst2Disp(f, 0x8d, 0x05, f->arginfo);
	/* pushl eax */
	AddInst1(f, 0x50);
	f->arginfo += 4;
    }

    /* Push bd on to stack */
    /* pushl esi */
    AddInst1(f, 0x56);
    /* Call generic string routine */
    AddCall(f, unmarshalString);
    /* addl 0x10, esp */
    AddInst3(f, 0x83, 0xc4, 0x10);
    
    /* Check the return code */    /* cmpl res:eax, 0 */
    AddInst1(f, 0x3d);
    AddImm32(f, 0);

    /* If return code is OK, jump round call to RaiseIDCFailure() */
    /* jne ??? */
    AddInst1(f, 0x75);
    src = f->offset;
    AddInst1(f, 0xff);

    AddInst1(f, 0xcc);
    AddCall(f, RaiseIDCFailure);
    
    /* Finish the target of the jump */
    AddReloc(f, src, f->offset, 8, True);
}
