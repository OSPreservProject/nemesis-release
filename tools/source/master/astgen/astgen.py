#! /usr/bin/env python
# Copyright 1996, 1997, 1998, 1999 Dickon Reed

import sys, os, string, astcompile

"""
astgen is a harness for handling the compilation of MIDDL interfaces. It
uses the MIDDL compiler CL to compile interfaces or takes the .ast marshalled
AST version of interfaces from disk. The compiler has multiple backends known
as generators which are loaded dynamically according to command line options.
Several useful generators are supplied with astgen for common Nemesis related
requirements, but more can be added dynamically using the backend search
path options.

Generators contain one class per class in the file BE.py, and Go function
for initialisation. At run time a class hierachy is constructed that
uses multiple inheritance to combine all the classes in BE and, inherited
from that, the classes in each backend. This subclass is then used to
instantiate the AST objects of the interfaces being dealt with.
"""

usage= """Usage: astgen [options] (.ast files or .if files)+

Options are:
   deps    Output .d files for each file generated
   tcmap   Output name to type code map
   +path   Add path to the search path for ast files
   Ppath   Add path to the search path for backends
   deps    For every target file requested, cause a .d file to be
           emitted as well
   idc[stubs,marshalling]
           Generate IDC stubs and marshalling
   ih      Generate .ih files
   def     Generate .def.c file
   c       Generate C template
   
   (other backends may be specified as options; they must be genFOO.py
    files on the backends search path).
Example:
    astgen ih Foo.if Bar.ast

.if files are automatically compiled using cl -A if they can be found in
the current directory.
"""


def error(mesg):
    sys.stderr.write('ERROR: '+mesg+'\n')
    sys.exit(1)
    
def process(astname,backend,dodeps, reposit, astpath):
    args = sys.argv[1:]
    namelist = string.split(astname, '.')
    ast = reposit.lookup(namelist[0])
    #f = OpenAst(astname,astpath)
    #ast = Ast(f.read(), backend, reposit)
    #f.close()
    #if len(args)==0:
    #	ast.output('')
    #	ast.reset()
   
    fixup(ast, reposit, backend)
    intfs = []
    for key in ast.intfgraph.keys():
	if key == namelist[0]:
	    intfs = intfs + [ast.intfgraph[key]]
	else:
	    intfs = [ast.intfgraph[key]] + intfs
    fingerprint = ast.fp
    wanted = [ast]
    deps = []
    for (g,kinds,arg,list) in backend['golist']:
	for k in kinds:
	    deps = deps + g(k, intfs, fingerprint)
    sys.stderr.write('Generated '+string.join(deps, ' ')+'\n')
    if not dodeps:
	return
    for name in deps + [ast.obj.name+'.if']:
	f = open (name+'.d' , 'w')
	for dep in deps:
	    f.write(dep + ' ')
	f.write(':')
	for i in intfs:
	    f.write(i.obj.name + '.ast' + ' ')
	f.write('\n')
	f.close()

files = []
args = []
dodeps = 0
domap = 0
astpath = ['.']

for argument in sys.argv[1:]:
    if argument == 'deps':
	dodeps = 1
    elif argument == 'tcmap':
	domap =1 
    elif argument[0] == 'P' and string.find(argument, '.') == -1:
	sys.path = [argument[1:]] + sys.path
    elif argument[0] == '+':
	astpath.append(argument[1:])
    elif string.find(argument, '.') == -1:
	args.append(argument)
    else:
	files.append(argument)

iffiles = []
astfiles = []

if len(files) == 0:
    error(usage)

for file in files:
    if file[-4:] == '.ast': astfiles.append(file)
    elif file[-3:] == '.if': iffiles.append(file)
    else: error('Invalid filename '+file+' on command line.\n'+usage)
               

astcompile.compile(iffiles)
astfiles = astfiles + map ( lambda x : x[:-3]+'.ast', iffiles)

from asthandle import *
backend = setupbe(args)

reposit = AstReposit(backend, astpath)

for file in astfiles: process(file, backend,dodeps, reposit, astpath)
    
f = open('typecodes.map','w')
m = reposit.mapping
f.write(`len(m)` + '\n')
for key in m.keys():
    try:
	v = m[key]
	fp = v.fp

	f.write(key+','+`fp`)
    except:
	print '\nFingerprint unknown for interface '+key+';\n\tleaving interface out of typecodes.map\n\tTry deleting '+key+'.ast and rebuilding\n'
    f.write('\n')
f.close()



