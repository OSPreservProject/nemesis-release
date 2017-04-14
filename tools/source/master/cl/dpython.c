/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless 
 *
 * dpython.c; main Python dump routines 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: dpython.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: dpython.c,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:09:28  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/07 20:48:03  rmm1002
 * Fixed output of unmatched IF stmts; added LOOP and EXIT dump clauses
 *
 * Revision 2.1  1997/04/04 15:35:01  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.16  1997/04/04 15:29:27  rmm1002
 * Added empty statement (';')
 *
 * Revision 1.15  1997/04/02 20:21:48  rmm1002
 * Fixed code generation! Now works for Compound(), While(), If(), Wibble(),
 * C_Block(), Expr() (only simple expressions), and added Repeat().
 *
 * Revision 1.14  1997/04/02 14:09:01  rmm1002
 * Trying to add REPEAT-UNTIL
 *
 * Revision 1.13  1997/03/21 12:22:23  rmm1002
 * Removed output of '[' and ']' around block statments (commented prev.)
 *
 * Revision 1.12  1997/03/19 21:55:33  rmm1002
 * Added support for inline C; now works
 *
 * Revision 1.11  1997/02/20 00:28:14  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:48:40  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 21:22:09  rmm1002
 * Finished fixup of layout#
 *
 * Revision 1.1  1997/01/30 21:41:36  rmm1002
 * Initial revision
 *
 */

#include <time.h>
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

#define outputCode(x)					\
    if ( opCode ) {					\
      dprintf( out, "b.code.append( " #x " )\n\n" );	\
    }

int anon_count	= 0;
int op_count	= 0;
int expr_count	= 0;
int stat_count	= 0;

void dump_python_r(OUTPUT *out, p_obj *me, p_obj *spec, int opCode) {
  /* Recursively traverse syntax tree, dumping Python for nodes as it goes */
  if (!me) 
    return;

  switch (me->cl) {
    case group_list: {
      p_list *lptr;

      lptr = me;
      while (lptr) {
	dump_python_r(out, LOOKUP_SUB(lptr, group_list_g1), spec, opCode);
	lptr = LOOKUP_SUB(lptr, group_list_next);
      }
    }
    break;

    case group_type:
	if (LOOKUP_STRING(me, group_type_oname)) 
	dprintf(out, "t = %s\n", LOOKUP_STRING(me, group_type_oname));
      dump_python_rtype(out, me, spec);
      break;

    case group_spec: 
      dump_python_rspec(out, me, spec);
      break;

    case group_decl: 
      dump_python_rdecl(out, me, spec);
      break;

    case group_exch: {
      /* dump_python_r(out, LOOKUP_SUB(me, group_exch_sco), specname);
       * dump_python_r(out, LOOKUP_SUB(me, group_exch_handles), specname); 
       */
    }
    break;

    case group_statement: {
      switch (me->form) {
	case statement_wibble: {
	  dprintf(out, "s = ModBEStatements.Wibble()\n");
	  outputCode( s );
	}
	break;

	case statement_empty: {
	  dprintf(out, "s = ModBEStatements.Empty()\n");
	  outputCode( s );
	}
	break;

	case statement_c_block: {
	  int slen = 0;
	  char *nc, *ncix, *oc;

	  dprintf(out, "s = ModBEStatements.C_Block()\n");
	  /* HACK: need to fix up c_code string to double up '\' (to stop
	   * Python interpreting them). Result is in "nc".
	   */
	  oc = LOOKUP_STRING(me, statement_c_block_code);
	  while (*oc++)
	    if (*oc == '\\')
	      slen++;
	  oc = LOOKUP_STRING(me, statement_c_block_code);
	  slen += strlen(oc);
	  nc = (char*)malloc( sizeof(char)*(slen+1) );
	  ncix = nc;

	  while ((*ncix++ = *oc++))
	    if (*oc == '\\')
	      *ncix++ = '\\';

	  dprintf(out, "s.code = \"\"\"%s\n\"\"\"\n", nc);
	  outputCode( s );
	}
	break;
	  
	case statement_block: { 
	  /* Compound() Class 
	   */
	  p_list *lptr;
	    
	  dprintf(out, "ob = b\n");
	  dprintf(out, "b = ModBEBlocks.Block()\n");
	  dprintf(out, "b.level = ob.level + 1\n");
	  dprintf(out, "b.code = []\n");
	  dprintf(out, "b.types = []\n");
	  dprintf(out, "b.vars = []\n");
	  dprintf(out, "b.haveparent = 1\n");
	  dprintf(out, "b.parent = ob\n");
	  dprintf(out, "b.imp = i\n");

	  for ( lptr = LOOKUP_SUB(me, statement_block_code); 
		lptr; 
		lptr = LOOKUP_SUB(lptr, group_list_next)) {
	    /* Always have opCode==0 below, since wish to add all statements
	     * to the special "b" block for the Compound() statement, and then
	     * output code for the Compound() statement.
	     */
	    dump_python_r(out, LOOKUP_SUB(lptr, group_list_g1), spec, 0);
	    if (LOOKUP_SUB(lptr, group_list_g1)->cl == group_statement) {
	      dprintf(out, "b.code.append( s )\n"); 
	    }
	  }
	  dprintf(out, "s = ModBEStatements.Compound()\n");
	  dprintf(out, "s.b = b\n");    
	  dprintf(out, "b = b.parent\n\n");
	}
	break;

	case statement_expr:
	  dprintf(out, "s = ModBEStatements.Expr()\n");
	  dump_python_r( out, LOOKUP_SUB(me, statement_expr_expr), 
			 spec, opCode );
	  dprintf(out, "s.ex = ex\n");
	  outputCode( s );
	  break;

	case statement_if_matched: {
	  int strue;
	  int sfalse;

	  strue = stat_count++;
	  sfalse = stat_count++;
	  dump_python_r(out, LOOKUP_SUB(me, statement_if_matched_truecode),
			spec, 0);
	  dprintf(out, "s%d = s\n", strue);
	  dump_python_r(out,LOOKUP_SUB(me, statement_if_matched_falsecode),
			spec, 0);
	  dprintf(out, "s%d = s\n", sfalse);
	  dprintf(out, "s = ModBEStatements.If()\n");
	  dprintf(out, "s.matched = 1\n");
	  dump_python_r(out, LOOKUP_SUB(me, statement_if_matched_test), 
			spec, 0);
	  dprintf(out, "s.test = ex\n");
	  dprintf(out, "s.truecode = s%d\n", strue);
	  dprintf(out, "s.falsecode = s%d\n", sfalse);
	  outputCode( s );
	}
	break;

	case statement_if_unmatched: {
	  int strue;

	  strue = stat_count++;
	  dump_python_r(out, LOOKUP_SUB(me, statement_if_unmatched_truecode), 
			spec, 0);
	  dprintf(out, "s%d = s\n", strue);
	  dprintf(out, "s = ModBEStatements.If()\n");
	  dprintf(out, "s.matched = 0\n");
	  dump_python_r(out, LOOKUP_SUB(me, statement_if_unmatched_test), 
			spec, 0);
	  dprintf(out, "s.test = ex\n");
	  dprintf(out, "s.truecode = s%d\n", strue);
	  outputCode( s );
	}
	break;

	case statement_repeat: {
	  int sbody;

	  sbody = stat_count++;
	  dump_python_r(out, LOOKUP_SUB(me, statement_repeat_code), 
			spec, 0);
	  dprintf(out, "s%d = s\n", sbody);
	  dump_python_r(out, LOOKUP_SUB(me, statement_repeat_test), 
			spec, 0);
	  dprintf(out, "s = ModBEStatements.Repeat()\n");
	  dprintf(out, "s.test = ex\n");
	  dprintf(out, "s.body = s%d\n", sbody);
	  outputCode( s );
	}
	break;

	case statement_while: {
	  int sbody;

	  sbody = stat_count++;
	  dump_python_r(out, LOOKUP_SUB(me, statement_while_code), 
			spec, 0);
	  dprintf(out, "s%d = s\n", sbody);
	  dump_python_r(out, LOOKUP_SUB(me, statement_while_test), 
			spec, 0);
	  dprintf(out, "s = ModBEStatements.While()\n");
	  dprintf(out, "s.test = ex\n");
	  dprintf(out, "s.body = s%d\n", sbody);
	  outputCode( s );
	}
	break;

	case statement_loop: {
	  int sbody;

	  sbody = stat_count++;
	  dump_python_r(out, LOOKUP_SUB(me, statement_loop_code),
			spec, 0);
	  dprintf(out, "s%d = s\n", sbody);
	  dprintf(out, "s = ModBEStatements.Loop()\n");
	  dprintf(out, "s.body = s%d\n", sbody);
	  outputCode( s );
	}
	break;

	case statement_exit: {
	  dprintf(out, "s = ModBEStatements.Exit()\n");
	  outputCode( s );
	}
	break;
      }
    }
    break;

    case group_sco: {
      /*    dump_python_r(out, LOOKUP_SUB(me, group_sco_scope), specname);
       *    dump_python_r(out, LOOKUP_SUB(me, group_sco_name), specname); 
       */
    }
    break;

    case group_proc: {
      dump_python_r(out, LOOKUP_SUB(me, group_proc_decl), spec, opCode);
      dprintf(out, "b = ModBEBlocks.Block()\n");
      dprintf(out, "b.level = 1\n");
      dump_python_r(out, LOOKUP_SUB(me, group_proc_decl), spec, opCode);
      dprintf(out, "b.header = o\n");
      dprintf(out, "b.code = []\n");
      dprintf(out, "b.types = []\n");
      dprintf(out, "b.vars = []\n");
      dprintf(out, "b.haveparent = 1\n");
      dprintf(out, "b.parent = i.topblock\n");
      dprintf(out, "b.imp = i\n");
      dump_python_r(out, LOOKUP_SUB(me, group_proc_statement), spec, opCode);
      dprintf(out, "i.topblock.code.append( b )\n\n");
    }
    break;

    case group_identifier:
      /* dprintf(out, " \"%s\" ", LOOKUP_STRING(me, group_identifier_string));
       */
      break;

    case group_expression:
      dump_python_rexpr(out, me, spec);
      break;
  }
}
