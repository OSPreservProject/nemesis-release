
/*
 * Exceptions.c : managing lists of exceptions.
 *
 * Exceptions are just declarations, and are handled in Decls.c. This
 * file is responsible for manipulating lists of exception identifiers
 * found in proc declarations.
 */

#include "Middlc.h"
#include "Exceptions.h"

/*
 * A new exception given as a scoped name: look up and ensure that the
 * name refers to an exception, and then create a new struct.
 */
ExcList_t New_ExcListScoped( ScopedName_t sn )
{
  ExcList_t e;
  Decl_t d = ScopedName_GetDecl( sn );
  
  if ( !d ) {
    Error_Scoped("'%s' is undefined", sn );
    return NULL_ExcList;
  }
  if ( Decl_Kind( d ) != Decl_Exc ) {
    Error_Scoped("'%s' is not an exception", sn );
    return NULL_ExcList;
  }
  e = new(ExcList);
  e->exc = d;
  e->next = NULL_ExcList;
  return e;
}

/*
 * A new exception given as a local name: look up and ensure that the
 * name refers to an exception, then create the struct.
 */
ExcList_t New_ExcListLocal( Identifier_t i )
{
  ExcList_t e;
  Decl_t d = Interface_LocalGetDecl( i );
  
  if ( !d ) {
    Error_Symbol("'%s' is undefined", i );
    return NULL_ExcList;
  }
  if ( Decl_Kind( d ) != Decl_Exc ) {
    Error_Symbol("'%s' is not an exception", i );
    return NULL_ExcList;
  }
  e = new(ExcList);
  e->exc = d;
  e->next = NULL_ExcList;
  return e;
}


/*
 * Add the exception to a list, creating the list if necessary.
 */
ExcList_t ExcList_Append( ExcList_t list, ExcList_t e )
{
  if ( list ) {
    ExcList_t el;

    for( el=list; el->next; el = el->next );
    el->next = e;
    return list;
  } else {
    return e;
  }
}


/*
 * Dump raises list in Python
 */
void ExcList_DumpPython( ExcList_t list )
{
  printf( "el = []\n" );

  for ( ; list; list = list->next ) {
    if ( list->exc->kind != Decl_Exc )
      Error_Bug( "ExcList_DumpPython: decl is not an exception" );
    printf( "el.append( %s )\n", list->exc->u.e.python_name );
  }
}
