/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless
 *
 * stackobj.h; parser stack objects
 *
 * Dickon Reed, 1996
 * Richard Mortier, 1997
 *
 * $Id: stackobj.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: stackobj.h,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:08:27  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:35:43  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.11  1997/02/20 00:28:41  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:48:58  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:48:39  rmm1002
 * Initial revision
 *
 */
#ifndef __STACKOBJ_H__
#define __STACKOBJ_H__

typedef struct p_obj_t {
  int		    cl;		/* entry in group.tab */
  int		    form;	/* if x.tab exists, entry in x.tab
				 * else zero */
  union {
    struct p_obj_t  *sub;
    char	    *string;
    double	    real;
    int		    integer;
    void	    *vptr;	/* be careful */
  } ch[5];
  char		    *oname;	/* name in python output file, null is
				 * anon (default) */
  struct p_obj_t *up;
  int occurence;
} p_obj;

typedef p_obj p_list;
typedef p_obj p_expression;
typedef p_obj p_type;
typedef p_obj p_decl;
typedef p_obj p_sco;
typedef p_obj p_identifier;
typedef p_obj p_statement;
typedef p_obj p_proc;
typedef p_obj p_spec;
typedef p_obj p_exch;

/* typesupport */

p_type *make_tupletype(p_list *list);

#endif
