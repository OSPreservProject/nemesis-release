/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless 
 *
 * dpdecl.c; functions for dumping Python code for declarations 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: dpdecl.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: dpdecl.c,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:09:28  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/12 13:16:15  rmm1002
 * Changed INTERFACE macro calls to IFACE (clash with definition of INTERFACE
 * keyword in lexer/grammar)
 *
 * Revision 2.1  1997/04/04 15:34:45  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.12  1997/04/02 20:28:28  rmm1002
 * Changed calls of dump_python_r to include new param (opCode)
 *
 * Revision 1.11  1997/02/20 00:27:52  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:44:39  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:40:53  rmm1002
 * Initial revision
 *
 */
#include <stdio.h>
#include <time.h>
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

void dump_python_arglist(OUTPUT *out, p_list *list, int kind, p_obj *spec) {
  p_list *lptr;

  dprintf(out, "al = []\n");
  lptr = list;
  while (lptr) {
    if (lptr->form == list_decl) {
      p_decl *decl;
      OUTPUT *tbuf;

      decl = LOOKUP_SUB(lptr, list_decl_decl);

#if 0
      dprintf(out, "# oname is %s\n", LOOKUP_STRING( LOOKUP_SUB(decl, group_decl_type), 
			      group_type_oname));
#endif
      if (!LOOKUP_STRING( LOOKUP_SUB(decl, group_decl_type), 
			  group_type_oname)) {
	  btt_visit(out, LOOKUP_SUB(decl, group_decl_type));
      }
      dump_python_ensureavail(out, LOOKUP_SUB(decl, group_decl_type), spec);
      dprintf( out, "arg = ModBEOps.%s()\n", 
	       kind == ARGLIST_RESULT? "Result" : "Parameter");
      dprintf( out, "arg.name = '%s'\n", 
	       LOOKUP_ID(decl, group_decl_name) );
      dprintf( out, "arg.type = %s\n", 
	       LOOKUP_STRING( LOOKUP_SUB(decl, group_decl_type), 
			      group_type_oname) );

      tbuf = dump_makebuffer(32);
      rep_type(tbuf, LOOKUP_SUB(decl, group_decl_type));
      dprintf(out, "arg.typename = '%s'\n", tbuf->ch[0].cptr);
      free_output(tbuf);
      dprintf(out, "arg.mode = '");

      switch (decl->form) {
	case decl_invar:
	  dprintf(out, "IN");
	  break;

	case decl_outvar:
	  dprintf(out, kind == ARGLIST_RESULT ? "RESULT" : "OUT");
	  break;

	case decl_inoutvar:
	  dprintf(out, "INOUT");
	  break;

	default:
	  dprintf(out, "UNKNOWN");
	  break;
      } /* End switch */
      dprintf(out, "'\n");

      dprintf( out, "arg.isLarge = %s\n", 
	       LOOKUP_SUB(decl, group_decl_type)->form \
	         <= type_method ? "0":"1"
	       );
      dprintf(out, "al.append( arg )\n");      
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
  }
}

void dump_python_rdecl(OUTPUT *out, p_obj *me, p_obj *spec) {
  p_type *type;
  
  type = LOOKUP_SUB(me, group_decl_type);
  
  switch (me->form) {
    case decl_type: {    
      if (! LOOKUP_STRING(type, group_type_oname) ) {

	char buf[128];
	sprintf(buf, "anon%d", anon_count);
#if 0
	set_obj( me, group_decl_name, 
		 make_obj_f( group_identifier, 0, 
			     group_identifier_string, strdup(buf), 
			     OBJE ) );
#endif
	sprintf( buf, "type_%s_anon%d", LOOKUP_ID(spec, group_spec_name), 
		 anon_count);
	LOOKUP_STRING(me, group_type_oname) = strdup(buf);

	anon_count++;
      }
      /* What to do about types that reference anonymous types... why should 
       * this be a special case? 
       */

      {

	  if (ISCOMPOUND(type) && 0) {
	      dprintf(predecl, "%s = ModBETypes.Type()\n", LOOKUP_STRING(type,group_type_oname));
	      dprintf(out, "t = %s\n", LOOKUP_STRING(type,group_type_oname));
	  } else {
	      dprintf(out, "t = ModBETypes.Type()\n");
	      dprintf(out, "alias = ModBETypes.Alias()\n");
	      dprintf(out, "alias.base = %s\n", LOOKUP_STRING(type, group_type_oname));
	      dprintf(out, "t.type = alias\n");
	  }

	 if (IFACE(spec)) 
	 dprintf(out, "i.types.append( t )\n\n");
	 if (!IFACE(spec)) 
	 dprintf(out, "b.types.append( t )\n\n");
	     dprintf(out, "t.localName = '%s'\n", LOOKUP_ID(me, group_decl_name));
	     
	     dprintf(out, "t.intf = i\n");
	     dprintf(out, "t.name = '%s_'+t.localName\n", LOOKUP_ID(spec, group_spec_name));  
	     dprintf(out, "t.clss = 'ALIAS'\n");
	 if (ISCOMPOUND(type) && 0) {

	     dump_python_r(out, LOOKUP_SUB(me, group_decl_type), spec, 0);
	 }
      }
    }
    break;

    case decl_procedure:
    case decl_procedure_nr: {
      p_list *lptr;
    
      if (me->form == decl_procedure)
	LOOKUP_STRING(me, decl_procedure_intf) = \
	  strdup(LOOKUP_ID(spec, group_spec_name));

      if (me->form == decl_procedure_nr) 
	LOOKUP_STRING(me, decl_procedure_nr_intf) = \
	  strdup(LOOKUP_ID(spec, group_spec_name));

      LOOKUP_STRING(type,type_method_intf) = \
	strdup(LOOKUP_ID(spec,group_spec_name));

      dprintf(out, "o = ModBEOps.Operation()\n");
      if (me->form == decl_procedure) 
	dprintf( out, "o.number = %d\n", 
		 LOOKUP_INT(me, decl_procedure_op_count));
      if (me->form == decl_procedure_nr) 
	dprintf( out, "o.number = %d\n", 
		 LOOKUP_INT(me, decl_procedure_nr_op_count));

      dprintf( out, "o.name = '%s'\n", 
	       LOOKUP_STRING( LOOKUP_SUB(me, group_decl_name), 
			      group_identifier_string ) );
      dprintf(out, "o.ifName = '%s'\n", LOOKUP_ID(spec, group_spec_name));
      dprintf(out, "o.intf = i\n");
      dprintf(out, "o.type = 'PROC'\n");
      dump_python_arglist( out, LOOKUP_SUB(type, type_method_args), 
			   ARGLIST_PARAM, spec);
      dprintf(out, "o.pars = al\n");
      dump_python_arglist( out, LOOKUP_SUB(type, type_method_rets), 
			   ARGLIST_RESULT, spec);
      dprintf(out, "o.results = al\n");
      dprintf(out, "el = []\n");

      if (me->form == decl_procedure) 
	lptr = LOOKUP_SUB(me, decl_procedure_raises);
      if (me->form == decl_procedure_nr) 
	lptr = LOOKUP_SUB(me, decl_procedure_nr_raises); 

      while (lptr) {
	p_sco *sco;
	char buf[128];
      
	sco = LOOKUP_SUB(lptr, list_sco_sco);
	if (LOOKUP_SUB(sco, group_sco_scope)) {
	  sprintf( buf, "excn_%s_%s", LOOKUP_ID(sco, group_sco_scope), 
		   LOOKUP_ID(sco, group_sco_name));
	} else {
	  sprintf( buf, "excn_%s_%s", LOOKUP_ID(spec, group_spec_name), 
		   LOOKUP_ID(sco, group_sco_name));
	}
	LOOKUP_STRING(sco, group_sco_oname) = strdup(buf);
	dprintf(out, "el.append( %s )\n", LOOKUP_STRING(sco, group_sco_oname));
	lptr = LOOKUP_SUB(lptr, group_list_next);
      }
      dprintf(out, "o.raises = el\n");
      dprintf(out, "o.returns = %d\n", (me->form == decl_procedure)? 1 : 0);
      if (IFACE(spec)) 
	dprintf(out, "i.ops.append( o )\n");
    }
    break;

    case decl_announcement: {
      dprintf(out, "o = ModBEOps.Operation()\n");
      dprintf(out, "o.number = %d\n", 
	      LOOKUP_INT(me, decl_announcement_op_count));
      dprintf(out, "o.name = '%s'\n", LOOKUP_ID(me, group_decl_name));
      dprintf(out, "o.ifName = '%s'\n", LOOKUP_ID(spec, group_spec_name));
      dprintf(out, "o.intf = i\n");
      dprintf(out, "o.type = 'ANNOUNCEMENT'\n");
      /* dump_python_arglist( out, LOOKUP_SUB(me, decl_announcement_list), 
			   ARGLIST_PARAM, spec); */
      dprintf(out, "o.pars = al\n");
      dprintf(out, "o.results = []\n");
      dprintf(out, "o.raises = []\n");
      dprintf(out, "i.ops.append( o )\n");
    }
    break;

    case decl_exception: {
      char buf[128];

      sprintf(buf, "excn_%s_%s", LOOKUP_ID(spec, group_spec_name), 
	      LOOKUP_ID(me, group_decl_name));
      LOOKUP_STRING(me, decl_exception_oname) = strdup(buf);
      dprintf(out, "e = ModBEOps.Exception()\n");
      dprintf(out, "e.name = '%s'\n", LOOKUP_ID(me, group_decl_name));
      dprintf(out, "e.ifName = '%s'\n", LOOKUP_ID(spec, group_spec_name));
      dprintf(out, "e.intf = i\n");
      dump_python_arglist( out, LOOKUP_SUB(me, decl_exception_list), 
			   ARGLIST_PARAM, spec);
      dprintf(out, "e.pars = al\n");
      dprintf(out, "%s = e\n", LOOKUP_STRING(me, decl_exception_oname));
      dprintf(out, "i.excs.append( e )\n");
    }
    break;

    case decl_var:
    case decl_invar:
    case decl_outvar:
    case decl_inoutvar: {
      int mnum;
      char buf[128];
      OUTPUT *tbuf;
    
      mnum = anon_count++;
      sprintf(buf, "anon%d", mnum);
      LOOKUP_STRING(type, group_type_oname) = strdup(buf);
      tbuf = dump_makebuffer(32);
      rep_type(tbuf, type);
    
      /* Declare type */
      dprintf(out, "t = ModBETypes.Type()\n");
      if (IFACE(spec)) 
	dprintf(out, "i.types.append( t )\n\n");
      if (!IFACE(spec)) 
	dprintf(out, "b.types.append( t )\n\n");


      dprintf(out, "t.localName = '%s'\n", buf);
      dprintf(out, "t.intf = i\n");
      dprintf(out, "t.name = '%s_%s'\n", 
	      LOOKUP_ID(spec, group_spec_name), buf);
      dump_python_r(out, LOOKUP_SUB(me, group_decl_type), spec, 0);
    
      /* Declare variable */
      dprintf(out, "v = ModBEBlocks.CVar()\n");
      dprintf(out, "v.name = '%s'\n", LOOKUP_ID(me, group_decl_name));
      dprintf(out, "v.type = %s\n", LOOKUP_STRING(type, group_type_oname));
      dprintf(out, "v.typename = '%s'\n", tbuf->ch[0].cptr);
      dprintf(out, "b.vars.append( v )\n");
      free_output(tbuf);
    }
    break;
  } /* End switch */
}
