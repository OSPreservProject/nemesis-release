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
**      mod/nemesis/sgen/synthgen.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Dynamically generates code for stubs, for simple operations
**      (ones requiring no memory allocation)
** 
** ID : $Id: synthgen.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
** 
*/

#define DO_ASSERTS

#include "stubgen.h"

#include "environment.h"
/* Helper function for synthesized stubs */

void RaiseIDCFailure(void) {
    
    RAISE_IDC$Failure();
}


#define NEXT_INSTRUCTION()			\
do {						\
    code += 1 + LARGE_INST(inst);		\
    inst = *code;				\
    if(LARGE_INST(inst)) {			\
	datasize = code[1];			\
    } else {					\
	datasize = DATASIZE(inst);		\
    }						\
} while (0)

#define CODE(_ctype) _ctype##__code

#define Boolean__ctype__code         bool_t__code
#define ShortCardinal__ctype__code   uint16_t__code
#define Cardinal__ctype__code        uint32_t__code
#define LongCardinal__ctype__code    uint64_t__code
#define ShortInteger__ctype__code    int16_t__code
#define Integer__ctype__code         int32_t__code
#define LongInteger__ctype__code     int64_t__code
#define Real__ctype__code            float32_t__code
#define LongReal__ctype__code        float64_t__code
#define Octet__ctype__code           uint8_t__code
#define Char__ctype__code            int8_t__code
#define Address__ctype__code         addr_t__code
#define Word__ctype__code            word_t__code


#define IN_PARM(_type)					\
if(USESIDC(inst)) {					\
    MarshalArgIDC(client, CODE(_type##__ctype));	\
    UnmarshalArgIDC(server, CODE(_type##__ctype));	\
} else {						\
    MarshalArg(client, sizeof(_type##__ctype));		\
    UnmarshalArg(server, sizeof(_type##__ctype));	\
}

#define RESULT_PARM(_type)						    \
if(USESIDC(inst)) {							    \
    if(first_result) {							    \
	UnmarshalRetIDC(client, CODE(_type##__ctype));			    \
	MarshalRetIDC(server, CODE(_type##__ctype));			    \
    } else {								    \
	StartIndirect(client, ALIGNVAL(inst), 0);			    \
	UnmarshalIndirectIDC(client, ALIGNVAL(inst), CODE(_type##__ctype)); \
	StartIndirect(server, 7, sizeof(_type##__ctype));		    \
	MarshalIndirectIDC(server, 7, CODE(_type##__ctype));		    \
    }									    \
} else {								    \
    if(first_result) {							    \
	UnmarshalRet(client, sizeof(_type##__ctype));			    \
	MarshalRet(server, sizeof(_type##__ctype));			    \
    } else {								    \
	StartIndirect(client, ALIGNVAL(inst), 0);			    \
	UnmarshalIndirect(client, ALIGNVAL(inst), sizeof(_type##__ctype));  \
	StartIndirect(server, 7, sizeof(_type##__ctype));		    \
	MarshalIndirect(server, 7, sizeof(_type##__ctype));		    \
    }									    \
}

#define OUTGOING(_type)							 \
if(USESIDC(inst)) {							 \
    MarshalIndirectIDC(client, ALIGNVAL(inst), CODE(_type##__ctype));	 \
    UnmarshalIndirectIDC(server, ALIGNVAL(inst), CODE(_type##__ctype));	 \
} else {								 \
    MarshalIndirect(client, ALIGNVAL(inst), sizeof(_type##__ctype));	 \
    UnmarshalIndirect(server, ALIGNVAL(inst), sizeof(_type##__ctype));	 \
}									 \

#define RETURNING(_type)						\
if(USESIDC(inst)) {							\
    UnmarshalIndirectIDC(client, ALIGNVAL(inst), CODE(_type##__ctype));	\
    MarshalIndirectIDC(server, ALIGNVAL(inst), CODE(_type##__ctype));	\
} else {								\
    UnmarshalIndirect(client, ALIGNVAL(inst), sizeof(_type##__ctype));	\
    MarshalIndirect(server, ALIGNVAL(inst), sizeof(_type##__ctype));	\
}									

bool_t SynthesiseFunctions(inst_t *codebase, word_t args, 
			   client_fn **client_code, server_fn **server_code) {
    
    synth_func *server, *client;
    inst_t inst, *code = codebase;
    word_t datasize;
    bool_t first_result = False, done_first_result = False;

    TRCSYN(pretty_print_prog(codebase));
    
    client = StartClient(args, code[0], code[2]);
    server = StartServer(code[1], args, code[0], code[2]);

    code += 3;

    /* Loop until we get to the end marker */

    inst = *code;

    if(LARGE_INST(inst)) {
        datasize = code[1];
    } else {
        datasize = DATASIZE(inst);
    } 

    while(!EOP(inst)) {
	
	/* This is the the start of a new parameter */

	ASSERT(SOP(inst));

	if(VALREF(inst) == BYVAL) {
	    
	    ASSERT(MODE(inst) == MODE_IN || MODE(inst) == MODE_RESULT);

	    if(MODE(inst) == MODE_IN) {

		switch(BASETYPE(inst)) {
		    
		case TYPE_INT8:
		    IN_PARM(Octet);
		    break;
		    
		case TYPE_INT16:
		    IN_PARM(ShortCardinal);
		    break;
		    
		case TYPE_INT32:
		    IN_PARM(Cardinal);
		    break;
		    
		case TYPE_INT64:
		    IN_PARM(LongCardinal);
		    break;
		    
		case TYPE_FLT32:
		    IN_PARM(Real);
		    break;
		    
		case TYPE_FLT64:
		    IN_PARM(LongReal);
		    break;
		    
		case TYPE_BOOL:
		    IN_PARM(Boolean);
		    break;
		    
		case TYPE_STRING:
		    ASSERT(!USESIDC(inst));
		    MarshalString(client, False);
		    UnmarshalString(server, False);
		    break;

		default:
		    ASSERT(False);
		    break;
		}
	    } else /* MODE RETURN */ {

		if(!first_result) {
		    switch(BASETYPE(inst)) {
			
		    case TYPE_INT8:
		    case TYPE_INT16:
		    case TYPE_INT32:
		    case TYPE_FLT32:
			SkipArg(client, 4, 0);
			SkipArg(server, 4, 8);
			break;

		    case TYPE_INT64:
		    case TYPE_FLT64:
			SkipArg(client, 8, 0);
			SkipArg(server, 8, 8);
			break;

		    default:
			ASSERT(False);
			break;
		    }
		}
	    }

	    NEXT_INSTRUCTION();

	} else /* BYREF */ {
	    
	    bool_t isModeIn = False;

	    switch(MODE(inst)) {
	    case MODE_IN:
		if(FORMAT(inst) == FORMAT_COPY) {
		    /* If this is a bulk-copied IN parameter, we can
                       'unmarshal' it in place ... */
		    StartIndirect(client, ALIGNVAL(inst), 0);
		    MarshalIndirect(client, ALIGNVAL(inst), datasize);
                    UnmarshalArgInPlace(server, datasize);
		    NEXT_INSTRUCTION();
		    break;
		}
		isModeIn = True;
		/* Otherwise, fall through to normal case */

	    case MODE_INOUT:
		StartIndirect(client, ALIGNVAL(inst), 0);
		StartIndirect(server, ALIGNVAL(inst), datasize);
		
		do {
		    ASSERT(!(SOS(inst) && !(FORMAT(inst) == FORMAT_COPY)));
		    ASSERT(!SOC(inst));

		    if(FORMAT(inst) == FORMAT_COPY) {
			word_t align = ALIGNVAL(inst);
			ASSERT(!USESIDC(inst));
			MarshalIndirect(client, align, datasize);
			UnmarshalIndirect(server, align, datasize);
		    } else /* FORMAT_STRUCT */ {
			
			switch(BASETYPE(inst)) {
			case TYPE_ALLOCSIZE:
			    /* Ignore */
			    break;

			case TYPE_INT8:
			    OUTGOING(Octet);
			    break;

			case TYPE_INT16:
			    OUTGOING(ShortCardinal);
			    break;

			case TYPE_INT32:
			    OUTGOING(Cardinal);
			    break;
			    
			case TYPE_INT64:
			    OUTGOING(LongCardinal);
			    break;

			case TYPE_FLT32:
			    OUTGOING(Real);
			    break;

			case TYPE_FLT64:
			    OUTGOING(LongReal);
			    break;

			case TYPE_BOOL:
			    OUTGOING(Boolean);
			    break;

			case TYPE_STRING:
			    ASSERT(!USESIDC(inst));
			    ASSERT(isModeIn);
			    MarshalString(client, True);
			    UnmarshalString(server, True);
			    break;

			default:
			    ASSERT(False);
			    break;
			}
		    }

		    NEXT_INSTRUCTION();

		} while (!SOP(inst));
		break;

	    case MODE_OUT:
		SkipArg(client, sizeof(addr_t), 0);
		SkipArg(server, sizeof(addr_t), datasize);
		
		do {
		    NEXT_INSTRUCTION();
		} while(!SOP(inst));

		break;

	    case MODE_RESULT:
		ASSERT(False);
		break;

	    }
	    
	}
	    
    }

    DoClientCall(client);
    DoServerInvocation(server);

    code = codebase + 3;

    inst = *code;

    if(LARGE_INST(inst)) {
        datasize = code[1];
    } else {
        datasize = DATASIZE(inst);
    } 

    while(!EOP(inst)) {
	
	/* This is the the start of a new parameter */

	ASSERT(SOP(inst));

	if(MODE(inst) == MODE_RESULT) {
	    first_result = !done_first_result;
	    done_first_result = True;
	} else {
	    first_result = False;
	}

	if(VALREF(inst) == BYVAL) {

	    ASSERT(MODE(inst) == MODE_IN || MODE(inst) == MODE_RESULT);

	    if(MODE(inst) == MODE_IN) {

		/* Skip IN params on return */

		switch(BASETYPE(inst)) {
		case TYPE_INT8:
		case TYPE_INT16:
		case TYPE_INT32:
		case TYPE_FLT32:
		    SkipArg(client, 4, 0);
		    SkipArg(server, 4, 0);
		    break;
		    
		case TYPE_INT64:
		case TYPE_FLT64:
		    SkipArg(client, 8, 0);
		    SkipArg(server, 8, 0);
		    break;

		case TYPE_STRING:
		    ASSERT(!USESIDC(inst));
		    SkipArg(client, sizeof(string_t), 0);
		    SkipArg(server, sizeof(string_t), 0);
		    break;

		default:
		    ASSERT(False);
		    break;
		}
	    } else /* MODE RETURN */ {

		switch(BASETYPE(inst)) {
		    
		case TYPE_INT8:
		    RESULT_PARM(Octet);
		    break;
		    
		case TYPE_INT16:
		    RESULT_PARM(ShortCardinal);
		    break;
		    
		case TYPE_INT32:
		    RESULT_PARM(Cardinal);
		    break;
		    
		case TYPE_INT64:
		    RESULT_PARM(LongCardinal);
		    break;
		    
		case TYPE_FLT32:
		    RESULT_PARM(Real);
		    break;
		    
		case TYPE_FLT64:
		    RESULT_PARM(LongReal);
		    break;
		    
		case TYPE_BOOL:
		    RESULT_PARM(Boolean);
		    break;
		    
		default:
		    ASSERT(False);
		    break;
		
		}		    
	    }

	    NEXT_INSTRUCTION();

	} else /* MODE BYREF */ {
	    
	    switch(MODE(inst)) {
	    case MODE_IN:

		/* Ignore this argument when returning. For the
                   server, if the argument was not referenced in
                   place, we need to skip the argument in the static
                   data area too */
		SkipArg(client, sizeof(addr_t), 0);
		if(FORMAT(inst) == FORMAT_COPY) {
		    SkipArg(server, sizeof(addr_t), 0);
		} else {
		    SkipArg(server, sizeof(addr_t), datasize);
		}
		
		do {
		    NEXT_INSTRUCTION();
		} while(!SOP(inst));
		
		break;
		
	    case MODE_INOUT:
	    case MODE_OUT:
		StartIndirect(client, ALIGNVAL(inst), 0);
		StartIndirect(server, ALIGNVAL(inst), datasize);
		
		do {
		    ASSERT(!SOS(inst));
		    ASSERT(!SOC(inst));
		    
		    if(FORMAT(inst) == FORMAT_COPY) {
			ASSERT(!USESIDC(inst));
			UnmarshalIndirect(client, ALIGNVAL(inst), datasize);
			MarshalIndirect(server, ALIGNVAL(inst), datasize);
		    } else /* FORMAT_STRUCT */ {
			
			switch(BASETYPE(inst)) {
			case TYPE_ALLOCSIZE:
			    /* Ignore */
			    break;
			    
			case TYPE_INT8:
			    RETURNING(Octet);
			    break;
			    
			case TYPE_INT16:
			    RETURNING(ShortCardinal);
			    break;
			    
			case TYPE_INT32:
			    RETURNING(Cardinal);
			    break;
			    
			case TYPE_INT64:
			    RETURNING(LongCardinal);
			    break;
			    
			case TYPE_FLT32:
			    RETURNING(Real);
			    break;
			    
			case TYPE_FLT64:
			    RETURNING(LongReal);
			    break;
			    
			case TYPE_BOOL:
			    RETURNING(Boolean);
			    break;
			    
			default:
			    ASSERT(False);
			    break;
			}
		    }
		    
		    NEXT_INSTRUCTION();
		    
		} while (!SOP(inst));
		break;
		
	    case MODE_RESULT:
		ASSERT(False);
		break;
		
	    }
	    
	}
	
    }

    *server_code = FinishServer(server);
    *client_code = FinishClient(client);

    return True;
}
	    
		    
