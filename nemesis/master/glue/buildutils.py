#
# nemmaster buildutils
#
# Copyright 1998 Dickon Reed
#
# Utilities for building; mostly wrappers and abstractions round 
# system calls
#

import os, posixpath, posix, string, posixpath, sys, errno, regsub

SymLinkFailed = 'SymLinkFailed'
NonZeroExitStatus = "NonZeroExitStatus"

def Warning(s): sys.stderr.write(s+'\n')

# a wrapper around system, for tracing purposes and to remind me when 
# I am still doing system calls

def system(callstr, pretend = 0, verbose=1):
    if verbose: 
	print 'Shell command :' + callstr
	sys.stdout.flush()
    if pretend: return
    r = os.system(callstr)
    if r != 0:
	if verbose: print 'Returned ',r
	raise NonZeroExitStatus, callstr

# make a directory if it does not exist yet
def maybemakedir(path, verbose = 1):
    if os.path.isdir(path):
	#if verbose: print 'Directory '+path+' already exists'
	return

    #if verbose: print 'System call mkdir('+path+')'

    try:
	os.mkdir(path)
    except:
	sys.stderr.write('\nWarning; Unable to create directory '+path+'\n')


# make directory and path; behaves like mkdir -p
def makedirandpath(fullpath):
    print fullpath;
    dirs = string.split(fullpath, '/');
    if len(dirs[0]) != 0:
	path = '/'
    else:
	path = ''

    for d in dirs:
	path = path + d + '/'
	maybemakedir(path)


# canonicalize a path
# the path consists of $canon/$subpath.
# the $canon part is initially empty, but is grown with known canonical bits of tree
# the $subpath part is *not* canonical yet
# this routine works recursively; it should first be invoked as 
#   canonicalize('', some_random_path)
#
# linkcache is a dictonary that should start off empty, and only be updated by this routine
# verbose is a flag which if set to a true value will produce tracing

def canonicalize(canon, subpath, linkcache = {}, verbose=0):

    if verbose: print canon, ' -> [',subpath,']'

    # split the subpath in to components
    pathsplit = string.split(subpath, '/')
    # if we haven't got any, then we get returned ['']
    if len(pathsplit) == 1 and pathsplit[0] == '':
	return canon

    # make linksrc the thing under consideration; ie the top element of subpath
    linksrc = canon + '/' + pathsplit[0]

    # define the variable linkdest to be None if linksrc is not a link,
    # else make it be the place that linksrc is pointing to
    if linkcache.has_key(linksrc): 
	linkdest = linkcache[linksrc]
    else:
	linkdest = None
	if posixpath.islink(linksrc):
	    linkdest = os.readlink(linksrc)
	linkcache[linksrc] = linkdest

    if linkdest:
	# linksrc is a link, deal with it
	if linkdest[0] == '/':
	    # we have found an absolute link
	    # so restart the resolution, using linkdest as our subpath
	    if verbose: print 'abslink',linkdest

	    return canonicalize('', linkdest[1:] + '/' + string.join(pathsplit[1:], '/'), linkcache, verbose)
	else:
	    # we have found a relative link
	    # peel off any .. by stripping away canon
	    while linkdest[0:2] == '../':
		if verbose: print 'uplink',linkdest
		canon = string.join(string.split(canon, '/')[:-1], '/')
		linkdest = linkdest[3:]
	    if verbose: print 'rellink',linkdest
	    # recurse with the new (possible reduced) canon and the stripped down path
	    return canonicalize(canon, linkdest + '/' + string.join(pathsplit[1:], '/'), linkcache, verbose)
    else:
	# we take this branch if the linksrc was absolute, or it did not exist
	if verbose: print 'abs ',pathsplit[0]
	return canonicalize(canon + '/' + pathsplit[0], string.join(pathsplit[1:], '/'), linkcache, verbose)

def relpath(src, dest, linkcache = {}, verbose = 0):
    # give up if either path is not relative
    if src[0] != '/' or dest[0] != '/':
	return src

    src = canonicalize('', src[1:], linkcache = linkcache, verbose = verbose)
    dest = canonicalize('', dest[1:], linkcache = linkcache, verbose = verbose)

    if src == dest:
	print 'Warning; creating symlink to itself called',src
	return src

    # split off each path in to components
    srcsplit = string.split(src[1:], '/')
    destsplit = string.split(dest[1:], '/')
	
    
    if verbose:
	print '(src,dest)',(src,dest)
	print 'split ',(srcsplit, destsplit)

    # figure out how much of the stuff is common
    common = 0
    for i in range(0, min(len(srcsplit), len(destsplit))):
	if srcsplit[i] == destsplit[i]: common = common + 1
	else: break

    if verbose:
	print 'First ',common,'parts are common'

    # if there is nothing common, give up rather than adding lots of ../
    if common == 0:
	return src

    # if everything is common, symlink up a few places
    if common == len(srcsplit):
	# src must be a directory
	tostrip = len(destsplit) - common
    else:
	# figure out how many ../ to have
	tostrip = len(destsplit) - common - 1 

    if verbose:
	print 'Will strip ',tostrip,'parts'
    
    # build the result
    relpath = '../' * tostrip + string.join(srcsplit[common:],'/')

    return relpath


# symlink with just a touch of extra error handling and tracing
def symlink(src, dest, linkcache = {}, canonicalize=1):
    if (os.path.islink(dest)) or os.path.exists(dest):
	print '(not overwriting',dest,')'
	return
    #print 'Symlink(',dest,')'
    if canonicalize:
	srcp = relpath(src, dest, linkcache)
    else:
	srcp = src
    os.symlink(srcp, dest)


# write out filename with contents of str iff (filename does not exist OR
# filename contents != str)

# useful so that mtimes do not change in the case where nothing has
# really changed in a file

GenerateOnlyTakesStrings = "GenerateOnlyTakesStrings"

def generate(filename, str):
    if type(str) != type(''):
	raise GenerateOnlyTakesStrings, type(str)
    happy = 0
    if posixpath.islink(filename):
	print '(Blowing away symlink '+filename+' to make way for autogenerated file)'
	os.remove(filename)
    try:
	o = open(filename, 'r')
	snow = o.read()
	o.close()
	if snow == str:
	    happy = 1
    except:
	# it is not there; pass
	pass
    if not happy:
	try:
	    o = open(filename, 'w')
	    o.write(str)
	    o.close()
	except:
	    print 'Error: Unable to generate '+filename
	    sys.exit(1)
    else:
	#print '(No changes to '+filename+')'
	pass

def merge_all(directories, field_name, detectdups = 1):
    #sys.stderr.write('Looking at packages directories '+`directories`+'\n')
    
    dirmap = {}
    packages = []
    deps = []
    for directory in directories:
        if not os.path.isdir(directory):
            #sys.stderr.write('Pacakges directory '+directory+' does not exist yet\n')
            continue
        files = os.listdir(directory)
        files.sort()

        for file in files:
            if file[-3:] == '.py':
                packagename = file[:-3]
                if packagename in packages:
                    #sys.stderr.write('(package '+packagename+' from '+dirmap[packagename]+' rather than '+directory+')\n')
                    pass
                else:
                    packages.append(packagename)
                    dirmap[packagename] = directory
    target = []
    sys.stderr.write( 'packages ')
    for package in packages:
        oldpath = sys.path
        filename = dirmap[package] +'/' + package + '.py'
        sys.path = [dirmap[package]]
        deps.append(filename)
        sys.stderr.write(package +' ') #+ 'from '+dirmap[package]+ ' '
                         
        exec 'import '+package
        newstuff = eval(package+'.'+field_name)
        if detectdups:
            namesofar = []
            for item in newstuff:
                if item.name in namesofar:
                    Warning('Warning: Item '+item.name+' is defined multiple times in file '+filename)
                namesofar.append(item.name)
                for itemp in target:
                    if item.name == itemp.name:
                        Warning('Warning: Item '+item.name+' is defined both in '+filename+' and '+target.filename)
                item.filename = filename
        target = target + newstuff
        sys.path = oldpath
    sys.stderr.write('\n')
    return (target,deps)

def copy(source, target):
    print source,'->',target
    oi = open(source, 'r')
    oo = open(target, 'w')
    oo.write(oi.read())
    oo.close()
    oi.close()
