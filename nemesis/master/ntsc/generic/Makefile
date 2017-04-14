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
#	./ntsc/generic
# 
# DESCRIPTION:
#
#	NTSC common to all architectures
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../..

CFILES  = kprintf.c string.c readnexus.c crash.c console.c

CFILES_alpha     = atropos.c
CFILES_arm       = atropos.c
CFILES_intel     = atropos.c

LOCAL_DERIVEDS   = kident.c

include $(ROOT)/mk/rules.mk

all:: NTSCgeneric

NTSCgeneric: $(OBJS)
	$(RM) NTSCgeneric
	$(LD) -r -o NTSCgeneric $(OBJS)
