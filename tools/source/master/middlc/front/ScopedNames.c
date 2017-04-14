
/*
 * ScopedNames.c : nodes representing scoped names.
 */

#include "Middlc.h"
#include "ScopedNames.h"


/*
 * Create a new scoped name
 *
 * First check that an interface exists with the right name, second
 * that it contains a decl of the right name. Flag an error if not.
 */
ScopedName_t New_ScopedName( Identifier_t scope, Identifier_t name )
{
  ScopedName_t sn;
  Interface_t  intf;
  Decl_t       d;
  bool_t found;

  SymbolTable_Lookup(globals.needs, scope->name, (caddr_t *)&intf, &found );
  if ( !found ) {
    Error_Symbol( "No scope called '%s'", scope );
    d = NULL_Decl;
  } else if ( !intf->symbols ) {
    Error_Symbol( "Recursive use of contents of interface '%s'", scope );
    d = NULL_Decl;
  } else {
    d = Interface_GetDecl( intf, name );
    if ( !d ) {
      Error_Symbols( "no such symbol '%s' in scope '%s'", name, scope );
    }
  }

  sn = new(ScopedName);
  sn->decl = d;
  sn->scope = scope;
  sn->name = name;
  return sn;
}
  
/*
 * Return the declaration corresponding to a scoped name 
 * or NULL_Decl. 
 */
Decl_t ScopedName_GetDecl( ScopedName_t sn )
{
  return sn ? sn->decl : NULL_Decl;
}
