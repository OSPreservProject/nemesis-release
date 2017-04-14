#
# nemmaster configutils
#
# Copyright 1998 Dickon Reed
#
#
# A few support routines, to do with the configuration system
#

import sys, nemclasses, string

from treeinfo import build_tree_dir

def getactivesubset(items):
    happyitems= []
    unhappyitems = []
    for item in items:
	if item.isactive():
	    happyitems.append(item)
	else:
	    unhappyitems.append(item)
    happyitemsc = map(lambda x:x.get_name(), happyitems)
    happyitemsc.sort()
    #sys.stderr.write('(active items: '+`happyitemsc`)

    unhappyitemsc = map(lambda x:x.get_name(), unhappyitems)
    unhappyitemsc.sort()

    #sys.stderr.write(')\n(inactive items: '+`unhappyitemsc`+')\n')
    return happyitems

    
