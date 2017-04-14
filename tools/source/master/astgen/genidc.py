import BE
import sys


def stubscmap(name):
    return 'IDC_S_'+name+'.c'

def marshallcmap(name):
    return 'IDC_M_'+name+'.c'

def marshallhmap(name):
    return 'IDC_M_'+name+'.h'


#
# Output the start of an operation
#
def IDCClientDef( name, bindType, pars, results, f ) :
	f.write( 'PUBLIC ' )
	if len (results) != 0 :
	    f.write( results[0].type.name )
	    if results[0].type.isLarge() :
	        f.write('*')
	else :
	    f.write( 'void' )
	f.write( '\n' + name + '_C (' + bindType + ' self' )
	for i in pars:
	    i.typedef (f, ',', '')

	for i in results[1:] :
	    i.typedef (f, ',')

	f.write( ')\n{\n' )
	if len (results) != 0:
	    f.write( BE.Tab +results[0].type.name + BE.Tab )
	    if results[0].type.isLarge() :
	        f.write('*')
	    f.write(' NOCLOBBER ')
	    f.write( results[0].name)
	    if results[0].type.isLarge() :
		f.write('=0')
	    f.write(';\n' )
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

class Interface(BE.Interface):
	# ########
	# IDC CODE - dme style
	# ########
	def IDCMarshalHeader( self , f ) :
		BE.StartCPPMacro( 'IDC_M_' + self.name, f )
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

		BE.EndCPPMacro( 'IDC_M_' + self.name, f )

	def IDCMarshalCode( self, f ) :
		self.preamble( f )

		f.write( '#include "nemesis.h"\n' )
		f.write( '#include "Heap.ih"\n\n' )
		
		f.write( '#include "' + marshallhmap(self.name)+ '"\n\n' )

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
	      '#include "' +marshallhmap(self.name)+ '"\n\n' + \
	      'static IDCServerStubs_Dispatch_fn	      Dispatch_m;\n'  + \
	      'static IDCServerStubs_op IDCServerStubs_ms = { Dispatch_m };\n' + \
	      'static const IDCServerStubs_cl server_cl = { &IDCServerStubs_ms, NULL };\n\n' + \
	      'extern const ' + name + '_cl client_cl;\n' + \
	      'static const IDCStubs_Rec _rec = {\n' + \
	      '  (IDCServerStubs_clp) &server_cl,\n' + \
	      '  (IDCClientStubs_clp) NULL,\n' + \
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
	  
	  count = 0
	  for i in self.allOps() :
	    f.write( \
		'        case ' + `count` + ':\n'  + \
		'          ' + i.ifName + '_' + i.name + '_S ((' + i.ifName + '_clp) _clp,_bnd,_idc,_bd);\n'  + \
		'          break;\n' )
	    count = count + 1

	  boilerplate = \
	      '        default:\n' + \
	      '          RAISE_IDC$Failure ();\n' + \
	      '        }\n' + \
	      '      }\n' + \
	      '    }\n'
	  f.write (boilerplate)

	  n = 1
	  for e in self.allExns() :
	    e.IDCServerCatcher( f, n )
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

	  f.write ('static' + BE.Tab + intfName + '_op' + BE.Tab + 'ms = {\n')
	  ops = self.allOps()
	  for i in ops:
	    # emit initialiser for method suite member
	    f.write ('  ' + intfName + '_' + i.name + '_m')
	    if i <> ops[-1]: f.write (',')
	    f.write ('\n')
	  f.write ('};\n\n')

	  f.write ('static' + BE.Tab + intfName + '_cl' + BE.Tab + \
	      'cl = { &ms, NULL };\n\n')

	  f.write ('CL_EXPORT (' + intfName + ', ' + intfName + \
	      ', cl);\n\n\n')

	  f.write ('/*---------------------------------------------------- Entry Points ----*/\n\n')

	  for i in self.allOps():
	    i.template (f, self, intfName, '_m')

	  f.write ('/*\n * End \n */\n')


class Operation(BE.Operation):
	# ########
	# IDC Code - dme style
	# ########

	def IDCServerStub( self, bindType, typedict, f ) :
		f.write( \
		    'PUBLIC void\n' + self.ifName + '_' + self.name + '_S (' + bindType + \
		    ' _clp, IDCServerBinding_clp _bnd, IDC_clp _idc, IDC_BufferDesc NOCLOBBER _bd)\n' + \
		    '{\n')
# _heap isn't used
#		    '  Heap_clp' + BE.Tab + '_heap = _bd->heap;\n' 

		if len (self.pars) != 0 :
		    f.write( '  /* Arguments */\n' )
		    ServerLocals( self.pars, f )
		if len (self.results) != 0 :
		    f.write( '  /* Results */\n' )
		    ServerResults( self.results, f )
		f.write( '\n' )

		idcUnmarshalArgs( self.pars, f )

		if self.type == 'ANNOUNCEMENT' :
			# Do Announcements by Operation with 0 results
			self.IDCOpServerStub( typedict, f )
		else :
			self.IDCOpServerStub( typedict, f )

		if len (self.pars) != 0 :
		    idcUnwindUnmarshalledArgs( self.pars, f )
		    f.write( '  RAISE_IDC$Failure ();\n' )
		f.write( '}\n' )

	def IDCOpServerStub( self, typedict, f ) :
		f.write( '  IDCServerBinding$AckReceive(_bnd, _bd);\n' )
		f.write( '  TRY\n  {\n    ' )

		if len (self.results) != 0:
		    f.write( self.results[0].name + ' = ' )

		f.write( self.ifName + '$' + self.name + '(_clp' )
		for i in self.pars :
		    if i.type.isLarge() or i.mode != 'IN' :
		      f.write( ', &' + i.name )
		    else :
		      f.write( ', ' + i.name )
		for i in self.results[1:] :
		    f.write( ', &' + i.name )

		f.write(');\n' )

		f.write( '    /* Marshal results */\n' )
		f.write( '    _bd = IDCServerBinding$InitReply(_bnd);\n' )
		f.write( '    if ( (True ' )

		for i in self.pars :
		    if i.mode != 'IN' :
		        f.write( ' &&\n        IDC_M_' + i.type.name + '(_idc, _bd, ') 
			if i.type.isLarge () : f.write( '&' )
			f.write( i.name + ')' )

		for i in self.results :
		    f.write( ' &&\n        IDC_M_' + i.type.name + '(_idc, _bd, ' + i.name + ')' )

		f.write( '))\n      IDCServerBinding$SendReply(_bnd, _bd);\n' )

		f.write( '    /* Must free large results now owned by "client" */\n' )
		for i in self.results :
		    if i.type.isLarge() or i.type.isString() :
		      f.write( '    ' + f.idc_f + '_' + i.type.name + '(' + \
			  f.clp + f.bd + ', ' + i.name + ');\n' )
		      if not i.type.isString() :
			  f.write( '    ' + f.free('') + i.name + ');\n' )

		f.write( '  }\n  FINALLY\n  {\n' )

		idcFreeUnmarshalledArgs( self.pars, f )

		f.write( '  }\n  ENDTRY\n\n' )
		f.write( '  return;\n\n' )

	def IDCClientStub( self, bindType, typedict, allExns, f ) :

	        name = self.ifName + '_' + self.name
		IDCClientDef( name, bindType, self.pars, self.results, f )
		
		count = 0
		flag = 0
		opslist = self.intf.allOps()
		while len(opslist)>0 and flag==0:
		    if opslist[0] == self:
			flag = 1
		    else:
			count = count + 1
			opslist = opslist[1:]
		
		f.write ( \
		    '  IDCClientStubs_State *_cst = self->st;\n' + \
		    '  IDCClientBinding_clp  _bnd = _cst->binding;\n' + \
		    '  IDC_clp	UNUSED	_idc = _cst->marshal;\n' + \
		    '  IDC_BufferDesc	_bd;\n' + \
		    '  uint32_t		_res;\n' + \
		    '  string_t         _exnname;\n' + \
		    '  \n' + \
		    '  _bd = IDCClientBinding$InitCall( _bnd, ' + `count` + ', "' + name + '");\n' + \
		    '  TRY\n' + \
		    '  {\n' \
		)

		f.write( BE.Tab + 'if (! ( True ' )
		for i in self.pars :
		    if i.mode != 'OUT' :
		        f.write( ' &&\n' + BE.Tab2 + 'IDC_M_' + i.type.name + '(_idc, _bd, ' )
			if i.mode != 'IN' and not i.type.isLarge () : f.write ('*')
			# XXX - if type is large, cast it to get rid off const
			# saves a warning
			if i.type.isLarge(): f.write('('+i.type.name+' *) ')
			f.write( i.name + ')' )
		f.write ( '))\n' + BE.Tab + '  RAISE_IDC$Failure ();\n\n' )

		if self.type == 'ANNOUNCEMENT' :
		    f.write( BE.Tab + 'IDCClientBinding$SendCall (_bnd, _bd);\n' )
		else :
		    self.IDCOpClientStub( typedict, allExns, f )
		
		f.write( \
		    '  }\n' + \
		    '  FINALLY\n' + \
		    '    IDCClientBinding$AckReceive( _bnd, _bd );\n' + \
		    '  ENDTRY;\n\n')

		if len (self.results) != 0:
		    f.write( '  return ' + self.results[0].name + ';\n' )
		else :
		    f.write( '  return;\n' )

		f.write ('\n}\n\n')

	def IDCOpClientStub( self, typedict, allExns, f ) :
	        f.write ( \
		    '    IDCClientBinding$SendCall ( _bnd, _bd );\n' + \
		    '    _res = IDCClientBinding$ReceiveReply(_bnd, &_bd, &_exnname);\n' + \
		    '\n' + \
		    '    switch (_res)\n' + \
		    '    {\n' + \
		    '    case 0:\n')

		mutes = 0
		for i in self.pars : 
		    if i.mode != 'IN' :
		        mutes = mutes + 1
			if mutes == 1 :
			    f.write ( BE.Tab + 'if (! (' )
			else :
			    f.write ( ' &&\n' + BE.Tab2 )
		        f.write( 'IDC_U_' + i.type.name + '(_idc, _bd, ' + i.name + ')' )
		if mutes :
		    f.write( '))\n' + BE.Tab + '  RAISE_IDC$Failure ();\n\n')


		if len (self.results) != 0 :
		    f.write( BE.Tab + '/* Unmarshal results */\n' )
		    idcUnmarshalResults ( self.results, f )

		f.write( '      break;\n' )

		if len (self.results) != 0 :
		    f.write( '\n' )
		    idcUnwindUnmarshalledResults( self.results, f )
		    f.write( BE.Tab + 'RAISE_IDC$Failure ();\n' )

		n = 1
		for i in allExns :
		  if i in self.raises :
		    i.IDCClientRaiser( f, n )
		  n = n + 1

		f.write( '    default:	RAISE_IDC$Failure ();\n    }\n' )
        def idcSig (self, f) :
		name = self.ifName + '$' + self.name
		name_args = name + '_Args'

		if len (self.pars) != 0 :
		  f.write( 'extern bool_t ' + f.idc_m + '_' + name + '(' \
		      + f.cl + ' *,IDC_BufferDesc, ' + name_args + ' *);\n')
		  f.write( 'extern ' + name_args + ' *' + f.idc_u + '_'  \
		      + name + '(' + f.cl + ' *,IDC_BufferDesc);\n')
		else :
		  f.write( '#define ' + f.idc_m + '_' + name + '(_idc,_bd,args)' \
		      + BE.Tab + f.true + '\n')
		  f.write( '#define ' + f.idc_u + '_' + name + '(_idc,_bd)' \
		      + BE.Tab + 'NULL\n')
		f.write ('\n')


class Exception(BE.Exception): 
	#
	# dme-style:
	#

	def IDCServerCatcher( self, f, n ) :
	  name = self.ifName + '$' + self.name
	  boilerplate = \
	      '    CATCH (' + name + ')\n' + \
	      '    {\n' + \
	      '      _bd = IDCServerBinding$InitExcept( _bnd, ' + `n` + ', "' + name + '" );\n' + \
	      '      if (! IDC_M_'+name+'(_idc,_bd, EXN_ARGP)) \n' + \
	      '        RAISE_IDC$Failure ();\n' + \
	      '      IDCServerBinding$SendReply( _bnd, _bd);\n' + \
	      '    }\n'
	  f.write (boilerplate)

	def IDCClientRaiser( self, f, n ) :
	  name = self.ifName + '$' + self.name
	  f.write ( \
	      '    case ' + `n` + ':	RAISE (' + name + ',	IDC_U_' + name + ' (_cst->marshal, _bd));\n')

        def idcSig (self, f) :
		name = self.ifName + '$' + self.name
		name_args = name + '_Args'

		if len (self.pars) != 0 :
		  f.write( 'extern bool_t ' + f.idc_m + '_' + name + '(' \
		      + f.cl + ' *,IDC_BufferDesc, ' + name_args + ' *);\n')
		  f.write( 'extern ' + name_args + ' *' + f.idc_u + '_'  \
		      + name + '(' + f.cl + ' *,IDC_BufferDesc);\n')
		else :
		  f.write( '#define ' + f.idc_m + '_' + name + '(_idc,_bd,args)' \
		      + BE.Tab + f.true + '\n')
		  f.write( '#define ' + f.idc_u + '_' + name + '(_idc,_bd)' \
		      + BE.Tab + 'NULL\n')
		f.write ('\n')
	def idcMarshal (self, f) : 
		if len (self.pars) != 0 :
		  name = self.ifName + '$' + self.name
		  name_args = name + '_Args'
		  f.write( 'bool_t ' + f.idc_m + '_' + name + '(' \
		      + f.cl + ' *_idc, IDC_BufferDesc _bd,' + name_args + ' *_args)\n' \
		      + '{\n' \
		      + '  return (\n' )
		  
		  for i in self.pars : 
		    f.write( BE.Tab2 + f.idc_m + '_' + i.type.name \
			+ '(_cst->marshal,_bd, _args->' + i.name + ')' )
		    if i != self.pars[-1] : f.write (' &&\n')

		  f.write( ');\n}\n\n' )
		    

	def idcUnmarshal (self, f) : 
		if len (self.pars) != 0 :
		  name = self.ifName + '$' + self.name
		  name_args = name + '_Args'
		  f.write( name_args + ' *' + f.idc_u + '_'  \
		      + name + '(' + f.cl + ' *_idc, IDC_BufferDesc _bd)\n' \
		      + '{\n' + '  ' + name_args \
		      + ' *__rec = exn_rec_alloc(sizeof(' + name_args + '));' \
		      + '\n\n  if( !(\n' )
		  
		  for i in self.pars : 
		    f.write( BE.Tab2 + f.idc_u + '_' + i.type.name \
			+ '(_cst->marshal,_bd, &(__rec->' + i.name + '))' )
		    if i != self.pars[-1] : f.write (' &&\n')

		  f.write( '))\n    RAISE_IDC$Failure ();\n' \
		      + '  return __rec;\n}\n\n' )





class Type(BE.Type):
    pass

class Predefined(BE.Predefined): pass

class Alias(BE.Alias): 
	def idcSig( self, name, f ) :
		if self.isLarge() :
		  idcSigAliasLarge( name, self.base.name, self.base.name, f )
		else :
		  idcSigAliasSmall( name, self.base.name, self.base.name, f )



class Enumeration(BE.Enumeration):
	def idcSig( self, name, f ) :
		idcSigAliasSmall( name, 'uint32_t', 'uint32_t', f )


class Record(BE.Record):
	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		for i in self.mems :
			i.idcFree( f )
		f.write( BE.Tab + 'return;\n}\n\n' )

	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		for i in self.mems :
			i.idcMarshal( f )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		for i in self.mems :
			i.idcUnmarshal( f )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n\n' )
		self.mems.reverse()
		firsttime = 0
		for i in self.mems :
			i.idcUnwind( f, firsttime )
			firsttime = 1
		f.write( BE.Tab + 'return( ' + f.false + ' );\n}\n\n' )
		self.mems.reverse()


class RecordMember(BE.RecordMember):
	def idcFree( self, f ) :
		f.write( BE.Tab + f.idc_f + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->' + self.name + ' );\n' )

	def idcMarshal( self, f ) :
		f.write( BE.Tab + 'if ( !' + f.idc_m + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->' + self.name + ' ) )\n' )
		f.write( BE.Tab2 + 'return ( ' + f.false + ' );\n' )

	def idcUnmarshal( self, f ) :
		f.write( BE.Tab + 'if ( !' + f.idc_u + '_'+ self.type.name + '(_idc,_bd, ' )
		# Pointer always wanted even for small types
		f.write( '&' )
		f.write( 'obj->' + self.name + ' ) )\n' )
		f.write( BE.Tab2 + 'goto unwind_' + self.name + ';\n' )
	
	def idcUnwind( self, f, firsttime ) :
	    if firsttime != 0:
		f.write( BE.Tab + f.idc_f + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->' + self.name + ' );\n' )
	    f.write( '  unwind_' + self.name + ':\n' )


class Choice(BE.Choice): 
	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

		### ASSUMPTION: Tags do not need freeing code
	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		f.write( BE.Tab + 'switch ( obj->tag ) {\n' )
		for i in self.elems :
			i.idcFree( f , self.base.name )
		f.write( BE.Tab + '}\n' )
		f.write( '}\n\n' )
		
	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		f.write( BE.Tab + 'if ( !' + f.idc_m + '_' + self.base.name )
		f.write( '(_idc,_bd, obj->tag ) )\n' )
		f.write( BE.Tab2 + 'return( ' + f.false + ' );\n\n' )
		f.write( BE.Tab + 'switch ( obj->tag ) {\n' )
		for i in self.elems : 
			i.idcMarshal( f , self.base.name )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		f.write( BE.Tab + 'if ( !' + f.idc_u + '_' + self.base.name )
		f.write( '(_idc,_bd, &obj->tag ) )\n' )
		f.write( BE.Tab2 + 'return( ' + f.false + ' );\n\n' )
		f.write( BE.Tab + 'switch ( obj->tag ) {\n' )
		for i in self.elems : 
			i.idcUnmarshal( f , self.base.name )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n}\n\n' )

class ChoiceElement(BE.ChoiceElement): 
	def idcFree( self, f, name ) :
		f.write( BE.Tab + 'case ' + name + '_' + self.name + ':\n' )
		f.write( BE.Tab2 + f.idc_f + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->u.' + self.name + ' );\n' )
		f.write( BE.Tab2 + 'break;\n' )

	def idcMarshal( self, f, name ) :
		f.write( BE.Tab + 'case ' + name + '_' + self.name + ':\n' )
		f.write( BE.Tab2 + 'if ( !' + f.idc_m + '_' + self.type.name + '(_idc,_bd, ' )
		if self.type.isLarge() :
		    f.write( '&' )
		f.write( 'obj->u.' + self.name + ' ) )\n' )
		f.write( BE.Tab3 + 'return( ' + f.false + ' );\n' )
		f.write( BE.Tab2 + 'break;\n' )

	def idcUnmarshal( self, f, name ) :
		f.write( BE.Tab + 'case ' + name + '_' + self.name + ':\n' )
		f.write( BE.Tab2 + 'if ( !' + f.idc_u + '_' + self.type.name + '(_idc,_bd, ' )
		f.write( '&' )
		f.write( 'obj->u.' + self.name + ' ) )\n' )
		f.write( BE.Tab3 + 'return( ' + f.false + ' );\n' )
		f.write( BE.Tab2 + 'break;\n' )


class Sequence(BE.Sequence):

	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

	# Assumption: length field does not need freeing code
	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		f.write( BE.Tab + self.base.name + BE.Tab + '*ptr;\n' )
		f.write( BE.Tab + 'for (ptr=SEQ_START(obj); ptr<SEQ_END(obj); ptr++)\n' )
		f.write( BE.Tab + '{\n' )
		f.write( BE.Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr);\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'SEQ_FREE_DATA (obj);\n' )
		f.write( '}\n\n' )
		
	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		f.write( BE.Tab + self.base.name + BE.Tab + '*ptr;\n' )
		f.write( BE.Tab + 'if (!' + f.idc_m + '_uint32_t(_idc,_bd, obj->len))\n' )
		f.write( BE.Tab2 + 'return ' + f.false + ';\n\n' )
		f.write( BE.Tab + 'for (ptr=SEQ_START(obj); ptr<SEQ_END(obj); ptr++)\n' )
		f.write( BE.Tab + '{\n' )
		f.write( BE.Tab2 + 'if (! ' + f.idc_m + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr))\n' )
		f.write( BE.Tab3 + 'return ' + f.false + ';\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n' )
		f.write( '}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		f.write( BE.Tab + self.base.name + BE.Tab + '*ptr;\n' )
		f.write( BE.Tab + 'uint32_t ' + BE.Tab + '_n;\n\n' )
		f.write( BE.Tab + 'if ( !' + f.idc_u + '_uint32_t(_idc,_bd, &_n ) )\n' )
		f.write( BE.Tab2 + 'return( ' + f.false + ' );\n\n' )
		f.write( BE.Tab + 'SEQ_INIT (obj, _n, ' + f.heap('_bd') +');\n\n')
		f.write( BE.Tab + 'for (ptr=SEQ_START(obj); ptr<SEQ_END(obj); ptr++)\n' )
		f.write( BE.Tab + '{\n' )
		f.write( BE.Tab2 + 'if (!' + f.idc_u + '_' + self.base.name + '(_idc,_bd, ptr))\n' )
		f.write( BE.Tab2 + 'goto unwind;\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n\n' )
		f.write( ' unwind:\n' )
		f.write( BE.Tab + 'while (ptr > SEQ_START(obj)) {\n' )
		f.write( BE.Tab2 + 'ptr--;\n' )
		f.write( BE.Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ')
		if not self.base.isLarge() :
		    f.write( '*' );
		f.write( 'ptr);\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'SEQ_FREE_DATA (obj);\n' )
		f.write( BE.Tab + 'return( ' + f.false + ' );\n' )
		f.write( '}\n\n' )

class Array(BE.Array): 
	def idcSig( self, name, f ) :
		return idcSigOtherLarge( name, f )

	def idcFree( self, name, f ) :
		idcFreeFuncbegin( name, f )
		f.write( BE.Tab + self.base.name + BE.Tab + '*ptr = &((*obj)[0]);\n' )
		f.write( BE.Tab + 'uint32_t' + BE.Tab + 'i;\n\n' )
		f.write( BE.Tab + 'for (i=0; i<' + `self.size` + '; i++, ptr++)\n' )
		f.write( BE.Tab + '{\n' )
		f.write( BE.Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr);\n' )
		f.write( BE.Tab + '}\n' )
		f.write( '}\n\n' )
		
	def idcMarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.m, f )
		f.write( BE.Tab + self.base.name + BE.Tab + '*ptr = &((*obj)[0]);\n' )
		f.write( BE.Tab + 'uint32_t ' + BE.Tab + 'i;\n\n' )
		f.write( BE.Tab + 'for (i=0; i<' + `self.size` + '; i++, ptr++)\n' )
		f.write( BE.Tab + '{\n' )
		f.write( BE.Tab2 + 'if (!' + f.idc_m + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr))\n' )
		f.write( BE.Tab3 + 'return ' + f.false + ';\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n}\n\n' )

	def idcUnmarshal( self, name, f ) :
		idcMarshalFuncbegin( name, f.u, f )
		f.write( BE.Tab + self.base.name + BE.Tab + '*ptr = &((*obj)[0]);\n' )
		f.write( BE.Tab + 'uint32_t ' + BE.Tab + 'i;\n\n' )
		f.write( BE.Tab + 'for (i=0; i<' + `self.size` + '; i++, ptr++)\n' )
		f.write( BE.Tab + '{\n' )
		f.write( BE.Tab2 + 'if (!' + f.idc_u + '_' + self.base.name + '(_idc,_bd, ' )
		# Pointer always wanted even for small types
		f.write( 'ptr))\n' )
		f.write( BE.Tab2 + 'goto unwind;\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.true + ' );\n\n' )
		f.write( ' unwind:\n' )
		f.write( BE.Tab + 'while (i--) {\n' )
		f.write( BE.Tab2 + 'ptr--;\n' )
		f.write( BE.Tab2 + f.idc_f + '_' + self.base.name + '(_idc,_bd, ' )
		if not self.base.isLarge() :
		    f.write( '*' )
		f.write( 'ptr);\n' )
		f.write( BE.Tab + '}\n' )
		f.write( BE.Tab + 'return( ' + f.false + ' );\n}\n\n' )

class BitSet(BE.BitSet):

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

class ByteSequence(BE.ByteSequence): 
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


class ByteArray(BE.ByteArray): 
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

class Set(BE.Set): 
	def idcSig( self, name, f ) :
		return idcSigAliasSmall( name, 'set_t', 'set_t', f )

class Ref(BE.Ref): 

	def idcSig( self, name, f ) :
		return idcSigAliasSmall( name, 'addr_t', 'addr_t', f )

class InterfaceRef(BE.InterfaceRef): 
	def idcSig( self, name, f ) :
		return idcSigAliasSmall( name, 'addr_t', 'addr_t', f )

class Import(BE.Import):
    def idcMarshalHdr( self, f ) :
	f.write( '#include "' + marshallhmap(self.name) + '"\n' )
      
    def IDCMarshalHdr( self, f ) :
	f.write( '#include "' + marshallhmap(self.name) + '"\n' )
    


class IDCFile:
    # A file and some string constants which vary between IDC styles
    def init (self, f) :
      self.f      = f
      self.idc    = 'idc'
      self.m      = 'm'
      self.u      = 'u'
      self.cl     = 'IDCControl'
      self.clp    = 'ctrl'
      self.bd     = ''
      self.idc_m  = 'idc_m'
      self.idc_u  = 'idc_u'
      self.idc_f  = 'idc_f'
      self.true   = 'TRUE'
      self.false  = 'FALSE'
      return self

    def malloc (self, clp) :
      return 'Malloc(' + clp + '->idc_heap, '

    def free (self, clp) :
      return 'FREE ('

    # Imported from oldModBE.py
    def heap (self, clp) :
      return '(' + clp + '->idc_heap)'
    # End import

    def write (self, str) :
      self.f.write (str)

class IDC1File:
    # A file and some string constants which vary between IDC styles
    def init (self, f) :
      self.f     = f
      self.idc   = 'IDC'
      self.m     = 'M'
      self.u     = 'U'
      self.cl    = 'IDC_cl'
      self.clp   = '_idc'
      self.bd    = ',_bd'
      self.idc_m = 'IDC_M'
      self.idc_u = 'IDC_U'
      self.idc_f = 'IDC_F'
      self.true  = 'True'
      self.false = 'False'
      return self

    def malloc (self, clp) :
      return 'Heap$Malloc(_bd->heap, '

    def free (self, clp) :
      return 'FREE ('

    def heap (self, clp) :
      return '(_bd->heap)'

    def write (self, str) :
      self.f.write (str)

def GenerateIDC1Marshalling(intf):
    # Create DME-style Local IDC Marshaling code

    IDC_M_h = IDC1File().init(open (marshallhmap(intf.name) , 'w'))
    intf.IDCMarshalHeader( IDC_M_h )

    IDC_M_c = IDC1File().init(open (marshallcmap(intf.name) , 'w'))
    if len (intf.types) != 0 :
	intf.IDCMarshalCode( IDC_M_c )
    else :
	blank( IDC_M_c )

def GenerateIDC1Stubs(intf):
    # Create DME-style Local IDC Stub code

    IDC_S_c = IDC1File().init(open ( stubscmap(intf.name) , 'w'))

    if len (intf.ops) != 0 :
	intf.IDCServerStubs( IDC_S_c )
	intf.IDCClientStubs( IDC_S_c )
    else :
	BE.blank(IDC_S_c)


def ServerLocals( pars, f ) :
	for i in pars : 
	    f.write( '  ' + i.type.name + BE.Tab + i.name + \
		';' + BE.Tab + '/* ' + i.mode + ' */\n' )

#
# Output the locals used to store the results returned by a server routine
#
def ServerResults( results, f ) :
	for i in results:
	    if i.type.isLarge() :
	        f.write( '  ' + i.type.name + BE.Tab + '*' + i.name + ';\n' )
	    else :
	        f.write( '  ' + i.type.name + BE.Tab + i.name + ';\n' )

#
# Output unmarshalling code for incoming args (in server operation stubs)
#
def idcUnmarshalArgs ( args, f ) :
	for i in args :
	  if i.mode != 'OUT':
	    f.write( BE.Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + '( ' + \
		f.clp + f.bd + ', &' + i.name + ') )\n' + \
		  BE.Tab2 + 'goto unwind_' + i.name + ';\n' )

	f.write( '\n' )

def idcFreeUnmarshalledArgs ( args, f ) :
	for i in args :
	    f.write( '    ' + f.idc_f + '_' + i.type.name + '( ' + f.clp + f.bd + ', ' )

	    if i.type.isLarge() :
	      f.write( '&' )

	    f.write( i.name + ');\n' )

def idcUnwindUnmarshalledArgs ( args, f ) :
	args.reverse()
	for i in args :
	  if i.mode != 'OUT':
	    if i != args[0]:
		f.write( BE.Tab + f.idc_f + '_' + i.type.name + '( ' + f.clp + f.bd + ', ' )
		if i.type.isLarge() :
		    f.write( '&' )
		f.write( i.name + ');\n' )
	    f.write( '  unwind_' + i.name + ':\n' )

	f.write( '\n' )
	args.reverse()

#
# Output unmarshalling code for incoming results (in client operation stubs)
#
def idcUnmarshalResults ( results, f ) :
        # the first result is always local
	i = results[0]
	if i.type.isLarge() :
	  f.write( BE.Tab + 'if (!(' + i.name + ' = ' + \
	      f.malloc(f.clp) + 'sizeof(*' + i.name + '))))\n' + \
	      BE.Tab2 + 'goto unwindmem_' + i.name + ';\n' )
	  
	if i.type.isString():
	    argcast = ' (string_t *) '
	else: 
	    argcast = ''


	if i.type.isLarge() :
	  f.write( BE.Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + '( ' + \
	    f.clp + f.bd + ', /* XXX no & */' + i.name + ') )\n' + \
	    BE.Tab2 + 'goto unwind_' + i.name + ';\n' )
	else :
	  f.write( BE.Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + '( ' + \
	    f.clp + f.bd + ','+ argcast +'&' + i.name + ') )\n' + \
	    BE.Tab2 + 'goto unwind_' + i.name + ';\n' )

	for i in results[1:] :
	    if i.type.isLarge() :
		f.write( BE.Tab + 'if (!(*' + i.name + ' = ' + \
		    f.malloc(f.clp) + 'sizeof(**' + i.name + '))))\n' + \
		    BE.Tab2 + 'goto unwindmem_' + i.name + ';\n' )

	    f.write( BE.Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + \
		'( ' + f.clp + f.bd + ', ' )

	    if i.type.isLarge() :
		f.write( '*' )

	    f.write( i.name + ') )\n' + \
		BE.Tab2 + 'goto unwind_' + i.name + ';\n' )

	f.write( '\n' )

def idcUnwindUnmarshalledResults ( results, f ) :
        first_result = results[0]
	results.reverse()
	for i in results :
	    if i != results[0]:
		f.write( BE.Tab + f.idc_f + '_' + i.type.name + '( ' + f.clp + f.bd + ', ' )
		if (i.type.isLarge() or i.type.isString()) and i != first_result:
		    f.write( '*' )
		f.write( i.name + ');\n' )
	    f.write( '    unwind_' + i.name + ':\n' )
	    if i.type.isLarge() :
		# Free the memory
		f.write( BE.Tab + f.free(f.clp) )
		if i != first_result :
		    f.write( '*' )
		f.write( i.name + ');\n' )
		f.write( '    unwindmem_' + i.name + ':\n' )
	f.write( '\n' )
	results.reverse()


def Go(kind, intflist, fp):
  intf = intflist[-1].obj
  l = []
  if kind == 'stubs':
      sys.stdout.write('('+intf.name+' stubs'+')')
      sys.stdout.flush()
      GenerateIDC1Stubs(intf)
      l.append(stubscmap(intf.name))

  if kind == 'marshalling':
      sys.stdout.write('('+intf.name+' marshalling'+')')
      sys.stdout.flush()
      GenerateIDC1Marshalling(intf)
      l.append(marshallcmap(intf.name))
      l.append(marshallhmap(intf.name))
  return l
