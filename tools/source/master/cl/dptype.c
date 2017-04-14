/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * dptype.c; dump Python for type 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: dptype.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: dptype.c,v $
 * Revision 1.1  1997/06/25 17:09:28  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/12 13:22:06  rmm1002
 * Removed ADDRESS and WORD types; must be DANGEROUS
 *
 * Revision 2.1  1997/04/04 15:34:58  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.13  1997/04/02 20:29:21  rmm1002
 * Changed calls of dump_python_r to include new param (opCode)
 *
 * Revision 1.12  1997/02/20 00:28:11  rmm1002
 * No substantive changes
 *
 * Revision 1.11  1997/02/10 19:43:05  rmm1002
 * *** empty log message ***
 *
 * Revision 1.10  1997/02/04 16:45:27  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:41:26  rmm1002
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

char *Predefined_Names[ TYPES_BUILTIN ]  = {
  "type_MIDDL_Boolean",
  "type_MIDDL_ShortCardinal",
  "type_MIDDL_Cardinal",
  "type_MIDDL_LongCardinal",
  "type_MIDDL_ShortInteger",
  "type_MIDDL_Integer",
  "type_MIDDL_LongInteger",
  "type_MIDDL_Real",
  "type_MIDDL_LongReal",
  "type_MIDDL_String",
  "type_MIDDL_Octet",
  "type_MIDDL_Char",
  "type_MIDDL_Address",	/* Dangerous Address */
  "type_MIDDL_Word"	/* Dangerous Word */
};

void dump_python_rtype(OUTPUT *out, p_obj *me, p_obj *spec) {
  p_list *lptr;

  if (me->form < TYPES_BUILTIN) {
    dprintf(out, "t.clss = 'ALIAS'\n");
    dprintf(out, "alias = ModBETypes.Alias()\n");
    dprintf(out, "alias.base = %s\n", Predefined_Names[me->form]);
    dprintf(out, "t.type = alias\n");
  } else {
    switch (me->form) {
      case type_enum: {
	p_list *lptr;
      
	dprintf(out, "t.clss = 'ENUMERATION'\n");
	dprintf(out, "enum = ModBETypes.Enumeration()\n");
	dprintf(out, "enum.elems = []\n"); 
      
	lptr = LOOKUP_SUB(me, type_enum_list);
	while (lptr) {
	  dprintf( out, "enum.elems.append( '%s' )\n", 
		   LOOKUP_ID(lptr, list_id_id));
	  lptr = LOOKUP_SUB(lptr, group_list_next);
	}
	dprintf(out, "t.type = enum\n");
      }
      break;

      case type_array:
      case type_bitarray: {
	p_obj *size;

	dump_python_ensureavail(out, LOOKUP_SUB(me, type_array_type), spec);
	dprintf(out, "t.clss = '%s'\n", me->form == type_bitarray ? \
		"BITARRAY" : "ARRAY");
	dprintf(out, "arr = ModBETypes.%s()\n", me->form == type_bitarray ? \
		"Array" : "Array");

	if (me->form == type_array)
	  dprintf(out, "arr.base = %s\n", 
		  LOOKUP_STRING(LOOKUP_SUB(me, type_array_type), 
				group_type_oname));
	if (me->form == type_bitarray)
	  /* Hack: I don't know what type ARRAY OF n BITS ought to generate, 
	   * so generate ARRAY OF n BOOLEAN 
	   */
	  dprintf(out,"arr.base = type_MIDDL_Boolean\n");
	dprintf(out, "arr.size = "); 

	size = 0;
	if (me->form == type_array) 
	  size = LOOKUP_SUB(me, type_array_size);
	if (me->form == type_bitarray) 
	  size = LOOKUP_SUB(me, type_bitarray_size);
	dump_python_cexpr(out, size);
	dprintf(out, "\n");
	/* What to do about subtype here? */
	dprintf(out, "t.type = arr\n");
      }
      break;

      case type_set:
	dump_python_ensureavail(out, LOOKUP_SUB(me, type_set_type), spec);
	dprintf(out, "t.clss = 'SET'\n");
	dprintf(out, "set =ModBETypes.Set()\n");
	dprintf(out, "set.base = %s\n", 
		LOOKUP_STRING(LOOKUP_SUB(me, type_set_type), 
			      group_type_oname));
	dprintf(out, "t.type = set\n");
	break;

      case type_sequence:
	dump_python_ensureavail(out, LOOKUP_SUB(me, type_sequence_type), spec);
	dprintf(out, "t.clss = 'SEQUENCE'\n");
	dprintf(out, "seq = ModBETypes.Sequence()\n");
	dprintf(out, "seq.base = %s\n", 
		LOOKUP_STRING(LOOKUP_SUB(me, type_sequence_type), 
			      group_type_oname));
	dprintf(out, "t.type = seq\n");
	break;

      case type_record:
	for ( lptr = LOOKUP_SUB(me, type_record_list); 
	      lptr; 
	      lptr = LOOKUP_SUB(lptr, group_list_next) ) {
	  if (lptr->form == list_type) {
	    p_decl *decl;

	    decl = LOOKUP_SUB(lptr, list_decl_decl);
	    dump_python_ensureavail( out, LOOKUP_SUB(decl, group_decl_type), 
				     spec);
	  }
	}
	dprintf(out, "t.clss = 'RECORD'\n");
	dprintf(out, "record = ModBETypes.Record()\n");
	dprintf(out, "record.mems = []\n");

	for ( lptr = LOOKUP_SUB(me, type_record_list); 
	      lptr; 
	      lptr = LOOKUP_SUB(lptr, group_list_next)) {
	  if (lptr->form == list_decl) {
	    p_decl *decl;

	    decl = LOOKUP_SUB(lptr, list_decl_decl);
	    if (decl->form == decl_var) {
	      dprintf(out, "record_mem = ModBETypes.RecordMember()\n");
	      dprintf(out, "record_mem.name = '%s'\n", 
		      LOOKUP_ID(decl, group_decl_name));
	      dump_python_ensureavail( out, LOOKUP_SUB(decl, group_decl_type), 
				       spec );
	      dprintf( out, "record_mem.type = %s\n", 
		       LOOKUP_STRING( LOOKUP_SUB(decl, group_decl_type), \
				      group_type_oname));
	      dprintf(out, "record.mems.append( record_mem )\n");
	    }
	  }
	}
	dprintf(out, "t.type = record\n");
	break;	   
	
      case type_tuple: { 
	/* Hack: tuples are records with field names f0, f1... */ 
	int count;
      
	for ( lptr = LOOKUP_SUB(me, type_tuple_list), count=0; 
	      lptr; 
	      lptr=LOOKUP_SUB(lptr, group_list_next), count++ ) {
	  if (lptr->form == list_type) {
	    p_decl *type;

	    type = LOOKUP_SUB(lptr, list_type_type);
	    dump_python_ensureavail(out, type, spec);
	  }
	}
	dprintf(out, "t.clss = 'RECORD'\n");
	dprintf(out, "record = ModBETypes.Record()\n");
	dprintf(out, "record.mems = []\n");
	for ( lptr = LOOKUP_SUB(me, type_tuple_list), count=0; 
	      lptr; 
	      lptr=LOOKUP_SUB(lptr, group_list_next), count++ ) {
	  if (lptr->form == list_type) {
	    p_decl *type;
	  
	    type = LOOKUP_SUB(lptr, list_type_type);
	    dprintf(out, "record_mem = ModBETypes.RecordMember()\n");
	    dprintf(out, "record_mem.name = 'f%d'\n", count);
	    dprintf(out, "record_mem.type = %s\n", 
		    LOOKUP_STRING(type, group_type_oname));
	    dprintf(out, "record.mems.append( record_mem )\n");
	  }
	}
	dprintf(out, "t.type = record\n");
      }
      break;	   	

      case type_iref:
	dprintf(out, "t.clss = 'IREF'\n");
	dprintf(out, "iref = ModBETypes.InterfaceRef()\n");
	dprintf(out, "iref.base = %s_%s\n", INTF_SYMBOL, 
		LOOKUP_STRING( LOOKUP_SUB(me, type_iref_id),
			       group_identifier_string));
	dprintf(out,"t.type = iref\n");
	break;
      
      case type_choice:
	dump_python_ensureavail(out, LOOKUP_SUB(me, type_choice_type), spec);
	dprintf(out, "t.clss = 'CHOICE'\n");
	dprintf(out, "choice = ModBETypes.Choice()\n");
	dprintf(out, "choice.base = %s\n", 
		LOOKUP_STRING( LOOKUP_SUB(me, type_choice_type), 
			       group_type_oname));
	dprintf(out, "choice.elems = []\n");
	lptr = LOOKUP_SUB(me, type_choice_list);
	while (lptr) {
	  /* Choice fields are munged to var decls */
	  if (lptr->form == list_decl) {
	    p_decl *field;

	    field = LOOKUP_SUB(lptr, list_decl_decl);
	    if (field->form == decl_var) {
	      dprintf(out, "choice_elem = ModBETypes.ChoiceElement()\n");
	      dprintf(out, "choice_elem.name = '%s'\n", 
		      LOOKUP_ID(field, group_decl_name));
	      dprintf(out, "choice_elem.type = %s\n", 
		      LOOKUP_STRING(LOOKUP_SUB(field, group_decl_type), 
				    group_type_oname));
	      dprintf(out, "choice.elems.append( choice_elem )\n");
	    }
	  }
	  lptr = LOOKUP_SUB(lptr, group_list_next);
	}
	dprintf(out, "t.type = choice\n");
	break;

      case type_ref:
	dump_python_ensureavail(out, LOOKUP_SUB(me, type_ref_type), spec);

	dprintf(out, "t.clss = 'REF'\n");
	dprintf(out, "ref = ModBETypes.Ref()\n");
	dprintf(out, "ref.base = %s\n", 
		LOOKUP_STRING( LOOKUP_SUB(me, type_ref_type), 
			       group_type_oname));
	dprintf(out, "t.type = ref\n");
	break;

      case type_method: {
	char *intf;

	dprintf(out, "t.clss = 'METHOD'\n");
	dprintf(out, "method = ModBETypes.Method()\n");
	dump_python_arglist(out, LOOKUP_SUB(me, type_method_args), 
			    ARGLIST_PARAM, spec);
	dprintf(out, "method.pars = al\n");
	dump_python_arglist(out, LOOKUP_SUB(me, type_method_rets), 
			    ARGLIST_RESULT, spec);
	dprintf(out, "method.results = al\n");
	intf = LOOKUP_STRING(me, type_method_intf);
	if (intf) {
	  dprintf(out, "method.clo = '%s_cl'\n", intf);
	} else {
	  dprintf(out, "method.clo = 'void'\n", intf);
	}
	dprintf(out, "t.type = method\n");
      }
      break;

      default:
	dprintf(out, "t.clss = 'ALIAS'\n");
	dprintf(out, "alias = ModBETypes.Alias()\n");
	dprintf(out, "alias.base = %s\n", LOOKUP_STRING(me, group_type_oname));
	dprintf(out, "t.type = alias\n");
	break;
    } /* End switch */
  } /* End IF...ELSE... */
#if 0
  if (LOOKUP_STRING(me, group_type_oname) && me->form != type_iref)
    if (strncmp(LOOKUP_STRING(me, group_type_oname), "type_MIDDL", 10))
      dprintf(predecl, "%s = t\n", LOOKUP_STRING(me, group_type_oname));
#endif
}

void dump_python_ensureavail(OUTPUT *out, p_obj *me, p_obj *spec) {
  /* "me" is a type */
  if (!me) 
    return;
  if (me->cl != group_type) 
    return;
  
  /* return if already available */
  if (LOOKUP_STRING(me, group_type_oname)) 
    return;

  dump_python_r(out, me, spec, 0);


  if (LOOKUP_STRING(me, group_type_oname))
      return;

  if (!LOOKUP_STRING( me, 
		      group_type_oname)) {
      btt_visit(out, me);
  }

}
