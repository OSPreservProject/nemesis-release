
/*
 * Identifiers.c : managing identifiers
 */

#include "Middlc.h"
#include "Identifiers.h"

/* 
 * Create a new identifier instance 
 */
Identifier_t New_Identifier( int lineno, char *s )
{
  Identifier_t i = new(Identifier);

  i->lineno = lineno;
  i->name   = strdup(s);
  return i;
}

/*
 * Create a new identifier list 
 */
IdentifierList_t New_IdentifierList( Identifier_t i )
{
  IdentifierList_t il = new(IdentifierList);

  il->i = i;
  il->next = NULL_IdentifierList;
  return il;
}

/*
 * Add an identifier to a list, which may be null.
 */
IdentifierList_t IdentifierList_Append( IdentifierList_t list, 
				       Identifier_t i )
{
  IdentifierList_t nl = New_IdentifierList( i );

  if ( list ) {
    IdentifierList_t l;

    for( l = list; l->next ; l=l->next );
    l->next = nl;
    return list;
  } else {
    return nl;
  }
}

/*
 * Return the string corresponding to the nth identifier in a list.
 */
char *IdentifierList_String( IdentifierList_t list, int i )
{
  while( i ) {
    list = list->next;
    if (!list) return (char *)0;
    i--;
  }
  
  return list->i->name;
}
 
  
