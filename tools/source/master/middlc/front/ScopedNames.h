
#ifndef _ScopedName_h_
#define _ScopedName_h_

/*
 * ScopedNames are created when an intf.name is encountered in the
 * parser.
 */

#define NULL_ScopedName ((ScopedName_t)0)
struct _ScopedName_t {
  Identifier_t scope;
  Identifier_t name;
  Decl_t decl;
};

/* Create a new scoped name */
extern ScopedName_t New_ScopedName( Identifier_t scope, Identifier_t name );

/* Return the declaration corresponding to a scoped name */
/* or NULL_Decl. */
extern Decl_t ScopedName_GetDecl( ScopedName_t sn );

#endif /* _ScopedName_h_ */
