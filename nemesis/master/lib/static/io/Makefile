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
#	./lib/io
# 
# DESCRIPTION:
#
#	Objects that are offered for statically linking against all
#	MODULEs and PROGRAMs in addition to veneers
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../../..

ASFILES_ix86 = $(wildcard *_ix86.S)
ASFILES_eb164 = $(wildcard *_alpha.S)
ASFILES_eb64 = $(wildcard *_alpha.S)

CFILES_ix86 = io.c
CFILES_eb164 = io.c
CFILES_eb64 = io.c
CFILES_arm = io.c
CFILES_shark = io.c

include $(ROOT)/mk/rules.mk

all:: libio.a

clean:: FRC
	$(RM) libio.a

libio.a: $(OBJS)
	$(RM) -f $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@
