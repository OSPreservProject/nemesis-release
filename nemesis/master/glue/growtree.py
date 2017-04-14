#! /usr/bin/env python

import sys
import treeinfo, sys, configutils, geninfrastructure, gendirs, os

os.chdir(treeinfo.build_tree_dir)

if treeinfo.source_tree_dir == None:
    sys.stderr.write('ERROR: grow support is not available becasue there is no source tree specified in treeinfo\n')
    sys.exit(1)
    
if len(sys.argv)==1:
    sys.stderr.write("""
growtree.py subpath subpath subpath

Grow those parts of the tree that have a path names starting with one of 
the subpaths. If one of the subpaths is empty, the whole tree is grown.
subpaths should be a slash seperated sequence of slashes, starting with
the root of the tree.

Example:

./growtree.py app mod/net

Or:

./growtree.py ""


""")
    sys.exit(1)

linkcache = {}
for usubpath in sys.argv[1:]:
    subpath = usubpath
    if usubpath == '.':
	subpath = ''
    if (len(subpath)>1 and subpath[0] == '/'):
	sys.stderr.write('Warning; subpath looks bad\n')
    if (len(subpath)>2 and subpath[:2] == './'):
	subpath = subpath[2:]
    sys.stderr.write('Filling out subpath "'+subpath+'"\n')
    geninfrastructure.go(subpath)
    gendirs.go(subpath)
    sys.stderr.write('\n')


