/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless */

/* tables.h; nature values used in tables */

/* Dickon Reed, 1996 */
/* Richard Mortier, 1997 */

/* $Id: tables.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ */

/*
 * $Log: tables.h,v $
 * Revision 1.1  1997/06/25 17:08:27  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:35:53  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.10  1997/02/04 16:46:41  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:52:52  rmm1002
 * Initial revision
 *
 */

enum p_nature {
  n_empty,
  n_generic,
  n_expr,
  n_id,
  n_sco,
  n_list,
  n_statement,
  n_type,
  n_spec,
  n_decl,
  n_exc_handler,
  n_proc,
  n_shortcut,
  n_string,
  n_integer,
  n_real,
  n_field,
  n_file
};

#define n_cptr n_string
#define n_field n_decl
#define n_arg n_decl
#define n_excpetion n_decl
#define n_block n_statement

/* The general format for tables */
struct table_format {
  char *name;
  int order;
  int nature[5]; 
};



