#! /usr/bin/env python

import sys
import os
import pwd
import time
import types
import md5
import string
from socket import gethostname
from group import list_group
from decl import list_decl
from spec import list_spec
from type import list_type
from expr import list_expr
import BE, os, astcompile
error = astcompile.error
# search path astpath for something named astpath, and open that file
def OpenAst(astname, astpath):
    o = 0
    i = 0
    f = None
    for i in range(0, len(astpath)):
        filename = astpath[i] + '/'+ astname
        if os.path.exists(filename):
            try:
                f = open(astpath[i]+'/'+astname, 'r')
            except:
                pass
        if f: break
    if not f:
        astcompile.compile( [astname[:-4]+'.if'])
        if os.path.exists(astname):
            f = open(astname, 'r')
        else:
            error('Could not find '+astname+' and could not compile it\n')
    return f


# Build a custom back end, from the list of targets in args
# Returns a back end represented as a dictionary of classes
#
# The back end produced is a set of classes. A superclass
# for each backend class is declared in the module BE.
#
# args are converted to the Python modules, and those are
# imported. Each class in the list 'clar' is looked up in
# that module, and if it is presnet is added to the clinherit
# dictionary under that classes name.
# 
# Each back end module loaded should have inherited from
# the corresponding classes in BE. So, when all the back end 
# modules have been loaded, the clinherit will contain a list
# of sub classes for each class in BE. Some of these lists 
# may be empty; a back end module need not necessarily declare
# any partciular class. Looking up a non existent class would
# raise an exception, so there is a catch for this round the
# code that raises it.
#
# This routine then constructs python code to declare a subclass
# for each back end class that inherits from all the classes
# in the clinherit dictionary entry. If the entry is null, it 
# inherits directly from the BE class. This python code is evaluated,
# and the class objects thus created and placed in classtable.
#
# Classtable also has a few extras:
#
# golist: Each module must declare a Go routine, which is placed
#         in the back end golist. They are called in sequence when
#         the python AST object has been fully constructed
# builtin: A dictionary of instances of all the 'predefined'
#          types. 

def setupbe(args):
    # the list of classes in the back end
    clar = [ 'Interface', 'Operation', 'Type', 'Predefined', 'Alias', 'Enumeration', 'Record', 'RecordMember', 'Choice', 'ChoiceElement', 'Array', 'Sequence','BitSet', 'ByteSequence', 'ByteArray', 'Set', 'Ref', 'InterfaceRef', 'Import', 'Exception', 'Parameter', 'Result' ]
    clinherit = {}
    for cl in clar:
	clinherit[cl] = []
    genlist = []

    for be in args:
	loc = string.find(be,'[')
	if loc == -1:
	    list = ['default']
	    name = be
	else:
	    name = be[:loc]
	    params = be[loc+1:-1]
	    list = string.split(params, ',')
	genlist.append( (name,list) )
	    
    golist = []

    for (arg,list) in genlist:
	impname = 'gen'+arg
	exec 'import '+impname
	for cl in clar:
	    gotit = 0
	    try:
		subcl = eval(impname+'.'+cl)
		gotit =1
	    except:
		pass
	    if gotit:
		clinherit[cl].append(impname+'.'+cl)
	    #print 'Registered '+impname+'.'+cl+'('+`subcl`+')'
	golist.append((eval(impname + '.Go'),list,arg,list))
    classtable = {}
    classtable['golist'] = golist
    for cl in clar:
	if len(clinherit[cl]) == 0:
	    l = 'BE.'+cl
	else:
	    l = ''
	    for interface in clinherit[cl]:
		l = l + interface
		if interface != clinherit[cl][-1]:
		    l = l + ','
	    
	str = 'class '+cl + '(' + l + ')' + ': pass'
	exec str
	classtable[cl] = eval(cl)
    predef = [ ('octet', 'uint8_t'), ('short_cardinal', 'uint16_t'), ('cardinal', 'uint32_t'), ('long_cardinal', 'uint64_t'), ('char', 'int8_t'), ('short_integer', 'int16_t'), ('integer', 'int32_t'), ('long_integer', 'int64_t'), ('real', 'float32_t'), ('long_real', 'float64_t'), ('boolean', 'bool_t'), ('string', 'string_t'), ('dangerous_word', 'word_t'), ('dangerous_address', 'addr_t') ]
    basictypesdict = {}
    count = 1
    for (middlname, cname) in predef:
	instance = classtable['Type']().predef(count, cname, classtable['Predefined']() )
	basictypesdict[middlname] = instance
	count = count +1
    classtable['builtin'] = basictypesdict
    return classtable

def hexstr(s):
	h = string.hexdigits
	r = ''
	for c in s:
		i = ord(c)
		r = r + h[(i >> 4) & 0xF] + h[i & 0xF]
	return r

# Generate a fingerpint from a string A fingerprint is the first 48
# bits of the md5 sum 

# Currently the python MD5 implementation under alpha appears to be
# broken - for now, we'll just run the md5 command over the string.

def genfp(str):
    if md5IsValid:
	fp = hexstr(md5.new(str).digest())[0:12]
    else:
	fname = 'md5sumstring.' + `os.getpid()`
	f = open(fname, 'w')
	f.write(str)
	f.close()
	p = os.popen('md5 <' + fname, 'r')
	d = p.read()
	p.close()
	os.unlink(fname)
	fp = d[0:12]
    return fp

md5IsValid = (hexstr(md5.new('a').digest()) == '0cc175b9c0f1b6a831c399e269772661')
    


#********************************************************************************
#********************************************************************************
#********************************************************************************


# tests that convertlist might be called with
# checks that certain conditions hold for each
# member of the list

# accept only  declarations for types or operations
# used by the spec routine

def testspecmember(node):
    (kind,_) = list_group[node.clss]
    if kind != 'decl':
	return 0
    (form,_) = list_decl[node.form]
    if form == 'type' or form=='procedure' or form=='procedure_nr' or form=='exception' or form=='announcement':
	return 1
    print 'Found a',form,'in a specification'
    return 0

# accept only arguments
# used by the method routine

def testarg(node):
    (kind,_) = list_group[node.clss]
    if kind != 'decl':
	return 0
    (form,_) = list_decl[node.form]
    if form == 'var' or form == 'invar' or form == 'outvar' or form == 'inoutvar':
	return 1
    print 'Found a',form,'in an argument list'
    return 0

# accept only identifiers
# used in various places, eg for enumeration fields

def testidentifier(node):
    (kind,_) = list_group[node.clss]
    if kind == 'identifier':
	return 1
    else:
	print 'Found a',form,'in an identifier list'
	return 0

# accept only raises clauses
# ie only scoped identifiers

def testraises(node):
    (kind,_) = list_group[node.clss]
    if kind == 'sco':
	return 1
    else:
	print 'Found a',form,'in a sco list'
	return 0


# called to post process an interfaces, settting 
# things up that can only be set up after all the
# interfaces have been read

def fixup(intf, intfdict,backend):
    # set the imports list
    for i in intf.obj.needslist:
	imp = backend['Import']()
	imp.mode = 'NEEDS'
	ast = intfdict.lookup(i)
	imp.intf = ast.obj
	imp.name = i
	intf.obj.imports.append(imp)
	intf.RegisterUse(ast)

def fixupextends(intf, intfdict, backend):
    for i in intf.obj.extendslist:
	#print 'Interface '+intf.obj.name+' extends '+`intf.obj.extends`
	imp = backend['Import']()
	imp.mode = 'EXTENDS'
	ast = intfdict.lookup(i)
	imp.intf = ast.obj
	imp.name = i
	intf.obj.imports.append(imp)
	intf.RegisterUse(ast)

    # rewrite all IREF types to be proper closure types
    old = intf.obj.types
    new = []
    for type in old:
	if type.clss == 'IREF':
	    ast = intfdict.lookup(type.target)
	    intf.RegisterUse(ast)
	    new.append(ast.obj.closuretype)
	else:
	    new.append(type)
    intf.obj.types = new


#********************************************************************************
#********************************************************************************
#********************************************************************************



# One AstNode is created per word in an AST
# The class just provides some convient access
# functions for  things associated with this node.
# After a node has been processed, a back end object
# may be associated with it that so that if it is referred
# to in a shortcut the corresponding back end object may be found

class AstNode:
    def __init__( self, str ):
	self.clss = ord(str[3])
	self.form = ord(str[2])
	self.payload = (ord(str[1])<<8) + (ord(str[0]))
	self.str = str
	self.haveobj = 0
    def gettuple(self):
	return (self.clss, self.form, self.payload, self.str)
    def getinteger(self):
	x = 0
	for i in range (0,4):
	    x = x + (ord(self.str[i])<<(8*i))
	return x
    def registerobj(self, obj):
	self.obj = obj
	self.haveobj = 1
    def getobjname(self):
	if self.haveobj == 1:
	    return self.obj.name
	else:
	    return 'UNINSTANTIATED'
    def getobj(self):
	if self.haveobj == 1:
	    return self.obj
	else:
            error('Missing %s%s (this in an internal failure of astgen, alas; a node has been reference before it was initialised)' % (`self`, `self.clss`))
    #def __repr__(self):
#	return 'Node '+' '+`list_group[self.clss-1]`+' payload '+`self.payload`


# Exactly one AstReposit exists during the compiler run time
# An AstReposit stores .ast interfaces that have been read
# It has the capability to read new ast files
# So, a back end object for an interface may be obtained by invoking lookup
# If the interface has already been instantiated, it will not be reread	

class AstReposit:
    def __init__(self, backend, astpath):
	self.mapping = {}
	self.backend = backend
	self.exnlist = {}
	self.astpath = astpath

    def register(self, ast):
	sys.stdout.write('.')
	sys.stdout.flush()
	self.mapping[ast.intf.name] = ast

    def lookup(self, name):
	if not self.mapping.has_key(name):
	    f = OpenAst(name + '.ast', self.astpath)
	    ast = Ast(f.read(), self.backend, self)
	    f.close()
	    self.mapping[name] = ast
	    ast.obj = ast.convert()
	    ast.fp = genfp(ast.ast)
	    fixupextends(ast, self, self.backend)
	    return ast
	return self.mapping[name]
    def require(self,name):
	self.lookup(name)




#********************************************************************************
#********************************************************************************
#********************************************************************************

# An Ast corresponds to an .ast file, and thus to an interface
# There are two main entry points:
#   convert, which generates a back end object representing the interface
#   output, which produces ASCII output describing the interface

# These routines work recursively. Ast objects keep the .ast data within
# themselves, and contain an index in to this object.

class Ast:
    def __init__(self, ast, backend, reposit ):
	self.ast = ast
	self.loc = 0
	self.index = 0
	self.fieldmode = -1
	self.intflist = {}
	self.reposit = reposit
	self.inargs = 0
	self.haveintf = 0
	if len(ast)==0: 
	    print 'Zero length ast! Aborting'
	    sys.exit(1)
	if ord(ast[1]) == 0xd and ord(ast[2]) == 0xf0 and ord(ast[3]) == 0xa5:
	    #print 'An AST, by jove: revision '+`ord(ast[0])`
	    pass
	else:
	    print 'Imposter!'
	lb = len(ast)
	lw = lb/4
	if lw*4 != lb:
	    print 'Not a whole number of words'
        self.words=[]
        for i in range(1,lw):
	    self.words.append( AstNode(ast[i*4 : (i+1)*4]))
	self.be = backend
	self.intfgraph = {}

    # RegisterUse of interface
    def RegisterUse(self, i):
	if not self.intfgraph.has_key(i.obj.name):
	    self.intfgraph[i.obj.name] = i

    def reset(self):
	self.loc = 0

    def advance(self):
	self.loc = self.loc + 1

    def outputhere(self):
	loc = self.loc
	self.output('')
	self.loc = loc

    # ensure that type is registed in this AST; 
    # that is, ensure it is in self.intf.types
    def RegisterType(self, type):
	if type.clss == 'ALIAS':
	    if type.type.base.clss == 'PREDEFINED' and type.localName == type.type.base.localName:
		#print 'Not regsitering '+type.localName+' as it is a predefined type'
		return
	for t2 in self.intf.types:
	    if t2.localName == type.localName:
		#print 'Not registering '+t2.name +' as it is already registered'
		return
	#print 'Registering '+type.name
	self.intf.types.append(type)

    def NewInterface(self,name):
	intf = self.be['Interface']()
	intf.name = name
	intf.closuretype = self.be['Type']()
	intf.closuretype.localName = 'clp'
	intf.closuretype.intf = intf
	intf.closuretype.name = name + '_clp'
	intf.closuretype.type = self.be['InterfaceRef']()
	intf.closuretype.type.base = intf
	intf.closuretype.clss = 'IREF'
	intf.closuretype.target = intf.name
	intf.closuretype.setup = 0
	intf.types = [intf.closuretype]
	intf.closuretype.known = 1
	intf.opcount = 0
	intf.exncount = 0
	intf.basictypes = {}
	for key in self.be['builtin'].keys():
	    intf.basictypes[key] = self.NewAlias(self.be['builtin'][key])
	    intf.basictypes[key].intf = intf
	return intf

    def NewType(self):
	t = self.be['Type']()
	t.haveref = 0
	return t

    def NewAlias(self, type, name=''):
	t = self.NewType()
	t.clss = 'ALIAS'
	if len(name)==0:
	    t.localName = type.name
	    t.name = type.name
	else:
	    t.localName = name
	t.predef = type.predef
	t.type = self.be['Alias']()
	t.type.base = type
	t.setup = 1
	t.known = 0
	if self.haveintf:
	    t.intf = self.intf
	return t

    def convertlist(self, testfunc, convfn):
	node = self.words[self.loc]
	(clss,form,lpayload,str) = node.gettuple()
	(clssname,order) = list_group[clss]
	#print 'List func '+`testfunc`
	self.advance()
	if clssname == 'NULL':
	    return []

	if clssname != 'list':
	    print 'Was expecting list, found '+clssname
	    sys.exit(1)

	l = []
	#print 'List length '+`lpayload`
	for i in range(0,lpayload):
	    node = self.words[self.loc]
	    (clss,form,payload,str) = node.gettuple()
	    (clssname, order) = list_group[clss]
	    if testfunc(node) == 0:
                error('Internal ASTgen error; a node of the incorrect type was found within a list')
	    l.append(convfn())
	return l

    def gettype(self):
	node = self.words[self.loc]
	(clss,form,payload,str) = node.gettuple()
	(clssname,order) = list_group[clss]
	if clssname == 'shortcut':
	        self.advance()
		flag = payload
		index = self.getinteger()
		ostr = ''
		flag = payload
		while flag>=0:
		    str = self.words[self.loc].str
		    for ch in str:
			if ord(ch)>0:
			    ostr = ostr + ch
		    self.advance()
		    flag = flag - 4
		if not ostr == self.intf.name:
		    intfast = self.reposit.lookup(ostr)
		    self.RegisterUse(intfast)
		else:
		    intfast = self
	
		return intfast.words[index].getobj()
	else:
	    return self.converttype()
	

    def converttype(self):
	    node = self.words[self.loc]
	    (clss,form,payload,str) = node.gettuple()
	    (clssname,order) = list_group[clss]
	    if clssname == 'shortcut':
		return self.convert()
	    self.advance()

	    if clssname != 'type':
                error('Was expecting a type but got a '+clssname+'. (This is an internal astgen error, caused by an internal reference to a type referring to a non type by mistake)')
	    (kind,lorder) = list_type[form]
	    
	    if kind == 'method':
		oldfieldmode = self.fieldmode
		self.fieldmode = 1
		stack = self.inargs
		self.inargs = 1
		args = self.convertlist(testarg, self.convert)
		self.fieldmode = 0
		results = self.convertlist(testarg, self.convert)
		self.inargs = stack
		for i in results:
		    if i.type.known == 0:
			self.RegisterType(i.type)
		self.fieldmode = oldfieldmode
		foo = self.convert()
		#if `foo` != 'None':
		#    self.intf = foo
		return (args, results)


	    if kind == 'ref':
		refbase = self.gettype()
		#print 'Referencing '+refbase.name+ ' ('+`refbase`+')'
		if refbase.haveref == 1:
		    return refbase.reftype

	    if self.intf.basictypes.has_key(kind):
		t = self.intf.basictypes[kind]
		t.setup = 1
		t.known = 0
		node.registerobj(t)
		return t

	    t = self.NewType()
	    indent = ''
	    t.setup = 1
	    t.known = 0
	    t.clss = 'SCREWED UP '+kind

	    if kind == 'enum':
		t.clss = 'ENUMERATION'
		t.type = self.be['Enumeration']()
		t.type.elems = self.convertlist(testidentifier, self.convert)
	    if kind == 'ref':
		t.clss = 'REF'
		t.type = self.be['Ref']()
		t.type.base = refbase
		# stick away the type so that we can use it again
		t.type.base.haveref = 1
		t.type.base.reftype = t
	    if kind == 'iref':
		#t.clss = 'IREF'
		n = self.convert()
		if n == self.intf.name:
		    bt = self.intf.closuretype
		#print self.intflist
		else:
		    if self.intflist.has_key(n):
			i = self.intflist[n]
		    else:
			i = self.NewInterface(n)
		    bt = i.closuretype
		t.clss = 'ALIAS'
		t.type = self.be['Alias']()
		t.type.base = bt
	    if kind == 'set':
		t.clss = 'SET'
		t.type = self.be['Set']()
		t.type.base = self.convert()
	    if kind == 'array':
		t.clss = 'ARRAY'
		t.type  = self.be['Array']()
		t.type.base = self.convert()
		t.type.size = self.convert()
	    if kind == 'bitarray':
		t.clss = 'BYTEARRAY'
		t.type  = self.be['ByteArray']()
		t.type.base = self.convert()
		t.type.size = self.convert()
	    if kind == 'sequence':
		t.clss = 'SEQUENCE'
		t.type = self.be['Sequence']()
		t.type.base = self.convert()
	    if kind == 'record':
		t.clss = 'RECORD'
		t.type = self.be['Record']()
		oldfieldmode = self.fieldmode
		self.fieldmode = 2
		t.type.mems = self.convertlist(testarg, self.convert)
		self.fieldmode = oldfieldmode
	    if kind == 'choice':
		t.clss = 'CHOICE'
		choice = self.be['Choice']()
		choice.base = self.convert()
		choice.elems = []
		l = self.convert()
		for el in l:
		    choice_elem = self.be['ChoiceElement']()
		    choice_elem.name = el.name
		    choice_elem.type = el.type
		    choice.elems.append(choice_elem)
		t.type = choice
	    if t.clss == 'SCREWED UP':
		print kind + ' not matched'
	    node.registerobj(t)

 	    return t


    def maketypeknown(self,t):
	if t.known == 1:
	    return
	if t.clss == 'ALIAS':
	    if t.type.base.clss == 'PREDEFINED':
		t.known = 1
		return
	    if t.type.base.clss == 'IREF':
		t.known = 1
		t.name = t.type.base.name
		t.localName = t.type.base.localName
		return
	flag = 0
	if t.clss == 'REF':
	    flag = 1
	    if t.type.base.clss == 'ALIAS':
		#print t.type.base.type.base.clss
		if t.type.base.type.base.clss == 'PREDEFINED':
		    flag =0
	if flag:
	    t.localName = t.type.base.localName + '_ptr'
	else:
	    t.localName = 'anon' + `self.index`
	    self.index = self.index + 1
	t.name = self.intf.name + '_' + t.localName
	t.intf = self.intf
	t.known = 1
	#print 'Registering '+t.name
	self.RegisterType(t)

################################################################################
# CONVERT to back end object
################################################################################

    def convert(self):
	node = self.words[self.loc]
	(clss,form,payload,str) = node.gettuple()
	(clssname,order) = list_group[clss]

	if clssname == 'type':
	    # build an anonymous type
	    # this path should only be used when a type is found outside 
	    # a type declaration
	    t = self.converttype()
	    flag = 1
	    if t.clss == 'ALIAS':
		if t.type.base.clss == 'PREDEFINED':
		    flag = 0
	    if flag and t.known == 0:
	    	self.maketypeknown(t)
	    return t


	self.advance()
	if clssname == 'identifier':
	    # reconstruct string
	    ostr = ''
	    flag = payload
	    while flag>=0:
		str = self.words[self.loc].str
		for ch in str:
		    if ord(ch)>0:
			ostr = ostr + ch
		self.advance()
		flag = flag - 4
	    node.registerobj(ostr)
	    return ostr
	if clssname == 'shortcut':
	    flag = payload
	    index = self.getinteger()
	    ostr = ''
	    flag = payload
	    while flag>=0:
		str = self.words[self.loc].str
		for ch in str:
		    if ord(ch)>0:
			ostr = ostr + ch
		self.advance()
		flag = flag - 4
	    #print 'Interface '+ostr+' index '+`index`
	    if not ostr == self.intf.name:
		intfast = self.reposit.lookup(ostr)
		self.RegisterUse(intfast)
	    else:
		intfast = self
		#print 'Self reference from '+`self.loc`
	    oldnode = intfast.words[index]
	    #print 'Shortcut '+`list_group[form]`
	    (formstr,_) = list_group[form]
	    if formstr == 'type':
		if self.inargs:
		    return oldnode.getobj()
		t = self.NewType()
		t.clss = 'ALIAS'
		t.type = self.be['Alias']()
		t.type.base = oldnode.getobj()
		t.intf = t.type.base.intf
		t.name = t.type.base.name
		t.localName = t.type.base.localName
		t.setup = 1
		t.known = 0
		return t
	    return oldnode.getobj()
	if clssname == 'spec':
	    (kind,lorder) = list_spec[form]
	    name = self.convert()
	    #print 'Interface ' + name
	    if self.intflist.has_key(name):
		intf = self.intflist[name]
	    else:
		intf = self.NewInterface(name)
	    self.haveintf = 1
	    self.intf = intf
	    self.reposit.register(self);
	    self.intflist[name] = intf
	    node.registerobj(intf)


	    if kind == 'local_interface':
		intf.local = 1
	    else:
		intf.local = 0
	    #print 'Local is '+`intf.local`+' for '+`intf`

	    intf.frontEnd = 'AST'
	    intf.compTime = time.ctime( time.time() )
	    intf.excs = []
	    intf.ops = []
	    intf.isModule = 0
	    (uid,_,_,_,gcos,_,_) = pwd.getpwuid(os.getuid())
	    intf.userName = uid + ' ('+gcos+')'
	    intf.machine = gethostname()
	    intf.imports = []

	    extends=self.convert()
	    needs=self.convert()
	    if `extends` == 'None': extends=[]

	    if `needs` == 'None': needs=[]
	    intf.extendslist = extends
	    intf.needslist = needs

	    for i in needs:
		self.reposit.require(i)
	    for  i in extends:
		self.reposit.require(i)


	    body = self.convertlist(testspecmember, self.convert)
	    #for thing in body:
		#if thing.isbetype():
		#    intf.types.append(thing)
		#if thing.isbeexcn():
		#    intf.excs.append(thing)
		#if thing.isbeop():
		#    intf.ops.append(thing)

	    for i in extends:
		ast = self.reposit.lookup(i)
		ext = ast.obj
		self.RegisterUse(ast)
		intf.opcount = intf.opcount + ext.opcount

	    #print self.intf.name+' extends ' + `intf.extends`
	    intf.init()

	    return intf
	if clssname == 'list':
	    l = []
	    for i in range(0,payload):
		l.append(self.convert())
	    return l
	if clssname == 'expression':
	    (kind,lorder) = list_expr[form]
	    if kind != 'num_val':
		print kind + ' found; unsupported in astread'

	    for i in lorder:
		if i == 'n_cptr':
		    str = self.getstring()
		    return eval(str)
		elif i == 'n_integer':
		    integer = self.getinteger()
		elif i == 'n_empty':
		    pass
		else:
		    self.output(indent + i)
	    return
	if clssname == 'sco':
	    intf = self.convert()
	    local = self.convert()
	    if `intf` == 'None':
		intf = self.intf.name
	    return (intf, local)
	if clssname == 'decl':
	    if form > len(list_decl):
		print 'Form index ',`form`,' is off the end of ',`list_decl`
	    (kind,lorder) = list_decl[form]
	    str = self.convert()
	    if kind == 'type' or kind == 'procedure' or kind == 'procedure_nr' or kind =='announcement':
		type = self.converttype()
	    else:
		type = self.convert()

	    indent = ''
	    if kind == 'type':
		if type.clss == 'ALIAS':
		    if type.type.base.clss == 'PREDEFINED':
			# got built in class alias; realias
			type = self.NewAlias(type.type.base, str)
		if type.clss == 'IREF':
		    print 'IREF '+type.name
		flag = type.known
		type.known = 1
		if type.setup:
		    type.localName = str
		    type.intf = self.intf
		    type.name = self.intf.name + '_' + str
		self.RegisterType(type)
		node.registerobj(type)
		return type
	    if kind == 'exception':
		e = self.be['Exception']()
		e.name = str
		e.ifName = self.intf.name
		e.intf = self.intf
		e.number = self.intf.exncount;
		self.intf.exncount = self.intf.exncount + 1
		old = self.fieldmode
		self.fieldmode = 1
		e.pars = self.convertlist(testarg, self.convert)
		self.fieldmode = old
		self.reposit.exnlist[(self.intf.name, str)] = e
		#print 'Exception arguments are ' + `e.pars`

		node.registerobj(e)
		self.intf.excs.append(e)
		return e
	    if kind == 'procedure' or kind == 'procedure_nr' or kind =='announcement':
		o = self.be['Operation']()
		o.name = str
		o.ifName = self.intf.name
		o.intf = self.intf
		if kind == 'announcement': o.type = 'ANNOUNCEMENT'
		else: o.type = 'PROC'
		(o.pars, o.results) = type
		# make sure that the parameters are valid types
		tlist = o.pars + o.results
		
		for arg in tlist:
		    t = arg.type
		    if t.known == 0:
			self.maketypeknown(t)

		
		#args = self.convert()
		l = []

		if kind != 'announcement':
		    scolist = self.convertlist(testraises, self.convert)
		    for exn in scolist:
			e = self.reposit.exnlist[(exn)]
			l.append(e)
		o.raises = l
		o.number = self.intf.opcount
		self.intf.opcount = self.intf.opcount + 1
		if kind == 'procedure':
		    o.returns = 1
		else:
		    o.returns = 0
		node.registerobj(o)
		self.intf.ops.append(o)
		return o
	    
	    if kind == 'var' or kind == 'invar' or kind == 'outvar' or kind == 'inoutvar':
		#print 'Field mode '+["Out parameter","in parameter","argument"][self.fieldmode]
		if self.fieldmode == 1:
		    arg = self.be['Parameter']()
		    if kind == 'var' or kind == 'invar':
			arg.mode = 'IN'
		    if kind == 'outvar':
			arg.mode = 'OUT'
		    if kind == 'inoutvar':
			arg.mode = 'INOUT'
		elif self.fieldmode == 0:
		    arg = self.be['Result']()
		    arg.mode = 'RESULT'
		else: # self.fieldmode == 2:
		    arg = self.be['RecordMember']()
		arg.name = str
		arg.type = type
		arg.isLarge = arg.type.isLarge()
		return arg
	    if kind == 'spec':
		self.output( indent + kind + ' spec ')
		return
	    return

    def getinteger(self):
	v = self.words[self.loc].getinteger()
	self.advance()
	return v

    def getstring(self):
	# reconstruct string
	ostr = ''
	while 1:
	    str = self.words[self.loc].str
	    self.advance()
	    for ch in str:
		if ord(ch)>0:
		    ostr = ostr + ch
		else:
		    flag = 0
		    return ostr

################################################################################
# OUTPUT
################################################################################

    def output(self,indent):
	node = self.words[self.loc]
	(clss,form,payload,str) = node.gettuple()

	if clss>len(list_group):
	    print 'Erk!: '+`self.words[self.loc].gettuple()`
	    self.advance()
	    return
	(clssname,order) = list_group[clss]
	#print `self.loc`+':'+`self.words[self.loc].gettuple()`
	if clssname == 'identifier':
	    print indent + '[' + self.convert() + ']'
	    return
	self.advance()
	if clssname == 'list':
	    #print indent + 'List of ' + `payload`
	    for i in range(0,payload):
		self.output( indent + `i`+':')
	    return
	if clssname == 'shortcut':
	    st = self.loc
	    self.loc = payload
	    self.output(indent + '->')
	    self.loc = st
	    return 
	if clssname == 'spec':
	    (kind,lorder) = list_spec[form]
	    name = self.convert()
	    print indent + name + ' is a ' + kind
	    self.output( indent + name + ' extends ')
	    self.output( indent + name + ' needs ')
	    self.output( indent + name + ' body ')
	    return
	if clssname == 'decl':
	    if form > len(list_decl):
		print 'Form index ',`form`,' is off the end of ',`list_decl`
	    (kind,lorder) = list_decl[form]
	    str = self.convert()
	    #print 'At end of string loc is '+`self.loc`
	    self.output( indent + str+ ' ' +kind )
	    #print 'At end of type loc is '+`self.loc`
	    if kind == 'announcement':
		self.output( indent + kind + ' list ')
		return
	    if kind == 'exception':
		self.output( indent + kind + ' list ')
		return
	    if kind == 'procedure' or kind == 'procedure_nr':
		#print 'At start of raises loc is '+`self.loc`
		self.output( indent + str +' raises ')
		#self.output( indent + 'op ' + str + ' counter ')
		#print 'At end of raises loc is '+`self.loc`
		#op = self.getinteger()
		#op = self.intf.opcount
		#self.intf.opcount = self.intf.opcount + 1
		#print indent + str + ' op ' + `op`
		print 'At ',str, self.loc
		#print 'At end of procedure, loc is '+`self.loc`
		#str = ast.getstring()
		#print indent + ' intf ' + str
		return
	    if kind == 'spec':
		self.output( indent + kind + ' spec ')
		return
	    return
	if clssname == 'type':
	    (kind,lorder) = list_type[form]
	    print indent + kind
	    if kind == 'enum':
		self.output( indent + 'pos ')
		return
	    if kind == 'ref':
		self.output( indent + ' REF ')
		return
	    if kind == 'iref':
		self.output( indent)
		return
	    if kind == 'set':
		self.output( indent + ' of ')
		return
	    if kind == 'method':
		self.output( indent + ' args ')
		self.output( indent + ' rets ')
		self.output( indent + ' intf ')
		return
	    if kind == 'array':
		self.output( indent + ' type ')
		self.output( indent + ' size ')
		return
	    if kind == 'bitarray':
		self.loc == self.output( indent + ' size ')
		return
	    if kind == 'sequence':
		self.output( indent + ' of ')
		return
	    if kind == 'record':
		self.output( indent + ' field ')
		return
	    if kind == 'choice':
		self.output( indent + ' param ')
		self.output( indent + ' option ')
		return
	    return
	if clssname == 'NULL':
	    print indent + 'nothing'
	    return
	if clssname == 'sco':
	    self.output(indent+ ' scope ')
	    self.output(indent+ ' name ')
	    return
	if clssname == 'expression':
	    (kind,lorder) = list_expr[form]
	    print 'Expression kind ' +kind
	    for i in lorder:
		if i == 'n_cptr':
		    str = self.getstring()
		    print indent + '[' + `str` + ']'
		elif i == 'n_integer':
		    integer = self.getinteger()
		    print indent + `integer`
		elif i == 'n_empty':
		    pass
		else:
		    self.output(indent + i)
	    return
	print 'Unmatched '+clssname


