##########################################################################
#                                                                       #
#  Copyright 1994, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# DIRECTORY:
#
#	sys/memory
# 
# DESCRIPTION:
#
#	Makefile for central memory code (virtual & physical memory 
#       allocators, high-level translation [viz MMU.if], etc). 
#

ROOT		= ../..

MODULE		= memory

include $(ROOT)/mk/autoconf.mk

ifeq (${CONFIG_MEMSYS_EXPT},y) 
MEMSYS	= expt
ifeq (${CONFIG_PTAB_GPT},y) 
PTAB	= gpt
else 
PTAB    = linear
endif
endif

ifeq (${CONFIG_MEMSYS_STD},y) 
MEMSYS	= std
PTAB	= 121
endif

CFILES		= salloc_$(MEMSYS).c frames.c
CFILES_eb164    = eb164_mmu_$(PTAB).c
CFILES_intel	= intel_mmu.c
CFILES_srcit	= srcit_mmu.c 
CFILES_shark	= shark_mmu.c ofw.c

include $(ROOT)/mk/rules.mk

