/*
*****************************************************************************
**                                                                          *
**  Copyright 1997, University of Cambridge Computer Laboratory             *
**                                                                          *
**  All Rights Reserved.                                                    *
**                                                                          *
*****************************************************************************
**
** FILE:
**
**    clangermod.c
**
** FUNCTIONAL DESCRIPTION:
**
**    clanger start of day code
**
** ENVIRONMENT: 
**
**
**
** ID : $Id: clangermod.c 1.2 Thu, 20 May 1999 15:25:49 +0100 dr10009 $
**
**
*/

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <links.h>
#include <stdio.h>

#include <ClangerMod.ih>
#include <Clanger_st.h>

extern Clanger_op clanger_ms;

#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED\n", __FUNCTION__);


static	ClangerMod_New_fn 		ClangerMod_New_m;

static	ClangerMod_op	ms = {
  ClangerMod_New_m
};

const static	ClangerMod_cl	cl = { &ms, NULL };

CL_EXPORT (ClangerMod, ClangerMod, cl);


/*---------------------------------------------------- Entry Points ----*/

static Clanger_clp ClangerMod_New_m (
	ClangerMod_cl	*self)
{
    Clanger_st *st;
    Type_Any      any, ts_any;
    st = Heap$Malloc (Pvs(heap), sizeof (*st));
    if (!st) RAISE_Heap$NoMemory();

    /* now setup everything that isn't going to change with the way
       that this clanger is to do IO */

    st->yydebug = False;
    st->cmod = NAME_FIND("modules>ContextMod", ContextMod_clp);
    st->cutil = NAME_FIND("modules>ContextUtil", ContextUtil_clp);
    st->spawnmod = NAME_FIND("modules>SpawnMod", SpawnMod_clp);

    st->namespace = ContextMod$NewMerged(st->cmod, Pvs(heap), Pvs(types));
    
    st->current= (Context_clp)(st->namespace);
    st->cur_name= (char *)NULL;
    
    st->c      = ContextMod$NewContext(st->cmod,Pvs(heap),Pvs(types));
    
    MergedContext$AddContext(st->namespace,
			     st->c,
			     MergedContext_Position_Before, 
			     False, NULL );
    MergedContext$AddContext(st->namespace,
			     Pvs(root),
			     MergedContext_Position_After, 
			     False, NULL );

    /* We make a bit of an assumption about RootC being a MergedContext -
       but in any sensible domain it will be. */
    CX_ADD_IN_CX (st->c, "FrontC", st->c, Context_clp);
    CX_ADD_IN_CX (st->c, "RootC",  Pvs(root), MergedContext_clp);
    CX_ADD_IN_CX (st->c, "MergeC", st->namespace,MergedContext_clp);
    
    /* This appears to be left over from ages ago. Nothing else ever gets
       added to the list, anyway. */
    LINK_INIT(&(st->front));
    st->front.c = st->c;
    
    /*
    ** Finally, we want to get an 'IREF' context (which is a bit tricky) 
    ** and install it in the merged root 
    */
    if(!Context$Get((Context_cl *)Pvs(types), "IREF",  &any)) {
	printf("Clanger; can't find IREF.\n");
	return NULL;
    }

    TypeSystem$Info(Pvs(types), any.val, &ts_any);
     Context$Add(st->c, "IREF", &ts_any);

    /* set up the lexer */
    st->lex.chars[0] = '\0';
    st->lex.index  = 0;
    st->lex.state  = 0;
    st->lex.hexval = 0;

    st->closure.op = &clanger_ms;
    st->closure.st = st;
    st->lexitem = NULL;
    st->lexundoindex = 0;

    st->currentlineindex = 0; st->currentline[0] = 0; st->atendofline = 0;
    return & (st->closure);
}

/*
 * End 
 */
