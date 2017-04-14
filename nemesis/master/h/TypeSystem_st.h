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
**      TypeSystem internal data structures
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Describes all the types in the Nemesis type system
** 
** ENVIRONMENT: 
**
**      User space
** 
** ID : $Id: TypeSystem_st.h 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#ifndef _TypeSystem_st_h_
#define _TypeSystem_st_h_

#include <Operation.ih>
#include <TypeSystem.ih>
#include <TypeSystemF.ih>
#include <LongCardTbl.ih>
#include <StringTbl.ih>

/* #define TYPESYSTEM_TRACE */
#ifdef TYPESYSTEM_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x)
#endif


/*
 * Parameter type : used in operations
 */
typedef const struct Param_t Param_t;
struct Param_t {
    Operation_Parameter	rep;
    string_t		name;
};

/*
 * Field type : used in Records, Choices, Enumerations and Exceptions
 */
typedef const struct Field_t Field_t;
struct Field_t {
    Type_AnyI	val;
    string_t  	name;
};

/*
 * Enumeration and record type state
 */
typedef const struct EnumRecState_t EnumRecState_t;
struct EnumRecState_t {
    word_t 	 num;
    Field_t	*elems; 	/* val = enum. value or record type code */
};


/*
 * Choice type state
 */
typedef const struct ChoiceState_t ChoiceState_t;
struct ChoiceState_t {
    EnumRecState_t ctx;
    Type_Code 	 disc;		/* discriminator	*/
};

/*
 * Representations of interfaces.
 */

/* Forward declarations */
typedef const struct Intf_st		Intf_st;
typedef const struct Operation_t 	Operation_t;
typedef const struct Exc_t 		Exc_t;
typedef const struct ExcRef_t           ExcRef_t;
typedef const struct TypeRep_t	TypeRep_t;

/* Type within an interface */
struct TypeRep_t {
    Type_AnyI 	any;		/* Class-specific stuff		*/
    Type_Any 	code;		/* Type code of type, as an any	*/
    Type_Name   name;		/* Name of type			*/
    Intf_st     *intf;		/* Pointer to defining interface*/
    Heap_Size	size;		/* Size of an instance		*/
};
/* Interface itself */
struct Intf_st {
    TypeRep_t	 rep;		/* All the above		*/
    Type_Code   const *needs;	/* List of needed interfaces	*/
    word_t 	 num_needs;	/* How many of them are there	*/
    TypeRep_t   * const *types; /* List of defined types	*/
    word_t 	 num_types;	/* How many of them are there	*/
    bool_t	 local;		/* Is this interface local?	*/
    Type_Code 	 extends;	/* Supertype, if any 		*/
    Operation_t  * const *methods; /* Table of methods		*/
    word_t	 num_methods;	/* No. of methods we define 	*/
    Exc_t	 * const *exns;	/* Table of exceptions		*/
    word_t	 num_excepts;	/* No. of exceptions we define 	*/
};
/* Operation types */

/* Operations */
struct Operation_t {
    string_t 	   name;	/* Name of Operation		*/
    Operation_Kind kind;	/* Type of Operation		*/
    Param_t       *params;	/* Array of parameters		*/
    uint32_t	   num_args;	/* How many there are		*/
    uint32_t	   num_res;	/* How many there are		*/
    uint32_t	   proc_num;	/* Procedure number		*/
    ExcRef_t      *exns;        /* Array of exceptions          */       
    uint32_t       num_excepts; /* Number of exceptions         */
    Operation_clp  cl;		/* Closure for operation	*/
};

struct ExcRef_t {
    Type_Code      intf;
    uint32_t       num;
};

/* Exceptions */
struct Exc_t {
    EnumRecState_t params;      /* Parameters - must be first   */
    Exception_cl cl;            /* Closure for exception        */
    Intf_st    *intf;           /* Defining interface           */
    string_t 	name;		/* Name of exception		*/
};



/* internal function for adding copies of names to sequences with ADDH */
extern void AddName(string_t name,
		    Heap_clp h,
		    Context_Names *seq );
extern void AddQualName(string_t name,
			string_t ifname,
			Heap_clp h,
			Context_Names *seq );

/*
 * Macros for manipulating type codes
 */
#define TCODE_META_NAME 	"IREF"
#define TCODE_MASK		((Type_Code) 0xffffUL)
#define TCODE_INTF_CODE(_tc)	((_tc) & ~TCODE_MASK)
#define TCODE_IS_INTERFACE(_tc) (!((_tc) & TCODE_MASK))

#define TCODE_VALID_TYPE(_tc,_b) \
( ((_tc) & TCODE_MASK) <= (_b)->num_types)

#define TCODE_WHICH_TYPE(_tc,_b) \
    (((_b)->types)[ ((_tc) & TCODE_MASK) - 1 ])

#define TCODE_NONE		0xffffffffffffffffULL


/*
 *  The 'mythical' interface IREF which
 *	defines all predefined Middl types and all interfaces in Nemesis
 *	(including IREF).
 */

#define IREF_clp__code 0
    extern Intf_st IREF__intf;


/* 
 * TypeSystem state
 */
typedef struct {
    TypeSystemF_cl	        cl;
    LongCardTbl_clp		intfsByTC; /* Type.Code -> (Intf_st *) */
    StringTbl_clp		intfsByName; /* string_t  -> (Intf_st *) */
} TypeSystem_st;



#endif /* _TypeSystem_st_h_ */

    
