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
#	sys/PCI
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../..

MODULE		= PCI

CFILES_ix86	= intel_bios32.c
CFILES_alpha	= alpha_bios32.c
CFILES_eb164    = alcor.c
CFILES		= PCIBiosMod.c pci.c

EXTRA_OBJDIR	= $(ROOT)/lib/static
EXTRA_OBJS 	= io/libio.a

include $(ROOT)/mk/rules.mk

