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
 **      ./sys/TypeSystem/TypeSystem.c
 ** 
 ** FUNCTIONAL DESCRIPTION:
 ** 
 **      Implements TypeSystem.if; ie. the top-level interface to the
 **	Nemesis type system at runtime
 ** 
 ** ENVIRONMENT: 
 **
 **      Static, reentrant, anywhere
 ** 
 ** ID : $Id: TypeSystem.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
 ** 
 **
 */

#include <nemesis.h>
#include <contexts.h>
#include <exceptions.h>
#include <string.h>
#include <stdio.h>
#include <ntsc.h>

#include <Pervasives.ih>
#include <TypeSystemMod.ih>

#include "TypeSystem_st.h"
	    
/*
 * Statics
 */

/* shared between TypeSystem and Metainterface */
static Context_Add_fn 		Add;
static Context_Remove_fn 	Remove;
static Context_Dup_fn 		Dup;
static Context_Destroy_fn	Destroy;

/* exclusive to the TypeSystem interface */
static Context_List_fn                   Ts_List;
static Context_Get_fn                    Ts_Get;
static TypeSystem_Info_fn                Ts_Info;
static TypeSystem_Size_fn                Ts_Size;
static TypeSystem_Name_fn                Ts_Name;
static TypeSystem_IsLarge_fn             Ts_IsLarge;
static TypeSystem_IsType_fn              Ts_IsType;
static TypeSystem_Narrow_fn              Ts_Narrow;
static TypeSystem_UnAlias_fn             Ts_UnAlias;
static TypeSystemF_RegisterIntf_fn       Ts_RegisterIntf;

/* excusive to the Metainterface interface */
static Context_List_fn 		If_List;
static Context_Get_fn		If_Get;
static Interface_Extends_fn	If_Extends;
static Interface_Info_fn	If_Info;

/*
 * TypeSystem closure stuff 
 */
static TypeSystemF_op typesystem_ops = {
    Ts_List,
    Ts_Get,
    Add, 
    Remove,
    Dup,
    Destroy,
    Ts_Info,
    Ts_Size,
    Ts_Name,
    Ts_IsLarge,
    Ts_IsType,
    Ts_Narrow,
    Ts_UnAlias,
    Ts_RegisterIntf
};

/*
 * Meta-interface closure stuff
 */
static Interface_op meta_ops = {
    If_List,
    If_Get,
    Add, 
    Remove,
    Dup,
    Destroy,
    If_Extends,
    If_Info,
};

PUBLIC const Interface_cl meta_cl = { &meta_ops, BADPTR /*&IREF__intf*/ };

/*
 * Method suites for all type representation which include a closure
 */ 
extern Record_op   	record_ops;
extern Enum_op 		enum_ops;
extern Choice_op   	choice_ops;
extern Interface_op 	interface_ops;
extern Operation_op	operation_ops;
extern Exception_op     exception_ops;

/*
 * Module stuff
 */
static	TypeSystemMod_New_fn 	TypeSystemMod_New_m;
static	TypeSystemMod_op	ms = { TypeSystemMod_New_m };
static	const TypeSystemMod_cl	cl = { &ms, NULL };

CL_EXPORT (TypeSystemMod, TypeSystemMod, cl);
CL_EXPORT_TO_PRIMAL (TypeSystemMod, TypeSystemModCl, cl);
/* 
** If you want to get output re: narrows failing, #define NARROW_TRACE 
** in config.mk (or edit it in place)
*/
#ifdef NARROW_TRACE
#define NTRC(_x) _x
#else
#define NTRC(_x)
#endif


/* 
 * Creator
 */
static TypeSystemF_clp 
TypeSystemMod_New_m (TypeSystemMod_cl *self, 
		     Heap_clp h,
		     LongCardTblMod_clp l,
		     StringTblMod_clp s)
{
    extern void Register_Interfaces(TypeSystem_cl *ts);
    TypeSystem_st *res;

    res=Heap$Malloc(h, sizeof(*res));
    CL_INIT (res->cl, &typesystem_ops, res);

    res->intfsByTC   = LongCardTblMod$New (l, h);
    res->intfsByName = StringTblMod$New (s, h);

    /* Could do with some base types! */
    Ts_RegisterIntf(&res->cl, (TypeSystemF_IntfInfo)&IREF__intf);

    /* The meta-interface closure (IREF) is of type "Interface" but
       has different methods since it's state record is TypeSystem_st
       rather than Intf_st. */
    *(addr_t *)(&meta_cl.st) = res;  /* XXX n.b. this writes 'const' data */

    return &res->cl;
}

/* 
 * Interface Registration
 */
static void
Ts_RegisterIntf (TypeSystemF_cl	*self, TypeSystemF_IntfInfo intf)
{
    TypeSystem_st	*st      = (TypeSystem_st *) self->st;
    Intf_st	*intf_st = (Intf_st *) intf;
    Intf_st	*previous;

    if (StringTbl$Get (st->intfsByName, intf_st->rep.name,
		       (addr_t*) &previous))
	RAISE_TypeSystemF$NameClash();

    if (LongCardTbl$Get (st->intfsByTC, intf_st->rep.code.val,
			 (addr_t*) &previous))
	RAISE_TypeSystemF$TypeCodeClash();

    if (intf != &IREF__intf) {
	Interface_clp iclp = (Interface_clp)(word_t) intf_st->rep.any.val;
	addr_t clp;
	int i;

	/* Fill in operation tables of closures */
	iclp->op = &interface_ops;

	/* Types */
	for (i = 0; i < intf_st->num_types; i++) {
	    clp = (addr_t) (pointerval_t) intf_st->types[i]->any.val;
	    switch (intf_st->types[i]->any.type) {
	    case TypeSystem_Choice__code:
		((Choice_clp)clp)->op = &choice_ops;
		break;
	    case TypeSystem_Enum__code:
		((Enum_clp)clp)->op = &enum_ops;
		break;
	    case TypeSystem_Record__code:
		((Record_clp) clp)->op = &record_ops;
		break;
	    default:
		
	    }
	}
	/* Operations */
	for (i = 0; i < intf_st->num_methods; i++) {
	    intf_st->methods[i]->cl->op = &operation_ops;
	}

	/* Exceptions */
	for (i = 0; i < intf_st->num_excepts; i++) {
	    *(Exception_op **)&intf_st->exns[i]->cl.op = &exception_ops;
	}
    }

    StringTbl$Put   (st->intfsByName, intf_st->rep.name, intf);
    LongCardTbl$Put (st->intfsByTC, intf_st->rep.code.val, intf);

    /* bump the "IREF" pseudo-interface num_types */
    *(word_t *)(&IREF__intf.num_types) += 1;
}



/********************************************************/
/*							*/
/*	Internal calls				       	*/
/*							*/
/********************************************************/

/*
 * Look up a type name in this interface
 */
static TypeRep_t *Int_Get ( TypeSystem_st *st, string_t name )
{
    string_t	 	extra = strpbrk (name, SEP_STRING ".");
    char           	sep   = '\0';
    Intf_st		*b;
    TypeRep_t	        *res = NULL; /* until proven innocent */

    if (extra)
    {
	sep      = *extra;	/* save separator */
	*extra++ = '\0';
    }

    /* now "name" is just the interface, and "extra" is any extra qualifier */

    if (StringTbl$Get (st->intfsByName, name, (addr_t*)&b))
    {
	/* We've found the first component. */
	if (extra)
	{
	    int i;
      
	    /* Simple linear search of type declarations */
	    for (i = 0; b->types[i]; i++)
		if (strcmp (extra, b->types[i]->name) == 0)
		    res = b->types[i];

	    /* special case if it's an intf type defined by the metainterface */
	    if (!res && b == &IREF__intf) {
		if(extra) 
		    res = Int_Get (st, extra);
		else res= &(IREF__intf.rep);
	    }
	}
	else
	{
	    /* Otherwise return this interface clp */
	    res = &(b->rep);
	}
    }    
  
    if (extra)
	*(--extra) = sep;	/* restore the separator we nuked above */
  
    return res;
}

/*********************************************************
 *
 *	Common (context) Operations
 *
 *********************************************************/

/*
 * Add, Remove, Dup and Destroy methods are all nulls.
 */
static void Add(Context_cl *self, string_t name, const Type_Any *obj )
{ RAISE_Context$Denied(); return; }
static void Remove(Context_cl *self, string_t name )
{ RAISE_Context$Denied(); return; }
static Context_clp Dup(Context_cl *self, Heap_clp h, WordTbl_clp xl)
{ RAISE_Context$Denied(); return NULL; }
static void Destroy( Context_cl *self )
{ return; }


/*********************************************************
 *
 *	Type System Operations
 *
 *********************************************************/

/*
 *  Return a list of all types in the TypeSystem
 */
static Context_Names *Ts_List (Context_cl *self)
{
    TypeSystem_st			*st  = (TypeSystem_st *) self->st;
    NOCLOBBER StringTblIter_clp	iter = NULL;
    Context_Names			*seq;

    TRC(printf("TypeSystem$List: called\n"));
    
    /* Get the result sequence */
    seq = SEQ_CLEAR (SEQ_NEW (Context_Names, 4*StringTbl$Size (st->intfsByName),
			      Pvs(heap)));

    /* Run through all the interfaces */
    TRY
    {
	string_t		name;
	Intf_st	       *tb;
	TypeRep_t	      **trep ;

	iter = StringTbl$Iterate (st->intfsByName);

	while (StringTblIter$Next (iter, &name, (addr_t*)&tb))
	{
	    AddName (tb->rep.name, Pvs(heap), seq);

	    /* Run through all the types defined in the current interface */
	    for (trep = (TypeRep_t **)tb->types; *trep; trep++)
		AddQualName ( (*trep)->name, tb->rep.name, Pvs(heap), seq);
	}
	StringTblIter$Dispose (iter);
    }
    CATCH_ALL {
		  DB(printf("TypeSystem$List: failed in Malloc: undoing.\n"));
		  if (iter) StringTblIter$Dispose (iter);
		  SEQ_FREE_ELEMS (seq);
		  SEQ_FREE (seq);
		  DB(printf("TypeSystem$List: done undoing.\n"));
		  RAISE_Heap$NoMemory();
	      }
    ENDTRY;

    TRC(printf("TypeSystem$List: done.\n"));
    return seq;
}

/* 
 * Type System Get method - return the type code as any.
 */ 
static bool_t Ts_Get( Context_cl *self, string_t name, Type_Any *o )
{
    TypeRep_t *tr = Int_Get ((TypeSystem_st *) self->st,  name);
  
    if (!tr)
	return False;

    ANY_COPY (o, &(tr->code));
    return True;
}


/*
 * Info returns information about a type given its typecode
 */
static Interface_clp Ts_Info(TypeSystem_cl	*self,
			     Type_Code	tc,
			     Type_Any	*rep)

{
    TypeSystem_st *st = (TypeSystem_st *) self->st;
    Intf_st	*b;
    TypeRep_t	*tr;

    /* Check the type code refers to a valid interface */
    if (!LongCardTbl$Get (st->intfsByTC, TCODE_INTF_CODE (tc), (addr_t*)&b))
	RAISE_TypeSystem$BadCode(tc);

    /* Deal with the case where the type code refers to an interface type */
    if (TCODE_IS_INTERFACE (tc)) 
    {
	ANY_COPY (rep, &(b->rep.any));
      
	return (Interface_clp) &meta_cl;
    }
  
    /* Check that within the given interface this is a valid type */
    if (! TCODE_VALID_TYPE (tc, b))
	RAISE_TypeSystem$BadCode (tc);

    tr = TCODE_WHICH_TYPE (tc, b);
  
    ANY_COPY (rep, (Type_Any *)&tr->any);

    return (Interface_clp) (word_t) (b->rep.any.val);
}

/*
 * Return the size of a type
 */
static Heap_Size Ts_Size(TypeSystem_cl	*self,
			 Type_Code	tc )
{
    TypeSystem_st *st = (TypeSystem_st *) self->st;
    Intf_st	*b  = NULL;

    /* Check the type code refers to a valid interface */
    if (!LongCardTbl$Get (st->intfsByTC, TCODE_INTF_CODE (tc), (addr_t*)&b))
	RAISE_TypeSystem$BadCode(tc);

    /* Deal with the case where the type code refers to an interface type */
    if (TCODE_IS_INTERFACE (tc))
	return b->rep.size;
  
    /* Check that within the given interface this is a valid type */
    if (! TCODE_VALID_TYPE (tc, b))
	RAISE_TypeSystem$BadCode(tc);

    return TCODE_WHICH_TYPE(tc, b)->size;
}

static Type_Name Ts_Name(TypeSystem_clp self, Type_Code tc) {

    TypeSystem_st *st   = (TypeSystem_st *) self->st;
    Intf_st	  *b    = NULL;
    Type_Name      name = NULL;
    TypeRep_t	  *tr;

    /* Check the type code refers to a valid interface */
    if (!LongCardTbl$Get (st->intfsByTC, TCODE_INTF_CODE (tc), (addr_t*)&b))
	RAISE_TypeSystem$BadCode(tc);

    /* Deal with the case where the type code refers to an interface type */
    if (TCODE_IS_INTERFACE (tc)) {
	name = strdup(b->rep.name);
	if(!name) RAISE_Heap$NoMemory();
	return name;
    }

    /* Check that within the given interface this is a valid type */
    if (! TCODE_VALID_TYPE (tc, b))
	RAISE_TypeSystem$BadCode (tc);

    tr = TCODE_WHICH_TYPE (tc, b);
    name = strdup(tr->name);
    if(!name) RAISE_Heap$NoMemory();

    return name;
}
    
/*
 * Return whether a type is large
 */
static bool_t Ts_IsLarge (TypeSystem_cl *self, Type_Code tc )
{
    TypeSystem_st *st = (TypeSystem_st *) self->st;
    Intf_st	*b;
    TypeRep_t	*tr;

    /* Check the type code refers to a valid interface */
    if (!LongCardTbl$Get (st->intfsByTC, TCODE_INTF_CODE (tc), (addr_t*)&b))
	RAISE_TypeSystem$BadCode(tc);

    /* Deal with the case where the type code refers to an interface type */
    if (TCODE_IS_INTERFACE (tc)) 
	return False;

    /* Check that within the given interface this is a valid type */
    if (! TCODE_VALID_TYPE (tc, b))
	RAISE_TypeSystem$BadCode(tc);

    tr = TCODE_WHICH_TYPE (tc, b);

    if (tr->any.type == TypeSystem_Alias__code) {
	/* XXX - what if word shorter? */
	return Ts_IsLarge (self, ((Type_Any *)&tr->any)->val); 
    } else {
	return ( tr->any.type != TypeSystem_Predefined__code &&
		tr->any.type != TypeSystem_Enum__code       &&
		tr->any.type != TypeSystem_Set__code        &&
		tr->any.type != TypeSystem_Ref__code        &&
		tr->any.type != TypeSystem_Iref__code );
    }
}

/*
 * Try to find out if two types are compatible
 */
static bool_t Ts_IsType(TypeSystem_cl *self,
			Type_Code sub, 
			Type_Code super )
{
    TypeSystem_st *st = (TypeSystem_st *) self->st;
    Intf_st	*b;

    /* Check the type code refers to a valid interface */
    if (!LongCardTbl$Get (st->intfsByTC, 
			  TCODE_INTF_CODE (super), 
			  (addr_t*)&b)) {
	eprintf("TypeSystem$IsType: unknown typecode (super=%lx)\n", super);
	RAISE_TypeSystem$BadCode (super);
    }

    /* Quick and dirty check for equality */
    if (sub == super) {
	return True;
    }

    /* Check the type code refers to a valid interface */
    if (!LongCardTbl$Get (st->intfsByTC, TCODE_INTF_CODE (sub), (addr_t*)&b)) {
	eprintf("TypeSystem$IsType: unknown typecode (sub=%lx)\n", sub);
	RAISE_TypeSystem$BadCode (sub);
    }

    /* Deal with the case where the type code refers to an interface type */
    if (TCODE_IS_INTERFACE (sub)) 
    {
	while (b->rep.code.val != super) 
	{
	    /* Look up the supertype */
	    if ( !b->extends ) {
		return False;
	    }
	    if (!LongCardTbl$Get (st->intfsByTC, b->extends, (addr_t*)&b)) {
		eprintf("TypeSystem$IsType: unknown typecode (super=%lx)\n", 
			b->extends);
		RAISE_TypeSystem$BadCode (sub);
	    }
	}
	return True;
    }

    /* We have a concrete type and it's not the same typecode, so fail. */
    return False;
}

/*
 * Try to narrow a type  
 */
static Type_Val Ts_Narrow(TypeSystem_cl *self,
			  const Type_Any *a, 
			  Type_Code tc )
{
    if (!Ts_IsType (self, a->type, tc)) {
	NTRC(printf("TS$Narrow: any's type (%qx) incompat with desired %qx\n",
		   a->type, tc));
	RAISE_TypeSystem$Incompatible();
    }
    
    return a->val;
}

/*
 * UnAlias a type
 */
static Type_Code Ts_UnAlias(TypeSystem_cl	*self,
			    Type_Code a )
{
    TypeSystem_st *st = (TypeSystem_st *) self->st;
    Intf_st	*b  = NULL;
    TypeRep_t	*tr = NULL;

    for(;;) {
	/* Check the type code refers to a valid interface */
	if (!LongCardTbl$Get (st->intfsByTC, TCODE_INTF_CODE (a), (addr_t*)&b))
	    RAISE_TypeSystem$BadCode (a);
	
	/* Deal with the case where the type code refers to an interface type */
	if (TCODE_IS_INTERFACE (a))
	    return a;
	
	/* Check that within the given interface this is a valid type */
	if (! TCODE_VALID_TYPE (a, b))
	    RAISE_TypeSystem$BadCode(a);
	
	/* Get the representation of this type */
	tr = TCODE_WHICH_TYPE (a, b); 

	/* If it's not an alias, return it */
	if ( tr->any.type != TypeSystem_Alias__code ) return a;
	
	/* Else go round again. */
	a = ((Type_Any *)&tr->any)->val;
    }
    RAISE_TypeSystem$BadCode (a);
}



/*********************************************************
 *
 *	Meta-interface Operations
 *
 *********************************************************/

/*
 *  Return a list of all types in the TypeSystem
 */
static Context_Names *If_List(Context_cl *self )
{
    TypeSystem_st		       *st   = (TypeSystem_st *) self->st;
    NOCLOBBER StringTblIter_clp	iter = NULL;
    Context_Names		       *seq;

    TRC(printf("IREF$List: called\n"));
    
    /* Get the result sequence */
    seq = SEQ_CLEAR (SEQ_NEW (Context_Names, IREF__intf.num_types, Pvs(heap)));
 
    TRY
    {
	StringTblIter_clp	iter = StringTbl$Iterate (st->intfsByName);
	string_t		name;
	Intf_st	       *tb;
	TypeRep_t	      **trep ;

	/* Run through all the predefined types */
	for (trep = (TypeRep_t **)IREF__intf.types; *trep; trep++)
	    AddName ((*trep)->name, Pvs(heap), seq);

	/* then all the others */
	iter = StringTbl$Iterate (st->intfsByName);

	while (StringTblIter$Next (iter, &name, (addr_t*)&tb))
	    AddName (tb->rep.name, Pvs(heap), seq);

	StringTblIter$Dispose (iter);
    }
    CATCH_ALL
    {
	DB(printf("IREF_List: failed in Malloc: undoing.\n"));
	if (iter) StringTblIter$Dispose (iter);
	SEQ_FREE_ELEMS (seq);
	SEQ_FREE (seq);
	RERAISE;
    }
    ENDTRY;

    TRC(printf("IREF_List: done.\n"));
    return seq;
}

/*
 * Meta-interface Get method - this differs from Ts_Get in that 
 * we have to deal with PROCs and EXCEPTIONs as well as TYPEs.
 */ 
static bool_t If_Get( Context_cl *self, string_t name, Type_Any *o )
{
    TypeSystem_st *st = self->st;
    Intf_st *b;
    string_t	extra = strpbrk (name, SEP_STRING ".");
    char     	sep   = '\0';
    TypeRep_t   **trep;
    bool_t	res = False;	/* until proven innocent */

    /* First we check for builtin types (e.g. STRING, CHAR, etc.) */
    for (trep = (TypeRep_t **)IREF__intf.types; *trep; trep++)
    {
	if (strcmp ((*trep)->name, name) == 0)
	{
	    ANY_COPY (o, &((*trep)->any));
	    return True;
	}	
    } 

    /* Otherwise look up the leading component of "name" */
  
    if (extra)
    {
	sep      = *extra;	/* save separator */
	*extra++ = '\0';
    }
  
    /* now "name" is just the interface, and "extra" is any extra qualifier */

    if (StringTbl$Get (st->intfsByName, name, (addr_t*)&b))
    {
	/* We've found the first component. If there are no more components,
	   then simply return the Type_Any; otherwise, have to recurse a bit. */
	if (!extra)
	{
	    ANY_COPY (o, &(b->rep.any));
	    res = True;
	}
	else
	{
	    Context_clp cx; 
	    /* in this case, need first half to be a context */
	    if (! Ts_IsType ((TypeSystem_clp) &st->cl,
			     b->rep.any.type, Context_clp__code))
		RAISE_Context$NotContext();

	    cx  = (Context_clp) (word_t) Ts_Narrow ((TypeSystem_clp) &st->cl,
						    (Type_Any *)&(b->rep.any), 
						    Context_clp__code);
	    res = Context$Get (cx, extra, o);
	}
    }
  
    if (extra)
	*(--extra) = sep;	/* restore the separator we nuked above */
  
    return res;
}


/*
 * Extends method : the meta-interface does not extend anything
 */
static bool_t If_Extends( Interface_cl *self, Interface_clp *i )
{
    return False;
}

/*
 * Info method : return information about the meta-interface 
 */
static Interface_Needs *If_Info(Interface_cl *self,
				bool_t *local,
				Type_Name *name,
				Type_Code *code )
{
    Interface_Needs *needs = SEQ_NEW(Interface_Needs, 0, Pvs(heap));
    *local = False;
    *name  = strdup(TCODE_META_NAME);
    *code  = IREF_clp__code;
    return needs;
}

/*********************************************************
 *
 *	Adding names to sequences
 *
 *********************************************************/

void AddName(string_t name,
	     Heap_clp h,
	     Context_Names *seq )
{
    string_t copy = strduph (name, h);

    if (! copy )
	RAISE_Heap$NoMemory();

    SEQ_ADDH (seq, copy);

    TRC(printf("TypeSystem$AddName: added '%s'\n", copy));
}

void AddQualName(string_t name,
		 string_t ifname,
		 Heap_clp h,
		 Context_Names *seq )
{
    string_t	 text;

    if (!(text = Heap$Malloc(h,
			     strlen(ifname) + 1 +
			     strlen(name) + 1 ) ) )
	RAISE_Heap$NoMemory();
  
    strcpy(text, ifname);
    strcat(text, "." );
    strcat(text, name );

    SEQ_ADDH (seq, text);

    TRC(printf("TypeSystem$AddQualName: added '%s'\n", text));
}

