#! /usr/bin/env python

#
# nemmaster nemtree
#
# Copyright 1998 Dickon Reed
#
# The entry point to nemmaster; builds a tree for you
#

from sys import argv
import sys, posixpath
from buildutils import *

def usage():
    if sys.argv[0] == './changefoundations.py':
	changeusage()
    
    sys.stderr.write("""

(To obtain a detailed reference error message, run this script as 
./changefoundations.py instead. See the build system users guide for more
details.). Typical usage:

- Enter the glue directory of the private tree.
- Execute layfoundations.py there, specifying the absolute root of the
new build tree, the absolute root of the private tree and the
architecture name. I have environment variables NB and NP set up for
containing the roots of my build and private tree, so I go:

cd $NM/glue
./layfoundations.py $NB $NM intel $NP/choices.intel

This takes things as far as mkbuildtree use to. So, you will still need
to type make in the build tree.

""")
    sys.exit(1)



def changeusage():
    sys.stderr.write("""
Usage: layfoundations.py (actions) (options) (arguments)
or     changefoundations.py (actions) (options) (arguments)

The layfoundations.py script can be run either as layfoundations.py or
changefoundations.py. Layfoundations performs a number of actions, listed
below. If invoked as layfoundations.py all actions are turned on by default.
If invoekd as changefoundations.py, however, all actions are off by default. 

Some actions have extra arguments associated with them, which are listed
after all the actions and options. Every action can be enabled or disabled
individually, though no doubt some combinations will not prove useful. To 
enable an action, add -enable_foo to the command line. -disable_foo disables
the action. If foo is all, then all actions are enabled or disabled. Note that
+ may be used intead of -, and that if enable_ or disable_ are omitted, then
layfoundations defaults to disabling options and changefoundations to enabling
options.

The actions, in the order in which they are performed, are described in the build
system users guide.

""")  
    sys.exit(1)





defpairs = {
    'axp3000' : ('alpha', 'alpha_osf1_3.2'),
    'eb164' : ('alpha', 'alpha_osf1_3.2'),
    'eb64' : ('alpha', 'alpha_osf1_3.2'),
    'fpc3' : ('arm', 'linux_for_arm'),
    'riscpc' : ('arm', 'linux_for_arm'),
    'shark' : ('arm', 'linux_for_arm'),
    'srcit' : ('arm', 'linux__for_arm'),
    'intel' : ('ix86', 'ix86_linux'),
    'maxine' : ('mips', 'ultrix')
}

platform_override = 'none'
UnknownCommandLineOption = 'UnknownCommandLineOption'
obtainmode = 'resolved_link'

invokemode = 'usage'
print sys.argv
defflag = 0
if sys.argv[0] == './layfoundations.py':
    invokemode = 'lay'
    defflag = 1

if sys.argv[0] == './changefoundations.py':
    invokemode = 'change'
    defflag = 0

thingstodo = {
    'locatetree' : defflag,
    'gentreeinfo' : defflag,
    'createtree' : defflag,
    'geninfra' : defflag,
    'filldirs' : defflag,
    'genmake' : defflag
}

choices= None

if invokemode == 'usage':
    usage()

try:
    if len(argv) == 1: 
	argsleft = 0
    else:
	argsleft = (argv[1][0] in  ['+', '-'])
    while argsleft:
	option = argv[1][1:]
	if option == 'link':
	    obtainmode = 'link'
	elif option == 'copy':
	    obtainmode = 'copy'
	elif option == 'resolved':
	    obtainmode = 'resolved_link'
	elif option == 'disable_all':
	    for k in thingstodo.keys():
		thingstodo[k] = 0
	elif option == 'enable_all':
	    for k in thingstodo.keys():
		thingstodo[k] = 0
	elif option[:8] == 'disable_':
	    if thingstodo.has_key(option[8:]):
		thingstodo[option[8:]] = 0
	    else:
		raise UnknownCommandLineOption
	elif option[:7] == 'enable_':
	    if thingstodo.has_key(option[7:]):
		thingstodo[option[7:]] = 1
	    else:
		raise UnknownCommandLineOption
	elif option[:9] == 'platform=':
	    platform_override = option[9:]
	elif option in thingstodo.keys():
	    if invokemode == 'change':
		thingstodo[option] = 1
	    if invokemode == 'lay':
		thingstodo[option] = 0
	else:
	    sys.stderr.write('Unknown command line option '+option+'\n')
	    raise UnknownCommandLineOption, option
	argv = [argv[0]] + argv[2:]
	if len(argv) == 1:
	    argsleft = 0
	elif argv[1][0] != '+':
	    argsleft = 0
except UnknownCommandLineOption:
    usage()

print 'Todo set is ',thingstodo

if thingstodo['locatetree']:
    try:
	build_tree_dir = argv[1]
    except: 
	usage()
else:
    if thingstodo['gentreeinfo']:
	sys.stderr.write('Error; gentreeinfo action enabled but locatetree disabled; illegal\n\n')
	sys.exit(2)
    try:
	from treeinfo import *
    except:
	sys.stderr.write('Error; locatetree not specified and treeinfo.py could not be found, so there\nwas not way of finding the build tree.\n\nHint: rerun in the glue directory of the build tree.\n')
	sys.exit(3)
if thingstodo['gentreeinfo']:
    try:
	source_tree_dir = argv[2]
	target_name = argv[3]
	if len(argv)==5:
	    choices = argv[4]
	(arch_name, platform_name) = defpairs[target_name]
    except:
	usage()
    if platform_override != 'none':
	platform_name = platform_override

if thingstodo['createtree']:
    print 'Source tree is',source_tree_dir
    print 'Build tree is',build_tree_dir
    print 'Target is',(target_name,arch_name,platform_name)

    makedirandpath(build_tree_dir)
    maybemakedir(build_tree_dir + '/glue')
    maybemakedir(build_tree_dir + '/h')
    maybemakedir(build_tree_dir + '/h/info')
    maybemakedir(build_tree_dir + '/h/autoconf')
    maybemakedir(build_tree_dir + '/links')


sys.path = [build_tree_dir+'/glue' ,os.getcwd()]+sys.path

os.chdir(build_tree_dir)


if thingstodo['gentreeinfo']:
    print 'Generating tree information'
    o = open(build_tree_dir+'/glue/treeinfo.py', 'w')
    o.write('arch_name = "'+arch_name+'"\n')
    o.write('platform_name = "'+platform_name+'"\n')
    o.write('target_name = "'+target_name+'"\n')
    o.write('source_tree_dir = "'+source_tree_dir+'"\n')
    o.write('build_tree_dir = "'+build_tree_dir+'"\n')
    o.write('obtainmode = "'+obtainmode+'"\n')
    o.close()
    if choices:
	print 'Linking choices file',choices
	try:
	    os.symlink(choices, build_tree_dir+'/choices')
	except:
	    print 'Warning; failed to make symlink to ',choices,' from ',build_tree_dir+'/choices'
    else:
	if not posixpath.exists(build_tree_dir+'/choices'):
	    print 'Creating null choices file'
	    o = open(build_tree_dir+'/choices', 'w')
	    o.write('# null choices file automatically generated by layfoundations.py\n')
	    o.close()
from treeinfo import *

# we are now operating in the root of the (new?) build tree, with nemmaster
# and treeinfo on syspath. So the work of building the tree can begin properly

import blueprint, gendirs, geninfrastructure, sourcemanager

print 'Obtaining glue directory'
sourcemanager.obtaindir('glue')

if thingstodo['geninfra']:
    print '\n\nGenerating infrastructure'
    geninfrastructure.go()

if thingstodo['filldirs']:
    print '\n\nFilling out directories'
    gendirs.go()

if thingstodo['genmake']:
    print '\n\nSorting out make system'
    # legacy Make system stuff

    symlink(build_tree_dir+'/mk/'+platform_name+'.pfm.mk', build_tree_dir+'/mk/platform.mk')
    symlink(build_tree_dir+'/mk/'+target_name+'.tgt.mk', build_tree_dir+'/mk/target.mk')
    symlink(build_tree_dir+'/glue/layfoundations.py', build_tree_dir+'/glue/changefoundations.py')
    sourcemanager.obtain('Makefile')
    # XXX arch is not hooked in? (seemed to be what mkbuildtree did)

    o = open('mk/source.mk', 'w')
    o.write('SRCROOT = '+source_tree_dir+'\n')
    o.write('BUILDROOT = '+build_tree_dir+'\n')
    o.close()

    # special case the root makefile, as it is both infrastrcutre and functional
    sourcemanager.obtain('Makefile')
    
    import plumbtree
print '\n\nTree set up. Hack away.\n\n'

