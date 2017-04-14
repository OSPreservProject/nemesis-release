/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 
/* CLUless 
 * * dpython.c; main Python dump routines 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: dast.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: dast.c,v $
 * Revision 1.3  1998/04/28 14:45:55  dr10009
 * *** empty log message ***
 *
 * Revision 1.1  1997/06/25 17:06:44  dr10009
 * Initial revision
 *
 * Revision 2.2  1997/04/07 20:48:03  rmm1002
 * Fixed output of unmatched IF stmts; added LOOP and EXIT dump clauses
 *
 * Revision 2.1  1997/04/04 15:35:01  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.16  1997/04/04 15:29:27  rmm1002
 * Added empty statement (';')
 *
 * Revision 1.15  1997/04/02 20:21:48  rmm1002
 * Fixed code generation! Now works for Compound(), While(), If(), Wibble(),
 * C_Block(), Expr() (only simple expressions), and added Repeat().
 *
 * Revision 1.14  1997/04/02 14:09:01  rmm1002
 * Trying to add REPEAT-UNTIL
 *
 * Revision 1.13  1997/03/21 12:22:23  rmm1002
 * Removed output of '[' and ']' around block statments (commented prev.)
 *
 * Revision 1.12  1997/03/19 21:55:33  rmm1002
 * Added support for inline C; now works
 *
 * Revision 1.11  1997/02/20 00:28:14  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:48:40  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 21:22:09  rmm1002
 * Finished fixup of layout#
 *
 * Revision 1.1  1997/01/30 21:41:36  rmm1002
 * Initial revision
 *
 */

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
#include "dast.h"
#include "output.h"
#include "md5.h"
#define ASTVERSION 0

typedef unsigned long long int uint64_t;



#define MAGICMARKER 0xBABE007

#define SHORTCUTS

int gcounter;

p_obj *currentintf;

void dputword(OUTPUT *out, uint32_t x) {
    int i;
    switch (out->form) {
    case output_filestream:
	fputc(x & 0xff, out->ch[0].file);
	fputc((x>>8)&0xff, out->ch[0].file);
	fputc((x>>16)&0xff, out->ch[0].file);
	fputc((x>>24)&0xff, out->ch[0].file);
	break;
    case output_buffer:
	for (i=0; i<4; i++) {
	    if (out->ch[0].cptr  + out->ch[1].integer <= out->ch[2].cptr) {
		char *old;

		old = out->ch[0].cptr;
		out->ch[0].cptr = malloc(sizeof(char)*2*(out->ch[1].integer));
		memcpy(out->ch[0].cptr, old, out->ch[1].integer);
		out->ch[1].integer *= 2;
		out->ch[2].cptr = (out->ch[2].cptr - old) + (out->ch[0].cptr);
		
	    }
	    
	    *out->ch[2].cptr ++ = x & 0xff;
	    x>>=8;
	    i++;
	}
	break;
    }
    gcounter++;
}


void dputstring(OUTPUT *code, char *me, int *counter) {
    int i;
    char *str;
    int l;
    int j;
    int out;
    str = me;

    if (!str) { 
	dputword(code, 0);
	(*counter)++;
	return;
    }

    i = l = strlen(str)+1;
    while (*str || i>0) {
	out = 0;
	for (j=0; j<4; j++) {
	    if (*str) {
		out += (*(str++)) << (j*8);
	    } else {
	    }
	    i --;

	}
	dputword(code, out);
    }

}

void reset_ast_r(p_obj *me) {
    int i;
    if (!me) return;
    me->occurence = 0;

    switch (me->cl) {
	case group_expression:
	for (i=0; i<t_expr[me->form].order; i++) {
	    switch (t_expr[me->form].nature[i]) {
	    case n_integer:
		break;
	    case n_cptr:
		break;
	    default:
		reset_ast_r(LOOKUP_SUB(me, group_list_g1));
		break;
	    }
	}
	break;
    case group_list: {
	p_list *lptr;
	lptr = me;
	while (lptr) {
	    reset_ast_r(LOOKUP_SUB(lptr, group_list_g1));
	    lptr = LOOKUP_SUB(lptr, group_list_next);
	}
    } break;
    case group_statement: {
      int i;

      for (i = 0; i < t_statement[me->form].order; i++) {
	if (t_statement[me->form].nature[i] != n_empty) {
	  reset_ast_r(me->ch[i].sub);
	}
      }
    } break;

    case group_sco: 
	reset_ast_r(LOOKUP_SUB(me, group_sco_scope));
	reset_ast_r(LOOKUP_SUB(me, group_sco_name));
	break;

    
    
    case group_proc:
	reset_ast_r(LOOKUP_SUB(me, group_proc_decl));
	reset_ast_r(LOOKUP_SUB(me, group_proc_statement));
	break;

    case group_type: {
	int i;
	for (i=0; i<3; i++) {
	    switch (t_type[me->form].nature[i]) {
	    case n_empty:
		break;
	    case n_cptr:
		break;
	    case n_integer:
		break;
	    default:
		reset_ast_r(LOOKUP_SUB(me,i));
	    }
	} 
    } break;

    case group_spec: 
	reset_ast_r(LOOKUP_SUB(me, group_spec_name));
	reset_ast_r(LOOKUP_SUB(me, group_spec_extends));
	reset_ast_r(LOOKUP_SUB(me, group_spec_needs));
	reset_ast_r(LOOKUP_SUB(me, group_spec_body));
	break;

    case group_decl:
	reset_ast_r(LOOKUP_SUB(me, group_decl_name));
	reset_ast_r(LOOKUP_SUB(me, group_decl_type));
	switch (me->form) {
	case decl_announcement:
	    /* dputword(code,  me->ch[decl_announcement_op_count].integer); */
	    break;
	case decl_exception:
	    reset_ast_r(LOOKUP_SUB(me, decl_exception_list));
	    break;
	case decl_procedure:
	    reset_ast_r(LOOKUP_SUB(me, decl_procedure_raises));
	    /* dputword(code,  me->ch[decl_procedure_op_count].integer); */
	    break;
	case decl_procedure_nr:
	    reset_ast_r(LOOKUP_SUB(me, decl_procedure_nr_raises));
	    /* dputword(code,  me->ch[decl_procedure_op_count].integer); */

	    break;
	case decl_spec:
	    reset_ast_r(LOOKUP_SUB(me, decl_spec_spec));
	    break;
	}
	break;

    case group_exch:
	reset_ast_r(LOOKUP_SUB(me, group_exch_sco));
	reset_ast_r(LOOKUP_SUB(me, group_exch_handle));
	break;
    }
}

void dump_ast_r(OUTPUT *code, p_obj *me, int *counter) {
    uint32_t x;
    int i;
    int payload;

    /* fprintf(stderr,"%d:\n", *counter); */
    if (!me) {
	dputword(code, 0);
	(*counter)++;
	return;
    }

    x = ((me->cl + 1)<<24)  + ( (me->form + 1)<<16);
    payload = 0;

#ifdef SHORTCUTS
    if ( me->occurence) {
	/* fprintf(stderr,"%d: Adding shortcut for %d\n", *counter, me->occurence); */
	char *intfname;
	if (!me->up) {
	    fprintf(stderr,"Alert! Unscoped shortcut\n");
	}
	intfname = LOOKUP_ID(me->up, group_spec_name);
	x = ((group_shortcut + 1)<<24) + ((me->cl+1) << 16) + strlen(intfname);
	
	dputword(code, x);
	dputword(code, me->occurence);
	dputstring(code, intfname, counter);
#if 0
	{
	    extern OUTPUT *deps;
	    dprintf(deps, "%s.if ", intfname);
	}
#endif
	return;
    }
#endif
    /* twiddling the payload */
    switch (me->cl) {
    case group_list: {
	int count;
	p_list *lptr;
	count =0;
	lptr = me;
	while (lptr) {
	    lptr = LOOKUP_SUB(lptr, group_list_next);
	    count++;
	}
	payload = count;
    } break;
    case group_identifier:
	payload = strlen(LOOKUP_STRING(me, group_identifier_string));
	break;
    }
    x += payload;


#ifdef TRUST_COUNTER
    me->up = currentintf;
    me->occurence = *counter;
#else
    me->up = currentintf;
    me->occurence = gcounter;
#endif
    (*counter) = (*counter) + 1;


    dputword(code, x);

    

    /* tails of the payload */
    switch (me->cl) {
    case group_identifier: {
	dputstring(code, LOOKUP_STRING(me, group_identifier_string), counter);
    }
    break;
    
    case group_expression:
	for (i=0; i<t_expr[me->form].order; i++) {
	    switch (t_expr[me->form].nature[i]) {
	    case n_integer:
		dputword(code, me->ch[i].integer);
		(*counter) ++ ;
		break;
	    case n_cptr:
		dputstring(code, me->ch[i].string, counter);
		break;
	    default:
		dump_ast_r(code, LOOKUP_SUB(me, group_list_g1),counter);
		break;
	    }
	}
	break;
	
    case group_list: {
	p_list *lptr;
	lptr = me;
	while (lptr) {
	    dump_ast_r(code, LOOKUP_SUB(lptr, group_list_g1),counter);
	    lptr = LOOKUP_SUB(lptr, group_list_next);
	}
    } break;
    
    case group_statement: {
      int i;

      for (i = 0; i < t_statement[me->form].order; i++) {
	if (t_statement[me->form].nature[i] != n_empty) {
	  dump_ast_r(code, me->ch[i].sub, counter);
	}
      }
    } break;

    case group_sco: 
	dump_ast_r(code, LOOKUP_SUB(me, group_sco_scope),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_sco_name),counter);
	break;

    
    
    case group_proc:
	dump_ast_r(code, LOOKUP_SUB(me, group_proc_decl),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_proc_statement),counter);
	break;

    case group_type: {
	int i;
	for (i=0; i<3; i++) {
	    switch (t_type[me->form].nature[i]) {
	    case n_empty:
		break;
	    case n_cptr:
		dputstring(code, me->ch[i].string, counter);
		break;
	    case n_integer:
		dputword(code, me->ch[i].integer);
		(*counter)++;
		break;
	    default:
		dump_ast_r(code, LOOKUP_SUB(me,i), counter);
	    }
	} 
    } break;

    case group_spec: 
	currentintf = me;
	gcounter = 1;
	(*counter) = 1;
	dump_ast_r(code, LOOKUP_SUB(me, group_spec_name),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_spec_extends),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_spec_needs),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_spec_body),counter);
	break;

    case group_decl:
	dump_ast_r(code, LOOKUP_SUB(me, group_decl_name),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_decl_type),counter);
	switch (me->form) {
	case decl_announcement:
	    /* dputword(code,  me->ch[decl_announcement_op_count].integer); */
	    break;
	case decl_exception:
	    dump_ast_r(code, LOOKUP_SUB(me, decl_exception_list),counter);
	    break;
	case decl_procedure:
	    dump_ast_r(code, LOOKUP_SUB(me, decl_procedure_raises),counter);
	    /* dputword(code,  me->ch[decl_procedure_op_count].integer); */
	    (*counter)++;
	    break;
	case decl_procedure_nr:
	    dump_ast_r(code, LOOKUP_SUB(me, decl_procedure_nr_raises),counter);
	    /* dputword(code,  me->ch[decl_procedure_op_count].integer); */

	    break;
	case decl_spec:
	    dump_ast_r(code, LOOKUP_SUB(me, decl_spec_spec),counter);
	    break;
	}
	break;

    case group_exch:
	dump_ast_r(code, LOOKUP_SUB(me, group_exch_sco),counter);
	dump_ast_r(code, LOOKUP_SUB(me, group_exch_handle),counter);
	break;
    }
}



void dump_ast(OUTPUT *code, p_list *speclist) {
    int counter;
    p_list *lptr;
    OUTPUT *devnull;
 
   counter = 1;
    
    dputword(code, 0xA5F00D00 + ASTVERSION);
    gcounter = 0;
    currentintf = 0;

    devnull = dump_makestream(fopen("/dev/null", "w"));

    lptr = speclist;
    while (lptr) {
	p_obj *here;
	here = LOOKUP_SUB(lptr, group_list_g1);
	lptr = LOOKUP_SUB(lptr, group_list_next);
	if (lptr) {
	    /* recurse but put down /dev/null */
	    dump_ast_r(devnull, here, &counter);
	} else {
	    dump_ast_r(code, here, &counter);
	}

    }

    free_output(devnull);
    reset_ast_r(speclist);
#if 0
    sink = dump_makebuffer(65556);
    dump_ast_r(sink, speclist, &counter);

    {
	uint64_t md5sum;
	struct MD5Context ctx;
        md5byte digest[16];
	int i;

	MD5Init(&ctx);
	MD5Update(&ctx, code->ch[0].cptr, code->ch[output_buffer_size].integer);

	MD5Final(digest, &ctx);

	for (i=0; i<16; i++) {
	    dprintf(code, "%c", digest[i]);
	}
    }
    free_output(sink);
#endif    
    
}
