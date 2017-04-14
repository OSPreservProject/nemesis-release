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
**      app/stubgen
**
** FUNCTIONAL DESCRIPTION:
** 
**      Automatic stub generation
** 
** ENVIRONMENT: 
**
**      Shared library
** 
** ID : 	$Id: stubgen.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

/* PRB: Some random thoughts...

Putting a huge amount of marshalling code and IDC Stub code into the
boot image really gets on my nerves.  And it's all doing
exactly the same thing anyway... stick a few bytes in a buffer and
poke an event count.  What's the big deal.  Why are there so many
different sets of stubs?  You even have to statically link all of the
marshalling code together since it's full of extern references
-- which don't even have the interface typecode as part of their name.
And exceptions are just strings... that's particularly foolish.  Do we
trust the linker or dynamic loader to get the right version of an
interface?  I think not.

Time to sort this mess out.

Why not export a context into the namespace where you can look up a
typecode and get back a set of IDC stubs.

Sounds too good to be true? 
Read on...

Since we have the new fangled all seeing, all powerful TypeSystem
thang, why not use it.  By writing a nasty little recursive procedure
we can get the typesystem to give us detailed information on any type
defined in a MIDDL interface.  Not being theory people, we don't
usually have much call for recursion, so let's make the most of it!

We can use the TypeSystem to create a flattened representation of any
MIDDL type which can then be used to drive a simple marshalling state
machine which only has to worry about MIDDL base types.  Each stub
method (client or server) could sneakily load up the procedure number
and bail into this state machine.  That way the only difference
between IDC stubs is the tables used to drive the state machine and
that's just data... easy.

Of course some people like to pass huge structures across interfaces,
and MIDDL has STRING and SEQUENCE types which are variable sized, and
CHOICE types which are just a pain in the neck.  Oh, and I almost
forgot EXCEPTIONS which don't even have typecodes.  Sounds like this
state machine isn't going to go very fast.

But... anybody who writes Interfaces which require me to do all of
that dreadful malloc stuff clearly don't care how slow the
stubs go! :-)

Anything which only uses SMALL types could be arranged to point at a
different stub which could be blindingly fast... basically a bcopy of
the args array into the buffer.  Forget about packing together things
which are smaller than a word. All the stub needs is the number of
args and the number of results.

This byte-code thang isn't a new idea... the Opera group have done
this for MSRPC3, but not a run-time of course :-). Opera uses the
following encoding:

#define ODRc__Boolean           'b'     // C style   =int
#define ODRc__CXXBoolean        'B'     // C++ style =pt  
#define ODRc__String            's'
#define ODRc__Octet             'o'
#define ODRc__Character         'x'
#define ODRc__ShortCardinal     'q'
#define ODRc__Cardinal          'c'
#define ODRc__LongCardinal      'C'
#define ODRc__ShortInteger      'w'
#define ODRc__Integer           'i'
#define ODRc__LongInteger       'I'
#define ODRc__Real              'r'
#define ODRc__LongReal          'R'
#define ODRc__Pointer           'p'
Constructors:
   abc          : Record
   (10t)        : Array of type t
   (t)          : Sequence of type t
   {a,b,c}      : Choice or a,b or c
   [a]          : Leave space for a (eg unmarshall around a virtual fn table in Cxx)

They use a human-readable string but we don't really care... 
There are 16 MIDDL base types with type codes from 0 to 0xf ... 
pretty obvious encoding here.  

*/

/* PBM

OK, so it's now a bit more complex than when PRB originally planned it ...

Some random documentation about the bytecode: most of it should be
fairly obvious (provided you're me, obviously ...).

Sequences and Choices:

A sequence is started with an SOS bit. The instructions up to the
corresponding EOS bit describe the type of the sequence element. If
the instruction containing the SOS bit is an ALLOCSIZE then the
sequence is described by structure and the data size determines the
size of each whole element. Otherwise 

a) The instruction containing the SOS bit describes the element type
completely,

b) The EOS bit should be set in the same instruction.

c) The type of the sequence element should be simple - no choices,
sequences or custom thingies.

If the last instruction describing a sequence element has the EOS bit
set (e.g. a sequence of a record type whose last element is a
sequence) then an empty copy instruction is inserted after the nested
EOS bit to contain the EOS of the enclosing sequence.

A choice begins with a SOC bit. The instruction containing it should
be an ALLOCSIZE, giving the overall size of the choice.  (including
the tag). The next instruction, tagged with an NC bit, starts to
describe the type of the first choice member. The start of the
description of the second and successive choice members is indicated
with an NC bit. An NC bit combined with an EOC bit indicates the end
of the choice. 

Choices and Sequences can both be nested in any order.

Arrays: A bit evil, this:

If the array element is very simple, it will just be coded as a big
copy parameter.  

XXX The following paragraph is not yet true ... XXX
Otherwise, the SOA bit will be set, and the datasize portion of the
instruction will contain the number of elements in the array (rather
than the normal size of the object). The LARGE_INST bit will be set,
and the real size of the object will be stored as the next
instruction. Hopefully setting LARGE_INST will fool the rest of the code
successfully ...
XXX The preceding paragraph is not yet true XXX 

*/

#include "stubgen.h"
#include <domid.h>
#include <time.h>
#include <ecs.h>
#include <IDCMarshalCtl.ih>
#include <Interface.ih>
#include <Operation.ih>
#include <Enum.ih>
#include <Record.ih>
#include <Choice.ih>


/*
 * Module stuff */
static	Context_List_fn 	StubContext_List_m;
static	Context_Get_fn 		StubContext_Get_m;
static	Context_Add_fn 		StubContext_Add_m;
static	Context_Remove_fn 	StubContext_Remove_m;
static	Context_Dup_fn 		StubContext_Dup_m;
static	Context_Destroy_fn 	StubContext_Destroy_m;

static	Context_op	stubcontext_ms = {
  StubContext_List_m,
  StubContext_Get_m,
  StubContext_Add_m,
  StubContext_Remove_m,
  StubContext_Dup_m,
  StubContext_Destroy_m
};
const static	Context_cl	stubcontext_cl = { &stubcontext_ms, NULL };
CL_EXPORT (Context, StubGenC, stubcontext_cl);

typedef struct {
} *Context_st;

static  IDCClientStubs_InitState_fn    ClientStubs_InitState;
static  IDCClientStubs_DestroyState_fn ClientStubs_DestroyState;

static IDCClientStubs_op GenericIDCClientStub_ms = {
    ClientStubs_InitState,
    ClientStubs_DestroyState
};

/*---------------------------------------------------- Entry Points ----*/

//#define DBOUT(x) ntsc_putcons(x)
#define DBOUT(x)

static  StubGen_GenIntf_fn              StubGen_GenIntf_m;
static  StubGen_GenType_fn              StubGen_GenType_m;
/* StubGen_Marshal_m and StubGen_Unmarshal_m are in engine.c */

static  StubGen_op      stubgen_ms = {
  StubGen_GenIntf_m,
  StubGen_GenType_m,
  StubGen_Marshal_m,
  StubGen_Unmarshal_m,
};

static Closure_Apply_fn                 StubGen_Init_m;

static Closure_op stubgen_init_ms = { StubGen_Init_m };
const static Closure_cl stubgen_init = { & stubgen_init_ms, NULL };
CL_EXPORT(Closure, StubGenInit, stubgen_init);

static Context_Names *StubContext_List_m (Context_cl	*self )
{
    Context_clp realstubs = self->st;

    return Context$List(realstubs);
}

static bool_t 
StubContext_Get_m (
	Context_cl	*self,
	string_t	name	/* IN */,
	Type_Any	*obj	/* OUT */ )
{
    Context_clp realstubs = self->st;
    StubGen_clp stubgen = NAME_FIND("sys>StubGen", StubGen_clp);
    Type_Code NOCLOBBER tc = 0;
    IDCStubs_Info NOCLOBBER info;
    bool_t NOCLOBBER success = True;
    
    TRY {
	info = NAME_LOOKUP(name, realstubs, IDCStubs_Info);
	
	TRC(eprintf("Found static stubs for %s\n", name));
    } CATCH_Context$NotFound(UNUSED n) {
	
	TRC(eprintf("Generating stubs for %s\n", name));
	
	TRY {
	    tc = NAME_LOOKUP(name, (Context_clp) Pvs(types), Type_Code);
	    TRC(eprintf("Found tc %qx from typesystem\n", tc));
	} CATCH_Context$NotFound(UNUSED n) {
	    tc = strtou64(name, NULL, 16);
	    TRC(eprintf("Got tc %qx from string\n", tc));
	} ENDTRY;
	
	TRY {
	    TRC(Time_T start = NOW();
		Time_T taken);
	    info = StubGen$GenIntf(stubgen, tc, NULL, Pvs(heap));
	    TRC(taken = NOW() - start;
		eprintf("Stubs: Time taken %u us\n", taken / 1000));
	} CATCH_StubGen$Failure() {
	    success = False;
	} ENDTRY;
    } ENDTRY;

    if(success) {

	ANY_INIT(obj, IDCStubs_Info, info);
    }
    return success;
}

static void  StubContext_Add_m (Context_cl	*self,
				string_t	name	 /* IN */,
				const Type_Any	*obj	/* IN */ )
{
    Context_clp realstubs = self->st;

    Context$Add(realstubs, name, obj);
}

static void StubContext_Remove_m (Context_cl *self, string_t name /* IN */ ) {
    Context_clp realstubs = self->st;

    Context$Remove(realstubs, name);
}

static Context_clp StubContext_Dup_m (Context_cl *self, 
				      Heap_clp h, WordTbl_clp xl) {
    Context_clp realstubs = self->st;

    return Context$Dup(realstubs, h, xl);
}

static void StubContext_Destroy_m (Context_cl	*self )
{
    Context_clp realstubs = self->st;

    Context$Destroy(realstubs);
}

typedef struct _StubGen_st {
    StubGen_cl         stubgen_cl;
    StubGen_clp        realStubGen;
    LongCardTbl_clp    ctlTbl;
    LongCardTbl_clp    alignTbl;
    LongCardTbl_clp    codesTbl;
    mutex_t            mu;
    LongCardTblMod_clp tblMod;
} StubGen_st;

typedef struct _PerCtl_st {
    StubGen_st       *sg_st;
    IDCMarshalCtl_clp ctl;
    LongCardTbl_clp   stubsTbl;
    LongCardTbl_clp   typesTbl;
    LongCardTbl_clp   smallTypesTbl;
} PerCtl_st;

/* A code record for a particular type */
    
typedef struct _CodeRec {
    word_t codeLen; /* code length in inst_t's */
    word_t bufSize;
    word_t typeSize;
    word_t align;
    bool_t large;
    /* Followed by code */
} CodeRec;

/******************************************/


#define NEXT(b)  ({ (*(b))+= 1 + LARGE_INST(**(b)) + 2 * ((FORMAT(**(b)) == FORMAT_STRUCT) && BASETYPE(**(b)) == TYPE_CUSTOM); **(b) = 0; (*(b))[1] = 0; })

#define SIZEVAL(b) (LARGE_INST(*(b)) ? ((b))[1] : DATASIZE(*(b)))	

#define IREF_clp__code 0

#define CODE_BLOCK_SIZE 1024

/******************************************/


static const char * const mode[] = {
    "IN", "OUT", "IN OUT", "RESULT"
};

/* Table giving the maximum alignment for location at a given offset
   from a starting location */

static const int align_table[8] = {
    7, 0, 1, 0, 3, 0, 1, 0,
};

#define ALIGNMENT8 1

typedef struct {
    uint8_t dummy;
    uint16_t uint16;
} align_16_struct;

#define ALIGNMENT16 (OFFSETOF(align_16_struct, uint16))

typedef struct {
    uint8_t dummy;
    uint32_t uint32;
} align_32_struct;

#define ALIGNMENT32 (OFFSETOF(align_32_struct, uint32))

typedef struct {
    uint8_t dummy;
    uint64_t uint64;
} align_64_struct;

#define ALIGNMENT64 (OFFSETOF(align_64_struct, uint64))

typedef struct {
    uint8_t dummy;
    word_t word;
} align_word_struct;

#define ALIGNMENT_WORD (OFFSETOF(align_word_struct, word))

typedef struct {
    uint8_t dummy;
    float32_t float32;
} align_f32_struct;

#define ALIGNMENT_F32 (OFFSETOF(align_f32_struct, float32))

typedef struct {
    uint8_t dummy;
    float64_t float64;
} align_f64_struct;

#define ALIGNMENT_F64 (OFFSETOF(align_f64_struct, float64))

/* Returns byte alignment of specified type, and caches results */

static word_t alignment(PerCtl_st *st, Type_Code tc) {

    Type_Any rep;
    Interface_clp intf;
    word_t align = 0;

    if(LongCardTbl$Get(st->sg_st->alignTbl, tc, (addr_t *) &align)) {
	DBOUT('+');
	return align;
    }

    intf = TypeSystem$Info(Pvs(types), tc, &rep);

    switch(rep.type) {
    case TypeSystem_Alias__code:
	align = alignment(st, TypeSystem$UnAlias(Pvs(types), tc));
	break;

    case TypeSystem_Predefined__code:
	switch(tc) {

	case uint8_t__code:
	case int8_t__code:
	    align = 1;
	    break;

	case uint16_t__code:
	case int16_t__code:
	    align = ALIGNMENT16;
	    break;

	case uint32_t__code:
	case int32_t__code:
	case bool_t__code:
	    align = ALIGNMENT32;
	    break;

	case uint64_t__code:
	case int64_t__code:
	    align = ALIGNMENT64;
	    break;

	case float32_t__code:
	    align = ALIGNMENT_F32;
	    break;

	case float64_t__code:
	    align = ALIGNMENT_F64;
	    break;

	case word_t__code:
	case string_t__code:
	case addr_t__code:
	    align = ALIGNMENT_WORD;
	    break;
	    
	default:
	    ntsc_dbgstop();
	    break;
	    
	}
	break;

    case TypeSystem_Enum__code:
	align = ALIGNMENT32;
	break;

    case TypeSystem_ArrayIndex__code:
	ntsc_dbgstop(); /* What be this ? XXX */
	break;

    case TypeSystem_Array__code:
    {
	TypeSystem_Array *array;
	array = NARROW(&rep, TypeSystem_Array);
	align = alignment(st, array->tc);
	break;
    }
	
    case TypeSystem_BitSet__code:
	ntsc_dbgstop();
	break;

    case TypeSystem_Set__code:
	align = ALIGNMENT64;
	break;

    case TypeSystem_Ref__code:
    case TypeSystem_Iref__code:
	align = ALIGNMENT_WORD;
	break;

    case TypeSystem_Sequence__code:
	align = ALIGNMENT_WORD;
	break;

    case TypeSystem_Record__code:
    case TypeSystem_Choice__code:
    {
	Context_clp context;
	Context_Names *names;
	Type_Code elem_tc;
	bool_t isRecord = ISTYPE(&rep, TypeSystem_Record);
	int i;
	Type_Any any;
	int maxalign = isRecord?0:ALIGNMENT32;

	ANY_UNALIAS(&rep);
	context = NARROW(&rep, Context_clp);

	names = Context$List(context);
	for (i= 0; i < SEQ_LEN(names); i++) {

	    char *name = SEQ_ELEM(names, i);
	    
	    if(!Context$Get(context, name, &any)) {
		ntsc_dbgstop();
	    }

	    elem_tc = isRecord?
		NARROW(&any, Record_Field)->tc:
		NARROW(&any, Choice_Field)->tc;

	    maxalign = MAX(maxalign, alignment(st, elem_tc));
	    
	}    
	
	SEQ_FREE(names);
	align = maxalign;
	break;
    }

    default:
	ntsc_dbgstop();
	break;
    }

    DBOUT('!');
    LongCardTbl$Put(st->sg_st->alignTbl, tc, (addr_t) align);
    return align;
}

/* The next few functions deal with producing an efficient and correct
   canonical representation for a given parameter definition. Don't
   change them without a lot of serious thought ... */

__inline__ void set_data_size(inst_t **b, int size) {

    if(LARGE_INST(**b)) {
	/* This item has the 'large' flag set - datasize is in next word */
	(*b)[1] = size;
    } else {
	/* Mask off the old size and set the new - if this takes it
           over the maximum 'small' data length, promote it to a
           'large' item */

	**b &= ~(MAX_SMALL_SIZE << SIZE_OFFSET);
	if(size > MAX_SMALL_SIZE) {
	    (*b)[1] = size;
	    **b |= LARGE_INST(SETVAL);
	} else {
	    **b |= (size << SIZE_OFFSET);
	}
    }
}
	
__inline__ void marshal_copy(inst_t **b, int size, int align, bool_t verbose) 
{ 
    /* If the last thing was a structured type, or if this copy is
       more strictly aligned than the last one, move to the next
       instruction, padding if necessary */

    if(verbose) {
	eprintf("marshal_copy: size %u, align %u\n", size, align);
    }

    if((FORMAT(**b) & FORMAT_STRUCT)) {
	NEXT(b); 
    } else if (SIZEVAL(*b) && ((align -1) > ALIGNVAL(**b))) { 
	set_data_size(b, (SIZEVAL(*b) + ALIGNVAL(**b)) & ~ALIGNVAL(**b));
	NEXT(b);
    }

    /* If there is a gap between the last copy and this, then pad
       out the last copy. */
    
    if(LARGE_INST(**b)) {
	(*b)[1] = (((*b)[1] + align -1) & -align) + size;
    } else {
	if(DATASIZE(**b) == 0) {
	    **b |= ((align - 1) << ALIGN_OFFSET);
	}	    
	set_data_size(b, ((DATASIZE(**b) + align - 1) & -align) + size);
    }
    if(verbose) {
	eprintf("marshal_copy: buf %p, size %u\n", *b, SIZEVAL(*b));
    }
} 

__inline__ void marshal_structured(inst_t **b, int type, int align,
				   bool_t useidc, bool_t verbose) 
{ 
    if(FORMAT(**b) & FORMAT_STRUCT) {
	/* If we've already got a structured thing in here then go on
           to next instruction */
	NEXT(b); 
    } else if (DATASIZE(**b) || LARGE_INST(**b)) { 
	int copylen, remainder;
	/* If we've got a copy block, check that the length is
           OK for the alignment, then go on to next instruction */
	
	copylen = SIZEVAL(*b);

	/* We can pad out this copy to the point where the structured
           object starts, to help the alignment. */
	
	copylen = (copylen + align - 1) & -align;
	
	if((remainder = (copylen & ALIGNVAL(**b)))) {
	    /* Split this off into two blocks, one with the initial
               alignment and as long as possible, the other with the
               remainder and smaller alignment. */ 
	    copylen = copylen - remainder;

	    ASSERT(remainder < 8);

	    if(copylen != 0) {
		set_data_size(b, copylen);
		NEXT(b);
	    } else {
		/* Lose this instruction */
		set_data_size(b, 0);
		**b &= ~(LARGE_INST(SETVAL) | 7 << ALIGN_OFFSET);
	    }

	    marshal_copy(b, remainder, align_table[remainder%8], verbose);
	} else if (copylen != DATASIZE(**b)) {
	    set_data_size(b, copylen);
	}
	NEXT(b);
    }

    **(b) |= SETTYPE(type) | FORMAT_STRUCT | ((align -1) << ALIGN_OFFSET); 

    if(useidc) {
	**(b) |= USEIDC;
    }
} 

bool_t simple_inst(inst_t inst) {

    return (!SOC(inst) && 
	    !SOS(inst) && 
	    !((FORMAT(inst) == FORMAT_STRUCT) && 
	      BASETYPE(inst) == TYPE_CUSTOM) && 
	    !LARGE_INST(inst) &&
	    !USESIDC(inst));

}

static bool_t flatten(PerCtl_st *st, buf_t buf, Type_Code tc, 
		      word_t *bufsize, bool_t useidc, int base_align, 
		      int elem_offset, bool_t verbose)
{
    Interface_clp intf;
    Record_clp record;
    Context_Names    *names;
    Type_Any          rep;
    Type_Any          any;
    int i, n;

    int currentalign;
    int newalign;
    IDCMarshalCtl_Code code;
    
    currentalign = ((base_align - 1) & align_table[elem_offset % 8]) + 1;
    newalign = MAX(currentalign, alignment(st, tc));

    if(verbose) {

	eprintf("base_align = %u, elem_offset = %u, "
		"currentalign = %u, newalign = %u\n", 
		base_align, elem_offset, currentalign, newalign);
    }

    if(!IDCMarshalCtl$Type(st->ctl, tc, &code)) {
	return False;
    }

    if(code == IDCMarshalCtl_Code_Custom) {
	
	word_t typesize;
	/* IDC wants complete control over marshalling this type */
	
	typesize = TypeSystem$Size(Pvs(types), tc);
	*bufsize = 0;
	
	marshal_structured(buf, TYPE_CUSTOM, newalign, useidc, verbose); 
	
	if(typesize > MAX_SMALL_SIZE) {
	    **buf |= LARGE_INST(SETVAL);
	    (*buf)[1] = typesize;
	    (*buf)[2] = tc & 0xffffffff;
	    (*buf)[3] = tc >> 32;
	} else {
	    **buf |= (typesize << SIZE_OFFSET);
	    (*buf)[1] = tc & 0xffffffff;
	    (*buf)[2] = tc >> 32;
	    
	}
	
	return True;

    } else if(code == IDCMarshalCtl_Code_Native) {
	
	/* IDC has no interest in anything below here */
	
	useidc = False;
	
    } else {
	useidc = True;
    }

    /* Special case Type.Any */
#if 0
    if(tc == Type_Any__code) {

	marshal_structured(buf, TYPE_ANY, ALIGNMENT_WORD, useidc, verbose);
	set_data_size(buf, sizeof(Type_Any));

	return True;
    }
#endif

    intf = TypeSystem$Info(Pvs(types), tc, &rep);

    switch(rep.type) {
    case TypeSystem_Alias__code:
	tc = rep.val;
	tc = TypeSystem$UnAlias (Pvs(types), tc);
	TRCTS("ALIAS -> %qx\n", tc);
	return(flatten(st, buf, tc, bufsize, useidc,
		       base_align, elem_offset, verbose));
	break;
    case TypeSystem_Predefined__code:

	if(tc == string_t__code) {
	    marshal_structured(buf, TYPE_STRING, ALIGNMENT_WORD, 
			       useidc, verbose);
	} else /* Not a string */ {
	    
	    int typesize = TypeSystem$Size(Pvs(types), tc);
	    
	    if(useidc) {

		switch(tc) {
		case uint8_t__code:
		case int8_t__code:
		    marshal_structured(buf, TYPE_INT8, ALIGNMENT8, 
				       True, verbose);
		    break;

		case uint16_t__code:
		case int16_t__code:
		    marshal_structured(buf, TYPE_INT16, ALIGNMENT16, 
				       True, verbose);
		    break;

		case uint32_t__code:
		case int32_t__code:
		    marshal_structured(buf, TYPE_INT32, ALIGNMENT32, 
				       True, verbose);
		    break;

		case uint64_t__code:
		case int64_t__code:
		    marshal_structured(buf, TYPE_INT64, ALIGNMENT64, 
				       True, verbose);
		    break;

		case float32_t__code:
		    marshal_structured(buf, TYPE_FLT32, ALIGNMENT_F32, 
				       True, verbose);
		    break;

		case float64_t__code:
		    marshal_structured(buf, TYPE_FLT64, ALIGNMENT_F64, 
				       True, verbose);
		    break;
		}
	    } else /* Dont use IDC */ {
	    
		marshal_copy(buf, typesize, newalign, verbose);
		*bufsize += typesize;
	    }

	}
	TRCTS("PREDEFINED %qx\n", tc);
	break;
    case TypeSystem_Enum__code:
	TRCTS("ENUM\n");

	if(useidc) {
	    marshal_structured(buf, TYPE_INT32, ALIGNMENT32, True, verbose);
	} else {
	    marshal_copy(buf, sizeof(uint32_t), newalign, verbose);
	    *bufsize += sizeof(uint32_t);
	}
	break;
    case TypeSystem_ArrayIndex__code:
	ntsc_dbgstop(); /* What be this ? XXX */
	break;

    case TypeSystem_Array__code:
    {
	TypeSystem_Array *array = NARROW(&rep, TypeSystem_Array);
	int elemsize = TypeSystem$Size(Pvs(types), array->tc);
	/* Maybe ought to marshal as an explicit array - but then more
           difficult to optimise large copies. Needs more cunningness
           in flatten() XXX */

	TRCTS("ARRAY %qx\n", tc);
	for (n = 0; n < array->n; n++) {
	    if(!flatten(st, buf, array->tc, bufsize, useidc,
			base_align, elem_offset + n * elemsize, verbose)) {
		return False;
	    };
	}
	break;
    }
    case TypeSystem_BitSet__code:
	TRCTS("BITSET\n");
	ntsc_dbgstop();
	break;
    case TypeSystem_Set__code:
	TRCTS("SET\n");
	marshal_copy(buf, sizeof(uint64_t), ALIGNMENT64, verbose);
	*bufsize += sizeof(uint64_t);
	break;
    case TypeSystem_Ref__code:
	TRCTS("REF %qx\n", tc);
	marshal_copy(buf, sizeof(addr_t), ALIGNMENT_WORD, verbose);
	*bufsize += sizeof(addr_t);
	break;
    case TypeSystem_Iref__code:
	TRCTS("IREF %qx\n", tc);
	marshal_copy(buf, sizeof(addr_t), ALIGNMENT_WORD, verbose);
	*bufsize += sizeof(addr_t);
	break;
    case TypeSystem_Sequence__code:
    {
	inst_t *tmp = NULL, *start = NULL;
	int elem_align = alignment(st, tc);
	int elemsize; 

	tc = rep.val;
	elemsize = TypeSystem$Size(Pvs(types), tc);
	TRCTS("SEQUENCE %qx\n", tc);

	marshal_structured(buf, TYPE_ALLOCSIZE, elem_align, useidc, verbose);
	
	TRCTS("size = %u\n", elemsize);
	set_data_size(buf, elemsize);


	**buf |= SOS(SETVAL);
	    
	start = *buf;

	NEXT(buf);

	tmp = *buf;

	{
	    word_t bsize;
	    if(!flatten(st, buf, tc, &bsize, useidc,
			elem_align, 0, verbose)) {
		return False;
	    }
	}

	/* If we've just finished a nested sequence, go on to next
           instruction to mark the end of sequence */
	if(**buf & EOS(SETVAL)) {
	    NEXT(buf);
	}

	/* Optimise simple sequences */

	if((tmp == *buf) && simple_inst(**buf)) {
	    inst_t newinst;
	    TRC(eprintf("Optimising Sequence:\n"));
	    TRC(pretty_print_inst(start));
	    TRC(pretty_print_inst(tmp));

	    ASSERT(FORMAT(*start) == FORMAT_STRUCT);
	    ASSERT(BASETYPE(*start) == TYPE_ALLOCSIZE);

	    newinst = *start & (~(FORMAT(SETVAL)|(SETTYPE(SETVAL))));

	    TRC(pretty_print_inst(&newinst));

	    newinst |= *tmp & (FORMAT(SETVAL) | SETTYPE(SETVAL));

	    TRC(pretty_print_inst(&newinst));

	    *start = newinst;

	    *buf = start;
	}

	**buf |= EOS(SETVAL);
	if(!useidc) {
	    *bufsize += sizeof(uint32_t);
	}

	break;
    }
    case TypeSystem_Record__code:
    {
	inst_t *tmp = NULL, *start = NULL;
	int offset = 0;
	int recsize = TypeSystem$Size(Pvs(types), tc);
	if(SOP(**buf)) {
	    
	    marshal_structured(buf, TYPE_ALLOCSIZE, alignment(st, tc), 
			       useidc, verbose);

	    set_data_size(buf, recsize);

	    start = *buf;
	    NEXT(buf);
	    tmp = *buf;
	
	}
	    
	TRCTS("RECORD [\n");

	ANY_UNALIAS(&rep);
	record = NARROW(&rep, Record_clp);
	names = Context$List((Context_clp)record);
	for (i= 0; i < SEQ_LEN(names); i++) {
	    Record_Field *f;
	    
	    char *name = SEQ_ELEM(names, i);
	    
	    if(!Context$Get((Context_clp)record, name, &any)) {
		ntsc_dbgstop();
	    }
	    
	    f = NARROW(&any, Record_Field);

	    offset = f->offset;

	    TRCTS("  %s : %qx\n", name, f->tc);
	    if(!flatten(st, buf, f->tc, bufsize, useidc,
			base_align, elem_offset + offset, verbose)) {
		return False;
	    }

	    /* This value is only used after the last pass through the
               loop */

	    offset += TypeSystem$Size(Pvs(types), f->tc);
	    
	}    
	
	/* Pad out the record if necessary */
	if((offset < recsize) && (FORMAT(**buf) == FORMAT_COPY)) {
	    set_data_size(buf, SIZEVAL(*buf) + recsize - offset);
	}

	TRCTS("]\n");
	DBOUT('@');
	SEQ_FREE(names);

	if((tmp == *buf) && (!USESIDC(**buf))) {
	    inst_t newinst;
	    TRC(
		eprintf("Optimising record:\n");
		pretty_print_inst(start);
		pretty_print_inst(tmp);
		);

	    ASSERT(FORMAT(*start) == FORMAT_STRUCT);
	    ASSERT(BASETYPE(*start) == TYPE_ALLOCSIZE);
	    if(DATASIZE(*start) < DATASIZE(*tmp)) {
		eprintf("Base:\n");
		pretty_print_inst(start);
		eprintf("Contents:\n");
		pretty_print_inst(tmp);
	    }
	    ASSERT(DATASIZE(*start) >= DATASIZE(*tmp)); 

	    /* We have a record consisting of a single thing (either a
               structured type or a bulk copy). Condense it into a
               single reference instruction */

	    newinst = *start & (~(FORMAT(SETVAL)|(SETTYPE(SETVAL))));
	    newinst |= *tmp & (FORMAT(SETVAL)|(SETTYPE(SETVAL)));
	    *start = newinst;
	    TRC(pretty_print_inst(start));
	    *buf = start;
	}

	

	break;
    }
    case TypeSystem_Choice__code:
    {
	int ualign = 0;
	int choicesize = TypeSystem$Size(Pvs(types), tc);

	Context_clp context;
	Context_Names *names;
	Type_Any any;
	Type_Code utc;
	
	ANY_UNALIAS(&rep);
	context = NARROW(&rep, Context_clp);
	names = Context$List(context);

	/* Find the maximum alignment of any of the choice's members */
	for (i= 0; i < SEQ_LEN(names); i++) {
	    
	    char *name = SEQ_ELEM(names, i);
	    
	    if(!Context$Get(context, name, &any)) {
		ntsc_dbgstop();
	    }
	    
	    utc = NARROW(&any, Choice_Field)->tc;
	    
	    ualign = MAX(ualign, alignment(st, utc));
	    
	}    
	
	/* If we're the first thing in a parameter, put a size marker
           in the bytecode */
	if(SOP(**buf)) {
	    marshal_structured(buf, TYPE_ALLOCSIZE, newalign, useidc, verbose);

	    set_data_size(buf, choicesize);
	    
	}
	
	**buf |= SOC(SETVAL);
	NEXT(buf);

	TRCTS("CHOICE\n");
	for (i= 0; i < SEQ_LEN(names); i++) {
		
	    char *name = SEQ_ELEM(names, i);
	    Choice_Field *field;
	    word_t bsize;

	    if(!Context$Get(context, name, &any)) {
		/* XXX Something is really broken in the typesystem
                   ... */
		ntsc_dbgstop();
	    }
	    
	    field = NARROW(&any, Choice_Field);
	    
	    TRCTS("  %s : %qx\n", name, field->tc);
	    
	    /* Put a NextChoice marker with the appropriate alignment */
	    **buf |= NC(SETVAL) | ((ualign - 1) << ALIGN_OFFSET);

	    if(!flatten(st, buf, field->tc, &bsize, useidc,
			ualign, 0, verbose)) {
		return False;
	    }
	    NEXT(buf);
	}

	**buf |= EOC(SETVAL) | NC(SETVAL);
	
	DBOUT('@');
	SEQ_FREE(names);
	
	break;
    }
    default:
	DB(printf("UNKNOWN %qx\n", tc));
	ntsc_dbgstop();
	break;
    }
    
    return True;

}

static bool_t process_small(PerCtl_st *st, Type_Code tc, buf_t buf, 
			    word_t *size) {
    
    Type_Any rep;
    Interface_clp iref;
    IDCMarshalCtl_Code code;
    bool_t useidc;

    if(!IDCMarshalCtl$Type(st->ctl, tc, &code)) {
	return False;
    }

    **buf |= BYVAL | FORMAT_STRUCT;

    if(code == IDCMarshalCtl_Code_Custom) {
	int tsize;

	tsize = TypeSystem$Size(Pvs(types), tc);

	**buf |= SETTYPE(TYPE_CUSTOM) | BYVAL | FORMAT_STRUCT;
	
	if(tsize > MAX_SMALL_SIZE) {
	    **buf |= LARGE_INST(SETVAL);
	    (*buf)[1] = tsize;
	    (*buf)[2] = tc & 0xffffffff;
	    (*buf)[3] = tc >> 32;
	} else {
	    **buf |= (tsize << SIZE_OFFSET);
	    (*buf)[1] = tc & 0xffffffff;
	    (*buf)[2] = tc >> 32;
	}
	*size = 0;
	**buf |= (alignment(st, tc) - 1) << ALIGN_OFFSET;
	return True;
    }

    useidc = (code == IDCMarshalCtl_Code_IDC);

    iref = TypeSystem$Info(Pvs(types), tc, &rep);
    TRC({
	string_t name = TypeSystem$Name(Pvs(types), tc);
	printf("Param name is '%s' (%qx)\n", name, tc);
	FREE(name);
	})

    switch(rep.type) {
    case TypeSystem_Alias__code:
	ASSERT(False); /* Error - should already be unaliased */
	break;
	
    case TypeSystem_Predefined__code:
	
	switch(tc) {
	case uint8_t__code:
	case int8_t__code:
	    TRC(printf("Found INT8 param\n"));
	    **buf |= SETTYPE(TYPE_INT8);
	    break;

	case uint16_t__code:
	case int16_t__code:
	    TRC(printf("Found INT16 param\n"));
	    **buf |= SETTYPE(TYPE_INT16);
	    break;

	case bool_t__code:
	case uint32_t__code:
	case int32_t__code:
	    TRC(printf("Found INT32 param\n"));
	    **buf |= SETTYPE(TYPE_INT32);
	    break;
	    
	case uint64_t__code:
	case int64_t__code:
	    TRC(printf("Found INT64 param\n"));
	    **buf |= SETTYPE(TYPE_INT64);
	    break;

	case float32_t__code:
	    TRC(printf("Found FLT32 param\n"));
	    **buf |= SETTYPE(TYPE_FLT32);
	    break;

	case float64_t__code:
	    TRC(printf("Found FLT64 param\n"));
	    **buf |= SETTYPE(TYPE_FLT64);
	    break;

	case string_t__code:
	    TRC(printf("Found STRING param\n"));
	    **buf |= SETTYPE(TYPE_STRING);
	    break;

	case word_t__code:
	case addr_t__code:
	    TRC(printf("Found WORD param\n"));
	    **buf |= SETTYPE(TYPE_WORD);
	    break;
	    
	default:
	    ASSERT(False);
	}
	
	break;
	
    case TypeSystem_Set__code:	
	TRC(printf("Found SET param\n"));
	**buf |= SETTYPE(TYPE_INT64);
	break;

    case TypeSystem_Enum__code:
	TRC(printf("Found ENUM param\n"));
	**buf |= SETTYPE(TYPE_INT32);
	break;

    case TypeSystem_Ref__code:
    case TypeSystem_Iref__code:
	TRC(printf("Found REF param\n"));
	**buf |= SETTYPE(TYPE_WORD);
	break;
	
    case TypeSystem_ArrayIndex__code:
	ASSERT(False);
	break;

    default:
	ASSERT(False);
	break;
    }

    if(useidc) {
	**buf |= USEIDC;
	/* If the USEIDC flag is set, we don't want the byte-code
           engine checking space for this parameter */
	*size = 0;
    } else {
	/* Using the native format, every item is 8 byte aligned */
	*size = 8;
    }

    return True;
}


static CodeRec *get_code_rec(PerCtl_st *st, Type_Code tc, 
			     bool_t useSmall, bool_t verbose) {

    IDCMarshalCtl_Code code;
    CodeRec *cr = NULL;
    bool_t large = TypeSystem$IsLarge(Pvs(types), tc);
    LongCardTbl_clp tbl = NULL;

    if(large) {
	useSmall = False;
    }

    if(useSmall) {
	tbl = st->smallTypesTbl;
    } else {
	tbl = st->typesTbl;
    }
    
    if(!LongCardTbl$Get(tbl, tc, (addr_t *)&cr)) {
	
	bool_t res;
	inst_t *ip, *codestart;
	
	/* We've not dealt with this type before - generate a record
           for it */
	if(!IDCMarshalCtl$Type(st->ctl, tc, &code)) {
	    return NULL;
	}
	
	/* XXX Ought to be growable ... */
	cr = Heap$Calloc(Pvs(heap), 1, CODE_BLOCK_SIZE); 
	
	cr->align = alignment(st, tc);
	cr->large = TypeSystem$IsLarge(Pvs(types), tc);
	cr->typeSize = TypeSystem$Size(Pvs(types), tc);
	cr->bufSize = 0;
	ip = codestart = (inst_t *) (cr + 1);

	/* Mark the start of a parameter */
	ip[0] = SOP(SETVAL);

	if(useSmall) {
	    res = process_small(st, tc, &ip, &cr->bufSize);
	} else {
	    res = flatten(st, &ip, tc, &cr->bufSize, 
			  True, cr->align, 0, verbose);
	}

	if(!res) {
	    /* We couldn't flatten this type */
	    FREE(cr);
	    return NULL;
	}
	
	NEXT(&ip);

	/* Resize the record to a sensible length */
	cr->codeLen = ip - codestart;

	ASSERT(cr->codeLen * sizeof(inst_t) < CODE_BLOCK_SIZE);

	cr = REALLOC(cr, sizeof(CodeRec) + cr->codeLen * sizeof(inst_t));

	/* Save it in the table for future use */
	LongCardTbl$Put(tbl, tc, cr);
	DBOUT('~');
    } else {
	DBOUT('?');
    }

    return cr;
}

static void promote_small(inst_t *i) {

    if(USESIDC(*i)) {
	return;
    }
    
    if((BASETYPE(*i)  == TYPE_INT8) || (BASETYPE(*i) == TYPE_INT16)) {
	TRCTS("Promoting parm to INT32\n");
	*i = (*i & (~SETTYPE(SETVAL))) | SETTYPE(TYPE_INT32);
    }

#ifdef SMALL_IS_64BIT
    if((BASETYPE(*i) == TYPE_INT32)) {
	TRCTS("Promoting parm to INT64\n");
	*i = (*i & (~SETTYPE(SETVAL))) | SETTYPE(TYPE_INT64);
    }
#endif
}
    
static bool_t param_signature(PerCtl_st *st, Operation_Parameter *param, 
			      buf_t buf, bool_t *result, 
			      inst_t *staticcounts,
			      bool_t verbose)
{
    Type_Code          tc = param->type;
    CodeRec    *cr = NULL;
    inst_t     *ip;
    bool_t useSmall = 
	(param->mode == Operation_ParamMode_In) ||
	(param->mode == Operation_ParamMode_Result);

    cr = get_code_rec(st, tc, useSmall, verbose);
    if(!cr) return False;

    /* Now fill in the parameter in the buffer */

    ip = (inst_t *) (cr + 1);

    memcpy(*buf, ip, cr->codeLen * sizeof(inst_t));

    switch(param->mode) {
    case Operation_ParamMode_In:
    {
	**buf |= MODE_IN;
	
	if(cr->large) {

	    **buf |= BYREF;
	    staticcounts[1] += TROUNDUP(cr->typeSize, uint64_t);
	} else if(cr->codeLen == 1) {
	    promote_small(*buf);
	}
	
	staticcounts[0] += cr->bufSize;

	break;
    }

    case Operation_ParamMode_InOut:
    {
	**buf |= MODE_INOUT | BYREF;

	staticcounts[0] += cr->bufSize;
	staticcounts[1] += TROUNDUP(cr->typeSize, uint64_t);
	staticcounts[2] += cr->bufSize;
	break;
    }

    case Operation_ParamMode_Out:
    {
	**buf |= MODE_OUT | BYREF;

	staticcounts[1] += TROUNDUP(cr->typeSize, uint64_t);
	staticcounts[2] += cr->bufSize;
	break;
    }
    case Operation_ParamMode_Result:
    {
	/* If first result is SMALL then returned by value, else reference */
	/* All others by reference. However, the bytecode has an
           implicit BYREF for a small RESULT param */
	/* For LARGE results, a pointer to Malloc'ed state is returned */
	**buf |= MODE_RESULT;
	
	if(cr->large) {
	    **buf |= BYREF;
	} else {
	    /* We want to promote the parameter if it's the first
               result */

	    if(!*result) {
		promote_small(*buf);
	    }
	}

	if(*result) {
	    /* If this is not the first result, reserve space for it
               at the server side. */
	    staticcounts[1] += sizeof(uint64_t);
	} else {
	    *result = True;
	}

	staticcounts[2] += cr->bufSize;

	break;
    }
    }

    *buf += cr->codeLen;

    return True;
}

static bool_t operation_signature(PerCtl_st *st, Operation_clp op, 
				  buf_t buf, int *num_args,
				  bool_t verbose)
{
    Context_Names       *names;
    bool_t inresults = False;
    int i, pass;
    inst_t *staticcounts;
    bool_t resultsFirst = False;

    /* Save space for the static data counts:

     data in buffer on call
     data allocated by server
     data in buffer on return

     */

    if(!IDCMarshalCtl$Operation(st->ctl, op, &resultsFirst)) {
	return False;
    }

    staticcounts = *buf;
    *buf += 3;

    names = Context$List((Context_clp)op);

    *num_args = SEQ_LEN(names);

    for(pass = 0; pass < 2; pass++) {

	inresults = False;

	for (i = 0; i < SEQ_LEN(names); i++) {
	    Operation_Parameter *param;
	    Type_Any             any;
	    bool_t process;
	    bool_t ok;

	    Context$Get((Context_clp) op, SEQ_ELEM(names, i), &any);
	    param = NARROW(&any, Operation_Parameter);

	    process = ((pass == 0) !=  
		(param->mode == Operation_ParamMode_Result));

	    if(resultsFirst) {
		process = !process;
	    }

	    if(process) {
		ok = param_signature(st, param, buf, &inresults, 
				     staticcounts, verbose);
		
		if(!ok) {
		    return False;
		}
	    
		TRCTS("Counts: Call - %u, Server - %u, Return - %u\n",
			     staticcounts[0], staticcounts[1], 
			     staticcounts[2]);
	    }
	}
    }

    **buf |= SOP(SETVAL) | EOP(SETVAL);

    DBOUT('#');
    SEQ_FREE(names);

    return True;
}

static void exception_signature(PerCtl_st *st, Exception_clp exn, buf_t buf) {

    Context_Names       *names;
    int                  num_params, i;
    Type_Code            tc;

    int offset = 0, align = 0, size = 0;
    int maxalign = 0;
    inst_t *staticcounts;
    word_t bufsize = 0;
    names = Context$List((Context_clp)exn);

    num_params = SEQ_LEN(names);

    /* Skip space for the static data counts (mostly ignored for
       exceptions) */

    staticcounts = *buf;
    staticcounts[0] = 0;
    staticcounts[1] = 0;
    staticcounts[2] = 0;

    *buf += 3; 

    /* Treat the exception record as a single large OUT parameter */

    **buf = SOP(SETVAL) | MODE_OUT | BYREF;

    if(num_params > 0) {
	
	/* From here on would be absolutely trivial if an exception
	   parameter record were represented properly in the
	   typesystem - we'd basically just say
	   
	   staticcounts[1] += flatten(st, buf, paramrectypecode, &size);
	   staticcounts[2] += size;
	   
	   But it isn't. :-(. So we have to work it out ourselves ...
	   
	   */
	
	/* First run through each of the elements to calculate the record
	   alignment and size */
	
	for(i = 0; i < num_params; i++) {
	    
	    tc = NAME_LOOKUP(SEQ_ELEM(names, i), (Context_clp) exn, Type_Code);

	    align = alignment(st, tc);

	    maxalign = MAX(maxalign, align);

	    /* Pad the record size to match the alignment of this element */
	    size = (size + align - 1) & -align;

	    /* Now add the size of this element */
	    size += TypeSystem$Size(Pvs(types), tc);
	    
	};
	
	/* maxalign now contains the alignment of the most constrained
	   element in the record, and hence the alignment of the record
	   itself */
	
	/* Pad the record size to ensure that the size is a multiple of
	   the alignment */
	
	size = (size + maxalign - 1) & -maxalign;
	staticcounts[1] = size;
	marshal_structured(buf, TYPE_ALLOCSIZE, maxalign, False, False);
	
	set_data_size(buf, size);
	
	NEXT(buf);
	
	/* Now run through the elements again, generating marshalling code */
	
	offset = 0;
	
	for(i = 0; i < num_params; i++) {
	    
	    tc = NAME_LOOKUP(SEQ_ELEM(names, i), (Context_clp) exn, Type_Code);
	    
	    align = alignment(st, tc);

	    
	    if(!flatten(st, buf, tc, &bufsize, 
			True, maxalign, offset, False)) {
		ntsc_dbgstop();
	    }
	    
	    offset += TypeSystem$Size(Pvs(types), tc);
	    
	}

	NEXT(buf);

    }
    
    staticcounts[2] = bufsize;
    **buf |= SOP(SETVAL) | EOP(SETVAL);

    SEQ_FREE_ELEMS(names);
    SEQ_FREE(names);

}

bool_t find_canned(inst_t *code, code_entry **res) {

    code_entry *entry = (code_entry *) canned_table;

    while(entry->code != NULL) {
	if(!memcmp(entry->code, code+3, entry->len)) {
	    TRC(eprintf("Found canned stub\n"));
	    *res = entry;
	    return True;
	}
	entry++;
    }
    return False;
}

bool_t is_simple(inst_t *code, bool_t strict) {

    int mode = 0;
    code += 3;

    while(!EOP(*code)) {
	
	TRCTS((pretty_print_inst(code), "Checking for simplicity\n"));

	if(SOP(*code)) mode = MODE(*code);

	if(SOS(*code) || SOC(*code)) {
	    TRCTS("Contains SOC/SOS: not simple\n");
	    return False;
	}

	if(FORMAT(*code) == FORMAT_STRUCT) {
	    
	    if((BASETYPE(*code) == TYPE_STRING) && (USESIDC(*code))) {
		TRCTS("String by IDC : not simple\n");
		return False;
	    }

	    if((BASETYPE(*code) == TYPE_STRING) && (mode != MODE_IN)) {
		TRCTS("String by mode other than IN\n");
		return False;
	    }

	    if(BASETYPE(*code) == TYPE_CUSTOM) {
		TRCTS("Custom: not simple\n");
		return False;
	    }

	    if(strict && ((VALREF(*code)) == BYREF)) {
		TRCTS("Structured Reference: not simple\n");
		return False;
	    }

	}	    

	if((mode == MODE_RESULT) && VALREF(*code) == BYREF) {
	    TRCTS("Large result: not simple\n");
	    return False;
	}

	if(strict && USESIDC(*code)) {
	    TRCTS("Uses IDC marshalling: not simple\n");
	    return False;
	}

	code += 1 + LARGE_INST(*code);
    }

    TRCTS("Is Simple!\n");

    return True;

}

void pretty_print_inst(inst_t *code) {

    char buf[256], tmp[256];
    inst_t i;
    uint32_t size;

    buf[0] = 0;

    i = code[0];
    size = SIZEVAL(code);
    
    sprintf(buf, "(%p) ", code);

    if(SOP(i)) {
	strcat(buf, "SOP | ");
	
	if(EOP(i)) {
	    strcat(buf, "EOP | ");
	    goto doprint;
	}
	    
	strcat(buf, mode[MODE(i) >> MODE_OFFSET]);
	strcat(buf, " | ");
	
	if(VALREF(i) == BYVAL) {
	    strcat(buf, "VALUE | ");
	} else {
	    strcat(buf, "REF | ");
	}
	
    }
    
    if(FORMAT(i) == FORMAT_COPY) {
	strcat(buf, "COPY | ");
    } else {
	strcat(buf, "STRUCT | ");
	
	switch(BASETYPE(i)) {
	    
	case TYPE_ALLOCSIZE:
	    strcat(buf, "ALLOC | ");
	    break;

	case TYPE_INT8:
	    strcat(buf, "INT8 | ");
	    break;

	case TYPE_INT16:
	    strcat(buf, "INT16 | ");
	    break;

	case TYPE_INT32:
	    strcat(buf, "INT32 | ");
	    break;
	    
	case TYPE_INT64:
	    strcat(buf, "INT64 | ");
	    break;
	    
	case TYPE_FLT32:
	    strcat(buf, "FLT32 | ");
	    break;
	    
	case TYPE_FLT64:
	    strcat(buf, "FLT64 | ");
	    break;
	    
	case TYPE_STRING:
	    strcat(buf, "STRING | ");
	    break;
	    
	case TYPE_CUSTOM:
	{
	    Type_Code tc;

	    if(LARGE_INST(i)) {
		tc = code[2] | ((uint64_t)(code[3]) << 32);
	    } else {
		tc = code[1] | ((uint64_t)(code[2]) << 32);
	    } 
	    
	    sprintf(tmp, "CUSTOM (%qx) | ",tc);
	    strcat(buf, tmp);
	    break;
	}
	default:
	    strcat(buf, "Unknown | ");
	    
	}
	
    }

    if(SOS(i)) {
	strcat(buf, "SOS | ");
    }
    
    if(SOC(i)) {
	strcat(buf, "SOC | ");
    }
    
    if(NC(i)) {
	strcat(buf, "NC | ");
    }
    
    if(size || 
       (FORMAT(i) == FORMAT_STRUCT && BASETYPE(i) == TYPE_ALLOCSIZE) ) {
	sprintf(tmp, "%d%s | ", size, LARGE_INST(i)?"(L)":"");
	strcat(buf, tmp);
    }
    
    if(EOS(i)) {
	strcat(buf, "EOS | ");
    }
    
    if(EOC(i)) {
	strcat(buf, "EOC | ");
    }
    
    if(ALIGNVAL(i)) {
	int len;
	strcat(buf, "ALIGN ");
	len = strlen(buf);
	buf[len] = ALIGNVAL(i) + '1';
	buf[len + 1] = 0;
	strcat(buf, " | ");
    }

    if(USESIDC(i)) {
	strcat(buf, "USEIDC | ");
    }

doprint:

    buf[strlen(buf) - 2] = 0;
    eprintf("%s\n", buf);

}

void pretty_print_bytecodes(inst_t *code) {

    inst_t  i;
    do {

	i = *code;
	pretty_print_inst(code);
	
	code += 1 + LARGE_INST(i);
	if((FORMAT(i) == FORMAT_STRUCT) &&
	   (BASETYPE(i) == TYPE_CUSTOM)) {
	    code += 2;
	}
	
    } while (!EOP(i));
}

void pretty_print_prog(inst_t *code) {

    eprintf("Static data: Call %u, Server %u, Return %u\n", 
	    code[0], code[1], code[2]);
    code += 3;

    pretty_print_bytecodes(code);
}

typedef SEQ_STRUCT(stub_operation_info) op_seq;
typedef SEQ_STRUCT(stub_exception_info) exn_seq;

extern addr_t Generic_op[];

#ifdef STATIC_VARS
int totalcount = 0;
int codecount = 0;
#endif

#ifdef STATIC_VARS

LongCardTbl_clp tbl;

void add_code_to_tbl(inst_t *code, int len) {

    inst_t *i = code;
    int l = len;
    uint64_t hash = 0;
    code_entry *entry, *old_entry;

    /* Make hash of code ... */

    while(l-- > 0) {
	hash ^= *i++;
	hash = (hash << 1) | (hash >> 63);
    }

    if(LongCardTbl$Get(tbl, hash, (addr_t *)&entry)) {
	while(entry) {
	    if(!memcmp(code, entry->code, len * sizeof(inst_t))) {
		entry->count++;
		eprintf("Found a match - count %u!\n", entry->count);
		return;
	    }
	    old_entry = entry;
	    entry = entry->next;
	}

	entry = Heap$Malloc(Pvs(heap), 
			    sizeof(code_entry) + len * sizeof(inst_t));

	entry->code = (inst_t *)(entry + 1);
	entry->count = 1;
	entry->next = NULL;
	memcpy(entry->code, code, len * sizeof(inst_t));
	old_entry->next = entry;

	codecount++;
    } else {
	entry = Heap$Malloc(Pvs(heap), 
			    sizeof(code_entry) + len * sizeof(inst_t));

	entry->code = (inst_t *)(entry + 1);
	entry->count = 1;
	entry->next = NULL;
	
	LongCardTbl$Put(tbl, hash, entry);
	codecount++;
    }
}

#endif

static IDCMarshalCtl_Interface_fn NullCtlIntf;
static IDCMarshalCtl_Operation_fn NullCtlOp;
static IDCMarshalCtl_Type_fn      NullCtlType;

static IDCMarshalCtl_op NullCtl_ms = {
    NullCtlIntf,
    NullCtlOp,
    NullCtlType,
};

const static IDCMarshalCtl_cl NullCtl = { &NullCtl_ms, NULL };

typedef SEQ_STRUCT(Operation_clp) MiddlOpSeq;


/* get_ops gets operation records for an interface and all its base
   types */

void get_ops(Interface_clp intf, MiddlOpSeq *mops) {

    Interface_clp super;
    Context_Names * NOCLOBBER names = NULL;
    int i;

    TRY {
	if(Interface$Extends(intf, &super)) {
	    
	    Interface_Needs *needs;
	    Type_Name name;
	    Type_Code code;
	    bool_t local, isIREF;
	    

	    /* Check this isn't the IREF interface - it wouldn't break
               to recurse on it, but it would waste a lot of our time
               since it's big */

	    needs = Interface$Info(super, &local, &name, &code);

	    SEQ_FREE(needs);

	    isIREF = !strcmp(name, "IREF");
	    FREE(name);

	    if(!isIREF) {

		get_ops(super, mops);
	    }
	}
	
	names = Context$List((Context_clp)intf);
	
	for(i = 0; i < SEQ_LEN(names); i++) {
	    Type_Any any;

	    if(!Context$Get((Context_clp)intf, SEQ_ELEM(names, i), &any)) {
		fprintf(stderr, 
			"StubGen: Bogus interface element '%s'\n", 
			SEQ_ELEM(names, i));
		RAISE_StubGen$Failure();
	    }
	    
	    if(ISTYPE(&any, Operation_clp)) {
		SEQ_ADDH(mops, NARROW(&any, Operation_clp));
	    }
	}

    } FINALLY {

	if(names) {
	    SEQ_FREE_ELEMS(names);
	    SEQ_FREE(names);
	}
    } ENDTRY;
}

void StubGen_Init_m(Closure_clp self) {

    Context_clp ctx = self->st;
    StubGen_st *st = Heap$Malloc(Pvs(heap), sizeof(*st));

    if(VP$DomainID(Pvs(vp)) != DOM_NEMESIS) {
	Type_Any any;
	IDCOffer_clp offer;
	StubGen_clp NemSG = NAME_FIND("sys>StubGen", StubGen_clp);
	StubGen_st *NemSG_st = NemSG->st;
	*st = *NemSG_st;
	CL_INIT(st->stubgen_cl, &stubgen_ms, st);
	CX_ADD_IN_CX(ctx, "StubGen", &st->stubgen_cl, StubGen_clp);
	offer = NAME_FIND("sys>StubGenOffer", IDCOffer_clp);
	IDCOffer$Bind(offer, Pvs(gkpr), &any);
	st->realStubGen = NARROW(&any, StubGen_clp);
    } else {
	st->tblMod = NAME_FIND("modules>SafeLongCardTblMod",
			       LongCardTblMod_clp);
	
	CL_INIT(st->stubgen_cl, &stubgen_ms, st);
	st->realStubGen = NULL;
	st->ctlTbl = LongCardTblMod$New(st->tblMod, Pvs(heap));
	st->alignTbl = LongCardTblMod$New(st->tblMod, Pvs(heap));
	st->codesTbl = LongCardTblMod$New(st->tblMod, Pvs(heap));
	MU_INIT(&st->mu);
	CX_ADD("sys>StubGen", &st->stubgen_cl, StubGen_clp);
	IDC_EXPORT("sys>StubGenOffer", StubGen_clp, &st->stubgen_cl);
    }

}

static PerCtl_st *NewCtlSt(StubGen_st *sg_st, IDCMarshalCtl_clp ctl) {

    PerCtl_st *ctl_st = Heap$Malloc(Pvs(heap), sizeof(*ctl_st));
    
    ctl_st->sg_st = sg_st;
    ctl_st->ctl = ctl;
    ctl_st->stubsTbl = LongCardTblMod$New(sg_st->tblMod, Pvs(heap));
    ctl_st->typesTbl = LongCardTblMod$New(sg_st->tblMod, Pvs(heap));
    ctl_st->smallTypesTbl = LongCardTblMod$New(sg_st->tblMod, Pvs(heap));
    
    LongCardTbl$Put(sg_st->ctlTbl, (word_t)ctl, ctl_st);
    
    return ctl_st;
}

IDCStubs_Info StubGen_GenIntf_m(StubGen_clp self,
				Type_Code intfcode, 
				IDCMarshalCtl_clp _ctl,
				Heap_clp h) {

    StubGen_st *sg_st = self->st;
    PerCtl_st *ctl_st = NULL;
    IDCMarshalCtl_clp NOCLOBBER ctl = _ctl; 
    Interface_clp    intf;
    Type_Any         rep;
    int i;
    bool_t           ok;
    
    op_seq           ops;
    MiddlOpSeq       mops;
    exn_seq          exns;
    int              NOCLOBBER maxnumargs = 0;
    int              num_args;
    int              NOCLOBBER code_length = 0;
    generic_stub_st * NOCLOBBER gen_st = NULL;
    addr_t           state_mem;
    int              state_size;
    inst_t           *codeptr;
    Exception_List   *exnlist;
    Exception_clp    exn_clp;
    bool_t NOCLOBBER use_synth = False;
    bool_t NOCLOBBER use_canned = False;
    bool_t NOCLOBBER use_fast = True;
    bool_t verbose = False;

    if(!ctl) {
	ctl = (IDCMarshalCtl_clp) &NullCtl;
    }

    /* See if we have built stubs for this type/ctl combination already */

    {
	bool_t found;
	
	/* Try to find the record for this IDCMarshalCtl */

	found = LongCardTbl$Get(sg_st->ctlTbl, (word_t)ctl, (addr_t *)&ctl_st);

	/* Try to find the record for this type code */
	if(found) {
	    found = LongCardTbl$Get(ctl_st->stubsTbl, intfcode, 
				    (addr_t *)&gen_st);
	}
	
	if(found) {
		
	    TRC(eprintf("StubGen: found entry for %qx\n", intfcode));
	    if(VP$DomainID(Pvs(vp)) != DOM_NEMESIS) {
		DBOUT('%');
	    } else {
		DBOUT('&');
	    }
	    return &gen_st->rec;
	}

    }

    /* We haven't got the stubs - call Nemesis to make them for us */

    if(VP$DomainID(Pvs(vp)) != DOM_NEMESIS) {
	ASSERT(sg_st->realStubGen != NULL);
	return StubGen$GenIntf(sg_st->realStubGen, intfcode, ctl, NULL);
    }

    /* We must be in Nemesis already ... Just Do It */

    h = Pvs(heap);

    MU_LOCK(&sg_st->mu);
    
    /* Now that we've got the lock, check once more to see if the
       stubs are already there ... */

    {
	bool_t found;
	
	/* Try to find the record for this IDCMarshalCtl */
	
	found = LongCardTbl$Get(sg_st->ctlTbl, (word_t)ctl, (addr_t *)&ctl_st);

	/* Try to find the record for this type code */
	if(found) {
	    found = LongCardTbl$Get(ctl_st->stubsTbl, intfcode, 
				    (addr_t *)&gen_st);
	} else {
	    /* Generate the PerCtl state */
	    ctl_st = NewCtlSt(sg_st, ctl);
	}
	
	if(found) {
		
	    TRC(eprintf("StubGen: found entry for %qx\n", intfcode));
	    MU_RELEASE(&sg_st->mu);
	    return &gen_st->rec;
	}
    }

    TRY {

	int opnum = 0;
	SEQ_INIT(&ops, 0, Pvs(heap));
	SEQ_INIT(&exns, 0, Pvs(heap));
	SEQ_INIT(&mops, 0, Pvs(heap));
	

	TypeSystem$Info(Pvs(types), intfcode, &rep);
	
	if(verbose) {
	    string_t name = TypeSystem$Name(Pvs(types), intfcode);
	    eprintf("*** Generating stubs for '%s'\n", name);
	    FREE(name);
	}    
	
	ANY_UNALIAS(&rep);
	intf = NARROW(&rep, Interface_clp);
	
	if(!IDCMarshalCtl$Interface(ctl, intf)) {
	    RAISE_StubGen$Failure();
	}
	
	get_ops(intf, &mops);
	
	/* XXX If someone comes up with an interface with more than
           MAX_OPS operations, we need to extend the number of stubs
           in the $(arch)_stubs.s files. */

	ASSERT(SEQ_LEN(&mops) <= MAX_OPS);

	for (i = 0; i < SEQ_LEN(&mops); i++) {
	    Operation_clp op;
	    inst_t       *buf;
	    stub_operation_info op_info;
	    Operation_Kind kind;
	    
	    code_entry *entry = NULL;
		
	    op = SEQ_ELEM(&mops, i);
	    
	    {
		Interface_clp intf;
		uint32_t n, a, r, e;
		
		kind = Operation$Info(op, &op_info.name, &intf, &n, &a, &r, &e);
		ASSERT(kind == Operation_Kind_Proc);
		op_info.announcement = False;
	    }

	    if(verbose) {
		eprintf("Operation: '%s'\n", op_info.name);
	    }
	    
	    /* XXX Shouldn't be fixed size */
	    op_info.code = Heap$Malloc(Pvs(heap), 2048); 
	    memset(op_info.code, 0, 2048);
	    buf = op_info.code;
	    
	    ok = operation_signature(ctl_st, op, &buf, &num_args, verbose);
	    
	    if(!ok) {
		
		FREE(op_info.code);
		
		RAISE_StubGen$Failure();
		
	    }
	    
	    if(verbose) {
		pretty_print_prog(op_info.code);
	    }

	    /* Now see if we ought to be using canned stubs,
               synthesising code, or using a generic stub */

	    op_info.codelen = buf - op_info.code + 1;
	    op_info.opnum = opnum++;
	    code_length += op_info.codelen;

	    if(use_canned && find_canned(op_info.code, &entry)) {
		op_info.client_fn = entry->client;
		op_info.server_fn = entry->server;
		entry->refCount++;
	    } else /* Didn't find a canned stub */{
		inst_t *i = op_info.code;
		int l = op_info.codelen;
		uint64_t hash = 0;

		/* Make hash of code ...  :-) */
		
		while(l-- > 0) {
		    hash ^= *i++;
		    hash = (hash << 1) | (hash >> 63);
		}

		if(LongCardTbl$Get(sg_st->codesTbl, hash, (addr_t *)&entry)) {
		    while(entry) {
			if(entry->len == op_info.codelen && 
			   !memcmp(op_info.code, entry->code, 
				   op_info.codelen * sizeof(inst_t))) {
			    entry->refCount++;
			    TRC(eprintf("Found a match - count %u!\n", 
					entry->refCount));
			    op_info.client_fn = entry->client;
			    op_info.server_fn = entry->server;
			    break;
			}
			entry = entry->next;
		    }

		    
		}

		if(!entry) {
		    code_entry_type code_type;
		    code_entry *next;
		    
		    TRC(eprintf("Adding new entry!\n"));
#if (defined(__IX86__) || defined(__ALPHA__))
		    if(use_synth && is_simple(op_info.code, False)) {
			TRCSYN(eprintf("Generating synthetic functions for '%s'\n",
				       op_info.name));
			
			SynthesiseFunctions(op_info.code, num_args * 8,
					    &op_info.client_fn, 
					    &op_info.server_fn);

			code_type = code_synth;

			if(verbose) {
			    eprintf("StubGen: '%s': Client at %p, Server at %p\n",
				    op_info.name, op_info.client_fn, op_info.server_fn);
			}
		    } else 
#endif
			if(use_fast && is_simple(op_info.code, True)) {
			    code_type = code_fast;
			    op_info.client_fn = Fast_Stub_C;
			    op_info.server_fn = Fast_Stub_S;
			} else {
			    code_type = code_slow;
			    op_info.client_fn = Generic_Stub_C;
			    op_info.server_fn = Generic_Stub_S;
			}

		    entry = Heap$Malloc(Pvs(heap), 
					sizeof(code_entry) + 
					op_info.codelen * sizeof(inst_t));
		    entry->code = (inst_t *)(entry + 1);
		    entry->refCount = 1;
		    entry->server = op_info.server_fn;
		    entry->client = op_info.client_fn;
		    entry->len = op_info.codelen;
		    entry->type = code_type;
		    memcpy((addr_t)entry->code, op_info.code, 
			   op_info.codelen * sizeof(inst_t));
		    
		    if(LongCardTbl$Get(sg_st->codesTbl,hash,(addr_t *)&next)) {
			entry->next = next;
		    } else {
			entry->next = NULL;
		    }
		    LongCardTbl$Put(sg_st->codesTbl, hash, entry);
		}
	    }
	    
	    if(num_args > maxnumargs) {
		maxnumargs = num_args;
	    }
	    
	    TRC(eprintf("Num args = %u, maxnumargs = %u\n", 
			num_args, maxnumargs));
	    
	    TRC(pretty_print_prog(op_info.code));
	    
	    exnlist = Operation$Exceptions(op, Pvs(heap));
	    
	    if(SEQ_LEN(exnlist)) {
		
		int exn_num;
		op_info.has_exns = True;
		
		for(exn_num = 0; exn_num < SEQ_LEN(exnlist); exn_num++) {
		    
		    string_t exnname, exn_intfname, exn_fullname;
		    Interface_clp exn_intf;
		    uint32_t num_params;
		    Interface_Needs *needs;
		    Type_Code exn_intfcode;
		    stub_exception_info *exn_info;
		    bool_t local;
		    
		    exn_clp = SEQ_ELEM(exnlist, exn_num);
		    
		    exnname = 
			Exception$Info(exn_clp, &exn_intf, &num_params);
		    
		    needs = Interface$Info(exn_intf, &local,
					   &exn_intfname,
					   &exn_intfcode);
		    SEQ_FREE(needs);
		    
		    exn_fullname = Heap$Malloc(Pvs(heap),
					       strlen(exnname) + 
					       strlen(exn_intfname) + 2);
		    
		    strcpy(exn_fullname, exn_intfname);
		    strcat(exn_fullname, "$");
		    strcat(exn_fullname, exnname);
		    
		    FREE(exn_intfname);
		    FREE(exnname);
		    
		    TRC(eprintf("Exception name : '%s'\n", exn_fullname));
		    
		    for(exn_info = SEQ_START(&exns); exn_info < SEQ_END(&exns);
			exn_info++) {
			
			TRC(eprintf("Checking against '%s'\n", 
				    exn_info->name));
			
			if(!strcmp(exn_info->name, exn_fullname)) {
			    break;
			}
		    }
		    
		    if(exn_info == SEQ_END(&exns)) {
			stub_exception_info exn_rec;
			
			exn_rec.name = exn_fullname;
			/* Add the exception record */
			exn_rec.procs = 0;
			
			exn_rec.code = Heap$Malloc(Pvs(heap), 2048);
			memset(exn_rec.code, 0, 2048);
			buf = exn_rec.code;
			
			exception_signature(ctl_st, exn_clp, &buf);
			
			exn_rec.codelen = buf - exn_rec.code + 1;
			code_length += exn_rec.codelen;
			
			TRC(pretty_print_prog(exn_rec.code));
			
			SEQ_ADDH(&exns, exn_rec);
			exn_info = SEQ_END(&exns) - 1;
		    } else {
			FREE(exn_fullname);
		    }
		    
		    /* Record that this procedure can raise the exception */
		    
		    exn_info->procs |= SET_ELEM(SEQ_LEN(&ops));
		    TRC(eprintf("proc mask = %qx\n", exn_info->procs));
		}
		
	    } else {
		op_info.has_exns = False;
	    }
	    
	    SEQ_FREE(exnlist);
		
	    SEQ_ADDH(&ops, op_info);
		
	}
	    
	/* We now have all the data we need - build the state record */

	state_size = sizeof(generic_stub_st) + 
	    SEQ_LEN(&ops) * sizeof(stub_operation_info) +
	    SEQ_LEN(&exns) * sizeof(stub_exception_info) +
	    code_length * sizeof(inst_t) + 1024 /* XXX */;

	TRC(eprintf("Mallocing %u bytes\n", state_size));

	state_mem = Heap$Malloc(h, state_size);
	if(!state_mem) RAISE_Heap$NoMemory();
	gen_st = (generic_stub_st *)state_mem;
	state_mem = gen_st + 1;

	gen_st->intfname = TypeSystem$Name(Pvs(types), intfcode);
		
	gen_st->ops = state_mem;
	state_mem += sizeof(stub_operation_info) * SEQ_LEN(&ops);

	gen_st->exns = state_mem;
	state_mem += sizeof(stub_exception_info) * SEQ_LEN(&exns);

	/* Byte code begins after the exception records */
	
	codeptr = (inst_t *)state_mem;
	
	/* Set up the back pointer that let the server stubs find their
	   state (per-stubs state, rather than per-binding state */
	
	gen_st->srv_backptr = gen_st;
	gen_st->maxargssize = maxnumargs * sizeof(uint64_t);
	

	/* Set up closures */
	
	CL_INIT(gen_st->client_cl, &GenericIDCClientStub_ms, gen_st);
	CL_INIT(gen_st->server_cl, &gen_st->srv_optable, NULL);
	CL_INIT(gen_st->surr_cl, Generic_op, NULL);
	
	/* Set up IDCStubs_Rec state */
	
	gen_st->rec.stub = &gen_st->server_cl;
	ANY_INITC(&gen_st->rec.surr, intfcode, (pointerval_t)&gen_st->surr_cl);
	gen_st->rec.clnt = &gen_st->client_cl;
	gen_st->rec.idc = BADPTR;
	gen_st->rec.c_sz = gen_st->rec.s_sz = FRAME_SIZE;
	
	gen_st->num_ops = SEQ_LEN(&ops);
	
	*(addr_t *)(&gen_st->srv_optable.Dispatch) = (addr_t)Generic_Dispatch;
	
	/* Copy in the operations - copy their byte code to the allocated
	   space after the end of the state field */
	
	for(i = 0; i < gen_st->num_ops; i++) {
	    
	    stub_operation_info   *op;
	    
	    op = &SEQ_ELEM(&ops, i);
	    
	    gen_st->ops[i] = *op;
	    gen_st->ops[i].code = codeptr;
	    
	    memcpy(codeptr, op->code, op->codelen * sizeof(inst_t));
	    codeptr += op->codelen;
	    
	}

	gen_st->num_exns = SEQ_LEN(&exns);
	
	for(i = 0; i < gen_st->num_exns; i++) {
	    
	    stub_exception_info   *exn;
	    
	    exn = &SEQ_ELEM(&exns, i);
	    
	    gen_st->exns[i] = *exn;
	    gen_st->exns[i].code = codeptr;
	    
	    memcpy(codeptr, exn->code, exn->codelen * sizeof(inst_t));
	    codeptr += exn->codelen;
	    
	}
	
	/* Store the entry for next time */
	LongCardTbl$Put(ctl_st->stubsTbl, intfcode, gen_st);
	    
    } FINALLY {

	/* Free any data structures that are hanging around */
	
	for(i = 0; i < SEQ_LEN(&ops); i++) {
	    
	    stub_operation_info *o;
	    o = &SEQ_ELEM(&ops, i);
	    FREE(o->code);
	}

	SEQ_FREE_DATA(&ops);

	for(i = 0; i < SEQ_LEN(&exns); i++) {
	    
	    stub_exception_info *e;
	    e = &SEQ_ELEM(&exns, i);
	    FREE(e->code);
	}

	SEQ_FREE_DATA(&exns);

	SEQ_FREE_DATA(&mops);

	MU_RELEASE(&sg_st->mu);

    } ENDTRY;

#if 0
    /* Add the entry to the stubs table */

    {
	
	stub_entry *entry, *oldentry = NULL;
	
	entry = Heap$Malloc(Pvs(heap), sizeof(*entry));
	    
	entry->tc = intfcode;
	entry->ctl = ctl;
	entry->stubs = st;
	entry->refCount = 1;
	
	TRC(eprintf("StubGen: Adding stubs %p (%p) to table\n", st, entry));
	LongCardTbl$Get(stubsTbl, intfcode, (addr_t *)&oldentry);
	
	if(oldentry) {
	    TRC(eprintf("StubGen: found old entry %p (%p)\n", oldentry->stubs,
			oldentry));

	    ASSERT(oldentry->tc == intfcode);
	    ASSERT(oldentry->ctl != ctl); 
	} else {
	    TRC(eprintf("StubGen: didn't find another entry\n"));
	}
	
	entry->next = oldentry;
	LongCardTbl$Put(stubsTbl, intfcode, entry);
    }	
#endif

    return &gen_st->rec;
}
    
static StubGen_Code *StubGen_GenType_m(StubGen_clp self,
			      Type_Code tc,
			      IDCMarshalCtl_clp ctl,
			      Heap_clp h) {
    StubGen_st *sg_st = self->st;
    PerCtl_st *ctl_st= NULL;
    CodeRec *cr;
    StubGen_Code *NOCLOBBER code = NULL;
    inst_t *codebuf, *buf, *ip;
    
    if(VP$DomainID(Pvs(vp)) != DOM_NEMESIS) {
	ASSERT(sg_st->realStubGen != NULL);
	return StubGen$GenType(sg_st->realStubGen, tc, ctl, NULL);
    }

    MU_LOCK(&sg_st->mu);

    TRY {

	if(! LongCardTbl$Get(sg_st->ctlTbl, (word_t)ctl, (addr_t *)&ctl_st)) {
	    ctl_st = NewCtlSt(sg_st, ctl);
	}
	    
	cr = get_code_rec(ctl_st, tc, False, False);

	if(!cr) RAISE_StubGen$Failure();

	code = SEQ_NEW(StubGen_Code, cr->codeLen + 4, Pvs(heap)); 
	
	codebuf = SEQ_START(code);
	codebuf[0] = 0;
	codebuf[1] = 0;
	codebuf[2] = 0;

	buf = codebuf + 3;
	ip = (inst_t *)(cr + 1);
	memcpy(buf, ip, cr->codeLen * sizeof(inst_t));

	*buf |= MODE_INOUT | BYREF;

	buf += cr->codeLen;
	*buf++ = SOP(SETVAL) | EOP(SETVAL);

	TRC(pretty_print_prog(codebuf));

    } FINALLY {
	MU_RELEASE(&sg_st->mu);
    } ENDTRY;

    return code;
}

static void ClientStubs_InitState(IDCClientStubs_clp self, 
				  IDCClientStubs_State *cs) {
    generic_stub_st *st = self->st;

    generic_op_st *ops = Heap$Calloc(Pvs(heap), sizeof(*ops), st->num_ops);
    int i;

    for(i = 0; i < st->num_ops; i++) {
	ops[i].info = &st->ops[i];
	ops[i].cs   = cs;
	ops[i].func = ops[i].info->client_fn;
    }

    cs->stubs = ops;
}

static void ClientStubs_DestroyState(IDCClientStubs_clp self, 
				     IDCClientStubs_State *cs) {
    
    FREE(cs->stubs);

}

#if 0
PUBLIC void Main (Closure_clp self)
{
    Interface_clp    IREF;
    Interface_clp    NOCLOBBER intf = NULL;
    Type_Any         rep;
    Context_Names   *names;
    Context_Names   *intfs;
    int              NOCLOBBER j;
    bool_t           local;
    Interface_Needs *needs;
    Type_Code        intfcode;

    printf("stubgen: entered\n");

#ifdef STATIC_VARS
    {
	LongCardTblMod_clp tblmod = NAME_FIND("modules>LongCardTblMod",
					      LongCardTblMod_clp);
	tbl = LongCardTblMod$New(tblmod, Pvs(heap));
    }
#endif

    /* Get IREF */
    IREF = TypeSystem$Info(Pvs(types), 0, &rep);

    intfs = Context$List((Context_clp) IREF);

    for (j = 0; j < SEQ_LEN(intfs); j++) {

	name = SEQ_ELEM(intfs, j);

	if(strcmp(name, "StubTest")) {
	    continue;
	}

	eprintf("INTERFACE %s\n", name);
	Wr$Flush(Pvs(out));

	TRY {
	    intf = NAME_LOOKUP(name, (Context_clp)IREF, Interface_clp);
	} 
	CATCH_TypeSystem$Incompatible() {
	    intf = NULL;
	}
	ENDTRY;
	if (!intf) continue;
	
	needs = Interface$Info(intf, &local, &name, &intfcode);
	SEQ_FREE(needs);
	FREE(name);

     	{

	    IDCStubs_Info i = 
		StubGen$GenIntf(&stubgen_cl, NULL, intfcode, Pvs(heap));

	    char buf[256];

	    sprintf(buf, "stubs>%Q", intfcode);
	    eprintf("buf = '%s'\n", buf);
	    
	    CX_ADD(buf, i, IDCStubs_Info);
	    
	    eprintf("Added Stubs_Info %p\n", i);
	    
	}

#if 0
	PAUSE(SECONDS(1));
	printf("\n\n");
#endif
    }

#ifdef STATIC_VARS

    printf("*** LARGE %d / %d\n", nlarge, nlarge+nsmall);

    printf("*** total codes: %d\n", codecount);

    {
	LongCardTblIter_clp iter = LongCardTbl$Iterate(tbl);
	code_entry *entry;
	uint64_t hash;

	while(LongCardTblIter$Next(iter, &hash, &entry)) {
	    while(entry != NULL) {
		if(entry->count > 10) {
		    eprintf("Code with %u matches\n", entry->count);
		    pretty_print_prog(entry->code);
		    eprintf("\n");
		}
		entry = entry->next;
	    }
	}
    }

#endif

}
#endif

static bool_t NullCtlIntf(IDCMarshalCtl_clp self,
			  Interface_clp intf) {
    
    return True;

}

static bool_t NullCtlOp(IDCMarshalCtl_clp self,
			Operation_clp op,
			bool_t * resultsFirst) {

    *resultsFirst = False;

    return True;
}

static bool_t NullCtlType(IDCMarshalCtl_clp self,
			  Type_Code  type,
			  IDCMarshalCtl_Code *code) {

    *code = IDCMarshalCtl_Code_Native;

    return True;

}

const string_t stubslist[] = {
    
    "SB",
    "SBPlayCtl",
    "SBMixerCtl",
    "SBRecCtl",
    "AALPod",
    "Spans",
    "FB",
    "WS",
    "USDCtl",
    "USDCallback",
    "USDDrive",
};

#if 0

Closure_Apply_fn StubCount_m;
static Closure_op sc_ms = { StubCount_m };
static const Closure_cl sc_cl = { &sc_ms, NULL };
CL_EXPORT(Closure, StubCount, sc_cl);


void StubCount_m(Closure_clp self) {

    int counts[60];
    int maxRefCount = 0, i;
    int slowCount = 0, slowCountTot = 0, fastCount = 0, fastCountTot = 0,
	synthCount = 0, synthCountTot = 0, cannedCount=0, cannedCountTot=0;
    

    void show_stub(const code_entry *code) {
	while(code) {
	    string_t typestr = "INVALID";
	    
	    switch(code->type) {
	    case code_canned:
		typestr = "Canned";
		cannedCount ++;
		cannedCountTot += code->refCount;
		break;
		
	    case code_synth:
		typestr = "Synthesised";
		synthCount++;
		synthCountTot += code->refCount;
		break;
		
	    case code_fast:
		typestr = "Fast";
		fastCount++;
		fastCountTot += code->refCount;
		break;
		
	    case code_slow:
		typestr = "Slow";
		slowCount++;
		slowCountTot += code->refCount;
		break;
		
	    default:
		ASSERT(False);
		break;
	    }
	    
	    eprintf("%s Stub : refCount %u\n", typestr, code->refCount);
	    counts[code->refCount]++;
	    maxRefCount = MAX(maxRefCount, code->refCount);
	    
	    code = code->next;
	}
    }


    PAUSE(SECONDS(30));
#if 0
    for(i = 0; i < sizeof(stubslist) / sizeof(string_t); i++) {
	Type_Code tc = NAME_LOOKUP(stubslist[i], (Context_clp)Pvs(types), Type_Code);
	info = StubGen$GenIntf(&stubgen_cl, tc, NULL, Pvs(heap));
    }
#endif    

    {
	Context_clp ctx = NAME_FIND("genstubsNemesis", Context_clp);
	Type_Code tc;
	uint64_t hash;
	stub_entry *stub;
	code_entry *code;
    
	LongCardTbl_clp stubsTbl = NAME_LOOKUP("types", ctx, LongCardTbl_clp);
	LongCardTbl_clp codesTbl = NAME_LOOKUP("codes", ctx, LongCardTbl_clp);

	LongCardTblIter_clp stubsIter = LongCardTbl$Iterate(stubsTbl);
	LongCardTblIter_clp codesIter = LongCardTbl$Iterate(codesTbl);

	eprintf("%d stubs\n", LongCardTbl$Size(stubsTbl));

	while(LongCardTblIter$Next(stubsIter, &tc, (addr_t *)&stub)) {
	    string_t name = TypeSystem$Name(Pvs(types), tc);

	    eprintf("%qx:'%20s' : refCount = %u\n", tc, name, stub->refCount);

	    FREE(name);
	}

	LongCardTblIter$Dispose(stubsIter);

	memset(counts, 0, sizeof(counts));

	while(LongCardTblIter$Next(codesIter, &hash, (addr_t *)&code)) {
	    
	    show_stub(code);
	    if(code->refCount > 3) {
		pretty_print_prog(code->code);
	    }
	}

	i = 0;
	while(canned_table[i].code != NULL) {
	    if(canned_table[i].refCount) {
		show_stub(&canned_table[i]);
		pretty_print_bytecodes(canned_table[i].code);
	    }
	    i++;
	}

	eprintf("Reuse frequencies:\n");
	
	for (i = 0; i <= maxRefCount; i++) {
	    eprintf("%2u: %-3u\n", i, counts[i]);
	}

	eprintf("Canned: %2u, total %2u\n", cannedCount, cannedCountTot);
	eprintf("Fast:   %2u, total %2u\n", fastCount, fastCountTot);
	eprintf("Slow:   %2u, total %2u\n", slowCount, slowCountTot);
	eprintf("Synth:  %2u, total %2u\n", synthCount, synthCountTot);
	LongCardTblIter$Dispose(codesIter);
    }
}

#endif
