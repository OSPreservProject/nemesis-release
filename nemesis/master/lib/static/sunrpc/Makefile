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
#	lib/sunrpc
# 
# DESCRIPTION:
#
#	sunrpc implementation
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:19:49 +0000 dr10009 $
#

ROOT	= ../../..

SUBDIRS = h

LOCAL_INC = -I./h

XDR_FILES  = $(wildcard xdr*.c)
RPC_FILES  = $(wildcard rpc*.c)
AUTH_FILES  = $(wildcard auth*.c)
PMAP_FILES  = $(wildcard pmap*.c)
CLNT_FILES  = $(wildcard clnt*.c)
SVC_FILES  = $(wildcard svc*.c)

CFILES = $(XDR_FILES) $(CLNT_FILES) $(RPC_FILES) $(AUTH_FILES) $(PMAP_FILES) $(SVC_FILES)

include $(ROOT)/mk/rules.mk

librpc.a: $(OBJS)
	$(RM) -f $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

all::	librpc.a


