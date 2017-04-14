/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless */

/* typesupport.h; header for typesupport.c */

/* Dickon Reed, 1996 */
/* Richard Mortier, 1997 */

/* $Id: typesupport.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ */

/*
 * $Log: typesupport.h,v $
 * Revision 1.3  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.2  1998/04/14 14:15:41  dr10009
 * Multiple interfaces at once
 *
 * Revision 1.1  1997/06/25 17:08:27  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/12 13:15:19  rmm1002
 * Removed make_predeftype heaeder
 *
 * Revision 2.1  1997/04/04 15:36:09  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.10  1997/02/04 16:46:55  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:52:41  rmm1002
 * Initial revision
 *
 */

#ifndef __TYPESUPPORT_H__
#define __TYPESUPPORT_H__

void rep_type(OUTPUT *out, p_obj *me);
int hash_string(char *str, int size);
void build_type_table(OUTPUT *out, p_obj *me);

int compare_lists(p_list *l, p_list *r);
int compare_types(p_type *t1, p_type *t2);

p_type *make_tupletype(p_list *list);
p_list *make_tuplelist(p_list *list);
p_type *make_promotype(p_type *l, p_type *r, int leftpromo);
p_obj *make_deepref_type(p_obj *type);
void btt_visit(OUTPUT *out, p_obj *type) ;
#endif /* __TYPESUPPORT_H__ */
