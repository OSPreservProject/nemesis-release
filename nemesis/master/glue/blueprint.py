# Nemesis tree blueprint
#
# Copyright 1998 University of Cambridge
#
# code to read the packages files and incorporate
# the changes described in choices, resulting in a specialized
# tree.
import treeinfo
import customizer
import sys, string, os, buildutils
from nemclasses import *

dirs = [treeinfo.build_tree_dir+'/glue/packages']
if treeinfo.source_tree_dir:
    dirs.append( treeinfo.source_tree_dir+'/glue/packages')

(items, deps) = buildutils.merge_all(dirs,'items')

cust = customizer.Customizer(items)
cust.deps = cust.deps + deps
cust.init(treeinfo.arch_name, treeinfo.platform_name, treeinfo.target_name)
cust.interpret(treeinfo.build_tree_dir+'/choices')

cust.makeconsistent(1)
items = cust.getitems()
db = cust.getdb()
searchpath = cust.getpath()
deps = cust.deps
