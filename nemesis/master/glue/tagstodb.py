#! /usr/bin/env python
import string, regsub, sys

def multisplit(str, seps):
    list = []
    index = 0
    base = 0
    place = 0
    while index < len(str):
	if not ord(str[index]) in range (32, 127) and not str[index] in seps:
	    #print ord(str[index]), str[index]
	    pass
	if str[index] in seps:
	    list.append(str[base:index])
	    place = 1
	elif place:
	    base = index
	    place = 0
	index = index + 1
    if not place:
	list.append(str[base:])
    #print str,'->',list

    return list

## o = open(sys.argv[1], 'r')
## l = o.readlines()
## o.close()

## for line in l:
##     spl = string.split(line)[:-1]
##     filename = string.split(spl[1], '/')[-1]
##     extn= string.split(filename, '.')[-1]
##     if extn in ['h', 'ih']:
## 	#print '    ("'+regsub.sub('\$', '\\$',spl[0])+'", "'+filename+'"),'
## 	pass

o = open(sys.argv[1], 'r')
l = o.readlines()
o.close()

sourcefile = None
sourcefilestarts = 0
for index in range(0, len(l)):
    if len(l[index]) == 2 and ord(l[index][0]) == 12:
	sourcefile = string.split(l[index+1],',')[0]
	filename = string.split(sourcefile, '/')[-1]
	sourcefilestarts = index + 1
    splsc = multisplit(l[index][:-1], [';', chr(1), chr(127)])
    #print splsc
    if splsc[0][:7] == 'typedef':
	print '    ("'+splsc[-2]+'", "'+filename+'"),'
    if splsc[0][:7] == '#define':
	splsp = multisplit(splsc[0], [' ', chr(9)])
	if len(splsp) == 2:
	    print '    ("'+splsp[1]+'", "'+filename+'"),'

