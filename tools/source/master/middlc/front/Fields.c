
/*
 * Fields.c : Creating and manipulating fields.
 */

#include "Middlc.h"
#include "Fields.h"

/*
 * Create a new Field
 */
Field_t New_Field( IdentifierList_t list, Type_t type )
{
  Field_t f = new(Field);

  f->il = list;
  f->type = type;
  return f;
}


/* 
 * Create a new field list
 */
FieldList_t New_FieldList( Field_t f )
{
  FieldList_t fl = new(FieldList);

  fl->f = f;
  fl->next = NULL_FieldList;
  return fl;
}

/*
 * Append a field to a list, creating the list if necessary
 */
FieldList_t FieldList_Append( FieldList_t list, Field_t f )
{
  FieldList_t fl = New_FieldList( f );

  if (list) {
    FieldList_t flm;

    for( flm = list; flm->next; flm = flm->next );
    flm->next = fl;
    return list;
  } else {
    return fl;
  }
}


