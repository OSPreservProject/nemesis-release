
/*
 * Errors.c : handling errors during compilation.
 */

#include "Middlc.h"
#include "Errors.h"

static void Flag( char *buf );

/*
 * Simple error - just a string
 */
void Error_Simple( char *msg )
{
  Flag( msg );
}

/*
 * Error with associated symbol
 */
void Error_Symbol( char *msg, Identifier_t name )
{
  char buf[ MAX_STRING_LENGTH ];

  sprintf( buf, msg, name->name );
  Flag( buf );
}

/*
 * Error with two symbols
 */
void Error_Symbols( char *msg, Identifier_t i1, Identifier_t i2 )
{
  char buf[ MAX_STRING_LENGTH ];

  sprintf( buf, msg, i1->name, i2->name );
  Flag( buf );
}


/*
 * Error with a scoped name
 */
void Error_Scoped( char *msg, ScopedName_t sn )
{
  char buf1[ MAX_STRING_LENGTH ];
  char buf2[ MAX_STRING_LENGTH ];

  sprintf( buf1, "%s.%s", sn->scope->name, sn->name->name );
  sprintf( buf2, msg, buf1 );
  Flag( buf2 );
}

/*
 * Error with associated string
 */
void Error_String( char *msg, char *str )
{
  char buf[ MAX_STRING_LENGTH ];

  sprintf( buf, msg, str );
  Flag( buf );
}

/* 
 * Fatal error - die
 */
void Error_Fatal( char *msg )
{
  Error_Simple( msg );
  exit(1);
}

/* 
 * BUG! - die
 */
void Error_Bug( char *msg )
{
  fprintf( stderr, "ATTN BUG: %s\n", msg );
  exit(1);
}

/*
 * Actually flag an error
 */
static void Flag( char *buf )
{
  globals.errors = True;
  fprintf( stderr, "%s:%d: %s\n", 
	  FileStack_Filename(),
	  FileStack_Lineno(),
	  buf );
}
