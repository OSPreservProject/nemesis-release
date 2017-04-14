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
#      dev/pci/s3
# 
# DESCRIPTION:
#
#      S3 Framebuffer
# 
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
# 

ROOT=../../..

SUBDIRS=xrip

CFILES = s3.c blit_16.c
ASFILES_eb164	= stub.S

EXTRA_OBJDIR = .
EXTRA_OBJS = xrip/xripbase.o xrip/commonhw/common_hw.o xrip/s3/s3lump.o xrip/hc/HC.o $(ROOT)/lib/static/io/libio.a

PROGRAM = fbs3

include $(ROOT)/mk/rules.mk

