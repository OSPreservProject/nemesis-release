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
**      Nodes.c
** 
** FUNCTIONAL DESCRIPTION:
** 
**      Management of parse tree nodes in Clanger.
** 
** ENVIRONMENT: 
**
**      Part of Clanger interpreter - user space domain
** 
** ID : $Id: Nodes.c 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
** 
**
*/

#define DEBUG
#define CLANGER_TRACE

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

#include <nemesis.h>
#include <exceptions.h>
#include <links.h>
#include <stdio.h>

#include "Clanger_st.h"

/*
 * Names of node types.
 */
const char * const node_names[] = {
    "Loop", 
    "If",
    "Finally",
    "Raise",
    "Break",
    "InvocSt",
    "NullSt",
    "InvocVal",
    "Cons",
    "Bind",
    "Boolean",
    "Unary",
    "Binary",
    "Ident",
    "Number",
    "String",
    "LitBool"
};

/*
 * Build a new parse tree node
 */
Expr_t *MakeExprNode(Clanger_st *st, 
		     uint32_t major_type,
		     uint64_t val,
		     Expr_t  *ch1,
		     Expr_t  *ch2,
		     Expr_t  *ch3,
		     Expr_t  *ch4 )
{
    Expr_t *e;

    if ( !(e = Heap$Malloc(Pvs(heap),sizeof(*e))) ) {
	DB(fprintf(stderr, "Clanger$MakeExprNode: no memory.\n"));
	RAISE_Heap$NoMemory();
    }

    LINK_INITIALISE(e);

    e->chain = st->parse_tree; st->parse_tree = e;

    e->major_type = major_type;
    e->val = val;
    e->ch1 = ch1;
    e->ch2 = ch2;
    e->ch3 = ch3;
    e->ch4 = ch4;

    return e;
}

/* 
 * Free the current parse tree
 */
void FreeParse (Clanger_st *st)
{
    while (st->parse_tree)
    {
	Expr_t *e = st->parse_tree;

	st->parse_tree = e->chain;

	switch (e->major_type)
	{
	case NODE_IDENT:
	case NODE_STRING:
	    if (e->val) FREE ((addr_t) (word_t) e->val);
	    break;
	default:
	}

	FREE (e);
    }
}






