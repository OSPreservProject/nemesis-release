#
# nemmaster buildutils
#
# Copyright 1998 Dickon Reed
#
# The set of classes that build item may belong to
#

import types, string, sys

# the set of classes that are available in choices files
# Remember to add any new classes to this list

itemclasseslist = [
    'NTSC', 'Primal', 'Program', 'PosixProgram', 'Module', 'AutoModule', 'Document',
    'PureConfiguration', 'Directory', 'InterfaceCollection', 'StaticLibrary',
    'BlobDirectory']


# these are just here to avoid typos in options dictionaries

path = 'path'
requires = 'requires'
description = 'description'
disableconfig = 'disableconfig'
requires = 'requires'
qos = 'qos'
env = 'env'
exports = 'exports'
helptext = 'helptext'
system_interfaces = 'system_interfaces'
public_interfaces = 'public_interfaces'

period = 'period'
slice = 'slice'
latency = 'latency'
extra = 'extra'
kernel = 'kernel'
sysheap = 'sysheap'
defstack = 'defstack'
heap = 'heap'
contexts = 'contexts'
endpoints = 'endpoints'
frames = 'frames'
binobject = 'binobject'
doctext = 'doctext'
autostart = 'autostart'
bloblist = 'bloblist'

MissingOption = 'MissingOption'
InvalidOption = 'InvalidOption'
RequirementsTooComplicated = 'RequirementsTooComplicated'

autodeduce = '***autodeduce***'
growrecursive = 'growrecursive'
makeflags = 'makefileflags'
configtypemap = { 
    'bool' : 'choice{{0,NO,n},{1,YES,y}}',
    '*bool' : '*choice{{0,NO,n},{1,YES,y}}',
    'quad' : 'choice{{0,NONE,0},{1,SUPPORT,1},{1,BUILD,2},{1,NBF,3}}',
    '*quad' : '*choice{{0,NONE,0},{1,SUPPORT,1},{1,BUILD,2},{1,NBF,3}}',

		  }


keylist = ['period', 'slice', 'latency', 'extra', 'kernel', 'sysheap', 'defstack', 'heap', 
	   'contexts', 'endpoints', 'frames',
	   'binobject', 'doctext', 'autostart', 'bloblist', 
	   'associated_cpp_name', 'binobject', 'description', 'env', 'helptext', 'growrecursive',
	   'makefileflags', 'custom', 'libio', 'install', 'localrules', 'libws', 'libsocket', 'donotinstall',
	   'libsunrpc', 'libmesa', 'path', 'requires', 'section', 'system_interfaces', 'type', 'value',
	   'qos', 'itemname', 'fullname', 'interfacelist', 'fieldname', 'tweakability',
           'package', 'contents', 'makevars']

invalidkey = 'invalidkey'
invaliddate = 'invaliddata'
#  The parent BuildItem class

class BuildItem:
    def __init__(self, name, options):
	self.options = options
	self.name = name
	# this get used by geninfrastructure
	self.subdirs = {}
	if not self.options.has_key('requires'):
	    self.options['requires'] = []
	self.originals = {}
	if options.has_key('path'):
	    # set the location overrides to null values
	    # a location of None means "out of the source tree"
	    # a string location means "from that directory"
	    self.location = None
	    self.locatefiles = {}
	for key in self.options.keys():
	    if not key in keylist:
		sys.stderr.write('ERROR: key '+key+' in item '+self.name+' is not valid')
                sys.stderr.exit(1)
	if not options.has_key('associated_cpp_name'):
	    str = string.upper(name) + '_ANON'
	    newstr = ''
	    for char in str:
		if char in ' /': newstr = newstr + '_'
		else: newstr = newstr + char
	    options['associated_cpp_name'] = newstr
        # key sanity checking
        if options.has_key('qos'):
            for req in ['extra', 'latency', 'period', 'slice']:
                if not options['qos'].has_key(req):
                    sys.stderr.write('ERROR: Item '+self.name+' contains qos field that is missing a "'+req+'" entry\n')
                    sys.exit(1)
    def requireoptions(self, *requiredoptions):
	for option in requiredoptions:
	    if not self.options.has_key(option):
		raise MissingOption, (self,option)
    def validoptions(self, *validoptions):
	for option in self.options.keys():
	    if not(option in validoptions):
		raise InvalidOption, (self, option)
    def get_class(self):
	return
    def get_exports(self):
	if self.options.has_key('exports'):
	    return self.options['exports']
	return []
    def get_path(self):
	return self.options['path']
    def get_name(self):
	return self.name
    def get_binary_object_name(self):
	if self.options.has_key(binobject):
	    return self.options['binobject']
	else:
	    return self.name
    def get_program_info(self):
	return
    def get_help_text(self):
	if self.options.has_key(helptext):
	    return self.options[helptext]
	else:
	    return 'no help available'
    def get_path(self):
	if self.options.has_key(path):
	    return self.options[path]
	else:
	    return 

    def __repr__(self):
	str = self.__class__.__name__+"('"+self.name +"', {\n"
	optcopy = self.options
	keys = optcopy.keys()	
	keys.sort()
	for option in keys:
	    if option == 'exports': continue
	    if type(optcopy[option]) ==  type( {} ):
		value = '{'
		sortedkeys = optcopy[option].keys()
		sortedkeys.sort()
		for key in sortedkeys:
		    value = value + `key` + ':' + `optcopy[option][key]` + ','
		value = value + '}'
	    else:
		value = `optcopy[option]`
	    str = str + '  '+option+' : '+value+',\n'
        return str + '})'
    def is_grow_recursive(self):
	if self.options.has_key(growrecursive):
	    return self.options[growrecursive]


	else:
	    return 1
    def get_system_interfaces(self):
	if self.options.has_key(system_interfaces):
	    return self.options[system_interfaces]
	else:
	    return []
    def get_public_interfaces(self):
	if self.options.has_key(public_interfaces):
	    return self.options[public_interfaces]
	else:
	    return []
    def get_make_flag(self, flagname):
	if self.options.has_key(makeflags):
	    if type(self.options[makeflags]) != type({}):
		print 'Makeflags of type '+`type(self.options[makeflags])` + ' in '+ self.get_name()
		raise MakeflagsOfWrongType
	    if self.options[makeflags].has_key(flagname):
		return self.options[makeflags][flagname]
	    else:
		return
	else:
	    # if no makeflags key, then everything defaults to off
	    return

    def isactive(self):
	if self.options['type'] in ['bool', 'quad']:
	    return self.options['value']
	return self.options['type'][self.options['value']][0]

# these get used by geninfrastructure
    def get_subdirs(self):
	return self.subdirs
    def add_subdir(self, dirname, item):
	self.subdirs[dirname] = item
    def place_in_kernel(self):
	if self.options['type'] != 'quad':
	    print 'Item ',self,' should be of type quad, as it may go in the kernel'
	    sys.exit(1)
	if self.options['value'] == 3:
	    return 1
	else:
	    return 0

	print 'No configuration for ',self.get_name()
	return 1
    # XXX; these are dangerous, and break the abstraction a touch
    def __getitem__(self, name):
	return self.options[name]
    def __setitem__(self,name, value):
	self.options[name] = value

class NTSC(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self, name, options)
	self.requireoptions(path)
    def get_class(self):
	return 'ntsc'

class Primal(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self, name, options)
	self.requireoptions(path, qos, env)
    def get_class(self):
	return 'primal'
    def get_program_info(self):
	return (self.options[qos], self.options[env])


class Program(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self, name, options)
	self.requireoptions(path, qos, env)
    def get_program_info(self):
	return (self.options[qos], self.options[env])
    def get_class(self):
	return 'program'

class PosixProgram(Program):
    def get_class(self):
	return 'posixprogram'

class Module(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self, name, options)
	self.requireoptions(path, exports)
    def get_class(self):
	return 'module'

class AutoModule(Module):
    def __init__(self, name, options):
	BuildItem.__init__(self, name, options)
	self.requireoptions(path)
	# do the nm thing here
	# XXX: in the mean time, fake it
	self.options['exports'] = [('nprimal', 'modules>'+name, name+'_export')]

class Document(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self,name,options)
	self.requireoptions(path)
    def get_class(self):
	return 'doc'

class PureConfiguration(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self,name,options)
    def get_class(self):
	return 'config'

class Directory(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self,name,options)
	self.requireoptions(path)
    def get_class(self):
	return 'dir'

class InterfaceCollection(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self,name,options)
	self.requireoptions(path,system_interfaces)
    def get_class(self):
	return 'interfaces'
	
class StaticLibrary(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self,name,options)
	self.requireoptions(path)
    def get_class(self):
	return 'staticlib'

class BlobDirectory(BuildItem):
    def __init__(self, name, options):
	BuildItem.__init__(self,name,options)
	self.requireoptions(path, bloblist)
    def get_class(self):
	return 'blobdir'
    def get_bloblist(self):
	return self.options[bloblist]









