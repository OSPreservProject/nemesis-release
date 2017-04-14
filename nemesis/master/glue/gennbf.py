#! /usr/bin/env python
#
# nemmaster gennbf
#
# Copyright 1998 Dickon Reed
#
# Build an .nbf file
#

import blueprint, sys, nemclasses, configutils, gendetails, buildutils
import tempfile, treeinfo, string, os, stat, posix, sys, time

def Error(x):
    sys.stderr.write(x+'\n')
    sys.exit(1)

def Warning(x):
    sys.stderr.write(x+'\n')

def Verbosity(x):
    sys.stderr.write(x+'\n')
def params_to_str(program):
    (qos, env) = program.get_program_info()
    extraflagvalue = ''
    kernelflagvalue = ''
    if qos[nemclasses.extra]: extraflagvalue = 'x'
    if env[nemclasses.kernel]: kernelflagvalue = 'k'
    str = ' ' + qos[nemclasses.period] + ' ' + qos[nemclasses.slice] + ' ' + qos[nemclasses.latency] + ' ' + kernelflagvalue + extraflagvalue 
    if env.has_key(nemclasses.autostart):
	if env[nemclasses.autostart]:
	    str = str + 'b'
    str = str + ' ' + env[nemclasses.sysheap] + ' ' + env[nemclasses.defstack] + ' ' + env[nemclasses.heap] + ' ' + `env[nemclasses.contexts]` + ' ' + `env[nemclasses.endpoints]` + ' ' + `env[nemclasses.frames]`
    return str

deps = blueprint.deps

if len(sys.argv) !=2 :
    sys.stderr.write("""
Usage: gennbf.py nbffilename

Generate a .nbf file in nbffilename
""")
    sys.exit(1)

# get a list of happy (ie configured active) items
happyitems = configutils.getactivesubset(blueprint.items)
namespaces = {}

nmcommand = 'nm'
nmfilt = ''
if treeinfo.platform_name in ['linux_for_arm']:
    nmcommand = "arm-unknown-coff-nm"
    nmfilt    = "| sed -e 's/_//'"
if treeinfo.target_name in ['eb164']:
    nmcommand = nmcommand + ' -Bn'

print 'There are ',len(happyitems),' active items'

haventsc = 0
haveprimal = 0
nbf_modules = {}
nbf_programs = {}
blobdirlist = {}

for item in happyitems:
    cl = item.get_class()
    if cl == 'ntsc':
	ntsc = item
	if haventsc:
	    Warning('Error; more than one NTSC remains! Dropping one')
	haventsc = 1

    if cl == 'primal':
	primal = item
	if haveprimal:
	    Warning('Error; more than one Primal remains! Dropping one')
	haveprimal = 1
	
    if cl == 'module':
	if item.place_in_kernel():
            if nbf_modules.has_key(item.name):
                Warning('Warning; module named '+item.name+' occurs more than once in active items set')
	    nbf_modules[item.name]=item

    if cl == 'program':
	if item.place_in_kernel():
            if nbf_modules.has_key(item.name):
                Warning('Warning; program named '+item.name+' occurs more than once in active items set')
	    nbf_programs[item.name]=item

    if cl == 'blobdir':
        if blobdirlist.has_key(item.name):
            Warning('Warning; blob directory '+item.name+' occurs more than once in active items set')
        blobdirlist[item.name] = item


print 'there are ',len(nbf_modules.keys()),'modules'
print 'there are ',len(nbf_programs.keys()),'programs'

if not haventsc:
    Error('Error; no NTSC remains')
if not haveprimal:
    Error('Error; no Primal remains')

# build the meta namespace dictionary

namespaces = {}
namespaces['nstd'] = {}
namespaces['nprimal'] = {} # so that it is there for blobs
for item in nbf_modules.values():
    # XXX cunningness here
    Verbosity('Module '+item.name+' path '+item.get_path()+' bin '+item.get_binary_object_name())
    symlistfilename = '.symbols.'+item.get_name()
    objname = treeinfo.build_tree_dir+'/'+item.get_path()+'/'+item.get_binary_object_name()
    regen = 1 # yes by default
    try:
	objstatbuf = os.stat(objname)
    except:
	Warning('Could not stat symbol cache file '+objname+'; regenerating it')
	objstatbuf = None
    # do we need to generate the local copy of the list of exported symbols in the file?

    try:
	symliststatbuf = os.stat(symlistfilename)
	if objstatbuf and objstatbuf[stat.ST_MTIME] < symliststatbuf[stat.ST_MTIME]:
            Verbosity('(Cache of %s newer than bin of %s)' % (time.ctime(symliststatbuf[stat.ST_MTIME]), time.ctime(objstatbuf[stat.ST_MTIME])))
	    regen = 0 # no if it exists and is in date
    except posix.error:
	pass # the stat must have failed, but I do not care

    deps.append(objname+nmfilt)

    if regen:
        Verbosity('Scanning for symbols')
	try:
	    buildutils.system(nmcommand+' '+objname+nmfilt +' | grep -E "_export$|_primal$" > '+symlistfilename, verbose=0)
	except:
	    sys.stderr.write('Could not obtain symbols from '+objname+'; perhaps it did not exist because it did not build properly?\n')
    exportsstrs = []
    
    otemp = open(symlistfilename)
    exportsstrs = otemp.readlines()
    otemp.close()
    exports = []
    for exportsstr in exportsstrs:
	fields = string.split(exportsstr[:-1])
	#print item.get_name(), fields[2]
	c_id_name = fields[2]
	splitlist = string.split(c_id_name, '_')
	symname = string.join(splitlist[:-1], '_')
	prefix = ''
	namespace = 'nprimal'
	if splitlist[-1] == 'export':
	    prefix = 'modules>'
	if splitlist[-1] == 'primal':
	    prefix = 'primal>'
        duplicated = 0
	for (namespacep, entryp, symbolp) in exports:
            if entryp == prefix+symname and namespacep == namespace:
                Warning('Warning; Name %s of namespace %s occurs both to C name %s and %s' % (entryp, namespace, c_id_name, symbolp))
                duplicated = 1
                
        if not duplicated:
            exports.append( (namespace, prefix+symname, c_id_name) )

    Verbosity('Symbols %s' % ( `map( lambda (ns, e, _) : ns+'>'+e, exports)`))
    for (namespace, entry, symbol) in exports:
	if not namespaces.has_key(namespace):
	    namespaces[namespace] = {}
	if namespaces[namespace].has_key(entry):
	    Warning('Warning; Name %s occurs both in item %s and item %s' % (entry, item.name, namespaces[namespace][entry][0].name))
	namespaces[namespace][entry] = (item, symbol)
    Verbosity('')
for item in blobdirlist.values():
    for (filename, handlename, namespacename) in item.get_bloblist():
	namespaces['nprimal']['blob>'+namespacename] = (item, handlename)

    
o = open(sys.argv[1], 'w')
if len(sys.argv)==3:
    depfilename = sys.argv[2]
else:
    depfilename = sys.argv[1]+'.d'
depo = open(depfilename, 'w')
depo.write(sys.argv[1] + ' : ' + string.join(deps, ' '))
o.write('# This file is autogenerated by gennbf.py; do not edit\n')
o.write('# '+gendetails.gendetails()+'\n')
# the string to add on to all paths
relpath = '../../../'

# the stuff we have to have
o.write('ntsc '+relpath+ntsc.get_path()+'/'+ntsc.get_binary_object_name()+'\n')

# write out lines to alias all the items

# write out the modules, putting ones in reqstartorder at the start
#reqstatorder = ['NemesisVPix86', 'libc', 'TrivWr']
reqstatorder = []

outsofar = {}
outputorder = []
for reqpos in reqstatorder:
    for module in nbf_modules:
	if module.name == reqpos and not(outsofar.has_key(module.name)):
	    outputorder.append(module)
	    outsofar[module.name] = 1

for module in nbf_modules.values():
    if not(outsofar.has_key(module.name)):
	outputorder.append(module)
        outsofar[module.name] = 1

for module in outputorder:
    o.write('module '+module.name+' = '+relpath+module.get_path()+ '/' + module.get_binary_object_name() + '\n')

# write out the blobs

for item in blobdirlist.values():
    path = item.get_path()
    for (filename, handlename, namespacename) in item.get_bloblist():
	o.write('blob '+handlename+' = ' + relpath+path+'/'+filename+'\n')

# write out the namespaces

namespacelistsorted = namespaces.keys()
namespacelistsorted.sort()
for namespace in namespacelistsorted:
    o.write('namespace '+namespace+' = {\n')
    entryslistsorted = namespaces[namespace].keys()
    entryslistsorted.sort()
    entryslistsorted.reverse()
    for entry in entryslistsorted:
	(item, symbol) = namespaces[namespace][entry]
	if item.get_class() == 'blobdir':
	    o.write('\t"'+entry+'" = blob ' + symbol + '\n')
	else:
	    o.write('\t"'+entry+'" = ' + item.name + '$' + symbol +'\n')
    o.write('}\n')


# write out the programs
o.write('\n\n# programs section\n\n')

o.write('primal '+relpath+primal.get_path()+'/'+primal.get_binary_object_name()+' nprimal '+params_to_str(primal)+'\n')

programnames = nbf_programs.keys()
programnames.sort()
for programname in programnames:
    program = nbf_programs[programname]
    o.write('program '+relpath+program.get_path()+'/'+program.get_binary_object_name() + ' '+ program.get_name() + ' nstd '+ params_to_str(program) + '\n')

o.write('end\n')
o.close()
depo.write('\n')
depo.close()


