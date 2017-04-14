/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * dpython.h; header for da*.c AST dumping files 
 *
 */

#ifndef __DAST_H__
#define __DAST_H__
#define TYPES_BUILTIN	14
#define ARGLIST_PARAM	0
#define ARGLIST_RESULT	1

#define IFACE(spec) \
  (spec->form == spec_interface || spec->form == spec_local_interface)

extern char *Predefined_Names[TYPES_BUILTIN];
extern int anon_count;
extern int op_count;
extern int expr_count;

#define ISCOMPOUND(x) (x->form >= type_enum)
#define INTF_SYMBOL "intf"
#define IMP_SYMBOL "imp"


void dump_ast(OUTPUT *code, p_obj *me);
void dump_ast_r(OUTPUT *code, p_obj *me, int *counter);


#endif
