%{

/*
 * Lexer.l : Flex definition for middlc_front.
 */

/*#define LEXER_TRACE*/
#ifdef LEXER_TRACE
#define FLEX_DEBUG
#define TRC(x) x
#else
#define TRC(x)
#endif

#undef YY_DECL
#define YY_DECL int yylex( YYSTYPE *lvalp )

//#undef yywrap
//#define yywrap() (1)

#include "Middlc.h"
#include "Grammar.tab.h"

%}

DIGIT	[0-9]
IDENT	[A-Za-z][A-Za-z0-9_]*

%option noyywrap
%%

"--".*"\n"	FileStack_NewLine();	/* Ignore comments */
"\n"		FileStack_NewLine();	/* Ignore newlines */
[ \t]+		/* Whitespace */

{DIGIT}+	lvalp->num = atoi(yytext); return TK_NUMBER;

"INTERFACE"	return TK_INTERFACE;
"BEGIN"		return TK_BEGIN;
"END"		return TK_END;
"TYPE"		return TK_TYPE;
"BOOLEAN"	return TK_BOOLEAN;
"SHORT"		return TK_SHORT;
"CARDINAL"	return TK_CARDINAL;
"LONG"		return TK_LONG;
"NETWORK"	return TK_NETWORK;
"INTEGER"	return TK_INTEGER;
"REAL"		return TK_REAL;
"STRING"	return TK_STRING;
"OCTET"		return TK_OCTET;
"CHAR"		return TK_CHAR;
"DANGEROUS"	return TK_DANGEROUS;
"ADDRESS"	return TK_ADDRESS;
"WORD"		return TK_WORD;
"ARRAY"		return TK_ARRAY;
"SET"		return TK_SET;
"OF"		return TK_OF;
"BITS"		return TK_BITS;
"SEQUENCE"	return TK_SEQUENCE;
"RECORD"	return TK_RECORD;
"CHOICE"	return TK_CHOICE;
"REF"		return TK_REF;
"PROC"		return TK_PROC;
"RETURNS"	return TK_RETURNS;
"IN"		return TK_IN;
"OUT"		return TK_OUT;
"ANNOUNCEMENT"	return TK_ANNOUNCEMENT;
"EXCEPTION"	return TK_EXCEPTION;
"RAISES"	return TK_RAISES;
"EXTENDS"	return TK_EXTENDS;
"NEEDS"		return TK_NEEDS;
"IREF"		return TK_IREF;
"LOCAL"		return TK_LOCAL;
"NEVER"		return TK_NEVER;

{IDENT}		{
		  char *idstr = malloc( yyleng+1 );

		  (void)strcpy( idstr, yytext );
		  lvalp->name = New_Identifier( FileStack_Lineno(), idstr );
		  TRC(printf("Lexer: identifier '%s'.\n", idstr ));
		  return TK_IDENTIFIER;
		}

.		return *yytext;
%%

/*
 * Called by the FileStack to register a new file with the Lexer.
 */
void *Lexer_FileInit( FILE *fd )
{
  TRC(printf("Lexer: new file.\n"));
  yyin = fd; 
  yy_switch_to_buffer( yy_create_buffer( yyin, YY_BUF_SIZE ) );

  return (void *) YY_CURRENT_BUFFER;
}

/*
 * Called by the FileStack to finish processing a nested file and return to
 * "old_buffer".
 */
void Lexer_FileEnds( void *old_buffer )
{
  TRC(printf("Lexer: file ends.\n"));

  yy_delete_buffer( YY_CURRENT_BUFFER );
  yy_switch_to_buffer( old_buffer );

  return;
}

