%{
/*
 *	descrip.y
 *	---------
 *
 * $Id: descrip.y 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * Copyright (c) 1996 University of Cambridge Computer Laboratory
 * Copyright (c) 1996 Richard Black
 * Right to use is administered by University of Cambridge Computer Laboratory
 *
 */

#include <stdio.h>
#include <string.h>
#include "descrip.h"

static void yyerror(char *s);

/* XXX - we appear to need both of the following for things to work */
#define YYERROR_VERBOSE
#define YYDEBUG 1

#define TRC(x)
%}

%token TK_NUM
%token TK_STRING
%token TK_TIME
%token TK_PATH
%token TK_NTSC
%token TK_MODULE
%token TK_BLOB
%token TK_SIZEDBLOB
%token TK_ALIGN
%token TK_NAMESPACE
%token TK_PROGRAM
%token TK_PRIMAL
%token TK_END
%token TK_SIZE

/* dont think i need any lefts 
%left '+' '-'
%left '*' DIV MOD
%left SQR
*/

%union
{
    int			  num;
    char		* string;
    nanoseconds		  time;
    long		  size;
    struct params	* params;
    struct simpleprog	* program;
    struct simplemod	* mod;
    struct simplename	* name;
    struct namelist	* namelist;
    struct modlist	* modlist;
    struct bloblist	* bloblist;
    struct simpleblob	* blob;
    struct ntsc		* ntsc;
    struct proglist	* proglist;
    struct nameslist	* nameslist;
}

/* try not to get confused between name, names, namelist and nameslist */
%type <num>		TK_NUM
%type <size>		TK_SIZE
%type <string>		TK_STRING TK_PATH path
%type <time>		TK_TIME
%type <params>		params
%type <nameslist>	nameslist
%type <program>		prog
%type <mod>		mod
%type <name>		name
%type <namelist>	namelist names
%type <modlist>		modlist
%type <blob>		blob
%type <bloblist>	bloblist
%type <ntsc>		ntsc
%type <proglist>	proglist

%%
/* ------------------------------------------------------------ */

image:	ntsc modlist bloblist nameslist proglist TK_END	{ make_image($1,$2,$4,$5); }


ntsc:	TK_NTSC path	{ 
    /* fprintf() needs to be before parse_ntsc() so if parse fails,
     * doesn't try to print newly free()ed pointer */
  fprintf(depend_output_file, " %s", $2);
  $$ = parse_ntsc($2); 
}
	

path:	  TK_STRING	{ $$ = $1; }
	| TK_PATH	{ $$ = $1; }

modlist:  		{ $$ = NULL; }
	| modlist mod	{ 
	  $$ = parse_module($1, $2); 
	}
	

bloblist:		{ $$ = NULL; }
	| bloblist blob {
	  $$ = parse_blob($1, $2);
	}

nameslist: 	        { $$ = NULL; }
	| nameslist names { 
	  $$ = parse_names($1, $2); 
	  TRC(fprintf(stderr,"Parsed namespace\n"));
	}
	;

proglist: 		{ $$ = NULL; }
	| proglist prog { 
	  $$ = parse_program($1, $2); 
	  TRC(fprintf(stderr,"Parsed program\n"));
	}
	;

params: TK_TIME TK_TIME TK_TIME TK_SIZE TK_SIZE TK_SIZE TK_NUM TK_NUM TK_NUM
	{
		$$ = xmalloc(sizeof(struct params));
		$$->p       = $1;
		$$->s       = $2;
		$$->l       = $3;
		$$->flags   = NULL;
		$$->astr    = $4;
		$$->stack   = $5;
		$$->heap    = $6;
		$$->nctxts  = $7;
		$$->neps    = $8;
		$$->nframes = $9;
	}
      | TK_TIME TK_TIME TK_TIME TK_STRING TK_SIZE TK_SIZE TK_SIZE TK_NUM TK_NUM TK_NUM
        {
		$$ = xmalloc(sizeof(struct params));
		$$->p       = $1;
		$$->s       = $2;
		$$->l       = $3;
		$$->flags   = str2flags($4);
		free($4);
		$$->astr    = $5;
		$$->stack   = $6;
		$$->heap    = $7;
		$$->nctxts  = $8;
		$$->neps    = $9;
		$$->nframes = $10;
	}
	;

prog:	TK_PROGRAM path TK_STRING TK_STRING params
	{ 
		$$ = xmalloc(sizeof(struct simpleprog));
		$$->is_primal = False;
		$$->binary = $2;
		$$->name= strdup($3); /* XXX smh22 hacking */
		$$->names = $4;
		$$->params = $5;
		fprintf(depend_output_file," %s", $2);
	}
        | TK_PRIMAL path TK_STRING params
	{ 
		$$ = xmalloc(sizeof(struct simpleprog));
		$$->is_primal = True;
		$$->binary = $2;
		$$->name= strdup("Primal"); /* XXX smh22 hacking */
		$$->names = $3;
		$$->params = $4;
		fprintf(depend_output_file," %s", $2);
	}
	;

mod:	TK_MODULE TK_STRING '=' path
	{
		$$ = xmalloc(sizeof(struct simplemod));
		$$->name = $2;
		$$->path = $4;
		fprintf(depend_output_file, " %s", $4);
	}
	;

blob:	TK_BLOB TK_STRING '=' path
	{
		$$ = xmalloc(sizeof(struct simpleblob));
		$$->name = $2;
		$$->path = $4;
		$$->align = 3;  /* by default, align to 8-byte boundary */
		fprintf(depend_output_file, " %s", $4);
	}
	|
	TK_BLOB TK_STRING '=' path TK_ALIGN TK_NUM
	{
		$$ = xmalloc(sizeof(struct simpleblob));
		$$->name = $2;
		$$->path = $4;
		$$->align = $6;
		fprintf(depend_output_file, " %s", $4);
	}
	;

names:	TK_NAMESPACE TK_STRING '=' '{' namelist '}'
	{
		$$ = $5;
		$$->bnl_name = $2;
	}
	;

namelist: 		
        { 
	        $$ = xmalloc(sizeof(struct namelist));	
	        memset($$, 0, sizeof(struct namelist) );	
	}
	| namelist name	{ $$ = parse_name($1, $2); }
	;

name:	  '"' path '"' '=' TK_BLOB TK_STRING
	{
		$$ = xmalloc(sizeof(struct simplename));
		$$->path = $2;
		$$->mod = $6;
		$$->symbol = NULL;
		$$->type = NAME_TYPE_BLOB;
	}
	| '"' path '"' '=' TK_STRING '$' TK_STRING
	{
		$$ = xmalloc(sizeof(struct simplename));
		$$->path = $2;
		$$->mod = $5;
		$$->symbol = $7;
		$$->type = NAME_TYPE_SYMBOL;
	}
	;

%%
/* ------------------------------------------------------------ */

#include "descriplex.c"

extern char *parser_input_file_name;

static void yyerror(char *s)
{
    printf("%s:%d: error: %s\n", parser_input_file_name, yylineno, s);
}

/* End of descrip.y */
