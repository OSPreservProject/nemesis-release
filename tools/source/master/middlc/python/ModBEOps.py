#
#	ModBEOps.py
#	-----------
#
# $Id: ModBEOps.py 1.1 Thu, 18 Feb 1999 14:20:06 +0000 dr10009 $
#

import sys
import string

Tab = '\t'
Tab2 = Tab + Tab
Tab3 = Tab2 + Tab
Tab4 = Tab3 + Tab

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

        def representation( self, f ):
	  	  
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
	  f.write( '  ' + `len(self.pars)` + ',\n' )
	  f.write( '  ' + `len(self.results)` + ',\n')
	  f.write( '  ' + `self.number` + ',\n')
	  f.write( '  (void *) &' + self.fullName + '__opcl\n};\n\n')

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
		
	def template(self, f, intf, implName, suffix) :

	        f.write ('static ')
		self.fnDecl (implName, intf.clType, suffix, f, 0)
		f.write ('\n{\n')

		stType = implName + '_st'
		f.write ('  ' + stType + Tab + '*st = self->st;\n\n')
		f.write ('  UNIMPLEMENTED;\n')
		f.write ('}\n\n')

	# ########
	# IDC Code - dme style
	# ########

	def IDCServerStub( self, bindType, typedict, f ) :
		f.write( \
		    'PUBLIC void\n' + self.ifName + '_' + self.name + '_S (' + bindType + \
		    ' _clp, IDCServerBinding_clp _bnd, IDC_clp _idc, IDC_BufferDesc NOCLOBBER _bd)\n' + \
		    '{\n')
# _heap isn't used
#		    '  Heap_clp' + Tab + '_heap = _bd->heap;\n' 

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

		f.write ( \
		    '  IDCClientStubs_State *_cst = self->st;\n' + \
		    '  IDCClientBinding_clp  _bnd = _cst->binding;\n' + \
		    '  IDC_clp	UNUSED	_idc = _cst->marshal;\n' + \
		    '  IDC_BufferDesc	_bd;\n' + \
		    '  uint32_t		_res;\n' + \
		    '  \n' + \
		    '  _bd = IDCClientBinding$InitCall( _bnd, ' + `self.number` + ', "' + name + '");\n' + \
		    '  TRY\n' + \
		    '  {\n' \
		)

		f.write( Tab + 'if (! ( True ' )
		for i in self.pars :
		    if i.mode != 'OUT' :
		        f.write( ' &&\n' + Tab2 + 'IDC_M_' + i.type.name + '(_idc, _bd, ' )
			if i.mode != 'IN' and not i.type.isLarge () : f.write ('*')
			# XXX - if type is large, cast it to get rid off const
			# saves a warning
			if i.type.isLarge(): f.write('('+i.type.name+' *) ')
			f.write( i.name + ')' )
		f.write ( '))\n' + Tab + '  RAISE_IDC$Failure ();\n\n' )

		if self.type == 'ANNOUNCEMENT' :
		    f.write( Tab + 'IDCClientBinding$SendCall (_bnd, _bd);\n' )
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
		    '    _res = IDCClientBinding$ReceiveReply(_bnd, &_bd, NULL);\n' + \
		    '\n' + \
		    '    switch (_res)\n' + \
		    '    {\n' + \
		    '    case 0:\n')

		mutes = 0
		for i in self.pars : 
		    if i.mode != 'IN' :
		        mutes = mutes + 1
			if mutes == 1 :
			    f.write ( Tab + 'if (! (' )
			else :
			    f.write ( ' &&\n' + Tab2 )
		        f.write( 'IDC_U_' + i.type.name + '(_idc, _bd, ' + i.name + ')' )
		if mutes :
		    f.write( '))\n' + Tab + '  RAISE_IDC$Failure ();\n\n')


		if len (self.results) != 0 :
		    f.write( Tab + '/* Unmarshal results */\n' )
		    idcUnmarshalResults ( self.results, f )

		f.write( '      break;\n' )

		if len (self.results) != 0 :
		    f.write( '\n' )
		    idcUnwindUnmarshalledResults( self.results, f )
		    f.write( Tab + 'RAISE_IDC$Failure ();\n' )

		n = 1
		for i in allExns :
		  if i in self.raises :
		    i.IDCClientRaiser( f, n )
		  n = n + 1

		f.write( '    default:	RAISE_IDC$Failure ();\n    }\n' )

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
		      + Tab + f.true + '\n')
		  f.write( '#define ' + f.idc_u + '_' + name + '(_idc,_bd)' \
		      + Tab + 'NULL\n')
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
		    f.write( Tab2 + f.idc_m + '_' + i.type.name \
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
		    f.write( Tab2 + f.idc_u + '_' + i.type.name \
			+ '(_cst->marshal,_bd, &(__rec->' + i.name + '))' )
		    if i != self.pars[-1] : f.write (' &&\n')

		  f.write( '))\n    RAISE_IDC$Failure ();\n' \
		      + '  return __rec;\n}\n\n' )


	def idcServerCatcher( self, f ) :
		f.write( Tab + '    CATCH_' + self.ifName + '$' + self.name + '(' )
		self.arglist( f )
		f.write( ')\n' + Tab2 + 'idc_prepare_tx(ctrl);\n' )
		f.write( Tab2 + 'if (!idc_m_MIDDL_String(ctrl, ' + self.ifName + '$' + self.name + '))\n' )
		f.write( Tab3 + 'RAISE(NULL, NULL);\n' )
		for i in self.pars :
		    f.write( Tab2 + 'if (! idc_m_' + i.type.name + \
			'(ctrl, _' + i.name + ') )\n' )
		    f.write( Tab3 + 'RAISE(NULL, NULL);\n' )
		f.write( Tab2 + 'idc_send(ctrl);\n' )

	def idcClientRaiser( self, f, extra ) :
		f.write( Tab2 + extra + 'if (exn_matches(_exn, ' + self.ifName + '$' + self.name + '))\n' )
		if len (self.pars) == 0 :
		    f.write( Tab3 + 'RAISE(' + self.ifName + '$' + self.name + ', NULL);\n' )
		else :
		    f.write( Tab2 + '{\n' )
		    f.write( Tab3 + self.ifName + '$' + self.name + '_Args' + Tab + '*__rec = exn_rec_alloc(sizeof(' + self.ifName + '$' + self.name + '_Args' + '));\n' )
		    # Arguments to exceptions are shallow. Thus there
		    # is no need to have unwind code.
		    for i in self.pars :
		        f.write( Tab3 + 'if (!idc_u_' + i.type.name + '(ctrl, &__rec->' + i.name + '))\n' )
			f.write( Tab4 + 'RAISE(NULL, NULL);\n' )

		    f.write( Tab3 + 'RAISE (' + self.ifName + '$' + self.name + ', __rec);\n' )
		    f.write( Tab2 + '}\n' )

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

##############################################################
#
# Parameter field
#
##############################################################
class Parameter:

	def typedef( self, f, sep, prefix ) :
		f.write (sep + '\n' + Tab)
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

#
# Output the locals used by a server routine
#	
def ServerLocals( pars, f ) :
	for i in pars : 
	    f.write( '  ' + i.type.name + Tab + i.name + \
		';' + Tab + '/* ' + i.mode + ' */\n' )

#
# Output the locals used to store the results returned by a server routine
#
def ServerResults( results, f ) :
	for i in results:
	    if i.type.isLarge() :
	        f.write( '  ' + i.type.name + Tab + '*' + i.name + ';\n' )
	    else :
	        f.write( '  ' + i.type.name + Tab + i.name + ';\n' )

#
# Output unmarshalling code for incoming args (in server operation stubs)
#
def idcUnmarshalArgs ( args, f ) :
	for i in args :
	  if i.mode != 'OUT':
	    f.write( Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + '( ' + \
		f.clp + f.bd + ', &' + i.name + ') )\n' + \
		  Tab2 + 'goto unwind_' + i.name + ';\n' )

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
		f.write( Tab + f.idc_f + '_' + i.type.name + '( ' + f.clp + f.bd + ', ' )
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
	  f.write( Tab + 'if (!(' + i.name + ' = ' + \
	      f.malloc(f.clp) + 'sizeof(*' + i.name + '))))\n' + \
	      Tab2 + 'goto unwindmem_' + i.name + ';\n' )
	  
	if i.type.isString():
	    argcast = ' (string_t *) '
	else: 
	    argcast = ''


	if i.type.isLarge() :
	  f.write( Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + '( ' + \
	    f.clp + f.bd + ', /* XXX no & */' + i.name + ') )\n' + \
	    Tab2 + 'goto unwind_' + i.name + ';\n' )
	else :
	  f.write( Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + '( ' + \
	    f.clp + f.bd + ','+ argcast +'&' + i.name + ') )\n' + \
	    Tab2 + 'goto unwind_' + i.name + ';\n' )

	for i in results[1:] :
	    if i.type.isLarge() :
		f.write( Tab + 'if (!(*' + i.name + ' = ' + \
		    f.malloc(f.clp) + 'sizeof(**' + i.name + '))))\n' + \
		    Tab2 + 'goto unwindmem_' + i.name + ';\n' )

	    f.write( Tab + 'if ( ! ' + f.idc_u + '_' + i.type.name + \
		'( ' + f.clp + f.bd + ', ' )

	    if i.type.isLarge() :
		f.write( '*' )

	    f.write( i.name + ') )\n' + \
		Tab2 + 'goto unwind_' + i.name + ';\n' )

	f.write( '\n' )

def idcUnwindUnmarshalledResults ( results, f ) :
        first_result = results[0]
	results.reverse()
	for i in results :
	    if i != results[0]:
		f.write( Tab + f.idc_f + '_' + i.type.name + '( ' + \
		    f.clp + f.bd + ', ' )
		if (i.type.isLarge() or i.type.isString()) and i != first_result:
		    f.write( '*' )
		f.write( i.name + ');\n' )
	    f.write( '    unwind_' + i.name + ':\n' )
	    if i.type.isLarge() :
		# Free the memory
		f.write( Tab + f.free(f.clp) )
		if i != first_result :
		    f.write( '*' )
		f.write( i.name + ');\n' )
		f.write( '    unwindmem_' + i.name + ':\n' )
	f.write( '\n' )
	results.reverse()



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
	    f.write( Tab +results[0].type.name + Tab )
	    if results[0].type.isLarge() :
	        f.write('*')
	    f.write(' NOCLOBBER ')
	    f.write( results[0].name)
	    if results[0].type.isLarge() :
		f.write('=0')
	    f.write(';\n' )

# End of file
