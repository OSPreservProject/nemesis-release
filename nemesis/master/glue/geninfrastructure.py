# nemmaster geninfrastructure
#
# Copyright 1998 Dickon Reed
#

import os, string, posixpath, sys
import configutils, blueprint, buildutils, gendetails, genmakefile
from treeinfo import build_tree_dir, platform_name

# log the fact that we are blowing away the makefile
def irradicate(infra, odel, odelscript):
    str = './'+infra+'/Makefile'
    odel.write(str + (39-len(str))*' '+' >> ./contrib/old/makefiles/'+ string.join(string.split(infra, '/'), '.') + '.makefile\n' )
    odelscript.write('mv ./'+infra+'/Makefile  ./contrib/old/makefiles/'+ string.join(string.split(infra, '/'), '.') + '.makefile\n')
    odelscript.write('mv ./'+infra+'/RCS/Makefile,v  ./contrib/old/makefiles/RCS/'+ string.join(string.split(infra, '/'), '.') + '.makefile,v\n')



# this entry point just fixes up the subdirs list
# bought out in the hope it might be useful by itself
# returns a mapping from paths to items
def fixsubdirs():
    pathmap = {}
    for item in blueprint.items:
	path = item.get_path()

	if type(path) == type('abc'):
	    pathmap[path] = item
    for path in pathmap.keys():
	pathsplit = string.split(path, '/')
	for i in range(0, len(pathsplit)):
	    partpath = string.join(pathsplit[:i], '/')
	    if not pathmap.has_key(partpath):
		print 'Error: Build item has path '+path+', but no build item has path "'+partpath+'"'
		sys.exit(1)
	    tackedpath = string.join(pathsplit[:i+1], '/')
	    if pathmap.has_key(tackedpath):
		pathmap[partpath].add_subdir(pathsplit[i], pathmap[tackedpath])
	    else:
		# for a/b/c/d/e/f, a/b exists, c doesn't, e does and d/e may or
		# may not
		# XXX; i don't think I have to worry here, as we will
		# eventually catch this case in the test above
		print 'Warning; could not find build item',tackedpath
		pass
    return pathmap

def go(startofpath=''):
    pathmap = fixsubdirs()

    pathsorted = pathmap.keys()
    pathsorted.sort()
    for path in pathsorted:
	#print 'Path: "'+path+'" item name '+pathmap[path].get_name()
	if path[0:len(startofpath)] == startofpath:
	   buildutils.maybemakedir(build_tree_dir+'/'+path)

    for path in pathmap.keys():
	if path[0:len(startofpath)] == startofpath:
	    item = pathmap[path]
	    sys.stdout.write('mk '+item.get_name())
	    sys.stdout.flush()
	    genmakefile.genmakefile(item)
	    sys.stdout.write(',')
	    sys.stdout.flush()
