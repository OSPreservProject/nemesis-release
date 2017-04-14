/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * symtab.c; code to handle symbol table
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: symtab.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: symtab.c,v $
 * Revision 1.2  1998/04/29 12:35:13  dr10009
 * *** empty log message ***
 *
 * Revision 1.1  1997/06/25 17:10:19  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:35:48  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.11  1997/02/20 00:28:55  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:46:35  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:50:01  rmm1002
 * Initial revision
 *
 */

#include <stdio.h>
#include <string.h>

#include "cluless.h"
#include "stackobj.h"
#include "parsupport.h"
#include "expr.h"
#include "statement.h"
#include "list.h"
#include "spec.h"
#include "decl.h"
#include "type.h"
#include "symtab.h"
#include "group.h"

int scope_level;

symtab_entry *symtab;

void symtab_init(void) {
    scope_level = 0;
    symtab = 0;
}

void symtab_enter_scope(void) {
  scope_level++;
  if (verbosity & VERBOSE_SYMTAB) 
    fprintf(stderr, "Entering scope level %d\n", scope_level);
}

void symtab_exit_scope(void) {
  symtab_entry *ptr, *optr;

  scope_level--;
  if (verbosity & VERBOSE_SYMTAB) 
    fprintf(stderr, "Entering scope level %d\n", scope_level);
  ptr = symtab;
  while (ptr && ptr->scope > scope_level) {
    if (verbosity & VERBOSE_SYMTAB)
      fprintf( stderr, "(Declaration %s going out of scope)\n", 
	       LOOKUP_STRING( LOOKUP_SUB(ptr->decl, group_decl_name),
			      group_identifier_string));
    optr = ptr;
    ptr = ptr->next;
    free(optr);
  }
  symtab = ptr;
}

void symtab_new_decl(p_decl *decl) {  
  switch (decl->form) {
    default: {
      symtab_entry *ptr;

      if (verbosity & VERBOSE_SYMTAB) {
	fprintf( stderr, 
		 "(Declaration %s entering scope level %d with form %d)\n", 
		 LOOKUP_STRING( LOOKUP_SUB(decl, group_decl_name), 
				group_identifier_string ),
		 scope_level, decl->form );
      }
      ptr = NEW( symtab_entry );
      ptr->next = symtab;
      ptr->decl = decl;
      ptr->scope = scope_level;
      symtab = ptr;
    }
  }
}

p_decl *symtab_lookup_decl(p_identifier *id, int valid_form) {
  symtab_entry *ptr;
  char *sid, *hid;

  sid = LOOKUP_STRING(id, group_identifier_string);
  ptr = symtab;
  while (ptr) {
    if ((1<<ptr->decl->form) & valid_form) { 
      /* match */
      hid = LOOKUP_STRING( LOOKUP_SUB(ptr->decl, group_decl_name), 
			   group_identifier_string);
   
      if (!strcmp(hid, sid)) {
	if (verbosity & VERBOSE_SYMTAB) 
	  fprintf( stderr, "Looked up %s of type %x from set %x\n", 
		   LOOKUP_STRING(id, group_identifier_string), 
		   1<<(ptr->decl->form), valid_form);
	return ptr->decl;
      }
    }
    ptr = ptr->next;
  }
  return 0;
}

void symtab_dump(OUTPUT *out) {
  symtab_entry *ptr;

  ptr = symtab;
  dprintf(out, "Symbol table\n");
  while (ptr) {
    dprintf(out, "["); 
    dump_tree(out, ptr->decl);
    dprintf(out, "]\n");
    ptr = ptr->next;
  }
}
