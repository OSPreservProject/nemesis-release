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
**      Interp.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Clanger interpreter: parse tree walker
** 
** ENVIRONMENT: 
**
**      Clanger, user domain
** 
** ID : $Id: Interp.c 1.2 Tue, 04 May 1999 18:45:21 +0100 dr10009 $
** 
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <time.h>
#include <ecs.h>
#include <stdio.h>
#include <string.h>

#include <IDCMacros.h>

#include <DomainMgr.ih>
#include <Exec.ih>
#include <Load.ih>

#include <Type.ih>
#include <Context.ih>
#include <ContextUtil.ih>

#include <FSUtil.ih>
#include <FileIO.ih>

#include "Clanger_st.h"
#include "Clammar.tab.h"

#include <ntsc.h>

#ifdef DEBUG
#define DB(_x) _x
#else
#define DB(_x) 
#endif 

#ifdef DEBUG
#define CLANGER_TRACE 1
#endif

#ifdef CLANGER_TRACE
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#undef DUMPEXPR

/*
 * Statics
 */
static void SpawnInvoc(Clanger_st *st, string_t name, Expr_t *env,
		       Expr_t *invoc, /* OUT */ Type_Any *a );
static void EvalBoolean(Clanger_st *st, Expr_t *e,  /* OUT */ Type_Any *a );
static bool_t EvalToBoolean(Clanger_st *st, Expr_t *e );
static void EvalUnary(Clanger_st *st, Expr_t *e, /* OUT */ Type_Any *a );
static void EvalBinary(Clanger_st *st, Expr_t *e, /* OUT */ Type_Any *a );
static void EvalConstructor(Clanger_st *st, Expr_t *e, /* OUT */ Type_Any *a );
static void EvalListConstructor(Clanger_st *st, Expr_t *e, /* OUT */ Type_Any *a );
static void Bind(Clanger_st *st, Expr_t *lvalue, Expr_t *rvalue,
		 Context_clp cx, bool_t grow);
static void PromoteInt(Type_Any *a );
static void NormaliseInt(Type_Any *a );
static void EnsureInt(Type_Any *a );
#ifdef DUMPEXPR
static void DumpExpr(Expr_t *e );
#endif
static void List (Clanger_st *st, Expr_t *path);
static void ChangeCurContext (Clanger_st *st, Expr_t *path);
static void PrintCurContext (Clanger_st *st);
static void Exec (Clanger_st *st, Expr_t *file, Expr_t *parm);
static void Fork(Clanger_st *st, Expr_t *cls, Expr_t *stk);
static void Remove(Clanger_st *st, Expr_t *l);
static void PagedOutput(Clanger_st *st, Expr_t *l);
static void DestroyDomain (Clanger_st *st, Expr_t *l);
static void Pause(Clanger_st *st, Expr_t *l);
static void Include(Clanger_st *st, Expr_t *l);



static bool_t
RangeCheckSigned (Type_Any *val, int64_t min, int64_t max, int64_t *sval);

static bool_t
RangeCheckUnsigned (Type_Any *val, uint64_t max, uint64_t *uval);


/* 
 * Clanger_Statement executes a list of clanger statements.
 * It returns True if the list was exited by a break statement, 
 * False otherwise.
 */
void Clanger_Statement( Clanger_st *st, Expr_t *stmtlst )
{
    Expr_t *NOCLOBBER s = stmtlst;

    /* Kluge for null statements */
    if ( !stmtlst ) return;

    /* Iterate over all statements in this list. */
    do {

	switch( s->major_type ) {

	case NODE_NULLST:
	    TRC(printf("Clanger$Statement: null.\n"));
	    break;

	case NODE_LOOP:
	    TRY {
		Clanger_Statement(st, s->ch1 );
		while( s->ch2 == 0 || EvalToBoolean(st, s->ch2 )) {
		    Clanger_Statement(st, s->ch3 );
		    Clanger_Statement(st, s->ch4 );
		}
	    } CATCH_Clanger$Break() {
		TRC(printf("Clanger$Statement: break caught.\n"));
	    } ENDTRY;
	    break;

	case NODE_IF:
	    if ( EvalToBoolean(st, s->ch1 ) ) {
		Clanger_Statement(st, s->ch2 ); 
	    } else {
		Clanger_Statement(st, s->ch3 ); 
	    }
	    break;
      
	case NODE_FINALLY:
	    TRY {
		Clanger_Statement(st, s->ch1 );
	    } FINALLY {
		Clanger_Statement(st, s->ch2 );
	    } ENDTRY;
	    break;

	case NODE_BREAK:
	    RAISE_Clanger$Break();
	    break;

	case NODE_BIND:
	    Bind (st, s->ch1, s->ch2, NULL, True);
	    break;
	    
	case NODE_LIST: 
	    List (st, s->ch1);
	    break;

	case NODE_PAUSE:
	    Pause (st, s->ch1);
	    break;

	case NODE_CD:
	    ChangeCurContext (st, s->ch1);
	    break;

	case NODE_PWD:
	    PrintCurContext(st);
	    break;

	case NODE_EXEC:
	    Exec (st, s->ch1, s->ch2);
	    break;

	case NODE_FORK:
	    Fork (st, s->ch1, s->ch2);
	    break;

	case NODE_HALT:
	    printf("Clanger: halting...\n");
	    PAUSE(SECONDS(1));
	    ntsc_dbgstop();
	    break;

	case NODE_RM:
	    Remove (st, s->ch1);
	    break;

	case NODE_MORE: 
	    PagedOutput(st, s->ch1);
	    break;

	case NODE_INCLUDE:
	    Include(st,s->ch1);
	    break;

	case NODE_KILL:
	    DestroyDomain (st, s->ch1);
	    break;

	case NODE_INVOCST:
	    Clanger_Function(st, 
			     (string_t) (word_t) s->ch2->val,
			     (string_t) (word_t) s->ch3->val,
			     s->ch4,
			     s->ch1,
			     NULL, False, NULL, NULL, NULL);
	    break;

	case NODE_SPAWN:
	    SpawnInvoc(st,
		       (string_t)(word_t)s->ch1->val,
		       s->ch2, s->ch3, NULL);
	    break;

	case NODE_RAISE:
	    printf("Clanger$Statement: node %s is not yet implemented.\n", 
		    node_names[s->major_type]);
	    break;

	default:
	    printf("Clanger$Statement: node %x is not executable.\n", 
		   s->major_type);
	    break;
	}
    
	s = s->next;

    } while ( s != stmtlst );
    return;
}

/*
 * Clanger expression evaluator.
 * This is where it gets hairy!
 */
PUBLIC void Clanger_Eval(Clanger_st *st, Expr_t *e,
			 /* OUT */ Type_Any *a )
{

    /* Ensure that result is initialised */
    a->type = a->val = 0;

    switch( e->major_type ) {

    case NODE_BOOLEAN:
	EvalBoolean(st, e, a );
	break;
    case NODE_UNARY:
	EvalUnary(st, e, a);
	break;
    case NODE_BINARY:
	EvalBinary(st, e, a );
	break;
    case NODE_IDENT:
        {
	    char *name= (string_t)((word_t)(e->val));

	    if(name[0]=='>') { /* absolute pathname => just one lookup */
		name++;
		if (!Context$Get((Context_clp)(st->namespace), name, a)) {
		    DB(printf("Clanger_Eval: failed to find %s\n", name));
		    RAISE_Context$NotFound(name);
		}
	    } else {   
		/* Non-absolute pathname => look up first in local namespace, 
		   and then as global identfiers. NB: this means local vars 
		   can shadow absolute ones. */
		if (!Context$Get((Context_clp)(st->namespace), name, a)) {
		    DB(printf("Clanger_Eval: failed to find %s locally\n",
			      name));
		    if (!Context$Get(st->current, name, a)) {
			DB(printf("Clanger_Eval: failed to find %s\n", name));
			RAISE_Context$NotFound( name);
		    }
		}
	    }
	}
	break;
    case NODE_NUMBER:
	ANY_INIT(a, int64_t, e->val);
	break;
    case NODE_STRING:
	ANY_INIT(a, string_t, e->val);
	break;
    case NODE_LITBOOL:
	ANY_INIT(a, bool_t, e->val);
	break;
    case NODE_LITNULL:
	ANY_INIT(a, addr_t, NULL);
	break;
    case NODE_INVOCVAL:
	Clanger_Function(st, 
			 (string_t) (word_t) e->ch1->val,
			 (string_t) (word_t) e->ch2->val,
			 e->ch3,
			 NULL,
			 e->ch4 ? (string_t) (word_t) e->ch4->val : NULL,
			 False, NULL, NULL, /* nospawn */
			 a);
	break;
    case NODE_SPAWN:
	SpawnInvoc(st,(string_t)(word_t)e->ch1->val,e->ch2,e->ch3,a);
	break;
    case NODE_CONS:
	EvalConstructor(st, e, a);
	break;
    case NODE_CONSLIST:
	EvalListConstructor(st, e, a);
	break;
    default:
	DB(printf("Clanger$Eval: invalid node type %d\n", e->major_type));
	ANY_INIT(a, int64_t, 0 );
	break;
    }
}

/* 
 * Type-checked assignment, including range checks.
 */
PUBLIC bool_t
Clanger_Assign (Clanger_st *st, Type_Any *lval, Type_Any *val)
{
    Type_Any 	tmp;
    Type_Any 	ltmp;
    bool_t	res;
    int64_t	sval;
    uint64_t	uval;

    ANY_COPY (&tmp, val);
    ANY_COPY (&ltmp, lval);

    UnAlias (&tmp);
    UnAlias (&ltmp);

    if (TypeSystem$IsType (Pvs(types), tmp.type, ltmp.type))
    {
	/* No problem ... */
	lval->val = val->val; /* leave lval->type alone */
	return True;
    }
    /* If both have numeric types, do the range check, otherwise barf. */

    PromoteInt (&tmp);

    TRC(printf("++ promoted int val, now type is %qx\n", tmp.type));

    switch (ltmp.type)
    {
	/* signed cases */
    case int8_t__code:
	if ((res = RangeCheckSigned (&tmp, -0x80ll, 0x7fLL, &sval)))
	    (int8_t) lval->val = (int8_t) sval;
	break;

    case int16_t__code:
	if ((res = RangeCheckSigned (&tmp, -0x8000LL, 0x7fffLL, &sval)))
	    (int16_t) lval->val = (int16_t) sval;
	break;

    case int32_t__code:
	if ((res = RangeCheckSigned (&tmp, -0x80000000LL, 0x7fffffffLL, &sval)))
	    (int32_t) lval->val = (int32_t) sval;
	break;

    case int64_t__code:
	if ((res = RangeCheckSigned (&tmp, (int64_t) 0x8000000000000000ULL,
				     0x7fffffffffffffffLL, &sval)))
	    (int64_t) lval->val = (int64_t) sval;
	break;

	/* unsigned cases */
    case uint8_t__code:
	if ((res = RangeCheckUnsigned (&tmp, 0xffULL, &uval)))
	    (uint8_t) lval->val = (uint8_t) uval;
	break;

    case uint16_t__code:
	if ((res = RangeCheckUnsigned (&tmp, 0xffffULL, &uval)))
	    (uint16_t) lval->val = (uint16_t) uval;
	break;

    case uint32_t__code:
	if ((res = RangeCheckUnsigned (&tmp, 0xffffffffULL, &uval)))
	    (uint32_t) lval->val = (uint32_t) uval;
	break;

    case uint64_t__code:
	if ((res = RangeCheckUnsigned (&tmp, 0xffffffffffffffffULL, &uval)))
	    (uint64_t) lval->val = (uint64_t) uval;
	break;

    case word_t__code:
	if ((res = RangeCheckUnsigned (&tmp, WORD_MAX, &uval)))
	    (word_t) lval->val = (word_t) uval;
	break;

    case addr_t__code:
	if ((res = RangeCheckUnsigned (&tmp, WORD_MAX, &uval)))
	    lval->val = (word_t) uval;
	break;

    default:
	res = False;
    }

    return res;
}

/* 
 * In the next two functions, "val" must have been "PromoteInt"ed.
 */
static bool_t
RangeCheckSigned (Type_Any *val, int64_t min, int64_t max, int64_t *sval)
{
    switch (val->type)
    {
    case int32_t__code:	*sval = (int32_t)  val->val; break;
    case int64_t__code:	*sval = (int64_t)  val->val; break;
    case uint32_t__code:	*sval = (uint32_t) val->val; break;

    case uint64_t__code: /* need to worry about overflow */
	if ((uint64_t) val->val > 0x7fffffffffffffffULL)
	    return False;
	else
	    *sval = (uint64_t) val->val;
	break;

    default:
	return False;
    }
    return (*sval >= min && *sval <= max);
}

static bool_t
RangeCheckUnsigned (Type_Any *val, uint64_t max, uint64_t *uval)
{
    switch (val->type)
    {
    case int32_t__code:
	if ((int32_t) val->val < 0)
	    return False;
	else
	    *uval = (int32_t) val->val;
	break;

    case int64_t__code:
	if ((int64_t) val->val < 0)
	    return False;
	else
	    *uval = (int64_t) val->val;
	break;

    case uint32_t__code:	*uval = (uint32_t) val->val; break;
    case uint64_t__code:	*uval = (uint64_t) val->val; break;
    default:
	return False;
    }
    return (*uval <= max);
}


/*
 * Evaluate a boolean operation (&&, || or !)
 */
static void EvalBoolean(Clanger_st *st, Expr_t *e, 
			/* OUT */ Type_Any *a )
{
    switch( e->val ) {
    case '!':
	ANY_INIT(a, bool_t, !EvalToBoolean(st, e->ch1 ));
	break;
    case TK_OR:
	ANY_INIT(a, bool_t, EvalToBoolean(st, e->ch1 ) ||
		 EvalToBoolean(st, e->ch2 ));
	break;
    case TK_AND:
	ANY_INIT(a, bool_t, EvalToBoolean(st, e->ch1 ) &&
		 EvalToBoolean(st, e->ch2 ));
	break;
    default:
	DB(printf("Clanger$EvalBoolean: bad op code %d\n", e->val));
	RAISE_Clanger$InternalError();
    }
    TRC(printf("Clanger$EvalBoolean: returning %s\n", 
		a->val? "True" : "False"));
}

/*
 * Evaluate an expression and convert to a boolean 
 */
static bool_t EvalToBoolean(Clanger_st *st, Expr_t *e )
{
    Type_Any a;

    Clanger_Eval(st, e, &a);
    UnAlias(&a);
    PromoteInt(&a);
    NormaliseInt(&a);

    if ( a.type == bool_t__code ) return (bool_t)(a.val);

    EnsureInt(&a);
    return ( a.val != 0 );
}

/*
 * Evaluate a unary expression
 */
static void EvalUnary(Clanger_st *st, Expr_t *e, 
		      /* OUT */ Type_Any *a )
{
    Clanger_Eval(st, e->ch1, a );
    UnAlias(a);
    PromoteInt(a);
  
    if ( a->type == float32_t__code || a->type == float64_t__code ) {
	DB(printf("Clanger$EvalUnary: floats not supported.\n"));
	RAISE_Clanger$InternalError();
    }

    EnsureInt(a);

    switch( e->val ) {
    case TKF_UPLUS:
	break;
    case TKF_UMINUS:
	a->val = - ((int64_t)(a->val)); break;
    case '~':
	a->val = ~(a->val); break;
    default:
	DB(printf("Clanger$EvalUnary: bad operator %d\n", e->val));
	RAISE_Clanger$InternalError();
    };
    NormaliseInt(a);
    TRC(printf("Clanger$EvalUnary: returning %d\n", a->val));
}

/*
 * Evaluate a binary expression
 */
static void EvalBinary(Clanger_st *st, Expr_t *e, 
		       /* OUT */ Type_Any *a )
{
    Type_Any o1, o2;

    TRC(printf("EvalBinary:");DumpExpr(e));

    /* Evaluate both operands */
    Clanger_Eval(st, e->ch1, &o1 );
    Clanger_Eval(st, e->ch2, &o2 );
  
    /* Convert to canonical types */ 
    UnAlias(&o1);
    UnAlias(&o2);
    
    if ((o1.type == string_t__code) && (o2.type == string_t__code)) {
	string_t s1 = (string_t) (word_t) o1.val;
	string_t s2 = (string_t) (word_t) o2.val;
	string_t result;
	/* These are strings */

	switch (e->val) {
	    
	case '+':
	    result = malloc(strlen(s1) + strlen(s2) + 1);
	    strcpy(result, s1);
	    strcat(result, s2);
	    ANY_INIT(a, string_t, result);
	    break;

	case TK_EQ:
	    ANY_INIT(a, bool_t, (strcmp(s1, s2) == 0));
	    break;

	case TK_NE:
	    ANY_INIT(a, bool_t, (strcmp(s1, s2) != 0));
	    break;

	default:
	    fprintf(stderr, "Got unknown string op %d (%c)\n", e->val, e->val);
	    RAISE_Clanger$Type();
	    break;
	}

    } else {
	bool_t sgned = False;

	/* These are hopefully some kind of integers */
	PromoteInt(&o1);
	PromoteInt(&o2);
	/* Ensure we have what we think we have */
	EnsureInt(&o1);EnsureInt(&o2);
	
	/* Set the type of the result */
	if ( o1.type == uint64_t__code || o2.type == uint64_t__code ) {
	    ANY_CAST(a, uint64_t );
	    sgned  = False;
	} else if ( o1.type == int64_t__code || o2.type == int64_t__code ) {
	    ANY_CAST(a, int64_t );
	    sgned  = True;
	} else if ( o1.type == uint32_t__code || o2.type == uint32_t__code ) {
	    ANY_CAST(a, uint32_t );
	    sgned  = False;
	} else {
	    ANY_CAST(a, int32_t );
	    sgned  = True;
	}
	
	/* Normalise the values */
	NormaliseInt(&o1); NormaliseInt(&o2);
	
	switch ( e->val ) {
	case '|':
	    a->val = o1.val | o2.val; break;
	case '^':
	    a->val = o1.val ^ o2.val; break;
	case '&':
	    a->val = o1.val & o2.val; break;
	case TK_LEFT:
	    a->val = o1.val << o2.val; break;
	case TK_RIGHT:
	    a->val = o1.val >> o2.val; break;
	case '-':
	    a->val = o1.val - o2.val; break;
	case '+':
	    a->val = o1.val + o2.val; break;
	case TK_EQ:
	    ANY_INIT(a, bool_t, (o1.val == o2.val) ); break;
	case TK_NE:
	    ANY_INIT(a, bool_t, (o1.val != o2.val) ); break;
	case '%':
	    a->val = sgned ? 
		(int64_t)(o1.val) % (int64_t)(o2.val) : o1.val % o2.val;
	    break;
	case '*':
	    a->val = sgned ? 
		(int64_t)(o1.val) * (int64_t)(o2.val) : o1.val * o2.val;
	    break;
	case '/':
	    a->val = sgned ? 
		(int64_t)(o1.val) / (int64_t)(o2.val) : o1.val / o2.val;
	    break;
	case '<':
	    a->val = sgned ? 
		(int64_t)(o1.val) < (int64_t)(o2.val) : o1.val < o2.val;
	    ANY_CAST(a, bool_t ); break;
	case TK_GT:
	    a->val = sgned ? 
		(int64_t)(o1.val) > (int64_t)(o2.val) : o1.val > o2.val;
	    ANY_CAST(a, bool_t ); break;
	case TK_LE:
	    a->val = sgned ? 
		(int64_t)(o1.val) <= (int64_t)(o2.val) : o1.val <= o2.val;
	    ANY_CAST(a, bool_t ); break;
	case TK_GE:
	    a->val = sgned ? 
		(int64_t)(o1.val) >= (int64_t)(o2.val) : o1.val >= o2.val;
	    ANY_CAST(a, bool_t ); break;
	default:
	    DB(printf("Clanger$EvalBinary: bad operator %d\n", e->val));
	    RAISE_Clanger$InternalError();
	};
	NormaliseInt(a);
    }
}

/* 
 * Evaluate a context constructor
 */
static void
EvalConstructor (Clanger_st *st, Expr_t *e, /* OUT */ Type_Any *a)
{
    Expr_t       *bindings = e->ch1;
    Expr_t       *bind;
    Context_clp	cx= ContextMod$NewContext (st->cmod, Pvs(heap), Pvs(types));

    if ((bind = bindings))
	do
	{
	    Bind (st, bind->ch1, bind->ch2, cx, True);
	    bind = bind->next;
	}
	while (bind != bindings);

    ANY_INIT (a, Context_clp, cx);
}

/* 
 * Evaluate a list constructor
 */
static void
EvalListConstructor (Clanger_st *st, Expr_t *e, /* OUT */ Type_Any *a)
{
    Expr_t       *bindings = e->ch1;
    Expr_t       *bind;
    int n;
    Type_AnySeq *list;
    n=0;
    if ((bind = bindings))
	do
	{
	    n++;
	    bind = bind->next;
	}
	while (bind != bindings);
    list = SEQ_NEW(Type_AnySeq, n, Pvs(heap));
    n=0;
    if ((bind = bindings))
	do
	{
	    Type_Any any;
	    Clanger_Eval(st, bind, &any);
	    SEQ_ELEM(list, n) = any;
	    n++;
	    bind = bind->next;
	}
	while (bind != bindings);
    ANY_INIT (a, Type_AnySeq, list);
}

/* 
 * Bind id "l" to expr "r" in context "cx",
 * possibly creating missing subcontexts.
 */

static void
Bind (Clanger_st *st, Expr_t *l, Expr_t *r,
      Context_clp cx, bool_t grow)
{
    Type_Any a, tmp;
    string_t lname = (string_t) (word_t) l->val;
  
    if (r)
	Clanger_Eval(st, r, &a);
    else
	ANY_INIT (&a, bool_t, True); /* no rvalue, so it's a flag */

    TRC (printf("Clanger$Bind: rval (%llx, %x) to ", a.type, a.val));
  
    if (lname)
    {
	TRC(printf ("%s, grow=%d\n", lname, grow));

	/* if we don't know which context to bind in, try either the
         * root, or the current one, depending on lname */
	if (!cx)
	{
	    if (lname[0] == '>')
	    {
		cx = (Context_clp)(st->namespace);
		lname++;
	    }
	    else
	    {
		cx = st->current;
	    }
	}

	if (grow) /* XXX - this could belong in Context as a param to Add */
	{
	    bool_t      growing = False; /* until proven guilty */
	    Context_clp head_cx = cx;
	    string_t    head    = lname;
	    string_t    tail;
	    Type_Any	tmp;

	    while ((tail = strpbrk (head, SEP_STRING)) != NULL)
	    {
		char sep = *tail;
		*tail = '\0';
		TRC(printf("does '%s' exist in %x?\n", head, head_cx));
		if (growing || !Context$Get(head_cx, head, &tmp))
		{
		    Context_clp new_cx = 
			ContextMod$NewContext(st->cmod, Pvs(heap), Pvs(types));
		    TRC(printf(" - either growing(%d) or apparently not. "
			       "Made new CX at %x\n", growing, new_cx));
		    ANY_INIT (&tmp, Context_clp, new_cx);
		    Context$Add (head_cx, head, &tmp);
		    growing = True;
		}
		*tail++ = sep;
		head    = tail;
		head_cx = NARROW (&tmp, Context_clp);
	    }
	}
	
	if(Context$Get(cx, lname, &tmp)) {
	    Context$Remove (cx, lname);
	}

	if (ISTYPE(&a, string_t))
	{
	    a.val = (word_t) strdup((string_t)(word_t)(a.val));
	}

	Context$Add    (cx, lname, &a);

	{
	    Type_Any ts_any;

	    TypeSystem$Info(Pvs(types), a.type, &ts_any);
	    TRC({
		string_t tname = TypeSystem$Name(Pvs(types), a.type);
		printf("%s : val= %qx, type is \"%s\"\n", 
		       lname, a.val, tname);
		FREE(tname);
	    });
	}
    }
    else
    {
	TRC (printf ("(void)\n"));
    }
}


/*
 * Strip down an alias type to its bare bones
 */
PUBLIC void UnAlias( Type_Any *a )
{
    Type_Code base = TypeSystem$UnAlias (Pvs(types), a->type);
    ANY_CASTC (a, base);
}

/* This now is in TypeSystem */
PUBLIC Type_Code
UnAliasCode (Type_Code tc)
{
    return TypeSystem$UnAlias (Pvs(types), tc);
}

/*
 * Promote an integer type as much as possible
 */
static void PromoteInt( Type_Any *a )
{
    switch ( a->type ) {
    
    case int8_t__code:
	a->val = (int64_t)((int8_t)(a->val));
#ifdef __ALPHA__
	a->type = int64_t__code;
#else /* ! __ALPHA__ */
	a->type = int32_t__code;
#endif
	break;

    case int16_t__code:
	a->val = (int64_t)((int16_t)(a->val));
#ifdef __ALPHA__
	a->type = int64_t__code;
#else /* ! __ALPHA__ */
	a->type = int32_t__code;
#endif
	break;

    case int32_t__code:
	a->val = (int64_t)((int32_t)(a->val));
#ifdef __ALPHA__
	a->type = int64_t__code;
#else /* ! __ALPHA__ */
	a->type = int32_t__code;
#endif
	break;

    case uint8_t__code:
	a->val = (uint64_t)((uint8_t)(a->val));
#ifdef __ALPHA__
	a->type = uint64_t__code;
#else /* ! __ALPHA__ */
	a->type = uint32_t__code;
#endif
	break;

    case uint16_t__code:
	a->val = (uint64_t)((uint16_t)(a->val));
#ifdef __ALPHA__
	a->type = uint64_t__code;
#else /* ! __ALPHA__ */
	a->type = uint32_t__code;
#endif
	break;

    case uint32_t__code:
	a->val = (uint64_t)((uint32_t)(a->val));
#ifdef __ALPHA__
	a->type = uint64_t__code;
#else /* ! __ALPHA__ */
	a->type = uint32_t__code;
#endif
	break;

    case word_t__code:
	a->val = (uint64_t)((word_t)(a->val));
#ifdef __ALPHA__
	a->type = uint64_t__code;
#else /* ! __ALPHA__ */
	a->type = uint32_t__code;
#endif
	break;

    case bool_t__code:
	a->val = (uint64_t)((bool_t)(a->val));
#ifdef __ALPHA__
	a->type = uint64_t__code;
#else /* ! __ALPHA__ */
	a->type = uint32_t__code;
#endif
	break;
	
    }
}

/*
 * Normalise an integer any
 */
static void NormaliseInt(Type_Any *a )
{
    switch( a->type ) {
    case bool_t__code:
	break;
    case int32_t__code:
	a->val = ((int32_t)(a->val));
	break;
    case uint32_t__code:
	a->val = ((uint32_t)(a->val));
	break;
    case int64_t__code:
	a->val = ((int64_t)(a->val));
	break;
    case uint64_t__code:
	a->val = ((uint64_t)(a->val));
	break;
    default:
	DB(printf("Clanger$NormaliseInt: bad type code %x\n", a->type));
	RAISE_Clanger$InternalError();
    }
}

/*
 * Ensure that a value is a promoted integer
 */
static void EnsureInt(Type_Any *a)
{
    if ( a->type == float32_t__code || a->type == float64_t__code ) {
	DB(printf("Clanger$EvalUnary: floats not supported.\n"));
	RAISE_Clanger$InternalError();
    }

    if (a->type != int64_t__code &&
	a->type != int32_t__code &&
	a->type != uint64_t__code &&
	a->type != uint32_t__code ) {
	DB(printf("Clanger$EvalUnary: not an integer.\n"));
	RAISE_Clanger$Type();
    }
}

#ifdef DUMPEXPR
static void DumpExpr(Expr_t *e )
{
    printf("Expression (at %x): \n", e);

    printf("  next = %x\n",e->next);
    printf("  prev = %x\n",e->prev);
    printf("  majr = %x\n",e->major_type);
    printf("  minr = %x\n",e->minor_type);
    printf("  val  = %x\n",e->val);
    printf("  ch1  = %x\n",e->ch1);
    printf("  ch2  = %x\n",e->ch2);
    printf("  ch3  = %x\n",e->ch3);
    printf("  ch4  = %x\n",e->ch4);
    printf("  ch5  = %x\n",e->ch5);
}
#endif

static void SpawnInvoc(Clanger_st *st, string_t name, Expr_t *env,
		       Expr_t *s,
		       /* OUT */ Type_Any *a)
{
    switch (s->major_type) {
    case NODE_INVOCST:
	Clanger_Function(st, 
			 (string_t) (word_t) s->ch2->val,
			 (string_t) (word_t) s->ch3->val,
			 s->ch4,
			 s->ch1,
			 NULL, True, name, env, NULL);
	break;
    case NODE_INVOCVAL:
	Clanger_Function(st, 
			 (string_t) (word_t) s->ch1->val,
			 (string_t) (word_t) s->ch2->val,
			 s->ch3,
			 NULL,
			 s->ch4 ? (string_t) (word_t) s->ch4->val : NULL,
			 True, name, env,
			 a);
	break;
    default:
	printf("Spawn of something other than an invocation - aargh!\n");
	break;
    }
}

/* XXX this doesn't terminate Clanger, it terminates the current domain.
   Scary. Don't do it. */
void TerminateShell()
{
    DomainMgr_cl *dm;

    printf("Clanger: exiting.\n");
    dm= IDC_OPEN("sys>DomainMgrOffer", DomainMgr_clp);
    DomainMgr$Destroy(dm, VP$DomainID(Pvs(vp)));
}

#ifdef BUILTIN_LIST

static void print_item(Type_Any *any, char *name) 
{
    Type_Any rep, tmp_any;
    string_t tname;
    Interface_clp scope;

    tname = TypeSystem$Name(Pvs(types), any->type);
    
    scope = TypeSystem$Info(Pvs(types), any->type, &tmp_any);
    
    UnAlias(any);

    TypeSystem$Info(Pvs(types), any->type, &rep);

    {

	string_t intfname, fullname;
	Interface_Needs *needs;
	bool_t local;
	Type_Code code;

	needs = Interface$Info(scope, &local, &intfname, &code);

	if(strcmp(intfname, "IREF") != 0) {
	    int len = strlen(intfname) + strlen(tname)+2;
	    fullname = Heap$Malloc(Pvs(heap), len);
	    sprintf(fullname, "%s.%s", intfname, tname);

	    FREE(tname);
	    tname = fullname;

	} 

	FREE(intfname);
	SEQ_FREE(needs);

    }
	
    printf("%-20s : %-20s = %a\n", name, tname, any);

    FREE(tname);

}

static int my_strcmp(const void *k1, const void *k2) 
{
    return strcmp(*(const char **)k1,*(const char **)k2);
}

#endif /* BUILTIN_LIST */

static void List (Clanger_st *st, Expr_t *l)
{
#ifdef BUILTIN_LIST
    Context_Names  *NOCLOBBER names;
    int NOCLOBBER i;
    Type_Any any;
#endif /* BUILTIN_LIST */
    Context_clp    cx;
    Context_clp NOCLOBBER the_cx;
    Type_Any       cx_any;
    bool_t NOCLOBBER is_cx;
    string_t NOCLOBBER path = l? ((string_t) (word_t) l->val) : NULL;

    if(!path) { 
	the_cx= st->current;
	is_cx = True;
	/* XXX We don't actually know whether st->current is a MergedContext
	   or not; if it's the root of a domain then it probably is. This
	   is bad, anyway. */
	ANY_INIT(&cx_any, MergedContext_clp, st->current);
	path="(current context)";
    } else {
	if(path[0]=='>') {     /* absolute path */
	    path++;            /* => skip over first char */
	    cx= (Context_clp)(st->namespace); /*    and lookup in 'root' */
	} else cx= st->current;

	the_cx= (Context_cl *)NULL;
	if(!Context$Get(cx, path, &cx_any)) {
	    printf("%s not found.\n", path);
	    return;
	}

	if (ISTYPE(&cx_any, Context_clp)) {
	    the_cx= NARROW(&cx_any, Context_clp);
	    is_cx= True;
	} else {
	    Type_Code real_tc = TypeSystem$UnAlias(Pvs(types), cx_any.type);
	    if(TypeSystem$IsType(Pvs(types), real_tc, Context_clp__code)) {
		the_cx = (Context_clp)(pointerval_t)cx_any.val;
		is_cx = True;
	    } else {
		is_cx= False;
	    }
	} 
    }

#ifdef BUILTIN_LIST
    if(is_cx) {
	/* Ok, here we have 'the_cx' holding where we want to list */
	names= Context$List(the_cx);
	qsort( SEQ_START(names), SEQ_LEN(names), SEQ_DSIZE(names), my_strcmp);
	for(i=0; i < SEQ_LEN(names); i++) {
	    TRY {
		if(!Context$Get(the_cx, SEQ_ELEM(names, i), &any)) {
		    printf("Clanger: typesystem has changed under my feet!\n");
		    RAISE_Clanger$InternalError();
		}
	    } CATCH_TypeSystem$BadCode(c) {
		printf("Bad typecode %qx on item %s\n",c,SEQ_ELEM(names,i));
	    } ENDTRY;
	    TRY {
		print_item(&any, SEQ_ELEM(names, i));
	    } CATCH_ALL {
		printf("Caught exception %s on item %s\n",
		       exn_ctx.cur_exception, SEQ_ELEM(names,i));
	    } ENDTRY;
#if 0
	    /* XXX; fix to use strings */
	    if(!((i+1)&0x1f)) {
		printf("----- Hit <Return> to Continue ('q' to quit) -----\n");
		if((c= Rd$GetC(st->rd))=='q')
		    i= SEQ_LEN(names);
	    }
#endif /* 0 */
	}
    } else {
	/* Ok, here cx_any is not in fact a context, just a 'simple' object. */
	print_item(&cx_any, path);
    }
#endif /* BUILTIN_LIST */
    ContextUtil$List(st->cutil, &cx_any, Pvs(out), path);
    return;
}

static void Pause(Clanger_st *st, Expr_t *l) {
    /* la la la.. maybe I ought to do more here */
    PAUSE(l->val);
}

static void ChangeCurContext (Clanger_st *st,  Expr_t *l)
{
    Context_clp NOCLOBBER cx;
    Context_clp NOCLOBBER the_cx;
    string_t NOCLOBBER path = l? ((string_t) (word_t) l->val) : NULL;
    char buf[256];
    bool_t NOCLOBBER ok = True;

    if(!path) { /* this is how we get back to 'root' again */
	if(st->cur_name) FREE(st->cur_name);
	st->cur_name= (char *)NULL;
	the_cx= (Context_clp)(st->namespace);
    } else {
	if(path[0]=='>') {     /* absolute path */
	    path++;            /* => skip over first char */
	    cx= (Context_clp)(st->namespace); /*    and lookup in 'root' */
	} else cx= st->current;

	the_cx= (Context_cl *)NULL;
	TRY {
	    the_cx= NAME_LOOKUP(path, cx, Context_clp);
	} CATCH_Context$NotContext() {
	    printf("%s is not a context => cannot 'ls' it.\n", path);
	    ok= False;
	} CATCH_Context$Denied() {
	    printf("** access denied.\n");
	    ok= False;
	} CATCH_Context$NotFound(name) {
	    printf("%s not found.\n", name);
	    ok= False;
	} CATCH_ALL {
	    ok= False;
	} ENDTRY;
	if(!ok) 
	    return;

	if(path[0]!='>') { /* relative path */
	    sprintf((char *)buf, "%s>%s", (st->cur_name) ?
		    st->cur_name:"", path);
	    if(st->cur_name) FREE(st->cur_name);
	    st->cur_name= strdup(buf);
	} else {
	    if(st->cur_name) FREE(st->cur_name);
	    if(strlen(path)>1) {
		sprintf((char *)buf, "%s", path);
		st->cur_name= strdup(buf);
	    } else st->cur_name= (char *)NULL;
	}
    }

    /* Now set the new path */
    if(st->current != the_cx)
	st->current = the_cx;

    printf("Current context is %s\n", 
	       (st->cur_name==NULL)?">":st->cur_name);

    return;
}

static void PrintCurContext (Clanger_st *st)
{
    printf("Current context is %s\n", 
	       (st->cur_name==NULL)?">":st->cur_name);
    return;
}

static void Exec (Clanger_st *st, Expr_t *l, Expr_t *p)
{
    Exec_clp    exec= NAME_FIND("modules>Exec", Exec_clp);
    Type_Any    mod_any,env_any;
    Context_clp	the_cx, exts;
    Domain_ID   did;
    char *file = (char *)((word_t) l->val);
    char *parm = p?(char *)((word_t) p->val):NULL;
    char *name;
    int lastlevel;
    char *cptr;
    int count;
    char *fileproper, *path;
    char buffer[1024];

    TRC(eprintf("Clanger exec entered\n"));

    TRC(eprintf("Looking up the file in the namespace\n"));
    /* Lookup the file in the namespace */
    if(file[0]=='>') { /* absolute pathname */
	file++;        /* => skip first char */
	the_cx= (Context_clp)(st->namespace);
    } else the_cx= st->current;

    TRC(eprintf("Lookup up the param context\n"));
    /* Lookup the param context */
    if (parm) {
	if (parm[0]=='>') {
	    file++;
	    exts=(Context_clp)(st->namespace);
	} else exts=st->current;
    } else {
	exts=NULL;
    }
    lastlevel = 0;
    cptr = file;
    count = 0;
    while (*cptr) {
	if (*cptr == '>') lastlevel = count;
	count++;
	cptr++;
    }
    fileproper=file;
    path = 0;
    if (lastlevel) {
	cptr = file;
	path = buffer;
	count =0;
	while (count < lastlevel) {
	    *path = *cptr;
	    path++;
	    cptr++;
	    count++;
	}
	fileproper = file;
	*path = 0;
	path++;
	cptr++;
	while (*cptr) {
	    *path = *cptr;
	    path++;
	    cptr++;
	}
	*path = 0;
	path = buffer;
    }
    TRC(eprintf("Looking up the file in the context\n"));
	if(!(Context$Get(the_cx, file, &mod_any))) {
	    printf("Clanger Exec: cannot find file %s\n", file);
	    RAISE_Clanger$InternalError();
	}

    /* take the last component as the name of the domain */
    name = strrchr(file, '>');
    if (!name)
	name = file;
    else
	name++;
    TRC(eprintf("Doing context get on env\n"));
    if (exts)
    {
	if(!(Context$Get(exts, parm, &env_any)))
	{
	    printf("Clanger Exec: cannot find environment context %s\n", parm);
	    RAISE_Clanger$InternalError();
	}
	exts=NARROW(&env_any, Context_clp);
    }

#if 0
    pdid = VP$ProtDomID(Pvs(vp)); /* XXX should be new? user-def? */
#endif /* 0 */
    TRC(eprintf("Calling exec run\n"));
    /* use NULL pdom to mean use "pdom" from namespace or create new one */
    did = Exec$Run(exec, &mod_any, exts, NULL_PDID, name);

    TRC(printf("Clanger: new domain (id=%qx) created.\n", did));

    return;
}

static void Fork(Clanger_st *st, Expr_t *cls, Expr_t *stk)
{
    Context_clp the_cx;
    string_t closure_name = (string_t)((word_t)cls->val);
    Type_Any any;
    Closure_clp cl;

    if (closure_name[0]=='>') {
	closure_name++;
	the_cx = (Context_clp)(st->namespace);
    } else the_cx = st->current;

    if (!Context$Get(the_cx, closure_name, &any)) {
	printf("Clanger: cannot find %s\n", closure_name);
	RAISE_Clanger$InternalError();
    }

    cl=NARROW(&any, Closure_clp);
    Threads$Fork(Pvs(thds),cl->op->Apply,cl,stk?stk->val:0);
}

static void Remove(Clanger_st *st, Expr_t *l)
{
    Context_clp	NOCLOBBER the_cx;
    char *NOCLOBBER name = (char *)((word_t) l->val);
    Type_Any junk;

    /* Lookup the file in the namespace */
    if (name[0]=='>') { /* absolute pathname */
	name++;        /* => skip first char */
	the_cx= (Context_clp)(st->namespace);
    } else the_cx= st->current;
    
    if(!(Context$Get(the_cx, name, &junk))) {
	printf("Clanger: cannot find %s\n", name);
	RAISE_Clanger$InternalError();
    }
    
    TRY {
	Context$Remove(the_cx, name);
    } CATCH_Context$Denied() {
	printf("Clanger: permission denied (rm of %s)\n", name);
    } ENDTRY;

    TRC(printf("Clanger: removed %s\n", name));
    return;
}




static Rd_clp AnyToRd(Type_Any *item) {
    /* Ok, it must be a File, or a Rd, or an offer for one of these */

    /* bind IDCOffers */
    if (ISTYPE (item, IDCOffer_clp)) {
	ObjectTbl$Import (Pvs(objs), (IDCOffer_clp) (word_t) item->val, 
			  item);
    }

    /* open Files */
    if (ISTYPE (item, FileIO_clp)) {
	FSUtil_clp fsutil = NAME_FIND("modules>FSUtil", FSUtil_clp);	
	Rd_clp ret;
	
	ret = FSUtil$GetRd(fsutil, 
			   (FileIO_clp)(word_t)item->val, 
			   Pvs(heap), 
			   True);
	ANY_INIT(item, Rd_clp, ret);
    }

    /* load the contents of readers */
    if (!ISTYPE (item, Rd_clp)) {
	printf("Clanger AnyToRd: cannot manufacture a reader.\n");
	return 0;
    }
    
    return (Rd_clp)(word_t)item->val;
}

static void Include(Clanger_st *st, Expr_t *l)
{
    Type_Any item;
    Rd_clp rd; 

    Clanger_Eval(st, l, &item);

    rd = AnyToRd(&item);
    if (rd)    NewLexRd( st, rd);
    return;

}


static void PagedOutput(Clanger_st *st, Expr_t *l)
{

    Type_Any item;
    Rd_clp rd; 
    int c, cnt, linecnt;

    Clanger_Eval(st, l, &item);

    rd= AnyToRd(&item);
    cnt= linecnt= 0;
    
    TRY {
	while( (c = Rd$GetC(rd)) != EOF) {
	    Wr$PutC(st->wr, c);
	    cnt++;
#if 0
	    if(c=='\n') {
		Wr$Flush(st->raw_wr);
		cnt= 0;

		/* XXX; fix this to work when executing strings */
		if(++linecnt == 25) {
		    printf("-- Hit <Return> to Continue ('q' to quit) --\n");
		    if((c= Rd$GetC(st->rd))=='q')
			break;
		    linecnt= 0;
		}
	    }
#endif

	}
    } CATCH_Rd$EndOfFile() {
	printf("-- End of file.\n");
    } ENDTRY;

    return;

}

static void DestroyDomain (Clanger_st *st, Expr_t *l)
{
    Type_Any      any;
    Domain_ID     id;
    DomainMgr_clp dm;

    Clanger_Eval(st, l, &any);

    /* XXX XXX XXX this is a _horrible_ hack */
    id=NARROW(&any, int64_t);

    id |= 0xd000000000000000LL;

    printf("Kill: %qx\n",id);

    dm = IDC_OPEN("sys>DomainMgrOffer", DomainMgr_clp);
    DomainMgr$Destroy(dm, id);

    return;
}
