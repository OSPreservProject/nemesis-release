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
#	lib/c/intmath/ix86
# 
# DESCRIPTION:
#
#	64-bit integer divides for MIPS
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
#

ROOT	= ../../../..

LDLUMP	:= intmath

include $(ROOT)/mk/platform.mk


# If LIBGCCA (defined in platform.mk) exists and is readable, certain integer
# arithmetic routines will be extracted (in .o form) from it and
# used in Nemesis.  Otherwise, these routines will be compiled from
# possibly less efficient sources in lib/c/intmath/mips.


foo := $(strip $(shell test -r $(LIBGCCA) && echo go_for_it))


ifeq ($(foo),go_for_it)

  all:: # is still the default target..

  LOCAL_OBJS := _divdi3.o _moddi3.o _udivdi3.o _umoddi3.o _udivmoddi4.o \
		_ucmpdi2.o _muldi3.o

  $(LOCAL_OBJS) : $(LIBGCCA)
	@echo "+++ USING INTEGER ROUTINES FROM $(LIBGCCA) for $(MODULE)"
	ar xv $< $(LOCAL_OBJS)

else

  CFILES = $(wildcard *.c)

endif

include $(ROOT)/mk/rules.mk






