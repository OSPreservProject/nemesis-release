#########################################################################
#                                                                       #
#  Copyright 1994, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# FILE:
#
#	intel-smp.tgt.mk
# 
# DESCRIPTION:
#
#	This file contains definitions specific to the intel SMP
#	target machine.  It is included from
#	rules.mk via the link "target.mk".
# 
# ID : $Id: intel-smp.tgt.mk 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
# 

MC		= intel-smp

#
# Target-machine-specific compiler options:
#
MC_CFLAGS	= -D__LANGUAGE_C__ -D__LANGUAGE_C -D__SMP__
MC_ASFLAGS	= -D__ASSEMBLY__ -D__SMP__
MC_DEF		= -DINTEL_SMP
MC_INC		=

CAPITALASM = 1

include $(ROOT)/mk/ix86_target.mk

# This is a ghastly hack until such times as linux NFS and/or gas is
# fixed.

ifdef NFSHACK

%.o %.c.d: %.c

# Note that $@ appears to expand to the alphabetically first thing in
# front of the colon (i.e. %.c.d) so instead we use $*.o

%.o %.c.d: %.c
	( $(RM) /tmp/nemhack$$$$.o; \
	$(CC) -MD $(CFLAGS) $(DEF) $(INC) -o /tmp/nemhack$$$$.o -c $< \
	&& cp /tmp/nemhack$$$$.o $*.o \
	&& $(RM) /tmp/nemhack$$$$.o \
	|| ($(RM) /tmp/nemhack$$$$.o $*.o \
	&& exit 1))
	$(RM) $*.c.d
	$(MV) $*.d $*.c.d

%.o : %.S
	( $(RM) /tmp/nemhack$$$$.o; \
	$(CC) $(ASFLAGS) $(DEF) $(INC) -o /tmp/nemhack$$$$.o -c $< \
	&& cp /tmp/nemhack$$$$.o $*.o \
	&& $(RM) /tmp/nemhack$$$$.o \
	|| ($(RM) /tmp/nemhack$$$$.o $*.o \
	&& exit 1))
endif
# so we can configure other things
TARGET_CONFIG_DAT=intel-smp.dat
