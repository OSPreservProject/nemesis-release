%{
/* CLUless compiler front end
 ***********************************************************************
 * Copyright 1997 University of Cambridge Computer Laboratory
 * All Rights Reserved
 * Authors: Dickon Reed (Dickon.Reed@cl.cam.ac.uk)
 *          Richard Mortier (rmm1002@cam.ac.uk)
 ***********************************************************************/ 
/* cl.l; Flex input file
 *
 * Dickon Reed, 1996 
 * Richard Mortier, 1997 
 *
 * $Id: cl.l 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
 *
 * $Log: cl.l,v $
 * Revision 1.2  1998/04/28 14:36:23  dr10009
 * sort out for libc6
 *
 * Revision 1.1  1997/06/25 17:06:44  dr10009
 * Initial revision
 *
 * Revision 2.6  1997/04/12 13:13:20  rmm1002
 * Fixed range checking for constants (floating pt, numbers, chars, etc).
 *
 * Revision 2.5  1997/04/11 16:31:15  rmm1002
 * Finally (hah!) fixed string/char handling for those including
 * '\' escape codes.
 *
 * Revision 2.4  1997/04/10 22:59:07  rmm1002
 * Added lexing for characters and strings...
 *
 * Revision 2.3  1997/04/07 20:50:33  rmm1002
 * More fiddling with comments; now count lines correctly, so parse errors
 * *mean* something. Also removed some defunct debugging stmts.
 *
 * Revision 2.2  1997/04/06 18:52:34  rmm1002
 * Yet another comment reading bug fixed; apparently I am unable to copy
 * exampes from the Flex info pages properly... (Start Condition node)
 *
 * Revision 2.1  1997/04/04 15:34:12  rmm1002
 * Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
 *
 * Revision 1.16  1997/04/02 14:06:43  rmm1002
 * Reinstated THEN
 *
 * Revision 1.15  1997/03/19 21:55:08  rmm1002
 * Finished parsing of inline C (C_BLOCK)
 *
 * Revision 1.14  1997/03/11 22:30:02  rmm1002
 * *** empty log message ***
 *
 * Revision 1.13  1997/03/07 16:25:07  rmm1002
 * Fixed the comment handling & added lexing for blocks of C
 *
 * Revision 1.12  1997/02/20 00:25:55  rmm1002
 * Removed NETWORK token (and THEN token)
 *
 * Revision 1.11  1997/02/10 19:41:35  rmm1002
 * More minor alterations
 *
 * Revision 1.10  1997/02/04 16:48:22  rmm1002
 * Finished first look and layout fixup
 *
 * Revision 1.1  1997/01/30 21:21:24  rmm1002
 * Initial revision
 *
 */

#define YYSTYPE p_obj *
#include "cluless.h"
#include "stackobj.h"
#include "cl.tab.h" 
#include "group.h"
#include "expr.h"
#include "type.h"
#include "statement.h"
#include "parsupport.h"
#include "dpython.h"
#include <limits.h>
#include <errno.h>
#include <float.h>

#undef YY_DECL
#define YY_DECL int yylex ( YYSTYPE *lvalp )

/* {comment_start}(.|\n)*{comment_end} {}
 *
 * {c_block_start}(.|\n)*{c_block_end} {
 * 
 * The above don't work since flex generates an "aggressive" lexer, that will
 * read until the *final* relevant delimiter in the file, not to the *first*
 * relevant delimiter, which is what we want...
 */
%}

/* Standard regular expressions */
identifier		[a-zA-Z_][0-9a-zA-Z_]*

exponent		[eE][-+]?[0-9]+
mantissa		([0-9]*"."[0-9]+)|([0-9]+".")
floating_constant	(({mantissa}{exponent}?)|([0-9]+{exponent}))[FfLl]?

integer_suffix_opt	([uU]?[lL]?)|([lL][uU])
decimal_constant	[0-9]+{integer_suffix_opt}

simple_esc		[abfnrtv'"?\\]
octal_esc		[0-7]{1,3}
hex_esc			[xX][0-9a-fA-F]+

escape_sequence		[\\]({simple_esc}|{octal_esc}|{hex_esc})
char_cpt		[^'\\\n]|{escape_sequence}
string_cpt		[^"\\\n]|{escape_sequence}

horizontal_white	[ \t]

one_line_comment	"--"
comment_start		"/*"
%x comment

c_block_start		"/{"
%x c_block
%%

{one_line_comment}.*\n	instack[instackptr].line++ ;

{comment_start}		BEGIN(comment); 
<comment>[^*\n]*		
<comment>"*"+[^*/\n]*	
<comment>"\n"		instack[instackptr].line++ ;
<comment>"*"+"/"	BEGIN(INITIAL);

{c_block_start}		BEGIN(c_block);
<c_block>[^}\n]*	|
<c_block>"}"+[^}/\n]*	yymore();
<c_block>"\n"		yymore(); instack[instackptr].line++ ;
<c_block>"}"+"/"	{
			char *code;

			/* Set up string to copy block of code into */
			code = malloc( sizeof(char)*(yyleng+1) );
			code[0] = '{';

			/* Copy block of code to c and make node */
			strncpy( code+1, yytext, yyleng-1 );
			code[yyleng] = '\0';

			*lvalp = make_obj_f( group_statement, 
					     statement_c_block,
					     statement_c_block_code, code,
					     OBJE );
			BEGIN(INITIAL);
			return C_BLOCK;
			}

{horizontal_white}+
\n			instack[instackptr].line ++ ;

MODULE	                return MODULE;
INTERFACE		return INTERFACE;
BEGIN			return CLU_BEGIN;
END			return CLU_END;
DANGEROUS		return DANGEROUS;
LOCAL			return LOCAL;

ANY			return ANY;
TYPE			return TYPE;
BOOLEAN			return BOOLEAN;
SHORT			return SHORT;
CARDINAL		return CARDINAL;
LONG			return LONG;
INTEGER			return INTEGER;
REAL			return REAL;
OCTET			return OCTET;
CHAR			return CHAR;
STRING			return STRING;
ADDRESS			return ADDRESS;
WORD			return WORD;
ARRAY			return ARRAY;
SET			return SET;
OF			return OF;
SEQUENCE		return SEQUENCE;
RECORD			return RECORD;
TUPLE                   return TUPLE;
CHOICE			return CHOICE;
REF			return REF;
IREF			return IREF;

PROC			return PROC;
NEVER			return NEVER;
RETURNS			return RETURNS;
IN			return IN;
OUT			return OUT;
ANNOUNCEMENT		return ANNOUNCEMENT;
EXCEPTION		return EXCEPTION;
RAISES			return RAISES;
EXTENDS			return EXTENDS;
NEEDS			return NEEDS;
METHOD                  return METHOD;
FROM                    return FROM;
TO                      return TO;

WIBBLE			return WIBBLE;
RAISE			return RAISE;
TRY			return TRY;
EXCEPT			return EXCEPT;
FINALLY			return FINALLY;
LOOP			return LOOP;
WHILE			return WHILE;
DO			return DO;
REPEAT			return REPEAT;
UNTIL			return UNTIL;
ELSE			return ELSE;
EXIT			return EXIT;
RETURN			return RETURN;
IF			return IF;
THEN			return THEN;

TRUE			return TRUE;
FALSE			return FALSE;
BITS                    return BITS;

PVS                     return PVS;

{identifier}          	{
			char *ptr;

			ptr = malloc( sizeof(char)*(yyleng + 1) );
			strcpy(ptr, yytext); 
			*lvalp = make_obj_f( group_identifier, 0, 
					     group_identifier_string, ptr, 
					     OBJE );	
			return IDENTIFIER;
			}
			
{decimal_constant}      {
			char *ptr;
			p_type *type_obj;
			int type;
			unsigned long long int n;

			ptr = malloc( sizeof(char)*(yyleng + 1) );
			strcpy(ptr, yytext); 

			errno = 0;
			n = strtoul( ptr, (char**)NULL, 10 );
			/* If cardinal is out of range, then n set to
			 * HUGE_VAL and errno to ERANGE, defined in
                         * errno.h */
			if (errno == ERANGE)
			  yyerror("Number out of range\n");

			if (n >= 0 && n <= USHRT_MAX)
			  type = type_short_cardinal;
			else if (n > USHRT_MAX && n <= UINT_MAX)
			  type = type_cardinal;
			else if (n > UINT_MAX && n <= ULONG_MAX)
			  type = type_long_cardinal;

			type_obj = make_obj_f( group_type, type,
					       group_type_oname,
					       Predefined_Names[type],
					       OBJE );
			*lvalp = make_obj_f( group_expression, expr_num_val,
					     expr_num_val_val, ptr,
					     group_expression_type, type_obj,
					     OBJE );
			return CONSTANT;
			}

{floating_constant} 	{
			char *ptr;
			p_type *type_obj;
			double d;
			int type;

			ptr = malloc( sizeof(char)*(yyleng + 1) );
			strcpy(ptr, yytext); 

			errno = 0;
			d = strtod( ptr, (char **)NULL );
			if (errno == ERANGE)
			  yyerror("Floating point number out of range\n");

			if (d >= 0 && d <= FLT_MAX)
			  type = type_real;
			else if (d > FLT_MAX && d <= DBL_MAX)
			  type = type_long_real;

			type_obj = make_obj_f( group_type, type, 
					       group_type_oname, 
					       Predefined_Names[type],
					       OBJE );
  			*lvalp = make_obj_f( group_expression, expr_real_val,
					     expr_real_val_val, ptr,
					     group_expression_type, type_obj,
					     OBJE );
			return CONSTANT;
			}

"\'"{char_cpt}"\'"	{ /* "'a'" results in yytext=[':a:':\0] and
			   * yyleng = 3 */
			char *cc;
			int cnt=0, ci=0, yi;
			p_type *type_obj;

			cc = yytext;
			while (*cc++)
			  if (*cc == '\\')
			    cnt++;
			cc = malloc( sizeof(char)*((yyleng-1)+cnt) );

			for (yi=0; yi<=yyleng; yi++) {
			  if ( (yi!=0) && (yi!=(yyleng-1)) )
			    cc[ci++] = yytext[yi];
			  if (yytext[yi] == '\\')
			    cc[ci++] = '\\';
			}

			type_obj = make_obj_f( group_type, type_char,
					       group_type_oname, 
					       Predefined_Names[type_char],
					       OBJE );

			*lvalp = make_obj_f( group_expression, expr_char_val,
					     expr_char_val_val, cc,
					     group_expression_type, type_obj,
					     OBJE );
			return CONSTANT;
			}

"\""{string_cpt}+"\""	{
			char *ss;
			int cnt=0, si=0, yi;
			p_type *type_obj;
			
			ss = yytext;
			while (*ss++)
			  if (*ss == '\\')
			    cnt++;
			ss = malloc( sizeof(char)*((yyleng-1)+cnt) );

			for (yi=0; yi<=yyleng; yi++) {
			  if ( (yi!=0) && (yi!=(yyleng-1)) )
			    ss[si++] = yytext[yi];
			  if (yytext[yi] == '\\')
			    ss[si++] = '\\';
			}
			
			type_obj = make_obj_f( group_type, type_string,
					       group_type_oname, 
					       Predefined_Names[type_string],
					       OBJE );

			*lvalp = make_obj_f( group_expression, expr_str_val,
					     expr_str_val_val, ss,
					     group_expression_type, type_obj,
					     OBJE );
			return CONSTANT;
			}

":"                  	return ':';
"."		     	return '.';	
";"			return ';';
"["			return '[';
"]"			return ']';
"("			return '(';
")"			return ')';
"{"			return '{';
"}"			return '}';
","			return ',';
"->"                    return ARROW;
"=>"                    return DARROW;

"+"			return '+';
"-"			return '-';
"*"			return '*';
"/"			return '/';
"%"			return '%';

"|"			return '|';

":="			return ASS;
"=="			return EQ;

"="			return '=';
"!="			return NE;
"<"			return '<';
">"			return '>';
"<="			return LE;
">="			return GE;

"&"			return '&';

.			{
			fprintf( stderr,
				 "%s:%d: Unrecognized character [%c]\n",
				 instack[instackptr].name,
				 instack[instackptr].line,
				 *yytext );
			}

%%

/* Notify lexer of new buffer */ 
void lex_begin(void) { 
  yyin = instack[instackptr].file; 
  yy_switch_to_buffer( yy_create_buffer( yyin, YY_BUF_SIZE )); 
  instack[instackptr].lexdata = (void *) YY_CURRENT_BUFFER; 
}

/* Notify lexer of end of buffer; instack[instackptr] is new context */
void lex_end(void) {
  yy_delete_buffer(YY_CURRENT_BUFFER);
  yy_switch_to_buffer(instack[instackptr].lexdata);
}
