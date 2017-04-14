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
#	./lib/statics/system
# 
# DESCRIPTION:
#
#	Objects that are offered for statically linking against all
#	MODULEs and PROGRAMs in addition to veneers
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT	= ../../..


# setjmp might want to move to libc.a in order to be overridden when
# we want to start using floating point regs.

ASFILES_intel = $(wildcard *_intel.S)
ASFILES_intel += $(wildcard *_ix86.S)
CFILES_intel = $(wildcard *_intel.c)
CFILES_intel += $(wildcard *_ix86.c)

ASFILES_intel-smp = $(wildcard *_intel-smp.S)
ASFILES_intel-smp += $(wildcard *_ix86.S)
CFILES_intel-smp = $(wildcard *_intel.c)
CFILES_intel-smp += $(wildcard *_ix86.c)

ASFILES_alpha = $(filter-out crt0_alpha.S, $(wildcard *_alpha.S))
ASFILES_arm   = $(filter-out crt0_arm.S, $(wildcard *_arm.S))
CFILES_arm = $(wildcard *_arm.c) main.c

# common C files:
CFILES =

include $(ROOT)/mk/rules.mk

all:: libsystem.a crt0.o

clean:: FRC
	$(RM) libsystem.a crt0.S

crt0.S: crt0_$(ARCH).S
	$(LN) $< $@

libsystem.a: $(OBJS)
	$(RM) -f $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@
