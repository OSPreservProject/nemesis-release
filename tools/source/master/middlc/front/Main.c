
/*
 * MAIN module for the middlc compiler front end.
 */

#include "Middlc.h"

#define PATHSIZE (40)

static char *banner = "$Id: Main.c 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $";

static void Usage( char *progName );

struct _Globals globals;

/*
 * Main procedure 
 */
int main( int argc, char *argv[] )
{
  int i;
  char *p;
  char *iname = NULL;
  Spec_t spec = NULL_Spec;

  globals.symbols   = New_SymbolTable();
  globals.types     = NULL_DeclList;
  globals.all_intfs = NULL_InterfaceList;
  globals.needs     = New_SymbolTable();
  globals.extends   = New_SymbolTable();
  globals.op_count  = 0;

  globals.pathsize = 0;
  globals.path[ globals.pathsize++ ] = ".";

  globals.suffixsize = 0;
  globals.suffices[ globals.suffixsize++ ] = ".if";
  globals.suffices[ globals.suffixsize++ ] = "";

  globals.mode  = Out_Python;
  globals.pmode = ParserMode_Extends;

  globals.banner = banner;

  globals.local  = False;
  globals.top    = True;
  globals.verb   = False;
  globals.errors = False;

  for (i = 1; i < argc; i++)
    {
      p = argv[i];
      if (*p++ == '-')
	{
	  switch (*p ) 
	    {
	    case 'I':
	      globals.path[ globals.pathsize++ ] = ++p;
	      break;
	      
	    case 'S':
	      globals.suffices[ globals.suffixsize++] = ++p;
	      break;

	    case 'n':
	      globals.mode = Out_ParseOnly;
	      break;
      
	    case 'x':
	      globals.mode = Out_Xref;
	      break;	      
	      
	    case 'p':
	      globals.mode = Out_Python;
	      break;

	    case 'M':
	      globals.mode = Out_MakeDepend;
	      break;

	    case 'v':
	      globals.verb = True;
	      break;

	    default:
	      Usage(argv[0]);
	      return(1);
	    }
	} 
      else
	{
	  iname = argv[ i ];
	}
    }
  if ( !iname ) 
    {
      Usage(argv[0]);
      exit(1);
    }

  if ( globals.verb )
    {
      fprintf( stderr, "%s :\n", banner );
      fprintf( stderr, "Include path:\n" );
      for( i = 0; i < globals.pathsize; i ++ )
	{
	  fprintf( stderr, "   %s\n", globals.path[i] );
	}
      fprintf( stderr, "Suffix list:\n" );
      for( i = 0; i < globals.suffixsize; i ++ )
	{
	  fprintf( stderr, "   %s\n", globals.suffices[i] );
	}
      fprintf( stderr, "Top level interface is %s\n", iname );
    }

  FileStack_Init( globals.pathsize, globals.path,
		  globals.suffixsize, globals.suffices );
  if ( FileStack_Push( iname, True ) ) 
      return(1);

  spec = Parser_Parse( );
  FileStack_Pop();

  if ( globals.verb ) 
    {
      fprintf( stderr, "Parsing complete.\n" );
    }

  if ( globals.errors ) 
    {
      fprintf( stderr, "%s: errors encountered => no output generated.\n",
	       argv[0] );
      exit(1);
    }

  switch( globals.mode ) 
    {
    case Out_ParseOnly:
      break;
      
    case Out_Xref:
      fprintf( stderr, "%s: Xref mode not implemented. Sorry.\n", argv[0] );
      return 1;

    case Out_Python:
      Spec_DumpPython( spec );
      break;

    case Out_MakeDepend:
      Spec_MakeDepend( spec );
      break;
    }
  return 0;
}


static void Usage( char *progName )
{
  fprintf( stderr, "Usage: %s [-Idir ...] [-Ssuffix ...] [-nxpM] [-v] intf\n",
	  progName);
  return;
}

