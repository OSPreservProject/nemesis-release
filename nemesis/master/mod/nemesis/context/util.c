/*
*****************************************************************************
**                                                                          *
**  Copyright 1998, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**	util.c
**
** FUNCTIONAL DESCRIPTION:
**
**	Do fun things with contexts
**
** ENVIRONMENT: 
**
**	User space
**
** ID : $Id: util.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Wr.ih>
#include <Context.ih>
#include <MergedContext.ih>
#include <TradedContext.ih>
#include <ContextUtil.ih>


#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

static  ContextUtil_List_fn             ContextUtil_List_m;

static  ContextUtil_op  ms = {
    ContextUtil_List_m
};

static const ContextUtil_cl  cl = { &ms, NULL };

CL_EXPORT (ContextUtil, ContextUtil, cl);

/*---------------------------------------------------- Entry Points ----*/

static string_t code_to_string(Type_Code code)
{
    Type_Any rep, tmp_any;
    string_t tname;
    Interface_clp scope;

    tname = TypeSystem$Name(Pvs(types), code);
    scope = TypeSystem$Info(Pvs(types), code, &tmp_any);
    
    code=TypeSystem$UnAlias(Pvs(types), code);

    TypeSystem$Info(Pvs(types), code, &rep);

    {
	string_t intfname, fullname;
	Interface_Needs *needs;
	bool_t local;
	Type_Code code;

	needs = Interface$Info(scope, &local, &intfname, &code);

	if (strcmp(intfname, "IREF") != 0) {
	    int len = strlen(intfname) + strlen(tname)+2;
	    fullname = Heap$Malloc(Pvs(heap), len);
	    sprintf(fullname, "%s.%s", intfname, tname);

	    FREE(tname);
	    tname = fullname;
	} 

	FREE(intfname);
	SEQ_FREE(needs);
    }

    return tname;
}

static void print_item(Wr_clp wr, uint32_t indent,
		       const Type_Any *any, char *name, char *owner)
{
    Type_Any foo;
    string_t tname, oname=NULL;
    int i;

    ANY_COPY(&foo, any);

    tname=code_to_string(foo.type);

    if (ISTYPE(&foo, IDCOffer_clp)) {
	oname=code_to_string(IDCOffer$Type(NARROW(&foo, IDCOffer_clp)));
    } else {
	oname=NULL;
    }

    if (oname) {
	string_t s;
	s=Heap$Malloc(Pvs(heap), strlen(tname)+11);
	sprintf(s, "%s(%s)",tname,oname);
	FREE(tname);
	FREE(oname);
	tname=s;
    }

    for (i=0; i<indent; i++) wprintf(wr, " ");
    if (name && owner) {
	wprintf(wr, "%-20s : %-20s %-18s = %a\n", name, tname, owner, any);
    } else if (name) {
	wprintf(wr, "%-20s : %-20s = %a\n", name, tname, any);
    } else {
	wprintf(wr, "(%s);   = %a\n", tname, any);
    }

    FREE(tname);
}

static int my_strcmp(const void *k1, const void *k2) 
{
    return strcmp(*(const char **)k1,*(const char **)k2);
}

static void list_context(Wr_clp wr,
			 Context_clp c, uint32_t indent, bool_t traded)
{
    Context_Names *names;
    Type_Any any;
    int NOCLOBBER i;
    char owner[20];
    Security_Tag t;

    names = Context$List(c);
    qsort(SEQ_START(names), SEQ_LEN(names), SEQ_DSIZE(names), my_strcmp);
    for (i=0; i < SEQ_LEN(names); i++) {
	if (!Context$Get(c, SEQ_ELEM(names, i), &any)) {
	    wprintf(wr,"Couldn't Context$Get %s\n",SEQ_ELEM(names, i));
	    continue;
	}
	if (traded) {
	    TRY {
		t=TradedContext$Owner((TradedContext_clp)c, SEQ_ELEM(names,i));
		sprintf(owner,"[%qx]",t);
	    } CATCH_ALL {
		sprintf(owner,"[unknown]");
	    } ENDTRY;
	}
	TRY {
	    print_item(wr, indent, &any, SEQ_ELEM(names, i),
		       traded?owner:NULL);
	} CATCH_ALL {
	    wprintf(wr,"Caught exception %s on item %s\n",
		    exn_ctx.cur_exception, SEQ_ELEM(names,i));
	} ENDTRY;
    }
    SEQ_FREE(names);
}

static void ContextUtil_List_m (
    ContextUtil_cl  *self,
    const Type_Any  *cx     /* IN */,
    Wr_clp  wr      /* IN */,
    string_t name )
{
    if (!ISTYPE(cx, Context_clp)) {
	print_item(wr, 0, cx, name, NULL);
	return;
    }

    /* It's a Context of some kind. Do we know about it? */

    if (ISTYPE(cx, MergedContext_clp)) {
	MergedContext_clp c;
	MergedContext_ContextList *l;
	int i;

	/* Iterate through all of the stacked contexts,
	   listing each of them */
	c=NARROW(cx, MergedContext_clp);
	l=MergedContext$ListContexts(c);
	wprintf(wr,"%-20s : MergedContext        = %p; %d members:\n",
		name, c, SEQ_LEN(l));
	for (i=0; i<SEQ_LEN(l); i++) {
	    wprintf(wr," (Context at %p, layer %d)\n",SEQ_ELEM(l, i), i);
	    list_context(wr, (Context_clp)SEQ_ELEM(l, i), 2, False);
	}
    } else if (ISTYPE(cx, TradedContext_clp)) {
	TradedContext_clp c;

	c=NARROW(cx, TradedContext_clp);
	wprintf(wr,"%-20s : TradedContext        = %p:\n", name, c);
	list_context(wr, (Context_clp)c, 1, True);
    } else {
	Context_clp c;

	c=NARROW(cx, Context_clp);
	wprintf(wr,"%-20s : Context              = %p:\n", name, c);
	list_context(wr, c, 1, False);
    }
}

/*
 * End 
 */

