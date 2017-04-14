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
#	lib/nemesis/Stubs
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.2 Thu, 18 Feb 1999 14:17:29 +0000 dr10009 $
#

ROOT		:= ../../..
MODULE		:= Stubs

CUSTOM_MARSHAL_INTFS :=

include $(ROOT)/mk/config.mk

MARSHAL_INTFS	:= \
#Time Thread Domain ProtectionDomain Event Channel \
#Binder BinderCallback Type Heap Context \
#IDC IDCOffer IDCClientBinding Gatekeeper \
#DomainMgr VP Activation WordTblIter WordTbl Interrupt \
#File IO Stretch StretchAllocator Serial Rd Wr\
$(sort $(CONFIG_MARSHAL))

STUBS_INTFS := \
#Binder BinderCallback DomainMgr Context Interrupt Rd Wr \
#StretchAllocator Serial \
$(sort $(CONFIG_STUBS))

CUSTOM_STUBS_INTFS := ConsoleWr

CUSTOM_MARSHAL_INTFS :=

CFILES := stubs.c stubslist.c

LOCAL_DERIVEDS := stubslist.c

include $(ROOT)/mk/rules.mk

stubslist.c: genstubs $(ROOT)/mk/autoconf.mk
	sh ./genstubs $(STUBS_INTFS) $(CUSTOM_STUBS_INTFS) > stubslist.c ||\
	{ rm -f stubslist.c ; exit 1 ; } 

clean ::
	$(RM) typecodes.map


Stubs: Makefile
