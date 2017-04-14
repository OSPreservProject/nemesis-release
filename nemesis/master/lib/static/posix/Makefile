#########################################################################
#                                                                       #
#  Copyright 1994, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#	./lib/statics/system
# 
# DESCRIPTION:
#
#	Objects that are offered for statically linking against all
#	MODULEs and PROGRAMs in addition to veneers
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../../..


# common C files:
CFILES = posixmain.c

include $(ROOT)/mk/rules.mk

all:: libposix.a

clean:: FRC
	$(RM) libposix.a

libposix.a: $(OBJS)
	$(RM) $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@
