/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * typesupport.c; type support functions
 *
 * Dickon Reed, 1996
 * Richard Mortier, 1997
 *
 * $Id: typesupport.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: typesupport.c,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:10:19  dr10009
 * Initial revision
 *
 * Revision 2.3  1997/04/12 13:15:03  rmm1002
 * Removed make_predeftype fn
 *
 * Revision 2.2  1997/04/09 14:45:36  rmm1002
 * Layout stuff
 *
 * Revision 2.1  1997/04/04 15:36:06  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.13  1997/04/02 20:29:57  rmm1002
 * Changed calls of dump_python_r to include new param (opCode)
 *
 * Revision 1.12  1997/02/20 00:29:10  rmm1002
 * No substantive changes
 *
 * Revision 1.11  1997/02/10 19:43:26  rmm1002
 * *** empty log message ***
 *
 * Revision 1.10  1997/02/04 16:46:51  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 18:34:01  rmm1002
 * Added ref to dpython.h (replacing dump.h)
 *
 * Revision 1.1  1997/01/30 21:52:27  rmm1002
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
#include "expr.h"
#include "group.h"
#include "parsupport.h"
#include "typesupport.h"
#include "dpython.h"

/* A special dump-like function, to compute strings representing the
 * type. These then form the keys by which types are indexed. */
#define TYPE_OPENBRACKET ""
#define TYPE_CLOSEBRACKET ""
#define TYPE_SEPARATE ""

void rep_type(OUTPUT *out, p_obj *me) {
  p_list *lptr;
  int i;

  if (!me) 
    return;

  switch (me->cl) {
    case group_identifier:
#ifdef TYPE_BRACKETIDS
      dprintf( out, "%s%s%s", TYPE_OPENBRACKET, 
	       LOOKUP_STRING(me, group_identifier_string), 
	       TYPE_CLOSEBRACKET );
#else
      dprintf(out, "%s", LOOKUP_STRING(me, group_identifier_string));
#endif
      break;

    case group_list:
      for (lptr = me; lptr; lptr = LOOKUP_SUB(lptr, group_list_next)) {
	dprintf(out, "%s", TYPE_OPENBRACKET);
	rep_type(out, LOOKUP_SUB(lptr, group_list_g1));
	dprintf(out, "%s", TYPE_CLOSEBRACKET);
      }
      break;

    case group_type:
      dprintf(out,"%s", t_type[me->form].name);
      /* Note that the 2 below is deliberate; we want to exclude the
       * only 3rd placed type attribute, the method decl hack so as to
       * avoid a loop. */
      for (i=0; i < 2; i++) {
	if (me->form == type_method) 
	  dprintf(out, "_");
	switch (t_type[me->form].nature[i]) {
	  case n_empty:
	    break;

	  case n_integer:
	    dprintf(out, "%d", me->ch[i].integer);
	    break;

	  default:
	    rep_type(out, LOOKUP_SUB(me, i));
	    break;
	}
      }
      break;

    case group_decl:
      rep_type(out, LOOKUP_SUB(me, group_decl_type));
      dprintf(out, "%s%s", TYPE_SEPARATE, LOOKUP_ID(me, group_decl_name));    
      break;

    default:
      dprintf(out, "q");
  }
}

int hash_string(char *str, int size) {
  int x;
  char *ptr;
  
  x = 0;
  ptr = str;
  while (*ptr) {
    x = ((x<<8) + *ptr) % size;
    ptr ++;
  }
  return x;
}

void btt_visit(OUTPUT *out, p_obj *type) {
  OUTPUT *tbuf;
  char *name;
  int mnum;
  extern OUTPUT *predecl;
 
  tbuf = dump_makebuffer(16);
  rep_type(tbuf, type);

  if (verbosity & 256) {
    dprintf(messages, "String %s from: ", tbuf->ch[0].cptr);
    dump_tree(messages, type);
    dprintf(messages, "\n");
  }
  if (!LOOKUP_STRING(type, group_type_oname)) {
    char buf[128];

    mnum = anon_count++;
    sprintf(buf, "%s_type%d", basefilename, mnum);
    LOOKUP_STRING(type, group_type_oname) = strdup(buf);
    dprintf(predecl, "%s = ModBETypes.Type()\n", buf);
    dprintf(predecl, "%s.localName = '%s'\n", buf, buf);
    dprintf(out, "%s.intf = i \n", buf);
    dprintf(predecl, "%s.name = '%s'\n", buf, buf+5);
    dprintf(out, "i.types.append( %s )\n", buf);
    dump_python_r(predecl, type, 0, 0);
  }
  
  name = LOOKUP_STRING(type, group_type_oname);
  if (name)  
    dprintf(out, "tt.append( ('%s',%s) )\n", tbuf->ch[0].cptr, name);
}

void btt_r(OUTPUT *out, p_obj *me, p_obj *spec) {
  int i, n;
    
  if (!me) 
    return;
  
  /* Specific annotations here */
  if (me->cl == group_type && me->form == type_method) {
    LOOKUP_STRING(me, type_method_intf) = \
      strdup(LOOKUP_ID(spec, group_spec_name));
  }
  
  for (i = 0; i < t_group[me->cl].order; i++) {
    n = t_group[me->cl].nature[i];
    if (n == n_generic)
      n = t_group[me->cl].x0[me->form].nature[i];
    switch (n) {
      case n_empty:
      case n_cptr:
      case n_generic:
      case n_integer:
      case n_real:
	break;

      default:
	btt_r(out, me->ch[i].sub, spec);
    }
  }
  if (me->cl == group_type) {
    btt_visit(out, me);
  } 
}
   
void build_type_table(OUTPUT *out, p_obj *me) {
  dprintf(out, "tt = []\n");
  btt_r(out, me, me);
  dprintf(out, "i.typetable = tt\n");
  dprintf(out, "########################################\n");
}

int compare_types(p_type *t1, p_type *t2) {
  if (!t1 || !t2) {
    if (!t1 && !t2) 
      yyerror("Attempted type check of an untyped expression against an "
	      "untyped expression");
    if (!t1 && t2) 
      yyerror("Attempted type check of an untyped expression against a "
	      "typed expression\n Typed expression has type:");
    if (t1 && !t2) 
      yyerror("Attempted type check of a typed expression against an untyped "
	      "expression\n Typed expression has type:");
    if (t1) {
      dump_tree(messages, t1);
      dprintf(messages, "\n");
    }
    if (t2) {
      dump_tree(messages, t2);
      dprintf(messages, "\n");
    }
    if (t1) 
      return TYPES_PROMOTE_2;
    if (t2) 
      return TYPES_PROMOTE_1;

    return TYPES_IDENTICAL;
  }
  switch (t1->form) {
    /* Simple types; these are only equal to themselves, and have no
     * children to worry about */
    case type_boolean:
    case type_string:
    case type_char:
    case type_dangerous_word:
    case type_dangerous_address:
      if (t1->form == t2->form) 
	return TYPES_IDENTICAL;
      break;

      /* promotable simple types */
    case type_short_cardinal:
    case type_cardinal:
    case type_long_cardinal:
    case type_short_integer:
    case type_integer:
    case type_long_integer:
    case type_real:
    case type_long_real:
      if (t1->form == t2->form) 
	return TYPES_IDENTICAL;
      if (t1->form > t2->form) 
	return TYPES_PROMOTE_2;
      if (t1->form < t2->form) 
	return TYPES_PROMOTE_1;
      break;

    case type_array:
      if (t1->form != t2->form) 
	return TYPES_DISTINCT;
      if (t1->ch[1].integer != t2->ch[1].integer) 
	return TYPES_DISTINCT;
      return compare_types( LOOKUP_SUB(t1, type_array_type), 
			    LOOKUP_SUB(t2, type_array_type) );
      break;

      /* compound simple types */
    case type_set:
    case type_sequence:
    case type_ref:
      if (t1->form != t2->form) 
	return TYPES_DISTINCT;
      return compare_types( LOOKUP_SUB(t1, group_type_g1), 
			    LOOKUP_SUB(t2, group_type_g1) );
      break;

      /* compound simple list types */
    case type_enum: 
    case type_choice: {
      int x;
      
      if (t1->form != t2->form) 
	return TYPES_DISTINCT;
      x = compare_lists( LOOKUP_SUB(t1, group_type_g1), 
			 LOOKUP_SUB(t2, group_type_g2) );
      if (!x) 
	return TYPES_IDENTICAL;
    }
    break;

    case type_record:
    case type_tuple: {
      /* This is a pain. What happens if some elements ought to be
       * promoted and some demoted in t1? So, we either say they are
       * identical or distinct 
       */
      p_list *lastup, *rastup;
      int x;
      
      if (t2->form != type_record && t2->form != type_tuple) 
	return TYPES_DISTINCT;
      lastup = t1->form == type_tuple ? LOOKUP_SUB(t1, type_tuple_list) : \
	make_tuplelist(LOOKUP_SUB(t1,type_record_list));
      rastup = t2->form == type_tuple ? LOOKUP_SUB(t2, type_tuple_list) : \
	make_tuplelist(LOOKUP_SUB(t2, type_record_list));
      x = compare_lists(lastup, rastup);
      if (!x) 
	return TYPES_IDENTICAL;
    }

    case type_iref: {
      if (t1->form != t2->form) 
	return TYPES_DISTINCT;
      if (!strcmp(LOOKUP_STRING(LOOKUP_SUB(t1, type_iref_id), 
				group_identifier_string),
		  LOOKUP_STRING(LOOKUP_SUB(t2, type_iref_id), 
				group_identifier_string)))
	return TYPES_IDENTICAL;
    }

    default:
      yyerror("Comparing unknown types");
      break;
  }
  return TYPES_DISTINCT;
}

p_list *make_tuplelist(p_list *list) {
  p_list *lptr, *tptr, **assprev;
  p_list *tuplelist;
  p_type *type;

  tuplelist = 0;
  assprev = &tuplelist;
  lptr = list;

  while (lptr) {
    switch (lptr->form) {
      case list_decl:
	type = LOOKUP_SUB(LOOKUP_SUB(lptr, list_decl_decl), group_decl_type);
	break;

      case list_expression:
	type = LOOKUP_SUB(LOOKUP_SUB(lptr, list_expression_expr), 
			  group_expression_type);
	break;

      default:
	yyerror("Something without a type found while attempting to form a type tuple");
    }

    tptr = make_obj_f( group_list, list_type, 
		       list_type_type, type, 
		       group_list_next, 0, 
		       OBJE );
    (*assprev) = tptr;
    assprev = &(LOOKUP_SUB(tptr, group_list_next));
    lptr = LOOKUP_SUB(lptr, group_list_next);
  }
  return tuplelist;
}

/* Make tuple from list of things with type information or pass
 * straight through a single type.  */
p_type *make_tupletype(p_list *list) {
  return make_obj_f( group_type, type_tuple, 
		     type_tuple_list, make_tuplelist( list ), 
		     OBJE );
}

/* Make possibly promoted type, from l OP r. l is superior to r;
 * recurses on tuples or records */
p_type *make_promotype(p_type *l, p_type *r, int leftpromo) {
  if (!l || !r) {
    yyerror("Found an untyped expression while attempting a promotion");
    return 0;
  }
  /* Tuples are complicated with type promotion */
  if ( (l->form == type_tuple || l->form == type_record) || 
       (r->form == type_record || r->form == type_tuple) ) {
    p_list *lptr, *rptr, **prev, *optr, *out;
    p_type *type;
    
    if (!l || !r) {
      yyerror("One part of an expression is untyped");
      return r;
    }
    
    lptr = (l->form == type_tuple) ? LOOKUP_SUB(l,type_tuple_list) : 
      make_tuplelist ( LOOKUP_SUB(l, type_record_list));
    rptr = (r->form == type_tuple) ? LOOKUP_SUB(r,type_tuple_list) : 
      make_tuplelist ( LOOKUP_SUB(r, type_record_list));

    out = 0;
    prev = &out;

    while (lptr && rptr) {
      optr = make_obj_f( group_list, list_type,
			 list_type_type, make_promotype(
					   LOOKUP_SUB(lptr, list_type_type),
					   LOOKUP_SUB(rptr, list_type_type), 
					   leftpromo ),
			 group_list_next, 0, 
			 OBJE );
      *prev = optr;
      prev = &(LOOKUP_SUB(optr, group_list_next));
      lptr = LOOKUP_SUB(lptr, group_list_next);
      rptr = LOOKUP_SUB(rptr, group_list_next);
    }

    if (lptr) { 
      yyerror("Attempt to compare tuples; dominant tuple was too long");
    }
    if (rptr) {
      yyerror("Attempt to compare tuples; recessive tuple was too long");
    }
    type = make_obj_f( group_type, type_tuple, 
		       type_tuple_list, out,
		       OBJE );
    return type;
  } else {
    switch (compare_types(l, r)) {
      case TYPES_IDENTICAL:
	return r;
	break;

      case TYPES_PROMOTE_1:
	if (leftpromo) {
	  return r;
	} else {
	  yyerror("Illegal left promotion required to type check this; ");
	  dprintf(messages, "Dominant type: "); 
	  dump_tree(messages, l);
	  dprintf(messages, "Recessive type: "); 
	  dump_tree(messages, r);
	}
	break;

      case TYPES_PROMOTE_2:
	return l;
	break;

      case TYPES_DISTINCT:
	yyerror("Types incompatible");
	dprintf(messages, "Occured when comparing: ");
	dump_tree(messages, l);
	dprintf(messages, " with: ");
	dump_tree(messages, r);
	dprintf(messages, "\n");
	break;
    }
    return 0;
  }
}

int compare_lists(p_list *l, p_list *r) {
  p_list *lptr, *rptr;
  int res;
  
  lptr = l; 
  rptr = r;
  while (lptr && rptr) {
    if (lptr->form != rptr->form) {
      yyerror("Attempt to compare two lists failed");
    }
    
    switch (lptr->form) {
      case list_id:
	if ((res = strcmp(LOOKUP_STRING( LOOKUP_SUB(lptr, list_id_id),
					group_identifier_string ),
			 LOOKUP_STRING( LOOKUP_SUB(rptr, list_id_id), 
					group_identifier_string) )))
	  return res;
	break;

      case list_type:
	res = compare_types( LOOKUP_SUB(lptr, list_type_type), 
			     LOOKUP_SUB(rptr, list_type_type) );
	if (res != TYPES_IDENTICAL) 
	  return 1;
	break;

      case list_decl: {
	p_decl *ld, *rd;

	if ( (ld = LOOKUP_SUB(lptr, list_decl_decl))->form != 
	     (rd = LOOKUP_SUB(rptr, list_decl_decl))->form) 
	  return 1;
	switch (ld->form) {
	  case decl_var:
	    res = compare_types( LOOKUP_SUB(ld, group_decl_type),
				 LOOKUP_SUB(rd, group_decl_type) );
	    if (res != TYPES_IDENTICAL) 
	      return 1;
	    if ((res = strcmp(LOOKUP_STRING( LOOKUP_SUB(ld, group_decl_name), 
					    group_identifier_string),
			     LOOKUP_STRING( LOOKUP_SUB(rd, group_decl_name), 
					    group_identifier_string) )))
	      return res;
	    break;
	}
	break;

      }
      default:
	yyerror("Don't know how to compare two list items found");
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
    rptr = LOOKUP_SUB(rptr, group_list_next);
  }

  /* If either list has anything left over, then they are distinct */
  if (lptr) 
    return -1;
  if (rptr) 
    return 1;
  return 0;
}


/* Make a reference of a type, with the twist that we descend into
 * records and other such types */
p_obj *make_deepref_type(p_obj *type) {
  p_obj *newtype;
  p_list *lptr, *nlptr, **prev;

  if(!type) 
    return NULL;
  switch (type->form) {
    case type_record:
      newtype = make_obj_f(group_type, type_record, OBJE);
      prev = &LOOKUP_SUB(newtype, type_record_list);
      for ( lptr = LOOKUP_SUB(type, type_record_list); 
	    lptr; 
	    lptr = LOOKUP_SUB(lptr, group_list_next) ) {
	p_decl *decl, *newdecl;
	if (lptr->form == list_decl) {
	  decl = LOOKUP_SUB(lptr, list_decl_decl);
	  newdecl = make_obj_f( group_decl, decl->form,
				group_decl_name, LOOKUP_SUB(decl, 
							    group_decl_name),
				group_decl_type, 
				make_deepref_type(LOOKUP_SUB(decl, 
							     group_decl_type)),
				OBJE );
	  nlptr = make_obj_f( group_list, list_decl,
			      list_decl_decl, newdecl,
			      group_list_next, 0,
			      OBJE );
	  *prev = nlptr;
	  prev = &(LOOKUP_SUB(nlptr, group_list_next));
	}
      }
      break;

    case type_tuple:
      newtype = make_obj_f( group_type, type_tuple, 
			    OBJE );
      prev = &(LOOKUP_SUB(newtype, type_tuple_list));
      for ( lptr = LOOKUP_SUB(type, type_tuple_list); 
	    lptr; 
	    lptr = LOOKUP_SUB(lptr, group_list_next) ) {   
	if (lptr->form == list_type) {
	  nlptr = make_obj_f( group_list, list_type,
			      list_type_type, 
			      make_deepref_type(LOOKUP_SUB(lptr, 
							   list_type_type)),
			      group_list_next, 0,
			      OBJE );
	  *prev = nlptr;
	  prev = &(LOOKUP_SUB(nlptr, group_list_next));
	}
      }
      break;

    case type_array:
      if (LOOKUP_SUB(type, type_array_type)->form == type_array) {
	newtype = make_obj_f( group_type, type_ref,
			      type_ref_type, 
			      make_deepref_type(LOOKUP_SUB(type, 
							   type_array_type)),
			      OBJE );
      } else {
	newtype = make_obj_f( group_type, type_ref,
			      type_ref_type, LOOKUP_SUB(type, type_array_type),
			      OBJE );
      }
      break;

    default:
      /* Deep enough; insert a REF here */
      newtype = make_obj_f( group_type, type_ref,
			    type_ref_type, type,
			    OBJE );
      break;
  }
  return newtype;
}
