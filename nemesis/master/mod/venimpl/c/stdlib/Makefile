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
#	lib/c/stdlib
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../../../..

LDLUMP		= stdlib

CFILES_mips	= atof.c strtod.c	# alphas don't like FP yet
CFILES_intel	= atof.c strtod.c

CFILES		= $(filter-out $(CFILES_mips), $(wildcard *.c))

include $(ROOT)/mk/rules.mk

