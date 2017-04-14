#! /usr/bin/env python

import marshal, string, sys

f = open('repo/intfbuilder.cache', 'r')
cache = marshal.load(f)
f.close()

for interface in cache.keys():
    interfacebase = string.split(interface, '.')[0]
    print '    ("'+interfacebase+'\(\$\|_\)", "'+interfacebase+'.ih"),'
print '\n],\n{'
for interface in cache.keys():
    interfacebase = string.split(interface, '.')[0]
    l = ''
    for dep in cache[interface]['deps']:
	if dep != interface:
	    depbase = string.split(dep, '.')[0]
	    l = l +  '"'+depbase+'.ih", '
    if l != '':
	print '    "'+interfacebase+'.ih" : ['+l+'],'

