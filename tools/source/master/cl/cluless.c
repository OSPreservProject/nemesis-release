/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory
 * All Rights Reserved
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)
 *          Richard Mortier (rmm1002@cam.ac.uk)
 *********************************************************************** 
 *
 * cluless.c; main module 
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: cluless.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: cluless.c,v $
 * Revision 1.5  1998/04/29 12:35:05  dr10009
 * *** empty log message ***
 *
 * Revision 1.4  1998/04/14 14:15:41  dr10009
 * Multiple interfaces at once
 *
 * Revision 1.3  1997/07/14 11:36:21  dr10009
 * *** empty log message ***
 *
 * Revision 1.2  1997/07/14 11:35:36  dr10009
 * *** empty log message ***
 *
 * Revision 1.1  1997/06/25 17:06:44  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:34:30  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.13  1997/04/04 15:24:18  rmm1002
 * Now uses vsprintf() instead of vsnprintf() in dprintf() since vsnprintf() is GNU
 * extension and breaks compilation on Alphas
 *
 * Revision 1.12  1997/02/26 00:09:32  rmm1002
 * Rationalized got_spec / got_interface / got_implementation,
 * only outputs to stdout (for shell script to use).
 *
 * Revision 1.11  1997/02/20 00:27:26  rmm1002
 * No substantive changes
 *
 * Revision 1.10  1997/02/04 16:44:31  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.4  1997/02/03 21:54:50  rmm1002
 * Finished fixup of layout
 *
 * Revision 1.3  1997/02/03 21:02:25  rmm1002
 * Removed duplicated strdup definition and finished layout fixup
 *
 * Revision 1.2  1997/01/30 21:13:33  rmm1002
 * Some indentation fix-up
 *
 * Revision 1.1  1997/01/30 17:49:23  rmm1002
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
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
#include "cl.tab.h"
#include "tables.h"
#include "output.h"
#include "group.h"
#include "typesupport.h"

OUTPUT   *code;
OUTPUT *preamble;
OUTPUT *predecl;
OUTPUT *postdecl;
OUTPUT   *messages;
OUTPUT   *treeout;
OUTPUT   *pythonout;
OUTPUT   *deps;
INPUT    *instack; //[INPUT_STACK_SIZE];
int      verbosity;
int      texout;
int      instackptr;
int      suffixtablesize;
int      includetablesize;
char     *suffixtable[SUFFIX_TABLE_SIZE];
char     *includetable[INCLUDE_TABLE_SIZE];
char     basefilename[256];
enum     parser_mode pm;
p_list   *speclist;

void usage(void) {
  fprintf( stderr, "cl\n" );
  fprintf( stderr, "Usage: cl [args] fname\n" );
  fprintf( stderr, "  -vN       set verbosity to N;\n" );
  fprintf( stderr, "              defaults to fully verbose\n" );
  fprintf( stderr, "  -tfname   generate ASCII art parse tree as fname;\n" );
  fprintf( stderr, "              defaults to [basename].tree\n" );
  fprintf( stderr, "  -T        generate treeTeX output\n" );
  fprintf( stderr, "  -I        delete include path\n" );
  fprintf( stderr, "  -Ipath    add path to include path\n" );
  fprintf( stderr, "  -S        delete suffix set\n" );
  fprintf( stderr, "  -Ssuffix  add suffix to suffix set\n" );
  fprintf( stderr, "  -A        write out AST file\n");
  fprintf( stderr, "  -M        write out deps file\n");
}

int main(int argc, char *argv[]) {
  int i;
  int treeflag;
  int writeout;
  char *ptr;
  char *infile;
  char *treefilename;
  char treename[256];
  int genast;
  int gendeps;

  treefilename		= 0;
  treeflag		= 0;
  infile		= 0;
  verbosity		= 0;
  suffixtablesize	= 2;
  suffixtable[0]	= ".if";
  suffixtable[1]	= ".middl";
  includetablesize	= 1;
  includetable[0]	= ".";
  genast =0;
  gendeps = 0;
  messages = dump_makestream(stderr);
  
  for (i=1; i < argc; i++) { 
    /* Parse command line */
    ptr = argv[i];

    if (*ptr == '-') {
      ptr++;
      /* ...it's an option */
      switch (*ptr) {
	case 'v': /* Fully verbose operation */
	  ptr++;
	  if (*ptr) {
	    verbosity |= atoi(ptr);
	  } else {
	    verbosity = 65535; /* all bits set */
	  }
	  break;

	case 't': /* Generate ASCII art tree as basename.tree, or
		   * supplied fname.tree */
	  ptr++;
	  if (*ptr) {
	    treefilename = ptr;
	  } else {
	    treeflag = 1;
	  }
	  break;

	case 'T': 
	  /* Generate TreeTeX output (instead of ASCII) */
	  texout = 1;
	  break;

      case 'A':
	  /* generate AST code */
	  genast = 1;
	  break;

      case 'M':
	  /* generate deps */
	  gendeps = 1;
	  break;

	case 'I': 
	  /* Add path to include path, or delete include path */
	  ptr++;
	  if (*ptr) {
	    /* Add ptr to include table */
	    includetable[includetablesize] = ptr;
	    includetablesize++;
	  } else {
	    /* Reset include table */
	    includetablesize = 0;
	  }
	  break;

	case 'S': 
	  /* Add suffix to suffix set, or delete suffix set */
	  ptr ++;
	  if (*ptr) {
	    /* Add ptr to suffix table */
	    suffixtable[suffixtablesize] = ptr;
	    suffixtablesize++;
	  } else {
	    /* Reset suffix table */
	    suffixtablesize = 0;
	  }
	  break;

	default:
	  usage();
	  return 2;
	  break;

      } /* End switch */
    } else {
      char *cptr;
      int highwater;

      /* now reset everything that is necessary between compiles */
      instackptr		= 0;
      infile		= 0;
      treeout		= 0;
      writeout		= 0;
      texout		= 0;
      highwater = debugging_get_highwater();

      fprintf(stderr,".");
      symtab_init();
      infile = ptr;
      strcpy(basefilename, infile);
      cptr = strrchr(basefilename, '.');
      if (cptr) *cptr = 0;

      if (verbosity) {
	  fprintf(stderr, "Verbosity level %d\n", verbosity);
      }
      
      if (!infile) {
	  usage();
	  fprintf(stderr, "Input not specified\n");
	  return 1;
      }
      
      if (treeflag) {
	  strcpy(treename, basefilename);
	  strcat(treename, ".trees");
      } else if (treefilename) {
	  strcpy(treename, treefilename);
      }
      
      if (*treename) {
	  treeout = dump_makestream(fopen(treename, "w"));
      }
      
      pm = pm_regular;
      
      
      if (gendeps) {
	  char astdname[128];
	  sprintf(astdname,"%s.ast.d", basefilename);
	  
	  deps = dump_makestream(fopen(astdname, "w"));
	  dprintf(deps,"%s.ast: ", basefilename);
      } else {
	  deps = 0;
      }
      
      
      /* push infile on to the stack */
      if (input_open(infile)) {
	  return 3;
      }
      
      /*************************************\
      *** The bit that does all the work! ***
      \*************************************/
      speclist = 0;
      yyparse();
      
      if (genast) {
	  OUTPUT *ast;
	  FILE *astf;
	  char astname[128];
	  
	  sprintf(astname,"%s.ast", basefilename);
	  astf= fopen(astname, "wb");
	  ast = dump_makestream(astf);
	  dump_ast(ast, speclist);
	  
	  free_output(ast);
      }
      
      if (gendeps) {
	  dprintf(deps,"\n");
	  free_output(deps);
      }
      
      input_close();
      
      /*************************************\
      ***** Close files and free memory *****
      \*************************************/
      free_output(treeout);
      debugging_pull_plug(highwater);
    }
  }
  fprintf(stderr,"\n");
  free_output(messages);

  return 0;    
}

/* Hooks for the parser */
void got_spec(void *tree) {
  p_list *oldspeclist, *lptr;

  if (treeout) 
    dump_spec(treeout, tree);

  oldspeclist = speclist;
  speclist = make_obj_f( group_list, list_spec, 
			 list_spec_spec, tree, 
			 group_list_next, 0, 
			 OBJE );
  lptr = oldspeclist;

  /* Step through list of specifications and add current speclist to end */
  while (lptr && LOOKUP_SUB(lptr, group_list_next)) {
    lptr = LOOKUP_SUB(lptr, group_list_next);
  }

  if (lptr) {
    /* False only if this is the first spec received, in which case it is the
     * total list itself.
     */
    LOOKUP_SUB(lptr, group_list_next) = speclist;
    speclist = oldspeclist;
  }
}

/* Annotate expression with Lvalue/Rvalue (1/0) information */
void annotate_expression(p_expression *expr, int l) {

  if (!expr) 
    return;
  
  if (l) {
    LOOKUP_SUB(expr, group_expression_ltype) = \
      make_deepref_type( LOOKUP_SUB(expr, group_expression_type) ); 
    if (verbosity & 128) {
      dprintf(messages, "\nDeep frying:\n"); 
      dump_tree(messages, LOOKUP_SUB(expr, group_expression_type));
      dprintf(messages, "\nFried as:\n"); 
      dump_tree(messages, LOOKUP_SUB(expr, group_expression_ltype));
      dprintf(messages, "\n");
    }
  } else {
    LOOKUP_SUB(expr,group_expression_ltype) = 0;
  }

  /* Watch out if your editing font makes an l look like a 1 */
  switch (expr->form) {  
    case expr_ass_op:
      if (l) 
	yyerror("Warning: assignment of lvalues");
      annotate_expression(LOOKUP_SUB(expr, group_expression_g1), 1);
      annotate_expression(LOOKUP_SUB(expr, group_expression_g2), l);
      break;

    case expr_array_lookup:
      if (l) 
	yyerror("Warning: looking up in lvalue");
      annotate_expression(LOOKUP_SUB(expr, expr_array_lookup_l), 1);
      annotate_expression(LOOKUP_SUB(expr, expr_array_lookup_r), l);

    default: {
	int  j,n;
	
	n = t_expr[expr->form].order;
	for (j=0; j < n; j++) {
	  switch (t_expr[expr->form].nature[j]) {
	    case n_expr:
	      annotate_expression(LOOKUP_SUB(expr, j), l);
	      break;

	    case n_list: {
	      p_list *ptr;
	    
	      ptr = expr->ch[j].sub;
	      while (ptr) {	  
		annotate_expression(LOOKUP_SUB(ptr, j), l);
		ptr = LOOKUP_SUB(ptr, group_list_next);
	      }
	    }
	    break;
	  } /* end switch */
	}    
      }
  } /* end switch */
}

void got_expression(void *tree) {
  /* Annotate entire expression sub-tree to be an rval as one can't
   * assign to an expression (!)  
   */
  p_expression *expr;

  expr = (p_expression *) tree;
  annotate_expression(expr, 0);
}

void dump_spec(OUTPUT *out, void *vptr) {
  p_spec *spec;
  p_list *lptr;
  OUTPUT *buf;

  spec = vptr;

  buf = dump_makebuffer(1024);

  dprintf(buf, "\\section*{Cluless Specification}\n");
  dprintf(buf, "Name ["); 
  dump_tree(buf, LOOKUP_SUB(spec, group_spec_name));
  dprintf(buf, "]\nExtends ["); 
  dump_tree(buf, LOOKUP_SUB(spec, group_spec_extends));
  dprintf(buf, "]\nNeeds [");
  dump_tree(buf, LOOKUP_SUB(spec,group_spec_needs));
  dprintf(buf, "]\n\n");

  lptr = LOOKUP_SUB(spec, group_spec_body);
  if (!lptr) 
    dprintf(buf, "No body!\n");

  while (lptr) {
    dprintf(buf, "Found item of class %d form %d\n", lptr->cl, lptr->form);

    switch (lptr->form) {
      case list_decl:  
	dprintf(buf, PREAMBLE);
	dump_tree(buf, LOOKUP_SUB(lptr, list_decl_decl));
	dprintf(buf, POSTAMBLE);
	break;

      case list_proc:
	dprintf(buf, PREAMBLE);
	dump_tree( buf, LOOKUP_SUB( LOOKUP_SUB(lptr, list_proc_block), \
				    group_proc_decl) );;
	dprintf(buf,POSTAMBLE);
	dprintf(buf,"\n");
	dprintf(buf,PREAMBLE);
	dump_tree(buf, LOOKUP_SUB( LOOKUP_SUB(lptr, list_proc_block), \
				   group_proc_statement) );
	dprintf(buf, POSTAMBLE);
	break;
    } /* End switch */

    lptr = LOOKUP_SUB(lptr, group_list_next);
  } /* End while */

  if (verbosity & VERBOSE_INTERMEDIATE) 
    fprintf(stderr, "Tree was: %s\n", buf->ch[0].cptr);
#if 0
  if (texout) {
    dprintf(out, "%s", buf->ch[0].cptr);
  } else {
    tree_main(buf->ch[0].cptr, out);
  }
#endif
  free_output(buf);
}  

OUTPUT *dump_makestream(FILE *stream) {
  OUTPUT *ptr;

  if (!stream) 
    return 0;

  ptr = NEW(OUTPUT);
  ptr->form = output_filestream;
  ptr->ch[0].file = stream;
  return ptr;
}
  
OUTPUT *dump_makebuffer(int size) {
  OUTPUT *ptr;
  char *buf;
  
  buf = malloc( sizeof(char)*size );
  if (!buf) 
    return NULL;

  ptr = NEW(OUTPUT);
  ptr->form = output_buffer;
  ptr->ch[0].cptr = buf;
  ptr->ch[1].integer = size;
  ptr->ch[2].cptr = buf;
  return ptr;
}

void free_output(OUTPUT *me) {
  if (!me) 
    return;

  switch (me->form) {
    case output_buffer:
      free(me->ch[0].cptr);
      break;

    case output_filestream:
      fclose(me->ch[0].file);
  }
  free(me);
}
    
void dprintf(OUTPUT *out, const char *fmt, ...) {
  va_list ap;

  if (!out) { 
    fprintf(stderr, "Warning; Attempt to output to a null object\n");
    return;
  }

  va_start(ap, fmt);
  switch (out->form) {
    case output_filestream:
      vfprintf(out->ch[0].file, fmt, ap);
      break;

    case output_buffer: {
	int n, r, flag;
	/* n is amount of spare space */
	flag = 0;
	
	do {
	  n = out->ch[2].cptr - out->ch[0].cptr;
	  n = out->ch[1].integer - n; 
	  
	  r = vsnprintf(out->ch[2].cptr, n, fmt, ap);

	  if (r<n && r>=0) {
	    flag = 1;
	  } else {
	    int offset;
	    char *old;

	    /* buffer full, so double it */
	    offset = (out->ch[2].cptr) - (out->ch[0].cptr);
	    old = out->ch[0].cptr;
	  
	    (out->ch[0].cptr) = malloc(sizeof(char)*2*(out->ch[1].integer));
	    strcpy(out->ch[0].cptr, old);	  
	    (out->ch[1].integer) *= 2;
	    (out->ch[2].cptr) = (out->ch[0].cptr) + offset;
	  }
	} while (!flag);
	/* and move on the pointer */

	(out->ch[2].cptr) += r;
	break;
      }
  }
  va_end(ap);
}

int input_open(char *name) {
  FILE *file;
  int i;
  char filename[256];
  file = 0;
  i=0;
  while (!file && i < includetablesize) {
      sprintf(filename, "%s/%s", includetable[i], name);

      file = fopen(filename, "r");
      i++;
  }

  if (!file) {
      fprintf(stderr,"Could not open %s\n", name);
      return 1;
  }


  instackptr++;
  instack = malloc(INPUT_STACK_SIZE*sizeof(INPUT));
  instack[instackptr].file = file;
  strcpy(instack[instackptr].name, name);
  instack[instackptr].line = 1;
  instack[instackptr].lexdata = 0;
  lex_begin();
  if (deps) {
      dprintf(deps, "%s ", filename);
  }
  return 0;
}

void input_close(void) {
  fclose(instack[instackptr].file);
  instackptr--;
  lex_end();
}

#undef malloc
#undef free
#define MALLOC_LIMIT 16384
void *malloc_table[MALLOC_LIMIT] = {0,}; 
int malloc_index = 0;

void* debugging_malloc(unsigned long s) {
    int *x;
    x = malloc(s+(2*sizeof(int)));
    x[0] = malloc_index;
    malloc_table[malloc_index] = x;
    malloc_index ++;
    if (malloc_index == MALLOC_LIMIT) {
	fprintf(stderr,"Debugging malloc buffer overflowed, please fix cluless.c MALLOC_LIMIT\n");
	exit(1);
    }
    return x+2;
}

void debugging_free(void *ptr) {
#if 0
    int *iptr = (int *) ptr;
    
    malloc_table[*(iptr-2)] = 0;
    free(ptr);
#endif
}

int debugging_get_highwater(void) {
    return malloc_index;
}

void debugging_pull_plug(int index) {
    int i;
#if 0
    fprintf(stderr,"%d to %d\n", index, malloc_index);
#endif
    for (i=index; i<malloc_index; i++) {
	if (malloc_table[i]) {
	    free(malloc_table[i]);
	}
    }
    malloc_index = index;
}
