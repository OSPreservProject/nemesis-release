#! /usr/bin/env python

# Nemesis tree regenerate_package
#
# Copyright 1999 University of Cambridge
#
#
# generates a new set of package description records
# based on the contents of the blueprint and the build
# tree.
#
# possibly also generates a bunch of package directories

import treeinfo
import customizer
import sys, os, buildutils
from nemclasses import *

(items,_) = buildutils.merge_all([treeinfo.build_tree_dir+'/glue/packages', treeinfo.source_tree_dir+'/glue/packages'], 'items')

class Finder:
    def __init__(self, surpressbackups, root = './'):
        self.root = root
        self.files = {}
        self.symlinks = {}
        self.danglies = {}
        self.directories = {}
        self.vdirectories = {}
        self.surpressbackups = surpressbackups
        os.path.walk(root, self.visit, None)
    def visit(self, bogus, dirname, names):
        dirname = dirname[len(self.root):]
        if dirname == '': dirname = '/'        
        self.directories[dirname] = names
        sys.stderr.write('.')
        vnames = []
        for name in names:
            if self.surpressbackups and (name[-1] == '~' or name[-4:] in [ '.pyc', '.bak']) : continue
            vnames.append(name)
            if dirname:
                filename = dirname + '/' + name
            else:
                filename = name
            if os.path.isdir(filename): pass
            islink = os.path.islink(filename)
            isfile = os.path.isfile(filename)
            if islink and isfile: self.symlinks[filename] = 1
            if (not islink) and isfile: self.files[filename] = 1
            if islink and (not isfile): self.danglies[filename] = 1
        self.vdirectories[dirname] = vnames
    def newdirs(self, original):
        l = []
        directories = self.directories.keys()
        directories.sort()
        for directory in directories:
            if not os.path.isdir(original + '/'+ directory):
                l.append(directory)
        return l

cust = customizer.Customizer(items, donottouch=1)
cust.interpret(treeinfo.build_tree_dir+'/choices')
items = cust.getitems()

packages = {}

core_package = 'ccore'

print 'Building up file and directory list'

finder = Finder(1, root = treeinfo.source_tree_dir)

for item in items:
    if not item.options.has_key('package'):
        item.options['package'] = core_package
    
    if item.options['package']:
        package = item.options['package']
        newfiles = {}
        if item.options.has_key('path'):
            path = item.options['path']
            recurse = item.is_grow_recursive()
            if not finder.vdirectories.has_key('/'+path) and not path == '':
                print 'Item',item.name,'dir',path,'missing!'
                package = None
            else:
                dirslist = ['/'+path]
                if recurse:
                    for dir in finder.directories.keys():
                        if dir[1:len(path)+1] == path:
                            subpath = dir[len(path)+1:]
                            if subpath != '' and subpath[0] == '/':
                                dirslist.append(dir)
                for dir in dirslist:
                    if len(finder.vdirectories[dir])>0:
                        newfiles[dir[1:]] = []                        
                        for thingy in finder.vdirectories[dir]:
                            if os.path.isfile(treeinfo.source_tree_dir+'/'+dir[1:]+'/'+thingy):
                                newfiles[dir[1:]].append(thingy)

                item.options['contents'] = newfiles
        if package:
            if not packages.has_key(package):
                packages[package] = { 'items': [], 'files' : {} }
            packages[package]['items'].append(item)
            for dir in newfiles.keys():
                packages[package]['files'][dir] = newfiles[dir]


package_list = packages.keys()
package_list.sort()

print 'Packages',package_list
buildutils.maybemakedir('new_packages')

if 1:
    movedout = {}
    olddirs = []
    o = open('creation', 'w')
    
    for package in package_list:
        if package != core_package:
            for dir in packages[package]['files'].keys():
                for file in packages[package]['files'][dir]:
                    o.write('remove("moved to '+package+'", "'+dir+'/'+file+'")\n')
                    movedout[dir+'/'+file] = package
                olddirs.append((package,dir))
    for (npackage,directory) in olddirs:
        if not packages[core_package]['files'].has_key(directory):
            o.write('rmdir("moved to '+npackage+'", "'+directory+'")\n')
    o.close()
            
for package in package_list:
    print 'Package',package
    print len(packages[package]['files'].keys()),'directories'
    count = 0
    for dir in packages[package]['files'].keys():
        count = count + len(packages[package]['files'])
    #print 'Files',packages[package]['files']
    print count,'files'
    print len(packages[package]['items']),'items'

        
        
    print 'Generating glue/new_packages/'+package+'.py'
    o = open('new_packages/'+package+'.py', 'w')
    o.write("""
############################################################################
# Package database for """+package+"""
#
# This may look like source code, but it is actually the build system
# database format. You may edit this file, but only semantic changes will
# be kept the next time this file is automatically regenerated.
#

from nemclasses import *

items = []
""")

    sections = {}
    db = {}
    keylist = {}
    for item in packages[package]['items']:
        optcopy = item.options
        if not optcopy.has_key('section'): optcopy['section'] = 'unclassified'
        if not(sections.has_key(optcopy['section'])):
            sections[optcopy['section']] = []
        sections[optcopy['section']].append(item.name)
        for key in optcopy.keys():
            keylist[key] = 1
        if db.has_key(item.name):
            print 'Badness! on ',item.name
        db[item.name] = item
        item.noptions = optcopy

    keylistsorted = keylist.keys()
    keylistsorted.sort()
    for key in keylistsorted:
        o.write(key+' = '+`key`+'\n')

    sectionsorted = sections.keys()
    sectionsorted.sort()
    for section in sectionsorted:
        sections[section].sort()
        o.write('\n\n\n#######################################################################\n')
        o.write('# '+section+' ('+`len(sections[section])`+' items)\n\n\n')
        o.write('items = items + [\n')
        for itemname in sections[section]:
            item = db[itemname]
            item.options = item.noptions
            o.write( `item`+',\n\n')
        o.write(']\n\n')

    o.write("""
######################################################################
""")

    o.write('files = {\n')
    dirs = packages[package]['files'].keys()
    dirs.sort()
    for dir in dirs:
        files = packages[package]['files'][dir]
        files.sort()
        o.write('\t'+`dir`+' : '+`files`+',\n')
    o.write('}\n')
    o.close()

    if len(sys.argv) >= 2:
        if len(sys.argv) == 3:
            buildutils.system('mkdir -p '+sys.argv[1]+'/'+package+'/master')
            buildutils.system('(cd '+sys.argv[1]+'/'+package+' && prcs checkout -r '+sys.argv[2]+'.@ '+package+')')
        targetdir = sys.argv[1]+'/'+package+'/master'
        print targetdir
        #buildutils.maybemakedir(targetdir)
        for dir in packages[package]['files'].keys():
            if dir == 'glue/packages': continue
            print '\t'+dir
            buildutils.system('mkdir -p '+targetdir+'/'+dir)
            for file in packages[package]['files'][dir]:
                if os.path.isfile(treeinfo.source_tree_dir+'/'+dir+'/'+file):
                    buildutils.copy(
                        treeinfo.source_tree_dir+'/'+dir+'/'+file,
                        targetdir+'/'+dir+'/'+file)
        buildutils.system('mkdir -p '+targetdir+'/glue/packages')
        buildutils.copy(
            treeinfo.build_tree_dir+'/glue/new_packages/'+package+'.py',
            targetdir+'/glue/packages/'+package+'.py'
            )
        






