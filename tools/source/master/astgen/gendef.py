import BE
import sys

class Interface(BE.Interface):
	def representation( self, f ) :

	  self.preamble( f )

	  # Include all the needed interfaces
	  f.write('#include "nemesis.h"\n')
	  f.write('#include "TypeSystem_st.h"\n')

	  for i in self.imports :
	    f.write( '#include <' + i.intf.name + '.ih>\n' )

	  # Get our typecodes 
	  f.write('#include "' + self.name + '.ih"\n\n' )

	  # forward reference to our interface closure
	  f.write('extern Intf_st ' + self.name + '__intf;\n' )

	  #for i in self.imports :
	  #  f.write('extern Intf_st ' + i.name + '__intf;\n' )
	  #f.write( '\n' )

	  # Define all types
	  for i in self.types :
	    if i.clss != 'IREF' :
	      i.representation( f );

	  # List Types
	  self.numtypes = 0
	  f.write( 'static TypeRep_t *const ' + self.name + '__types[] = {\n')
	  for i in self.types :
	    if i.clss != 'IREF' :
	      f.write( '  &' + i.name + '__rep,\n' )
	      self.numtypes = self.numtypes + 1
	  f.write( '  (TypeRep_t *)0\n};\n\n' )

	  # List exceptions
	  for e in self.excs:
	      e.representation( f )

	  f.write( 'static Exc_t *const ' + self.name + '__exns[] = {\n')
	  for e in self.excs:
	      f.write( '  &' + e.fullName + '__rep,\n')
	  f.write( '};\n')    

	  # Define operations
	  count = 0
	  for i in self.allOps() :
	      if i in self.ops:
		  i.representation( f, count )
	      count = count + 1;
	  
	  # List Operations
	  f.write( 'static Operation_t *const ' + self.name + '__ops[] = {\n')
	  for i in self.ops :
	    f.write( '  &' + self.name + '$' + i.name + '__op,\n' )
	  f.write( '  (Operation_t *)0\n};\n\n' )

	  # List imported interface & find a supertype, if any
	  self.extends = '(Type_Code) 0'
	  f.write( 'static Type_Code const ' + self.name + '__needs[] = {\n')
	  for i in self.imports :
	    #tmp = '&' + i.intf.name + '__intf'
	    tmp = i.intf.name + '_clp__code'
	    f.write( '  ' + tmp + ',\n' )
	    if i.mode == 'EXTENDS' :
	      self.extends = tmp
	  f.write( '  TCODE_NONE\n};\n\n' )
	  
	  # Interface closure
	  f.write('static const Interface_cl ' + self.name + '__cl = {' )
	  f.write(' BADPTR /* => &interface_ops */, (void *) &' + self.name + '__intf };\n' )

          # Interface structure
	  f.write('Intf_st ' + self.name + '__intf ={\n')
	  f.write('  { { TypeSystem_Iref__code, (pointerval_t)NULL},\n') #&' +self.name +'__cl},\n')
	  #f.write('    TYPED_PTR(TCODE_NONE, NULL), \n')
	  f.write('    { Type_Code__code, ' + self.name + '_clp__code },\n' )
	  f.write('    "' + self.name + '",\n')
	  f.write('    BADPTR /* => &IREF__intf*/ ,\n' )
	  f.write('    sizeof(' + self.name + '_clp) },\n')
	  # f.write('  /* next in hash */ ' + self.name + '__next,\n')
	  f.write('  /* Needs list   */ ' + self.name + '__needs,\n')
	  f.write('  /* No. of needs */ ' + `len(self.imports)` + ',\n' )
	  f.write('  /* Types list   */ ' + self.name + '__types,\n')
	  f.write('  /* No. of types */ ' + `self.numtypes` + ',\n' )
	  f.write('  /* Local flag   */ ' + `self.local` + ',\n')
	  f.write('  /* Supertype    */ ' + self.extends + ',\n' )
	  f.write('  /* Operations   */ ' + self.name + '__ops,\n')
	  f.write('  /* No. of ops   */ ' + `len(self.ops)` + ',\n' )
	  f.write('  /* Exceptions   */ ' + self.name + '__exns,\n' )
	  f.write('  /* No. of exns  */ ' + `len(self.excs)` + ',\n' )
	  f.write('};\n')

	  f.write( '\n' )
	  
	  return

class Operation(BE.Operation):
        def representation( self, f, num ):
	  	  
	  self.fullName = self.ifName + '$' + self.name
	  f.write('/*\n * ' + self.name + '\n */\n' )

	  # First do all the arguments
	  f.write('static Param_t ')
	  f.write( self.fullName + '__params[] = {\n' )
	  for i in self.pars :
	    i.rep( f )
	  for i in self.results :
	    i.rep( f )
	  f.write('  { { 0,0 }, "" }\n};\n')

	  # Do all the exceptions
	  f.write('static ExcRef_t ' + self.fullName + '__exns[] = {\n' )
	  for e in self.raises:
	      f.write( '  {' + e.ifName + '_clp__code, ' + `e.number` + ' },\n' ) 
	  f.write( '};\n')

	  # Define the operation representation itself
	  #f.write('extern Operation_t ' + self.fullName + '__op;\n' )
	  f.write('static Operation_t ' + self.fullName + '__op;\n' )
	  f.write('static const Operation_cl ' + self.fullName + '__opcl = {\n' )
	  f.write('  BADPTR /* => &operation_ops */,\n' )
	  f.write('  (void *) &' + self.fullName + '__op\n};\n' )
	  f.write('static Operation_t ' + self.fullName + '__op = {\n' )
	  f.write( '  "' + self.name + '",\n')
	  f.write( '  Operation_Kind_')
	  if self.type == 'PROC' : 
	    if self.returns == 1 : f.write('Proc,\n')
	    else :                 f.write('NoReturn,\n')
	  else :                   f.write('Announcement,\n')
	  f.write( '  ' + self.fullName + '__params,\n')
	  f.write( '  ' + `len(self.pars)` + ', /* num args */\n' )
	  f.write( '  ' + `len(self.results)` + ', /* num results */\n')
	  f.write( '  ' + `num` + ', /* operation number */\n')
	  f.write( '  ' + self.fullName + '__exns,\n')
	  f.write( '  ' + `len(self.raises)` + ', /* num exceptions */\n')
	  f.write( '  (void *) &' + self.fullName + '__opcl\n};\n\n')


class Type(BE.Type):
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

class Exception(BE.Exception):
    def representation( self, f):

	self.fullName = self.ifName + '$' + self.name
	f.write('\n/*\n * ' + self.fullName +'\n */\n' )

	f.write('static Field_t ' + self.fullName +'__params[] = {\n')
	for i in self.pars:
	    f.write('  { ANYI_INIT_FROM_64( Type_Code__code, ' )
	    f.write(  i.type.name + '__code ), "' + i.name + '" },\n')
	    
	f.write('};\n')    

	f.write('static Exc_t ')
	f.write( self.fullName + '__rep = {\n' )
	f.write('  { ' + `len(self.pars)` + ', ' )
	f.write(self.fullName + '__params },\n');
	f.write('  { BADPTR /* => &exception_ops */,')
	f.write('  (void *) &' + self.fullName + '__rep },\n')
	f.write('  &' + self.intf.name + '__intf,\n' )
	f.write('  "' + self.name +'"\n};\n' )


    
class Alias(BE.Alias):
	def representation( self, t, f ) :
#	  f.write('  TYPED_PTR( TypeSystem_Alias__code, ' )
#	  f.write( self.base.name + '__code ),\n' )
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Alias__code, ' )
	  f.write( self.base.name + '__code ),\n' )
	def repaux( self, t, f ) :
	  pass

class Enumeration(BE.Enumeration):
	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Enum__code, (pointerval_t)NULL},\n') # &' + t.name + '__cl },\n' )
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


class Record(BE.Record):
	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Record__code, (pointerval_t)NULL},\n') #&'+t.name+'__cl },\n' )
	def repaux( self, t, f ) :
	  
	  # Write the individual Record_Fields
	  for i in self.mems:
	      i.rep(f, t)

	  # Write the fields context for the record.
	  f.write('static Field_t ' + t.name + '__fields[] = {\n')
	  for i in self.mems :
	    i.repaux( f, t )
	  f.write('{ ANYI_INIT_FROM_64(0ull,0ull),  "" }\n};\n')
	  
	  # Write the state structure
	  f.write('static EnumRecState_t ' + t.name + '__state = {\n')
	  f.write('  ' + `len(self.mems)` + ',\n')
	  f.write('  ' + t.name + '__fields\n};\n' )

	  # Declare the closure
	  f.write('static const Record_cl ' + t.name + '__cl = {' )
	  f.write(' BADPTR /* => &record_ops */, (void *) &' + t.name + '__state };\n' )

class RecordMember(BE.RecordMember):
    def rep (self, f, parent) :
	f.write('static const Record_Field ' + parent.name + '_' + self.name +'__field = {\n')
	f.write('    ' + self.type.name + '__code,\n')
	f.write('    OFFSETOF(' + parent.name + ', ' + self.name + ')\n')
	f.write('};\n')
	

    def repaux( self, f, parent ) :
	f.write('  { { Record_Field__code, ' )
	f.write('(pointerval_t) NULL }, "' + self.name + '" },\n') #&' + parent.name + '_' + self.name + '__field }, "' + self.name + '" },\n')

class Choice(BE.Choice):
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
	  f.write('  { TypeSystem_Choice__code, (pointerval_t)NULL},\n') #&' )
	  #f.write( t.name + '__cl },\n' )

class ChoiceElement(BE.ChoiceElement):
	def rec( self, c, t, f ) :
	  f.write('static Choice_Field ' )
	  f.write( t.name + '_' + self.name + '__crec = { ' )
	  f.write( `c.elems.index( self )` + ', ' )
	  f.write( self.type.name + '__code };\n' )

	def rep( self, t, f ) :
	    f.write('  {{ Choice_Field__code, (word_t)NULL, }, "' ) #&' )
	    #f.write( t.name + '_' + self.name + '__crec,  }, "' )
	    f.write( self.name + '" },\n')

    
class Sequence(BE.Sequence):
	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Sequence__code, ' )
	  f.write( self.base.name + '__code ),\n' )


class Array(BE.Array):
    	def repaux( self, t, f ) :
	  f.write('static const TypeSystem_Array ' + t.name + '__array = {\n' )
	  f.write('  ' + `self.size` + ',\n' )
	  f.write('  ' + self.base.name + '__code\n};\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Array__code, (pointerval_t)NULL},\n')#&'  )
	  #f.write( t.name + '__array },\n' )


class BitSet(BE.BitSet):
	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_BitSet__code, ' )
	  f.write( `self.size` + ' ),\n' )

class ByteSequence(BE.ByteSequence):
	def repaux( self, t, f ) :
	  pass
 
	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Sequence__code, ' )
	  f.write( self.base.name + '__code ),\n' )

class ByteArray(BE.ByteArray):
	def repaux( self, t, f ) :
	  f.write('static const TypeSystem_Array ' + t.name + '__array = {\n' )
	  f.write('  ' + `self.size` + ',\n' )
	  f.write('  ' + self.base.name + '__code\n};\n' )

	def representation( self, t, f ) :
	  f.write('  { TypeSystem_Array__code, (pointerval_t)NULL},\n') #&' )
	  #f.write( t.name + '__array },\n' )

class Set(BE.Set):
	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Set__code, ' )
	  f.write( self.base.name + '__code ),\n' )

class Ref(BE.Ref):
	def repaux( self, t, f ):
	  pass

	def representation( self, t, f ) :
	  f.write('  ANYI_INIT_FROM_64( TypeSystem_Ref__code, ' )
	  f.write( self.base.name + '__code ),\n' )



def Go(kind, intflist, fp):
  intf = intflist[-1].obj
  #sys.stdout.write('('+intf.name+'.def.c'+')')
  #sys.stdout.flush()
  t_d  = open (intf.name + '.def.c', 'w')
  intf.representation (t_d)
  return [intf.name + '.def.c']

