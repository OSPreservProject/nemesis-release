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
#	./lib/c
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../../..

CFILES = jmptbl.c

SUBDIRS	= str stdio stdlib

include $(ROOT)/mk/rules.mk

all:: libc

ofiles = jmptbl.o str/str stdio/stdio stdlib/stdlib \
		$(ROOT)/lib/static/system/libsystem.a

libc: $(ofiles)
	$(LD) $(LDFLAGS) -o libc $(ofiles) $(LIBGCCA)
