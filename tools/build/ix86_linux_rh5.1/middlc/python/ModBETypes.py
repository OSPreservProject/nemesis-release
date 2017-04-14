# CLUless
#
# ModBETypes.py; Python code for outputting C for MIDDL types, etc
#
# $Id: ModBETypes.py 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
#
# $Log: ModBETypes.py,v $
# Revision 2.14  1997/06/24 14:20:28  dr10009
# Warning fixes
#
# Revision 2.1  1997/04/04 15:39:46  rmm1002
# Mass check-in; now have vaguely working compiler, makes on P5 & Alpha
#
# Revision 1.3  1997/03/19 21:58:29  rmm1002
# Added missing import (for ModBEOps)
#
# Revision 1.2  1997/02/12 00:00:39  rmm1002
# Added Method stuff from old version
#
# Revision 1.1  1997/02/05 17:33:34  rmm1002
# Initial revision
#
#

#
# Imports
#
import sys
import string
import ModBEOps
#
# Useful constants for writing output.
#
Tab = '\t'
Tab2 = Tab + Tab
Tab3 = Tab2 + Tab
Tab4 = Tab3 + Tab

##########################################################################
#
# General type
#
##########################################################################
class Type:

	def init ( self, ifName, name, type ) :
		
		self.localName = name
		self.name = ifName + '$' + name
		self.type = type
		self.tc   = 'UNKNOWN';
		return self

	def predef ( self, tc, name, type ) :
		self.localName = name
		self.name = name
		self.type = type
		self.type.name = name
		self.tc   = hex(tc)[2:]
		self.clss = 'PREDEFINED'
		return self

	def typeDef( self, intf, f ) :
		self.type.typeDef( self.name, f )
		f.write('\n' )

	def isLarge( self ) :
		return self.type.isLarge()

	def isString( self ) :
	        return self.unAlias() == type_MIDDL_String

	def unAlias( self ) :
	        if self.clss == 'ALIAS' :
		  return self.type.base.unAlias()
		else :
		  return self

	def typecode( self, n, f ) :
	  f.write('#define ' + self.name + '__code (' )
	  f.write( `n` + ' + ' + self.intf.name + '_clp__code )\n' )
	  f.write('#define ' + self.name + '__wordconv ')
	  if self.type.ispointer() == 1:
	      f.write('(pointerval_t)')
	  f.write('\n')
	  
	def representation( self, f ) :
	  
	  f.write('/*\n * ' + self.name + '\n */\n' )
	  self.typeCodeName = self.name + '__code'
	  self.typeNameName = '"' + self.localName + '"'
	  self.typeRepName  = self.name + '__rep'
	  
	  # Give types a chance to define extra representation structures
	  self.type.repaux( self, f )

	  # Define its representation
	  f.write('static TypeRep_t ' + self.typeRepName + ' = {\n')  
	  self.type.representation( self, f )
	  #f.write('  TYPED_PTR ( TCODE_NONE, 0 ),\n' )
	  f.write('  { Type_Code__code, ' + self.name + '__code },\n' )
	  f.write('  ' + self.typeNameName + ',\n' )
	  f.write('  &' + self.intf.name + '__intf,\n' )
	  f.write('  sizeof(' + self.name + ')\n};\n\n' )

	def idcSig( self, f ) :
		self.type.idcSig( self.name, f )
		f.write('\n\n')

	def idcFree( self, f ) :
		if self.clss in ( 'RECORD', 'CHOICE', 'SEQUENCE', 'ARRAY' ) :
			self.type.idcFree( self.name, f )
			f.write('\n')
		
	def idcMarshal( self, f ) :
		if self.clss in ( 'RECORD', 'CHOICE', 'SEQUENCE', 'ARRAY' ) :
			self.type.idcMarshal( self.name, f )
			f.write('\n')

	def idcUnmarshal( self, f ) :
		if self.clss in ( 'RECORD', 'CHOICE', 'SEQUENCE', 'ARRAY' ) :
			self.type.idcUnmarshal( self.name, f )
			f.write('\n')


##########################################################################
#
# Predefined types
#
##########################################################################

class Predefined:

        def typeDef( self, name, f ) :
		f.write('typedef ' + self.name + ' ' + name + ';\n')
	        pass

	def isLarge( self ) :
	  	return 0

	def idcSig() :
		return
	def idcFree() :
		return
	def idcMarshal() :
		return
	def idcUnmarshal() :
		return
	def ispointer(self):
	    return 0

type_MIDDL_Octet          = Type().predef(1, 'uint8_t',   Predefined() )
type_MIDDL_ShortCardinal  = Type().predef(2, 'uint16_t',  Predefined() )
type_MIDDL_Cardinal       = Type().predef(3, 'uint32_t',  Predefined() )
type_MIDDL_LongCardinal   = Type().predef(4, 'uint64_t',  Predefined() )
type_MIDDL_Char           = Type().predef(5, 'int8_t',    Predefined() )
type_MIDDL_ShortInteger   = Type().predef(6, 'int16_t',   Predefined() )
type_MIDDL_Integer        = Type().predef(7, 'int32_t',   Predefined() )
type_MIDDL_LongInteger    = Type().predef(8, 'int64_t',   Predefined() )
type_MIDDL_Real           = Type().predef(9, 'float32_t', Predefined() )
type_MIDDL_LongReal       = Type().predef(10, 'float64_t', Predefined() )
type_MIDDL_Boolean        = Type().predef(11, 'bool_t',    Predefined() )
type_MIDDL_String         = Type().predef(12, 'string_t',  Predefined() )
type_MIDDL_Word           = Type().predef(13, 'word_t',    Predefined() )
type_MIDDL_Address        = Type().predef(14, 'addr_t',    Predefined() )
type_MIDDL_NetCardinal    = Type().predef(15, 'nint32_t',  Predefined() )


##########################################################################
#
# Alias type			(base)
#
##########################################################################
class Alias:

	def typeDef( self, name, f ) :
		f.write( 'typedef ' + self.base.name + ' ' + name + ';\n' )

	def isLarge( self ) :
		return self.base.isLarge()

	def repaux( self, t, f ) :
	  pass

	def representation( self, t, f ) :
#	  f.write('  TYPED_PTR( TypeSystem_Alias__code, ' )
#	  f.write( self.base.name + '__code ),\n' )
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Alias__code, ' )
	  f.write( self.base.name + '__code ),\n' )

	def idcSig( self, name, f ) :
		if self.isLarge() :
		  idcSigAliasLarge( name, self.base.name, self.base.name, f )
		else :
		  idcSigAliasSmall( name, self.base.name, self.base.name, f )

	def ispointer(self):
	    return self.base.type.ispointer()

##########################################################################
#
# Enumeration type		(elems)
#
##########################################################################
class Enumeration:

	def typeDef( self, name, f ) :
		f.write( 'typedef enum ' + name + ' {\n' )
		for i in self.elems[:-1] :
			f.write( Tab + name + '_' + i + ',\n' )
		f.write( Tab + name + '_' + self.elems[-1] + \
		    '\n} ' + name + ';\n' )

	def isLarge( self ) :
		return 0
	def ispointer(self) :
	    return 0

	def repaux( self, t, f ) :
	  
	  # First the state record for the context
	  f.write('static Field_t ' + t.name + '__elems[] = {\n')
	  j = 0
	  for i in self.elems :
	    f.write('  { { Enum_Value__code, ' + `j` + ' }, "' )
	    f.write(   i + '" },\n')
	  f.write('  { {0, 0 }, "" }\n};\n')

	  f.write('static EnumRecState_t ' + t.name + '__state = {\n')
	  f.write('  ' + `len(self.elems)` + ',\n')
	  f.write('  ' + t.name + '__elems\n};\n' )

	  # Declare the closure
	  f.write('static const Enum_cl ' + t.name + '__cl = {' )
	  f.write(' BADPTR /* => &enum_ops */, (void *) &' + t.name + '__state };\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Enum__code, (pointerval_t) &' + t.name + '__cl },\n' )

	def idcSig( self, name, f ) :
		idcSigAliasSmall( name, 'uint32_t', 'uint32_t', f )

##########################################################################
#
# Record type			(mems)
#
##########################################################################
class Record:
 
	def typeDef( self, name, f ) :
		f.write( 'typedef struct ' + name + ' ' + name + ';\n' )
		f.write( 'struct ' + name + ' {\n' )
		for i in self.mems :
			i.typeDef( f )
		f.write( '};\n' );

	def isLarge( self ) :
		return 1
	
	def ispointer( self ) :
	    return 1

	def repaux( self, t, f ) :
	  
	  # Write the fields context for the record.
	  f.write('static Field_t ' + t.name + '__fields[] = {\n')
	  for i in self.mems :
	    i.rep( f )
	  f.write('{ ANYI_INIT_FROM_64(0ull,0ull),  "" }\n};\n')
	  
	  # Write the state structure
	  f.write('static EnumRecState_t ' + t.name + '__state = {\n')
	  f.write('  ' + `len(self.mems)` + ',\n')
	  f.write('  ' + t.name + '__fields\n};\n' )

	  # Declare the closure
	  f.write('static const Record_cl ' + t.name + '__cl = {' )
	  f.write(' BADPTR /* => &record_ops */, (void *) &' + t.name + '__state };\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Record__code, (pointerval_t) &'+t.name+'__cl },\n' )


	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		for i in self.mems :
			i.idcFree( f )
		f.write( Tab + 'return;\n}\n\n' )

	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		for i in self.mems :
			i.idcMarshal( f )
		f.write( Tab + 'return( ' + f.true + ' );\n}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		for i in self.mems :
			i.idcUnmarshal( f )
		f.write( Tab + 'return( ' + f.true + ' );\n\n' )
		self.mems.reverse()
		firsttime = 0
		for i in self.mems :
			i.idcUnwind( f, firsttime )
			firsttime = 1
		f.write( Tab + 'return( ' + f.false + ' );\n}\n\n' )
		self.mems.reverse()

# Record member type
class RecordMember: 

	def typeDef( self, f ) :
		f.write( Tab + self.type.name + ' ' + self.name + ';\n' )

	def rep( self, f ) :
	    f.write('  { ANYI_INIT_FROM_64( Type_Code__code, ' )
	    f.write(  self.type.name + '__code ), "' + self.name + '" },\n')

	def idcFree( self, f ) :
		f.write( Tab + f.idc_f + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->' + self.name + ' );\n' )

	def idcMarshal( self, f ) :
		f.write( Tab + 'if ( !' + f.idc_m + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->' + self.name + ' ) )\n' )
		f.write( Tab2 + 'return ( ' + f.false + ' );\n' )

	def idcUnmarshal( self, f ) :
		f.write( Tab + 'if ( !' + f.idc_u + '_'+ self.type.name + '(_idc,_bd, ' )
		# Pointer always wanted even for small types
		f.write( '&' )
		f.write( 'obj->' + self.name + ' ) )\n' )
		f.write( Tab2 + 'goto unwind_' + self.name + ';\n' )
	
	def idcUnwind( self, f, firsttime ) :
	    if firsttime != 0:
		f.write( Tab + f.idc_f + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->' + self.name + ' );\n' )
	    f.write( '  unwind_' + self.name + ':\n' )
	
##########################################################################
#
# Choice type			(base elems)
#
##########################################################################
class Choice: 

	def typeDef( self, name, f ) :
		f.write( 'typedef struct ' + name + ' ' + name + ';\n' )
		f.write( 'struct ' + name + ' {\n' )
		f.write( Tab + self.base.name + ' tag;\n' )
		f.write( Tab + 'union {\n' )
		for i in self.elems :
			i.typeDef( f )
		f.write( Tab + '} u;\n};\n' );

	def isLarge( self ) :
		return 1

	def ispointer(self):
	    return 1

	def repaux( self, t, f ) :
	  # Write the field records for each choice 
	  for i in self.elems :
	    i.rec( self, t, f )
	  
	  # Write the fields for the context
	  f.write('static Field_t ' + t.name + '__fields[] = {\n')
	  for i in self.elems :
	    i.rep( t, f )
	  f.write(' { ANYI_INIT_FROM_64(0ull,0ull), "" }\n};\n' )

	  # Write the state record for the context
	  f.write('static ChoiceState_t ' + t.name + '__state = {\n')
	  f.write('  {' + `len(self.elems)` + ', ' + t.name + '__fields },\n' )
	  f.write('  ' + self.base.name + '__code\n};\n' )

	  # Declare the closure
	  f.write('static const Choice_cl ' + t.name + '__cl = {' )
	  f.write(' BADPTR /* => &choice_ops */, (void *) &' + t.name + '__state };\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Choice__code, (pointerval_t) &' )
	  f.write( t.name + '__cl },\n' )

	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

		### ASSUMPTION: Tags do not need freeing code
	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		f.write( Tab + 'switch ( obj->tag ) {\n' )
		for i in self.elems :
			i.idcFree( f , self.base.name )
		f.write( Tab + '}\n' )
		f.write( '}\n\n' )
		
	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		f.write( Tab + 'if ( !' + f.idc_m + '_' + self.base.name )
		f.write( '(_idc,_bd, obj->tag ) )\n' )
		f.write( Tab2 + 'return( ' + f.false + ' );\n\n' )
		f.write( Tab + 'switch ( obj->tag ) {\n' )
		for i in self.elems : 
			i.idcMarshal( f , self.base.name )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.true + ' );\n}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		f.write( Tab + 'if ( !' + f.idc_u + '_' + self.base.name )
		f.write( '(_idc,_bd, &obj->tag ) )\n' )
		f.write( Tab2 + 'return( ' + f.false + ' );\n\n' )
		f.write( Tab + 'switch ( obj->tag ) {\n' )
		for i in self.elems : 
			i.idcUnmarshal( f , self.base.name )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.true + ' );\n}\n\n' )

# Choice Element Type
class ChoiceElement: 

	def typeDef( self, f ) :
		f.write( Tab2 + self.type.name + ' ' + self.name + ';\n' )

	def rec( self, c, t, f ) :
	  f.write('static const Choice_Field ' )
	  f.write( t.name + '_' + self.name + '__crec = { ' )
	  f.write( `c.elems.index( self )` + ', ' )
	  f.write( self.type.name + '__code };\n' )

	def rep( self, t, f ) :
	    f.write('  {{ Choice_Field__code, (word_t)&' )
	    f.write( t.name + '_' + self.name + '__crec,  },"'+self.name+'"}, \n')
	    #f.write( self.name + '" },\n')

	def idcFree( self, f, name ) :
		f.write( Tab + 'case ' + name + '_' + self.name + ':\n' )
		f.write( Tab2 + f.idc_f + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->u.' + self.name + ' );\n' )
		f.write( Tab2 + 'break;\n' )

	def idcMarshal( self, f, name ) :
		f.write( Tab + 'case ' + name + '_' + self.name + ':\n' )
		f.write( Tab2 + 'if ( !' + f.idc_m + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->u.' + self.name + ' ) )\n' )
		f.write( Tab3 + 'return( ' + f.false + ' );\n' )
		f.write( Tab2 + 'break;\n' )

	def idcUnmarshal( self, f, name ) :
		f.write( Tab + 'case ' + name + '_' + self.name + ':\n' )
		f.write( Tab2 + 'if ( !' + f.idc_u + '_' + self.type.name + '(_idc,_bd, ' )
		f.write( '&' )
		f.write( 'obj->u.' + self.name + ' ) )\n' )
		f.write( Tab3 + 'return( ' + f.false + ' );\n' )
		f.write( Tab2 + 'break;\n' )

##########################################################################
#
# Sequence Type			(base)
#
##########################################################################
class Sequence:
	
	def typeDef( self, name, f ) :
		f.write( 'typedef struct ' + name + ' ' + name + ';\n' )
		f.write( 'struct ' + name + ' {\n' )
		f.write( Tab + 'uint32_t' + Tab + 'len;\n' )
		f.write( Tab + 'uint32_t' + Tab + 'blen;\n' )
		f.write( Tab + self.base.name + Tab + '*data;\n' )
		f.write( Tab + self.base.name + Tab + '*base;\n};\n' )

	def isLarge( self ) :
		return 1

	def ispointer( self ) :
	    return 1

	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Sequence__code, ' )
	  f.write( self.base.name + '__code ),\n' )

	# Assumption: length field does not need freeing code
	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		f.write( Tab + self.base.name + Tab + '*ptr;\n' )
		f.write( Tab + 'for (ptr=SEQ_START(obj); ptr<SEQ_END(obj); ptr++)\n' )
		f.write( Tab + '{\n' )
		f.write( Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr);\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'SEQ_FREE_DATA (obj);\n' )
		f.write( '}\n\n' )
		
	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		f.write( Tab + self.base.name + Tab + '*ptr;\n' )
		f.write( Tab + 'if (!' + f.idc_m + '_uint32_t(_idc,_bd, obj->len))\n' )
		f.write( Tab2 + 'return ' + f.false + ';\n\n' )
		f.write( Tab + 'for (ptr=SEQ_START(obj); ptr<SEQ_END(obj); ptr++)\n' )
		f.write( Tab + '{\n' )
		f.write( Tab2 + 'if (! ' + f.idc_m + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr))\n' )
		f.write( Tab3 + 'return ' + f.false + ';\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.true + ' );\n' )
		f.write( '}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		f.write( Tab + self.base.name + Tab + '*ptr;\n' )
		f.write( Tab + 'uint32_t ' + Tab + '_n;\n\n' )
		f.write( Tab + 'if ( !' + f.idc_u + '_uint32_t(_idc,_bd, &_n ) )\n' )
		f.write( Tab2 + 'return( ' + f.false + ' );\n\n' )
		f.write( Tab + 'SEQ_INIT (obj, _n, ' + f.heap('_bd') +');\n\n')
		f.write( Tab + 'for (ptr=SEQ_START(obj); ptr<SEQ_END(obj); ptr++)\n' )
		f.write( Tab + '{\n' )
		f.write( Tab2 + 'if (!' + f.idc_u + '_' + self.base.name + '(_idc,_bd, ptr))\n' )
		f.write( Tab2 + 'goto unwind;\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.true + ' );\n\n' )
		f.write( ' unwind:\n' )
		f.write( Tab + 'while (ptr > SEQ_START(obj)) {\n' )
		f.write( Tab2 + 'ptr--;\n' )
		f.write( Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ')
		if not self.base.isLarge() :
		    f.write( '*' );
		f.write( 'ptr);\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'SEQ_FREE_DATA (obj);\n' )
		f.write( Tab + 'return( ' + f.false + ' );\n' )
		f.write( '}\n\n' )

##########################################################################
#
# Array Type			(size base)
#
##########################################################################
class Array:

	def typeDef( self, name, f ) :
		f.write( 'typedef ' + self.base.name + ' ' + name )
		f.write( '[' + `self.size` + '];\n' )
		f.write( '#define ' + name + '_SIZE ' + `self.size` + '\n' )

	def isLarge( self ) :
		return 1

	def ispointer( self ):
	    return 1

	def repaux( self, t, f ) :
	  f.write('static const TypeSystem_Array ' + t.name + '__array = {\n' )
	  f.write('  ' + `self.size` + ',\n' )
	  f.write('  ' + self.base.name + '__code\n};\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Array__code, (pointerval_t)&'  )
	  f.write( t.name + '__array },\n' )

	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		f.write( Tab + self.base.name + Tab + '*ptr = &((*obj)[0]);\n' )
		f.write( Tab + 'uint32_t' + Tab + 'i;\n\n' )
		f.write( Tab + 'for (i=0; i<' + `self.size` + '; i++, ptr++)\n' )
		f.write( Tab + '{\n' )
		f.write( Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr);\n' )
		f.write( Tab + '}\n' )
		f.write( '}\n\n' )
		
	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		f.write( Tab + self.base.name + Tab + '*ptr = &((*obj)[0]);\n' )
		f.write( Tab + 'uint32_t ' + Tab + 'i;\n\n' )
		f.write( Tab + 'for (i=0; i<' + `self.size` + '; i++, ptr++)\n' )
		f.write( Tab + '{\n' )
		f.write( Tab2 + 'if (!' + f.idc_m + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr))\n' )
		f.write( Tab3 + 'return ' + f.false + ';\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.true + ' );\n}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		f.write( Tab + self.base.name + Tab + '*ptr = &((*obj)[0]);\n' )
		f.write( Tab + 'uint32_t ' + Tab + 'i;\n\n' )
		f.write( Tab + 'for (i=0; i<' + `self.size` + '; i++, ptr++)\n' )
		f.write( Tab + '{\n' )
		f.write( Tab2 + 'if (!' + f.idc_u + '_' + self.base.name + '(_idc,_bd, ' )
		# Pointer always wanted even for small types
		f.write( 'ptr))\n' )
		f.write( Tab2 + 'goto unwind;\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.true + ' );\n\n' )
		f.write( ' unwind:\n' )
		f.write( Tab + 'while (i--) {\n' )
		f.write( Tab2 + 'ptr--;\n' )
		f.write( Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr);\n' )
		f.write( Tab + '}\n' )
		f.write( Tab + 'return( ' + f.false + ' );\n}\n\n' )

##########################################################################
#
# Bit Set Type			(size)
#
##########################################################################
class BitSet:

	def typeDef( self, name, f ) :
		f.write( 'typedef bitset_t ' + name + ';\n' )
		f.write( '#define ' + name + '_SIZE ' + `self.size` + '\n' )

	def isLarge( self ) :
		return 0

	def ispointer( self ) :
	    return 0

	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_BitSet__code, ' )
	  f.write( `self.size` + ' ),\n' )

	def idcSig( self, name, f ) :
	  f.write( '/* marshall a bitset */\n' )
	  f.write( '#define ' + f.idc_m + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
	  f.write(  f.idc_m + '_bitset( (_bd), (bitset_t)(obj),' )
	  f.write(  `self.size` + ' )\n' )
	  f.write( '#define ' + f.idc_u + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
	  f.write(  f.idc_u + '_bitset( (_bd), (bitset_t)(obj),' )
	  f.write(  `self.size` + ' )\n' )
	  f.write( '#define ' + f.idc_f + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
	  f.write(  f.idc_f + '_bitset((_idc),(_bd), (bitset_t)(obj),' )
	  f.write(  `self.size` + ' )\n' )


##########################################################################
#
# ByteSequence Type		(base)
#
##########################################################################
class ByteSequence:
	
	def typeDef( self, name, f ) :
		f.write( 'typedef struct ' + name + ' ' + name + ';\n' )
		f.write( 'struct ' + name + ' {\n' )
		f.write( Tab + 'uint32_t' + Tab + 'len;\n' )
		f.write( Tab + 'uint32_t' + Tab + 'blen;\n' )
		f.write( Tab + self.base.name + Tab + '*data;\n' )
		f.write( Tab + self.base.name + Tab + '*base;\n};\n' )

	def isLarge( self ) :
		return 1

	def ispointer( self ):
	    return 1

	def repaux( self, t, f ) :
	  pass
 
	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Sequence__code, ' )
	  f.write( self.base.name + '__code ),\n' )

	def idcSig( self, name, f ) :
		f.write( '#define ' + f.idc_m + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
		f.write(  f.idc_m + '_counted_bytes( (_idc),  (_bd), ' )
		f.write( '((obj)->data), (((obj)->len) * sizeof(' + self.base.name + ')))\n' )

		f.write( '#define ' + f.idc_u + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
		f.write(  f.idc_u + '_counted_bytes( (_idc),  (_bd), ' )
		f.write( '(&((obj)->data)), (&((obj)->len)), sizeof(' + self.base.name + ') )\n' )

		f.write( '#define ' + f.idc_f + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
		f.write(  f.idc_f + '_counted_bytes( (_idc),  (_bd), ' )
		f.write( '((obj)->data), (((obj)->len) * sizeof(' + self.base.name + ')))\n' )

##########################################################################
#
# ByteArray Type		(size base)
#
##########################################################################
class ByteArray:

	def typeDef( self, name, f ) :
		f.write( 'typedef ' + self.base.name + ' ' + name )
		f.write( '[' + `self.size` + '];\n' )
		f.write( '#define ' + name + '_SIZE ' + `self.size` + '\n' )

	def isLarge( self ) :
		return 1

	def ispointer( self ):
	    return 1

	def repaux( self, t, f ) :
	  f.write('static const TypeSystem_Array ' + t.name + '__array = {\n' )
	  f.write('  ' + `self.size` + ',\n' )
	  f.write('  ' + self.base.name + '__code\n};\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Array__code, (pointerval_t)&' )
	  f.write( t.name + '__array },\n' )

	def idcSig( self, name, f ) :
		f.write( '#define ' + f.idc_m + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
		f.write(  f.idc_m + '_bytes( (_idc), (_bd), (uint8_t *)obj, ' )
		f.write( `self.size` + ' )\n' )
		
		f.write( '#define ' + f.idc_u + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
		f.write(  f.idc_u + '_bytes( (_idc),  (_bd), (uint8_t *)obj, ' )
		f.write( `self.size` + ' )\n' )
		
		f.write( '#define ' + f.idc_f + '_' + name + '(_idc,_bd, obj ) \\\n\t' )
		f.write(  f.idc_f + '_bytes( (_idc),  (_bd), (uint8_t *)obj, ' )
		f.write( `self.size` + ' )\n' )

##########################################################################
#
# Set Type			(base)
#
##########################################################################
class Set:
	
	def typeDef( self, name, f ) :
		f.write( 'typedef set_t ' + name + ';\n' )

	def isLarge( self ) :
		return 0

	def ispointer( self ):
	    return 1

	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Set__code, ' )
	  f.write( self.base.name + '__code ),\n' )

	def idcSig( self, name, f ) :
		return idcSigAliasSmall( name, 'set_t', 'set_t', f )

##########################################################################
#
# Ref Type			(base name)
#
##########################################################################
class Ref:

	def typeDef( self, name, f ) :
	  	f.write( 'typedef ' + self.base.name + ' *' + name + ';\n' )

	def isLarge( self ) :
		return 0

	def ispointer( self ):
	    return 1

	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Ref__code, ' )
	  f.write( self.base.name + '__code ),\n' )

	def idcSig( self, name, f ) :
		return idcSigAliasSmall( name, 'addr_t', 'addr_t', f )

##########################################################################
#
# InterfaceRef Type		(base name)
#
##########################################################################
class InterfaceRef:

	def typeDef( self, name, f ) :
	        pass
		# HACK

	def isLarge( self ) :
		return 0

	def ispointer( self ):
	    return 1

	def idcSig( self, name, f ) :
		return idcSigAliasSmall( name, 'addr_t', 'addr_t', f )

#
#
# Method Type    (args rets)
#
#
class Method:

        def typeDef( self, name, f ) :
                o = ModBEOps.Operation()
                o.pars = self.pars
                o.results = self.results
                o.returns = 1
                o.name = '(*' + name + ')' 
                f.write( 'typedef ')
                o.fnDecl('',self.clo,'',f,0)
                f.write(';\n')
                # HACK :-)

        def isLarge( self ):
                return 0

        def repaux(self,t,f):
          pass

        def representation(self,t,f):
          pass  

###########################################################################
# 
#  Auxiliary routines
#
###########################################################################

#
# Output an IDCsignature for an alias-like type
#
def idcSigAliasSmall( name, base, rtn, f ) :
	f.write( '#define ' + f.idc_m + '_' + name + '( _idc,_bd, obj ) \\\n\t' )
	f.write(  f.idc_m + '_' + rtn + '((_idc),(_bd), (' + base + ')(obj) )\n' ) 
	f.write( '#define ' + f.idc_u + '_' + name + '( _idc,_bd, obj ) \\\n\t' )
	f.write(  f.idc_u + '_' + rtn + '((_idc),(_bd), (' + base + ' *)(obj) )\n' )
	f.write( '#define ' + f.idc_f + '_' + name + '( _idc,_bd, obj ) \\\n\t' )
	f.write(  f.idc_f + '_' + rtn + '((_idc),(_bd), (' + base + ')(obj) )\n' )

def idcSigAliasLarge( name, base, rtn, f ) :
	f.write( '/* idcSigAliasLarge */\n' )
	f.write( '#define ' + f.idc_m + '_' + name + '( _idc,_bd, obj ) \\\n\t' )
	f.write(  f.idc_m + '_' + rtn + '((_idc),(_bd), (' + base + ' *)(obj) )\n' ) 
	f.write( '#define ' + f.idc_u + '_' + name + '( _idc,_bd, obj ) \\\n\t' )
	f.write(  f.idc_u + '_' + rtn + '((_idc),(_bd), (' + base + ' *)(obj) )\n' )
	f.write( '#define ' + f.idc_f + '_' + name + '( _idc,_bd, obj ) \\\n\t' )
	f.write(  f.idc_f + '_' + rtn + '((_idc),(_bd), (' + base + ' *)(obj) )\n' )

#
# Output an IDC signature for a miscellaneous type
#
def idcSigOtherSmall( name, f ) :
	f.write( 'extern bool_t ' + f.idc_m + '_' + name + '(' + f.cl + ' *, ' )
	f.write( name + ');\n')
	f.write( 'extern bool_t ' + f.idc_u + '_' + name + '(' + f.cl + ' *, ' )
	f.write( name + ' *);\n')
	f.write( 'extern void   ' + f.idc_f + '_' + name + '(' + f.cl + ' *, ' )
	f.write( name + ');\n')

def idcSigOtherLarge( name, f ) :
	f.write( '/* idcSigOtherLarge */\n' )
	f.write( 'extern bool_t ' + f.idc_m + '_' + name + '(' + f.cl + ' *, IDC_BufferDesc,  ' )
	f.write( name + ' *);\n')
	f.write( 'extern bool_t ' + f.idc_u + '_' + name + '(' + f.cl + ' *, IDC_BufferDesc,  ' )
	f.write( name + ' *);\n')
	f.write( 'extern void   ' + f.idc_f + '_' + name + '(' + f.cl + ' *, IDC_BufferDesc,  ' )
	f.write( name + ' *);\n')

#
# Output an IDC function beginning for a record or choice (un)marshaling code
#
def idcMarshalFuncbegin( name, dir, f ) :
	f.write( 'bool_t ' + f.idc + '_' + dir + '_' + name + '(' + f.cl + ' *_idc, IDC_BufferDesc _bd,  ' )
	f.write( name + ' *obj )\n' )
	f.write( '{\n' )
#
# Output an IDC function beginning for a record or choice freeing code
#
def idcFreeFuncbegin( name, f ) :
	f.write( 'void   ' + f.idc_f + '_' + name + '(' + f.cl + ' *_idc, IDC_BufferDesc _bd, ' )
	f.write( name + ' *obj )\n' )
	f.write( '{\n' )
