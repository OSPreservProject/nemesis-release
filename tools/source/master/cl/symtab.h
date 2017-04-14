/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless */

/* symtab.h; header for symtab.c */

/* Dickon Reed, 1996 */
/* Richard Mortier, 1997 */

/* $Id: symtab.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ */

/*
 * $Log: symtab.h,v $
 * Revision 1.1  1997/06/25 17:08:27  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:35:51  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.10  1997/02/04 16:46:38  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:50:09  rmm1002
 * Initial revision
 *
 */

typedef struct symtab_entry_t {
  struct symtab_entry_t *next;
  int scope;
  p_decl *decl;
} symtab_entry;

void symtab_enter_scope(void);
void symtab_exit_scope(void);
void symtab_new_decl(p_decl *decl);
p_decl *symtab_lookup_decl(p_identifier *id, int form);
void symtab_dump(OUTPUT *out);

