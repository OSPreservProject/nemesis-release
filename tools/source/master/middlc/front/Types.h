
#ifndef _Types_h_
#define _Types_h_

/*
 * Types.h : manipulating types in the ast.
 */

typedef enum { 
  TypeBoolean,
  TypeShortCard,
  TypeCard,
  TypeLongCard,
  TypeShortInt,
  TypeInt,
  TypeLongInt,
  TypeReal,
  TypeLongReal,
  TypeString,
  TypeOctet,
  TypeChar,
  TypeAddress,
  TypeWord
} TypePredef_t;

typedef enum {
  TypeKindPredef,
  TypeKindEnum,
  TypeKindArray,
  TypeKindBitSet,
  TypeKindSet,
  TypeKindSequence,
  TypeKindRecord,
  TypeKindChoice,
  TypeKindIref,
  TypeKindRef,
  TypeKindAlias,
  TypeKindBogus
} TypeKind_t;
  

#define NULL_Type ((Type_t)0)
struct _Type_t {
  Decl_t  decl;
  char   *python_name;          /* NULL <=> anonymous */
  Type_t  reftype;
  bool_t  isDeep;

  TypeKind_t  kind;
  union {

    /* Predefined type */
    struct {
      TypePredef_t t;
    } p;

    /* Enumeration */
    struct {
      IdentifierList_t list;
      SymbolTable_t values;
    } e;

    /* Array */
    struct {
      int size;
      Type_t base;
    } a;

    /* Bit set */
    struct { 
      int size;
    } bs;

    /* Set */
    struct {
      Type_t base;
    } st;
    
    /* Sequence */
    struct {
      Type_t base;
    } sq;

    /* Record */
    struct {
      FieldList_t fields;
    } r;

    /* Choice */
    struct {
      Type_t base;
      CandidateList_t list;
    } c;

    /* Ifref */
    struct {
      Identifier_t name;
    } i;

    /* Ref */
    struct {
      Type_t base;
    } rf;

    /* Alias */
    struct {
      Type_t base;
    } al;
  } u;
};

#define NULL_TypeList ((TypeList_t)0)
struct _TypeList_t {
  Type_t t;
  TypeList_t next;
};

extern Type_t New_Type( void );

extern Type_t Type_Predefined( TypePredef_t t );
extern Type_t Type_NewEnum( IdentifierList_t l );
extern Type_t Type_NewArray( int size, Type_t base );
extern Type_t Type_NewBitSet( int size );
extern Type_t Type_NewSet( Type_t base );
extern Type_t Type_NewRef( Type_t base );
extern Type_t Type_NewSequence( Type_t base );
extern Type_t Type_NewRecord( FieldList_t fields );
extern Type_t Type_NewChoice( Type_t base, CandidateList_t l );
extern Type_t Type_NewAlias( Type_t base );
extern Type_t Type_ScopedName( ScopedName_t sn );
extern Type_t Type_LocalName( Identifier_t name );
extern Type_t Type_IrefName( Identifier_t name );
extern bool_t Type_IsLarge( Type_t t );
extern bool_t Type_IsDeep( Type_t t );
extern bool_t Type_IsAnonymous( Type_t t );

extern void   Type_DumpPredefined( void );
extern void   Type_DumpPython( Type_t t );

extern TypeList_t TypeList_Append( TypeList_t list, Type_t type );

#endif /* _Types_h_ */


