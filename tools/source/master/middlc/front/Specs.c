
/*
 * Specs.c : the top-level structures returned by the parser.
 */

#include "Middlc.h"
#include "Specs.h"

#include <time.h>
#include <unistd.h>

Spec_t New_IntfSpec( Interface_t intf )
{
  Spec_t s = new(Spec);
  
  s->intf = intf;
  return s;
}


/* 
 * Dump a spec as Python program.
 */

void Spec_DumpPython( Spec_t s )
{
  printf( "import ModBE\n" );
  printf( "import ModBEIface\n" );
  printf( "import ModBEOps\n" );
  printf( "import ModBETypes\n" );
  printf( "import ModBEImports\n" );

  Type_DumpPredefined();

  Interface_DumpPython( s->intf );
}

/* 
 * Dump make dependency line for a spec.
 */

void Spec_MakeDepend( Spec_t s )
{
  printf( "%s.ih %s.def.c IDC_S_%s.c IDC_M_%s.c IDC_M_%s.h : \\\n    ",
	  s->intf->name->name, s->intf->name->name, s->intf->name->name,
	  s->intf->name->name, s->intf->name->name );

  Interface_MakeDepend( s->intf );
  printf( "\n");
}

/*  
 * Emit Python to initialise banner fields in the Python object "i"
 */

void Spec_DumpBannerInit( )
{
  time_t theTime = time((time_t *) 0);
  char  *timestr = ctime(&theTime);
  char   hoststr[200];

#if 0
  extern int gethostname( char *address, int address_len );
#endif

  timestr[ strlen(timestr) - 1 ] = '\0'; /* nuke \n */

  if ( gethostname( hoststr, 200 ) ) {
    fprintf(stderr, "Failed to get hostname.\n");
    exit(1);
  }

  printf("i.frontEnd = '%s'\n", globals.banner );
  printf("i.compTime = '%s'\n", timestr );
  printf("i.userName = '%s'\n", getlogin() );
  printf("i.machine = '%s'\n", hoststr );

}
