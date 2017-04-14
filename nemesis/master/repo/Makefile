
#########################################################################
#                                                                       #
#  Copyright 1998, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#	./repo
# 
# DESCRIPTION:
#
#	Interface repository
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

#
# Note; this makefile is only concerned with building interfaces found in this
# directory. It builds all .if files in the directory, and treats them as source.
# 
# The interface files themselves are symlinks created by 
# $(ROOT)/glue/plumbtree.py. So, if the right symlinks aren't here then make
# in this directory won't fix it.
#

#
# Note that $(ROOT)/glue/intfbuilder.py is used to do most of the building
# here. It doesn't work like traditional makes do; rather, it maintains a 
# cache of the metadata in the file intfbuilder.cache. So, if things get
# confused, delete this file to cause it to assume it knows nothing about
# the tree, and therefore should rebuild everything.
#

#
# To see tracing of the actions of intfbuilder, set INTFBUILD_VERBOSE to
# verbose

ROOT = ..

include $(ROOT)/mk/rules.mk


all:: 
	PYTHONPATH=`pwd`/../glue python ../glue/createiflinks.py repo
	PYTHONPATH=`pwd`/../glue python ../glue/intfbuilder.py repo $(INTFBUILD_VERBOSE)

IntfTypes: IntfTypes.o
	$(LD) $(LDFLAGS) -o IntfTypes IntfTypes.o


clean ::
	$(RM) *.ast *.def.c *.ih *.if intfbuilder.cache typecodes.map


