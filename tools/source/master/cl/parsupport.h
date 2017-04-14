/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless */

/* parsupport.h; header for parsupport.c */

/* Dickon Reed, 1996 */
/* Richard Mortier, 1997 */

/* $Id: parsupport.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ */

/*
 * $Log: parsupport.h,v $
 * Revision 1.1  1997/06/25 17:08:27  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:35:37  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.10  1997/02/04 16:46:24  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:47:13  rmm1002
 * Initial revision
 *
 */

#include <stdarg.h>

#define OBJE -1
#define TYPES_IDENTICAL 0
#define TYPES_PROMOTE_1 1
#define TYPES_PROMOTE_2 2
#define TYPES_DISTINCT 3

/*
 * Macros for locating objects with given parameter in the tree
 */

#define LOOKUP_SUB(obj, field)    (obj->ch[field].sub)
#define LOOKUP_STRING(obj, field) (obj->ch[field].string)
#define LOOKUP_INT(obj, field)    (obj->ch[field].integer)
#define LOOKUP_ID(obj, field)     LOOKUP_STRING(LOOKUP_SUB(obj, field), group_identifier_string)

p_obj *make_obj_f(int cl, int form, ...);
void set_obj_f(p_obj *obj, va_list ap);
void set_obj(p_obj *obj, int param, void *val); /* val must be a pointer */

p_obj *lookup(p_obj *obj, int name);

p_expression *make_bracket(p_list *sub);
p_expression *make_binop(int op, p_expression *l, p_expression *r, int leftpromo);
p_expression *make_boolop(int op, p_expression *l, p_expression *r);
p_expression *make_boolval(int val);
p_expression *make_unaryop(int op, p_expression *x);

void augment_identlist(p_list *l, p_type *t);
void attach_list(p_list *x, p_list *y);
void setmode_list(p_list *x, int mode);

p_decl *lookup_decl(p_list *l, p_identifier *id);
p_list *make_listnode(int form, void *datum, p_list *next);
p_list *make_finalnode(int form, void *datum);
p_decl *make_decl_list(int form, p_identifier *id, p_list *list);
p_decl *make_decl_type(int form, p_identifier *id, p_type *type);
p_type *get_decl_type(p_decl *decl);

void follow_interfaces(p_obj *me, enum parser_mode newpm);

