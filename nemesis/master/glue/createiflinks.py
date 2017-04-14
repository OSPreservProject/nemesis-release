#! /usr/bin/env python

# Copyright 1998 Dickon Reed

# create installed interface links, removing all interfaces

import configutils, buildutils, blueprint, string, os, sys

happyitems = configutils.getactivesubset(blueprint.items)
from treeinfo import build_tree_dir, source_tree_dir

repolocmap = { 'sys' : build_tree_dir+'/repo',
	       'pub' : build_tree_dir+'/repo'
	       }
newintfs = { 'sys': {}, 'pub': {} }

for item in happyitems:
    path = item.get_path()
    if path:
	for intf in item.get_system_interfaces():
	    intfparts = string.split(intf, '.')
	    newintfs['sys'][intfparts[0]] = (item,intf)
	for intf in item.get_public_interfaces():
	    intfparts = string.split(intf, '.')
	    newintfs['pub'][intfparts[0]] = (item,intf)

# ignore pub for the time being

contents = newintfs['sys']
l = os.listdir(build_tree_dir+'/repo')
for file in l:
    if len(file)>3 and file[-3:] == '.if' and not (contents.has_key(file[:-3])):
	os.unlink(build_tree_dir+'/repo/'+file)
	sys.stdout.write('X')
for interface in contents.keys():
    dest = None
    (item, ifname) = contents[interface]

    try:
	os.unlink(build_tree_dir+'/repo/'+interface+'.if')
    except os.error:
	# can't have existed- so what...
	pass

    os.symlink('../'+item.get_path()+'/'+ifname, build_tree_dir+'/repo/'+ifname)
