# nemmaster geninfrastructure
#
# Copyright 1998 Dickon Reed
#

import configutils, blueprint, buildutils, treeinfo, sys, os, posixpath
import socket, string, posixpath
	

def install(buildpath, subpath, instname):
    dest = buildpath + '/links/'+instname
    if posixpath.exists(dest):
	print '\nWarning; not overwriting file',dest,'with symlinks'
	return
    try:
	os.symlink('../'+subpath+'/'+instname, dest)
    except:
	print 'Error creating symlink',dest,'pointing to','../'+subpath+'/'+instname
    sys.stdout.write(instname+',')
    sys.stdout.flush()


buildpath = treeinfo.build_tree_dir
happyitems = configutils.getactivesubset(blueprint.items)

mapping = {}
mapping['image'] = 'boot/images/'+treeinfo.target_name
forbidden = []
for item in happyitems:
    if item.get_path() and item.get_binary_object_name():
	instname = ''

	if item.get_name() == 'boot_images_'+treeinfo.target_name:
	    instname = 'image'
	if item.get_class() in ['program', 'posixprogram', 'module']:
	    instname = item.get_binary_object_name()
	if item.get_make_flag('donotinstall'):
	    instname = ''
	if instname != '':
	    if item.options['value'] == 2:
		mapping[instname] = item.get_path()
	    else:
		forbidden.append(instname)

	if item.get_make_flag('install'):
	    for instname in item.get_make_flag('install'):
		mapping[instname] = item.get_path()

print 'Omitting ',forbidden
print 'Linking ',len(mapping.keys())	

files = os.listdir('.')

for file in files:
    if posixpath.islink(file) and file != 'Makefile' and (file in forbidden or file in mapping.keys()):
	os.unlink(file)

for file in mapping.keys():
    install(buildpath, mapping[file], file)

print

