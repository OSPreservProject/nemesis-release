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
#      ntsc/ix86/intel
# 
# DESCRIPTION:
#
#      Intel NTSC
# 
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 15:09:39 +0000 dr10009 $
# 

ROOT		= ../../..

LDLUMP		= NTSCix86

CFILES		= i386-stub.c stub-support.c kernel.c \
		mmgmt.c syscalls.c timer_rdtsc.c timer.c local.c serial.c

ASFILES 	= khead.S idt.S gdb-asm.S

LOCAL_OBJS 	= ../../generic/NTSCgeneric $(LIBGCCA)

LOCAL_CFLAGS	= -D__ELF__

include $(ROOT)/mk/rules.mk

LDFLAGS		= -n -Ttext 100000
