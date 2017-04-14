/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * parsupport.c; support routines for grammar (creating nodes, etc)
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997
 *
 * $Id: parsupport.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: parsupport.c,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:10:19  dr10009
 * Initial revision
 *
 * Revision 2.3  1997/04/12 13:27:19  rmm1002
 * Removed make_predeftype calls.
 *
 * Revision 2.2  1997/04/09 14:46:26  rmm1002
 * Layout stuff
 *
 * Revision 2.1  1997/04/04 15:35:35  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.12  1997/02/26 00:11:39  rmm1002
 * Changed ref. to input_file to ref. to input_open
 *
 * Revision 1.11  1997/02/20 00:28:28  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:45:57  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:47:07  rmm1002
 * Initial revision
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "cluless.h"
#include "stackobj.h"
#include "tables.h"
#include "expr.h"
#include "list.h"
#include "statement.h"
#include "parsupport.h"
#include "type.h"
#include "spec.h"
#include "decl.h"
#include "dtree.h"
#include "dpython.h"
#include "group.h"
#include "symtab.h"

p_obj *make_obj_f(int cl, int form, ...) {
  p_obj *ptr;
  va_list ap;
  int t;

  if (verbosity & VERBOSE_GEN) {
    fprintf(stderr, "[");
    fprintf(stderr, "%s ", t_group[cl].name);
    if (t_group[cl].x0) 
      fprintf(stderr, "%s ", t_group[cl].x0[form].name);
  }

  ptr = NEW(p_obj);
  ptr->cl = cl;
  ptr->form = form;
  if (verbosity & VERBOSE_GEN) 
    fprintf(stderr,"="); 

  for (t=0; t < 5; t++) 
    ptr->ch[t].sub = 0;
  va_start(ap, form);
  set_obj_f(ptr, ap);
  va_end(ap);
  
  if (verbosity & VERBOSE_GEN)
    fprintf(stderr, "]\n");

  ptr->oname = 0;
  ptr->occurence = 0;
  ptr->up = 0;
  return ptr;
}

void set_obj(p_obj *ptr, int param, void *val) {
  int t;
  int cl, form;

  cl = ptr->cl;
  form = ptr->form;
  if (verbosity & VERBOSE_GEN) 
    fprintf(stderr, "((%d", param);

  t = t_group[cl].nature[param];
  if (t == n_generic) {
    if (verbosity & VERBOSE_GEN)
      fprintf(stderr, " GEN");
    if ( t_group[cl].x0 ) {
      t = (t_group[cl].x0)[form].nature[param];
    }
  }
  if (verbosity & VERBOSE_GEN) 
    fprintf(stderr, ", %d) ", t);

  switch (t) {
    case n_empty:
      break;

    case n_generic:
      if (verbosity & VERBOSE_GEN)
	fprintf(stderr, "Warning: unresolved generic in constructor\n");
      break;

    case n_cptr:
      ptr->ch[param].string = val;
      if (verbosity & VERBOSE_GEN)
	dprintf(messages, "%s", ptr->ch[param].string);
      break;

    case n_real:
      ptr->ch[param].real = *((double *) val);
      if (verbosity & VERBOSE_GEN)
	dprintf(messages, "%f", ptr->ch[param].real);
      break;

    case n_integer:
      ptr->ch[param].integer = *((int *) val);
      if (verbosity & VERBOSE_GEN)
	dprintf(messages, "%d", ptr->ch[param].integer);
      break;

    default:
      ptr->ch[param].sub = val;
      if (verbosity & VERBOSE_GEN)
	dump_tree(messages, ptr->ch[param].sub);
      break;
  } /* End switch */

  if (verbosity & VERBOSE_GEN)
    fprintf(stderr, ")");
  return;
}

void set_obj_f(p_obj *ptr, va_list ap) {
  int param;
  int t;
  int cl, form;

  cl = ptr->cl;
  form = ptr->form;
  while ((param = va_arg(ap, int)) != OBJE && param >= 0 && param < 5) {
    if (verbosity & VERBOSE_GEN) 
      fprintf(stderr, "((%d", param);

    t = t_group[cl].nature[param];
    if (t == n_generic) {
      if (verbosity & VERBOSE_GEN)
	fprintf(stderr, " GEN");
      if ( t_group[cl].x0 ) {
	t = (t_group[cl].x0)[form].nature[param];
      }
    }

    if (verbosity & VERBOSE_GEN) 
      fprintf(stderr, ",%d) ", t);

    switch (t) {
      case n_empty:
	break;

      case n_generic:
	if (verbosity & VERBOSE_GEN)
	  fprintf(stderr, "Warning: unresolved generic in constructor\n");
	break;

      case n_cptr:
	ptr->ch[param].string = va_arg(ap, char *);
	if (verbosity & VERBOSE_GEN)
	  dprintf(messages, "%s", ptr->ch[param].string);
	break;

      case n_real:
	ptr->ch[param].real = va_arg(ap, double);
	if (verbosity & VERBOSE_GEN)
	  dprintf(messages, "%f", ptr->ch[param].real);
	break;

      case n_integer:
	ptr->ch[param].integer = va_arg(ap, int);
	if (verbosity & VERBOSE_GEN)
	  dprintf(messages, "%d", ptr->ch[param].integer);
	break;

      default:
	ptr->ch[param].sub = va_arg(ap, p_obj *);
	if (verbosity & VERBOSE_GEN)
	  dump_tree(messages, ptr->ch[param].sub);
	break;
    } /* End switch */
    if (verbosity & VERBOSE_GEN)
      fprintf(stderr, ")");
  }
            
  return;
}

p_expression *make_unaryop(int op, p_expression *x) {
  return make_obj_f( group_expression, op, 
		     group_expression_g1, x, 
		     OBJE );
}

p_expression *make_boolop(int op, p_expression *l, p_expression *r) {
  p_type *type_obj;

  type_obj = make_obj_f( group_type, type_boolean, 
			 group_type_oname, Predefined_Names[type_boolean],
			 OBJE );

  return make_obj_f( group_expression, op, 
		     group_expression_g1, l, 
		     group_expression_g2, r, 
		     group_expression_type, type_obj, 
		     OBJE );
}
 
p_expression *make_bracket(p_list *sub) {
  p_expression *r;

  /* Either simple bracketting, or a tuple */
  if (LOOKUP_SUB(sub, group_list_next) == 0) {
    /* Simple form... */
    if (sub->form != list_expression) {
      yyerror("Attempted to construct expression from a not-expression");
    } else {
      /* snip list */
      r = LOOKUP_SUB(sub, list_expression_expr);
      free(sub);
    }
  } else {
    /* Tuple... */
    r = make_obj_f( group_expression, expr_tuple, 
		    expr_tuple_tuple, sub, 
		    group_expression_type, make_tupletype(sub), 
		    OBJE );
  }

  return r;
}  
    
/* Build the node element for a binary operator in the expression tree */
p_expression *make_binop( int op, p_expression *l, 
			  p_expression *r, 
			  int leftpromo ) {

  return make_obj_f( group_expression, op,
		     group_expression_g1, l,
		     group_expression_g2, r,
		     group_expression_type, 
		     make_promotype( LOOKUP_SUB(l,group_expression_type),
				     LOOKUP_SUB(r,group_expression_type),
				     leftpromo ),
		     OBJE );
}

/* Attach y to the end of list x; concatenate the two lists */
void attach_list(p_list *x, p_list *y) {
  p_list *ptr;

  ptr = x;
  while (ptr && LOOKUP_SUB(ptr, group_list_next))
    ptr=LOOKUP_SUB(ptr, group_list_next);

  if (ptr) {
    LOOKUP_SUB(ptr, group_list_next) = y;
  }
}

/* Convert a list of identifiers to a list of fields, by attaching
 * type t to each identifier 
 */
void augment_identlist(p_list *l, p_type *t) {
  p_list *ptr, *res, *next;
  p_identifier *id;
  p_decl *decl;

  /* Reform the list, from identifiers to fields */
  ptr = l;
  res = 0;
  while (ptr) {
    if (ptr->form == list_id) {
      id = LOOKUP_SUB(ptr, list_id_id);
      ptr->form = list_decl;
      next = LOOKUP_SUB(ptr, group_list_next);
      
      decl = make_obj_f( group_decl, decl_var, 
			 group_decl_name, id, 
			 group_decl_type, t, 
			 OBJE );
      ptr -> form = list_decl;
      ptr -> ch[0].sub = decl;

      /* ptr = make_obj_f( group_list, list_decl,
       *		   group_list_next, next,
       *	           list_decl_decl, decl,
       *	           OBJE );
       */
      if (!res) 
	res = ptr;
      /* Warning: this cross links all the identifiers in this list to
       * the same type; possibly dangerous when freeing?  
       */
    } else {
      yyerror("Attempt to form augment a non-identifier");
    }  
    ptr = next;
  }
}

void setmode_list(p_list *x, int form) {
  p_list *ptr;

  ptr = x;

  while (ptr) {
    if (ptr->form == list_decl) {
      LOOKUP_SUB(ptr, list_decl_decl)->form = form;
    } else {
      yyerror("Attempted to set mode of something not a declaration");
    } 
    ptr = LOOKUP_SUB(ptr, group_list_next);
  }  
}

p_decl *lookup_decl(p_list *l, p_identifier *id) {
  p_list *lptr;

  lptr = l;
  while (lptr) {
    if (lptr->form != list_decl) {
      yyerror("Found something other than declaration in declaration lookup");
    } else {
      if ( !strcmp( LOOKUP_STRING(id, group_identifier_string), 
		    LOOKUP_STRING(LOOKUP_SUB(LOOKUP_SUB(lptr, list_decl_decl),
					     group_decl_name),
				  group_identifier_string))) {
	return LOOKUP_SUB(lptr, list_decl_decl);
      }
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
  }

  yyerror("Declaration not found");
  return 0;
}

/* Recurse down all the interfaces mentioned in "me" */
void follow_interfaces(p_obj *me, enum parser_mode newpm) {
  if (!me) 
    return;

  switch (me -> cl) {
    case group_list:
      /* XXX This follows interfaces twice if they are in both NEEDS
       * and EXTENDS?  NO -- just goes down the list, looking at first
       * the current object's interface and then at the next object in
       * the list... (recursively).  */
      follow_interfaces(LOOKUP_SUB(me, group_list_g1), newpm);
      follow_interfaces(LOOKUP_SUB(me, group_list_next), newpm);
      break;

    case group_identifier: {
      p_decl *decl;
      char filename[256];
      int gotfile;
      int i, s;
      enum parser_mode oldpm;

      decl = symtab_lookup_decl(me, (1<<decl_spec));
#if 0
      if ( decl && pm == pm_extends) {
	yyerror("Warning: Cyclic EXTENDS graph");
      }
#endif
      if (decl) 
	return; /* It's known, so no point in reading it again */
    
      if (pm == pm_needs && newpm == pm_extends) {
	/* While looking at an extends, we found a needs clause; treat
	 * as a needs clause */
	newpm = pm_needs;
      }
    
      /* Now we want to find interface file me */
      gotfile = 3;
      i = 0;    
      s = 0;
    
      while (gotfile && (s < suffixtablesize)) {
	strcpy(filename, LOOKUP_STRING(me, group_identifier_string));
	strcat(filename, suffixtable[s]);
	if (verbosity & VERBOSE_INCLUDE) {
	  fprintf(stderr, "Looking for file %s\n",filename);
	}
	gotfile = input_open(filename);
	s++;
      }

      if (i >= includetablesize) {
	fprintf( stderr, "Interface %s could not be located. Skipping.\n",
		 LOOKUP_STRING(me, group_identifier_string) );
	return;
      }

      if (verbosity & VERBOSE_INCLUDE) {
	fprintf(stderr, "Found file %s\n", filename);
      }

      oldpm = pm;
      pm = newpm;
      yyparse(); /* Parse new interface */
      pm = oldpm;
      input_close();
    }
    break;

    default:
      break;
  }
}
