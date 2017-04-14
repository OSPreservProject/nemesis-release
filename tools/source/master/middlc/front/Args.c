
/*
 * Argument lists are like fields, but they have a mode as well.
 */

#include "Middlc.h"
#include "Args.h"


/*
 * Create a new arglist from a field.
 */
ArgList_t New_ArgList( Mode_t mode, Field_t field )
{
  ArgList_t a = NULL_ArgList;

  if (field) {
    a = new(ArgList);
    a->f = field;
    a->m = mode;
    a->next = NULL_ArgList;
  }
  return a;
}

/*
 * Append an argument to a list, creating the list if necessary
 */
ArgList_t ArgList_Append( ArgList_t list, ArgList_t al )
{
  if ( list ) {
    ArgList_t a;

    for( a=list; a->next; a = a->next );
    a->next = al;
    return list;
  } else {
    return al;
  }
}


/* 
 * Check that the field identifiers in "al1" and "al2" are distinct,
 * issuing an error if not.
 */

void ArgList_CheckDuplicateIds( ArgList_t al1, ArgList_t al2 )
{
  SymbolTable_t tbl = New_SymbolTable();
  IdentifierList_t il;

  for ( ; al1; al1 = al1->next ) {
    for ( il = al1->f->il; il; il = il->next ) {
      if ( SymbolTable_Enter( tbl, il->i->name, NULL ) )
	Error_Symbol( "argument or result '%s' is duplicated", il->i );
    }
  }

  for ( ; al2; al2 = al2->next ) {
    for ( il = al2->f->il; il; il = il->next ) {
      if ( SymbolTable_Enter( tbl, il->i->name, NULL ) )
	Error_Symbol( "argument or result '%s' is duplicated", il->i );
    }
  }

  SymbolTable_Delete( tbl );
}

/* 
 * Check that the arguments in "al" are suitable for an ANNOUNCEMENT:
 *   ie. that none of them is Out or InOut.
 */
void ArgList_CheckAnnouncement( ArgList_t al )
{
  for ( ; al; al = al->next ) {
    if ( al->m != Mode_In )
      Error_Simple( "argument in ANNOUNCEMENT not passed IN" );
  }
}

/* 
 * Check that the arguments in "al" are suitable for an EXCEPTION:
 *   ie. that none of them requires a deep free
 */
void ArgList_CheckException( ArgList_t al )
{
  for ( ; al; al = al->next ) {
    Type_t t = al->f->type;
    if ( Type_IsLarge( t ) ||
	 (t->kind == TypeKindPredef && t->u.p.t == TypeString)) {
      Error_Symbol( "EXCEPTION argument '%s' needs a deep free",
		     al->f->il->i );
    }
  /*
    if ( Type_IsDeep( al->f->type ) )
      Error_Symbol( "EXCEPTION argument '%s' needs a deep free",
		    al->f->il->i );
   */

  }
}

/*
 * Dump argument defns in Python to initialise "al"
 */
void ArgList_DumpPython( ArgList_t list , ArgKind kind)
{
  char *parm_or_res;

  if ( kind == ArgKind_Parameter ) {
    parm_or_res  = "Parameter";
  } else if ( kind == ArgKind_Result ) {
    parm_or_res  = "Result";
  } else {
    Error_Bug( "ArgList_DumpPython: unknown ArgKind" );
  }
  printf ("al = []\n");

  for ( ; list ; list = list->next ) {
    Type_t           type   = list->f->type;
    IdentifierList_t il;

    for ( il = list->f->il; il; il = il->next ) {

      printf( "arg = ModBEOps.%s()\n", parm_or_res );
      
      printf( "arg.name = '%s'\n", il->i->name );
      printf( "arg.type = %s\n", type->python_name );
      
      printf( "arg.mode = '");
      switch (list->m) {
      case Mode_In:
	printf("IN");
	break;
      case Mode_Out:
	printf("OUT");
	break;
      case Mode_InOut:
	printf("INOUT");
	break;
      case Mode_Result:
	printf("RESULT");
	break;
      default:
	printf("UNKNOWN");
      }
      printf("'\n");

      printf( "arg.isLarge = %s\n", Type_IsLarge( type ) ? "1" : "0" );
      
      printf( "al.append( arg )\n" );
    }
  }
}
