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
**      mod/nemesis/sgen/engine.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Byte code interpreter for dynamic stubs
** 
** ENVIRONMENT: 
**
**      Shared library/Nemesis domain
** 
** ID : $Id: engine.c 1.3 Tue, 04 May 1999 18:45:45 +0100 dr10009 $
** 
**
*/


#include<stubgen.h>


#define NEXT_INSTRUCTION()				\
do {							\
    code += 1 + LARGE_INST(inst);			\
    if((!fast) && (FORMAT(inst) == FORMAT_STRUCT)	\
       && (BASETYPE(inst) == TYPE_CUSTOM)) code += 2;	\
    inst = *code;					\
    if(LARGE_INST(inst)) {				\
        datasize = code[1];				\
    } else {						\
        datasize = DATASIZE(inst);			\
    }							\
} while(0)



#define MAX_SEQS 10

typedef struct {
    inst_t *marker;
    addr_t   argp;
    uint32_t nelems;
    Type_Code type;
} seq_stack_elem;


#define OUT_OF_SPACE() { eprintf("Out of space (line %d) !!!\n", __LINE__); if(!((!call)&&marshal)) return False; bd->ptr = NULL; bd->space = 0; }

#define DO_MARSHAL(_type, _argp)					     \
if(fast || call || (!marshal) || (bd->ptr != NULL)) {			     \
    if((!fast) && USESIDC(inst)) {					     \
        _type##__ctype *_tmp = (_type##__ctype *) (_argp);		     \
        TRCMUF(eprintf("Calling IDC to marshal " #_type " : %" _type##__spec \
                        " from %p to %p\n", *_tmp, _tmp, bd->ptr));	     \
        if(!IDC$m##_type(idc, bd, *_tmp)) {OUT_OF_SPACE();};		     \
    } else {								     \
        if(fast || (bd->space >= 8)) {					     \
	    _type##__ctype *_tmp = (_type##__ctype *) (_argp);		     \
            ASSERT(!USESIDC(inst));					     \
            TRCMUF(eprintf("Marshalling " #_type " : %" _type##__spec	     \
	       	           " from %p to %p\n", *_tmp, _tmp, bd->ptr));	     \
            (*(_type##__ctype *)(((uint64_t *)(bd->ptr))++)) = *_tmp;	     \
            bd->space -= 8;						     \
        } else {							     \
            OUT_OF_SPACE();						     \
        }								     \
    }									     \
} else {								     \
    eprintf("Buffer overflowed, not marshalling\n");			     \
}

#define DO_UNMARSHAL(_type, _argp)					    \
if(fast || call || (!marshal) || (bd->ptr != NULL)) {			    \
    if((!fast) && USESIDC(inst)) {					    \
        _type##__ctype *_tmp = (_type##__ctype *) (_argp);		    \
	TRCMUF(eprintf("Calling IDC to unmarshal "#_type" from %p to %p\n", \
		       bd->ptr, _tmp));					    \
        if(!IDC$u##_type(idc, bd, _tmp)) {OUT_OF_SPACE();};		    \
        TRCMUF(eprintf("Called IDC to unmarshal " #_type		    \
                       " : %" _type##__spec "\n", *_tmp));		    \
    } else {								    \
        _type##__ctype *_tmp = (_type##__ctype *) (_argp);		    \
        ASSERT(!USESIDC(inst));						    \
	if(fast || (bd->space >= 8)) {					    \
	    TRCMUF(eprintf("Unmarshalling "#_type" from %p to %p\n",	    \
		           bd->ptr, _tmp));				    \
            *_tmp = *((_type##__ctype *)(((uint64_t *)(bd->ptr))++));	    \
            TRCMUF(eprintf("Unmarshalled "#_type" : %"			    \
		           _type##__spec "\n", *_tmp));			    \
	    bd->space -= 8;						    \
        } else {							    \
	    OUT_OF_SPACE();						    \
	}								    \
    }									    \
} else {								    \
    eprintf("Buffer overflowed, not unmarshalling\n");			    \
}


#define DO_COPY(_type, _argp)			\
if(marshal) {					\
    DO_MARSHAL(_type, _argp);			\
} else {					\
    DO_UNMARSHAL(_type, _argp);			\
}

	
#define DO_COPY_BULK(_type)						\
if(marshal) {								\
    TRCMUF(eprintf("Copying "#_type" : %p from %p to %p\n", 		\
		   (addr_t)(word_t)*(_type *)argp, argp, bd->ptr));	\
    *((_type *)bd->ptr)++ = *((_type *)argp)++;				\
} else {								\
    TRCMUF(eprintf("Copying "#_type" : %p from %p to %p\n", 		\
		   (addr_t)(word_t)*(_type *)bd->ptr,			\
		   bd->ptr, argp));					\
    *((_type *)argp)++ = *((_type *)bd->ptr)++;				\
}

#define DO_COPY_8()  DO_COPY_BULK(uint8_t)
#define DO_COPY_16() DO_COPY_BULK(uint16_t)
#define DO_COPY_32() DO_COPY_BULK(uint32_t)
#define DO_COPY_64() DO_COPY_BULK(uint64_t)

#define ADD_POINTER(_seq, _ptr)	addPointer((_seq), (_ptr))

MUF_INLINE  bool_t marshalString(IDC_BufferDesc bd, string_t s) { 
    
    word_t len;
#ifdef DEBUG_MUF
    bool_t debugmuf = False; /* Debugging is off by default */
#endif
    len = (s) ? strlen(s) + 1 : 0;

    if(bd->space < len + sizeof(uint64_t)) {
	DB(eprintf("No space to marshal string of length %u\n", len));
	return False;
    } else {
	
	bd->space -= len + sizeof(uint64_t);

	*((uint64_t *)(bd->ptr))++ = len;
	
	if(len) {
	    TRCMUF(eprintf("Marshalling string %p '%s' to %p\n", (addr_t) s, s, bd->ptr));
	    strcpy(bd->ptr, s);
	    bd->ptr += TROUNDUP(len, uint64_t);
	} else {
	    TRCMUF(eprintf("Marshalling NULL string\n"));
	}
    }		
    TRCMUF(eprintf("Marshalled string successfully\n"));
    return True;
}

MUF_INLINE  bool_t unmarshalString(IDC_BufferDesc bd,
				   string_t *res,
				   bool_t doCopy,
				   Heap_clp h) {

    word_t len;
#ifdef DEBUG_MUF
    bool_t debugmuf = False; /* Debugging is off by default */
#endif

    if(bd->space < sizeof(uint64_t)) {
	DB(eprintf("No space to unmarshal string len\n"));
	*res = NULL;
	return False;
    } else {
	len = *((uint64_t *)(bd->ptr))++;
	bd->space -= sizeof(uint64_t);

	if(len) {
	    if(bd->space < len) {
		DB(eprintf("No space to unmarshal %u chars\n", len));
		*res = NULL;
		return False;
	    } else {
		if(doCopy) {
		    /* We have a heap to copy into */
		    if(!(*res = Heap$Malloc(h, len))) return False;

		    memcpy(*res, bd->ptr, len);
		    (*res)[len - 1] = 0;
		} else {
		    /* We have no heap - leave the string in place */
		    if(((uint8_t *)bd->ptr)[len - 1] != 0) {
			DB(eprintf("Badly formed string %p, len %u\n", 
				   bd->ptr, len));
			ntsc_dbgstop();
			return False;
		    }
		    *res = (string_t) bd->ptr;
		}
		bd->ptr += TROUNDUP(len, uint64_t);
		bd->space -= TROUNDUP(len, uint64_t);
		
		TRCMUF(eprintf("Unmarshalled string '%s'\n", *res));
	    }
	} else {
	    TRCMUF(eprintf("Unmarshalling NULL string\n"));
	    *res = NULL;
	}
    }
    return True;
}

MUF_INLINE bool_t marshalBulkSequence(IDC_BufferDesc bd,
				      IDC_AddrSeq *s,
				      uint32_t elemsize) {
    
    uint32_t len = s->len;
    uint32_t datalen = TROUNDUP(s->len * elemsize, uint64_t);
    if(bd->space < datalen + 8) {
	DB(eprintf("Stubgen: No room to marshal sequence\n"));
	return False;
    }
    *(uint32_t *)bd->ptr = len;
    memcpy(bd->ptr + 8, s->data, datalen);
    bd->ptr += datalen + 8;
    bd->space -= datalen + 8;
    return True;
}

MUF_INLINE bool_t unmarshalBulkSequence(IDC_BufferDesc bd,
					IDC_AddrSeq *s,
					uint32_t elemsize) {

    uint32_t len, datalen;
    if(bd->space < 8) {
	DB(eprintf("Stubgen: No room to unmarshal sequence length\n"));
	return False;
    }
    len = *(uint32_t *)bd->ptr;
    datalen = TROUNDUP(len * elemsize, uint64_t);
    if(bd->space < datalen + 8) {
	DB(eprintf("Stubgen: No room to unmarshal sequence data\n"));
	return False;
    }
    s->base = s->data = bd->ptr + 8;
    s->len = s->blen = len;
    
    bd->ptr += datalen + 8;
    bd->space -= datalen + 8;
    return True;
}

   
static bool_t marshalAny(IDC_clp idc, IDC_BufferDesc bd, 
			 Type_Any *any, bool_t doFree) {
    
#if 0
    StubGen_clp sgen = NAME_FIND("modules>StubGen", StubGen_clp);
    StubGen_Code NOCLOBBER *code;

    TRY {
	code = StubGen$GenType(sgen, any->type, NULL, Pvs(heap));
    } CATCH_StubGen$Failure() {
	DB(eprintf("Couldn't get marshalling code!\n"));
	code = NULL;
    } ENDTRY;
    
    if(!code) {
	return False;
    }
    
    if(bd->space < 8) {
	DB(eprintf("StubGen: No room to marshal ANY typecode\n"));
	return False;
    }

    *((uint64_t *)(bd->ptr))++ = any->type;
    bd->space -= 8;
    
    return StubGen$Marshal(sgen, code, idc, bd, &any->val, doFree);
#endif
    return False;
}

static bool_t unmarshalAny(IDC_clp idc, IDC_BufferDesc bd,
			   Type_Any *any, IDC_AddrSeq *mlist) {
#if 0
    StubGen_clp sgen = NAME_FIND("modules>StubGen", StubGen_clp);
    StubGen_Code NOCLOBBER *code;

    if(bd->space < 8) {
	DB(eprintf("StubGen: No room to unmarshal ANY typecode\n"));
	return False;
    }

    any->type = *((uint64_t *)(bd->ptr))++;
    bd->space -= 8;

    TRY {
	code = StubGen$GenType(sgen, any->type, NULL, Pvs(heap));
    } CATCH_StubGen$Failure() {
	DB(eprintf("Couldn't get marshalling code!\n"));
	code = NULL;
    } ENDTRY;

    if(!code) return False;

    return StubGen$Unmarshal(sgen, code, idc, bd, &any->val, mlist);
#endif
    return False;
}

#ifdef DEBUG_MUF
const char nibble_table[] = { '0', '1', '2', '3', '4', '5', '6', '7', 
			'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

static void memdump(uint8_t *addr, int len) {
    char buffer[65];
    int bpos = 0;

    while(len --) {
	buffer[bpos] = nibble_table[((*addr) >> 4) & 0xf];
	buffer[bpos+1] = nibble_table[(*addr) & 0xf];
	bpos += 2;
	addr ++;
	
	if((bpos == 64) || (len == 0)) {
	    buffer[bpos] = 0;
	    eprintf("%s\n", buffer);
	    bpos = 0;
	}
    }
}
#endif

static void addPointer(IDC_AddrSeq *seq, addr_t ptr) {
    if(!seq->base) {
	SEQ_INIT(seq, 0, Pvs(heap));
    }
    SEQ_ADDH(seq, ptr);
}

MUF_INLINE static bool_t Generic_MUF(IDC_clp idc,
				     IDC_BufferDesc NOCLOBBER bd,
				     inst_t * NOCLOBBER code,
				     stack_frame_t *sf, 
				     addr_t NOCLOBBER staticp, 
				     IDC_AddrSeq * NOCLOBBER inmallocs,
				     IDC_AddrSeq * NOCLOBBER outmallocs, 
				     bool_t call,
				     bool_t marshal,
				     bool_t NOCLOBBER fast,
#ifdef DEBUG_MUF
				     bool_t debugmuf,
#endif
				     uint64_t * NOCLOBBER result) {
    inst_t NOCLOBBER inst;
    uint32_t NOCLOBBER datasize;

    seq_stack_elem seq_stack[MAX_SEQS];
    int NOCLOBBER sp = 0;
    bool_t NOCLOBBER sos = False;
    uint32_t num_seq_elems = 0;
    addr_t argp;
    bool_t NOCLOBBER done_first_result = False;
    bool_t NOCLOBBER first_result = False;
    Heap_clp NOCLOBBER h = bd->heap;

    /* First and third 'instructions' are actually sizes of static
       data on the wire for call and return respectively (second
       instruction is size of static data at server) */

    if(call) {
        datasize = code[0];
    } else {
	datasize = code[2];
    }
    
    {
	addr_t basep;

	/* Ensure first parametyer is 8-byte aligned */
	basep = (addr_t)TROUNDUP((pointerval_t)bd->ptr, uint64_t);
	bd->space -= basep - bd->ptr;
	bd->ptr = basep;
    }

    TRCMUF(eprintf("Buffer space = %u, datasize = %u\n", bd->space, datasize));

    if(bd->space < datasize) {
	DB(eprintf("Buffer too small for static data!\n"));
	return False;
    }

    bd->space -= datasize;

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

	TRCMUF(eprintf("\n"));
	TRCMUF(eprintf("Start of parameter: code = %p, bufp = %p "
		       "(base %p), sf = %p\n", 
		       code, bd->ptr, bd->base, sf));

	TRCMUF(pretty_print_inst(code));

	if(MODE(inst) == MODE_RESULT) {
	    first_result = !done_first_result;
	    done_first_result = True;
	} else {
	    first_result = False;
	}
   
	if(VALREF(inst) == BYVAL) {
	    
	    ASSERT(MODE(inst) == MODE_IN || MODE(inst) == MODE_RESULT);
	    
	    TRCMUF(eprintf("By Value\n"));
	
	    if(MODE(inst) == MODE_IN) {
		
		TRCMUF(eprintf("IN parameter\n"));

		/* IN parameter */
		
		if(call) {
		    
		    if(marshal) {
			
			/* Marshalling IN parameter by value */

			TRCMUF(eprintf("Marshalling IN parameter by value\n"));

			switch(BASETYPE(inst)) {

			case TYPE_ALLOCSIZE:
			{
			    ASSERT(False);
			    break;
			}

			case TYPE_INT8:
			{
			    DO_MARSHAL(Octet, SKIP_INT32_ARG(*sf));
			    break;
			}

			case TYPE_INT16:
			{
			    DO_MARSHAL(ShortCardinal, SKIP_INT32_ARG(*sf));
			    break;
			}

			case TYPE_INT32:
			    
			{
			    DO_MARSHAL(Cardinal, SKIP_INT32_ARG(*sf));
			    break;
			}
			case TYPE_INT64:
			{
			    DO_MARSHAL(LongCardinal, SKIP_INT64_ARG(*sf));
			    break;
			}
			case TYPE_FLT32:
			{
			    DO_MARSHAL(Real, SKIP_FLOAT32_ARG(*sf));
			    break;
			}
			case TYPE_FLT64:
			{
			    DO_MARSHAL(LongReal, SKIP_FLOAT64_ARG(*sf));
			    break;
			}
			case TYPE_STRING:
			{
			    ASSERT(!(fast && USESIDC(inst)));
			    if(bd->ptr != NULL) {
				string_t tmp;
				
				GET_PTR_ARG(*sf, &tmp);
				    
				if((!fast) && USESIDC(inst)) {
				    if(!IDC$mString(idc, bd, tmp)) {
					OUT_OF_SPACE();
				    } 
				} else {
				    if(!marshalString(bd, tmp)) {
					OUT_OF_SPACE();
				    }
				}
			    } else {
				eprintf("Buffer overflowed - ignoring\n");
			    }
			    break;
			}
			
			case TYPE_BOOL:
			{
			    DO_MARSHAL(Boolean, SKIP_INT32_ARG(*sf));
			    break;
			}

			case TYPE_CUSTOM:
			{
			    if(!fast) {
				Type_Code tc;

				if(LARGE_INST(inst)) {
				    tc = code[2] | ((uint64_t)(code[3]) << 32);
				} else {
				    tc = code[1] | ((uint64_t)(code[2]) << 32);
				} 

				TRCMUF(eprintf(
				    "Marshalling custom IN parm (%qx)\n",
				    tc));

				
				if(bd->ptr != NULL) {
				    addr_t argp = NULL;
				    if(datasize <= 4) {
					argp = SKIP_INT32_ARG(*sf);
				    } else if (datasize == 8) {
					argp = SKIP_INT64_ARG(*sf);
				    } else {
					ASSERT(False);
				    }

				    if(!IDC$mCustom(idc, bd, tc, argp, False)) {
					OUT_OF_SPACE();
				    }
				}
				
			    } else {
				ASSERT(False);
			    }
			    break;
			}
			default:
			    ASSERT(False);
			    break;

			}
	
		    } else {
			
			TRCMUF(eprintf("Unmarshalling IN parm by value\n"));

			/* Unmarshalling IN parameter by value */
			
			switch(BASETYPE(inst)) {

			case TYPE_ALLOCSIZE:
			    ASSERT(False);
			    break;

			case TYPE_INT8:
			    DO_UNMARSHAL(Octet, SKIP_INT32_ARG(*sf));
			    break;

			case TYPE_INT16:
			    DO_UNMARSHAL(ShortCardinal, SKIP_INT32_ARG(*sf));
			    break;

			case TYPE_INT32:
			    DO_UNMARSHAL(Cardinal, SKIP_INT32_ARG(*sf));
			    break;

			case TYPE_INT64:
			    DO_UNMARSHAL(LongCardinal, SKIP_INT64_ARG(*sf));
			    break;

			case TYPE_FLT32:
			    DO_UNMARSHAL(Real, SKIP_FLOAT32_ARG(*sf));
			    break;

			case TYPE_FLT64:
			    DO_UNMARSHAL(LongReal, SKIP_FLOAT64_ARG(*sf));
			    break;

			case TYPE_STRING:
			{
			    ASSERT(!(fast && USESIDC(inst)));
			    if(bd->ptr != NULL) {
				string_t tmp;

				if(USESIDC(inst)) {
				    if(!IDC$uString(idc, bd, &tmp)) {
					tmp = NULL;
					OUT_OF_SPACE();
				    } else {
					if(tmp) {
					    ADD_POINTER(inmallocs, tmp);
					}
				    }
				} else {
				    if(!unmarshalString(bd, 
							&tmp,
							False,
							NULL)) {
					OUT_OF_SPACE();
				    }
				    
				}
				
				SET_PTR_ARG(*sf, tmp);
				    
			    } else {
				eprintf("Bufffer overflowed, ignoring\n");
			    }
			    break;
			}

			case TYPE_BOOL:
			    DO_UNMARSHAL(Boolean, SKIP_INT32_ARG(*sf));
			    break;

			case TYPE_CUSTOM:
			{
			    if(!fast) {
				Type_Code tc;

				if(LARGE_INST(inst)) {
				    tc = code[2] | ((uint64_t)(code[3]) << 32);
				} else {
				    tc = code[1] | ((uint64_t)(code[2]) << 32);
				} 

				TRCMUF(eprintf(
				    "Unmarshalling custom IN  parm (%qx)\n",
				    tc));

				if(bd->ptr != NULL) {

				    addr_t argp = NULL;
				    if(datasize == 4) {
					argp = SKIP_INT32_ARG(*sf);
				    } else if (datasize == 8) {
					argp = SKIP_INT64_ARG(*sf);
				    } else {
					ASSERT(False);
				    }

				    if(!IDC$uCustom(idc, bd, tc, argp, 
						    inmallocs)) {
					OUT_OF_SPACE();
				    }
				}
			    } else {
				ASSERT(False);
			    }
			    break;
			}
				    
			default:
			    ASSERT(False);
			    break;
			}
		    }
		    
		} else {
		    
		    TRCMUF(eprintf("Ignoring IN parameter on return\n"));

		    /* Ignore byval IN parameters on return */

			
		    switch(BASETYPE(inst)) {
			
		    case TYPE_INT8:
		    case TYPE_INT16:
		    case TYPE_INT32:
			SKIP_INT32_ARG(*sf);
			break;

		    case TYPE_FLT32:
			SKIP_FLOAT32_ARG(*sf);
			break;

		    case TYPE_INT64:
			SKIP_INT64_ARG(*sf);
			break;
			
		    case TYPE_FLT64:
			SKIP_FLOAT64_ARG(*sf);
			break;
			
		    case TYPE_STRING:
			SKIP_PTR_ARG(*sf);	
			break;
			
		    case TYPE_CUSTOM:
			if(datasize == 8) {
			    SKIP_INT64_ARG(*sf);
			} else if(datasize == 4) {
			    SKIP_INT32_ARG(*sf);
			} else {
			    ASSERT(False);
			}
			break;
			
		    default:
			ASSERT(False);
			break;
		    }
		    
		}
		
	    } else { 
		
		/* RESULT parameter */
		
		TRCMUF(eprintf("RESULT parameter\n"));

		if(call) {
		    
		    if((!first_result) && (!marshal)) {

			/* Allocate space for parameter. The bytecode
                           compiler assumes that small result
                           parameters are all uint64_t's or smaller
                           when calculating space for static data, to
                           save having to have a switch statement here
                           and keep things nicely aligned */

			addr_t buffer = ((uint64_t *)staticp)++;
			TRCMUF(eprintf("Alloced static RESULT parameter: %p\n",
				       buffer));
			SET_PTR_ARG(*sf, buffer);

		    } else {

			/* Ignore RESULT parameter on call marshal*/
			TRCMUF(eprintf("Ignoring RESULT parameter\n"));
			
			/* Don't bother with the SKIP_xxx_ARG, since
                           we won't be using the stack frame after
                           this. 

			   Actually this isn't strictly true - IIOP
			   has the result parameter at the start of
			   the bytecode. But there's only one result
			   allowed, so it will be the return value
			   rather than a by-reference argument. XXX */

			
		    }
			
		} else /* return */ {
		    
		    if(marshal) {

			uint64_t *res;

			if(first_result) {

			    res = result;

			} else {

			    GET_PTR_ARG(*sf, &res);

			}
			TRCMUF(eprintf("Marshalling RESULT %qx by value\n",
				       res));
			    
			/* Marshalling RESULT parameter on return */
			    
			switch(BASETYPE(inst)) {
				
			    /* We might have an int8 or int16
                               parameter here, since results (other
                               than the first) will not be promoted */

			case TYPE_INT8:
			    ASSERT(!first_result);
			    DO_MARSHAL(Octet, (uint8_t *)res);
			    break;

			case TYPE_INT16:
			    ASSERT(!first_result);
			    DO_MARSHAL(ShortCardinal, (uint16_t *)res);
			    break;

			case TYPE_INT32:
			    DO_MARSHAL(Cardinal, (uint32_t *)res);
			    break;
			    
			case TYPE_FLT32:
			    DO_MARSHAL(Real, (float32_t *)res);
			    break;
			    
			case TYPE_INT64:
			    DO_MARSHAL(LongCardinal, (uint64_t *)res);
			    break;
			    
			case TYPE_FLT64:
			    DO_MARSHAL(LongReal, (float64_t *)res);
			    break;
			    
			case TYPE_STRING:
			{
			    if(!fast) {
				if(bd->ptr != NULL) {
				    string_t tmp =(string_t)(pointerval_t)*res;

				    if(USESIDC(inst)) {
					if(!IDC$mString(idc, bd, tmp)) {
					    OUT_OF_SPACE();
					}
				    } else {
					if(!marshalString(bd, tmp)) {
					    OUT_OF_SPACE();
					}
					
					if(tmp) FREE(tmp);
				    }
				} else {
				    eprintf("Buffer overflowed - ignoring\n");
				}
			    } else {
				ASSERT(False);
			    }
			    break;
			}

			case TYPE_CUSTOM:
			{
			    if(!fast) {

				Type_Code tc;

				if(LARGE_INST(inst)) {
				    tc = code[2] | ((uint64_t)(code[3]) << 32);
				} else {
				    tc = code[1] | ((uint64_t)(code[2]) << 32);
				} 

				TRCMUF(eprintf(
				    "Marshalling custom RESULT parm (%qx)\n",
				    tc));

				if(bd->ptr != NULL) {

				    if(!IDC$mCustom(idc, bd, tc, res, True)) {
					OUT_OF_SPACE();
				    }
				}
			    } else {
				ASSERT(False);
			    }
			    break;
			}

			default:
			    ASSERT(False);
			    break;
			    
			}
			
		    } else /* Unmarshalling RESULT on return */ {

			addr_t res;

			if(first_result) {

			    TRCMUF(eprintf("Unmarshalling into result (%p)\n",
				    result));

			    res = result;
			} else {

			    GET_PTR_ARG(*sf, &res);
			    TRCMUF(eprintf("Unmarshalling RESULT param (%p)\n",
				    res));
				    
			}

			/* copy buffer to result; */
			switch(BASETYPE(inst)) {
				
			case TYPE_INT8:
			    ASSERT(!first_result);
			    DO_UNMARSHAL(Octet, (uint8_t *)res);
			    break;

			case TYPE_INT16:
			    ASSERT(!first_result);
			    DO_UNMARSHAL(ShortCardinal, (uint16_t *)res);
			    break;

			case TYPE_INT32:
			    DO_UNMARSHAL(Cardinal, (uint32_t *)res);
			    break;
			    
			case TYPE_FLT32:
			    DO_UNMARSHAL(Real, (float32_t *)res);
			    break;
			    
			case TYPE_INT64:
			    DO_UNMARSHAL(LongCardinal, (uint64_t *)res);
			    break;
			    
			case TYPE_FLT64:
			    DO_UNMARSHAL(LongReal, (float64_t *)res);
			    break;
			    
			case TYPE_STRING:
			{
			    if(!fast) {
				if(bd->ptr != NULL) {
				    string_t *tmp = res;
				
				    if(USESIDC(inst)) {
					if(!IDC$uString(idc, bd, tmp)) {
					    *tmp = NULL;
					    OUT_OF_SPACE();
					}
				    } else {
					if(!unmarshalString(bd, tmp,
							    True, Pvs(heap))) {
					    OUT_OF_SPACE();
					}
					
				    }

				    if(*tmp) {
					ADD_POINTER(outmallocs, *tmp);
				    }

				} else {
				    eprintf("Buffer overflowed, not unmarshalling\n");
				}
			    } else {
				ASSERT(False);
			    }
				
			    break;
			}

			case TYPE_CUSTOM:
			{
			    if(!fast) {
				Type_Code tc;

				if(LARGE_INST(inst)) {
				    tc = code[2] | ((uint64_t)(code[3]) << 32);
				} else {
				    tc = code[1] | ((uint64_t)(code[2]) << 32);
				} 

				TRCMUF(eprintf(
				    "Unmarshalling custom RESULT (%qx)\n",
				    tc));

				if(bd->ptr != NULL) {

				    if(!IDC$uCustom(idc, bd, tc, res, 
						    outmallocs)) {
					OUT_OF_SPACE();
				    }
				}
			    } else {
				ASSERT(False);
			    }
			    break;
			}
			    
			default:
			    ASSERT(False);
			    break;
			    
			}

		    } /* if(marshal) */

		} /* if(call) */
		    
	    } /* if(MODE_IN) */

	    /* Advance IP */

	    NEXT_INSTRUCTION();

	    TRCMUF(eprintf("bufp now %p\n", bd->ptr));
		    
	} else /* By Reference */ { 
	    
	    IDC_AddrSeq * NOCLOBBER mlist = NULL;
	    sos = False;

	    TRCMUF(eprintf("By Reference\n"));

	    if(!marshal) {
		if((!call) || (MODE(inst) != MODE_IN)) {
		    mlist = outmallocs;
		} else {
		    mlist = inmallocs;
		}
	    }

	    /* parameter passed by reference */

	    /* XXX Optimise for large IN parameters on call unmarshal */
	    
	    argp = NULL;
	    
	    if(marshal) {
		    
		if(call) {
		    
		    if(MODE(inst) == MODE_IN || MODE(inst) == MODE_INOUT) {
			
			GET_PTR_ARG(*sf, &argp);
			
		    } else {
			
			/* We don't need to do anything with this
			argument - skip its place in the stack frame
			unless it's the first result, in which case it
			doesn't have a place in the stack frame. */

			if(!first_result) { 
			    SKIP_PTR_ARG(*sf); 
			}
			
		    }
		    
		} else /* Marshalling return */ { 


		    
		    if(MODE(inst) != MODE_IN) {
			
			if(MODE(inst) == MODE_RESULT) {
			    if(first_result) {
			    
				argp = (addr_t)(pointerval_t)*result;
			    
			    } else {

				/* For a result, the first argument is 
				   actually a pointer to the pointer */
				
				addr_t *tmp;

				GET_PTR_ARG(*sf, &tmp);
				TRCMUF(eprintf("Got pointer %p\n", tmp));
				argp = *tmp;
			    
			    }
			} else {

			    GET_PTR_ARG(*sf, &argp);

			}
			
		    } else {
			
			/* Use up the unused argument */
			SKIP_PTR_ARG(*sf);
			
		    }
		}
		
	    } else { /* Unmarshal */
		
		uint32_t argsize;
		/* reserve space */
		
		if(SOS(inst)) {
		    
		    /* Parameter is a sequence */
		    TRCMUF(eprintf("Parameter is sequence\n"));
		    argsize = sizeof(IDC_AddrSeq);
		    
		} else {
		    
		    if(FORMAT(inst) == FORMAT_STRUCT) {
			
			switch(BASETYPE(inst)) {
			    
			case TYPE_INT8:
			case TYPE_INT16:
			case TYPE_INT32:
			case TYPE_FLT32:
			    
			    argsize = sizeof(uint32_t);
			    break;
			    
			case TYPE_INT64:
			case TYPE_FLT64:
			    
			    argsize = sizeof(uint64_t);
			    break;
			    
			case TYPE_STRING:
			    
			    argsize = sizeof(addr_t);
			    break;
			    
			case TYPE_ALLOCSIZE:
			case TYPE_CUSTOM:
			    argsize = datasize;
			    break;

			case TYPE_ANY:
			    argsize = sizeof(Type_Any);
			    break;
			    
			default:
			    ASSERT(False);
			    argsize = 0;
			    break;
				
			}
		    } else {
			argsize = datasize;
		    }
		    
		}
		
		TRCMUF(eprintf("Argsize = 0x%x\n", argsize));
	    
		if(call) {
		    
		    addr_t buffer;
		
		    if(first_result) {
			
			/* Ignore this parameter */
			
		    } else { 
			
			int align;
			/* Allocate some space and set the
			   argument pointer */
			
			if(MODE(inst) == MODE_IN &&
			   FORMAT(inst) == FORMAT_COPY &&
			   !SOS(inst)) {

			    /* This is a large bulk-copied parameter -
                               just point to it */
			    
			    TRCMUF(eprintf(
				"Unmarshalling in place - setting arg to %p\n",
				bd->ptr));
			    
			    SET_PTR_ARG(*sf, bd->ptr);
			    bd->ptr += TROUNDUP(argsize, uint64_t);
			} else {
				
			    if(MODE(inst) == MODE_RESULT) {

				/* We actually only want to reserve space
				   for a pointer here */

				argsize = sizeof(addr_t);
			    
			    }

			    /* Ensure that staticp matches specified alignment */
			    align = ALIGNVAL(inst);
			    staticp = 
			    (addr_t)((((word_t)staticp) + align) & (~align));
			    
			    buffer = staticp;
			    staticp += argsize;
			    
			    TRCMUF(eprintf("Allocated static parm size %u at %p\n",
					   argsize, buffer));

			    
			    SET_PTR_ARG(*sf, buffer);
			    
			    if(MODE(inst) == MODE_IN || 
			       MODE(inst) == MODE_INOUT) {
			    
				/* If it's an IN or INOUT parameter, we'll
				   need to copy it in */
			    
				argp = buffer;
			    }
			
			}
		    }
		    
		} else { /* Unmarshal on Return */
		    
		    if(MODE(inst) == MODE_RESULT) {
			
			addr_t buffer, *resarg;
			/* Results are returned on Pvs(heap) */
			
			buffer = Heap$Malloc(Pvs(heap), argsize);

			argp = buffer;
			TRCMUF(eprintf("Allocated result buffer %p\n", 
				       buffer));

			if(first_result) {
			    
			    /* Set function result pointer */
			    
			    *result = (pointerval_t) buffer;
			    TRCMUF(eprintf("Set result to %p\n", buffer));

			} else {
			    
			    /* Store result in location pointed to
			       by arg */
			    
			    GET_PTR_ARG(*sf, &resarg);
			    TRCMUF(eprintf("Setting pointer %p to buffer %p\n",
				    resarg, buffer));
			    *resarg = buffer;
			    
			}
			
		    } else if(MODE(inst) == MODE_IN) {
			
			/* Skip this parameter */
			
			SKIP_PTR_ARG(*sf);
			
		    } else {
			
			/* OUT or INOUT */
			
			GET_PTR_ARG(*sf, &argp);
			
		    }
		    
		}

	    }

	    TRCMUF(eprintf("argp = %p, sf = %p\n", argp, sf));

	    /* At this stage:

	       If argp is non-NULL, then it points to the place that
	       the arguments should be copied to/from.

	       The stack frame for this argument is set up if needed.

	       */

#define SRCP  (marshal?argp:bd->ptr)
#define DESTP (marshal?bd->ptr:bufp)

	    if(argp != NULL) {

		/* Loop until next parameter */

		do {
		    
		    int align;

		    TRCMUF(pretty_print_inst(code));

		    TRCMUF(eprintf("argp = %p, align = %u\n",
				   argp, ALIGNVAL(inst) + 1));

		    /* Ensure that argp matches specified alignment */
		    align = ALIGNVAL(inst);
		    argp = (addr_t)((((word_t)argp) + align) & (~align));

		    TRCMUF(eprintf("argp = %p\n", argp));
		    /* If next choice indicator, skip forward */
		    
		    if((!fast) && NC(inst)) {
			
			int choicecount = 0;
			
			TRCMUF(eprintf("Next Choice\n"));
			
			
			while (1) {
			    
			    ASSERT(!(SOC(inst) && EOC(inst)));
			    
			    /* If this is the end of a choice, decrement
			       the counter */

			    if(EOC(inst)) {
				choicecount--;
			    }
			    
			    if(choicecount < 0) {
				/* When we have gone past more EOCs than SOCs,
				   we have reached the end */
				
				break;
				
			    }
			    
			    /* If this is the start of a nested choice,
			       increment the counter */
			    
			    if(SOC(inst)) {
				choicecount++;
			    }
			    
			    NEXT_INSTRUCTION();
			    TRCMUF(pretty_print_inst(code));
			    
			}
			
			TRCMUF(eprintf("Finished choice\n"));

			/* Update argp properly */
			
			sp--;
			
			ASSERT(seq_stack[sp].type == TypeSystem_Choice__code);

			argp = seq_stack[sp].argp;
			TRCMUF(eprintf("argp now %p\n", argp));

			/* Skip to next part */
			NEXT_INSTRUCTION();
			TRCMUF(pretty_print_inst(code));
			
			
			continue;
			
		    }
		    
		    if((!fast) && !sos && SOS(inst)) {
			
			seq_stack_elem *elem = &seq_stack[sp++];
			IDC_AddrSeq *seq = argp;
			
			TRCMUF(eprintf("Start of Sequence\n"));
			
			if(sp == MAX_SEQS) {
			    eprintf("Sequence Stack overflow!\n");
			    ntsc_dbgstop();
			}
			
			/* Store state on stack */
			
			elem->marker = code;
			elem->argp = argp + sizeof(*seq);
			elem->nelems = num_seq_elems;
			elem->type = TypeSystem_Sequence__code;

			if(marshal) {

			    num_seq_elems = seq->len;

			    TRCMUF(eprintf("Sequence length = %u\n",
					   num_seq_elems));
			    
			    if(bd->ptr != NULL) {

				if(USESIDC(inst)) {
				    
				    if(!IDC$mStartSequence(idc, 
							   bd, 
							   num_seq_elems)) {
					OUT_OF_SPACE();
				    } else {
					if(!IDC$mSequenceElement(idc, bd)) {
					    OUT_OF_SPACE();
					}
				    }
				    
				} else {
				    DO_MARSHAL(Cardinal, &num_seq_elems);
				}
			    }
			    
			} else {

			    if(bd->ptr != NULL) {
				
				if(USESIDC(inst)) {
				    
				    if(!IDC$uStartSequence(idc,
							   bd,
							   &num_seq_elems)) {
					OUT_OF_SPACE();
				    } else {
					if(!IDC$uSequenceElement(idc, bd)) {
					    OUT_OF_SPACE();
					}
				    }
				} else {
				    DO_UNMARSHAL(Cardinal, &num_seq_elems);
				}
				
				TRCMUF(eprintf("Sequence length = %u\n",
					       num_seq_elems));
				
				seq->len = seq->blen = num_seq_elems;
				
				TRCMUF(eprintf("Mallocing %u bytes\n",
					       num_seq_elems * datasize));
			    }

			    if(num_seq_elems) {
				if(call) {
				    seq->data = seq->base = 
					Heap$Malloc(h, 
						    num_seq_elems * 
						    datasize);
				} else {
				    seq->data = seq->base =
					Heap$Malloc(Pvs(heap),
						    num_seq_elems *
						    datasize);
				}
				/* Store reference to allocated memory */
				ADD_POINTER(mlist, seq->base);
			    } else {
				seq->data = seq->base = NULL;
			    }
			}
			
			argp = seq->data;
			TRCMUF(eprintf("Sequence data at %p\n", argp));

			if(!num_seq_elems) {
			    
			    int nested_seqs=1;

			    /* This is an empty sequence - skip it */

			    while(1) {
				
				ASSERT(nested_seqs>=0);

				if(EOS(inst)) {
				    nested_seqs--;
				}

				if(!nested_seqs) {
				    break;
				}

				NEXT_INSTRUCTION();
				
				if(SOS(inst)) {
				    TRCMUF(eprintf("Nested SOS\n"));
				    nested_seqs++;
				}
			    }
			
			    /* Set num_seq_elems to 1, so when it's
                               decremented we hit the end of the
                               sequence, then jump to the cleanup code */

			    num_seq_elems = 1;
			    goto done_sequence;
			    
			}			

			if(FORMAT(inst) == FORMAT_COPY) {
			    /* This can be copied in bulk ... */

			    int bufsize = TROUNDUP(datasize * num_seq_elems, 
						  uint64_t);
			    
			    TRCMUF(eprintf("Copying 0x%x bytes between %p and %p\n", 
					   bufsize, argp, bd->ptr));
			    
			    ASSERT(EOS(inst));

			    /* Check space */
			    
			    if(bd->space < bufsize) {
				OUT_OF_SPACE();
			    } else {
				bd->space -= bufsize;
			    }

			    if(marshal) {
				memcpy(bd->ptr, argp, bufsize);
			    } else {
				memcpy(argp, bd->ptr, bufsize);
			    }

			    bd->ptr += bufsize;
			    bd->space -= bufsize;

			    /* Set num_seq_elems to 1, so when it's
                               decremented we hit the end of the
                               sequence, then jump to the cleanup code */

			    num_seq_elems = 1;
			    goto done_sequence;
			}
				       
		    }
		    
		    if((!fast) && SOC(inst)) {
			
			int choicenum, choicecount;
			seq_stack_elem *elem = &seq_stack[sp++];
			
			TRCMUF(eprintf("Start of Choice\n"));
			
			elem->argp = argp + datasize;
			elem->type = TypeSystem_Choice__code;

			TRCMUF(eprintf("Current argp = %p, pushed argp %p\n", 
				argp, elem->argp));

			/* Transfer the choice tag */

			DO_COPY(Cardinal, (uint32_t *)argp);
			choicenum = *((uint32_t *)argp)++;

			TRCMUF(eprintf("Choice tag = %u\n", choicenum));
			choicecount = 0;
			
			/* Skip forward to right place in choice */
			
			while(1) {
			    
			    NEXT_INSTRUCTION();
			    
			    TRCMUF(pretty_print_inst(code));

			    ASSERT(!(SOC(inst) && EOC(inst)));
			    
			    ASSERT(choicecount >= 0);
			    
			    if(choicecount == 0 && NC(inst)) {
				
				choicenum --;
				
			    }
			    
			    if(choicenum < 0) {
				
				/* We have reached the right bit */

				break;
			    }
			    
			    if(SOC(inst)) {
				TRCMUF(eprintf("Nested SOC\n"));
				choicecount++;
				
			    }
			    
			    if(EOC(inst)) {
				
				TRCMUF(eprintf("Nested EOC\n"));
				choicecount++;
				
			    }
			}

			/* Ensure that argp matches specified alignment */
			align = ALIGNVAL(inst);
			argp = (addr_t)((((word_t)argp) + align) & (~align));

		    }
		    
		    
		    /* Now actually do the marshalling/unmarshalling */
		    
			
		    if(FORMAT(inst) == FORMAT_COPY) {
			
			/* This is a lump of data */
			int bufsize = TROUNDUP(datasize, uint64_t);
			
			TRCMUF(eprintf("Copying 0x%x bytes between %p and %p\n", 
				datasize, argp, bd->ptr));

			TRCMUF(memdump(SRCP, datasize));

			/* Check space */
			
			/* If it's a fast call, we don't need to check
                           the space, since we've done it at the
                           top. If it's a slow call, we'll have
                           checked for fixed sized objects, but there
                           may be variable sized objects if we're
                           dealing with a sequence or choice - in this
                           case the stack pointer will be non-zero */

			if((!fast) && sp) {
			    if(bd->space < bufsize) {
				DB(eprintf("bufsize = %u, space = %u\n", 
					   bufsize, bd->space));
				OUT_OF_SPACE();
			    } else {
				bd->space -= bufsize;
			    }
			}
			
			/* Catch the common cases to execute them quickly */
			
			if((bufsize != 0) && (fast || bd->space)) {
			    
			    int align = ALIGNVAL(inst);

			    switch(align) {
			    case 7:
				if(bufsize > 80) {
				    TRCMUF(eprintf("Copying using memcpy\n"));

				    if(marshal) {
					memcpy(bd->ptr, argp, datasize);
				    } else {
					memcpy(argp, bd->ptr, datasize);
				    }
				    argp += datasize;
				    bd->ptr += bufsize;
				} else {
				    /* We are eight byte aligned - we
                                       can optimise by copying 64
                                       bit quantities */
				    
				    switch(bufsize>>3) {
					
				    case 10:
					DO_COPY_64();
				    case 9:
					DO_COPY_64();
				    case 8:
					DO_COPY_64();
				    case 7:
					DO_COPY_64();
				    case 6:
					DO_COPY_64();
				    case 5:
					DO_COPY_64();
				    case 4:
					DO_COPY_64();
				    case 3:
					DO_COPY_64();
				    case 2:
					DO_COPY_64();
				    case 1:
					DO_COPY_64();
					break;
					
				    default:
					ASSERT(False);
					
				    }
				}
				break;
				
			    case 3:
				/* 4-byte alignment */
				if(datasize > 40) {
				    TRCMUF(eprintf("Copying using memcpy\n"));

				    if(marshal) {
					memcpy(bd->ptr, argp, datasize);
				    } else {
					memcpy(argp, bd->ptr, datasize);
				    }
				    argp += datasize;
				    bd->ptr += bufsize;
				} else {
				    switch(datasize >> 2) {
				    case 10:
					DO_COPY_32();
				    case 9:
					DO_COPY_32();
				    case 8:
					DO_COPY_32();
				    case 7:
					DO_COPY_32();
				    case 6:
					DO_COPY_32();
				    case 5:
					DO_COPY_32();
				    case 4:
					DO_COPY_32();
				    case 3:
					DO_COPY_32();
				    case 2:
					DO_COPY_32();
				    case 1:
					DO_COPY_32();
					bd->ptr = (addr_t) 
					    TROUNDUP(bd->ptr, uint64_t);
					break;

				    default:
					ASSERT(False);
				    }
				}
				break; /* 4-byte alignment */

			    case 1:
				/* 2-byte alignment */
				switch(datasize>>1) {
				case 4:
				    DO_COPY_16();
				case 3:
				    DO_COPY_16();
				case 2:
				    DO_COPY_16();
				case 1:
				    DO_COPY_16();
				    bd->ptr = (addr_t)
					TROUNDUP(bd->ptr, uint64_t);
				    break;

				default:
				    TRCMUF(eprintf("Copying using memcpy\n"));

				    if(marshal) {
					memcpy(bd->ptr, argp, datasize);
				    } else {
					memcpy(argp, bd->ptr, datasize);
				    }
				    argp += datasize;
				    bd->ptr += bufsize;
				}
				break;

			    case 0:
				/* Unaligned */
				switch(datasize) {
				case 4:
				    DO_COPY_8();
				case 3:
				    DO_COPY_8();
				case 2:
				    DO_COPY_8();
				case 1:
				    DO_COPY_8();
				    bd->ptr = (addr_t)
					TROUNDUP(bd->ptr, uint64_t);
				    break;

				default:
				    TRCMUF(eprintf("Copying using memcpy\n"));

				    if(marshal) {
					memcpy(bd->ptr, argp, datasize);
				    } else {
					memcpy(argp, bd->ptr, datasize);
				    }
				    argp += datasize;
				    bd->ptr += bufsize;
				    
				}
			    }
			}
		    } else /* This is something structured */ {
			
			TRCMUF(eprintf("Structured object\n"));

			switch(BASETYPE(inst)) {
			    
			case TYPE_INT8:

			    DO_COPY(Octet, (uint8_t *)argp);
			    ((uint8_t *)argp)++;
			    break;


			case TYPE_INT16:

			    DO_COPY(ShortCardinal, (uint16_t *)argp);
			    ((uint16_t *)argp)++;
			    break;

			case TYPE_INT32:
			    
			    DO_COPY(Cardinal, (uint32_t *)argp);
			    ((uint32_t *)argp)++;
			    break;
				
			case TYPE_FLT32:

			    DO_COPY(Real, (float32_t *)argp);
			    ((float32_t *)argp)++;
			    break;

			case TYPE_INT64:

			    DO_COPY(LongCardinal, (uint64_t *)argp);
			    ((uint64_t *)argp)++;
			    break;

			case TYPE_FLT64:

			    DO_COPY(LongReal, (float64_t *)argp);
			    ((float64_t *)argp)++;
			    break;

			case TYPE_STRING:

			    if(!fast) {
				if(marshal) {
				
				    if(bd->ptr != NULL) {
					string_t tmp = *((string_t *)argp)++;
					
					if(USESIDC(inst)) {
					    if(!IDC$mString(idc, bd, tmp)) {
						OUT_OF_SPACE();
					    }
					} else {
					    if(!marshalString(bd, tmp)) {
						OUT_OF_SPACE();
					    }
					    
					    if((!call) && tmp) FREE(tmp);
					}
					
				    } else {
					eprintf(
					    "Buffer overflowed - ignoring\n");
				    }
				} else /* Unmarshal */ {
				    
				    if(bd->ptr != NULL) {
					
					string_t tmp;
					
					if(USESIDC(inst)) {
					    if(!IDC$uString(idc, bd, &tmp)) {
						tmp = NULL;
						OUT_OF_SPACE();
					    }
					} else {
					    if(!unmarshalString(bd, &tmp,
								True, h)) {
						tmp = NULL;
						OUT_OF_SPACE();
					    }
					}
					
					TRCMUF(eprintf("Unmarshalled string to %p\n",
						       argp));
					*((string_t *)argp)++ = tmp;
					
					if(tmp) {
					    ADD_POINTER(mlist, tmp);
					    TRCMUF(eprintf("Unmarshalled '%s'\n", tmp));
					} 
						       
				    } else {
					eprintf(
					    "Buffer overflowed - ignoring\n");
				    }
				} /* if(marshal) */
			    } else {
				ASSERT(False);
			    }				
			    break;

			case TYPE_ALLOCSIZE:

			    TRCMUF(eprintf("Ignored ALLOCSIZE declaration\n"));
			    break;

			case TYPE_CUSTOM:
			    if(!fast) {
				Type_Code tc;

				eprintf("Got custom thingy (BYREF) ***!\n");

				if(LARGE_INST(inst)) {
				    tc = code[2] | ((uint64_t)(code[3]) << 32);
				} else {
				    tc = code[1] | ((uint64_t)(code[2]) << 32);
				} 

				if(marshal) {

				    if(bd->ptr != NULL) {
					if(!IDC$mCustom(idc, bd, tc, 
							argp, !call)) {
					    OUT_OF_SPACE();
					}
				    }

				} else {
				    if(bd->ptr != NULL) {
					if(!mlist->base) {
					    SEQ_INIT(mlist, 0, Pvs(heap));
					}

					if(!IDC$uCustom(idc, bd, tc, 
							argp, mlist)) {
					    OUT_OF_SPACE();
					}
				    }
				}
						    
			    } else {
				/* Shouldn't be in a fast call */
				ASSERT(False);
			    }
			    break;
			    
			case TYPE_ANY:
			    if(!fast) {
				if(marshal) {
				    if(bd->ptr != NULL) {
					if(USESIDC(inst)) {
					    if(!IDC$mCustom(idc, bd, 
							    Type_Any__code, 
							    argp, !call)) {
						OUT_OF_SPACE();
					    }
					} else {
					    if(!marshalAny(idc, bd, 
							   argp, !call)) {
						OUT_OF_SPACE();
					    }
					}
				    }
				} else {
				    if(bd->ptr != NULL) {
					if(!mlist->base) {
					    SEQ_INIT(mlist, 0, Pvs(heap));
					}
				    
					if(USESIDC(inst)) {
					    if(!IDC$uCustom(idc, bd, 
							    Type_Any__code, 
							    argp, mlist)) {
						OUT_OF_SPACE();
					    }
					} else {
					    if(!unmarshalAny(idc, bd, 
							     argp, mlist)) {
						OUT_OF_SPACE();
					    }
					}
				    }
				}
			    } else {
				/* Shouldn't be in a fast call */
				ASSERT(False);
			    }
			    break;

				    
			default:
			    ASSERT(False);
			    break;

			    
			}
		    

		    }

		done_sequence:
		    
		    /* Reset sequences if necessary */
		    
		    if((!fast) && EOS(inst)) {
		    
			TRCMUF(eprintf("End of Sequence\n"));
			
			num_seq_elems --;
			
			if(num_seq_elems) {
			    
			    TRCMUF(eprintf("Skipping back to start\n"));
			    
			    code = seq_stack[sp - 1].marker;
			    inst = code[0];
			    if(LARGE_INST(inst)) {
				datasize = code[1];
			    } else {
				datasize = DATASIZE(inst);
			    }

			    sos = True;

			    if(bd->ptr && USESIDC(inst)) {
				if(marshal) {
				    if(!IDC$mSequenceElement(idc, bd)) {
					OUT_OF_SPACE();
				    }
				} else {
				    if(!IDC$uSequenceElement(idc, bd)) {
					OUT_OF_SPACE();
				    }
				}
			    }
				    
			} else {
			    
			    TRCMUF(eprintf("Sequence finished\n"));
			    
			    sp--;

			    ASSERT(seq_stack[sp].type == 
				   TypeSystem_Sequence__code);
			    
			    num_seq_elems = seq_stack[sp].nelems;
			    argp = seq_stack[sp].argp;
			    
			    if((!call) && marshal) {
				IDC_AddrSeq *seq = argp - sizeof(*seq);
				
				/* Need to free data */
				if(seq->base) FREE(seq->base);
			    }

			    if(bd->ptr && USESIDC(*seq_stack[sp].marker)) {
				if(marshal) {
				    if(!IDC$mEndSequence(idc, bd)) {
					OUT_OF_SPACE();
				    }
				} else {
				    if(!IDC$uEndSequence(idc, bd)) {
					OUT_OF_SPACE();
				    }
				}
			    }
			    
			    TRCMUF(eprintf("argp now %p\n", argp));
			    sos = False;

			    NEXT_INSTRUCTION();
			}
			
		    } else {
			
			/* Reset sequence flag */
			sos = False;

			NEXT_INSTRUCTION();
			
		    }
		    
		    /* Loop until we reach a new parameter. If we're
		       looping back to the start of a sequence that
		       corresponds to a parameter start, don't break out */
		    
		    } while(sos || !SOP(inst));
		
	    } else {
		
		TRCMUF(eprintf("Skipping ignored byte code\n"));
		
		/* Advance until we find the next parameter */

		do {
		    NEXT_INSTRUCTION();
		} while(!SOP(inst));
		
	    }
	} /* By reference */

	TRCMUF(eprintf("Going on to next parameter\n"));

    } /* End of instructions */

    TRCMUF(eprintf("Finished\n"));

    return (bd->ptr != NULL);

}

/* Reduce working set size by routing all exceptions through a single
   version of the inline marshalling engine - speed isn't so critical
   here, and the canned stubs will probably want to use it too. */

static bool_t Static_MUF(IDC_clp idc, IDC_BufferDesc bd,
		  inst_t *code, stack_frame_t *sf,
		  addr_t staticp, IDC_AddrSeq *inmallocs,
		  IDC_AddrSeq *outmallocs, bool_t call,
		  bool_t marshal,
#ifdef DEBUG_MUF
		  bool_t debugmuf,
#endif
		  uint64_t *result) {
    
    return Generic_MUF(idc, bd, code, sf, staticp, inmallocs, outmallocs, 
		       call, marshal, False,
#ifdef DEBUG_MUF
		       debugmuf,
#endif
		       result);
}

void HandleClientException(stub_operation_info *info, int exn_num,
			   string_t exnname,
			   IDC_BufferDesc bd, 
			   IDCClientBinding_clp bnd,
			   IDC_clp idc) {

    IDC_AddrSeq mallocs;
    addr_t                exn_args;
    stub_exception_info  *exn;

#ifdef DEBUG_MUF
    bool_t                NOCLOBBER debugmuf = False;
#endif

    stack_frame_t sf;
    generic_stub_st *st = 
	((generic_stub_st *) (((addr_t)(info - info->opnum)) - 
			      sizeof(generic_stub_st)));

    
    mallocs.base = NULL;

    if(exn_num != -1) {
	exn_num --;

	TRCMUF(eprintf("Got numbered exception %d\n", exn_num));
	
	if(exn_num >= st->num_exns) {
	    
	    /* Invalid exception */
	    DB(eprintf("Server returned invalid exception number %u\n", 
		       exn_num));
	    RAISE_IDC$Failure();
	    
	}
    } else {
	TRCMUF(eprintf("Got named exception '%s'\n", exnname));

	if(!exnname) {
	    DB(eprintf("Binding %p returned NULL exnname\n", bnd));
	    RAISE_IDC$Failure();
	}

	for(exn_num = 0; exn_num<st->num_exns; exn_num++) {
	    if(!strcmp(st->exns[exn_num].name, exnname)) {
		TRCMUF(eprintf("Exception number is %d\n", exn_num));
		break;
	    }
	}
	if(exn_num == st->num_exns) {
	    DB(eprintf("Server returned invalid exception name '%s'\n", 
		       exnname));
	    FREE(exnname);
	    RAISE_IDC$Failure();
	}

	FREE(exnname);
    }
    exn = &st->exns[exn_num];

    TRCMUF(eprintf("Exception '%s' raised by server\n", exn->name));
    
    if(!(exn->procs & SET_ELEM(info->opnum))) {
	
	DB(eprintf("Operation '%s' not permitted to raise exception '%s'\n",
		   info->name, exn->name));
	RAISE_IDC$Failure();
    }
    
    /* Allocate an exception args structure, and prime the
       stack frame. The second word in the byte code list
       contains the size of the exception argument record */
    
    exn_args = exn_rec_alloc(exn->code[1]);
    
    ALLOC_CALLER_STACK_FRAME(sf, sizeof(addr_t));

    RESET_STACK_FRAME(sf);
    
    SET_PTR_ARG(sf, exn_args);
    
    RESET_STACK_FRAME(sf);
    
    /* Unmarshal the exception */
    
    if(Static_MUF(idc, bd, exn->code, &sf, NULL, 
		     NULL, &mallocs, False, False,
#ifdef DEBUG_MUF				
		     debugmuf,
#endif
		     NULL)) {
	    
	/* Raise the exception */
	
	if(mallocs.base) {
	    FREE(mallocs.base);
	}

	IDCClientBinding$AckReceive(bnd, bd);
	RAISE(exn->name, (addr_t)(pointerval_t)exn_args);
    } else {

	if(mallocs.base) {
	    SEQ_FREE_ELEMS(&mallocs);
	    FREE(mallocs.base);
	}

	IDCClientBinding$AckReceive(bnd, bd);
	RAISE_IDC$Failure();
    }
}

void HandleServerException(stub_operation_info *info, 
			   exn_context_t *exn_context,
			   IDCServerBinding_clp bnd, 
			   IDC_BufferDesc bd,
			   IDC_clp idc) {

#ifdef DEBUG_MUF
    bool_t debugmuf = False;
#endif

    stack_frame_t sf;

    generic_stub_st *st;

    int i;

    if(!info) {
	exn_context_t exn_ctx = *exn_context;
	DB(eprintf("HandleServerException: Invalid server op\n"));
	RERAISE;
    }

    st = ((generic_stub_st *) (((addr_t)(info - info->opnum)) - 
			       sizeof(generic_stub_st)));
        
    TRCMUF(eprintf("Server op '%s' raised exception '%s', st = %p\n", 
		   info->name, exn_context->cur_exception, st));
    
    for(i = 0; i < st->num_exns; i++) {
	
	if(!strcmp(exn_context->cur_exception, 
		   st->exns[i].name)) {
	    
	    stub_exception_info *exn = &st->exns[i];
	    
	    if(!(exn->procs & SET_ELEM(info->opnum))) {
		
		exn_context_t exn_ctx = *exn_context;
		DB(eprintf("Operation '%s' not permitted to "
			   "raise exception '%s'\n",
			   info->name, exn_context->cur_exception));
		
		RERAISE;
	    }
	    
#ifdef DELAYED_ACK
	    if(bd) {
		IDCServerBinding$AckReceive(bnd, bd);
	    }
#endif

	    TRC(eprintf("Exception number is %u\n", i));
	    bd = IDCServerBinding$InitExcept(bnd, i+1, 
					     exn->name);
	    
	    TRCMUF(pretty_print_prog(exn->code));
	    
	    /* Prime the stack frame to make it
	       look like it contains a single OUT
	       parameter */
	    
	    ALLOC_CALLER_STACK_FRAME(sf, sizeof(addr_t));
	    
	    TRCMUF(eprintf("exn_context.exn_args = %p\n", 
			   exn_context->exn_args));
	    
	    SET_PTR_ARG(sf, exn_context->exn_args);
	    
	    RESET_STACK_FRAME(sf);
	    
	    if(!Static_MUF(idc, bd, exn->code, &sf, 
			      NULL, NULL, NULL, False, True, 
#ifdef DEBUG_MUF				
			      debugmuf,
#endif
			      0)) {
		
		RAISE_IDC$Failure();
	    }	
	    
	    IDCServerBinding$SendReply(bnd, bd);
	    
	    break;
	}
    }

    if(i == st->num_exns) {
	exn_context_t exn_ctx = *exn_context;

	if(!strcmp(exn_ctx.cur_exception, "Channel$BadState")) {
	    /* The Binder has to raise this to jump back through us
               when someone suicides - don't warn about it ... */
	    TRCMUF(eprintf("Reraising Channel$BadState\n"));
	} else {
	    DB(eprintf("Invalid exception '%s' raised\n", 
		       exn_ctx.cur_exception));
	}
	RERAISE;
    }
    
}

#include <stdarg.h>

/* To trace the progress of an operation, set the string constant
   debugname to the name of the desired operation, and compile with
   the DEBUG_MUF flag set. To trace all operations, compile with
   DEBUG_MUF_ALL set. For proper tracing it may be necessary to set
   the useSynth and useCanned flags in stubgen.c to False. */

const string_t debugname = "NewInterface";

MUF_INLINE static uint64_t Client_Stub(generic_op_st *op,
				       stack_frame_t *sf,
				       bool_t fast) {
    IDCClientBinding_clp  bnd;
    IDC_clp               idc;
    IDC_BufferDesc        bd;
    word_t                res;
    uint64_t              retval;
    inst_t               *code;
    stub_operation_info  *info;
    string_t              exnname;

    string_t              name;

#ifdef DEBUG_MUF
    bool_t                NOCLOBBER debugmuf = False;
#endif

    TRCMUF(eprintf("Entered Client_Stub\n"));

    info = op->info;
    
    bnd = op->cs->binding;
    idc = op->cs->marshal;

    code = info->code;
    name = info->name; /* For debugging purposes */

#ifdef DEBUG_MUF
#ifndef DEBUG_MUF_ALL
    if(!strcmp(name, debugname)) 
#endif
    {
	debugmuf = True;
    }

#endif

    TRCMUF(eprintf("\n\nClient Stub %p Operation: '%s' (%s)\n", op, name, fast?"fast":"slow"));

    TRCMUF(eprintf("Info = %p, Client fn = %p, Server fn = %p\n", info, info->client_fn, info->server_fn));

    TRCMUF(eprintf("opnum = %u\n", info->opnum));

    TRCMUF(pretty_print_prog(code));

    bd = IDCClientBinding$InitCall( bnd, info->opnum, name);

    /* If this is a fast call, code[0] contains the wire length of the
       sent data (fixed length in known format). If this is zero,
       don't bother marshalling */

    if((!fast) || (code[0] != 0)) {
	/* Marshal args */
	if(!Generic_MUF(idc, bd, code, sf, NULL, NULL, NULL,
			True, True, fast, 
#ifdef DEBUG_MUF				
			debugmuf,
#endif
			NULL)) {

	    DB(eprintf("Client_Stub '%s': call MUF failed\n", name));
	    RAISE_IDC$Failure();
	}
    }

    TRCMUF(eprintf("Client_Stub: Making binding call\n"));
    IDCClientBinding$SendCall ( bnd, bd );
    
    res = IDCClientBinding$ReceiveReply(bnd, &bd, &exnname);

    if ((!res) && (!exnname)) {
	
	/* If this is a fast call, then code[2] contains the wire
           length of the returned data (fixed length in known
           format). If this is zero, don't bother unmarshalling */

	if((!fast) || (code[2] != 0)) {
	    /* Unmarshal results */
	    IDC_AddrSeq               mallocs;
	    bool_t success;
	
	    mallocs.base = NULL;
	    
	    RESET_STACK_FRAME(*sf);
	    
	    success = Generic_MUF(idc, bd, code, sf, NULL, NULL, &mallocs,
				  False, False, fast, 
#ifdef DEBUG_MUF				
				  debugmuf,
#endif
				  &retval);
	
	    if(mallocs.base) {
		if(!success) {
		    TRCMUF(eprintf("Freeing malloced stuff\n"));
		    SEQ_FREE_ELEMS(&mallocs);
		}
		FREE(mallocs.base);
	    }
	    
	    if(!success) {
		DB(eprintf("Client_Stub '%s': return MUF failed\n", name));
		RAISE_IDC$Failure();
	    }
	}
    } else {
	
	/* We got an exception - throw it */
	HandleClientException(info, res, exnname, bd, bnd, idc);
	
    } 
    
    IDCClientBinding$AckReceive( bnd, bd );
    
    TRCMUF(eprintf("Stub %p finished: '%s'\n", op, name));

    /* Return first result by value */
    return retval;

}

/* These two functions should both have Client_Stub inlined within
   them - by hardcoding the 'fast' parameter, the compiler can produce
   a streamlined version for simple operations and a heavyweight one
   for more complex operations */

uint64_t Generic_Stub_C(generic_op_st *op, ...) {

    stack_frame_t sf;
    va_list args;
    uint64_t res;
    
    va_start(args, op);

    INIT_CALLEE_STACK_FRAME(sf, &va_arg(args, word_t));

    res = Client_Stub(op, &sf, False);

    va_end(args);

    return res;
}

uint64_t Fast_Stub_C(generic_op_st *op, ...) {

    stack_frame_t sf;
    va_list args;
    uint64_t res;
    
    va_start(args, op);

    INIT_CALLEE_STACK_FRAME(sf, &va_arg(args, word_t));

    res = Client_Stub(op, &sf, True);

    va_end(args);

    return res;
}

void Generic_Dispatch (IDCServerStubs_clp self) {

    generic_stub_st       *st   = STUB_ST_FROM_CLP(self);
    IDCServerStubs_State  *ss   = self->st;
    _generic_cl           *clp  = ss->service;
    IDC_clp                idc  = ss->marshal;
    IDCServerBinding_clp   bnd  = ss->binding;
    IDC_BufferDesc         NOCLOBBER bd;
    word_t                 opnum;
    string_t               name = NULL;
    stub_operation_info   *NOCLOBBER op = NULL;

    TRY {
	
	bd = NULL;

	while(IDCServerBinding$ReceiveCall (bnd, (IDC_BufferDesc *)&bd, 
					    &opnum, &name)) {

	    if(name) {

		for(opnum = 0; opnum < st->num_ops; opnum++) {
		    if(!strcmp(name, st->ops[opnum].name)) {
			break;
		    }
		}
		FREE(name);
	    }
		
	    if(opnum >= st->num_ops) {
		
		DB(eprintf(
		    "Generic_Dispatch: Client requested invalid op %u\n", 
		    opnum));

		ASSERT(False);
		RAISE_IDC$Failure();
		
	    }

	    op = &st->ops[opnum];
	
	    TRC(eprintf("Dispatching to server fn %p, op %p, '%s$%s'\n", 
			op->server_fn, op, st->intfname, op->name));

	    (op->server_fn)(st, clp, opnum, bnd, idc, bd);

#ifdef DELAYED_ACK
	    {
		IDC_BufferDesc tmpbd = bd;
		bd = NULL;
		IDCServerBinding$AckReceive(bnd, tmpbd);
	    }
#endif
	}
	
    } CATCH_ALL {

	HandleServerException(op, &exn_ctx, bnd, bd, idc);
	
    } ENDTRY;

}

MUF_INLINE static void Server_Stub (generic_stub_st     *st, 
				    _generic_cl         *clp,
				    word_t               opnum,
				    IDCServerBinding_clp bnd,
				    IDC_clp              idc,
				    IDC_BufferDesc       _bd,
				    bool_t               fast,
				    addr_t               _statics,
				    addr_t               stackspace)
{
    stub_operation_info   *opn;
    inst_t                *code;
    addr_t                 func;
    IDC_AddrSeq            inmallocs, outmallocs;
    uint64_t               result;
    stack_frame_t          sf;
    bool_t NOCLOBBER       success = False;
    addr_t NOCLOBBER       statics = _statics;
    string_t              name;

#ifdef DEBUG_MUF
    bool_t NOCLOBBER debugmuf  = False;
#endif

    opn = &st->ops[opnum];
    
#ifdef DEBUG_MUF
#ifndef DEBUG_MUF_ALL	
    if(!strcmp(opn->name, debugname)) 
#endif
    {
	debugmuf = True;
    }
#endif

    name = opn->name; /* For debugging purposes */

    TRCMUF(eprintf("\n\nServer Stub %p Operation: '%s' (%s)\n", opn, name, fast?"fast":"slow"));

    /* Set up a stack frame - leave space for the closure pointer, as
       well as the calculated arguments */

    TRCMUF(eprintf("Creating stack frame, size %u\n", 
		   st->maxargssize + sizeof(addr_t)));
    INIT_CALLER_STACK_FRAME(sf, st->maxargssize + sizeof(addr_t), stackspace);

    TRCMUF(eprintf("Static space at %p-%p\n", 
		   statics, statics + (opn->code[1] * 5)));

    code = opn->code;

    TRCMUF(pretty_print_prog(code));


    RESET_STACK_FRAME(sf);
    SET_PTR_ARG(sf, clp);
    
    /* Reset list of malloc'ed data structures for reference
       params */

    inmallocs.base = NULL;
    outmallocs.base = NULL;

    TRY {
	
	IDC_BufferDesc bd = _bd;
	if(!Generic_MUF(idc, bd, code, &sf, statics, &inmallocs, &outmallocs,
			True, False, fast, 
#ifdef DEBUG_MUF				
			debugmuf,
#endif
			NULL)) {
	    
	    DB(eprintf("Server_Stub '%s': call MUF failed\n", opn->name));
	    RAISE_IDC$Failure();
	}
	
#ifndef DELAYED_ACK
	IDCServerBinding$AckReceive(bnd, bd);
#endif

	func = ((addr_t *)(clp->op))[opnum];
	
	TRCMUF(eprintf("Calling closure %p\n",clp));
	
	CALL_STACK_FRAME(sf, func, &result);
	
	TRCMUF(eprintf("Operation returned result %qx\n", 
		       result));
	
	success = True;
	
    } FINALLY {
	
	if(success) {
	    
	    IDC_BufferDesc NOCLOBBER bd = NULL;
	    /* We called the function without it raising any
	       exceptions. Marshal the results and free stuff */
	    
	    /* Release the mallocs lists if necessary */
	    
	    /* The stuff malloced for IN parameters can be freed now,
               since the server method shouldn't have changed IN
               parameters at all. If this is a fast call, there won't
               have been anything that needed mallocing anyway */

	    if((!fast) && inmallocs.base) {
		SEQ_FREE_ELEMS(&inmallocs);
		FREE(inmallocs.base);
	    }
	    
	    /* Stuff malloced for IN OUT, OUT and RESULT parameters
               can't simply be freed, since the server method might
               have changed them - we need to free them as we walk
               over the parameters. We free the sequence of pointers
               at this stage though. */

	    if(outmallocs.base) {
		FREE(outmallocs.base); 
	    }
	    
	    bd = IDCServerBinding$InitReply(bnd);

	    /* If it's a fast call, then code[2] contains the wire
               length of the returned data, AND there should have been
               nothing malloced when unmarshalling. In this case, if
               code[2] is 0, don't bother marshalling results */
	    
	    if((!fast) || (code[2] != 0)) {
		TRCMUF(eprintf("Marshalling return parameters\n"));
		
		RESET_STACK_FRAME(sf);
		SKIP_PTR_ARG(sf);
		
		
		if(!Generic_MUF(idc, bd, code, &sf, NULL, NULL, NULL,
				False, True, fast, 
#ifdef DEBUG_MUF				
				debugmuf,
#endif
				&result)) {

		    DB(eprintf("Server_Stub '%s': return MUF failed\n", 
			       opn->name));
		    RAISE_IDC$Failure();
		}
	    }
	    
	    IDCServerBinding$SendReply(bnd, bd);
	    
	} else /* not success */ {
	    
	    /* We got an exception, expected or otherwise.
	       Just free any outstanding dynamic structures
	       and let someone else close the binding down if
	       this was an abnormal exception */
	    
	    if(inmallocs.base) {
		SEQ_FREE_ELEMS(&inmallocs);
		FREE(inmallocs.base);
	    }

	    if(outmallocs.base) {
		SEQ_FREE_ELEMS(&outmallocs);
		FREE(outmallocs.base);
	    }
	}
	
    } ENDTRY_INLINE;

}

/* These two functions should both have Server_Stub inlined within
   them - by hardcoding the 'fast' parameter, the compiler can produce
   a streamlined version for simple operations and a heavyweight one
   for more complex operations. We have to do the stack allocation for
   the static space and the stack frame here, since gcc refuses to
   inline anything containing alloca(), and performance goes down the
   toilet */

void Generic_Stub_S (generic_stub_st     *st, 
		     _generic_cl         *clp,
		     word_t               opnum,
		     IDCServerBinding_clp bnd,
		     IDC_clp              idc,
		     IDC_BufferDesc       bd) {

    stub_operation_info   *opn;
    addr_t statics = NULL;

    opn = &st->ops[opnum];
    
    /* The second 'instruction' in the byte code is actually
       the amount of memory for the server to allocate for
       static reference parameters */

    if(opn->code[1]) {
	statics = alloca(opn->code[1] * 5);
    }

    Server_Stub(st, clp, opnum, bnd, 
		idc, bd, False, statics,
		GET_STACK_FRAME_SPACE(st->maxargssize + sizeof(addr_t)));

}

void Fast_Stub_S (generic_stub_st     *st, 
		     _generic_cl         *clp,
		     word_t               opnum,
		     IDCServerBinding_clp bnd,
		     IDC_clp              idc,
		     IDC_BufferDesc       bd) {

    stub_operation_info   *opn;
    addr_t statics = NULL;
    opn = &st->ops[opnum];

    /* The second 'instruction' in the byte code is actually
       the amount of memory for the server to allocate for
       static reference parameters */

    if(opn->code[1]) {
	statics = alloca(opn->code[1] * 5);
    }

    Server_Stub(st, clp, opnum, 
		bnd, idc, bd, 
		True, statics,
		GET_STACK_FRAME_SPACE(st->maxargssize + sizeof(addr_t)));

}

bool_t StubGen_Marshal_m(StubGen_clp self,
			 StubGen_Code *code,
			 IDC_clp idc,
			 IDC_BufferDesc bd,
			 addr_t element,
			 bool_t doFree) {

    stack_frame_t sf;

#ifdef DEBUG_MUF
    bool_t debugmuf = False;
#endif

    ALLOC_CALLER_STACK_FRAME(sf, sizeof(addr_t));

    SET_PTR_ARG(sf, element);

    RESET_STACK_FRAME(sf);

    TRCMUF(pretty_print_prog(code->data));

    return Static_MUF(idc, bd, code->data, &sf,
		      NULL, NULL, NULL, !doFree, True,
#ifdef DEBUG_MUF
		      debugmuf,
#endif
		      NULL);
}

bool_t StubGen_Unmarshal_m(StubGen_clp self,
			   StubGen_Code *code,
			   IDC_clp idc,
			   IDC_BufferDesc bd,
			   addr_t element,
			   IDC_AddrSeq *mallocs) {
    
    stack_frame_t sf;

#ifdef DEBUG_MUF
    bool_t debugmuf = False;
#endif

    ALLOC_CALLER_STACK_FRAME(sf, sizeof(addr_t));

    RESET_STACK_FRAME(sf);

    SET_PTR_ARG(sf, element);

    RESET_STACK_FRAME(sf);

    /* We don't know whether this is a call or a return - but we'll
       treat it as a return, since we know where the object has to
       live */

    return(Static_MUF(idc, bd, code->data, &sf, NULL, NULL,
		      mallocs, False, False,
#ifdef DEBUG_MUF
		      debugmuf,
#endif
		      NULL));

}


