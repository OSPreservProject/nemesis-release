#
# nemmaster gendirs
#
# Copyright 1998 Dickon Reed
#
# Generate the build tree directories
#

import configutils, blueprint, buildutils, os, string, sys
from treeinfo import build_tree_dir, platform_name
import sourcemanager

def go(startofpath = ''):
    #happyitems = configutils.getactivesubset(blueprint.items)
    happyitems = blueprint.items

    # find out all the items with paths, and all the paths
    # used in those items
    pathitems = []
    pathlist = []

    count = 0
    for item in happyitems:
	path = item.get_path()

	if path:
	    pathitems.append(item)
	    pathlist.append(path)

    linkcache = {}

    existingdirs = {}
    existingdirs[ '' ] = 1

    for item in pathitems:
	modpath = item.get_path()
	l = string.split(modpath, '/')
	lprime = []
	# make sure the directories are created
	for pathitem in l:
	    subpath = string.join(lprime, '/')

	    if not(existingdirs.has_key(subpath)):
		buildutils.maybemakedir(subpath)
		existingdirs[subpath] =1
	    lprime.append(pathitem)

	# if we are inside the directory specified by startofpath, 
	if item.get_path()[0:len(startofpath)] == startofpath:
	    sys.stdout.write('ln '+item.get_name())
	    sys.stdout.flush()
	    if item.options.has_key('makefileflags') and item.options['makefileflags'].has_key('custom') and not item.options['makefileflags']['custom']:
		item.locatefiles['Makefile'] = 0
	    sourcemanager.obtaindir(item.get_path(), linkcache, item.is_grow_recursive(), item.location, item.locatefiles)
	    sys.stdout.write(',')
	    sys.stdout.flush()
	    count = count + 1

    if count == 0:
	sys.stderr.write("""
Warning; no items found in directory where gendirs was executed.
Perhaps you are running make grow in a subdirectory of an item?
If so, you must run it in the top-level directory of the item.

Eg. ntsc/ix86/intel is a subdirectory of ntsc_ix86; to grow
the intel directory you must grow in the ntsc/ix86 directory
""")

##     framework = {}
##     for path in pathlist:
## 	breakdown = string.split(path, '/')
## 	subpath = []
## 	for dir in breakdown:
## 	    framework[string.join(subpath,'/')] = 1
## 	    subpath.append(dir)


