#
# ModBEIface.py
#


import sys
import string
import time


import ModBETypes
import ModBEOps
import ModBEImports

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
		return self

	def preamble( self, f ) :
		f.write( '/*\n' )
		f.write( ' * Machine generated using: \n' )
		f.write( ' * - front:\t' + self.frontEnd + '\n')
		f.write( ' * - back:\t$Id: ModBEIface.py 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $\n')
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

	def typeHeader( self, f, fp ) :
		self.code = fp 
		StartCPPMacro( self.name, f )

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
		f.write( '#define ' + self.clType + 'p__wordconv (pointerval_t)\n\n')
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

		EndCPPMacro( self.name, f )
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
		    Tab + self.opType + Tab + '*op;\n')
		f.write ( '#ifndef __' + self.name + '_STATE\n' )
		f.write (  Tab + 'addr_t'    + Tab + ' st;\n' )
		f.write ( '#else \n' )
		f.write (  Tab + '__' + self.name + '_STATE' + Tab + ' *st;\n')
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
		    Tab + 'addr_t ' + Tab + ' op;\n' + \
		    Tab + 'addr_t ' + Tab + ' st;\n};\n\n' )

	# Return a list of all the operations of this interface, including
	# those from interfaces it EXTENDS
	def allOps( self ) :
	        if not self.allOpsCache :
		  for imp in self.imports :
		    if imp.mode == 'EXTENDS' :
		      for o in imp.intf.allOps() :
			self.allOpsCache.append( o )
		      break;
		  for o in self.ops :
		    self.allOpsCache.append( o )

		return self.allOpsCache

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
	# Type Representation Functions
	#
	###########################################################

	def representation( self, f ) :

	  self.preamble( f )

	  # Include all the needed interfaces
	  f.write('#include "nemesis_types.h"\n')
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

	  # Define operations
	  for i in self.ops :
	    i.representation( f )

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
	  f.write('  { { Interface_clp__code, (pointerval_t) &' +self.name +'__cl},\n')
	  #f.write('    TYPED_PTR(TCODE_NONE, NULL), \n')
	  f.write('    { Type_Code__code, ' + self.name + '_clp__code },\n' )
	  f.write('    "' + self.name + '",\n')
	  f.write('    BADPTR /* => &IREF__intf*/\n  },\n' )
	  # f.write('  /* next in hash */ ' + self.name + '__next,\n')
	  f.write('  /* Needs list   */ ' + self.name + '__needs,\n')
	  f.write('  /* No. of needs */ ' + `len(self.imports)` + ',\n' )
	  f.write('  /* Types list   */ ' + self.name + '__types,\n')
	  f.write('  /* No. of types */ ' + `self.numtypes` + ',\n' )
	  f.write('  /* Local flag   */ ' + `self.local` + ',\n')
	  f.write('  /* Supertype    */ ' + self.extends + ',\n' )
	  f.write('  /* Operations   */ ' + self.name + '__ops,\n')
	  f.write('  /* No. of ops   */ ' + `len(self.ops)` + '\n' )
	  f.write('};\n')

	  f.write( '\n' )
	  
	  return



	# ########
	# IDC CODE - rjb style
	# ########
	def idcMarshalHeader( self , f ) :
		StartCPPMacro( 'idc_m_' + self.name, f )
		self.preamble( f )

		f.write( '#include "' + self.name + '.h"\n' )
		f.write( '\n' )

		for i in self.imports :
		  i.idcMarshalHdr ( f )

		for i in self.types :
		  i.idcSig ( f )

		EndCPPMacro( 'idc_m_' + self.name, f )

	def idcMarshalCode( self, f ) :
		self.preamble( f )

		f.write( '#include <stdlib.h>\n' )
		f.write( '#include <malloc.h>\n' )
		f.write( '#include <idc/idc.h>\n\n' )
		
		f.write( '#include "idc_m_' + self.name + '.h"\n\n' )

		for i in self.types :
			i.idcFree( f )
			i.idcMarshal( f )
			i.idcUnmarshal( f )

		f.write( '\n/* End of File */\n' )

	def idcServerHeader( self, f ) :
		StartCPPMacro( 'idc_s_' + self.name, f )
		self.preamble( f )

		f.write( 'extern bool_t idc_s_create_' + self.name + '(\n' )
		f.write( Tab + 'void' + Tab2 + '**result,\n' )
		f.write( Tab + self.name + '_clp' + Tab2 + 'implementation,\n' )
		f.write( Tab + 'IDCControl' + Tab2 + '*ctrl,\n' )
		f.write( Tab + 'struct _heap' + Tab + '*heap);\n\n' )

		f.write( 'extern void idc_s_destroy_' + self.name + '(\n' )
		f.write( Tab + 'void' + Tab2 + '*self);\n\n' )

		f.write( 'extern void idc_s_dispatcher_' + self.name + '(\n' )
		f.write( Tab + 'void' + Tab2 + '*self);\n\n' )

		EndCPPMacro( 'idc_s_' + self.name, f )			

	def idcServerStubs( self, f ) :
		self.preamble( f )
		f.write( '#include <stdlib.h>\n' )
		f.write( '#include <malloc.h>\n' )
		f.write( '#include <assert.h>\n' )
		f.write( '#include <Try.h>\n' )
		f.write( '#include <idc/idc.h>\n\n' )
		f.write( '#include "idc_m_' + self.name + '.h"\n' )
		f.write( '#include "idc_s_' + self.name + '.h"\n\n' )

		f.write( 'typedef struct {\n' )
		f.write( Tab + self.name + '_clp' + Tab + 'implementation;\n' )
		f.write( Tab + 'IDCControl' + Tab + '*ctrl;\n' )
		f.write( '} idc_s_state_' + self.name + ';\n\n' )

		for i in self.ops :
			i.idcServerStub( self.name + '_clp', self.types, f )
			f.write('\n')

		self.idcServerOpsDispatcher( f )
		f.write( '\n' )
		self.idcServerBindingDestructor( f )
		f.write( '\n' )
		self.idcServerBindingConstructor( f )
		f.write( '\n/* End of File */\n' )

	def idcClientHeader( self, f ) :
		StartCPPMacro( 'idc_c_' + self.name, f )
		self.preamble( f )

		f.write( 'extern bool_t idc_c_create_' + self.name + '(\n' )
		f.write( Tab + self.name + '_clp' + Tab2 + '*result,\n' )
		f.write( Tab + 'IDCControl' + Tab2 + '*ctrl,\n' )
		f.write( Tab + 'struct _heap' + Tab + '*heap);\n\n' )

		f.write( 'extern void idc_c_destroy_' + self.name + '(\n' )
		f.write( Tab + self.name + '_clp' + Tab + 'self);\n' )

		f.write( '\n' )

		EndCPPMacro( 'idc_c_' + self.name, f )			

	def idcClientStubs( self, f ) :
		self.preamble( f )
		f.write( '#include <stdlib.h>\n' )
		f.write( '#include <malloc.h>\n' )
		f.write( '#include <assert.h>\n' )
		f.write( '#include <Try.h>\n' )
		f.write( '#include <idc/idc.h>\n\n' )
		f.write( '#include "idc_m_' + self.name + '.h"\n' )
		f.write( '#include "idc_c_' + self.name + '.h"\n\n' )

		f.write( 'static const ' + self.name + '_op idc_c_op_' + self.name + ';\n\n' )

		for i in self.ops :
			i.idcClientStub( self.name + '_clp', self.types, '&idc_c_op_' + self.name, f )
			f.write('\n')

		self.idcClientBindingDestructor( f )
		f.write( '\n' )
		self.idcClientBindingConstructor( f )
		f.write( '\n/* End of File */\n' )

	# ######
	# Called by idcServerStubs
	def idcServerOpsDispatcher( self, f ) :
		f.write( 'extern void idc_s_dispatcher_' + self.name + '(\n' )
		f.write( Tab + 'void' + Tab + '*vself)\n' )
		f.write( '{\n' )
		f.write( Tab + 'idc_s_state_' + self.name + Tab + '*self = vself;\n' )
		f.write( Tab + 'IDCControl' + Tab + '*ctrl = self->ctrl;\n' )
		f.write( Tab + self.name + '_clp' + Tab2 + 'impl = self->implementation;\n' )
		f.write( Tab + 'MIDDL_Cardinal' + Tab + '_procno;\n\n' )
		f.write( Tab + 'for(;;)\n' + Tab + '{\n' )
		f.write( Tab + '    TRY\n' )
		f.write( Tab2 + 'for(;;)\n' + Tab2 + '{\n' )
		f.write( Tab3 + 'idc_prepare_rx(ctrl);\n' )
		f.write( Tab3 + 'if (! idc_u_MIDDL_Cardinal( ctrl, &_procno ))\n' )
		f.write( Tab4 + 'return;\n\n' )
		f.write( Tab3 + 'switch (_procno) {\n' )
		for i in self.ops :
		    f.write( Tab3 + 'case ' + `i.number` + ':\n' )
		    f.write( Tab4 + 'idc_s_' + i.name + '(impl, ctrl);\n' )
		    f.write( Tab4 + 'break;\n\n' )

		f.write( Tab3 + 'default:\n' )
		f.write( Tab4 + 'RAISE(NULL, NULL);\n' )
		f.write( Tab3 + '}\n' )
		f.write( Tab2 + '}\n' )
		for i in self.allExns() :
		    i.idcServerCatcher( f )
		f.write( Tab + '    ENDTRY\n' )
		f.write( Tab + '}\n' )
		f.write( '}\n' )

	# Called by idcServerStubs
	def idcServerBindingConstructor( self, f ) :
		f.write( 'bool_t idc_s_create_' + self.name + '(\n' )
		f.write( Tab + 'void' + Tab2 + '**result,\n' )
		f.write( Tab + self.name + '_clp' + Tab2 + 'implementation,\n' )
		f.write( Tab + 'IDCControl' + Tab2 + '*ctrl,\n' )
		f.write( Tab + 'struct _heap' + Tab + '*heap)\n' )
		f.write( '{\n' )
		f.write( Tab + 'idc_s_state_' + self.name + ' *self;\n\n' )
		f.write( Tab + 'assert(heap == ctrl->idc_heap);\n' )
		f.write( Tab + 'self = heap->malloc(heap, sizeof(*self));\n' )
		f.write( Tab + 'if (!self)\n' + Tab2 + 'return FALSE;\n\n' )
		f.write( Tab + 'self->implementation = implementation;\n' )
		f.write( Tab + 'self->ctrl = ctrl;\n' )
		f.write( Tab + '*result = self;\n' )
		f.write( Tab + 'return TRUE;\n' )
		f.write( '}\n' )

	# Called by idcServerStubs
	def idcServerBindingDestructor( self, f ) :
		f.write( 'void idc_s_destroy_' + self.name + '(\n' )
		f.write( Tab + 'void' + Tab2 + '*vself)\n' )
		f.write( '{\n' )
		f.write( Tab + 'idc_s_state_' + self.name + Tab + '*self = vself;\n' )
		f.write( Tab + 'struct _heap' + Tab + '*heap = self->ctrl->idc_heap;\n\n' )
		f.write( Tab + 'heap->free(heap, self);\n' )
		f.write( '}\n' )

	# Called by idcClientStubs
	def idcClientBindingConstructor( self, f ) :
		f.write( 'static const ' + self.name + '_op idc_c_op_' + self.name + ' = {' )
		for i in self.ops :
		    f.write( '\n' + Tab + 'idc_c_' + i.name )
		    if i != self.ops[-1] : f.write( ',' )
		f.write( '\n};\n\n' )

		f.write( 'bool_t idc_c_create_' + self.name + '(\n' )
		f.write( Tab + self.name + '_clp' + Tab2 + '*result,\n' )
		f.write( Tab + 'IDCControl' + Tab2 + '*ctrl,\n' )
		f.write( Tab + 'struct _heap' + Tab + '*heap)\n' )
		f.write( '{\n' )
		f.write( Tab + self.name + '_clp' + Tab + 'self;\n\n' )
		f.write( Tab + 'assert(heap == ctrl->idc_heap);\n' )
		f.write( Tab + 'self = heap->malloc(heap, sizeof(*self));\n' )
		f.write( Tab + 'if (!self)\n' + Tab2 + 'return FALSE;\n\n' )
		f.write( Tab + 'self->st = ctrl;\n' )
		f.write( Tab + 'self->op = &idc_c_op_' + self.name + ';\n' )
		f.write( Tab + '*result = self;\n' )
		f.write( Tab + 'return TRUE;\n' )
		f.write( '}\n' )

	# Called by idcClientStubs
	def idcClientBindingDestructor( self, f ) :
		f.write( 'void idc_c_destroy_' + self.name + '(\n' )
		f.write( Tab + self.name + '_clp' + Tab + 'self)\n' )
		f.write( '{\n' )
		f.write( Tab + 'struct _heap' + Tab + '*heap = ((IDCControl *)(self->st))->idc_heap;\n\n' )
		f.write( Tab + 'heap->free(heap, self);\n' )
		f.write( '}\n' )




	# ########
	# IDC CODE - dme style
	# ########
	def IDCMarshalHeader( self , f ) :
		StartCPPMacro( 'IDC_M_' + self.name, f )
		self.preamble( f )

		f.write( '#include "' + self.name + '.ih"\n' )
		f.write( '#include "IDCMacros.h"\n' )
		f.write( '\n' )

		for i in self.imports :
		  i.IDCMarshalHdr ( f )
		f.write( '\n' )

		for i in self.types :
		  i.idcSig ( f )
		f.write( '\n' )

		for i in self.excs :
		  i.idcSig ( f )

		EndCPPMacro( 'IDC_M_' + self.name, f )

	def IDCMarshalCode( self, f ) :
		self.preamble( f )

		f.write( '#include "nemesis.h"\n' )
		f.write( '#include "Heap.ih"\n\n' )
		
		f.write( '#include "IDC_M_' + self.name + '.h"\n\n' )

		f.write( '#undef  SEQ_FAIL\n')
		f.write( '#define SEQ_FAIL() return False\n\n')

		for i in self.types :
			i.idcFree( f )
			i.idcMarshal( f )
			i.idcUnmarshal( f )

		for i in self.excs :
			i.idcMarshal( f )
			i.idcUnmarshal( f )

		f.write( '\n/* End of File */\n' )


	def IDCServerStubs( self, f ) :
	  name = self.name
	  self.preamble( f )

	  boilerplate = \
	      '#include <nemesis.h>\n' + \
	      '#include <typecodes.h>\n' + \
	      '#include <ShmTransport.h>\n' + \
	      '#include <IDCMacros.h>\n' + \
	      '#include <IDCStubs.ih>\n' + \
	      '#include "IDC_M_' + name + '.h"\n\n' + \
	      'static IDCServerStubs_Dispatch_fn	      Dispatch_m;\n'  + \
	      'static IDCServerStubs_op IDCServerStubs_ms = { Dispatch_m };\n' + \
	      'static const IDCServerStubs_cl server_cl = { &IDCServerStubs_ms, NULL };\n\n' + \
	      'extern const ' + name + '_cl client_cl;\n' + \
	      'static const IDCStubs_Rec _rec = {\n' + \
	      '  (IDCServerStubs_clp) &server_cl,\n' + \
	      '  { ' + name + '_clp__code, (pointerval_t) &client_cl}, \n' + \
	      '  BADPTR,\n' + \
	      '  FRAME_SIZE, FRAME_SIZE\n' + \
	      '};\n\n' + \
	      'PUBLIC const Type_AnyI ' + name + '__stubs = { IDCStubs_Info__code, (pointerval_t) &_rec }; \n\n' + \
	      '/*----------------------------------- ' + name + ' Server Stub Procs ---*/\n\n'
		    
	  f.write( boilerplate )
	  
	  for i in self.allOps():
	    f.write('extern void ' + i.ifName + '_' + i.name + '_S (' + i.ifName + \
		    '_clp _clp, IDCServerBinding_clp _bnd, IDC_clp _idc, IDC_BufferDesc _bd );\n')
	    
	  for i in self.ops :
	    i.IDCServerStub( name + '_clp', self.types, f )
	    f.write('\n')
	    
	  self.IDCServerOpsDispatcher( f )

	def IDCClientStubs( self, f ) :
	  name = self.name

	  f.write ( \
	      '\n/*----------------------------------------------------- Client Stubs ---*/\n\n')

	  for i in self.allOps() :
	    op = i.ifName + '_' + i.name
	    f.write ('extern ' + op + '_fn	' + op + '_C;\n')
	      
	  f.write ( \
	      '\n' + \
	      'static ' + name + '_op client_ms = {\n')

	  for i in self.allOps() :
	    f.write ('  ' + i.ifName + '_' + i.name + '_C,\n')

	  f.write ( \
	      '};\n' + \
	      '\n' + \
	      'static ' + name + '_cl client_cl = { &client_ms, NULL };\n\n' )

	  for i in self.ops :
	    i.IDCClientStub( self.name + '_clp', self.types, self.allExns(), f )
	    f.write('\n')

	  f.write( '\n/* End of File */\n' )

	# ######
	# Called by IDCServerStubs
	def IDCServerOpsDispatcher( self, f ) :
	  name = self.name
	  boilerplate = \
	      '/*----------------------------------------- IDCServerStubs Entry Points ---*/\n' + \
	      '\n' + \
	      'static void\n' + \
	      'Dispatch_m (IDCServerStubs_clp self)\n' + \
	      '{\n' + \
	      '  IDCServerStubs_State *_st  = self->st;\n' + \
	      '  IDC_clp	       _idc  = _st->marshal;\n' + \
	      '  IDCServerBinding_clp _bnd  = _st->binding;\n' + \
	      '  Binder_clp	       _clp  = _st->service;\n' + \
	      '  bool_t NOCLOBBER     _todo = True;\n' + \
	      '  IDC_BufferDesc       _bd;\n' + \
	      '  word_t               _opn;\n' + \
	      '  \n' + \
	      '  while (_todo)\n' + \
	      '  {\n' + \
	      '    TRY\n' + \
	      '    {\n' + \
	      '      while ((_todo = IDCServerBinding$ReceiveCall (_bnd, &_bd, &_opn, NULL))\n' + \
	      '	     == True)\n' + \
	      '      {\n' + \
	      '        switch (_opn)\n' + \
	      '        {\n'
	  f.write (boilerplate)
	     
	  for i in self.allOps() :
	    f.write( \
		'        case ' + `i.number` + ':\n'  + \
		'          ' + i.ifName + '_' + i.name + '_S ((' + i.ifName + '_clp) _clp,_bnd,_idc,_bd);\n'  + \
		'          break;\n' )

	  boilerplate = \
	      '        default:\n' + \
	      '          RAISE_IDC$Failure ();\n' + \
	      '        }\n' + \
	      '      }\n' + \
	      '    }\n'
	  f.write (boilerplate)

	  n = 1
	  for i in self.allExns() :
	    i.IDCServerCatcher( f, n )
	    n = n + 1

	  boilerplate = \
	      '    ENDTRY;\n' + \
	      '  }\n' + \
	      '}\n\n'
	  f.write (boilerplate)


	##################################################################
	# 
	# Emit template for C implementation of intf

	def template (self, f):
	  intfName = self.name
	  (year,_,_,_,_,_,_,_,_) = time.localtime(time.time())
	  boilerplate = \
	      '/*\n' + \
	      '*****************************************************************************\n' + \
	      '**                                                                          *\n' + \
	      '**  Copyright '+`year`+', University of Cambridge Computer Laboratory             *\n' + \
	      '**                                                                          *\n' + \
	      '**  All Rights Reserved.                                                    *\n' + \
	      '**                                                                          *\n' + \
	      '*****************************************************************************\n' + \
	      '**\n' + \
	      '** FILE:\n' + \
	      '**\n' + \
	      '**	\n' + \
	      '**\n' + \
	      '** FUNCTIONAL DESCRIPTION:\n' + \
	      '**\n' + \
	      '**	\n' + \
	      '**\n' + \
	      '** ENVIRONMENT: \n' + \
	      '**\n' + \
	      '**\n' + \
	      '**\n' + \
	      '** ID : $' + 'Id: $\n' + \
	      '**\n' + \
	      '**\n' + \
	      '*/\n' + \
	      '\n' + \
	      '#include <nemesis.h>\n' + \
	      '\n' + \
	      '#include <' + intfName + '.ih>\n' + \
	      '\n' + \
	      '#ifdef DEBUG\n' + \
	      '#define TRC(_x) _x\n' + \
	      '#else\n' + \
	      '#define TRC(_x)\n' + \
	      '#endif\n' + \
	      '\n' + \
	      '#define UNIMPLEMENTED printf("%s: UNIMPLEMENTED\n", __FUNCTION__);\n' + \
	      '\n\n'

	  f.write (boilerplate)

	  for i in self.allOps():
	    i.prototype (f, intfName + '_', '_m')
	  f.write ('\n')

	  f.write ('static' + Tab + intfName + '_op' + Tab + 'ms = {\n')
	  ops = self.allOps()
	  for i in ops:
	    # emit initialiser for method suite member
	    f.write ('  ' + intfName + '_' + i.name + '_m')
	    if i <> ops[-1]: f.write (',')
	    f.write ('\n')
	  f.write ('};\n\n')

	  f.write ('static' + Tab + intfName + '_cl' + Tab + \
	      'cl = { &ms, NULL };\n\n')

	  f.write ('CL_EXPORT (' + intfName + ', ' + intfName + \
	      ', cl);\n\n\n')

	  f.write ('/*---------------------------------------------------- Entry Points ----*/\n\n')

	  for i in self.allOps():
	    i.template (f, self, intfName, '_m')

	  f.write ('/*\n * End \n */\n')


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

 
