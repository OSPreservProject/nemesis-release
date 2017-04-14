
/* Decls.c : routines for manipulating Decls. */

#include "Middlc.h"
#include "Decls.h"

static bool_t Duplicate( Decl_t d, Identifier_t name );

/*
 * Take a list (which may be null) and a decl, and return the list
 * with the decl appended.
 */
DeclList_t DeclList_Append( DeclList_t list,
			   Decl_t decl )
{
  DeclList_t dl = new(DeclList);

  dl->d    = decl;
  dl->next = NULL_DeclList;

  if (list) {
    DeclList_t l = list;

    while( l->next ) l=l->next;
    l->next = dl;
    return list;
  } else {
    return dl;
  }
}

/*
 * Create a new announcement decl
 */
Decl_t New_AnnDecl( Identifier_t name,
		   ArgList_t args )
{
  Decl_t d = new(Decl);

  if ( Duplicate( d, name ) ) return d;

  d->kind = Decl_Ann;
  d->u.an.number = globals.op_count++;
  d->u.an.args   = args;

  ArgList_CheckDuplicateIds( args, NULL_ArgList );
  ArgList_CheckAnnouncement( args );

  return d;
}
  
/*
 * Create a new exception decl
 */
Decl_t New_ExcDecl( Identifier_t name,
		   ArgList_t args )
{
  Decl_t d = new(Decl);

  if ( Duplicate( d, name ) ) return d;

  d->kind = Decl_Exc;
  d->u.e.args = args;

  ArgList_CheckDuplicateIds( args, NULL_ArgList );
  ArgList_CheckException( args );

  return d;
}

static char *PythonTypeName( Identifier_t intf, Identifier_t name )
{
  char buf[MAX_STRING_LENGTH];

  sprintf( buf, "type_%s_%s", intf->name, name->name );
  return strdup( buf );
}

/*
 * Add a new (so far anonymous) type to the current interface
 */
void Decl_NewType( Type_t type )
{
  Decl_t d = new(Decl);

  type->decl        = d;
  type->python_name = NULL;

  d->name = NULL_Identifier;
  d->line = FileStack_Lineno();
  d->kind = Decl_Type;
  d->u.t.type = type;

  globals.types = DeclList_Append( globals.types, d );
}

/*
 * Give a type decl its name in a top-level type declaration
 */
Decl_t Decl_NameType( Identifier_t name, Type_t type )
{
  Decl_t d;

  if  ( ! Type_IsAnonymous ( type ) ) {
    type = Type_NewAlias( type );
    Decl_NewType( type );
  }

  d = type->decl;

  if ( Duplicate( d, name ) ) return d;
  d->kind = Decl_Type;

  type->python_name = PythonTypeName( globals.name, name );
  /* assert ( ! Type_IsAnonymous ( t ) ); */

  return d;
}



/*
 * Create a new proc decl
 */
Decl_t New_ProcDecl( Identifier_t name,
		    ArgList_t args,
		    ArgList_t results,
		    ExcList_t excs,
		    bool_t    returns )
{
  Decl_t d = new(Decl);

  if ( Duplicate( d, name ) ) return d;

  d->kind = Decl_Proc;
  d->u.p.number  = globals.op_count++;
  d->u.p.args    = args;
  d->u.p.results = results;
  d->u.p.excs    = excs;
  d->u.p.returns = returns;

  ArgList_CheckDuplicateIds( args, results );

  return d;
}


/*
 * Check that this declaration is not named the same as another in the
 * same file. If it is, issue an error and return the declaration as a
 * bogus one.
 */
static bool_t Duplicate( Decl_t d, Identifier_t name )
{
  d->name = name;
  d->line = FileStack_Lineno();

  if ( Interface_LocalGetDecl( name ) ) {
    Error_Symbol( "'%s' is redeclared in this spec.", name );
    d->kind = Decl_Bogus;
    return True;
  }

  Interface_AddSymbol( d );

  d->kind = Decl_New;
  return False;
}
  
/*
 * Attempt to narrow the decl to a type declaration, and return the
 * Type_t or NULL_Type.
 */
Type_t Decl_GetAsType( Decl_t d )
{
  if (d->kind == Decl_Type ) {
    return d->u.t.type;
  } else {
    return NULL_Type;
  }
}


/*
 * Return the kind of a declaration 
 */
Decl_Kind_t Decl_Kind( Decl_t d )
{
  return d->kind;
}

/*
 * Dump the python for a type, supplying a name for anonymous types.
 */
void Decl_DumpTypePython( Interface_t intf, Decl_t d, int i )
{
  char *intfname = intf->name->name;
  char *declname; 
  Type_t t = Decl_GetAsType( d );

  if ( !t ) Error_Bug( "Decl_DumpTypePython: decl is not a type" );

  if ( Type_IsAnonymous( t ) ) {
    char buf [MAX_STRING_LENGTH ];
    sprintf( buf, "anon%d", i );
    d->name = New_Identifier( d->line, buf );
    sprintf( buf, "type_%s_anon%d", intfname, i );
    t->python_name = strdup( buf );
  }

  if ( t->reftype && Type_IsAnonymous( t->reftype ) ) {
    Type_t rt = t->reftype;
    Decl_t rd = rt->decl;
    char buf [MAX_STRING_LENGTH ];

    sprintf( buf, "%s_ptr", d->name->name );
    rd->name = New_Identifier( d->line, buf );

    sprintf( buf, "%s_ptr", t->python_name );
    rt->python_name = strdup( buf );
  }

  declname = d->name->name;
  printf( "t = ModBETypes.Type()\n");
  printf( "t.localName = '%s'\n", declname );
  printf( "t.intf = i\n");
  printf( "t.name = '%s_%s'\n", intfname, declname);
  Type_DumpPython( d->u.t.type );
  printf( "%s = t\n", d->u.t.type->python_name );
  printf( "i.types.append( t )\n\n" );

}




/*
 * Dump the python
 */
void Decl_DumpPython( Decl_t d, bool_t ops, Interface_t intf )
{
  char *intfname = intf->name->name;
  char *declname = d->name->name;

  switch ( d->kind ) {
  case Decl_Proc:
    if ( ops ) {
      printf( "o = ModBEOps.Operation()\n");
      printf( "o.number = %d\n", d->u.p.number );
      printf( "o.name = '%s'\n", declname );
      printf( "o.ifName = '%s'\n", intfname );
      printf( "o.intf = i\n");
      printf( "o.type = 'PROC'\n" );
      ArgList_DumpPython( d->u.p.args, ArgKind_Parameter );
      printf( "o.pars = al\n" );
      ArgList_DumpPython( d->u.p.results, ArgKind_Result );
      printf( "o.results = al\n");
      ExcList_DumpPython( d->u.p.excs );
      printf( "o.raises = el\n" );
      printf( "o.returns = %d\n", d->u.p.returns );
      printf( "i.ops.append( o )\n");
    }
    break;

  case Decl_Ann: 
    if ( ops ) {
      printf( "o = ModBEOps.Operation()\n");
      printf( "o.number = %d\n", d->u.an.number );
      printf( "o.name = '%s'\n", declname );
      printf( "o.ifName = '%s'\n", intfname );
      printf( "o.intf = i\n");
      printf( "o.type = 'ANNOUNCEMENT'\n" );
      ArgList_DumpPython( d->u.an.args, ArgKind_Parameter );
      printf( "o.pars = al\n" );
      printf( "o.results = []\n");
      printf( "o.raises = []\n" );
      printf( "i.ops.append( o )\n");
    }
    break;

  case Decl_Exc:
    {
      char buf[ MAX_STRING_LENGTH ];
      sprintf( buf, "excn_%s_%s", intfname, declname );
      d->u.e.python_name = strdup( buf );
    }
    printf( "e = ModBEOps.Exception()\n");
    printf( "e.name = '%s'\n", declname );
    printf( "e.ifName = '%s'\n", intfname );
    printf( "e.intf = i\n");
    ArgList_DumpPython( d->u.e.args, ArgKind_Parameter );
    printf( "e.pars = al\n" );
    printf( "%s = e\n", d->u.e.python_name );
    printf( "i.excs.append( e )\n" );
    break;

  case Decl_Type: 
    /* Type declarations have already been emitted by Decl_DumpTypePython */
    break;

  default:
    Error_Bug( "Decl_DumpPython: bogus Decl_Kind\n" );

  }
}
