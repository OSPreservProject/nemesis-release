
#ifndef _Interfaces_h_
#define _Interfaces_h_

/*
 * Interfaces are the top-level structures returned by the parser,
 */

#define NULL_Interface ((Interface_t)0)
struct _Interface_t {
  Identifier_t name;
  Identifier_t instance;
  bool_t local;
  Interface_t  extends;
  InterfaceList_t needs;
  DeclList_t decls;
  DeclList_t types;
  Type_t iref_type;
  SymbolTable_t symbols;
  char *filename;
  bool_t python_dumped;
  bool_t depend_dumped;
};

#define NULL_InterfaceList ((InterfaceList_t)0)
struct _InterfaceList_t {
  Interface_t i;
  InterfaceList_t next;
};

extern bool_t      Interface_Start( Identifier_t name, bool_t local );

extern Interface_t New_Interface(bool_t local,
				 Interface_t extends,
				 InterfaceList_t needs,
				 DeclList_t decls );

extern Interface_t Interface_Extends( Identifier_t name );

extern Interface_t Interface_Needs( Identifier_t name, Identifier_t instance );

extern InterfaceList_t InterfaceList_Append( InterfaceList_t list,
				     Interface_t i );

/* Get the symbols from the specified interface as a declaration, or */
/* return NULL_Decl */
extern Decl_t Interface_GetDecl( Interface_t intf, Identifier_t name );

/* The same for the current interface */
extern Decl_t Interface_LocalGetDecl( Identifier_t name );

/* Add a new symbol to the current table */
extern void Interface_AddSymbol( Decl_t d );

/* Dump the python for top-level interface */
extern void Interface_DumpPython( Interface_t intf );

/* Dump the make dependencies for intf and its imports */
extern void Interface_MakeDepend( Interface_t intf );

#endif _Interfaces_h_
