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
#	./mod/clanger
# 
# DESCRIPTION:
#
#	Clanger command line
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT       = ../..

include $(ROOT)/mk/config.mk

ifdef CONFIG_CLANGER_BUILD
MODULE    = clanger

CFILES     = Clammar.tab.c $(filter-out Clammar.tab.c, $(wildcard *.c))

LOCAL_DERIVED_H = Clammar.tab.h
endif

include $(ROOT)/mk/rules.mk

ifdef CONFIG_CLANGER_BUILD

Clammar.tab.c Clammar.tab.h: Clammar.y
	BISON_SIMPLE=bison.nemesis $(BISON) -d $<

date.gen.c: 
	echo "const char * const cline_date = \"`date`\";" > date.gen.c
	$(CC) -c date.gen.c

endif
