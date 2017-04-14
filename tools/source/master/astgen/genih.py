import BE
import sys

class Interface(BE.Interface):
	def bothHeader( self, f, fp ) :
		self.code = fp 
		BE.StartCPPMacro( self.name, f )

		self.preamble( f )

		if len (self.allOps()) != 0:
		  self.closureDef( f )
		else :
		  self.nullClosureDef( f )

		for i in self.imports :
		  i.typeHdr( f )
		f.write( '\n' )

		f.write( '#define ' + self.clType + 'p__code (0x' + \
                   fp + '0000ull)\n' )
		f.write( '#define ' + self.clType + 'p__wordconv (pointerval_t)\n')
		f.write( '#define ' + self.clType + 'p__anytype (' +self.clType +'p)\n\n');
		self.numtypes = 0
		for i in self.types :
		  if i.clss != 'IREF' :
		    self.numtypes = self.numtypes + 1
		    i.typecode( self.numtypes, f);
	        f.write('\n')
 
		for i in self.types :
		  i.typeDef( self.name, f )
		f.write( '\n' )

		# Operation & Exception typedefs 
		for i in self.ops :
			i.typedef( self.name, self.clType, f )
			f.write( '\n' )

		f.write( '#ifndef __NO_METHOD_MACROS__\n' )
		for i in self.ops :
			i.macro( self.name, self.clType, f )
		f.write( '#endif /* __NO_METHOD_MACROS__ */\n' )
		f.write( '\n' )

		f.write( '#ifndef __NO_EXCEPTION_MACROS__\n' )
		for i in self.excs :
			i.typedef( f )
		f.write( '#endif /* __NO_EXCEPTION_MACROS__ */\n' )
		f.write( '\n' )

		# Method suite struct type
		if len (self.allOps()) != 0: self.opStruct( f )

		f.write( '\n' )

		BE.EndCPPMacro( self.name, f )
		return
	# Define the closure type
	def closureDef (self, f):
		f.write( 'typedef ' )
		if self.isModule: f.write ( 'const ' )
		f.write ( 'struct _' + self.clType + ' ' + \
			  self.clType + ';\n' )
		f.write ( 'typedef ' + self.clType + ' *' + \
		          self.clType + 'p;\n')
		f.write ( 'typedef const struct _' + self.opType + ' ' + \
			  self.opType + ';\n\n' )
		f.write ( 'struct _' + self.clType + ' {\n' + \
		    BE.Tab + self.opType + BE.Tab + '*op;\n')
		f.write ( '#ifndef __' + self.name + '_STATE\n' )
		f.write (  BE.Tab + 'addr_t'    + BE.Tab + ' st;\n' )
		f.write ( '#else \n' )
		f.write (  BE.Tab + '__' + self.name + '_STATE' + BE.Tab + ' *st;\n')
		f.write ( '#endif \n' )
		f.write ( '};\n\n' )

		if self.isModule:
		  f.write ( 'extern ' + self.clType + ' * const ' + \
		      self.name + ';\n\n' )

	# Define a null closure type if there are no methods
	def nullClosureDef (self, f):
		f.write( 'typedef const ' )
		f.write ( 'struct _' + self.clType + ' ' + \
			  self.clType + ';\n' )
		f.write ( 'typedef ' + self.clType + ' *' + \
		          self.clType + 'p;\n')
		f.write ( 'struct _' + self.clType + ' {\n' + \
		    BE.Tab + 'addr_t ' + BE.Tab + ' op;\n' + \
		    BE.Tab + 'addr_t ' + BE.Tab + ' st;\n};\n\n' )

class Operation(BE.Operation):
    pass

class Type(BE.Type):
    pass

class Predefined(BE.Predefined): pass

class Alias(BE.Alias): pass

class Enumeration(BE.Enumeration): pass

class Record(BE.Record): pass

class RecordMember(BE.RecordMember): pass

class Choice(BE.Choice): pass

class ChoiceElement(BE.ChoiceElement): pass

class Array(BE.Array): pass

class BitSet(BE.BitSet): pass

class ByteSequence(BE.ByteSequence): pass

class ByteArray(BE.ByteArray): pass

class Set(BE.Set): pass

class Ref(BE.Ref): pass

class InterfaceRef(BE.InterfaceRef): pass

class Import(BE.Import): pass



def Go(kind, intflist, fp):
  intf = intflist[-1].obj
  #sys.stdout.write('('+intf.name+'.ih'+')')
  #sys.stdout.flush()
  t_ih  = open (intf.name + '.ih', 'w')
  intf.bothHeader (t_ih, fp)
  return [intf.name+'.ih']
