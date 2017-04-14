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
#	./lib
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ..
include $(ROOT)/mk/autoconf.mk

SUBDIRS		= veneer static

ifdef CONFIG_XOPEN_LIB
SUBDIRS += xopen
endif 

include $(ROOT)/mk/rules.mk

