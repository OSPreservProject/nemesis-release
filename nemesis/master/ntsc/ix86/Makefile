######################################################################
# ntsc/ix86/Makefile, Copyright 1998 Univeristy of Cambridge CompLab
######################################################################
# DIRECTORY: ntsc/ix86
#
#
ROOT=../..
include $(ROOT)/mk/autoconf.mk
SUBDIRS = 
ifdef CONFIG_INTEL_TARGET
SUBDIRS += intel

NTSCix86:
	ln -s $(ROOT)/ntsc/ix86/intel/NTSCix86 NTSCix86

endif
ifdef CONFIG_INTEL_SMP_TARGET
SUBDIRS += intel_smp

NTSCix86:
	ln -s $(ROOT)/ntsc/ix86/intel/NTSCix86 NTSCix86
endif
include $(ROOT)/mk/rules.mk

all:: NTSCix86
