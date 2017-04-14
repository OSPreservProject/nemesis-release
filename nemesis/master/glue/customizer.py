# Nemesis tree blueprint customizer
#
# Copyright 1998 University of Cambridge
#
# The customizer class, used by blueprint to generate
# the customized blueprint.

import sys, string, qosutil, posixpath, regex, os, buildutils
from nemclasses import *

archlist = ['ix86', 'alpha', 'arm', 'mips']
platforms = ['osf1', 'hpux_for_arm', 'linux_for_arm', 'ix86_linux']
targets = ['axp3000', 'eb164', 'eb64', 'fpc3', 'intel_smp', 'intel', 'riscpc', 'shark', 'srcit']

Problem = 'Problem',

def error(str):
    raise Problem, str

def warning(str):
    sys.stderr.write('Warning; '+str+'\n')

class Customizer:
    def __init__(self, items, donottouch=0):
	self.items = items
	self.db = {}
	self.donottouch = donottouch
	self.fatal = 1
	self.aliases = {}
        self.items = items
	for key in keylist:
	    self.aliases[key] = key
	for key in itemclasseslist:
	    self.aliases[key] = eval(key)
	self.aliases['self'] = self

	self.extrasearchpath = []
	self.deps = []
        self.arch_name = None
        self.platform_name = None
        self.target_name = None
# this initialisation function is used when customizing the blueprint
# to a particular machine (eg by blueprint.py)

    def init(self, arch_name, platform_name, target_name):
        #sys.stderr.write('Initialising blueprint database\n')
        self.db = {}
	for item in self.items:
	    self.db[item.name] = item
        self.arch_name = arch_name
        self.platform_name = platform_name
        self.target_name = target_name
        todo = []
	for arch in ['ix86', 'alpha', 'arm', 'mips']:
            todo.append(arch+'_arch', arch_name == arch)
	for platform in platforms:
            todo.append(platform+'_plat', platform_name == platform)
	for target in targets:
            todo.append(target+'_target', target_name == target)
	todo.append('isa',
                    target_name in ['eb164', 'srcit', 'shark', 'intel', 'intel_smp'])
        todo.append('pci', target_name in ['eb164', 'intel', 'intel_smp'])
        todo.append('tc_bus', target_name in ['axp3000'])
        todo.append('ns16550_serial_device', target_name in ['eb164', 'eb64', 'shark', 'intel', 'intel_smp'])
        todo.append('vga_text_mode', target_name in ['intel', 'intel_smp'])
        todo.append('timer_rdtsc', target_name in ['intel', 'intel_smp'])
        todo.append('fp_support',target_name in ['intel', 'intel_smp']) 

        for (name, truthvalue) in todo:
            if self.db.has_key(name):
                if truthvalue:
                    self.set(name, 1, 0, 0)
                else:
                    self.destroy(name)


    def newitem(self, item):
        for itemp in self.items:
            if itemp.name == item.name:
                sys.stderr.write('Choices file duplicates item '+item.name+' that is already defined in blueprint\n')
	item.originals['new'] = 1
	self.items.append(item)
	self.db[item.name] = item

    def setvalue(self, item, value, verbose = 1):
	item.originals['value'] = item.options['value']
	if item.options['type'] in ['quad', 'bool'] and value > 0 and item.options.has_key('requires'):
	    for require in item.options['requires']:
		if not self.checkrequirement(require, item.name):
		    warning('Worryingly tring to set '+item.name+' to on '+`value`+' even though requirement '+`require`+' is not available yet. Perhaps later down the choices file I may discover it is available, and then everything will be okay. But if not, this set commmand will ultimately be futile.')
	if item.options['value'] == value:
	    if verbose:
		#warning('customizer has been asked to change the value of '+item.name+' to what it was already ('+`value`+')')
		pass
	else:
	    item.options['value'] = value

######################################################################
# operations on the customizer

    def set(self, itemname, value, verbose = 1, makeconsistent = 0):
	item = self.getitem(itemname)
	if not item.options.has_key('type'):
	    error('item '+itemname+' cannot be set because it has no type field')
	t = item.options['type']
	if type(value) == type('string'):
	    value = string.upper(value)
	if t == 'bool':
	    if value in ['NO','N','FALSE','F']: value = 0
	    if value in ['YES','Y','TRUE','T']: value = 1
	    if value in [0,1]:
		self.setvalue(item, value, verbose)
	    else:
		error('value '+`value`+' is out of range for a boolean option (value should be one of 0,1)')
	elif t == 'quad':
	    if value in ['NONE','N']: value = 0
	    if value in ['SUPPORT','S']: value = 1
	    if value in ['BUILD','B']: value = 2
	    if value in ['NBF','Y']: value = 3
	    if value in [0,1,2,3]:
		self.setvalue(item, value, verbose)
	    else:
		error('value '+`value`+' is out of range for a quad option (value should be one of 0, 1, 2 or 3)')
	elif type(t) == type([]):
	    if type(value) == type(''):
		count = 0
		hit = 0
		for (exists,name,potvalue) in t:
		    if value == potvalue:
			value = count
			hit = 1
		    count = count + 1
		if not hit:
		    error('value '+`value`+' is not valid (type is '+`t`+')')
		else:
		    self.setvalue(item, value, verbose)
	    elif type(value) == type(1):
		if not(value < len(t) and value>=0):
		    error('value '+`value`+' is out of range (should be in range [0,'+`len(t)`+']')
		else:
		    self.setvalue(item, value, verbose)
	    else:
		error('value '+`value`+' is invalid for a choice type')
	else:
	    error('item '+`itemname`+' has unknown type '+`t`)
	if makeconsistent: self.makeconsistent()

    def add_program(self, fullname):
	l = string.split(fullname, '/')
	if len(l)<2:
	    error('Program pathname to short')
	self.newitem(PosixProgram(l[-1], {
	    'description' : 'Nemesis application '+l[-1],
	    'path' : string.join(l[:-1], '/'),
	    'value': 2,
	    'type': 'quad',
	    'qos': qosutil.cpufraction(0, '20ms'),
	    'env': {'kernel':0, 'sysheap' : '64k', 'defstack' : '16k', 
		    'heap':'256k', 'contexts' : 32, 'endpoints' : 128,
		    'frames' : 96 }
	    }))

    def add_module(self, fullname):
	l = string.split(fullname, '/')
	if len(l)<2:
	    error('Module pathname to short')
	self.newitem(AutoModule(l[-1], {
	    'description' : 'Nemesis application '+l[-1],
	    'path' : string.join(l[:-1], '/'),
	    'value': 3,
	    'type': 'quad'
	    }))

    def add_interfaces(self, name, list):
	item = self.getitem(name)
	if not item.options.has_key('system_interfaces'):
	    item.options['system_interfaces'] = []
	set = {}
	for intf in item.options['system_interfaces']+list: set[intf]  = 1
	if not item.options.has_key('system_interfaces'): item.options['system_interfaces'] = None
	item.originals['system_interfaces'] = item.options['system_interfaces']
	item.options['system_interfaces'] = set.keys()

    def del_interfaces(self, name, list):
	item = self.getitem(name)
	if not item.options.has_key('system_interfaces'):
	    error('item '+name+' does not have any interfaces to remove')
	item.originals['system_interfaces'] = item.options['system_interfaces'] # XXX; breaks originals logic if add and del on same option
	set = {}
	for intf in item.options['system_interfaces']: set[intf]  = 1
	for intf in list:
	    if not set.has_key(intf):
		error('item '+name+' does not have interface '+intf+'; cannot remove it')
	    del set[intf]
	item.options['system_interfaces'] = set.keys()

    def tweak(self, name, optionname, value):
	item = self.getitem(name)
	if not optionname in keylist:
	    sys.stderr.write('Warning: unknown key '+optionname+' while tweaking '+name)
	if not item.options.has_key(optionname):
	    item.options[optionname] = None
	item.originals[optionname] = item.options[optionname]
	item.options[optionname] = value

    def add_item(self, item):
	self.newitem(item)

    def destroy(self, itemname):
        try:
            _ = self.getitem(itemname)
        except:
            sys.stderr.write('(Not) Destroying unknown item '+itemname+'\n')
            return
	tokill = [itemname]
	changes = 1
	while changes:
	    changes = 0
	    for item in self.items:
		for victim in tokill:
		    if victim in item.options['requires'] and not (item.name in tokill):
			tokill.append(item.name)
			changes = 1
	for itemname in tokill:
	    del self.db[itemname]
	    self.items = self.db.values()

    def reloc(self, itemname, newlocation):
	item = self.getitem(itemname)

	if not item.options.has_key('path'):
	    error('item '+itemname+' does not have a path so cannot be relocated')
	    
	if type(newlocation) == type(''):
	    item.location = newlocation

	if type(newlocation) == type({}):
	    for file in newlocation.keys():
		item.locatefiles[file] = newlocation[file]

    def include(self, filename):
	self.interpret(filename)

    def add_source_tree(self, location):
	self.extrasearchpath = self.extrasearchpath + [location]

    def use_package(self, location):
        if not os.path.isdir(location + '/glue/packages'):
            error('Package at '+location+' does not have a subdirectory glue/packages so cannot be added to a tree. Hint; add_source_tree will work with the same arugments, but you will need to manually tell the build system about items in the current tree. \n')
        self.extrasearchpath = self.extrasearchpath + [location]
        x = len(self.items)
        newitems, newdeps = buildutils.merge_all([location + '/glue/packages'], 'items')
        self.items = self.items + newitems
        self.deps = self.deps + newdeps
        #sys.stderr.write('(Adding %d items from %s)\n' % (len(self.items) - x, location))
        self.init(self.arch_name, self.platform_name, self.target_name)
        
######################################################################
# more utility functions

    def checkrequirement(self, require, srcname='unknown'):
	if type(require) == type((1,2)):
	    item = self.db[require[0]]
	    #print 'check choice set to',item.options['value'],'against requirement',require[1],'name',item.name,'srcname',srcname
	    return item.options['value'] == require[1]
	if not self.db.has_key(require):
	    sys.stderr.write('Warning; unknown requirement '+require+' of '+srcname+'\n')
            return 0
	return self.db[require].options['value'] > 0


    def makeconsistent(self, force = 0):
	if self.donottouch: return
	changes = 1
	while changes:
	    changes = 0
	    for item in self.items:
		if item.options.has_key('requires') and item.isactive():
		    for require in item.options['requires']:
			if not self.checkrequirement(require, item.name):
			    if type(item['type']) == type([]):
				index = 0
				for (activeness, friendly, cppname) in item['type']:
				    if not activeness:
					item['value'] = index
					break
				    index = index + 1
				if index == len(item['type']):
				    print 'Destorying ',item.name,' because it is a multiple choice with no inactive choices'
				    self.destroy(item.name)

				    break
				item['value'] = index
			    else:
				item['value'] = 0
			    #print 'Disabling ',item.name,' because requirement ',require,' is not available'
			    changes = 1

    def getitem(self, name):
	if self.db.has_key(name):
	    return self.db[name]
	else:
	    error('item '+name+' not known')
	    raise NoSuchItem

    def getitems(self):
	return self.items
    def getdb(self):
	return self.db
    
    def getpath(self):
	import treeinfo

	return [treeinfo.source_tree_dir]+self.extrasearchpath
    
######################################################################
# interpreter
    def interpret(self, filename):
        sys.stderr.write('Customizing using '+filename+'\n')
	try:
	    o = open(filename, 'r')
	except:
	    sys.stderr.write('ERROR; filename '+filename+' not found\n')
	    if self.fatal: sys.exit(1)
	    return 1
	self.deps = self.deps + [filename]
	data = o.read() + '\n'
	o.close()
	verbose = 0
	if os.environ.has_key('CUSTOMIZER_VERBOSE'): verbose = 1
	index = 0
	linenum = 1
	bra = 0
	command = ''
	comment = 0
	(filename_head, filename_tail) = posixpath.split(filename)
	if verbose: print 'Customzing from ',filename
	while index < len(data):
	    if data[index] == '\n':
		if not bra and not comment:
		    bail = 0
		    try:
			if len(command) > regex.match('[ \t]*', command): 
			    if verbose: print `linenum`+': ',command
			    exec('self.'+command, self.aliases)
		    except Problem, str:
			sys.stderr.write(str + '; ')
			bail = 1 
		    except AttributeError, str:
			sys.stderr.write('Not a valid command; ')
			bail = 1
		    except SyntaxError, str:
			sys.stderr.write('Syntax error;')
			bail = 1
		    except invalidkey, _: bail = 1
		    except NameError, str:
			badname = str
			if not (type(badname)  == type('')):
			    badname = str.args[0]
			sys.stderr.write('Invalid identifier '+`badname`+'; ')
			bail = 1
		    if bail:
			sys.stderr.write(' aborting interpret in '+filename+' line '+`linenum`+' while executing '+command+'\n')
			if self.fatal: sys.exit(1)
			return 1

		    command = ''
		comment = 0
		linenum = linenum + 1
	    else:
		if data[index] == '#': comment = 1
		if not comment:
		    if data[index:index+1] == '@':
			command = command + filename_head
		    else:
			if command == '\t': command = command + '        '
			else:
			    command = command + data[index]
			    if data[index] == '(': bra = bra + 1
			    if data[index] == ')': bra = bra - 1
	    index = index + 1
	    
