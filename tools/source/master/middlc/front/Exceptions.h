
#ifndef _Exceptions_h_
#define _Exceptions_h_

/*
 * Exceptions are just scoped identifiers, really.
 */

#define NULL_ExcList ((ExcList_t)0)
struct _ExcList_t {
  Decl_t exc; /* the exception itself */
  ExcList_t next;
};


extern ExcList_t New_ExcListScoped( ScopedName_t sn );
extern ExcList_t New_ExcListLocal( Identifier_t i );
extern ExcList_t ExcList_Append( ExcList_t list, ExcList_t e );
extern void      ExcList_DumpPython( ExcList_t list );

#endif /* _Exceptions_h_ */
