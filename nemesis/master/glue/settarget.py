#! /usr/bin/env python

usage = """

settarget.py platform target arch

set up a build tree in the current working directory. This is an alternative
to layfoundations.py.


"""

import sys, os

def error(mesg):
    sys.stderr.write('ERROR: '+mesg+'\n')
    sys.exit(0)

if len(sys.argv) != 4:
    sys.stderr.write('Usage: '+usage+'\n')
    sys.exit(0)
    
platform = sys.argv[1]
target = sys.argv[2]
arch = sys.argv[3]

platformmk = platform+'.pfm.mk'
targetmk = target+'.tgt.mk'

if not os.path.exists('mk/'+platformmk):
    error('Platform %s not found (no mk/%s)' % (platform, platformmk))

if not os.path.exists('mk/'+targetmk):
    error('Target %s not found (no mk/%s)' % (target, targetmk))

for file in ['mk/platform.mk', 'mk/target.mk', 'glue/treeinfo.py', 'mk/source.mk']:
    if os.path.exists(file): os.unlink(file)

os.symlink(platformmk, 'mk/platform.mk')
os.symlink(targetmk, 'mk/target.mk')

o = open('glue/treeinfo.py', 'w')
o.write("""
arch_name = "%s"
platform_name = "%s"
target_name = "%s"
source_tree_dir = None
build_tree_dir = "%s"
obtainmode = None
""" % (arch, platform, target, os.getcwd()))
o.close()


# make a choices file if none exists
if not os.path.exists('choices'):
    o = open('choices', 'w')
    o.write('# null choices file created by simplelayfoundations.py\n')
    o.close()

# create a null source.mk to stop make grumbling!

open('mk/source.mk', 'w').close()



sys.stderr.write('Build tree succesfully initialised: root %s platform %s target %s arch %s\n' % (os.getcwd(), platform, target, arch))


