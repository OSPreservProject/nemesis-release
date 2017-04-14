#########################################################################
#                                                                       #
#  Copyright 1996, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#	./lib/static/socket
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
#

ROOT		= ../../..

CFILES		= socket.c 

include $(ROOT)/mk/rules.mk

libsocket.a: $(OBJS)
	$(RM) -f $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

all::   libsocket.a

