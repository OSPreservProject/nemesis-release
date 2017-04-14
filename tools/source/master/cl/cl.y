/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory
 * All Rights Reserved
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)
 *          Richard Mortier (rmm1002@cam.ac.uk)
 *********************************************************************** 
 * cl.y; CLUless grammar for Bison
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997
 * 
 * $Id: cl.y 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $ 
 *
 * $Log: cl.y,v $
 * Revision 1.5  1998/10/01 09:45:18  dr10009
 * detect broken choices
 *
 * Revision 1.4  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.3  1998/03/13 22:17:02  dr10009
 * *** empty log message ***
 *
 * Revision 1.2  1997/06/25 17:16:33  dr10009
 * *** empty log message ***
 *
 * Revision 1.1  1997/06/25 17:06:44  dr10009
 * Initial revision
 *
 * Revision 2.5  1997/04/12 13:13:47  rmm1002
 * Removed refs to make_predeftype...
 *
 * Revision 2.4  1997/04/10 22:59:36  rmm1002
 * Concat'd NUMBER, FLOAT, etc into CONSTANT (incl. char/strings)
 *
 * Revision 2.3  1997/04/09 14:48:28  rmm1002
 * More fiddling with attach_list args, to get lists to be in the correct order
 *
 * Revision 2.2  1997/04/07 20:51:25  rmm1002
 * More fiddling. Removed Dickon's comments, ready for my own (!)
 * Fixed the cast warning by making it explicit; no idea why this should work.
 * c.f. XXX ...
 *
 * Revision 2.1  1997/04/04 15:34:16  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.20  1997/04/04 15:26:31  rmm1002
 * Much fiddling with ';' placement--should now be ok. Also fixed s/r conflicts
 * with IF..THEN..ELSE and exceptions for good (I think...)
 *
 * Revision 1.19  1997/04/02 20:23:05  rmm1002
 * Fixed handling of ';'s (no longer need (eg) WHILE (1) DO BEGIN WIBBLE; END;;
 * with ;; at end...
 * However, this added s/r conflict with IF-THEN-ELSE again.
 *
 * Revision 1.18  1997/04/02 14:07:13  rmm1002
 * Reinstated THEN in IF-ELSE
 *
 * Revision 1.17  1997/03/21 12:32:20  rmm1002
 * *** empty log message ***
 *
 * Revision 1.16  1997/03/19 21:54:31  rmm1002
 * Reverted a lot of LR <-> RR changes; finished C_BLCOK stuff
 * Fixed all S/R conflicts
 *
 * Revision 1.15  1997/03/11 21:06:33  rmm1002
 * S/R conflicts down to 1; added code to handle C_BLOCKs (doesn't work yet)
 *
 * Revision 1.14  1997/03/07 16:28:30  rmm1002
 * Modifications ready to add C_BLOCK facility;
 * removed "Sugar" secion -- have to use BEGIN/END and := now,
 * since {/} would be too confusing re. C_BLOCK
 *
 * Revision 1.13  1997/02/26 00:08:25  rmm1002
 * Rationalized got_spec / got_interface / got_implementation
 *
 * Revision 1.12  1997/02/20 00:26:42  rmm1002
 * Reduced S/R conflicts from 16 to 2
 * Still messing with recursion (left/right).
 *
 * Revision 1.11  1997/02/10 19:41:47  rmm1002
 * More minor alterations, incl. changing some definitions which appeared
 * to be left-recursive
 *
 * Revision 1.10  1997/02/04 16:44:14  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.2  1997/02/03 17:06:14  rmm1002
 * Commented out unary ops to find shift-reduce conflicts
 *
 * Revision 1.1  1997/01/30 21:24:52  rmm1002
 * Initial revision
 *
 */

%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cluless.h"
#include "stackobj.h"
#include "parsupport.h"
#include "expr.h"
#include "statement.h"
#include "list.h"
#include "spec.h"
#include "decl.h"
#include "type.h"
#include "symtab.h"
#include "group.h"
#include "dpython.h"

#define YYERROR_VERBOSE

#define YYSTYPE p_obj * /* Type used for semantic values */

void yyerror(const char *str);
int yylex(YYSTYPE *lvalp);
%}

%token CONSTANT
%token IDENTIFIER
%token C_BLOCK

/* Module level stuff */
%token INTERFACE
%token MODULE
%token CLU_BEGIN
%token CLU_END
%token DANGEROUS
%token LOCAL

/* Type related stuff */
%token ANY
%token TYPE
%token BOOLEAN
%token SHORT
%token CARDINAL
%token LONG
%token INTEGER
%token REAL
%token OCTET
%token CHAR
%token STRING
%token ADDRESS
%token WORD
%token ARRAY
%token SET
%token OF
%token SEQUENCE
%token RECORD
%token CHOICE
%token REF
%token IREF
%token METHOD
%token FROM
%token TO
%token BITS
%token TUPLE

/* Declaration related stuff */
%token PROC
%token NEVER
%token RETURNS
%token IN
%token OUT
%token ANNOUNCEMENT
%token EXCEPTION
%token RAISES
%token EXTENDS
%token NEEDS

/* Statement related stuff */
%token WIBBLE
%token RAISE
%token TRY
%token EXCEPT
%token FINALLY
%token LOOP
%token WHILE
%token DO
%token REPEAT
%token UNTIL
%token ELSE
%token EXIT
%token RETURN
%token IF
%token THEN

/* Boolean values */
%token TRUE
%token FALSE

/* Misc operators */
%token DARROW
%token ARROW
%token ASS

%token LE
%token GE
%token NE
%token EQ

%token PVS

%start spec_list
/* Define operator precedences; cf Bison Info file -- Node: Precedence Example
 * and Node: Contextual Precedence
 * 
 * DUMMY is a dummy precedence to fix shift-reduce conflicts where e.g. 
 * <delta> is on stack, and '*' <gamma> is coming -- either <delta> can reduce 
 * to <epsilon>, and stuff what's to come (make it a <gamma>), or the '*' (or
 * other operator) can shift, and we then get a <gamma> (or whatever) and 
 * reduce to <delta> using <delta> -> <delta> '*' <gamma>... Consequently,
 * the DUMMY precedence goes with the *next* highest operator (in this case
 * with the <epsilon> rule -- we wish to make this reduction lower priority
 * than the one involving '*' <gamma>.
 *
 * We need two DUMMY precedences to resolve the (effectively)
 * so-called ``dangling-else'' conflict (apparently *NOT* solved by
 * the ``solution'' in \cite{RedDragon}).  
 */
%nonassoc DUMMY
%left '<' '>' EQ NE LE GE
%left '+' '-'
%left '*' '/'
%left UPLUS UMINUS
%left USTAR '&'
%left '('

%pure_parser
%%

/* -- 
\section{Specifications}
\begin{verbatim} */
spec_list:
		/* --(empty) */ 
			{ 
			  $$ = (p_list *)0; 
			}
                | spec spec_list 
			{ 
			  $$ = make_obj_f( group_list, list_spec, 
					   list_spec, $1, 
					   group_list_next, $2,
					   OBJE );
			}
                ;
spec:
		IDENTIFIER ':' LOCAL INTERFACE '=' extend_list need_list
			{
			  symtab_new_decl( make_obj_f( group_decl, decl_spec,
						       group_decl_name, $1,
						       group_decl_type, 0,
						       decl_spec_spec, 0,
						       OBJE ));
			  follow_interfaces($6, pm_extends);
			  follow_interfaces($7, pm_needs);        
			}
			CLU_BEGIN middl_decl_list CLU_END '.'
			{
			  p_decl *decl;

			  $$ = make_obj_f( group_spec, spec_local_interface,
					   group_spec_name, $1,
					   group_spec_extends, $6,
					   group_spec_needs, $7,
					   group_spec_body, $10,
					   OBJE );
#if 0
			  fprintf(stderr,"%x: Intf extends is %x\n", $$, LOOKUP_SUB($$, group_spec_extends));
#endif
			  decl = symtab_lookup_decl($1, (1<<decl_spec));
			  if (!decl) {
			    yyerror("Dropped specification declaration");
			  } else {
			    LOOKUP_SUB(decl, decl_spec_spec) = $$;
			  } 
			  got_spec( $$ );
			} /* got interface */
		| IDENTIFIER ':' INTERFACE '=' extend_list need_list
			{
			  symtab_new_decl( make_obj_f( group_decl,decl_spec,
						       group_decl_name, $1,
						       group_decl_type, 0,
						       decl_spec_spec, 0,
						       OBJE ));
			  follow_interfaces($5, pm_extends);
			  follow_interfaces($6, pm_needs);
			}
			CLU_BEGIN middl_decl_list CLU_END '.' 
			{
			  p_decl *decl;

			  $$ = make_obj_f( group_spec, spec_interface,
					   group_spec_name, $1,
					   group_spec_extends, $5,
					   group_spec_needs, $6,
					   group_spec_body, $9,
					   OBJE );
			  decl = symtab_lookup_decl($1, (1<<decl_spec));
			  if (!decl) {
			    yyerror("Dropped specification declaration");
			  } else {
			    LOOKUP_SUB(decl, decl_spec_spec) = $$;
			  }
			  got_spec( $$ );
			} /* or module */
		| IDENTIFIER ':'
			{ 
			  follow_interfaces($1, pm_interface); 
			}
			MODULE '=' CLU_BEGIN block_list CLU_END '.' 
			{
			  $$ = make_obj_f( group_spec, spec_implementation,
					   group_spec_name, $1,
					   group_spec_body, $7 );
			  got_spec( $$ );
			}
                ;
extend_list:            
                /* --(empty) */ 
			{ 
			  $$ = 0; 
			}
                | EXTENDS IDENTIFIER ';'
			{ /* Yes, only allow 1 extends... */
			  $$ = make_obj_f( group_list, list_id, 
					   list_id_id, $2, 
					   group_list_next, 0, 
					   OBJE ); 
			}
                ;
need_list:      
                /* --(empty) */ 
			{ 
			  $$ = 0; 
 			}
                | NEEDS IDENTIFIER ';' need_list
			{ 
			  $$ = make_obj_f( group_list, list_id,
					   list_id_id, $2,
					   group_list_next, $4,
					   OBJE ); 
			}
                ;
/*--
\end{verbatim}
\section{Blocks and declarations}
\begin{verbatim} */
block_list:     
                /* --(empty) */ 
			{ 
			  $$ = 0; 
			} 
                | block block_list  
			{ 
			  $$ = make_obj_f( group_list, list_proc,
					   list_proc_block, $1,
					   group_list_next, $2, 
					   OBJE ); 
                        }
                | cl_decl block_list
			{ 
			  $$ = make_obj_f( group_list, list_decl, 
					   list_decl_decl, $1, 
					   group_list_next, $2,
					   OBJE ); 
                        }
                | error block_list
			{ 
			  $$ = $2; 
			} 
                ;
middl_decl_list:
                /* --(empty) */ 
			{ 
			  $$ = 0; 
			}
                | error middl_decl_list
			{ 
			  $$ = $2;
			}
                | middl_decl middl_decl_list
                        { 
			  $$ = make_obj_f( group_list, list_decl, 
					   list_decl_decl, $1, 
					   group_list_next, $2, 
					   OBJE ); 
			  if (pm == pm_interface)
			    symtab_new_decl( $1 );
                        }
                ;
middl_decl:     
                type_decl ';' 
			{ 
			  $$ = $1; 
			}
                | exc_decl ';' 
			{ 
			  $$ = $1; 
			}
                | op_decl ';'  
			{ 
			  $$ = $1; 
			}
                ;
cl_decl:        
                type_decl ';' 
			{ 
			  $$ = $1; 
			}
                | var_decl ';' 
			{ 
			  $$ = $1; 
			}
                ;
int_decl:       
                type_decl ';' 
			{ 
			  $$ = $1; 
			}
                | var_decl ';' 
			{ 
			  $$ = $1; 
			}
                ;
op_decl: 
		ann_decl
			{ 
			  $$ = $1; 
			}
		| proc_decl
			{ 
			  $$ = $1; 
			}
		;
ann_decl:
		IDENTIFIER ':' ANNOUNCEMENT '[' arg_list ']'  
			{ 
			  $$ = make_obj_f( group_decl, decl_announcement,
					   group_decl_type, 
			         make_obj_f( group_type, type_method,
					     type_method_args, $5,
					     type_method_rets, 0,
					     OBJE ),
					   group_decl_name, $1,
					   OBJE ); 
			  symtab_new_decl( $$ );
			}
                ;
exc_decl:       
		IDENTIFIER ':' EXCEPTION '[' arg_list ']' 
			{ 
			  $$ = make_obj_f( group_decl, decl_exception,
					   group_decl_type, 0,
					   group_decl_name, $1,
					   decl_exception_list, $5,
					   OBJE ); 
			  symtab_new_decl( $$ );
			}
                ;
type_decl:      
		IDENTIFIER ':' TYPE '=' type   
			{ 
			  $$ = make_obj_f( group_decl, decl_type,
					   group_decl_name, $1,
					   group_decl_type, $5,
					   OBJE ); 
			  symtab_new_decl( $$ );
			}
                ;
var_decl:       
		IDENTIFIER ':' type 
			{ 
			  $$ = make_obj_f( group_decl, decl_var,
					   group_decl_name, $1,
					   group_decl_type, $3,
					   OBJE ); 
			  symtab_new_decl( $$ );
			}
                ;
proc_decl:    
                IDENTIFIER ':' PROC '[' arg_list ']' 
			       RETURNS '[' res_list ']' raises 
			{
			  p_type *type;

			  type = make_obj_f( group_type, type_method,
					     type_method_args, $5,
					     type_method_rets, $9,
					     OBJE );
			  $$ = make_obj_f( group_decl, decl_procedure,
					   decl_procedure_raises, $11,
					   group_decl_name, $1,
					   group_decl_type, type,
					   OBJE );
			  symtab_new_decl( $$ );
			}
                | IDENTIFIER ':' PROC '[' arg_list ']' 
				 NEVER RETURNS raises 
			{
			  p_type *type;

			  type = make_obj_f( group_type, type_method,
					     type_method_args, $5,
					     type_method_rets, 0,
					     OBJE );

			  $$ = make_obj_f( group_decl, decl_procedure_nr,
					   decl_procedure_raises, $9,
					   group_decl_name, $1,
					   group_decl_type, type,
					   OBJE);
			  symtab_new_decl( $$ ); 
			}
		;
arg_list:       
		/* --(empty) */ 
			{ 
			  $$ = 0; 
			}
                | arg 
			{ 
			  $$ = $1;
			}
                | arg ',' arg_list
			{ 
			  attach_list($1, $3); 
			  $$ = $1;
			}
                | arg ';' arg_list
			{ 
			  attach_list($1, $3); 
			  $$ = $1;
			} /* for compatability */
                | error arg_list
			{
			  $$ = $2;
			}
                ;
arg:            
                field 
			{
			  $$ = $1;  
			  setmode_list($$, decl_invar); 
			  /* arguments have mode IN by default */
                        }
                | IN field 
			{
			  $$ = $2;
			  setmode_list($$, decl_invar);
                        }
                | OUT field 
			{
			  $$ = $2;
			  setmode_list($$, decl_outvar);
                        }
                | IN OUT field 
			{
			  $$ = $3;
			  setmode_list($$, decl_inoutvar);
                        } 
                ;

res_list:       
                /* --(empty) */ 
			{ 
			  $$ = 0; 
			}
                | field
			{ 
			  setmode_list($1, decl_outvar);
			  $$ = $1; 
			}
                | field ',' res_list 
			{ 
			  setmode_list($1, decl_outvar);
			  attach_list($1, $3);
			  $$ = $1;
                        }
                | field ';' res_list 
			{ 
			  setmode_list($1, decl_outvar);
			  attach_list($1, $3); 
			  $$ = $1; 
                        }
                | error res_list
			{
			  $$ = $2;
			}
                ;
field_list:     
                field 
			{ 
			  $$ = $1; 
			}
                | field ',' field_list 
			{
			  attach_list($1, $3);
			  $$ = $1;
                        }
                | error field_list
			{
			  $$ = $2;
			}
                ;
field:          
		ident_list ':' type 
			{
			  augment_identlist($1, $3);
			  $$ = $1;
			}
                ;
ident_list:     
                IDENTIFIER 
			{ 
			  $$ = make_obj_f( group_list, list_id,
					   group_list_g1, $1,
					   group_list_next, 0,
					   OBJE ); 
			}
                | IDENTIFIER ',' ident_list
			{ 
			  $$ = make_obj_f( group_list, list_id,
					   group_list_g1, $1,
					   group_list_next, $3,
					   OBJE ); 
			}
                ;
exc_list:       
                exc 
			{ 
			  $$ = make_obj_f( group_list, list_sco,
					   group_list_g1, $1,
					   group_list_next, 0,
					   OBJE );
			} 
                | exc ',' exc_list
			{ 
			  $$ = make_obj_f( group_list, list_sco,
					   group_list_g1, $1, 
					   group_list_next, $3,
					   OBJE ); 
			}
                ;
exc:            
                scoped_ident 
			{ 
			  $$ = $1; 
			}
                | IDENTIFIER 
			{
			  $$ = make_obj_f( group_sco, 0, 
					   group_sco_scope, 0,
					   group_sco_name, $1,
					   OBJE );
                        }
                ;
scoped_ident:   
		IDENTIFIER '.' IDENTIFIER 
			{
			  $$ = make_obj_f( group_sco, 0,
					   group_sco_scope, $1,
					   group_sco_name, $3,
					   OBJE );
                        }
                ;
raises:         
                /* --(empty) */ 
			{ 
			  $$ = 0; 
			}
                | RAISES exc_list 
			{ 
			  $$ = $2; 
			}
                ;
/*--
\end{verbatim}
\section{Types}
\begin{verbatim} */
type:           
                predef_type 
			{ 
			  $$ = $1; 
			}
                | cons_type 
			{ 
			  $$ = $1; 
			}
                | ref_type  
			{ 
			  $$ = $1; 
			}
                | iref_type 
			{ 
			  $$ = $1; 
			}
                | named_type 
			{ 
			  $$ = $1; 
			}
                ;
predef_type:    
                BOOLEAN 
			{ 
			  $$ = make_obj_f( group_type, type_boolean,
					   group_type_oname, 
					   "type_MIDDL_Boolean",
					   OBJE ); 
			}
                | SHORT CARDINAL 
			{
			  $$ = make_obj_f( group_type, type_short_cardinal,
					   group_type_oname, 
					   "type_MIDDL_ShortCardinal",
					   OBJE ); 
			}
                | CARDINAL 
			{ 
			  $$ = make_obj_f( group_type, type_cardinal,
					   group_type_oname, 
					   "type_MIDDL_Cardinal",
					   OBJE ); 
			}
                | LONG CARDINAL 
			{ 
			  $$ = make_obj_f( group_type, type_long_cardinal,
					   group_type_oname, 
					   "type_MIDDL_LongCardinal",
					   OBJE ); 
			}
                | SHORT INTEGER 
			{ 
			  $$ = make_obj_f( group_type, type_short_integer,
					   group_type_oname, 
					   "type_MIDDL_ShortInteger", 
					   OBJE ); 
			}
                | INTEGER 
			{ 
			  $$ = make_obj_f( group_type, type_integer,
					   group_type_oname, 
					   "type_MIDDL_Integer", 
					   OBJE ); 
			}
                | LONG INTEGER 
			{ 
			  $$ = make_obj_f( group_type, type_long_integer,
					   group_type_oname, 
					   "type_MIDDL_LongInteger", 
					   OBJE ); 
			}
                | REAL 
			{ 
			  $$ = make_obj_f( group_type, type_real,
					   group_type_oname, 
					   "type_MIDDL_Real", 
					   OBJE ); 
			}
                | LONG REAL 
			{ 
			  $$ = make_obj_f( group_type, type_long_real,
					   group_type_oname, 
					   "type_MIDDL_LongReal", 
					   OBJE ); 
			}
                | STRING 
			{ 
			  $$ = make_obj_f( group_type, type_string,
					   group_type_oname, 
					   "type_MIDDL_String", 
					   OBJE ); 
			}
                | OCTET 
			{ 
			  $$ = make_obj_f( group_type, type_octet,
					   group_type_oname,
					   "type_MIDDL_Octet", 
					   OBJE ); 
			}
                | CHAR 
			{ 
			  $$ = make_obj_f( group_type, type_char,
					   group_type_oname, 
					   "type_MIDDL_Char",
					   OBJE ); 
			}
                | DANGEROUS ADDRESS 
			{ 
			  $$ = make_obj_f( group_type, type_dangerous_address,
					   group_type_oname, 
					   "type_MIDDL_Address", 
					   OBJE ); 
			}

                | ADDRESS 
			{ 
			  $$ = make_obj_f( group_type, type_dangerous_address,
					   group_type_oname, 
					   "type_MIDDL_Address", 
					   OBJE ); 
			}
                | DANGEROUS WORD 
			{ 
			  $$ = make_obj_f( group_type, type_dangerous_word,
					   group_type_oname,
					   "type_MIDDL_Word", 
					   OBJE ); 
			} 
                | WORD 
			{ 
			  $$ = make_obj_f( group_type, type_dangerous_word,
					   group_type_oname,
					   "type_MIDDL_Word", 
					   OBJE ); 
			} 
                ;
cons_type:      
                '{' ident_list '}' 
			{ /* Enumerations, of a sort */
			  $$ = make_obj_f( group_type, type_enum,
					   type_enum_list, $2,
					   OBJE );  
                        } 
                | ARRAY expression OF type 
			{
			  $$ = make_obj_f( group_type, type_array,
					   type_array_type, $4,
					   type_array_size, $2,
					   OBJE );
			}
                | ARRAY expression OF BITS  
			{
			  $$ = make_obj_f( group_type, type_bitarray,
					   type_bitarray_size, $2,
					   OBJE );
			}
                | SET OF type 
			{ 
			  $$ = make_obj_f( group_type, type_set,
					   type_set_type, $3,
					   OBJE ); 
			}
                | SEQUENCE OF type 
			{ 
			  $$ = make_obj_f( group_type, type_sequence,
					   type_sequence_type, $3,
					   OBJE ); 
			} 
                | RECORD '[' field_list ']' 
			{ 
			  $$ = make_obj_f( group_type, type_record,
					   type_record_list, $3,
					   OBJE );
			}
                | TUPLE '[' type_list ']' 
			{
			  $$ = make_obj_f( group_type, type_tuple,
					   type_tuple_list, $3,
					   OBJE );
			}
                | CHOICE type OF '{' candidate_list '}'
			{
			  p_obj *enum_list, *cand_list, *candidate;
			  
			  $$ = make_obj_f( group_type, type_choice,
					   type_choice_type, $2,
					   type_choice_list, $5,
					   OBJE );
			  
			  if ($2->cl != group_type || $2->form != type_enum) {
			      dprintf(messages,"Error; choice type must be an enumeration\n");
			      YYERROR;
			  }
			  
			  enum_list = LOOKUP_SUB($2, type_enum_list);
			  cand_list = $5;
			  
			  while (cand_list) {
			      char *name;
			      p_obj *candptr;
			      p_obj *enumptr;
			      int hit;
			      candidate = LOOKUP_SUB(cand_list, group_list_g1);
			      
			      name = LOOKUP_ID(candidate, group_decl_name);
			      
			      candptr = $5;
			      while (candptr != cand_list) {
				  if (!strcmp(name, LOOKUP_ID(LOOKUP_SUB(candptr, group_list_g1), group_decl_name))) {
				      dprintf(messages,"Error; candidate name %s occurs more than once in a choice\n", name);
				      yyerror("Candidate name duplication illegal");
				      YYERROR;
				  }
				  candptr = LOOKUP_SUB(candptr, group_list_next);
			      }
			      enumptr = LOOKUP_SUB($2, type_enum_list);
			      
			      hit = 0;
			      while (enumptr) {
				  if (!strcmp(name, LOOKUP_STRING(LOOKUP_SUB(enumptr, group_list_g1), group_identifier_string))) {
				      hit = 1;
				  }
				  enumptr = LOOKUP_SUB(enumptr, group_list_next);
			      }
			      if (!hit) {
				  dprintf(messages,"Choice candidate %s not present in enumeration type\n", name);
				  yyerror("Candidate name not present in enumeration");
				  YYERROR;
			      }
			      cand_list = LOOKUP_SUB(cand_list, group_list_next);
			  }

			}
                | METHOD FROM '[' arg_list ']' TO '[' res_list ']' 
			{
			  $$ = make_obj_f( group_type, type_method,
					   type_method_args, $4,
					   type_method_rets, $8,
					   type_method_intf, 0,
					   OBJE );
			}               
                ;
ref_type:       
		REF type 
			{ 
			  $$ = make_obj_f( group_type, type_ref,
					   type_ref_type, $2,
					   OBJE ); 
			}
                ;
iref_type:      
		IREF IDENTIFIER 
			{ 
			  char buf[128];

			  sprintf( buf,
				   "type_%s_IREF",
				   LOOKUP_STRING($2, group_identifier_string));
			  $$ = make_obj_f( group_type, type_iref,
					   type_iref_id, $2,
					   group_type_oname, strdup(buf), 
					   OBJE );
			}
                ;
named_type:
                scoped_ident 
			{  
                          p_obj *d;
			  char *targetname;

			  targetname = LOOKUP_ID($1, group_sco_name);
			  d = 0;

			  if (verbosity & 4096) dprintf(messages,"Scoped type: scope [%s] name [%s]\n", LOOKUP_ID($1, group_sco_scope), LOOKUP_ID($1, group_sco_name));
                          if (LOOKUP_ID($1, group_sco_scope)) {
			    p_obj *sp;
			    p_obj *bptr;
			    p_obj *item;

			    sp = symtab_lookup_decl(
				   LOOKUP_SUB($1, group_sco_scope), 
				   (1<<decl_spec));
			    if (verbosity & 64) 
			      dump_tree(messages,sp);
			    if (!sp) {
			      yyerror("Couldn't find interface");
			      fprintf( stderr, 
				       "Looking for interface %s\n", 
				       LOOKUP_ID($1, group_sco_scope));
			      YYERROR;
			    }
			    
			    for ( bptr = LOOKUP_SUB(LOOKUP_SUB(sp,decl_spec_spec), group_spec_body); 
				  bptr; 
				  bptr = LOOKUP_SUB(bptr, group_list_next)) {
			      item = LOOKUP_SUB(bptr, list_decl_decl);
			      if (verbosity & 64) {
				  dprintf(messages,"item is: \n");
				  dump_tree(messages, item);
				  dprintf(messages,"Compare with identifier name %s\n", LOOKUP_ID(item, group_decl_name));
			      }
			      if (item->cl == group_decl && 
				  item->form == decl_type && !strcmp(LOOKUP_ID(item, group_decl_name), targetname)) {
				d = item;
			      }
			    }
			  } else {
			    yyerror("First part of scoped identifier null\n");
			  }
			      
                          if (d) {
			    $$ = LOOKUP_SUB(d, group_decl_type);
                          } else {
			    yyerror("Couldn't find type information");
			    dprintf(messages, "For: ");
			    dump_tree(messages, $1);
			    dprintf(messages, "\n");
			    YYERROR;
                          }                       
                        } /* or be local... */
                | IDENTIFIER 
			{
                          p_obj *d;
                          
                          d = symtab_lookup_decl($1, (1<<decl_type));
                          if (d) {
                                $$ = LOOKUP_SUB(d, group_decl_type);
                          } else {
			    yyerror("Couldn't find type information");
			    dprintf(messages, "For: ");
			    dump_tree(messages, $1);
			    dprintf(messages, "\n");
			    YYERROR;
                          }                       
			}
                ;
type_list:  
		type
      			{
			  $$ = make_obj_f( group_list, list_type,
					   list_type_type, $1,
					   group_list_next, 0,
					   OBJE );
			}
		| type ',' type_list
			{

			  $$ = make_obj_f( group_list, list_type,
					   list_type_type, $1,
					   group_list_next, $3,
					   OBJE );
			}
		;
candidate_list: 
                candidate 
			{ 
			  $$ = $1; 
			}
                | candidate ',' candidate_list
			{
			  attach_list($1, $3);
			  $$ = $1;
                        }
                ;
candidate:      
		ident_list DARROW type 
			{
			  augment_identlist($1, $3);
			  $$ = $1;
			}
                ;
/*--
\end{verbatim}
\section{Procedures}
\begin{verbatim} */
block:          
		op_decl '=' 
			{
			  p_list *lptr;
			  p_obj *type;

			  type = LOOKUP_SUB($1, group_decl_type);
			  symtab_enter_scope();
			  lptr = LOOKUP_SUB(type, type_method_args);
			  while (lptr) {
			    symtab_new_decl(LOOKUP_SUB(lptr, group_list_g1));
			    lptr = LOOKUP_SUB(lptr, group_list_next);
			  }
			  if ($1->form == decl_procedure) {
			    for ( lptr = LOOKUP_SUB(type, type_method_rets); 
				  lptr; 
				  lptr = LOOKUP_SUB(lptr, group_list_next))
			      symtab_new_decl(LOOKUP_SUB(lptr, group_list_g1));
			  } 
			  /* We want to be able to use this method
			   * within this code. It'll go out of scope
			   * soon, and then be reentered.  */
			  symtab_new_decl( $1 );
                        }
			CLU_BEGIN mixed_list CLU_END ';' 
			{ 
			  $$ = make_obj_f( group_proc, 0,
					   group_proc_decl, $1,
					   group_proc_statement, $5,
					   OBJE );
			  symtab_exit_scope();
			  symtab_new_decl( $1 );
                        }
                ;
mixed_list:     
                /* --(empty) */ 
			{ 
			  $$ = 0; 
			}
                | int_decl mixed_list
			{
			  $$ = make_obj_f( group_list, list_decl,
					   list_decl_decl, $1,
					   group_list_next, $2,
					   OBJE );
                        }
                | statement mixed_list
			{ 
			  $$ = make_obj_f( group_list, list_statement,
					   list_statement_statement, $1,
					   group_list_next, $2,
					   OBJE );
                        }               
                | error ';' mixed_list
			{ 
			  $$ = $3;
			}
		;
/*--
\end{verbatim}
\section{Statements}
\begin{verbatim} */
statement:      
		matched_statement
			{ 
			  $$ = $1; 
			}
                | unmatched_statement
			{ 
			  $$ = $1; 
			}
                ;
terminal_statement:
		';' /* The empty statement */
			{
			  $$ = make_obj_f( group_statement, statement_empty,
					   OBJE );
			}
                | WIBBLE ';' 
			{ 
			  $$ = make_obj_f( group_statement, statement_wibble,
					   OBJE );  
                        }
                | RAISE exc ';'
			{
			  $$ = make_obj_f( group_statement, statement_raise,
					   statement_raise_exception, $2,
					   OBJE );
                        } 
                | EXIT ';' 
			{
                          $$ = make_obj_f( group_statement, statement_exit,
					   OBJE );
                        } 
                | RETURN ';'
			{
                          $$ = make_obj_f( group_statement, statement_return,
					   OBJE );
                        } 
                | REPEAT statement UNTIL expression ';'
			{
                          $$ = make_obj_f( group_statement, statement_repeat,
					   statement_repeat_test, $4,
					   statement_repeat_code, $2,
					   OBJE );
                        }               
                | expression ';' 
			{
                          $$ = make_obj_f( group_statement, statement_expr,
					   statement_expr_expr, $1,
					   OBJE );
                        }
                | CLU_BEGIN mixed_list CLU_END 
			{
                          $$ = make_obj_f( group_statement, statement_block,
					   statement_block_code, $2,
					   OBJE );
                        }
		| C_BLOCK
			{
			  $$ = $1;
			}
                | TRY statement EXCEPT exc_handler_list CLU_END
			{
                          $$ = make_obj_f(group_statement, statement_tryexcept,
					  statement_tryexcept_code, $2,
					  statement_tryexcept_handlers, $4,
					  OBJE );
                        }
                | TRY statement FINALLY statement CLU_END
			{
			  $$ = make_obj_f(
				 group_statement, statement_tryfinally,
				 statement_tryfinally_code, $2,
				 statement_tryfinally_handle, $4,
				 OBJE );
                        } 
		;
non_terminal_statement:
                LOOP matched_statement 
			{
                          $$ = make_obj_f( group_statement, statement_loop,
					   statement_loop_code, $2,
					   OBJE );
                        } 
                | WHILE expression DO matched_statement 
			{
                          $$ = make_obj_f( group_statement, statement_while,
					   statement_while_test, $2,
					   statement_while_code, $4,
					   OBJE );
                        }  
		;
matched_statement:
		terminal_statement
			{
			  $$ = $1;
			}
		| non_terminal_statement
			{
			  $$ = $1;
			}
                | IF expression THEN matched_statement ELSE matched_statement
			{
			  $$ = make_obj_f( 
				 group_statement, statement_if_matched,
				 statement_if_matched_test, $2,
				 statement_if_matched_truecode, $4,
				 statement_if_matched_falsecode, $6,
				 OBJE );
                        }
                ;
unmatched_statement: 
                IF expression THEN statement
			{
			  $$ = make_obj_f(
				 group_statement, statement_if_unmatched,
				 statement_if_matched_test, $2,
				 statement_if_matched_truecode, $4,
				 OBJE );
                        } 
                | IF expression THEN matched_statement ELSE unmatched_statement
			{
			  $$ = make_obj_f(
				 group_statement, statement_if_matched,
				 statement_if_matched_test, $2,
				 statement_if_matched_truecode, $4,
				 statement_if_matched_falsecode, $6,
				 OBJE );
                        }
                | LOOP unmatched_statement 
			{
                          $$ = make_obj_f( group_statement, statement_loop,
					   statement_loop_code, $2,
					   OBJE );
                        } 
                | WHILE expression DO unmatched_statement 
			{
                          $$ = make_obj_f( group_statement, statement_while,
					   statement_while_test, $2,
					   statement_while_code, $4,
					   OBJE );
                        }  
		;
exc_handler_list: 
		exc_handler
			{
			  $$ = make_obj_f( group_list, list_exc_handler,
					   group_list_g1, $1,
					   group_list_next, 0,
					   OBJE );
			}
                | exc_handler '|' exc_handler_list 
			{ 
			  $$ = make_obj_f( group_list, list_exc_handler,
					   group_list_g1, $1,
					   group_list_next, $3,
					   OBJE ); 
			}
		;
exc_handler:    
                exc DARROW statement
			{
			  $$ = make_obj_f( group_exch, 0,
					   group_exch_sco, $1,
					   group_exch_handle, $3,
					   OBJE );
                        }
		;
/*--
\end{verbatim}
\section{Expressions}
\begin{verbatim} */
expression_list:        
                expression 
			{
			  $$ = make_obj_f( group_list, list_expression,
					   group_list_g1, $1,
					   group_list_next, 0,
					   OBJE );
                        }
                | expression ',' expression_list
			{
			  $$ = make_obj_f( group_list, list_expression,
					   group_list_g1, $1,
					   group_list_next, $3,
					   OBJE );
                        }
		;
alpha_expression:
                IDENTIFIER 
			{ 
			  p_obj *decl, *type;

			  decl=symtab_lookup_decl($1, ((1<<decl_var) +
						       (1<<decl_invar) + 
						       (1<<decl_outvar) +
						       (1<<decl_inoutvar) +
						       (1<<decl_procedure) +
						       (1<<decl_procedure_nr) +
						       (1<<decl_spec))
						  );
			  if (decl) {
			    type = LOOKUP_SUB(decl, group_decl_type);
			  } else { 
			    yyerror("Unrecognised identifier");
			    fprintf( stderr,
				     "Did not recognise %s\n",
				     LOOKUP_STRING($1, group_identifier_string)
				     );
			    YYERROR;                
			    type = 0;
			  }
			  $$ = make_obj_f( group_expression, expr_variable,
					   expr_variable_decl, decl,
					   group_expression_type, type,
					   OBJE );
                        } 
                | CONSTANT        
			{ 
			  $$ = $1; 
                        }
                | TRUE          
			{ 
			  p_type *type_obj;

			  type_obj = make_obj_f( 
				       group_type, type_boolean, 
				       group_type_oname, 
				       Predefined_Names[type_boolean],
				       OBJE );

			  $$ = make_obj_f( group_expression, expr_bool_val,
					   expr_bool_val_val, 1,
					   group_expression_type, type_obj,
					   OBJE ); 
			}
                | FALSE         
			{ 
			  p_type *type_obj;

			  type_obj = make_obj_f( 
				       group_type, type_boolean, 
				       group_type_oname, 
				       Predefined_Names[type_boolean],
				       OBJE );

			  $$ = make_obj_f( group_expression, expr_bool_val,
					   expr_bool_val_val, 0,
					   group_expression_type, type_obj,
					   OBJE ); 
			}
                | '(' expression_list ')' 
			{ 
			  $$ = make_bracket( $2 ); 
			}
                | PVS           
			{ 
			  $$ = make_obj_f( group_expression, expr_pvs,
					   make_obj_f( group_type, 
						       type_iref,
						       type_iref_id, 
						       "Pervasives",
						       group_type_oname,
						       "type_Pervasives_IREF",
						       OBJE ), 
					   OBJE); 
			}
		;
beta_expression:
                alpha_expression 
			{ 
			  $$ = $1; 
			}
                | beta_expression '[' expression ']' 
			{
			  p_obj *type;
                        
			  if (!LOOKUP_SUB($1, group_expression_type)) {
			    yyerror("Array is not typed");
			    YYERROR;
			  }
			  if (!LOOKUP_SUB( $1, group_expression_type)->form \
					       == type_array) {
			    yyerror("Array is not typed as an array");
			    YYERROR;
			  }                       
			  type = \
			    LOOKUP_SUB($1, group_expression_type)->ch[0].sub;
			  if (!$3) {
			    yyerror("Array argument is not defined. Weird.");
			    YYERROR;
			  }
			  if (!LOOKUP_SUB($3, group_expression_type)) {
			    yyerror("Array argument is not typed.");
			    YYERROR;
			  }
			  if (LOOKUP_SUB($3, group_expression_type)->form > \
			      type_long_integer) {
			    yyerror("Array argument not cardinal or integer");
			    YYERROR;
			  }

			  $$ = make_obj_f( group_expression, expr_array_lookup,
					   expr_array_lookup_l, $1,
					   expr_array_lookup_r, $3,
					   group_expression_type, type,
					   OBJE );
			}                 
                | beta_expression '.' IDENTIFIER 
			{ 
			  p_obj *field;
			  
			  if (!LOOKUP_SUB($$, group_expression_type)) {
			    yyerror("Left hand side of . is not typed");
			    YYERROR;
			  }
			  if (LOOKUP_SUB($1, group_expression_type)->form != \
			      type_record) {
			    yyerror("Attempted lookup in non-record");
			    YYERROR;
			  }
			  field = lookup_decl(
				    LOOKUP_SUB(
				      LOOKUP_SUB($1, group_expression_type),
				      group_type_g1), 
				    $3 );
			  if (field->form != decl_var) {
			   yyerror("Found non-variable declaration in record");
			   YYERROR;
			  } 
			  $$ = make_obj_f( group_expression, expr_dot_op,
					   group_expression_g1, $1,
					   group_expression_g2, $3,
					   group_expression_type, 
					   LOOKUP_SUB(field, group_decl_type),
					   OBJE );
                        }                       
                | beta_expression ARROW IDENTIFIER 
			{
			  p_obj *field;

			  if (LOOKUP_SUB($1, group_expression_type)->form \
			      != type_ref) {
			   yyerror("Attempted to dereference a non-reference");
			   YYERROR;
			  }
			  if (LOOKUP_SUB(LOOKUP_SUB($1, group_expression_type),
					 type_ref_type)->form != type_record) {
			    yyerror("Attempted lookup in a non-record");
			    YYERROR;
			  }
			  field = lookup_decl(
				    LOOKUP_SUB(
				      LOOKUP_SUB(
					LOOKUP_SUB($1, group_expression_type),
					group_type_g1),
				      type_record_list), 
				    $3);
			  if (field->form != decl_var) {
			    yyerror("Found a non-variable declaration in a record");
			    YYERROR;
			  }
			  LOOKUP_SUB($$, group_expression_type) = \
			    LOOKUP_SUB(field,group_decl_type);

			  $$ = make_obj_f( group_expression, expr_arrow_op,
					   expr_arrow_op_sub, $1,
					   expr_arrow_op_field, $3,
					   group_expression_type,
					   LOOKUP_SUB(field, group_list_g1),
					   OBJE );
                        }
                | beta_expression '(' expression_list ')' %prec '('
			{
			  p_obj *decl, *eptr, *aptr;
			  p_type *etype, *atype, *rtype;
			  p_list *decllist;
			  
			  aptr = LOOKUP_SUB(
				   LOOKUP_SUB($1, group_expression_type),
				   type_method_args);
			  eptr = $3;
			  
			  while (eptr && aptr) {
			    if (eptr->form != list_expression) {
			      yyerror("A non expression was found in an argument list");
			      YYERROR;
			    }
			    
			    atype = 0;
			    etype = LOOKUP_SUB(
				      LOOKUP_SUB(eptr,list_expression),
				      group_expression_type );

			    if (etype->form == type_tuple) {
			      p_list *lptr;
			      
			      /* All should get this far */
			      lptr = LOOKUP_SUB(etype, type_tuple_list);
			      
			      if (!LOOKUP_SUB(lptr, group_list_next)) {
				/* One-tuple alert */
				etype = LOOKUP_SUB(lptr, list_type_type);
				if (verbosity & 128) {
				  fprintf( stderr, 
					   "Converted 1 tuple to nontuple\n");
				}
			      }
			    }
                        
			    if (!etype) {
			      yyerror("An untyped expression was found in an "
				      "argument list");
			      YYERROR;
			    }
			    
			    switch (aptr->form) {
			      case list_expression:
				atype = LOOKUP_SUB(
					  LOOKUP_SUB( aptr, 
						      list_expression_expr),
					  group_expression_type);
				break;
			      case list_decl:
				atype = LOOKUP_SUB(
					  LOOKUP_SUB( aptr,
						      list_decl_decl),
					  group_decl_type);
				break;
			      case list_type:
				atype = LOOKUP_SUB(aptr, list_type_type);
				break;
			      default:
				yyerror("Couldn't find the type of something found in a tuple type list");
				YYERROR;
			    }
			    if (verbosity & 128) {
			      fprintf(stderr, "etype: \n");
			      dump_tree(messages, etype);
			      fprintf(stderr, "atype: \n");
			      dump_tree(messages, atype);
			    }                          
			    switch (compare_types(etype, atype)) {
			      case TYPES_IDENTICAL:
			      case TYPES_PROMOTE_2:
				break;
			      case TYPES_PROMOTE_1:
				yyerror("Attempt to demote function argument");
				YYERROR;
				break;
			      case TYPES_DISTINCT:
				yyerror("Attempt to pass an incompatible argument");
				YYERROR;
				break;
			    }
			    eptr = LOOKUP_SUB(eptr, group_list_next);
			    aptr = LOOKUP_SUB(aptr, group_list_next);
			  }
			  if (aptr) {
			    yyerror("Too many arguments supplied to function");
			    YYERROR;
			  }
			  if (eptr) {
			    yyerror("Too few arguments supplied to function");
			    YYERROR;
			  }
			  
			  decllist = LOOKUP_SUB(
				       LOOKUP_SUB($1, group_expression_type),
				       type_method_rets);
			  if (LOOKUP_SUB(decllist, group_list_next)) {
			    /* More than one result; form tuple */
			    
			    /* XXX: The next line gives a warning;
                             * haven't looked at this yet. Warning
                             * removed with cast. Dunno why. Complete
                             * mystery...  
			     */
			    rtype = make_tupletype( decllist );
			  } else {
			    decl = LOOKUP_SUB(decllist, list_decl_decl);
			    rtype = LOOKUP_SUB(decl, group_decl_type);
			  }
			  			  
			  $$ = make_obj_f( group_expression, expr_invoke_op,
					   expr_invoke_op_sub, $1,
					   expr_invoke_op_list, $3,
					   group_expression_type, rtype,
					   OBJE );
			  if (LOOKUP_SUB($1, group_expression_type)->form != \
			      type_method) {
			    yyerror("Method invocation on a non-method");
			    YYERROR;
			  }
			}        
		;
gamma_expression:
                beta_expression %prec DUMMY
			{ 
			  $$ = $1; 
			}
                | '&' gamma_expression %prec '&'
			{ 
			  $$ = make_obj_f( group_expression, expr_address_op,
					   expr_address_op_l, $2,
					   group_expression_type, 
			         make_obj_f( group_type, type_ref,
					     type_ref_type, 
					     LOOKUP_SUB($2, 
							group_expression_type),
					     OBJE ),
					   OBJE );
                        }
                | '*' gamma_expression %prec USTAR
			{ 
			  p_obj *type, *supertype;

			  supertype = LOOKUP_SUB($2, group_expression_type);
			  if (supertype -> form == type_ref) {
			    type = LOOKUP_SUB(supertype, type_ref_type);
			  } else {
			    type = 0;
			  }
			  $$ = make_obj_f( group_expression, expr_deref_op,
					   expr_deref_op_l, $2,
					   group_expression_type, type,
					   OBJE );
			  if (supertype->form != type_ref) {
			    yyerror("Attempt to derefence a non-REF");
			    YYERROR;
			  }
                        }
                | '+' gamma_expression %prec UPLUS
			{ 
			  $$ = make_unaryop(expr_positive_op, $2); 
			  if (LOOKUP_SUB($2,group_expression_type)->form <= \
			      type_long_real) {
			    LOOKUP_SUB($$, group_expression_type) = \
			      LOOKUP_SUB($2, group_expression_type);
			  } else {
			    yyerror("Attempt to make positive a non numerical type");
			    YYERROR;
			  }
                        }
                | '-' gamma_expression %prec UMINUS
			{ 
			  $$ = make_unaryop(expr_negative_op, $2); 
			  if (LOOKUP_SUB($2, group_expression_type)) { 
			    switch (LOOKUP_SUB($2, 
					       group_expression_type)->form) {
			      case type_short_cardinal:
				LOOKUP_SUB($$, group_expression_type) = \
				  make_obj_f( group_type, type_short_integer,
					      OBJE );
				break;
			      case type_cardinal:
			      case type_long_cardinal:
				LOOKUP_SUB($$, group_expression_type) = \
				  make_obj_f( group_type, type_long_integer,
					      OBJE );
				break;
			      case type_short_integer:
			      case type_integer:
			      case type_long_integer:
			      case type_real:
			      case type_long_real:
				LOOKUP_SUB($$, group_expression_type) = \
				  make_obj_f( group_type,
				    LOOKUP_SUB($2,group_expression_type)->form,
					      OBJE);
				break;
			      default:
				yyerror("Attempt to negate non-numeric type");
				break;
			    }
			  }       
                        }
		;
delta_expression:
                gamma_expression %prec DUMMY
			{ 
			  $$ = $1; 
			}
                | delta_expression '*' gamma_expression %prec '*'
			{ 
			  $$ = make_binop(expr_mult_op, $1, $3, 1); 
			}          
                | delta_expression '/' gamma_expression %prec '/'
			{ 
			  $$ = make_binop(expr_div_op, $1, $3, 1); 
			}
                | delta_expression '%' gamma_expression %prec '%'
			{ 
			  $$ = make_binop(expr_mod_op, $1, $3, 1); 
			}
		;
epsilon_expression: /* To fix s-r conflict with previous expression */
                delta_expression %prec DUMMY
			{ 
			  $$ = $1; 
			}
                | epsilon_expression '+' delta_expression %prec '+'
			{ 
			  $$ = make_binop(expr_add_op, $1, $3, 1); 
			}
                | epsilon_expression '-' delta_expression %prec '-'
			{ 
			  $$ = make_binop(expr_sub_op, $1, $3, 1); 
			}
                ;
zeta_expression: /* To fix s-r conflict with *previous* expression */
                epsilon_expression %prec DUMMY 
			{ 
			  $$ = $1; 
			}
                | zeta_expression '<' epsilon_expression 
			{ 
			  $$ = make_boolop(expr_lt_op, $1, $3); 
			}
                | zeta_expression '>' epsilon_expression 
			{ 
			  $$ = make_boolop(expr_gt_op, $1, $3); 
			}
                | zeta_expression LE epsilon_expression 
			{ 
			  $$ = make_boolop(expr_le_op, $1, $3); 
			}
                | zeta_expression GE epsilon_expression 
			{ 
			  $$ = make_boolop(expr_ge_op, $1, $3); 
			}
                ;
eta_expression:
                zeta_expression 
			{ 
			  $$ = $1; 
			} 
                | eta_expression EQ zeta_expression 
			{ 
			  $$ = make_boolop(expr_eq_op, $1, $3);
			}
                | eta_expression NE zeta_expression 
			{ 
			  $$ = make_boolop(expr_ne_op, $1, $3);
			}
                ;
theta_expression:
                eta_expression  
			{ 
			  $$ = $1; 
			} 
                | theta_expression ASS eta_expression 
			{ 
			  $$ = make_binop(expr_ass_op, $1, $3, 0);
			}
                ;
expression: 
		theta_expression 
			{ 
			  $$ = $1; 
			  got_expression( $$ ); 
			}
		;
/*--
\end{verbatim}
*/
%%
void yyerror(string)
     const char*string;
{
  dprintf( messages,
	   "%s:%d: %s\n", 
	   instack[instackptr].name,
	   instack[instackptr].line,
	   string);
  exit(1);
}
