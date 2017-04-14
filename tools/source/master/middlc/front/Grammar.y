
%{

#include "string.h"
#include "Middlc.h"

#define GRAMMAR_TRACE
#ifdef GRAMMAR_TRACE
#define YYDEBUG 1
#define TRC(x) x
#else
#define TRC(x)
#endif

static int yyerror( char *message );
extern int yyparse();

static Spec_t tmp_result; /* top level parse tree node */

%}


%token <num>	TK_NUMBER
%token <name>	TK_IDENTIFIER

%token <name>	TK_INTERFACE
%token <name>	TK_BEGIN
%token <name>	TK_END
%token <name>	TK_TYPE
%token <name>	TK_BOOLEAN
%token <name>	TK_SHORT
%token <name>	TK_CARDINAL
%token <name>	TK_LONG
%token <name>	TK_NETWORK
%token <name>	TK_INTEGER
%token <name>	TK_REAL
%token <name>	TK_STRING
%token <name>	TK_OCTET
%token <name>	TK_CHAR
%token <name>	TK_DANGEROUS
%token <name>	TK_ADDRESS
%token <name>	TK_WORD
%token <name>	TK_ARRAY
%token <name>	TK_SET
%token <name>	TK_OF
%token <name>   TK_BITS
%token <name>	TK_SEQUENCE
%token <name>	TK_RECORD
%token <name>	TK_CHOICE
%token <name>	TK_REF
%token <name>	TK_PROC
%token <name>	TK_NEVER
%token <name>	TK_RETURNS
%token <name>	TK_IN
%token <name>	TK_OUT
%token <name>	TK_ANNOUNCEMENT
%token <name>	TK_EXCEPTION
%token <name>	TK_RAISES
%token <name>	TK_EXTENDS
%token <name>	TK_NEEDS
%token <name>	TK_IREF
%token <name>	TK_LOCAL

%union {		   		/* stack type */
  Interface_t     	intf;
  InterfaceList_t 	intflist;
  Decl_t          	decl;
  DeclList_t      	decllist;
  ExcList_t       	exclist;
  Type_t          	type;
  CandidateList_t 	candlist;
  ArgList_t       	arglist;
  Mode_t          	mode;
  Field_t		field;
  FieldList_t     	fieldlist;
  bool_t          	bool;
  Identifier_t	  	name;
  IdentifierList_t	namelist;
  ScopedName_t		scopedname;
  Spec_t		spec;
  int			num;
}

%type <decllist> 	Declarations 
%type <decl>		Declaration
%type <decl>		TypeDecl ExcDecl ProcDecl AnnDecl
%type <intf>		IntfSpec Extends Need
%type <bool>		IntfHeader
%type <intflist>	Needs
%type <exclist>		RaisesClause Exceptions Exception
%type <type>		Type Predefined Constructed RefType IrefType NamedType
%type <candlist>	Candidates Candidate
%type <arglist>		Arguments Args Arg Results Ress Res
%type <mode>		Mode 
%type <spec>		Specification
%type <field>		Field 
%type <fieldlist>	Fields
%type <namelist>	Identifiers
%type <scopedname>	ScopedIdentifier

%pure_parser
%start Specification

%%
/********************************************************/
/*							*/
/*	Grammar section					*/
/*							*/
/********************************************************/

Specification :	IntfSpec '.'
		  {
		    $$=New_IntfSpec( $1 );
		    tmp_result = $$;
		  }

/*
 * Interface structure
 */

IntfSpec:	IntfHeader Extends Needs TK_BEGIN Declarations TK_END
		  { $$=New_Interface( $1, $2, $3, $5 );  } 	
		;

IntfHeader:	TK_IDENTIFIER ':' TK_LOCAL TK_INTERFACE '='
		  { $$ = Interface_Start( $1, True ); }
		| TK_IDENTIFIER ':' TK_INTERFACE '='
		  { $$ = Interface_Start( $1, False ); }
		;

Extends:	/* empty */
		  { $$=NULL_Interface; }
		| TK_EXTENDS TK_IDENTIFIER ';'
		  { $$=Interface_Extends( $2 ); }
		;

Needs:		/* empty */
		  { $$=NULL_InterfaceList; }
		| Needs Need
		  { $$=InterfaceList_Append( $1, $2 ); }
		;

Need:		TK_NEEDS TK_IDENTIFIER ';'
		  { $$=Interface_Needs( $2, NULL_Identifier ); }
		;

/*
 * Declarations and operations
 */
Declarations:	/* empty */
		  { $$=NULL_DeclList; }
		| Declarations Declaration
		  { $$=DeclList_Append( $1, $2 ); }
		;

Declaration:	TypeDecl
		| ExcDecl
		| ProcDecl
		| AnnDecl
		| error ';'
		  { $$=NULL_Decl; }
		;

AnnDecl: 	TK_IDENTIFIER ':' TK_ANNOUNCEMENT '[' Arguments ']' ';'
		  { $$=New_AnnDecl( $1, $5 ); }
		;

ExcDecl:	TK_IDENTIFIER ':' TK_EXCEPTION '[' Arguments ']' ';'
		  { $$=New_ExcDecl( $1, $5 ); }
		;

TypeDecl:	TK_IDENTIFIER ':' TK_TYPE '=' Type ';'
		  { $$=Decl_NameType( $1, $5 ); }
		;

ProcDecl:	TK_IDENTIFIER ':' TK_PROC '[' Arguments ']'
				 TK_RETURNS '[' Results ']'
    				 RaisesClause ';'
		  { $$=New_ProcDecl( $1, $5, $9, $11, True ); }
		| TK_IDENTIFIER ':' TK_PROC '[' Arguments ']'
				 TK_NEVER TK_RETURNS
				 RaisesClause ';'
		  { $$=New_ProcDecl( $1, $5, NULL_ArgList, $9, False ); }
		;
	
RaisesClause:   /* empty */
		  { $$=NULL_ExcList; }
		| TK_RAISES Exceptions
		  { $$=$2; } 
		;

/*
 * Type system
 */
Type:		Predefined
		| Constructed
                  { (void) Decl_NewType( $1 ); $$ = $1; }
		| RefType
		| IrefType
		| NamedType
		;

Predefined:	TK_BOOLEAN
		  { $$=Type_Predefined( TypeBoolean ); }
		| TK_SHORT TK_CARDINAL
		  { $$=Type_Predefined( TypeShortCard ); }
		| TK_CARDINAL
		  { $$=Type_Predefined( TypeCard ); }
		| TK_LONG TK_CARDINAL
		  { $$=Type_Predefined( TypeLongCard ); }
		| TK_SHORT TK_INTEGER
		  { $$=Type_Predefined( TypeShortInt ); }
		| TK_INTEGER
		  { $$=Type_Predefined( TypeInt ); }
		| TK_LONG TK_INTEGER
		  { $$=Type_Predefined( TypeLongInt ); }
		| TK_REAL
		  { $$=Type_Predefined( TypeReal ); }
		| TK_LONG TK_REAL
		  { $$=Type_Predefined( TypeLongReal ); }
		| TK_STRING
		  { $$=Type_Predefined( TypeString ); }
		| TK_OCTET
		  { $$=Type_Predefined( TypeOctet ); }
		| TK_CHAR
		  { $$=Type_Predefined( TypeChar ); }
		| TK_ADDRESS
		  { $$=Type_Predefined( TypeAddress ); }
		| TK_WORD
		  { $$=Type_Predefined( TypeWord ); }
		| TK_DANGEROUS TK_ADDRESS
		  { $$=Type_Predefined( TypeAddress ); }
		| TK_DANGEROUS TK_WORD
		  { $$=Type_Predefined( TypeWord ); }
		;

Constructed:	'{' Identifiers '}'
		  { $$=Type_NewEnum( $2 ); }
		| TK_ARRAY TK_NUMBER TK_OF Type
		  { $$=Type_NewArray( $2, $4 ); }
		| TK_ARRAY TK_NUMBER TK_OF TK_BITS
		  { $$=Type_NewBitSet( $2 ); }
		| TK_SET TK_OF Type
		  { $$=Type_NewSet( $3 ); }
		| TK_SEQUENCE TK_OF Type
		  { $$=Type_NewSequence( $3 ); }
		| TK_RECORD '[' Fields ']'
		  { $$=Type_NewRecord( $3 ); }
		| TK_CHOICE Type TK_OF '{' Candidates '}'
		  { $$=Type_NewChoice( $2, $5 ); }
		;

RefType:	TK_REF Type
		  { $$=Type_NewRef( $2 ); }
		;

IrefType:	TK_IREF TK_IDENTIFIER
		  { $$=Type_IrefName( $2 ); }
		;

NamedType:	ScopedIdentifier
		  { $$=Type_ScopedName( $1 ); }
		| TK_IDENTIFIER
		  { $$=Type_LocalName( $1 ); }
		;

Candidates:	Candidate
		| Candidates ',' Candidate
		  { $$=CandidateList_Append( $1, $3 ); }
		;

Candidate:	Identifiers '=' '>' Type
		  { $$=New_CandidateList( $1, $4 ); }
		;

Arguments:	/* empty */
		  { $$=NULL_ArgList; }
		| Args
		;

Args:		Arg
		| Args ',' Arg
		  { $$=ArgList_Append( $1, $3 ); }
		| Args ';' Arg
		  { $$=ArgList_Append( $1, $3 ); }
		;

Arg:		Mode Field
		  { $$=New_ArgList( $1, $2 ); }
		;

Results:	/* empty */
		  { $$=NULL_ArgList; }
		| Ress
		;

Ress:		Res
		| Ress ',' Res
		  { $$=ArgList_Append( $1, $3 ); }
		| Ress ';' Res
		  { $$=ArgList_Append( $1, $3 ); }
		;

Res:		Field
		  { $$=New_ArgList( Mode_Result, $1 ); }
		;

Mode:		/* empty */
		  { $$=Mode_In; }
		| TK_IN
		  { $$=Mode_In; }
		| TK_OUT
		  { $$=Mode_Out; }
		| TK_IN TK_OUT
		  { $$=Mode_InOut; }
		;

Fields:		Field
		  { $$=New_FieldList( $1 ); }
		| Fields ',' Field
		  { $$=FieldList_Append( $1, $3 ); }
		;

Field:		Identifiers ':' Type
		  { $$=New_Field( $1, $3 ); }
		;

Identifiers:	TK_IDENTIFIER
		  { $$=New_IdentifierList( $1 ); }
		| Identifiers ',' TK_IDENTIFIER
		  { $$=IdentifierList_Append( $1, $3 ); }
		;


Exceptions:	Exception
		| Exceptions ',' Exception
		  { $$=ExcList_Append( $1, $3 ); }
		;

Exception:	ScopedIdentifier
		  { $$=New_ExcListScoped( $1 ); }
		| TK_IDENTIFIER
		  { $$=New_ExcListLocal( $1 ); }
		;

ScopedIdentifier: TK_IDENTIFIER '.' TK_IDENTIFIER
		  { $$=New_ScopedName( $1, $3 ); }
		;


%%

static int yyerror( char *message )
{
  Error_Simple( message );
  return 0;
}

Spec_t Parser_Parse()
{
  int i = yyparse();

  if ( i == 0 ) {
    return tmp_result;
  } else {
    return New_IntfSpec( NULL_Interface );
  }
}

