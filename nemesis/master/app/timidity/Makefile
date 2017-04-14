#########################################################################
#                                                                       #
#  Copyright 1997, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#	./app/test/timidity
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:19:27 +0000 dr10009 $
#

ROOT	= ../..
LOCAL_CFLAGS = -DLAB
PERSONALITY_LIBS = $(ROOT)/lib/static/posix/libposix.a

PROGRAM 	:= timidity

CFILES		:= common.c controls.c filter.c instrum.c mix.c nemesis_a.c nemesis_c.c nemmain.c \
		   output.c playmidi.c readmidi.c readppm.c resample.c tables.c timidity.c

include $(ROOT)/mk/local.rules.mk
include $(ROOT)/mk/rules.mk


