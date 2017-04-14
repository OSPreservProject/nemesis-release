#
# ModBEIface.py
#


import sys
import string
import time


Tab = '\t'
Tab2 = Tab + Tab
Tab3 = Tab2 + Tab
Tab4 = Tab3 + Tab
###########################################################
#
# Interface class
#
###########################################################
class Interface :

	def init( self ) :
		self.clType    = self.name + '_cl' # closure ptr
		self.opType    = self.name + '_op' # methods ptr
		self.msType    = self.name + '_ms' # method suite struct
		self.stType    = self.name + '_st' # state ptr
		self.fp        = 0;
		self.tc        = 'UNKNOWN';
		self.allOpsCache = []
		self.allExnsCache = []
		#n = []
		#for type in self.types:
		#    present = 0
		#    for t in n:
		#	if t == type:
		#	    present = 1
		#    if present == 0:
		#	n.append(type)
		#self.types = n
		return self

	def preamble( self, f ) :
		f.write( '/*\n' )
		f.write( ' * Machine generated using: \n' )
		f.write( ' * - front:\t' + self.frontEnd + '\n')
		f.write( ' * - back:\t$Id: BE.py 1.2 Thu, 20 May 1999 11:43:12 +0100 dr10009 $\n')
		f.write( ' * by ' + self.userName +' on '+ self.machine + '\n')
		f.write( ' * at ' + self.compTime + '\n' )
		f.write( ' */\n' )
		f.write( '\n' )

	def assignCodes( self, f, fp ) :

		# Really we need the typecodes of all types.  and NOW!
		for i in self.imports :
		  #f.write('IMPORT ' + i.intf.name + '\n')
		  i.intf.assignCodes( f, 'BAD' )

		self.fp = fp
		tc = string.atol(self.fp, 16) * 0x10000
		for i in self.types :
		  if i.clss != 'IREF' :
		    tc = tc + 1
		    i.tc = hex(tc)[2:-1]
		    #f.write(repr(i)) # XXX PRB
		    #f.write('  ' + i.name + '=' + i.tc + '\n')
		  else :
		    i.tc = self.tc;


	# Return a list of all the operations of this interface, including
	# those from interfaces it EXTENDS
	def allOps( self ) :
		l = []	        
	        for imp in self.imports :
		    if imp.mode == 'EXTENDS' :
		      for o in imp.intf.allOps() :
			l.append( o )

	        for o in self.ops :
		    l.append( o )
		return l

	# Return a list of all the exceptions of this interface, including
	# those from interfaces it EXTENDS
	def allExns( self ) :
	        if not self.allExnsCache :
		  for imp in self.imports :
		    if imp.mode == 'EXTENDS' :
		      for o in imp.intf.ops :
		        for e in o.raises :
			  if not e in self.allExnsCache :
			    self.allExnsCache.append( e )
		  for o in self.ops :
		    for e in o.raises :
		      if not e in self.allExnsCache :
			self.allExnsCache.append( e )

		return self.allExnsCache

	# Define the method suite struct type
	def opStruct( self, f ) :
		f.write( 'struct _' + self.opType + ' {\n' )

		for o in self.allOps() :
		    o.methodDecl ( f )
		f.write( '};\n\n' )

###########################################################
#
# Miscellaneous functions
#
###########################################################

def StartCPPMacro( name, f ) :
	CPPMacro = '_' + name + '_h_'
	f.write( '#ifndef ' + CPPMacro + '\n' )
	f.write( '#define ' + CPPMacro + '\n' )
	f.write( '\n' )
	
def EndCPPMacro( name, f ) :
	f.write( '\n' )
	f.write( '#endif /* _' + name + '_h_ */\n' )

 

##############################################################
#
# Operation class 
#
##############################################################
class Operation :

	def typedef( self, intf, cl, f ) :
	    # don't emit typedefs for ops inherited from an EXTENDS
	    if intf == self.ifName:
		f.write( 'typedef ')
		self.fnDecl (intf, cl, '_fn', f, 0)
		#                               ^^^
		# change to 1 when gcc can cope with attributes for
		# functions called via pointers.
		#
		f.write( ';\n' )
	    return

	def macro( self, intf, cl, f ) :
	    f.write( '#define ' + self.ifName + '$' + self.name + '(_self' )
	    for i in self.pars :
		f.write( ',' + '_' + i.name )
	    # the first result is returned directly
	    for i in self.results[1:] :
	        f.write( ',' + '_' + i.name )
	    f.write( ') (((_self)->op->' + self.name + ')((_self)' )
	    for i in self.pars :
		f.write( ',(' + '_' + i.name + ')' )
	    for i in self.results[1:] :
		f.write( ',(' + '_' + i.name + ')' )
	    f.write( ')) \n' )
	    return

	def fnDecl( self, intf, cl, suffix, f, typedef) :
	    # the first result is returned directly
	    noreturn = 0
	    if typedef and not self.returns:
	      noreturn = 1
	      f.write( 'NORETURN (' )
	    elif len (self.results) == 0 :
	      f.write( 'void ' )
	    else:
	      f.write( self.results[0].type.name  + ' ')
	      if self.results[0].type.isLarge() :
		f.write ('*')

	    f.write( intf + '_' + self.name + suffix + ' (\n' )
	    f.write( Tab + cl + Tab + '*self' )
	    for i in self.pars : 
	      i.typedef( f, ',', '' )
	    
	    if len(self.results) > 1 :
	      f.write( '\n   /* RETURNS */' )
	    for i in self.results[1:] : 
	      i.typedef( f, ',' )
	    f.write( ' )' )
	    if noreturn: f.write( ' )' )
	def methodDecl( self, f ) :
		f.write( Tab + self.ifName + '_' + self.name + '_fn' + \
		    Tab + '*' + self.name + ';\n' )

	def stateDecl( self, f ) :
	        for i in self.pars :
		  if i == self.pars[0]: sep = ''
		  else:                 sep = ';'
		  i.typedef (f, sep, '')
		f.write (';\n  /* Giles adds module state here */\n')

	def exportDecl( self, f ) :
	        for i in self.results :
		  # HACK: assumes _clp suffix on result type name
		  f.write ('extern const struct _' + i.type.name[:-4] + \
		           '_op ' + i.name + '_ms;\n')

	def prototype(self, f, prefix, suffix) :
		f.write( 'static' + Tab + self.ifName + '_' + self.name + \
		    '_fn ' + Tab2 + prefix + self.name + suffix + ';\n')
		
	def isbetype(self):
	    return 0
	
	def isbeop(self):
	    return 1
	
	def isbeexcn(self):
	    return 0

##############################################################
#
# Exception class 
#
##############################################################
class Exception :

	def typedef(self, f) :
		name = self.ifName + '$' + self.name
		name_args = name + '_Args'

		f.write ('#define ' + name + ' "' + name + '"\n')

		#  The argument record type -------------------------------

		if len (self.pars) != 0 :
		    f.write ('typedef struct ' + name_args + ' ' + name_args + ';\n')
		    f.write ('struct ' + name_args + ' {')
		    for i in self.pars :
		        if i == self.pars[0] : sep = ''
			else :                 sep = ';'
			i.typedef (f, sep, '')
		    f.write (';\n};\n\n')

		#  The RAISE macro  ----------------------------------------

		f.write ('#define RAISE_' + name + '(')
		self.arglist (f)

		f.write( ') \\\n' + '  { \\\n' )
		if len (self.pars) == 0 :
		    f.write( '    RAISE (' + name + ', NULL); \\\n' )
		else :
		    f.write( '    ' + name_args + ' *__rec = exn_rec_alloc(sizeof(' + name_args + ')); \\\n' )
		    for i in self.pars :
		        f.write( '    __rec->' + i.name + Tab + ' = (_' + i.name + '); \\\n') 
		    f.write( '    RAISE (' + name + ', __rec); \\\n' )
		f.write('  }\n\n' )

		#  The CATCH macro  ----------------------------------------

		f.write ('#define CATCH_' + name + '(')
		self.arglist (f)

		f.write ( ') \\\n  CATCHTOP (' + name + ') \\\n')

		if len (self.pars) != 0 :
		    f.write ('    '+ name_args + ' *__rec = exn_ctx.exn_args')
		    for i in self.pars :
		        i.typedef (f, '; \\', '_')
		        f.write (Tab + ' = __rec->' + i.name)
		    
		    f.write ('; \\\n')

		f.write ('  CATCHBOT\n\n')


	def arglist (self, f) :
		for i in self.pars :
	            f.write ('_' + i.name)
		    if i != self.pars[-1] : f.write (', ')

	def isbetype(self):
	    return 0

	def isbeop(self):
	    return 0

	def isbeexcn(self):
	    return 1

##############################################################
#
# Parameter field
#
##############################################################
class Parameter:

	def typedef( self, f, sep, prefix ) :
		f.write (sep + '\n' + Tab)
		#f.write ('/* '+self.typename + ' */ ')
		#print 'Argument size '+`self.isLarge`
	        if self.isLarge:
		  if self.mode == 'IN'   : f.write ('const ')
		  f.write (self.type.name + Tab + '*')
		else:
		  f.write (self.type.name + Tab)
		  if self.mode != 'IN'   : f.write ('*')
		f.write (prefix + self.name + Tab + \
		    '/* ' + self.mode + ' */')
		return

	def rep( self, f ) :
	  f.write('  { { ' + self.type.name + '__code, ' )
	  if self.mode == 'IN'   : f.write ('Operation_ParamMode_In')
	  if self.mode == 'OUT'  : f.write ('Operation_ParamMode_Out')
	  if self.mode == 'INOUT': f.write ('Operation_ParamMode_InOut')
	  f.write(' }, "' + self.name + '" },\n' )

##############################################################
#
# Results field
#
##############################################################
class Result:

	def typedef( self, f, sep ) :
		f.write (sep + '\n' + Tab + self.type.name + Tab)
	        if self.isLarge: f.write ('*')
		f.write ('*' + self.name)
		return

	def rep( self, f ) :
	  f.write('  { { ' + self.type.name + '__code, ' )
          f.write('Operation_ParamMode_Result }, "' + self.name + '" },\n' )

###########################################################################
# IDC Functions
###########################################################################



	    
class Type:

        def __init__(self):
	    self.users = []

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
	    type = self.unAlias()
	    if self.clss != 'PREDEFINED':
		return 0
	    if self.type == 'string_t':
		return 1
	    else:
		return 0

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
	  f.write('#define ' + self.name +'__anytype (' + self.name)
	  if self.type.isLarge() == 1:
	      f.write(' *')
	  f.write(')\n')
	  
	  

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

	def isbetype(self):
	    return 1

	def isbeexcn(self):
	    return 0
	
	def isbeop(self):
	    return 0


#	def __repr__(self):
#	    return 'Type '+self.clss


##########################################################################
#
# Predefined types
#
##########################################################################

class Predefined:

        def __init__(self):
	        self.known = 1

        def typeDef( self, name, f ) :
	        f.write('/* '+self.typename+' */ ')

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


	def ispointer(self):
	    return self.base.type.ispointer()


##########################################################################
#
# Enumeration type		(elems)
#
##########################################################################
class Enumeration:

	def typeDef( self, name, f ) :
		f.write( 'enum {\n')
		#' + name + ' {\n' )
		for i in self.elems[:-1] :
			f.write( Tab + name + '_' + i + ',\n' )
		f.write( Tab + name + '_' + self.elems[-1] + \
		    '\n};\n' )
		f.write('typedef enum_t '+name+';\n')
	def isLarge( self ) :
		return 0
	def ispointer(self) :
	    return 0



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



# Record member type
class RecordMember: 

	def typeDef( self, f ) :
		f.write( Tab + self.type.name + ' ' + self.name + ';\n' )

	
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


# Choice Element Type
class ChoiceElement: 

	def typeDef( self, f ) :
		f.write( Tab2 + self.type.name + ' ' + self.name + ';\n' )


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
	    return 0


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


#
#
# Method Type    (args rets)
#
#
#class Method:
#
#        def typeDef( self, name, f ) :
#                o = Operation()
#                o.pars = self.pars
#                o.results = self.results
#                o.returns = 1
#                o.name = '(*' + name + ')' 
#                f.write( 'typedef ')
#                o.fnDecl('',self.clo,'',f,0)
#                f.write(';\n')
#                # HACK :-)
#
#        def isLarge( self ):
#                return 0
#
#        def repaux(self,t,f):
#          pass
#
#        def representation(self,t,f):
#          pass  
#
#        def ispointer(self):
#	    return 1



class Import:

  def typeHdr( self, f ) :
    f.write( '#include "' + self.name + '.ih"\n' )
    
  def idcMarshalHdr( self, f ) :
    f.write( '#include "idc_m_' + self.name + '.h"\n' )
      
  def IDCMarshalHdr( self, f ) :
    f.write( '#include "IDC_M_' + self.name + '.h"\n' )

  def repHdr( self, f ) :
    f.write( '#include "' + self.name + '.def"\n' )


def blank(f):
    # Some files must exist because the Makefile cannot know whether they
    # really will exist or not and must always attempt to compile them
    f.write( '/* This file intentionally left blank */\n' )

    
