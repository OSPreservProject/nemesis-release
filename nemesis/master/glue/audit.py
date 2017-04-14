# tool to produce audit of the current blueprint

# Copyright 1998 Dickon Reed

import blueprint
import sys

def foreach(func):
    sorted = blueprint.db.keys()
    sorted.sort()
    for itemname in sorted:
	value = blueprint.db[itemname].options['value']
	t =  blueprint.db[itemname].options['type']
	item = blueprint.db[itemname]
	if t == 'bool':
	    if value: value = 'Y'
	    else: value = 'N'
	if t == 'quad':
	    value = ['NONE', 'SUPPORT', 'BUILD', 'NBF'][value]
	if type(t) == type([]):
	    value = t[value][2]    
	func(item, value)

def changes_output(item, value):
    print "set('"+item.name+"',"+`value`+')'

def extended_output(item, value):
    print "set('"+item.name+"',"+`value`+')'
    if item.options.has_key('description'): print '# '+item.options['description']
    if item.options.has_key('helptext'): print '# (',item.options['helptext'],')'
    print

def compact_output(item, value):
    if item.get_class() == 'dir':
	return
    print "set('"+item.name+"',"+`value`+')'
    if item.options.has_key('description'): print '# '+item.options['description']
    if item.options.has_key('helptext'): print '# (',item.options['helptext'],')'
    print



format = 'changes'
if len(sys.argv) == 2:
    format = sys.argv[1]


if format == 'changes':
    foreach(changes_output)

elif format == 'compact':
    foreach(compact_output)

elif format == 'extended':
    foreach(extended_output)

else:
    sys.stderr.write('Unknown audit format ',format)

