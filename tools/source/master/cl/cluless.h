/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory          *
 * All Rights Reserved                                                 *
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)                     *
 *          Richard Mortier (rmm1002@cam.ac.uk)                        *
 ***********************************************************************/ 

/* CLUless 
 *
 * cluless.h; misc stuff 
 *
 * Dickon Reed, 1996
 * Richard Mortier, 1997
 *
 *
 * $Id: cluless.h 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 *
 *
 * $Log: cluless.h,v $
 * Revision 1.3  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.2  1998/04/14 14:15:41  dr10009
 * Multiple interfaces at once
 *
 * Revision 1.1  1997/06/25 17:06:44  dr10009
 * Initial revision
 *
 * Revision 2.1  1997/04/04 15:34:36  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.12  1997/03/11 22:33:05  rmm1002
 * Fixed TR macro
 *
 * Revision 1.11  1997/02/26 00:11:01  rmm1002
 * Reordered stuff
 *
 * Revision 1.10  1997/02/04 16:48:30  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 17:44:37  rmm1002
 * Initial revision
 *
 */

#ifndef __CLULESS_H__
#define __CLULESS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Definitions */
#define INPUT_STACK_SIZE	50
#define SUFFIX_TABLE_SIZE	16
#define INCLUDE_TABLE_SIZE	64

#define VERBOSE_SYMTAB		1
#define VERBOSE_CREATE		2
#define VERBOSE_INTERMEDIATE	4
#define VERBOSE_GEN		8
#define VERBOSE_INCLUDE		16

#define PREAMBLE		"\\tree (\n"
#define POSTAMBLE		"\n)\n\\bigskip\n"

/* Macros */
#define NEW(x)			(x *) (malloc(sizeof(x)))

#define malloc(_x)              debugging_malloc(_x)
#define free(_x)                debugging_free(_x)

void *debugging_malloc(unsigned long s);
void debugging_free(void *ptr);
int debugging_get_hightwater(void);
void debugging_pull_plug(int index);

#ifdef DEBUG
#  define TR(a)			a
#else
#  define TR(a)			
#endif

/* Typedefs */
typedef struct {
  FILE  *file;
  char  name[256];
  int   line;
  void  *lexdata;
} INPUT;

typedef struct {
  int form;
  union {
    FILE  *file;
    char  *cptr;
    int   integer;
  } ch[3];
} OUTPUT;

/* Global variables */
extern OUTPUT	*messages;
extern OUTPUT	*code;
extern INPUT	instack[];
extern int	instackptr;

extern int	verbosity;

enum parser_mode {
  pm_regular,	/* top level file */
  pm_extends,	/* entered through an extends clause */
  pm_needs,	/* entered through a need clause */
  pm_interface	/* interface for this implementation; add declarations
		   into scope */
};
extern enum parser_mode pm;	/* parser FSM support */

extern char	*suffixtable[];
extern char	*includetable[];
extern int	suffixtablesize;
extern int	includetablesize;

extern char	basefilename[];

extern OUTPUT *predecl;
extern OUTPUT *postdecl;

/* Prototypes */
/* Parser routines */
int	yyparse(void);

void	got_spec(void *tree);
void	got_expression(void *tree);

/* Dump routines */
void	dump_spec(OUTPUT *out, void *spec);
OUTPUT	*dump_makestream(FILE *stream);
OUTPUT	*dump_makebuffer(int size);

/* File routines */
void	free_output(OUTPUT *me);
#define dprintf dump_printf

void	dprintf(OUTPUT *out, const char *fmt, ...);
int	input_file(char *name);
void	input_close(void);

int	tree_main(char *input, OUTPUT *output);

/* Lexer interface */
void	lex_begin(void);
void	lex_end(void);

void symtab_init(void);
#include "stackobj.h"
#include "dtree.h"
#include "typesupport.h"
void yyerror(const char *str);
int input_open(char *name);
int debugging_get_highwater(void);
#include "dast.h"
#endif /*  __CLULESS_H__*/
