
/*
 * Interfaces.c : the top-level structures returned by the parser.
 */

#include "Middlc.h"
#include "Interfaces.h"

/* 
 * If the handling of nested includes (especially A NEEDS B,
 * B EXTENDS C etc.) worries you, observe that neither B nor C
 * get their operations registered, since A only NEEDS them;
 * thus C is parsed in NEEDS mode.  Only interfaces that the
 * top level EXTENDS are parsed in EXTENDS mode.
 */

/* 
 * Start processing of a new interface.
 */
bool_t Interface_Start( Identifier_t name, bool_t local )
{
  Type_t       t;
  char         buf[MAX_STRING_LENGTH];

  globals.intf  = new(Interface);
  globals.name  = name;
  globals.local = local;

  t           = New_Type();
  t->kind     = TypeKindIref;
  t->u.i.name = name;
  t->isDeep   = True;

  sprintf( buf, "type_%s_IREF", name->name );
  t->python_name = strdup( buf );

  globals.intf->iref_type = t;

  SymbolTable_Enter( globals.needs, name->name, (caddr_t) globals.intf );

  return local;
}

/*
 * Create a new interface as part of the final reduction rule.
 * This will need some work
 */
Interface_t New_Interface( bool_t local,
			  Interface_t extends,
			  InterfaceList_t needs,
			  DeclList_t decls )
{
  Interface_t i = globals.intf;

  i->name = globals.name;
  i->extends = extends;
  i->needs = needs;
  i->decls = decls;
  i->local = local;
  i->symbols = globals.symbols;
  i->types = globals.types;
  i->filename = FileStack_Filename();
  i->python_dumped = False;
  i->depend_dumped = False;
  i->instance = NULL_Identifier;
  if ( local && !globals.local ) {
    Error_Symbol( "'%s' is local and included from a non-local spec",
		 globals.name );
  };

  globals.all_intfs = InterfaceList_Append( globals.all_intfs, i );

  return i;
}


/*
 * Create a new bogus interface.
 */
Interface_t Interface_NewBogus()
{
  Interface_t i = new(Interface);

  i->name = New_Identifier( 0, "__bogus__");
  i->extends = NULL_Interface;
  i->needs = NULL_InterfaceList;
  i->decls = NULL_DeclList;
  i->local = True;
  i->symbols = New_SymbolTable();
  i->types = NULL_DeclList;
  i->filename = "__bogus__";
  i->python_dumped = False;
  i->depend_dumped = False;
  i->instance = NULL_Identifier;

  return i;
}


/*
 * Find an interface and parse it for types and exceptions only.
 */
Interface_t Interface_Needs( Identifier_t name, Identifier_t instance )
{
  Interface_t	newif       = NULL_Interface;
  int		oldop_count = globals.op_count;
  Interface_t	oldif       = globals.intf;
  Parser_Mode_t	oldpmode    = globals.pmode;
  bool_t	top         = globals.top;
  bool_t	local       = globals.local;
  Identifier_t	oldname     = globals.name;
  bool_t	found;

  /* First look up this interface in the table of ones we have already */
  /* parsed. If it exists, we return it. */
  SymbolTable_Lookup( globals.needs, name->name, (caddr_t *)&newif, &found );
  if ( found ) return newif;
    
  /* Otherwise push the parser mode and parse this one. */
  globals.pmode = ParserMode_Needs;
  globals.top   = False;

  if ( !FileStack_Push( name->name, False )) {
    if ( globals.verb ) {
      fprintf( stderr, "Parsing interface %s.\n", name->name );
    }
    newif = Parser_Parse( )->intf;
    if (newif) newif->instance = instance;
    FileStack_Pop();
  }

  if ( !newif ) newif = Interface_NewBogus();

  globals.symbols    = New_SymbolTable();
  globals.types      = NULL_DeclList;
  globals.op_count   = oldop_count;

  globals.intf    = oldif;
  globals.pmode   = oldpmode;
  globals.top     = top;
 
  if ( (!local) && globals.local )
    Error_Simple( "non-LOCAL interface cannot need a LOCAL interface" );

  globals.local   = local;
  globals.name    = oldname;

  /* Regardless of whether this interface was successfully parsed, we */
  /* create an entry in the symbol table to mark it as done. */
  SymbolTable_Enter( globals.needs, name->name, (caddr_t)newif );

  return newif;
}


/*
 * Find an interface and parse it as a supertype.
 *
 * The mechanics of this are much like the NEEDS version. Note that at
 * this stage NO needed interfaces have been parsed (due to the syntax
 * of the language EXTENDS always precedes NEEDS). Hence a duplicate at
 * this stage is an error.
 *
 * Note that globals.pmode is not stacked.
 */
Interface_t Interface_Extends( Identifier_t name )
{
  Interface_t newif    = NULL_Interface;
  Interface_t oldif    = globals.intf;
  bool_t top           = globals.top;
  bool_t local         = globals.local;
  Identifier_t oldname = globals.name;
  bool_t found;

  /* First look up this interface in the table of ones we have already */
  /* parsed. If it exists, we return it. */
  SymbolTable_Lookup(globals.needs, name->name, (caddr_t *)&newif, &found );
  if ( found ) {
    if ( globals.pmode == ParserMode_Extends ) {
      Error_Simple("Circular EXTENDS relation" );
    }
    return newif;
  }
  
  /* Next, if we're in a NEEDS chain, parse as if a NEEDS */
  if ( globals.pmode == ParserMode_Needs )
    return Interface_Needs( name, NULL_Identifier );

  globals.top = False;
  /* Parse the interface */
  if ( !FileStack_Push( name->name, False )) {
    if ( globals.verb ) {
      fprintf( stderr, "Parsing interface %s.\n", name->name );
    }
    newif = Parser_Parse( )->intf;
    if (newif) newif->instance = NULL_Identifier;
    FileStack_Pop();
  }

  if ( !newif ) newif = Interface_NewBogus();

  globals.symbols    = New_SymbolTable();
  globals.types      = NULL_DeclList;

  /* DON'T reset globals.op_count, so that operations get numbered
     consistently */

  globals.top   = top;

  if ( (!local) && globals.local )
    Error_Simple( "non-LOCAL interface cannot extend a LOCAL interface" );

  globals.intf  = oldif;
  globals.local = local;
  globals.name  = oldname;

  /* Register its name */
  SymbolTable_Enter(globals.needs, name->name, (caddr_t)newif );
  SymbolTable_Enter(globals.extends, name->name, (caddr_t)newif );

  return newif;
}

/*
 * Append an interface to an interface list, creating the list if
 * necessary.
 */
InterfaceList_t InterfaceList_Append( InterfaceList_t list,
				     Interface_t i )
{
  InterfaceList_t il = new(InterfaceList);
  il->i = i;
  il->next = NULL_InterfaceList;
  
  if (list) {
    InterfaceList_t l;

    for( l=list; l->next; l=l->next );
    l->next = il;
    return list;
  } else {
    return il;
  }
}


/*
 * Get the symbols from the specified interface as a declaration, or
 * return NULL_Decl
 */
Decl_t Interface_GetDecl( Interface_t intf, Identifier_t name )
{
  bool_t found;
  Decl_t d;

  SymbolTable_Lookup( intf->symbols, name->name, (caddr_t *)&d, &found );
  
  return found ? d : NULL_Decl;
}

/*
 * The same for the current interface
 */
Decl_t Interface_LocalGetDecl( Identifier_t name )
{
  bool_t found;
  Decl_t d;

  SymbolTable_Lookup( globals.symbols, name->name, (caddr_t *)&d, &found );
  
  return found ? d : NULL_Decl;
}


/*
 * Add a new symbol to the current table
 */
void Interface_AddSymbol( Decl_t d )
{
  SymbolTable_Enter( globals.symbols, d->name->name, (caddr_t)d );
}

/* 
 * Dump Python to append "intf" to "i.imports".
 */
static void DumpImport( Interface_t intf, char *needs_or_extends )
{
  printf("imp = ModBEImports.Import()\n");
  printf("imp.mode = '%s'\n", needs_or_extends);
  printf("imp.intf = intf_%s\n", intf->name->name );
  printf("imp.name = '%s'\n", intf->name->name );
  printf("i.imports.append( imp )\n");
}

/*
 * Dump the interface defn in python
 */
static void DumpPython1( Interface_t intf, bool_t extends )
{
  InterfaceList_t il;
  DeclList_t dl;
  int i;

  if ( intf->python_dumped ) return;

  intf->python_dumped = True;

  /* Run through all needed interfaces, listing them as imports. For */
  /* each one, we recursively ask them to dump their stuff to python. */

  /* First dump any interface that this extends */
  if ( extends && intf->extends ) {
    DumpPython1( intf->extends, extends );
  }

  /* Next dump all the others that this one needs. */
  for( il = intf->needs; il; il = il->next ) {
    if ( il->i != intf->extends ) 
      DumpPython1( il->i, False );
  }

  printf("i = intf_%s\n", intf->name->name );

  Spec_DumpBannerInit();

  printf("i.idlFile = '%s'\n", intf->filename );
  printf("i.name = '%s'\n", intf->name->name );
  printf("i.isModule = 0\n" );
  printf("i.local = %d\n", intf->local );

  printf("i.imports = []\n" );

  if ( intf->extends )
    DumpImport( intf->extends, "EXTENDS" );

  for ( il = intf->needs; il; il = il->next )
    DumpImport( il->i, "NEEDS");

  for ( dl = intf->types, i = 0; dl; dl = dl->next, i++ ) {
    Decl_DumpTypePython ( intf, dl->d, i );
  }

  printf("i.excs = []\n" );
  printf("i.ops = []\n" );
   
  /* Now dump all our declarations */
  for( dl = intf->decls; dl; dl=dl->next ) {
    Decl_DumpPython( dl->d, extends, intf );
  }

  printf("i = i.init()\n\n###\n\n" );

}


static void DumpInterfaceVariables ( )
{
  InterfaceList_t il;

  for( il = globals.all_intfs; il; il = il->next ) {
    printf( "intf_%s = ModBEIface.Interface()\n", il->i->name->name );

    printf("i = intf_%s\n", il->i->name->name );
    printf("i.types = []\n" );

    /* The IREF type for this interface is a special case.  Bletch */

    printf( "t = ModBETypes.Type()\n");
    printf( "t.localName = 'clp'\n" );
    printf( "t.intf = i\n");
    printf( "t.name = '%s_clp'\n", il->i->name->name);
    Type_DumpPython( il->i->iref_type );
    printf( "%s = t\n",  il->i->iref_type->python_name );
    printf( "i.types.append( t )\n\n" );
  }

}

/*
 * Dump the top-level interface defn in python
 */
void Interface_DumpPython( Interface_t intf )
{
  DumpInterfaceVariables();
  DumpPython1( intf, True );
}

/*
 * Dump the make dependencies for intf and its imports
 */
void Interface_MakeDepend( Interface_t intf )
{
  InterfaceList_t il;

  if ( intf->depend_dumped ) return;

  intf->depend_dumped = True;

  printf( " %s", intf->filename);

  if ( intf->extends )
    Interface_MakeDepend( intf->extends );

  for ( il = intf->needs; il; il = il->next )
    Interface_MakeDepend( il->i );

}
