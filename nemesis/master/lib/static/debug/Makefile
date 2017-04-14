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
#	./lib/static/debug
# 
# DESCRIPTION:
#
#	Trivial debugging support library --- currently only contains 
#       some IO routines for debugging tricky parts of code.
#	This library is used by primal for output, and by eprintf().
#
#

ROOT	= ../../..

# common C files:
CFILES = triv_wr.c

include $(ROOT)/mk/rules.mk

all:: libdebug.a

clean:: FRC
	$(RM) libdebug.a

libdebug.a: $(OBJS)
	$(RM) $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@
