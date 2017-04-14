/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * dpython.h; header for dp*.c Python dumping files 
 *
 * Dickon Reed, 1996
 * Richard Mortier, 1997
 *
 * $Id: dpython.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: dpython.h,v $
 * Revision 1.1  1997/06/25 17:08:27  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/12 13:22:57  rmm1002
 * Changed TYPES_BUILTIN due to removing ADDRESS, WORD; changed INTERFACE to
 * IFACE (clash with definition of INTERFACE keyword in grammar)
 *
 * Revision 2.1  1997/04/04 15:35:04  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.12  1997/04/02 20:29:30  rmm1002
 * *** empty log message ***
 *
 * Revision 1.11  1997/02/10 19:43:10  rmm1002
 * *** empty log message ***
 *
 * Revision 1.10  1997/02/04 16:48:42  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:41:42  rmm1002
 * Initial revision
 *
 */

void dump_python_r(OUTPUT *out, p_obj *me, p_obj *spec, int opCode);
void dump_python_header(OUTPUT *out);

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

/* dpexpr.c */
void dump_python_cexpr(OUTPUT *out, p_obj *me);
void dump_python_rexpr(OUTPUT *out, p_obj *me, p_obj *spec);
void dump_python_tupstruct(OUTPUT *out, p_obj *me);

/* dpdecl.c */
void dump_python_arglist(OUTPUT *out, p_list *list, int kind, p_obj *spec);
void dump_python_rdecl(OUTPUT *out, p_obj *me, p_obj *spec);

/* dpspec.c */
void dump_python_header(OUTPUT *out);
void dump_python_rspec(OUTPUT *out, p_obj *me, p_obj *spec);
void dump_python(OUTPUT *preamble, OUTPUT *predecl, OUTPUT *code,  p_obj *me);

/* dptype.c */
void dump_python_ensureavail(OUTPUT *out, p_obj *me, p_obj *spec);
void dump_python_rtype(OUTPUT *out, p_obj *me, p_obj *spec);

