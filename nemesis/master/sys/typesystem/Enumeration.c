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
**      Enumeration interface
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements the Enumeration, Record, Choice and Exception type classes
**	context for the TypeSystem. 
** 
** ENVIRONMENT: 
**
**      As TypeSystem.c
** 
** ID : $Id: Enumeration.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <string.h>

#include <TypeSystem.ih>
#include "TypeSystem_st.h"

/*
 * Method suites : most methods are shared since Records, Enumerations,
 * Choices and Exceptions are all are simple extensions of Context.
 */

static Context_List_fn 	 	E_List;
static Context_Get_fn 	  	E_Get;
static Context_Add_fn 		E_Add;
static Context_Remove_fn 	E_Remove;
static Context_Dup_fn 		E_Dup;
static Context_Destroy_fn 	E_Destroy;
static Choice_Base_fn		C_Base;
static Exception_Info_fn        Exn_Info;

Enum_op enum_ops = { 
    E_List, E_Get, E_Add, E_Remove, E_Dup, E_Destroy 
};

Record_op record_ops = { 
    E_List, E_Get, E_Add, E_Remove, E_Dup, E_Destroy 
};

Choice_op choice_ops = { 
    E_List, E_Get, E_Add, E_Remove, E_Dup, E_Destroy, C_Base 
};

Exception_op exception_ops = { 
    E_List, E_Get, E_Add, E_Remove, E_Dup, E_Destroy, Exn_Info
};

/*********************************************************
 *
 *	 Methods: only List and Get are really used.
 *
 *********************************************************/

static Context_Names *E_List( Context_cl *self )
{
  EnumRecState_t *st = (EnumRecState_t *)(self->st);
  Context_Names  *seq;
  NOCLOBBER int i;

  TRC(pr("Enum$List: called\n"));
    
  /* Get the result sequence */
  seq = SEQ_CLEAR (SEQ_NEW (Context_Names, st->num, Pvs(heap)));
  
  /* Run through all the types defined in the current interface */
  TRY { 
    for( i=0; i < st->num; i++ ) {
      AddName( (st->elems)[i].name, Pvs(heap), seq );
    }
  } CATCH_ALL {
    DB(pr("Enum$List: failed in Malloc: undoing.\n"));
    SEQ_FREE_ELEMS (seq);
    SEQ_FREE (seq);
    DB(pr("Enum$List: done undoing.\n"));
    RERAISE;
  }
  ENDTRY;

  TRC(pr("Enum$List: done.\n"));
  return seq;
}  


/*
 * Get method
 */
static bool_t E_Get( Context_cl *self, string_t name, Type_Any *o )
{
  EnumRecState_t *st = (EnumRecState_t *)(self->st);
  int i;

  /* Straight linear string search for now. */
  for( i = 0; i < st->num; i++ ) 
    {
#if 0 /* XXX PRB */
	/* lazily complete initialisation of Field_t val field */
	if ((st->elems)[i].val.type == TCODE_NONE) {
	    (st->elems)[i].val.val = (st->elems)[i].u.any.val;
	    (st->elems)[i].val.type = (st->elems)[i].u.any.type;
	}
#endif 0
      if (!(strcmp (name, (st->elems)[i].name ) ) )
	{
	  ANY_COPY(o,(Type_Any *)&((st->elems)[i].val));
	  return True;
	}
    }
  return False;
}
  
/*
 * Add, Remove, Dup and Destroy methods are all nulls.
 */
static void E_Add(Context_cl *self, string_t name, const Type_Any *obj )
{ RAISE_Context$Denied(); return; }
static void E_Remove(Context_cl *self, string_t name )
{ RAISE_Context$Denied(); return; }
static Context_clp E_Dup(Context_cl *self, Heap_clp h, WordTbl_clp xl)
{ RAISE_Context$Denied(); return NULL; }
static void E_Destroy( Context_cl *self )
{ return; }


/*
 * Return base type of a choice; i.e the type of the discriminator
 */
static Type_Code C_Base( Choice_cl *self ) 
{
  ChoiceState_t *st = (ChoiceState_t *)(self->st);
  
  return st->disc;
}

static string_t Exn_Info(Exception_clp self,
			 Interface_clp *i,
			 uint32_t      *n) {
    
    Exc_t *st = self->st;

    *i = (Interface_clp) st->intf->rep.any.val;
    *n = st->params.num;

    return strdup(st->name);
}

