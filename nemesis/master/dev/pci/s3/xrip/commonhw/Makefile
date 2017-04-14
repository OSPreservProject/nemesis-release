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
#      dev/pci/s3/commonhw
# 
# DESCRIPTION:
#
#      S3 framebuffer
# 
# ID : $Id: Makefile 1.1 Thu, 18 Feb 1999 14:18:52 +0000 dr10009 $
# 
ROOT = ../../../../..

LOCAL_INC = -I ../h
CFILES = IBMRGB.c Ti3026clk.c xf86_PCI.c ATTDac.c Ch8391clk.c CirrusClk.c I2061Aalt.c I2061Acal.c I2061Aset.c ICS2595.c S3gendac.c SC11412.c STG1703clk.c Ti3025clk.c
ASFILES_ix86 = ${wildcard *.S}
CFILES_alpha = BUSmemcpy.c IODelay.c SlowBcopy.c
 
LDLUMP = common_hw.o

include $(ROOT)/mk/rules.mk

