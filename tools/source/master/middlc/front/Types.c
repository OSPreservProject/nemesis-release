
/*
 * Types.c : managing types in the AST
 */

#include "Middlc.h"
#include "Types.h"



/*
 * Take a list (which may be null) and a type, and return the list
 * with the type appended.
 */
TypeList_t TypeList_Append( TypeList_t list, Type_t type )
{
  TypeList_t tl = new(TypeList);

  tl->t    = type;
  tl->next = NULL_TypeList;

  if (list) {
    TypeList_t l = list;

    while( l->next ) l=l->next;
    l->next = tl;
    return list;
  } else {
    return tl;
  }
}

static Type_t Predefined_Types[ TypeWord + 1 ];

static char * Predefined_Names[ TypeWord + 1 ]  = {
  "type_MIDDL_Boolean",
  "type_MIDDL_ShortCardinal",
  "type_MIDDL_Cardinal",
  "type_MIDDL_LongCardinal",
  "type_MIDDL_ShortInteger",
  "type_MIDDL_Integer",
  "type_MIDDL_LongInteger",
  "type_MIDDL_Real",
  "type_MIDDL_LongReal",
  "type_MIDDL_String",
  "type_MIDDL_Octet",
  "type_MIDDL_Char",
  "type_MIDDL_Address",
  "type_MIDDL_Word"
};

static bool_t Predefined_Deep[ TypeWord + 1 ] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0
};


/*  
 * Dump Python to initialise predefined types.
 */

void Type_DumpPredefined( void )
{
  int i;
  for ( i = 0; i <= TypeWord; i++ )
    printf( "from ModBETypes import %s\n", Predefined_Names[ i ] );

}

/* 
 * Create a new type
 */
Type_t New_Type()
{
  Type_t t = new(Type);

  t->decl         = NULL_Decl;
  t->python_name  = NULL;
  t->reftype      = NULL_Type;
  t->isDeep       = False;

  return t;
}

/* 
 * Create a bogus type
 */
Type_t Type_NewBogus()
{
  Type_t t = new(Type);

  t->kind = TypeKindBogus;

  return t;
}

/*
 * Get hold of a predefined type.
 */
Type_t Type_Predefined( TypePredef_t type )
{
  Type_t t = Predefined_Types[ type ];

  /* Lazily initialise the predefined types! */
  if ( !t ) {
    if ( type == TypeBoolean ) {
      t = Type_NewEnum(
        IdentifierList_Append(
          New_IdentifierList( New_Identifier( 0, "False" ) ),
          New_Identifier( 0, "True" )));
    } else {
      t = New_Type();
      t->kind  = TypeKindPredef;
      t->u.p.t = type;
    }
    t->python_name = Predefined_Names[ type ];
    t->isDeep = Predefined_Deep[ type ];
    Predefined_Types[ type ] = t;
  }

  if ( (type == TypeAddress || type == TypeWord) && (! globals.local) )
    Error_Simple( "Can't use DANGEROUS types in a non-LOCAL interface" );

  return t;
}

/*
 * Create a new enumeration type
 */
Type_t Type_NewEnum( IdentifierList_t l )
{
  Type_t t = New_Type();
  long i = 0;
 
  t->kind = TypeKindEnum;
  t->u.e.list = l;
  t->u.e.values = New_SymbolTable();
  
  for( ; l; l = l->next ) {
    if ( SymbolTable_Enter( t->u.e.values, l->i->name, (caddr_t)(i++) ) ) {
      Error_Symbol( "enumerator element '%s' is repeated", l->i );
      SymbolTable_Delete( t->u.e.values );
      free(t);
      return Type_NewBogus();
    }
  }
  return t;
}

/*
 * Create a new array
 */
Type_t Type_NewArray( int size, Type_t base )
{
  Type_t t = New_Type();

  t->kind = TypeKindArray;
  t->u.a.size = size;
  t->u.a.base = base;
  t->isDeep   = Type_IsDeep( base );
  return t;
}

/*
 * Create a new array
 */
Type_t Type_NewBitSet( int size )
{
  Type_t t = New_Type();

  t->kind = TypeKindBitSet;
  t->u.bs.size = size;

  if ( size > 64 ) {
    Error_Simple( "ARRAY n OF BITS not implemented for n > 64 yet." );
  }
  return t;
}

/*
 * Create a set
 */
Type_t Type_NewSet( Type_t base )
{
  Type_t t  = New_Type();

  t->kind   = TypeKindSet;
  t->u.st.base = base;
  return t;
}

/*
 * Create a ref
 */
Type_t Type_NewRef( Type_t base )
{
  Type_t t;

  if ( base->reftype )
    return base->reftype;

  t = New_Type();
  t->kind       = TypeKindRef;
  t->u.rf.base  = base;
  t->isDeep     = True;

  base->reftype = t;

  Decl_NewType( t );

  if ( ! globals.local )
    Error_Simple( "Can't use REF in a non-LOCAL interface" );

  return t;
}

/*
 * Create a sequence  
 */
Type_t Type_NewSequence( Type_t base )
{
  Type_t t  = New_Type();

  t->kind   = TypeKindSequence;
  t->u.sq.base = base;
  t->isDeep     = True;

  return t;
}

/*
 * Create a record
 */
Type_t Type_NewRecord( FieldList_t fields )
{
  Type_t t  = New_Type();

  t->kind   = TypeKindRecord;
  t->u.r.fields = fields;

  for ( ; fields ; fields = fields->next )
    t->isDeep = t->isDeep || Type_IsDeep( fields->f->type );

  return t;
}

/* 
 * Create a choice
 */
Type_t Type_NewChoice( Type_t base, CandidateList_t l )
{
  Type_t t  = New_Type();

  t->kind   = TypeKindChoice;
  if ( base->kind != TypeKindEnum ) {
    Error_Simple( "Choice discriminator is not an enumeration" );
    free( t );
    return Type_NewBogus();
  }
  t->u.c.base = base;
  t->u.c.list = l;

  for ( ; l ; l = l->next )
    t->isDeep = t->isDeep || Type_IsDeep( l->type );

  return t;
}

/* 
 * Create an alias
 */
Type_t Type_NewAlias( Type_t base )
{
  Type_t t  = New_Type();

  t->kind = TypeKindAlias;
  t->u.al.base = base;
  t->isDeep = Type_IsDeep( base );

  return t;
}

/*
 * Identify a type by means of a scoped name
 */
Type_t Type_ScopedName( ScopedName_t sn )
{
  Decl_t d = ScopedName_GetDecl( sn );
  Type_t t;

  if (!d ) {
    Error_Scoped("'%s' is undefined", sn );
    return Type_NewBogus();
  }
  if ( !(t = Decl_GetAsType( d ) ) ) {
    Error_Scoped("'%s' is not a type.", sn );
    return Type_NewBogus();
  }
  return t;
}

/* 
 * Return a type be means of a scoped name
 */
Type_t Type_LocalName( Identifier_t name )
{
  Decl_t d = Interface_LocalGetDecl( name );
  Type_t t;

  if (!d ) {
    Error_Symbol("'%s' is undefined", name );
    return Type_NewBogus();
  }
  if ( !(t = Decl_GetAsType( d ) ) ) {
    Error_Symbol("'%s' is not a type", name );
  }
  return t;
}

/*
 * Return the interface ref type of an interface.
 */
Type_t Type_IrefName( Identifier_t name )
{
  Interface_t  intf;
  bool_t       found;

  SymbolTable_Lookup( globals.needs, name->name, (caddr_t *)&intf, &found);

  if ( ! found )
  {
    Error_Symbol( "Bogus IREF: %s is not an INTERFACE", name );
    return Type_NewBogus();
  }
  return intf->iref_type;
}

/*
 * Return true if the type "t" is large, false otherwise.
 */
bool_t Type_IsLarge( Type_t t )
{
  if ( t->kind == TypeKindAlias )
    return Type_IsLarge( t->u.al.base );
  else
    return ( t->kind != TypeKindPredef &&
	     t->kind != TypeKindEnum   &&
	     t->kind != TypeKindSet    &&
	     t->kind != TypeKindRef    &&
	     t->kind != TypeKindIref );
}

/*
 * Return true if the type "t" needs a deep free, false otherwise.
 */
bool_t Type_IsDeep( Type_t t )
{
  if ( t )
    return t->isDeep;
  else
    return False;
}

/* 
 * Return whether type "t" is anonymous thus far.
 */
bool_t Type_IsAnonymous( Type_t t )
{
  return ( t->python_name == NULL );
}

/* 
 * Return true iff the type "t" can be an element of a
 * BYTEARRAY.
 */

static bool_t Type_IsByteArray( Type_t t )
{
  switch ( t->kind ) {
  case TypeKindPredef:
    return ( t->u.p.t == TypeOctet || t->u.p.t == TypeChar );

  case TypeKindArray:
    return Type_IsByteArray( t->u.a.base );

  case TypeKindAlias:
    return Type_IsByteArray( t->u.al.base );

  default:
    return False;
  }
}

/* 
 * Return true iff the type "t" can be an element of a
 * BYTESEQUENCE.
 */

static bool_t Type_IsByteSequence( Type_t t )
{
  switch ( t->kind ) {
  case TypeKindPredef:
    return ( t->u.p.t == TypeOctet || t->u.p.t == TypeChar );

  case TypeKindArray:
    return Type_IsByteArray( t->u.a.base );

  case TypeKindSequence:
    return Type_IsByteArray( t->u.sq.base );

  case TypeKindAlias:
    return Type_IsByteSequence( t->u.al.base );

  default:
    return False;
  }
}


/* 
 * Emit Python to initialise the class-specific components of the
 * Python Type() "t"
 */
void Type_DumpPython( Type_t t )
{
  switch ( t->kind ) {

  case TypeKindEnum:
    {
      IdentifierList_t il = t->u.e.list;

      printf( "t.clss = 'ENUMERATION'\n" );
      printf( "enum = ModBETypes.Enumeration()\n" );
      printf( "enum.elems = []\n" );
      for ( ; il; il = il->next ) { 
	printf( "enum.elems.append( '%s' )\n", il->i->name );
      }
      printf( "t.type = enum\n");
    }
    break;

  case TypeKindArray:
    {
      printf( "t.clss = '%s'\n",
	      Type_IsByteArray( t ) ? "BYTEARRAY" : "ARRAY" );
      printf( "arr = ModBETypes.%s()\n",
	      Type_IsByteArray( t ) ? "ByteArray" : "Array" );
      printf( "arr.size = %d\n", t->u.a.size );
      printf( "arr.base = %s\n", t->u.a.base->python_name );
      printf( "t.type = arr\n");
    }
    break;

  case TypeKindBitSet:
    {
      printf( "t.clss = 'BITSET'\n" );
      printf( "bs = ModBETypes.BitSet()\n" );
      printf( "bs.size = %d\n", t->u.bs.size );
      printf( "t.type = bs\n");
    }
    break;

  case TypeKindSet:
    {
      printf( "t.clss = 'SET'\n" );
      printf( "set = ModBETypes.Set()\n" );
      printf( "set.base = %s\n", t->u.st.base->python_name );
      printf( "t.type = set\n");
    }
    break;

  case TypeKindSequence:
    {
      printf( "t.clss = '%s'\n",
	      Type_IsByteSequence( t ) ? "BYTESEQUENCE" : "SEQUENCE" );
      printf( "seq = ModBETypes.%s()\n",
	      Type_IsByteSequence( t ) ? "ByteSequence" : "Sequence" );
      printf( "seq.base = %s\n", t->u.sq.base->python_name );
      printf( "t.type = seq\n");
    }
    break;

  case TypeKindRecord:
    {
      FieldList_t fl;
      IdentifierList_t il;

      printf( "t.clss = 'RECORD'\n" );
      printf( "record = ModBETypes.Record()\n" );
      printf( "record.mems = []\n" );

      for ( fl = t->u.r.fields; fl ; fl = fl->next ) {
	for ( il = fl->f->il ; il ; il = il->next ) {
	  printf( "record_mem = ModBETypes.RecordMember()\n" );
	  printf( "record_mem.name = '%s'\n", il->i->name );
	  printf( "record_mem.type = %s\n", fl->f->type->python_name );
	  printf( "record.mems.append( record_mem )\n" );
	}
      }

      printf( "t.type = record\n");
    }
    break;

  case TypeKindChoice:
    {
      CandidateList_t cl;
      IdentifierList_t il;

      printf( "t.clss = 'CHOICE'\n" );
      printf( "choice = ModBETypes.Choice()\n" );
      printf( "choice.base = %s\n", t->u.c.base->python_name );
      printf( "choice.elems = []\n" );

      for ( cl = t->u.c.list; cl ; cl = cl->next ) {
	for ( il = cl->il ; il ; il = il->next ) {
	  printf( "choice_elem = ModBETypes.ChoiceElement()\n" );
	  printf( "choice_elem.name = '%s'\n", il->i->name );
	  printf( "choice_elem.type = %s\n", cl->type->python_name );
	  printf( "choice.elems.append( choice_elem )\n" );
	}
      }

      printf( "t.type = choice\n");
    }
    break;

  case TypeKindIref:
    {
      printf( "t.clss = 'IREF'\n" );
      printf( "iref = ModBETypes.InterfaceRef()\n" );
      printf( "iref.base = intf_%s\n", t->u.i.name->name );
      printf( "t.type = iref\n");
    }
    break;

  case TypeKindRef:
    {
      printf( "t.clss = 'REF'\n" );
      printf( "ref = ModBETypes.Ref()\n" );
      printf( "ref.base = %s\n", t->u.rf.base->python_name );
      printf( "t.type = ref\n");
    }
    break;

  case TypeKindAlias:
    {
      printf( "t.clss = 'ALIAS'\n" );
      printf( "alias = ModBETypes.Alias()\n" );
      printf( "alias.base = %s\n", t->u.al.base->python_name );
      printf( "t.type = alias\n");
    }
    break;

  default:
    Error_Bug ("Type_DumpPython: bogus TypeKind_t" );

  }

}
