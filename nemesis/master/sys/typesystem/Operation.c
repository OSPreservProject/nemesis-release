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
**      Operation.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Implements Operation.if; ie. the way the operations within an
**  interface type are presented to the running Nemesis system.
** 
** ENVIRONMENT: 
**
**      Static, reentrant, anywhere
** 
** ID: $Id: Operation.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <string.h>
#include <ntsc.h>

#include "TypeSystem_st.h"


/*
 * Statics
 */
static Context_List_fn List_m;
static Context_Get_fn Get_m;
static Context_Add_fn Add_m;		/* just raises Context.Denied */
static Context_Remove_fn Remove_m;	/* just raises Context.Denied */
static Context_Dup_fn Dup_m;	/* just raises Context.Denied */
static Context_Destroy_fn Destroy_m;	/* just returns */
static Operation_Info_fn Info_m;
static Operation_Exceptions_fn Except_m;

/*
 * Module stuff
 */
Operation_op operation_ops = {
	List_m,
	Get_m,
	Add_m,
	Remove_m,
	Dup_m,
	Destroy_m,
	Info_m,
	Except_m
};

/*********************************************************
 *
 *	Operation Interface Operations
 *
 *********************************************************/

/*
   Operation$List(): returns a list of strings which are the formal
   arguments (In/Out/InOut/Result) to the arguments. 
   */
static 
Context_Names *List_m(Context_cl *self)
{
  Param_t *params = ((Operation_t *)(self->st))->params;
  Context_Names  *seq;
  NOCLOBBER int i;
  
  /* Get the result sequence */
  seq = SEQ_CLEAR (SEQ_NEW (Context_Names, 0, Pvs(heap)));
   
  /* Run along through the params and add each to the list */
  TRY {
    for(i=0; strlen(params[i].name); i++ ) {
      AddName( params[i].name, Pvs(heap), seq);
    }
  } CATCH_ALL {
    DB(pr("Operation$List: failed in Malloc: undoing.\n"));
    SEQ_FREE_ELEMS (seq);
    SEQ_FREE (seq);
    DB(pr("Operation$List: done undoing.\n"));
    RERAISE;
  } ENDTRY;
  
  return seq;
}

/* 
An Operation context maps the names of formal parameters (i.e. strings) 
onto Param_t records; each Param_t record contains information about 
the type of the parameter (as a Type_Code) and about the mode of
the parameter (as an Operation_ParamMode- i.e. In, Out, InOut or Result).
*/
static 
bool_t Get_m(Context_cl *self, string_t name, /* RETURNS */ Type_Any *o)
{
  Param_t *params = ((Operation_t *)(self->st))->params;
  bool_t found= False;
  int i;
  
  for(i=0; !found && strlen(params[i].name); i++ ) {
    if(!(strcmp(name, params[i].name))) {
      found=True;
      o->type= Operation_Parameter__code;
      o->val= (word_t)&(params[i].rep);
    }
  }
  
  return(found);
}

/*
 * Add, Remove and Destroy methods are all nulls.
 */
static void Add_m(Context_cl *self, string_t name, const Type_Any *obj )
{ RAISE_Context$Denied(); return; }
static void Remove_m(Context_cl *self, string_t name )
{ RAISE_Context$Denied(); return; }
static Context_clp Dup_m(Context_cl *self, Heap_clp h, WordTbl_clp xl)
{ RAISE_Context$Denied(); return NULL; }
static void Destroy_m( Context_cl *self )
{ return; }



static Operation_Kind Info_m(Operation_cl *self, 
			     /* RETURNS */ 
			     string_t *name, 
			     Interface_clp *i, 
			     uint32_t *n,
			     uint32_t *a, 
			     uint32_t *r,
			     uint32_t *e)
{
  Operation_t *st= (Operation_t *)self->st;
  
  *name= st->name;
  *n= st->proc_num;
  *a= st->num_args;
  *r= st->num_res;

  /* 
     for the present, there is no way for me to return either the 
     interface in which this operation lives, or the number of exceptions
     which this operation can raise. Need to change MIDDL generations
     and some other stuff for this; will do later. 
     */
  *i= 0;	
  *e= st->num_excepts;
  
  return(st->kind);	
}

static Exception_List *Except_m(Operation_cl *self, Heap_clp h)
{
    Operation_t *st= (Operation_t *)self->st;
    TypeSystem_st *ts_st = Pvs(types)->st; /* XXX */
    int i;

    Exception_List *l = SEQ_NEW(Exception_List, 0, h);
    
    for(i = 0; i < st->num_excepts; i++) {
	ExcRef_t *er = &st->exns[i];
	Intf_st  *intf_st;

	if(!LongCardTbl$Get(ts_st->intfsByTC, er->intf, (addr_t *)&intf_st)) {
	    /* Something is corrupted! */ 
	    ntsc_dbgstop();
	}
	
	SEQ_ADDH(l, (Exception_clp)&intf_st->exns[er->num]->cl);
    }
			    
    return l;
}

