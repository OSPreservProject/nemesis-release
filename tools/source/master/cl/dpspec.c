/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless 
 *
 * dpspec.c; dump Python for specifications 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: dpspec.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: dpspec.c,v $
 * Revision 1.3  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.2  1997/09/12 13:41:07  rjb17
 * dont use bad function
 *
 * Revision 1.1  1997/06/25 17:09:28  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/12 13:19:50  rmm1002
 * Changed magic constant to what it is and rmoved old junk for output options
 *
 * Revision 2.1  1997/04/04 15:34:54  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.17  1997/04/04 15:28:21  rmm1002
 * Fiddling. Python code now better laid out. Expanded ugly random "for" loop
 * so it is now legible
 *
 * Revision 1.16  1997/04/02 20:29:12  rmm1002
 * Changed calls of dump_python_r to include new param (opCode)
 *
 * Revision 1.15  1997/03/21 12:21:36  rmm1002
 * Additions to make compatible with "middlc -c";
 * Also more commenting of Python output
 *
 * Revision 1.14  1997/03/08 15:22:27  rmm1002
 * Changed banner info.
 *
 * Revision 1.13  1997/02/26 00:11:11  rmm1002
 * Support for fingerprinting (with fp_reduce) (HACK)
 *
 * Revision 1.12  1997/02/20 00:28:09  rmm1002
 * No substantive changes
 *
 * Revision 1.11  1997/02/10 19:42:48  rmm1002
 * *** empty log message ***
 *
 * Revision 1.10  1997/02/04 16:48:36  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 21:53:58  rmm1002
 * Finished fixup of layout
 *
 * Revision 1.1  1997/01/30 21:41:19  rmm1002
 * Initial revision
 *
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

#define PICKLING

void dump_python_header(OUTPUT *out) {
  int i;

  dprintf(out, "#############################\n");
  dprintf(out, "# Standard imports\n");
  dprintf(out, "import ModBE\n" );
  dprintf(out, "import ModBEIface\n" );
  dprintf(out, "import ModBEOps\n" );
  dprintf(out, "import ModBETypes\n" );
  dprintf(out, "import ModBEImports\n" );
  dprintf(out, "import ModBEImp\n" );
  dprintf(out, "import ModBEBlocks\n");
  dprintf(out, "import ModBEStatements\n");
  dprintf(out, "import ModBEExprs\n");
  dprintf(out, "import sys\n");
#ifdef PICKLING
  dprintf(out, "import pickle\n");
#endif
  dprintf(out, "tstack=[]\n");
  dprintf(out, "i=ModBEIface.Interface()\n");
  dprintf(out, "i.name='global'\n");
  dprintf(out, "i.types=[]\n");
  dprintf(out, "i.init()\n");
	dprintf(out, "tt=[]\n");

  for ( i = 0; i < TYPES_BUILTIN; i++ )
    dprintf(out, "from ModBETypes import %s\n", Predefined_Names[ i ] );  
}

void dump_python_rspec(OUTPUT *out, p_obj *me, p_obj *spec) {
  time_t theTime = time((time_t *) 0);
  char *timestr = ctime(&theTime);
  char hoststr[200];
  p_list *lptr;
  
  timestr[strlen(timestr) - 1] = '\0';  
  if (gethostname(hoststr, 200 )) {
    fprintf(stderr, "Failed to get hostname\n");
    strcpy(hoststr, "unknown");
  }
  LOOKUP_ID(me, group_spec_name) = LOOKUP_ID(me, group_spec_name );
  
  dprintf( out, "#########################################\n");
  dprintf( out, "# Main dump for %s %s\n",
	   me->form == spec_implementation? "implementation" : "interface",
	   LOOKUP_ID(me, group_spec_name) );
  dprintf( out, "i = %s_%s\n", 
	   me->form == spec_implementation? IMP_SYMBOL : INTF_SYMBOL, 
	   LOOKUP_ID(me, group_spec_name) );
  dprintf( out, "i.frontEnd = '%s'\n", "CLUless $Revision: 1.1 $ $Author: dr10009 $ $Date: Thu, 18 Feb 1999 14:20:06 +0000 $" );

  dprintf(out, "i.compTime = '%s'\n", timestr);
  dprintf(out, "i.userName = '%s'\n", getlogin());
  dprintf(out, "i.machine = '%s'\n", hoststr);
  dprintf(out, "i.idlFile = './%s'\n", instack[instackptr].name);
  dprintf(out, "i.name = '%s'\n", LOOKUP_ID(me, group_spec_name));
  dprintf(out, "i.isModule = %d\n", me->form == spec_implementation ? 1:0);
  dprintf(out, "i.local = %d\n", me->form == spec_local_interface ? 1:0);
  dprintf(out, "i.imports = []\n\n");

  if (me->form == spec_implementation) {
    /* Always EXTEND the interface for the implementation */
    dprintf(out, "imp = ModBEImports.Import()\n");
    dprintf(out, "imp.mode = '%s'\n", "EXTENDS");
    dprintf(out, "imp.intf = %s_%s\n", INTF_SYMBOL,
	    LOOKUP_ID(me, group_spec_name));	
    dprintf(out, "imp.name = '%s'\n", 
      
	    LOOKUP_ID(me, group_spec_name));
    dprintf(out, "i.imports.append( imp )\n");
    dprintf(out, "i.intf = %s_%s\n\n", INTF_SYMBOL, 
	    LOOKUP_ID(me, group_spec_name));
    dprintf(out, "# Building type table...\n");
    build_type_table(out, me);
  } 

  /* Dump extends... */
  dprintf(out, "\n# Dumping EXTENDS...\n");
  lptr = LOOKUP_SUB(me, group_spec_extends);

  while (lptr) {
    char *intf;

    if ((intf = LOOKUP_ID(lptr, list_id_id))) {
      dprintf(out, "imp = ModBEImports.Import()\n");
      dprintf(out, "imp.mode = '%s'\n", "EXTENDS");
      dprintf(out, "imp.intf = %s_%s\n", INTF_SYMBOL, intf);	
      dprintf(out, "imp.name = '%s'\n", intf);
      dprintf(out, "i.imports.append( imp )\n");
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
  }

  /* ...then needs... */
  dprintf(out, "\n# Dumping NEEDS...\n");
  lptr = LOOKUP_SUB(me, group_spec_needs);
  while (lptr) {
    char *intf;

    if ((intf = LOOKUP_ID(lptr, list_id_id))) {
      dprintf(out, "imp = ModBEImports.Import()\n");
      dprintf(out, "imp.mode = '%s'\n", "NEEDS");
      dprintf(out, "imp.intf = %s_%s\n", INTF_SYMBOL, intf);	
      dprintf(out, "imp.name = '%s'\n", intf);
      dprintf(out, "i.imports.append( imp )\n");
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
  }

  /* ...then types... */
  dprintf(out, "\n# Dumping TYPES...\n");
  lptr = LOOKUP_SUB(me, group_spec_body);
  while (lptr) {
    if (lptr->form == list_decl) {
      p_decl *decl;

      decl = LOOKUP_SUB(lptr, list_decl_decl);
      if (decl->form ==  decl_type)
	dump_python_r(out, decl, spec, 0);
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
  } 

  /* ...then declarations... */
  dprintf(out, "\n# Dumping DECLARATIONS...\n");
  dprintf(out, "i.excs = []\n");
  dprintf(out, "i.ops = []\n");
    
  lptr = LOOKUP_SUB(me, group_spec_body);
  while (lptr) {
    if (lptr->form == list_decl) {
      p_decl *decl;

      decl = LOOKUP_SUB(lptr, list_decl_decl);
      if (decl->form !=  decl_type)
	dump_python_r(out, decl, spec, 0);
    }
    lptr = LOOKUP_SUB(lptr, group_list_next);
  } 

  /* ...then code... */
  dprintf(out, "\n# Dumping CODE...\n");
  lptr = LOOKUP_SUB(me, group_spec_body);
  while (lptr) {
    if (lptr->form == list_proc)
      dump_python_r(out, LOOKUP_SUB(lptr, list_proc_block), spec, 1);
    lptr = LOOKUP_SUB(lptr, group_list_next);
  } 

  /* ...and finish! */
  dprintf(out, "\n# Done!\n");
  dprintf(out, "i = i.init()\n\n");        
}

/******************************************************\
**** Entry function, called from "cluless.c" main() ****
\******************************************************/
void dump_python(OUTPUT *preamble, OUTPUT *predecl, OUTPUT *out, p_obj *me) {
  p_list *lptr, *dptr;
  p_type *ireftype;
  p_spec *spec;
  char *name;

  /* Add imports and other gunk */
  dump_python_header(preamble);

  lptr = me;

  /* Specifications may be mutually recursive, so we need to declare
   * them all here before the main part of the output, so that they
   * are in scope later; while we're at it, set up some basic
   * information.  
   */
  while (lptr) {
    op_count = 0;
    spec = LOOKUP_SUB(lptr, list_spec_spec);
    name = LOOKUP_STRING( LOOKUP_SUB(spec, group_spec_name), \
			  group_identifier_string );

    switch (spec->form) {
      case spec_interface:
      case spec_local_interface:
	ireftype = make_obj_f( group_type, type_iref, 
			       type_iref_id, LOOKUP_SUB(spec, group_spec_name),
			       OBJE );
	dprintf(out, "#########################################\n");
	dprintf(out, "# Dump for interface %s beginning\n", name);
	dprintf(out, "%s_%s = ModBEIface.Interface()\n", INTF_SYMBOL, name);
	dprintf(out, "i = %s_%s\n", INTF_SYMBOL, name);
	dprintf(out, "i.types = []\n");
	dprintf(out, "i.name = '%s'\n", LOOKUP_ID(spec, group_spec_name));
	/* The horrible special case for the iref type */
	dprintf(predecl, "type_%s_IREF = ModBETypes.Type()\n", name); 
	dprintf(out, "t = type_%s_IREF\n", name);
	dprintf(out, "i.types.append( type_%s_IREF )\n\n", name);
	dprintf(out, "type_%s_IREF.localName = 'clp'\n",name);
	dprintf(out, "type_%s_IREF.intf = i\n",name);
	dprintf(out, "type_%s_IREF.name = '%s_clp'\n", name, name);
	dump_python_r(out, ireftype, spec, 0);


      
	for ( dptr = LOOKUP_SUB(spec, group_spec_body); 
	      dptr; 
	      dptr = LOOKUP_SUB(dptr, group_list_next) ) {
	  if (dptr->form == list_decl) {
	    p_obj *item;
	  
	    item = LOOKUP_SUB(dptr, list_decl_decl);
	    if (item->cl == group_decl) {
	      p_type *type;
	    
	      type = LOOKUP_SUB(item, group_decl_type);
	      if (type && !LOOKUP_STRING(type, group_type_oname)) {
		char buf[128];
		sprintf( buf, "type_%s_%s", name, 
			 LOOKUP_ID(item, group_decl_name) );
		LOOKUP_STRING(type, group_type_oname) = strdup(buf);
		
		dprintf(predecl, "%s = ModBETypes.Type()\n", buf);
		dprintf(predecl, "%s.name = '%s'\n", buf, LOOKUP_ID(item, group_decl_name));
		dprintf(out, "t = %s\n", buf);
		dump_python_r(out, type, spec, 0);
	      }
	      if (item->form == decl_procedure)
		LOOKUP_INT(item, decl_procedure_op_count) = op_count++;
	      if (item->form == decl_procedure_nr)
		LOOKUP_INT(item, decl_procedure_nr_op_count) = op_count++;
	      if (item->form == decl_announcement) 
		LOOKUP_INT(item, decl_announcement_op_count) = op_count++;
	    }
	  }
	}
	break;

      case spec_implementation:
	dprintf(out, "#########################################\n");
	dprintf(out, "# Dump for implementation %s beginning\n", name);
	dprintf(out, "%s_%s = ModBEImp.Implementation()\n", IMP_SYMBOL, name);
	dprintf(out, "i = %s_%s\n", IMP_SYMBOL, name);
	dprintf(out, "i.intf = %s_%s\n", INTF_SYMBOL, name);
	dprintf(out, "i.topblock = ModBEBlocks.Block()\n");
	dprintf(out, "b = i.topblock\n");
	dprintf(out, "b.level = 0\n");
	dprintf(out, "b.code = []\n");
	dprintf(out, "b.types = []\n");
	dprintf(out, "b.vars = []\n");
	dprintf(out, "b.imp = i\n");
	dprintf(out, "b.haveparent = 0\n\n");
	break;
    } /* End switch */

    lptr = LOOKUP_SUB(lptr, group_list_next);
  } /* End while */

  lptr = me;
  while (lptr) {
    spec = LOOKUP_SUB(lptr, list_spec_spec);        
    dump_python_r(out, spec, spec, 0);
    /* Note: the spec name will be figured out by the spec dump code */
    lptr = LOOKUP_SUB(lptr, group_list_next);
#ifdef PICKLING
    if (!lptr) {
	dprintf(out, "o = open('%s.p','w')\n", LOOKUP_ID(spec, group_spec_name));

	/* dprintf(out, "sys.stdout.write('Pickle, ')\npickle.dump(i,fp)\npickle.dump(i,o)\no.close()\n");  */
    }
#endif

  }

  /* spec is still the last specification, which is the primary one */
  if (!spec) 
    return;

}
