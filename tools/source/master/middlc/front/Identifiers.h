
#ifndef _Identifiers_h_
#define _Identifiers_h_

/* Whenever the lexer finds an identifier, is creates one of these to */
/* hold it. */

#define NULL_Identifier ((Identifier_t)0)
struct _Identifier_t {
  int lineno;
  char *name;
};

#define NULL_IdentifierList ((IdentifierList_t)0)
struct _IdentifierList_t {
  Identifier_t i;
  IdentifierList_t next;
};

extern Identifier_t New_Identifier( int lineno, char *s );

extern IdentifierList_t New_IdentifierList( Identifier_t i );

extern IdentifierList_t IdentifierList_Append( IdentifierList_t list, 
					      Identifier_t i );

extern char *IdentifierList_String( IdentifierList_t list, int i );


#endif /*  _Identifiers_h_ */



