#########################################################################
#                                                                       #
#  Copyright 1998, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#      dev/pci/mga
# 
# DESCRIPTION:
#
#      Device driver for PMAGBBA framestore
# 
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
# 

PROGRAM = fbmga

SUBDIRS = mga_hw common_hw

ROOT = ../../..

CFILES     = $(wildcard *.c)
ASFILES_eb164   = stub.S


EXTRA_OBJDIR = .
EXTRA_OBJS   = mga_hw/mga_hw.o common_hw/common_hw.o $(ROOT)/lib/static/io/libio.a

include $(ROOT)/mk/rules.mk
