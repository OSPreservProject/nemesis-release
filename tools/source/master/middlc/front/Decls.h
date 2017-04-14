
#ifndef _Decls_h_
#define _Decls_h_

/*
 * Decls.h : stuff for dealing with decls. These are the basic
 * building blocks of interface specs, and come in four
 * flavours: types, procs, announcements, and exceptions.
 */

typedef enum { 
  Decl_Type,		/* Type */
  Decl_Proc,		/* Method signature */
  Decl_Ann,		/* Announcement signature */
  Decl_Exc,		/* Exception signature */
  Decl_Bogus,		/* Error: name already taken */
  Decl_New		/* Unassigned as yet */
} Decl_Kind_t;
  
#define NULL_Decl ((Decl_t)0)
struct _Decl_t {
  Decl_Kind_t kind;
  Identifier_t name;
  int line;

  union {
    
    /* Type decl */
    struct {
      Type_t type;
    } t;

    /* Proc decl */
    struct {
      int       number;
      ArgList_t args;
      ArgList_t results;
      ExcList_t excs;
      bool_t	returns;
    } p;

    /* Announcement decl */
    struct {
      int       number;
      ArgList_t args;
    } an;

    /* Exception decl */
    struct {
      char      *python_name;  /* initialised during output phase */
      ArgList_t  args;
    } e;

  } u;
};

#define NULL_DeclList ((DeclList_t)0)
struct _DeclList_t {
  Decl_t d;
  DeclList_t next;
};


extern void   Decl_NewType( Type_t type );

extern Decl_t Decl_NameType( Identifier_t name, Type_t type );

extern Decl_t New_ProcDecl( Identifier_t name,
		    ArgList_t args,
		    ArgList_t results,
		    ExcList_t excs,
		    bool_t    returns );

extern Decl_t New_AnnDecl( Identifier_t name,
		   ArgList_t args );

extern Decl_t New_ExcDecl( Identifier_t name,
		   ArgList_t args );

/* Attempt to narrow the decl to a type declaration, and return the */
/* corresponding type, or NULL_Type */
extern Type_t Decl_GetAsType( Decl_t d );

/* Return the kind of a declaration */
extern Decl_Kind_t Decl_Kind( Decl_t d );

/* Dump the python */
extern void Decl_DumpTypePython( Interface_t intf, Decl_t d, int i );
extern void Decl_DumpPython( Decl_t d, bool_t ops, Interface_t intf );

extern DeclList_t DeclList_Append( DeclList_t l, Decl_t d );

#endif /* _Decls_h_ */
