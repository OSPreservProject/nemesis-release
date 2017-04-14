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
#	./mod/nemesis/sgen
# 
# DESCRIPTION:
#
#	Automatic IDC Stub generation
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT       = ../../..

MODULE    = stubgen

CFILES     = stubgen.c engine.c canned_stubs.c 
CFILES_ix86 = synthgen.c ix86_synth.c
CFILES_alpha = synthgen.c alpha_synth.c

ASFILES    = $(ARCH)_stubs.S

include $(ROOT)/mk/rules.mk

