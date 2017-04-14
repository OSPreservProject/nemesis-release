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
**      Functions.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Clanger interpreter: function call synthesiser and wrapper
** 
** ENVIRONMENT: 
**
**      Clanger, user domain
** 
** ID : $Id: Functions.c 1.2 Tue, 16 Mar 1999 17:58:53 +0000 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <stdio.h>

#include <Operation.ih>
#include "Clanger_st.h"

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x) 
#endif 

#ifdef CLANGER_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif


static const char * const mode_names[] = {
    "IN", "OUT", "IN OUT", "RESULT"
};

static const char * const opkind_names[] = {
    "PROC", "ANNOUNCEMENT", "NEVER RETURNS"
};


#define N_PARS	32	/* the maximum total number of args and results */

/* Expr_t's have no "anchor", when considered as LINKs */
#define N_EXPRS(e)	((e) ? LINK_LENGTH((e)) + 1: 0)

typedef Type_Val (* proc_ptr_t)();


static bool_t
ExprIsLValue (Expr_t *e)
{
    return (e->major_type == NODE_IDENT);
    /* XXX - should be more sophisticated than this when there are other
       sorts of lvalue like record/choice fields, array/sequence members etc. */
}


PUBLIC void
Clanger_Function(Clanger_st *st,
		 string_t    ifname,	/* Name of interface to use 	*/
		 string_t    opname,	/* Name of op. to invoke 	*/
		 Expr_t     *arglist,
		 Expr_t     *reslist,
		 string_t    filter,	/* for invocVals 		*/
		 bool_t NOCLOBBER spawn,/* Do we run this in a new domain? */
		 string_t    domname,   /* Name for spawned domain      */
		 Expr_t     *env,       /* Environment for spawned domain */
		 Type_Any   *res)	/* for invocVals 		*/
{
    Type_Any 	ifany;			/* value bound to ifname 	*/
    string_t 	iftypename;		/* Name of type of interface 	*/ 
    Type_Any 	iftypeany;		/* Representation of intf type	*/
    Interface_clp ifscope;		/* Defining scope of interface	*/
    Interface_clp iftype;		/* Type rep of interface	*/
    Context_Names *paramlist;		/* names of op params		*/
    Operation_clp NOCLOBBER optype;	/*      "          "            */
    word_t 	p;			/* formal param iterator	*/
    Par_t 	pars[ N_PARS ];		/* array of actual par records	*/
    word_t	pv;			/* index into parvals 		*/
    Expr_t      *arg;			/* iterates through arglist	*/
    word_t 	parvals[ N_PARS ];	/* array to pass to call synth	*/
    Type_Val    *retvaladdr;		/* address to put return val in	*/
    Type_Val    dummy;			/* if there aren't any results  */
    uint32_t	opnum, nargs, nres, nexc; /* operation info 		*/
    proc_ptr_t	opaddr;			/* address of proc to call	*/
    Context_clp the_cx;
    char       * NOCLOBBER ifnm= ifname;
    Spawn_clp NOCLOBBER spawn_clp;
    GenericClosure_t *clp;

    TRC(printf("Clngr$Fnctn: '%s$%s', %d args, %d results\n",
	       ifname, opname, N_EXPRS (arglist), N_EXPRS (reslist) ));

    if (spawn) {
	if (!st->spawnmod) {
	    fprintf(stderr,"Clanger_Function: warning: spawn not supported\n");
	    spawn=False;
	}
    }

    /* First try to get the value corresponding to the interface */
    if(ifname[0]=='>') {
	ifnm++;
	the_cx= (Context_clp ) st->namespace;
    } else the_cx= st->current;

    if (Context$Get(the_cx, ifnm, &ifany ))
    {
	/* Now get the type information on the interface */
	iftypename = TypeSystem$Name(Pvs(types), ifany.type);
	ifscope = TypeSystem$Info(Pvs(types),
				     ifany.type,
				     &iftypeany);
	TRC(printf("Clngr$fnctn: type of interface is %s.\n",iftypename));
  
	iftype = NARROW( &iftypeany, TypeSystem_Iref );
	TRC(printf("Clngr$fnctn: really is an interface.\n"));
    
	/* Look up the operation name */
	TRY {
	    optype = NAME_LOOKUP(opname, (Context_clp) iftype, Operation_clp);
	} CATCH_Context$NotFound(UNUSED n) {
	    printf("Can't find PROC `%s$%s'.\n", ifnm, opname);
	    RERAISE;
	} CATCH_TypeSystem$Incompatible() {
	    printf("`%s$%s' is not a PROC\n", ifnm, opname);
	    RERAISE;
	} ENDTRY;
    }
    else
    {
	/* see if it's a type in a value constructor */
	string_t ts_name = Heap$Malloc (Pvs(heap), strlen (ifname) + 1 /* '.' */
					+ strlen (opname) + 1 /* NUL */);
	Type_Any tc_any, arg_any, res_any;

	if (!ts_name) RAISE_Heap$NoMemory();

	sprintf (ts_name, "%s.%s", ifname, opname);
	if (!Context$Get ((Context_clp) Pvs(types), ts_name, &tc_any))
	{
	    printf ("%s$%s is neither a PROC nor a TYPE.\n", ifname, opname);
	    FREE (ts_name);
	    RAISE_TypeSystem$Incompatible();
	}
	FREE (ts_name);

	/* concrete type => do the assignment */
	if (N_EXPRS (arglist) != 1)
	{
	    printf ("Wrong number of arguments to constructor `%s$%s'\n",
		    ifname, opname);
	    RAISE_Clanger$Failure();
	}
	Clanger_Eval (st, arglist, &arg_any);

	res_any.type = (Type_Code) tc_any.val;
	if (! Clanger_Assign (st, &res_any, &arg_any))
	{
	    printf ("Value supplied is not assignable to type `%s$%s'.\n",
		    ifname, opname);
	    printf("Desired typecode is %qx, actual typecode is %qx\n", 
		   arg_any.type, res_any.type);
	    RAISE_TypeSystem$Incompatible();
	}

	if (filter)
	{
	    printf ("Value constructor `%s$%s' has no result `%s'\n.",
		    ifname, opname, filter);
	    RAISE_Clanger$Failure();
	}

	if (N_EXPRS (reslist) == 0 && res != NULL)
	{
	    ANY_COPY (res, &res_any);
	}
	else if (N_EXPRS (reslist) == 1)
	{
	    string_t lname = (string_t) (word_t) reslist->val;
	    if (lname)
	    {
		Type_Any tmp;
		if(Context$Get(st->front.prev->c, lname, &tmp)) {
		    Context$Remove (st->front.prev->c, lname);
		}
		Context$Add    (st->front.prev->c, lname, &res_any);
	    }
	}
	else
	{
	    printf ("Too many results for value constructor `%s$%s'\n.",
		    ifname, opname);
	    RAISE_Clanger$Failure();
	}

	return;
    }
  
    /* It's an operation "optype"; get and check the signature */
    {
	Operation_Kind	k;
	string_t		name;
	Interface_clp	intf;

	k = Operation$Info (optype, &name, &intf, &opnum, &nargs, &nres, &nexc);

	TRC(printf("Clngr$fnctn: Operation$Info gives %s, kind %s.\n",
		   name, opkind_names[k] );
	    printf("  %d args, %d results, %d execeptions, and opno %d.\n",
		   nargs, nres, nexc, opnum ));

	/* do the checks */
	if (nargs != N_EXPRS (arglist)) {
	    printf ("Wrong number of arguments for %s:%s$%s.\n",
		    ifname, iftypename, opname);
	    RAISE_Clanger$Failure();
	} 
	if (nres < N_EXPRS (reslist)) {
	    printf ("Too many result lvalues for %s:%s$%s.\n",
		    ifname, iftypename, opname);
	    RAISE_Clanger$Failure();
	}
	if (k == Operation_Kind_NoReturn)
	    printf ("Warning:  %s:%s$%s NEVER RETURNS.\n",
		    ifname, iftypename, opname);
    }

    /* Get the parm list */
    paramlist = Context$List ((Context_clp) optype);
    TRC(printf("Clngr$fnctn: there are %d parameters in all.\n",
	       SEQ_LEN(paramlist)));

    if ( SEQ_LEN(paramlist) >= N_PARS ) {
	printf ("Sorry, I can't cope with more than %d parameters, so I can't\n"
		"call %s:%s$%s.\n", N_PARS, ifname, iftypename, opname);
	RAISE_Clanger$Failure();
    }

    /* Now set up the parameters */
    p = 0;
  
    /* Go through all actual arguments, matching them to formal ones. */
    if ( arglist ) {
	arg = arglist; do {

	    Operation_Parameter *par;
	    Type_Any		   actual;
      
	    /* Get the parameter record from the Type System */
	    {
		Type_Any parany;
	
		if (!Context$Get((Context_clp) optype, SEQ_ELEM(paramlist, p),
				 &parany ))
		{
		    printf("Typesystem changed under my feet!\n"
			   "(I couldn't find parm `%s' of %s:%s$%s)\n",
			   SEQ_ELEM(paramlist, p), ifname, iftypename, opname);
		    RAISE_Context$NotFound(SEQ_ELEM(paramlist, p));
		}
		par = NARROW(&parany, Operation_Parameter);
		pars[p].mode   = par->mode;
		pars[p].tc     = UnAliasCode (par->type);
		pars[p].a.type = par->type;
		pars[p].lvalue = NULL;
	    }
	
	    switch (par->mode)
	    {
	    case Operation_ParamMode_InOut:
		if (ExprIsLValue (arg))
		    pars[p].lvalue = arg; /* need to do the assignment again */
		/* FALL THROUGH */
	    case Operation_ParamMode_In:

		/* Evaluate the actual argument, and typecheck it */
		Clanger_Eval(st, arg, &actual);
	
		if (pars[p].tc == Type_Any__code)
		{
		    pars[p].a = actual;
		}
		else if (! Clanger_Assign (st, &(pars[p].a), &actual))
		{
		    string_t atypename, ftypename;

		    atypename = TypeSystem$Name(Pvs(types), actual.type);
		    ftypename = TypeSystem$Name(Pvs(types), pars[p].a.type);
		    
		    printf (
			"Actual for argument `%s' of %s:%s$%s has type `%s'.\n"
			"The expected type was `%s'.\n",
			SEQ_ELEM(paramlist, p), ifname, iftypename, opname,
			atypename, ftypename);
		    
		    FREE (atypename);
		    FREE (ftypename);
		    RAISE_TypeSystem$Incompatible();
		}
		break;

	    case Operation_ParamMode_Out:
		if (! ExprIsLValue (arg))
		{
		    printf ("Actual for argument `%s' of %s:%s$%s is not an lvalue.\n",
			    SEQ_ELEM(paramlist, p), ifname, iftypename, opname);
		    RAISE_TypeSystem$Incompatible();
		}

		pars[p].lvalue = arg; /* treat as result; ie. do the assignment */

		if (pars[p].tc == Type_Any__code)
		{
		    /* skip - will be handled by marhalling below */
		}
		else if (TypeSystem$IsLarge (Pvs(types), pars[p].tc))
		{
		    Type_Any tmp;
		    if (Context$Get ((Context_clp) st->front.prev->c,
				     (string_t)(word_t)(arg->val), &tmp ))
		    {
			/* better have the right amount of space allocated;
			   NB. large => concrete */
			if (tmp.type != pars[p].a.type)
			{
			    string_t atypename = 
				TypeSystem$Name (Pvs(types), tmp.type);
			    string_t ftypename = 
				TypeSystem$Name (Pvs(types), pars[p].a.type);
	  
			    printf ("Actual for arg `%s' of %s:%s$%s has type `%s'.\n"
				    "The expected type was `%s'.\n",
				    SEQ_ELEM(paramlist, p), ifname, iftypename, opname,
				    atypename, ftypename);
	      
			    FREE (atypename);
			    FREE (ftypename);
			    RAISE_TypeSystem$Incompatible();
			} 
		    } else {
			/* need to allocate space if it's large and we don't
			   already know about it */
			pars[p].a.val = (Type_Val) (pointerval_t)
			    Heap$Malloc(Pvs(heap),
					TypeSystem$Size (Pvs(types), 
							 pars[p].tc));

			if (!pars[p].a.val)
			{
			    printf ("Couldn't allocate space for argument `%s' "
				    "of %s:%s$%s.\n",
				    SEQ_ELEM(paramlist, p), ifname, iftypename, opname);
			    RAISE_Heap$NoMemory();
			}
		    }
		}
		break;

	    default:
		printf ("TypeSystem botch: argument `%s' of %s:%s$%s is a result!\n",
			SEQ_ELEM(paramlist, p), ifname, iftypename, opname);
		RAISE_Clanger$Failure();
	    }      

	    /* Print it out */
	    TRC(printf("  param %d, '%s', mode %s, tc %qx.\n",
		       p, SEQ_ELEM(paramlist, p), mode_names[par->mode], par->type));
	    TRC(printf("  arg value %qx, tc %qx.\n",
		       pars[p].a.val, pars[p].a.type ));

	    /* Iterate */
	    p++;
	    arg = arg->next; 
	} while ( arg != arglist );
    }

    /* And the same for results */
    arg = reslist; /* possibly NULL, or short */

    while (p < SEQ_LEN (paramlist)) {

	Operation_Parameter *par;
    
	/* Get the parameter record */
	{
	    Type_Any parany;
      
	    if (!Context$Get((Context_clp) optype, SEQ_ELEM(paramlist, p),
			     &parany ))
	    {
		printf("Typesystem missing information some parameters!\n"
		       "(I couldn't find result `%s' of %s:%s$%s)\n",
		       SEQ_ELEM(paramlist, p), ifname, iftypename, opname);
		RAISE_Context$NotFound(SEQ_ELEM(paramlist, p));
	    }
	    par = NARROW(&parany, Operation_Parameter);
	    pars[p].mode   = par->mode;
	    pars[p].tc     = UnAliasCode (par->type);
	    pars[p].a.type = par->type;
	}
    
	/* For results, instead of evaluating arguments we must simply */
	/* record the type of result expected, and the Expr_t */
	/* corresponding to the lvalue, if any. When we do the call, the */
	/* result will be written into the val field of the any. */

	pars[p].lvalue = arg;
    
	/* Print it out */
	TRC(printf("  param %d, '%s', mode %s, tc %qx.\n",
		   p, SEQ_ELEM(paramlist, p), mode_names[par->mode], par->type));
    
	/* Iterate */
	p++;

	if (arg)
	{
	    if (arg->next == reslist)
		arg = NULL;  /* any remaining parameters get dummy result lvalues */
	    else
		arg = arg->next; 
	}
    }


    /*
     * What we now have is a list of parameters which must be marshalled
     * into the list of word_t's to pass to the called proc.
     */
    retvaladdr = &dummy;		        /* address to put result into 	*/
    pv = 0;				        /* index into parvals 		*/
    parvals[ pv++ ] = (word_t) ifany.val;	/* Closure pointer. 		*/
  
    for( p=0; p < SEQ_LEN(paramlist); p++ ) {

	if (pv >= N_PARS) {
	    printf ("Sorry, I can't cope with more than %d words' worth of\n"
		    "parameters, so I can't call %s:%s$%s. This is a compiled in\nlimit in mod/clanger/Functions.c\n",
		    N_PARS, ifname, iftypename, opname);
	    RAISE_Clanger$Failure();
	}

	/* Type_Any's are special - we have them in our hands, and
	   we don't want to be dealing with Anys for Anys, but for
	   RESULTs, we still need to pass a (Type_Any **). */
	if (pars[p].tc   == Type_Any__code &&
	    pars[p].mode != Operation_ParamMode_Result) {
	    parvals[pv++] = (word_t) (pointerval_t) &(pars[p].a);
	} else {
	    Type_Code tc = pars[p].tc;
	    switch( pars[p].mode ) {
	
	    case Operation_ParamMode_In:
		/* Pass small IN parameters by value, large by ref; for
		   small, the any contains the value; for large, the any's
		   value is already the pointer we want.  In the small case,
		   we still need to deal with sizeof(value) > sizeof(word_t). */
		if(TypeSystem$IsLarge(Pvs(types), tc)) {
		    
		    parvals[pv++] = (word_t) pars[p].a.val;
		    
		} else if((sizeof(uint64_t) > sizeof(word_t)) && 
			  (TypeSystem$Size(Pvs(types), tc) == 
			   sizeof(uint64_t))) {
		    parvals[pv++] = (word_t) (pars[p].a.val & 0xffffffff);
		    parvals[pv++] = (word_t) (pars[p].a.val >> 32);
		} else {
		    parvals[pv++] = (word_t) (pars[p].a.val);
		}
		break;
	
	    case Operation_ParamMode_Out:
	    case Operation_ParamMode_InOut:
		/* Pass small OUT and IN OUT parameters by ref to any's value;
		   otherwise, the any value is already a ref. */
		if (TypeSystem$IsLarge( Pvs(types), tc ) ) {
		    parvals[pv++] = (word_t) pars[p].a.val;
		} else {
		    parvals[pv++] = (word_t) &(pars[p].a.val);
		}
		break;
	
	    case Operation_ParamMode_Result:
		/* Treat the first RESULT as the return value, else pass by */
		/* reference. */
		if ( retvaladdr == &dummy ) {
		    retvaladdr = &(pars[p].a.val);
		} else {
		    parvals[pv++] = (word_t) &(pars[p].a.val);
		}
		break;
	    }
	}
    }
  
    /*
     * We now need to find out the address to jump at. We're committed at
     * this point, so now we construct a Spawn closure if required.
     */

    if (spawn) {
	Type_Any *a;

	spawn_clp = SpawnMod$Create(st->spawnmod,&ifany,Pvs(heap));
	if (domname) {
	    Spawn$SetName(spawn_clp, domname);
	}
	if (env) {
	    Context_clp e;
	    Type_Any any;
	    
	    if (Context$Get(st->current, (string_t)(word_t)env->val,&any)) {
		e=NARROW(&any, Context_clp);
		Spawn$SetEnv(spawn_clp, e);
	    } else {
		fprintf(stderr,"Clanger_Function: warning: %s is not bound\n",
			env->val);
	    }
	}
	a=Spawn$GetStub(spawn_clp);
	ANY_COPY(&ifany,a);
	FREE(a);
    }

    clp = (GenericClosure_t *) (pointerval_t) (ifany.val);
    parvals[0]=(word_t)clp;
    opaddr = (proc_ptr_t) clp->op[opnum];
    TRC(printf("Clngr$fnctn: cl at %p, op at %p, fn at %p.\n",
		      clp, clp->op, opaddr ));
    
    /*
     * We can now do the call.  We ASSUME that the stack/register usage
     * conventions are such that we can provide more arguments to the
     * call than will be used in the callee.
     */

    *retvaladdr = (*opaddr) (parvals[0],  parvals[1],  parvals[2],  parvals[3],  
			     parvals[4],  parvals[5],  parvals[6],  parvals[7],
			     parvals[8],  parvals[9],  parvals[10], parvals[11],
			     parvals[12], parvals[13], parvals[14], parvals[15],
			     parvals[16], parvals[17], parvals[18], parvals[19], 
			     parvals[20], parvals[21], parvals[22], parvals[23],
			     parvals[24], parvals[25], parvals[26], parvals[27],
			     parvals[28], parvals[29], parvals[30], parvals[31]);

    /* there should be N_PARS arguments in all */
  
    /* If we spawned a new domain, clean up the Spawn module */
    if (spawn) {
	// Spawn$Destroy(spawn_clp);
    }

    /*
     * Finally, collating results. We run through the Par_t list and for
     * each IN OUT, OUT or RESULT parameter with a valid lvalue, we try and
     * do the assignment.
     */
    {
	bool_t done_res = False;
    
	for (p = 0; p < SEQ_LEN (paramlist); p++)
	{
	    if (pars[p].mode != Operation_ParamMode_In)
	    {
#if defined(INTEL) || defined(ARM)
		/* XXX SDE fix up boolean values */
		if (pars[p].tc   == bool_t__code &&
		    pars[p].mode == Operation_ParamMode_Result)
		{
		    pars[p].a.val = pars[p].a.val & 0xffffffff;
		}
#endif
		/* pull (heap allocated) Type_Any RESULTs up into the pars array */
		if (pars[p].tc   == Type_Any__code &&
		    pars[p].mode == Operation_ParamMode_Result)
		{
		    Type_Any tmp, *result;
		    result = (Type_Any *) (pointerval_t)pars[p].a.val;
		    ANY_COPY (&tmp, result);
		    FREE (result);
		    ANY_COPY (&pars[p].a, &tmp);
		}

		TRC (printf ("Clngr$fnctn: assign result `%s' (val %qx) to ",
			     SEQ_ELEM(paramlist, p), pars[p].a.val));

		if (pars[p].lvalue && pars[p].lvalue->val)
		{
		    string_t lname = (string_t) (word_t) pars[p].lvalue->val;
		    Type_Any tmp;
		    TRC (printf ("lval `%s'.\n", lname));
		    
		    if(Context$Get(st->front.prev->c, lname, &tmp)) {
			Context$Remove (st->front.prev->c, lname);
		    }
		    Context$Add    (st->front.prev->c, lname, &pars[p].a);
		}
		else if (res && !done_res)
		{
		    if (!filter || strcmp (filter, SEQ_ELEM(paramlist, p)) == 0)
		    {
			TRC (printf ("(result).\n"));
			ANY_COPY (res, &pars[p].a);
			done_res = True;
		    }
		    else
		    {
			TRC (printf ("(filtered out).\n"));
		    }
		}
		else
		{
		    TRC (printf ("(void).\n"));
		}
	    }
	}
	SEQ_FREE_ELEMS (paramlist);
	SEQ_FREE (paramlist);
    }
    TRC(printf("End of invocation\n"));
}


