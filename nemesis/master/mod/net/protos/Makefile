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
#	./net/Protos
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
#

ROOT	= ../../..

SUBDIRS	=

MODULE = Protos

include $(ROOT)/mk/autoconf.mk

ifdef CONFIG_PROTO_COMMON_BUILD
LOCAL_OBJS = ether/Ether ip/IP udp/UDP
SUBDIRS += ether ip udp
endif

include $(ROOT)/mk/rules.mk

