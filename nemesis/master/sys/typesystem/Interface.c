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
**      Interface.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements Interface.if; ie. the way an interface type definition
**	is presented to the running Nemesis system.
** 
** ENVIRONMENT: 
**
**      Static, reentrant, anywhere
** 
** ID : $Id: Interface.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <contexts.h>
#include <exceptions.h>
#include <string.h>
#include <ntsc.h>
#include <stdio.h>

#include "TypeSystem_st.h"


/*
 * Statics
 */
static Context_List_fn 		List;
static Context_Get_fn 		Get;
static Context_Add_fn 		Add;
static Context_Remove_fn 	Remove;
static Context_Dup_fn	 	Dup;
static Context_Destroy_fn	Destroy;
static Interface_Extends_fn 	Extends;
static Interface_Info_fn 	Info;

/*
 * Module stuff
 */
Interface_op interface_ops = {
  List,
  Get,
  Add, 
  Remove,
  Dup,
  Destroy,
  Extends,
  Info
};


/*********************************************************
 *
 *	Interface Operations
 *
 *********************************************************/

/*
 *  Return a list of all types in the Interface
 *  17/11/94 smh: also now returns list of all exceptions and
 *  operations within the Interface. 
 */
static Context_Names *List(Context_cl *self )
{
  Intf_st *st = (Intf_st *)(self->st);
  Context_Names  *seq;
  NOCLOBBER int i;

  TRC(pr("Interface$List: called\n"));
    
  /* Get the result sequence */
  seq = SEQ_CLEAR (SEQ_NEW (Context_Names, st->num_types, Pvs(heap)));
  
  /* Run through all the stuff defined in the current interface */
  TRY {
    for( i=0; i < st->num_types; i++ ) {
      TypeRep_t *trep = (st->types)[i];
      AddName( trep->name, Pvs(heap), seq );
    }

    for( i=0; i < st->num_methods; i++ ) {
      Operation_t *op = (st->methods)[i];
      AddName( op->name, Pvs(heap), seq );
    }

    for( i=0; i < st->num_excepts; i++ ) {
      AddName( (st->exns[i])->name, Pvs(heap), seq );
    }

  }
  CATCH_ALL {
    DB(pr("Interface$List: failed in Malloc: undoing.\n"));
    SEQ_FREE_ELEMS (seq);
    SEQ_FREE (seq);
    DB(pr("Interface$List: done undoing.\n"));
    RERAISE;
  }
  ENDTRY;
  
  TRC(pr("Interface$List: done.\n"));
  return seq;
}

/*
 * Look up a type name in this interface
 * 17/11/94 smh: added in stuff for operations and exceptions too
 */
static bool_t Get( Context_cl *self, string_t name, Type_Any *o )
{
  Intf_st *st = (Intf_st *)(self->st);
  NOCLOBBER bool_t found= False;	
  string_t first, rest;
  int i;


  TRY {
    first= strduph(name, Pvs(heap));
  } CATCH_ALL {
    first=(string_t)0;
  } ENDTRY;
  
  if(!first) {
    return False;
  }
  
  for(rest= first; *rest && *rest!=SEP; )
    rest++;
  
  if(*rest) {	/* got compound */
    *rest= '\0';	
    rest++;
  } else {
    rest= (string_t)0;
  }
  
  /* First check the type declations */
  for(i=0; !found && i<st->num_types; i++ ) {
    if(!(strcmp(first, (st->types)[i]->name))) {
      ANY_COPY(o,&((st->types)[i]->any));
      found= True;
    }
  }
  
  /* Second, check the operations */
  for( i=0; !found && i<st->num_methods; i++ ) {
    if(!(strcmp(first, (st->methods)[i]->name))) {
      Type_Any opAny;
      
      opAny.type= Operation_clp__code;
      opAny.val= (pointerval_t)(((Operation_t *)st->methods[i])->cl);
      ANY_COPY(o,&opAny);
      found= True;
    }
  }
  
  /* Finally, check the exceptions */
  for( i=0; !found && i<st->num_excepts; i++ ) {
    if(!(strcmp(first, (st->exns)[i]->name))) {
      Type_Any exnAny;
      
      exnAny.type= Exception_clp__code;
      exnAny.val= (pointerval_t)&(((Exc_t *)st->exns[i])->cl);
      ANY_COPY(o,&exnAny);
      found= True;
    }
  }
  
  if(!found) 
    return(False);
  
  /* at this point, we've matched 'first'. If there's no rest, then 
     we're all done; if there is, however, may have to recurse. */
  if(!rest) 
    return(True);
  
  if(o->type==Operation_clp__code) /* this is only possibility for sub-ctxt */
    return (Context$Get ((Context_clp) (word_t) o->val, rest, o));
  
  return(False);
}
  
/*
 * Add, Remove and Destroy methods are all nulls.
 */
static void Add(Context_cl *self, string_t name, const Type_Any *obj )
{ RAISE_Context$Denied(); return; }
static void Remove(Context_cl *self, string_t name )
{ RAISE_Context$Denied(); return; }
static Context_clp Dup(Context_cl *self, Heap_clp h, WordTbl_clp xl )
{ RAISE_Context$Denied(); return NULL; }
static void Destroy( Context_cl *self )
{ return; }

/* 
 * Returns the supertype of an interface, if one exists
 */
static bool_t Extends( Interface_cl *self, Interface_clp *i )
{
  Intf_st *st = (Intf_st *)(self->st);
  
  if ( st->extends )
    {
	TypeSystem_st *tsst = (TypeSystem_st *) (Pvs(types)->st);
	Intf_st *super;
	if(!LongCardTbl$Get(tsst->intfsByTC, st->extends, (addr_t *)&super)) {
	    eprintf("Interface$Extends: TS is corrupted!\n");
	    ntsc_dbgstop();
	}

	*i = (Interface_clp)(pointerval_t)super->rep.any.val;
	return True;
    }
  else
    {
      return False;
    }
}

/*
 * Return information about an interface
 */
static Interface_Needs *Info(Interface_cl *self
		 /* RETURNS */,
		 bool_t		*local,
		 Type_Name	*name,
		 Type_Code	*code )
{
  Intf_st *st = (Intf_st *)(self->st);
  Interface_Needs *needs = SEQ_NEW(Interface_Needs, 0, Pvs(heap));
  const Type_Code *ncode = st->needs;
  
  TypeSystem_st *ts_st = Pvs(types)->st; /* XXX */

  TRC(pr("Got a call of Interface$Info()...\n"));
  *local = st->local;
  *name  = strdup(st->rep.name);
  *code  = st->rep.code.val;

  while(*ncode != TCODE_NONE) {
      Intf_st *nst;
      if(!LongCardTbl$Get(ts_st->intfsByTC, *ncode, (addr_t *)&nst)) {
	    /* Something is corrupted! XXX */ 
	    ntsc_dbgstop();
	}

      SEQ_ADDH(needs, (Interface_clp) (pointerval_t) nst->rep.any.val);
      ncode++;
  }

  return needs;
}
