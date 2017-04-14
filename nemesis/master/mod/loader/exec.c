/*
 *****************************************************************************
 **                                                                          *
 **  Copyright 1995, University of Cambridge Computer Laboratory             *
 **                                                                          *
 **  All Rights Reserved.                                                    *
 **                                                                          *
 *****************************************************************************
 **
 ** FILE:
 **
 **	mod/loader/Exec.c
 **
 ** FUNCTIONAL DESCRIPTION:
 **
 **	Implements Exec.if
 **
 ** ENVIRONMENT: 
 **
 **	User.
 **
 ** ID : $Id: exec.c 1.4 Wed, 26 May 1999 15:02:49 +0100 dr10009 $
 **
 **
 */

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <ecs.h>

#include <IDCMacros.h>
#include <VPMacros.h>

#include <Exec.ih>
#include <Closure.ih>
#include <Builder.ih>
#include <TypeSystem.ih>
#include <TypeSystemF.ih>

#include <salloc.h>

#ifdef EXEC_TRACE
#define TRC(_x) _x
#else 
#define TRC(_x)
#endif


#ifdef __ALPHA__
#define STARTSYMBOL "__start"
#endif

#ifdef __IX86__
#define STARTSYMBOL "_start"
#endif

#ifdef __ARM__
#define STARTSYMBOL "_start"
#endif

/*
 * Module stuff
 */
static  Exec_Run_fn             Exec_Run_m;
static  Exec_Load_fn            Exec_Load_m;
static  Exec_LoadProgram_fn     Exec_LoadProgram_m;
static  Exec_FillEnv_fn         Exec_FillEnv_m;

static  Exec_op ms = {
  Exec_Run_m,
  Exec_Load_m,
  Exec_LoadProgram_m,
  Exec_FillEnv_m
};

const static    Exec_cl cl = { &ms, NULL };

CL_EXPORT (Exec, Exec, cl);

#define GET_OR_DEFAULT(_cx, _name, _var, _type)	\
do {						\
    Type_Any _any;				\
    if (Context$Get((_cx), _name, &_any))	\
        _var= NARROW(&_any, _type);		\
} while (0)


/* XXX nasty hack because clanger thinks p,s,l are int64_t, and
 * everyone else think Time_ns */
#define GET_NUMERIC(_cx, _name, _var, _mytype)					\
do {									\
    Type_Any _any;							\
    if (Context$Get((_cx), _name, &_any))				\
    {									\
	if (ISTYPE(&_any, _mytype))					\
	    _var = NARROW(&_any, _mytype);		\
	else if (ISTYPE(&_any, int64_t))				\
	    _var = NARROW(&_any, int64_t);		\
	else								\
	    RAISE_TypeSystem$Incompatible();				\
    }									\
} while (0)


/* 
 * Prototypes
 */

static Exec_Exports *Exec_Load_m(Exec_cl*, const Type_Any *, 
				 ProtectionDomain_ID);
static bool_t HasSuffix (string_t, string_t, string_t *);


static const char * const names[] = {
    "name",
    "qos>aHeapWords",
    "qos>cpu>p",
    "qos>cpu>s",
    "qos>cpu>l",
    "qos>cpu>k",
    "qos>cpu>x",
    "qos>cpu>nctxts",
    "qos>cpu>nframes",
    "qos>cpu>neps",
    "pdom"
};

enum domain_parameters {
    P_NAME,
    P_AHEAPWORDS,
    P_CPU_P,
    P_CPU_S,
    P_CPU_L,
    P_CPU_K,
    P_CPU_X,
    P_CPU_NCTXTS,
    P_CPU_NFRAMES,
    P_CPU_NEPS,
    P_PDOM,
    NUMBER_OF_PARAMS
};


/* initialise any with the default value for parameter p, or return a non zero
   result on failure */
static int default_parameter(enum domain_parameters param, Type_Any *any) {
#define DEFAULT_VALUE(param_number, value, type) ({ \
    if (param == param_number ) { \
      ANY_INIT(any, type, value);\
      return 0;\
    } \
    })
 
    DEFAULT_VALUE(P_NAME, "anon", string_t);
    DEFAULT_VALUE(P_AHEAPWORDS, 8<<10, uint32_t);
    DEFAULT_VALUE(P_CPU_P, MILLISECS(10), Time_ns);
    DEFAULT_VALUE(P_CPU_S, MILLISECS(0), Time_ns);
    DEFAULT_VALUE(P_CPU_L, MILLISECS(10), Time_ns);
    DEFAULT_VALUE(P_CPU_K, True, bool_t);
    DEFAULT_VALUE(P_CPU_X, True, bool_t);
    DEFAULT_VALUE(P_CPU_NCTXTS, 10, uint32_t);
    DEFAULT_VALUE(P_CPU_NFRAMES, 64, uint32_t);
    DEFAULT_VALUE(P_CPU_NEPS, 64, uint32_t);
    DEFAULT_VALUE(P_PDOM, NULL_PDID, ProtectionDomain_ID);
    return 1;
}


/* 
 * Entry point
 */

static Domain_ID 
Exec_Run_m (Exec_cl *self, const Type_Any *module, 
	    Context_clp env, ProtectionDomain_ID pdom,NOCLOBBER string_t domname)
{
    /* the variables we have to discover, and their default values */
    NOCLOBBER Context_clp benv=NULL;
    NOCLOBBER word_t   aHeapWords;
    NOCLOBBER uint32_t nctxts;
    NOCLOBBER uint32_t neps;
    NOCLOBBER uint32_t nframes;
    NOCLOBBER Time_ns p, s, l;
    NOCLOBBER bool_t x, k;
    NOCLOBBER Closure_clp cl = NULL;
    Type_Any any;

    /* this is used to lookup names; if we don't find one, we 
       invoke FillEnv so that we can default the values. So the context
       must be read-write or complete.
       */
    /* this has to be a macro because it is polymorphic, alas */
#define LOOKUP(param, context, _type, isnumeric) \
    ({ \
      Type_Any any; \
      _type result; \
      bool_t gotparam; \
      TRC(printf("ExecRun reading parameter %d: %s\n", param, names[param])); \
      gotparam = False; \
      if (context) \
	  if (Context$Get(context, (char*)names[param], &any)) gotparam = True;\
      if (!gotparam) { \
	  if (!default_parameter(param, &any)) { \
	      gotparam = True; \
	  } \
      } \
      if (!gotparam) { \
	  TRC(printf("Name %s not found and no default available\n",  \
		     names[param]));  \
	  RAISE_Context$NotFound(names[param]); \
      } \
      if (ISTYPE(&any, _type)) { \
	  result = NARROW(&any, _type); \
      } else if (isnumeric && ISTYPE(&any, int64_t)) { \
	  result = (_type) NARROW(&any, int64_t); \
      } else { \
	  TRC(printf("Name %s has bad type code %qx\n", \
		     names[param], any.type)); \
          TRC(printf("Name %s in exec environment is type  %s and should be %s\n", \
		     names[param], TypeSystem$Name(Pvs(types), any.type), \
		     TypeSystem$Name(Pvs(types), _type##__code))); \
	  RAISE_TypeSystem$Incompatible(); \
	  result = NARROW(&any, _type); /* never invoked; placate GCC */ \
      } \
      result; \
    })

    if (!domname) domname = LOOKUP(P_NAME, env, string_t, 0);
          
    TRC(printf("Exec$Run: env=%p pdom=%x name=%s\n",
	       env, pdom, domname?domname:"(NULL)"));

    neps = LOOKUP(P_CPU_NEPS, env, uint32_t, 1);
    nframes = LOOKUP(P_CPU_NFRAMES, env, uint32_t, 1);
    nctxts =  LOOKUP(P_CPU_NCTXTS, env, uint32_t, 1);

    p = LOOKUP(P_CPU_P, env, Time_ns, 1);
    s = LOOKUP(P_CPU_S, env, Time_ns, 1);
    l = LOOKUP(P_CPU_L, env, Time_ns, 1);
    x = LOOKUP(P_CPU_X, env, bool_t, 0);
    k = LOOKUP(P_CPU_K, env, bool_t, 0);

    benv = NULL;
    if (env) 
	if (Context$Get(env, "b_env", &any)) 
	    benv      = NAME_LOOKUP("b_env", env, Context_clp);

    aHeapWords    = LOOKUP(P_AHEAPWORDS, env, uint32_t, 1);
    pdom          = LOOKUP(P_PDOM, env, ProtectionDomain_ID, 0);

    TRC(printf("Exec$Run: p=%qx s=%qx l=%qx x=%d k=%d "
	       "Aheapwords=%x nctxts=%d neps=%d nframes=%d benv=%p"
	       " name=%s pdid=%qx\n",
		p, s, l, x, k, aHeapWords, nctxts, neps, nframes, benv,
                domname, (uint64_t) pdom));

    /* finally, we can create the domain */
    TRC(printf("Building domain\n"));
    {   
	Builder_clp builder  = NAME_FIND("modules>Builder", Builder_clp);
	DomainMgr_clp dm     = IDC_OPEN("sys>DomainMgrOffer", DomainMgr_clp);
	Activation_clp vec;
	Domain_ID dom;
	VP_clp vp;
	IDCOffer_clp salloc_offer;
	StretchAllocator_clp salloc;

	/* Call DomainMgr$NewDomain to create the skeleton of a new domain,
	 * but we don't yet know the activation closure, so leave it NULL */
	TRC(printf("Exec$Run: about to call DomainMgr$NewDomain.\n"));
	vp = DomainMgr$NewDomain(dm, NULL, &pdom, nctxts, neps, nframes, 
				 aHeapWords*sizeof(word_t), k, domname,
				 &dom, &salloc_offer);
	
	/* If the pdom was NULL, it has now been set to something
         * plausable, so we can now invoke the loader */
	
	cl = NULL;
	TRY {
	    cl = Exec_LoadProgram_m(self, module, pdom);
	} CATCH_ALL {
	    fprintf(stderr,"No entry point found for program %s\n", domname);
	} ENDTRY;
	if (!cl)
	{
	    /* XXX hope we can destroy a semi-built domain */
	    DomainMgr$Destroy(dm, dom);
	    RAISE_Load$Failure(Load_Bad_SymTab);
	}

	/* Looked successful, so build the rest of the domain */

	/* Bind to the child's salloc */

	salloc = IDC_BIND(salloc_offer, StretchAllocator_clp);

	TRC(printf("Exec$Run: about to call Builder_NewThreaded\n"));
	vec = Builder$NewThreaded (builder, cl, salloc, pdom, 
				   RW(vp)->actstr, benv);

	/* we didn't know this before, but we do now */
	VP$SetActivationVector(vp, vec);

	/* Release the StretchAllocator */
	IDC_CLOSE(salloc_offer);

	TRC(printf("Exec$Run: about to call DomainMgr$AddContracted\n"));
	TRC(PAUSE(SECONDS(1)));
	TRY {
	    DomainMgr$AddContracted(dm, dom, p, s, l, x);
	} CATCH_DomainMgr$AdmissionDenied() {
	    printf("Exec$Run: new domain would over-commit.\n");
	} ENDTRY;
	TRC(printf("Done\n"));

	return dom;
    }
}


static Exec_Exports *Exec_Load_m (Exec_cl *self, const Type_Any *module,
				  ProtectionDomain_ID pdom)
{
    Type_Any	a;
    Exec_Exports *res = SEQ_NEW (Exec_Exports, 0, Pvs(heap));

    ANY_COPY (&a, module);

    if (ISTYPE (&a, Closure_clp)) {
	SEQ_ADDH (res, a);
	return res;
    }

    /* bind IDCOffers */
    if (ISTYPE (&a, IDCOffer_clp))
	ObjectTbl$Import (Pvs(objs), NARROW(&a, IDCOffer_clp), &a);

    /* open Files */
#if 0
    if (ISTYPE (&a, File_clp)) {
	Rd_clp rd;
	rd = File$OpenRd ((File_clp) a.val);

	ANY_INIT(&a, Rd_clp, rd);

    }
#endif
    /* load the contents of readers */
    if (ISTYPE (&a, Rd_clp))
    {
	ContextMod_clp cmod  = NAME_FIND ("modules>ContextMod", ContextMod_clp);
	Load_clp       ldr   = NAME_FIND ("modules>Load", Load_clp);
	Context_clp    syms  = NAME_FIND ("symbols", Context_clp);
	Context_clp    stubs = NAME_FIND ("stubs", Context_clp);
	Context_clp    exps  = 
	    ContextMod$NewContext (cmod, Pvs(heap), Pvs(types));
        TypeSystemF_clp tsf  = IDC_OPEN("sys>TypeSystemF", TypeSystemF_clp);
	NOCLOBBER Stretch_clp    text  = NULL;
	Stretch_clp    data  = NULL;
	Context_Names *NOCLOBBER names = NULL;
	ProtectionDomain_ID our_pdom = VP$ProtDomID(Pvs(vp));

	TRY
	{
	    NOCLOBBER int i;

	    text = Load$FromRd (ldr, NARROW(&a, Rd_clp), syms, exps, pdom, &data);

	    TRC(fprintf(stderr,"Text segment starts at %x\n", text));

	    if (pdom != our_pdom) {
		/* XXX SMH: get read access to image so can look at symbols */
		STR_SETPROT(text, our_pdom, SET_ELEM(Stretch_Right_Read));
		if (data) {
		    STR_SETPROT(data, our_pdom, SET_ELEM(Stretch_Right_Read));
		}
	    }

	    /* 
	    ** Now we copy the exports into syms, searching for the entry 
	    ** closure as we go.  
	    */
	    names = Context$List (exps);
	    TRC(fprintf(stderr,"Got context list\n"));
	    for (i = 0; i < SEQ_LEN (names); i++)
	    {
		string_t name = SEQ_ELEM (names, i);
		string_t real_name;
		Type_Any sym;

		if (! Context$Get (exps, name, &sym))
		    RAISE_Context$NotFound(name);
		TRY {
		    Context$Add (syms, name, &sym);
		} CATCH_ALL {
#ifdef DEBUG
		    fprintf(stderr, 
			    "Haha, name %s already bound, who cares.\n", name);
#endif
		} ENDTRY;

		/* if we have a __start symbol, then add this as a closure */
		if(!strcmp(STARTSYMBOL, name)) {
		    Load_Symbol  ls     = NARROW (&sym, Load_Symbol);
		    Type_Any *export    = Heap$Malloc(Pvs(heap), sizeof(Type_Any));

		    /* Want to add it as a 'closure' */
		    ANY_INIT(export, Closure_clp, ls->value);
		    SEQ_ADDH(res, *export);
		}

		/* add all CL_EXPORTed symbols to the "modules" context */
		if (HasSuffix ("_export", name, &real_name))
		{
		    TRC(fprintf(stderr,"Found export\n"));
		    {
			char fullname[256];
			Load_Symbol  ls     = NARROW (&sym, Load_Symbol);
			Type_Any   *xsym;
			TRC(fprintf(stderr,"Dereferencing %x\n", ls));
			xsym = (Type_Any *) (ls->value);
			TRC(fprintf(stderr,
				    "Pulling out symbol [%s] export [%x]\n", 
				    real_name, xsym));
			TRC(fprintf(stderr,"value [%qx] typecode [%qx]\n", 
				    xsym->val, xsym->type));
			strcpy(fullname, "modules>");
			strcat(fullname, real_name);
			TRC(fprintf(stderr,"Doing Context$Add(%s,%x)\n",
				    fullname, xsym));
			TRY {
			    Context$Add (Pvs(root), fullname, xsym);
			} CATCH_ALL { 
#ifdef DEBUG
			    fprintf(stderr, 
				    "Haha, name %s already bound, who cares.\n", name);
#endif
			} ENDTRY;

			TRC(fprintf(stderr,"Doing  Sequence add\n"));
			SEQ_ADDH    (res, *xsym);
			TRC(fprintf(stderr,"Freeing name\n"));
			FREE (real_name);
		    }
		    TRC(fprintf(stderr,"Dealt with export\n"));
		}

		/* register all __intf symbols with the type system */
		if (HasSuffix ("__intf", name, NULL))
		{
		  Load_Symbol  ls   = NARROW (&sym, Load_Symbol);
		  addr_t       intf = (addr_t) ls->value;
		  TRC(fprintf(stderr,"Interface=%s, addr=%qx.\n",name, intf));
		  TypeSystemF$RegisterIntf(tsf, intf);
		}

		/* add all __stubs symbols to the "stubs" context */
		if (HasSuffix ("__stubs", name, &real_name))
		{
		    Load_Symbol  ls  = NARROW (&sym, Load_Symbol);
		    Type_Any    *s   = 
			(Type_Any *) ls->value; /* (typed_ptr_t *) */
		    
		    TRY { 
			Context$Add (stubs, real_name, s);
		    } CATCH_ALL {
#ifdef DEBUG
			    fprintf(stderr, 
				    "Haha, name %s already bound, who cares.\n", name);
#endif
		    } ENDTRY;
			
		    FREE (real_name);
		}
	    }
	}
	FINALLY
	{
	    if (names)
	    {
		SEQ_FREE_ELEMS (names);
		SEQ_FREE (names);
	    }
	    Context$Destroy (exps);
	}
	ENDTRY;

#if 0
	/* XXX SDE: work out why this is failing, when adding the privileges
	   earlier on works. */
	if (pdom != our_pdom) {
	    /* SMH: remove the temp read access we gave ourselves */
	    /* XXX: stretches should `belong` to new dom I reckon */
	    STR_REMPROT(text, our_pdom);
	    if (data) {
		STR_REMPROT(data, our_pdom);
	    }
	}
#endif /* 0 */

    }
    else
	RAISE_TypeSystem$Incompatible();
    TRC(fprintf(stderr,"Finished Load\n"));
    return res;  
}

/* 
 * Return whether a string has the given suffix, and the corresponding prefix.
 */

static bool_t
HasSuffix (string_t suffix, string_t full_name, string_t *real_name)
{
    int i = strlen (full_name) - strlen (suffix);

    if (i <= 0)
	return False;
    else if (strcmp (full_name + i, suffix) == 0) {
	if (real_name) {
	    *real_name = Heap$Malloc (Pvs(heap), i + 1);
	    if (!*real_name) RAISE_Heap$NoMemory();
	    memcpy (*real_name, full_name, i);
	    (*real_name)[i] = '\0';
	}
	return True;
    } else
	return False;
}

static Closure_clp Exec_LoadProgram_m (
        Exec_cl *self,
        const Type_Any  *module /* IN */,
        ProtectionDomain_ID  pdom    /* IN */ )
{
    Exec_Exports *exps;
    Type_Any *ap; 		 
    Closure_clp cl;

    exps = Exec_Load_m(self, module, pdom);

    TRC(printf("Exec$LoadProgram; exports is %qx\n", exps));
    cl = NULL;

    for(ap = SEQ_START(exps); ap < SEQ_END (exps); ap++)
	if (ISTYPE(ap, Closure_clp)) {
	    TRC(printf("Exec$Run: init closure\n"));    
	    cl = (Closure_clp) (word_t) ap->val;
	}
    SEQ_FREE(exps);
    
    if (!cl) {
	eprintf("Error! no entry point found\n");
	RAISE_Exec$NoEntryPoint();
    }
    return cl;
}



static void Exec_FillEnv_m (
        Exec_cl *self,
        Context_clp     env     /* IN */ )
{ 
    int i;
    Type_Any any;

    if (!Context$Get(env, "qos", &any)) CX_NEW_IN_CX(env, "qos");
    if (!Context$Get(env, "qos>cpu", &any)) CX_NEW_IN_CX(env, "qos>cpu");


    for (i=0; i<NUMBER_OF_PARAMS; i++) {
	if (!Context$Get(env, (char*) names[i], &any)) {
	    if (default_parameter(i, &any)) {
		printf("Warning! No initialisation code in Exec for type %s\n", names[i]);
	    } else {
		Context$Add(env, (char*) names[i], &any);
	    }
	}
    }
}


/*
** End mod/loader/Exec.c
*/

