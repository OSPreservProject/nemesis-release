/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless 
 *
 * dpexpr.c; dumpy Python code for expressions 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: dpexpr.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: dpexpr.c,v $
 * Revision 1.1  1997/06/25 17:09:28  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/12 13:18:36  rmm1002
 * Changes due to storing numbers as strings now (output lookups have to be
 * LOOKUP_STRING, etc).
 *
 * Revision 2.1  1997/04/04 15:34:49  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.13  1997/04/02 20:28:57  rmm1002
 * Changed calls of dump_python_r to include new param (opCode)
 *
 * Revision 1.12  1997/04/02 14:07:47  rmm1002
 * Continue trying to fix expression generation & to add WHILE-DO
 * Still outputs too many code.append stmts sometimes...
 *
 * Revision 1.11  1997/02/20 00:28:06  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:45:15  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:41:09  rmm1002
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
#include "decl.h"
#include "dtree.h"
#include "expr.h"
#include "group.h"
#include "parsupport.h"
#include "typesupport.h"
#include "dpython.h"

/* Used for compile time constants */
void dump_python_cexpr(OUTPUT *out, p_obj *me) {

  if (!me) 
    return;
  if (me->cl != group_expression) 
    return;

  switch (me->form) {
    case expr_subexpr:
      dprintf(out, "(");
      dump_python_cexpr(out, LOOKUP_SUB(me, expr_subexpr_sub));
      dprintf(out, ")");
      break;

    case expr_num_val:
      dprintf(out, " %s ", LOOKUP_STRING(me, expr_num_val_val));
      break;

    case expr_bool_val:
      dprintf(out, " %d ", LOOKUP_INT(me, expr_bool_val_val));
      break;

    case expr_char_val:
      dprintf(out, " \"\"\"\\\'%s\\\'\"\"\" ",
	      LOOKUP_STRING(me, expr_char_val_val));
      break;

    case expr_str_val:
      dprintf(out, " \"\"\"\\\"%s\\\"\"\"\" ",
	      LOOKUP_STRING(me, expr_str_val_val));
      break;

    case expr_real_val:
      dprintf(out, " %s ", LOOKUP_STRING(me, expr_real_val_val));
      break;

    case expr_positive_op:
      dprintf(out, "+(");
      dump_python_cexpr(out, LOOKUP_SUB(me, expr_positive_op_c));
      dprintf(out, ")");
      break;

    case expr_negative_op:
      dprintf(out, "-(");
      dump_python_cexpr(out, LOOKUP_SUB(me, expr_negative_op_c));
      dprintf(out, ")");
      break;

    case expr_mult_op:
    case expr_div_op:
    case expr_add_op:
    case expr_sub_op:
      dprintf(out, "(");
      dump_python_cexpr(out, LOOKUP_SUB(me, group_expression_g1));
      dprintf(out, "%s", t_expr[me->form].name);
      dump_python_cexpr(out, LOOKUP_SUB(me, group_expression_g2));
      dprintf(out, ")");
      break;

    default:
      fprintf( stderr, 
	       "Error: constant expression used forbidden operator %s\n", 
	       t_expr[me->form].name );
      break;
  }
}

void dump_python_rexpr(OUTPUT *out, p_obj *me, p_obj *spec) {
  p_type *type;
  
  switch (me->form) {
    case expr_variable: {
      p_decl *decl;
	
      decl = LOOKUP_SUB(me, expr_variable_decl);
      dprintf(out, "ex = ModBEExprs.Variable()\n");
      if (decl->form == decl_procedure || decl->form == decl_procedure_nr) {
	dprintf(out, "ex.name = '%s_%s_op'\n", 
		LOOKUP_STRING(decl, (decl->form == decl_procedure? \
				     decl_procedure_intf : \
				     decl_procedure_nr_intf)), 
		LOOKUP_ID(decl, group_decl_name));
      } else {
	dprintf(out, "ex.name = '%s'\n", 
		LOOKUP_ID(LOOKUP_SUB(me, expr_variable_decl), 
			  group_decl_name));     
      }
      dprintf(out, "ex.form = '%s'\n", 
	      t_decl[LOOKUP_SUB(me, expr_variable_decl)->form].name);
    }
    break;

    case expr_num_val:
      dprintf(out, "ex = ModBEExprs.Literal()\n");
      dprintf(out, "ex.val = '%s'\n", LOOKUP_STRING(me, expr_num_val_val));
      break;

    case expr_bool_val:
      dprintf(out, "ex = ModBEExprs.Literal()\n");
      dprintf(out, "ex.val = '%d'\n", LOOKUP_STRING(me, expr_num_val_val));
      break;

    case expr_real_val:
      dprintf(out, "ex = ModBEExprs.Literal()\n");
      dprintf(out, "ex.val = '%s'\n", LOOKUP_STRING(me, expr_num_val_val));
      break;

    case expr_char_val:
      dprintf(out, "ex = ModBEExprs.Literal()\n");
      dprintf(out, "ex.val = \"\"\"\\\'%s\\\'\"\"\"\n", 
	      LOOKUP_STRING(me, expr_char_val_val));
      break;

    case expr_str_val: {
      dprintf(out, "ex = ModBEExprs.Literal()\n");
      dprintf(out, "ex.val = \"\"\"\\\"%s\\\"\"\"\"\n", 
	      LOOKUP_STRING(me, expr_str_val_val)); 
    }
    break;

    case expr_subexpr:
      dump_python_r(out, LOOKUP_SUB(me, expr_subexpr_sub), spec, 0);
      break;

    case expr_tuple: {
      p_list *lptr;
      int mnum;
    
      mnum = expr_count++;
      dprintf(out, "ex = ModBEExprs.Tuple()\n");
      dprintf(out, "ex.content = []\n");
      dprintf(out, "ex%d = ex\n", mnum);
    
      for ( lptr = LOOKUP_SUB(me, expr_tuple_tuple); 
	    lptr; 
	    lptr = LOOKUP_SUB(lptr, group_list_next)) {
	p_obj *expr;
      
	expr = LOOKUP_SUB(lptr, list_expression_expr);
	dump_python_r(out, expr, spec, 0);
	dprintf(out, "ex%d.content.append( ex )\n", mnum);
      }
      dprintf(out, "ex = ex%d\n", mnum);
    }
    break;

    case expr_arrow_op:
    case expr_dot_op:
      dump_python_r(out, LOOKUP_SUB(me, expr_dot_op_sub), spec, 0);
      dprintf(out, "exl = ex\n");
      dprintf(out, "ex = ModBEExprs.UnOp()\n");
      dprintf(out, "ex.l = exl\n");
      dprintf(out, "ex.pre = ''\n");
      dprintf(out, "ex.deep = 0\n");
      dprintf(out, "ex.post = '%s%s'\n", t_expr[me->form].name, 
	      LOOKUP_ID(me, expr_dot_op_field));
      break;

    case expr_invoke_op: {
      p_list *lptr;
      int mnum;
    
      mnum = expr_count++;
      dump_python_r(out, LOOKUP_SUB(me, expr_invoke_op_sub), spec, 0);
      dprintf(out, "exl = ex\n");
      dprintf(out, "ex = ModBEExprs.Invoke()\n");
      dprintf(out, "ex.l = exl\n");
      dprintf(out, "ex.args = []\n");
      dprintf(out, "ex%d = ex\n", mnum);
    
      for ( lptr = LOOKUP_SUB(me, expr_invoke_op_list); 
	    lptr; 
	    lptr = LOOKUP_SUB(lptr, group_list_next)) {
	dump_python_r(out, LOOKUP_SUB(lptr, list_expression_expr), spec, 0);
	dprintf(out, "ex%d.args.append ( ex )\n", mnum);
      }
      dprintf(out, "ex = ex%d\n", mnum);
    }
    break;
  
    case expr_address_op:
    case expr_deref_op: 
    case expr_positive_op:
    case expr_negative_op:
      dump_python_r(out, LOOKUP_SUB(me, group_expression_g1), spec, 0);
      dprintf(out, "exl = ex\n");
      dprintf(out, "ex = ModBEExprs.UnOp()\n");
      dprintf(out, "ex.l = exl\n");
      dprintf(out, "ex.deep = 1\n");
      dprintf(out, "ex.pre = '%s'\n", t_expr[me->form].name);
      dprintf(out, "ex.post = ''\n");
      break;

    case expr_mult_op:
    case expr_div_op:
    case expr_add_op:
    case expr_sub_op:
    case expr_lt_op:
    case expr_gt_op:
    case expr_le_op:
    case expr_ge_op:
    case expr_eq_op:
    case expr_ne_op:
    case expr_ass_op:
    case expr_array_lookup: {
      int lnum, rnum;
    
      lnum = expr_count++;
      rnum = expr_count++;
    
      dump_python_r(out, LOOKUP_SUB(me, group_expression_g1), spec, 0);
      dprintf(out, "ex%d = ex\n", lnum);
      dump_python_r(out, LOOKUP_SUB(me, group_expression_g2), spec, 0);
      dprintf(out, "ex%d = ex\n", rnum);
      dprintf(out, "ex = ModBEExprs.%s()\n", 
	      me->form == expr_ass_op ? "Assign" : "BinOp");
      dprintf(out, "ex.l = ex%d\n", lnum);
      dprintf(out, "ex.r = ex%d\n", rnum);
      if (me->form == expr_array_lookup) {
	dprintf(out, "ex.op = '['\n");
	dprintf(out, "ex.post = ']'\n");
      } else {
	dprintf(out, "ex.op = '%s'\n", t_expr[me->form].name);
	dprintf(out, "ex.post = ''\n");
      }      
    }
    break;

    case expr_pvs:
      dprintf(out, "ex = ModBEEXprs.Pvs()\n");
      break;

    default:
      dprintf(out, "ex = ModBEExprs.Unimplemented()\n");
      dprintf(out, "ex.form = '%s'\n", t_expr[me->form].name);
      dprintf(out, "ex.formindex = %d\n", me->form);
      break;
  } /* End switch */
  
  type = LOOKUP_SUB(me, group_expression_type);
  if ( type ) {
    OUTPUT *tbuf;

    dump_python_tupstruct(out, me);
    if (LOOKUP_SUB(me, group_expression_ltype)) {
      dprintf(out, "ex.lvalue = 1\n");
      tbuf = dump_makebuffer(32);
      if (verbosity & 128) {
	dprintf(messages, "Expression ltype is:\n");
	dump_tree(messages, LOOKUP_SUB(me, group_expression_ltype));
	dprintf(messages, "\n");
      }

      rep_type(tbuf, LOOKUP_SUB(me, group_expression_ltype));
      if (verbosity & 128) {
	dprintf(messages, "Expression type name is: %s\n", tbuf->ch[0].cptr);
      }
      dprintf(out, "ex.ltype = '%s'\n", tbuf->ch[0].cptr);
      free_output(tbuf);
      dprintf(out, "ex.over = ['']\n");
    } else {
      dprintf(out, "ex.lvalue = 0\n"); 
      dprintf(out, "ex.over = ex.tup\n");
    }
   
    tbuf = dump_makebuffer(32);
    rep_type(tbuf, type);
    dprintf(out, "ex.typename = '%s'\n", tbuf->ch[0].cptr);
    hash_string(tbuf->ch[0].cptr, 217);
    free_output(tbuf);
  }
}

void dump_python_tupstruct_r(OUTPUT *out, p_obj *me, char *prefix) {
  /* "me" is a type */
  if (!me) 
    return;
  
  switch (me->form) {
    case type_record: {
      p_list *lptr;
      char *base;

      for (base = prefix; *base; base++)
	;
      for ( lptr = LOOKUP_SUB(me, type_record_list); 
	    lptr; 
	    lptr = LOOKUP_SUB(lptr, group_list_next)) {
	p_decl *decl;

	if (lptr->form == list_decl) {
	  decl = LOOKUP_SUB(lptr, list_decl_decl);
	  sprintf(base, ".%s", LOOKUP_ID(decl, group_decl_name));
	  dump_python_tupstruct_r( out, 
				   LOOKUP_SUB(decl, group_decl_type), 
				   prefix );
	}
      }
    }
    break;

    case type_tuple: {
      p_list *lptr;
      int count;
      char *base;
    
      for (base=prefix; *base; base++)
	;
      count = 0;
      for ( lptr = LOOKUP_SUB(me, type_tuple_list); 
	    lptr; 
	    lptr = LOOKUP_SUB(lptr, group_list_next)) {
	sprintf(base, ".f%d", count);
 	dump_python_tupstruct_r(out, LOOKUP_SUB(lptr, list_type_type), prefix);
	count++;
      }
    }
    break;

    default:
      dprintf(out, "ex.tup.append( '%s' )\n", prefix);
  } /* End switch */
}

void dump_python_tupstruct(OUTPUT *out, p_obj *me) {
  p_type *type;
  char buf[1024];

  buf[0] = 0;
  type = LOOKUP_SUB(me, group_expression_type);
  dprintf(out, "ex.tup = []\n");
  dump_python_tupstruct_r(out, type, buf);
}
