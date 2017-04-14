#! /usr/bin/env python
#
# depsshow.py;  Copyright 1998, Dickon Reed
#
# Reads all .d files out of the current tree, looks at all absolute
# dependencies and compiles the inverse, and displays it. Therefore,
# you get shown all places where headers files (and other things) are used.

import os, posixpath, string, sys
f = os.popen('find . -name \*.d')
files = f.readlines()
f.close()

deps = {}
uses = {}

for file in files:
    (head, tail) = posixpath.split(file[:-1])
    path = head[2:]
    o = open(file[:-1], 'r')
    l = o.readlines()
    o.close()
    lines = map(lambda x: x[:-1], l)
    rang = range(0,len(lines))
    rang.reverse()
    flag = 1
    count = 0
    while count < len(lines):
	while len(lines[count])>1 and lines[count][-1] == '\\':
	    lines[count] = lines[count][:-1] + lines[count+1]
	    del lines[count+1]
	count = count + 1
    pathlen = len(string.split(path,'/'))
    reltoroot = '../'*pathlen
    for line in lines:
	filenames = string.split(line, ' ')
	for filename in filenames[1:]:
	    if filename[:len(reltoroot)] == reltoroot:
		fullname = path + '/' + filenames[0]
		if fullname[-1] == ':': fullname=fullname[:-1]
		depname = filename[len(reltoroot):]
		if not deps.has_key(fullname): deps[fullname] = []
		deps[fullname].append(depname)
		if not uses.has_key(depname): uses[depname] = []
		uses[depname].append( fullname )
    sys.stderr.write('.')

useskeys = uses.keys()
useskeys.sort()
for file in useskeys:
    print
    print
    print 'File',file,'is used in ',len(uses[file]),' files: ',uses[file]


