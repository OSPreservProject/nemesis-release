/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless */

/* dtree.c; parse tree dump routines */

/* Dickon Reed, 1996 */
/* Richard Mortier, 1997 */

/* $Id: dtree.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ */

/*
 * $Log: dtree.c,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:09:28  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:35:07  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.11  1997/02/20 00:28:17  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:45:42  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 18:33:11  rmm1002
 * Remove ref to dump.h
 *
 * Revision 1.1  1997/01/30 21:43:49  rmm1002
 * Initial revision
 *
 */

#include <stdio.h>
#include <string.h>
#include "cluless.h"
#include "stackobj.h"
#include "tables.h"
#include "list.h"
#include "statement.h"
#include "type.h"
#include "spec.h"
#include "dtree.h"
#include "expr.h"
#include "group.h"
#include "parsupport.h"
#include "decl.h"

/* Dump out the parsetree, for debugging purposes; produce output
 * compatible with 'tree' package */
void dump_tree(OUTPUT *out, p_obj *me) {
  
  if (!me) 
    return;

  switch (me->cl) {
    case group_identifier:
      dprintf(out, "%s", LOOKUP_STRING(me, group_identifier_string));
      break;

    case group_expression: {
      int j, n;

      dprintf( out, "(%s ((%s) (", 
	       t_expr[me->form].name, 
	       LOOKUP_SUB(me, group_expression_ltype)? "L" : "R"); 
      if (LOOKUP_SUB(me, group_expression_type)) {
	dump_tree(out, LOOKUP_SUB(me, group_expression_type));
      } else {
	dprintf(out, "untyped");
      }
      dprintf(out, ")) ");
      n = t_expr[me->form].order;

      for (j=0; j < n; j++) {
	switch (t_expr[me->form].nature[j]) {
	  case n_empty:
	    break;

	  case n_cptr:
	    dprintf(out, "(%s)", me->ch[j].string);
	    break;

	  case n_integer:
	    dprintf(out, "(%d)", me->ch[j].integer);
	    break;

	  case n_real:
	    dprintf(out, "(%f)", me->ch[j].real); 
	    break;

	  default:
	    dump_tree(out, me->ch[j].sub);
	    break;
	} /* End switch */
      }
      dprintf(out, ")");
    }
    break;

    case group_list: {
      p_list *lptr;

      lptr = me;
      dprintf(out, "(list");
      while (lptr) {
	dprintf(out, "(");
	dump_tree(out, LOOKUP_SUB(lptr, group_list_g1));
	lptr = LOOKUP_SUB(lptr, group_list_next);
	dprintf(out, ")");
      }
      dprintf(out, ")");
    }
    break;

    case group_statement: {
      int i;

      dprintf(out, "(%s(", t_statement[me->form].name);
      for (i = 0; i < t_statement[me->form].order; i++) {
	if (t_statement[me->form].nature[i] != n_empty) {
	  dump_tree(out, me->ch[i].sub);
	}
      }
      dprintf(out, "))");
    }
    break;

    case group_sco: {
      dprintf(out, "sco(");
      dump_tree(out, LOOKUP_SUB(me, group_sco_scope));
      dump_tree(out, LOOKUP_SUB(me, group_sco_name));
      dprintf(out, ")");
    }
    break;

    case group_proc: {
      dprintf(out, "proc(");
      dump_tree(out, LOOKUP_SUB(me, group_proc_decl));
      dump_tree(out, LOOKUP_SUB(me, group_proc_statement));
      dprintf(out, ")");
    }
    break;

    case group_type: {
      int i;

      dprintf(out, "%s(", t_type[me->form].name);
      for (i=0; i < 3; i++) {
	if (me->form == type_method) {
	  if (i == 0) 
	    dprintf(out, "(args");
	  if (i == 1) 
	    dprintf(out, ")(rets");
	}
	switch(t_type[me->form].nature[i]) {
	  case n_empty:
	    break;

	  case n_cptr:
	    dprintf(out, "(%s)", me->ch[i].string);
	    break;

	  case n_integer:
	    dprintf(out, "(%d)", me->ch[i].integer);
	    break;

	  default:
	    dprintf(out, "(");
	    dump_tree(out, LOOKUP_SUB(me, i));
	    dprintf(out, ")");
	    break;
	}
      }
      if (me->form == type_method) 
	dprintf(out, ")");
      dprintf(out, ")");
    }
    break;

    case group_spec: {
      dprintf(out, "(%s(", t_spec[me->form].name);
      dprintf(out, "(");
      dump_tree(out, LOOKUP_SUB(me, group_spec_name));
      dprintf(out, ")(extends(");
      dump_tree(out, LOOKUP_SUB(me, group_spec_extends));
      dprintf(out, "))(needs(");
      dump_tree(out, LOOKUP_SUB(me, group_spec_needs));
      dprintf(out, "))(body(");
      dump_tree(out, LOOKUP_SUB(me, group_spec_body));
      dprintf(out, ")))))");
    }
    break;

    case group_decl: {
      dprintf(out, "(%s(", t_decl[me->form].name);
      dump_tree(out, LOOKUP_SUB(me, group_decl_name));
      dump_tree(out, LOOKUP_SUB(me, group_decl_type));
      if (me->form == decl_procedure || me->form == decl_procedure_nr) 
	dprintf(out, "(raises(");

      switch (t_decl[me->form].nature[0]) {
	case n_empty:
	  break;

	default:
	  dump_tree(out, LOOKUP_SUB(me, group_decl_g1));
	  break;
      } 
      if (me->form == decl_procedure || me->form == decl_procedure_nr) 
	dprintf(out, "))");
      dprintf(out, "))");
    }
    break;

    case group_exch:
      dprintf(out, " exch(");
      dump_tree(out, LOOKUP_SUB(me, group_exch_sco));
      dump_tree(out, LOOKUP_SUB(me, group_exch_handle));
      dprintf(out, ")");
      break;
  } /* End switch */
}
