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
#	sys/DomainMgr
# 
# DESCRIPTION:
#
#	
#
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
#

ROOT		 = ../..

MODULE		 = Domains

CFILES		 = DomainMgr.c Binder.c Plumber.c security.c
CFILES_alpha	 = sched.c sched_alpha.c
CFILES_intel	 = sched.c sched_intel.c
CFILES_intel-smp = sched_intel-smp.c
CFILES_arm	 = sched.c sched_arm.c

include $(ROOT)/mk/rules.mk

