# nemmaster sourcemanager
#
# Copyright 1998 Dickon Reed
#
# Manage the place that source code is obtained from

from treeinfo import *
from buildutils import *
import posixpath, blueprint, os

def getholdof(filenamefrom, filenameto, linkcache = {}):
    #print 'Getholdof',filenamefrom,filenameto
    if obtainmode == 'link':
	symlink(filenamefrom, filenameto, linkcache, canonicalize=0)
    if obtainmode == 'resolved_link':
	symlink(filenamefrom, filenameto, linkcache, canonicalize=1)
    if obtainmode == 'copy':
	system('cp '+filenamefrom+' '+filenameto)

# linkdir; my own version of lndir
# the differences are that this one doesn't have different semantics
# for different versions of X11R5 (useful) and it can be made to do a shallow
# lndir; ie only link the files of the source directory rather than 
# linking all files reachable by recursion

# it also produces tracing about things it isn't doing, and could be made
# to blow away old files and/or links straightforwardly

# it assumes that the destination directory exists

def linkdir_compile(arg, dirname, names):
    (files, dirs, recursive) = arg
    dirs[dirname] = 1
    #print 'at start ',dirname, names
    names_copy = map(lambda x:x, names)
    count = 0
    for name in names_copy:
	if name != '.' and name !='..':
	    #print 'Statting ',dirname+'/'+name
	    if posixpath.isdir(dirname + '/' + name):
		if not recursive:
		    #print 'Blow away '+name
		    del names[count]
		    count = count - 1
	    else:
		#print 'Is file ',dirname,name
		files[dirname + '/' + name] = dirname
		#sys.stdout.write('.')
		#sys.stdout.flush()
	count = count +1
    #print 'left in ',dirname, names

def linkdir(sourcedir, targetdir, recursive, linkcache = {}, overrides = {}):
    #print 'linkdir', sourcedir, targetdir
    files = {}
    dirs = {}
    overrides_performed = []
    posixpath.walk(sourcedir, linkdir_compile, (files,dirs,recursive))
    #print
    #makedirandpath(targetdir)
    directorys = dirs.keys()
    directorys.sort()
    for directory in directorys:
	directoryimage = directory[len(sourcedir):]
	dirs[directory] = directoryimage
	maybemakedir(targetdir + directoryimage, verbose=0)
    for file in files.keys():
        if file[-1] == '~' or file[-4:] == '.pyc':
            #print 'Skipping file ',file
            pass
        else:
            pathend = string.rfind(file, '/')
            path = file[:pathend]
            subpath = path[len(sourcedir):]

            filename = file[pathend+1:]
            doit = 1
            abstargetname = targetdir+subpath+'/' + filename
            target = targetdir+subpath+'/' + filename
            reformedfile = filename
            if len(subpath)>1: reformedfile = subpath[1:]+'/'+filename
            if overrides.has_key(reformedfile): 
                file = overrides[reformedfile]
                overrides_performed.append([reformedfile])
            try:
                os.unlink(target)
            except posix.error:
                pass
            if file:
                getholdof(file,targetdir+subpath+'/' + filename, linkcache) 
            else:
                # user has specified None as an override; do not create the file
                pass
    return overrides_performed
# test code 
foo = """
from buildutils import linkdir
linkdir('/homes/dr10009/nl/dev/pci/s3/xrip', '/local/scratch/dr10009/test/wibble', 1)
#
from buildutils import linkdir
linkdir('/homes/dr10009/nl/dev/pci/s3/xrip', '/local/scratch/dr10009/test/wibble_norecurse', 0)
#
"""

# source manager proper

# obtain a file from the source trees
def obtain(treepathname, linkcache = {}):
    for sdir in blueprint.searchpath:
	getholdof(sdir+'/'+treepathname, build_tree_dir + '/'+treepathname, linkcache = {})
	
# obtain a directory from the source tree
def obtaindir(treepath, linkcache = {}, dorecursive = 1, locationo = None, overrides = {}):
    for sdir in blueprint.searchpath:
	location = locationo
	if location == None:
	    location = sdir + '/' + treepath
	targetdir = build_tree_dir+'/'+treepath
	overrides_performed = linkdir(location, targetdir, recursive = dorecursive, linkcache = linkcache, overrides = overrides)
	for override in overrides.keys():
	    if not override in overrides_performed:
		# this override did not get performed explicitly; do it by hand
		src = overrides[override]
		tgt = targetdir + '/'+ override
		# remove the file from the build tree
		if src != 0:
		    try:
			os.unlink(tgt)
		    except posix.error:
			pass
		# and create it if necessary
		if src:
		    getholdof(src, tgt, linkcache)
