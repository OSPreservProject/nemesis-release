
#ifndef _Fields_h_
#define _Fields_h_

/*
 * Fields are used in arguments and as components of records.
 */

#define NULL_Field ((Field_t)0)
struct _Field_t {
  IdentifierList_t il;
  Type_t type;
};

#define NULL_FieldList ((FieldList_t)0)
struct _FieldList_t {
  Field_t f;
  FieldList_t next;
};
  
extern Field_t New_Field( IdentifierList_t list, Type_t type );

extern FieldList_t New_FieldList( Field_t f );

extern FieldList_t FieldList_Append( FieldList_t list, Field_t f );

extern void        FieldList_DumpPython( FieldList_t list );

#endif /* _Fields_h_ */
