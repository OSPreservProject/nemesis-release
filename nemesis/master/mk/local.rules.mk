#########################################################################
#                                                                       #
#  Copyright 1997, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# FILE:
#
#	local.rules.mk
# 
# DESCRIPTION:
#
#	This file contains local make rules for different sites' use
#	of the Nemesis tree.  Every Makefile that's potentially
#	affected by one of these local rules defines ROOT relative to
#	itself, then:
#	- defines its local values
#	- includes the generic rules file as $(ROOT)/mk/rules.mk
#	- includes this file as $(ROOT)/mk/local.rules.mk
#
#	It is possible that rules defined herein will need to be
#	redefined at other sites
#	
# 
# ID : $Id: local.rules.mk 1.1 Thu, 18 Feb 1999 14:16:19 +0000 dr10009 $
# 

MAKEFILE_DEPS += $(ROOT)/mk/local.rules.mk

#
# TeX-related rules; the Cambridge defaults here relate to web2C 6.1,
# without application of the kpathsea patch.  The rules as expressed
# say "prefix the TeX and BibTeX input paths by the directory
# $(ROOT)/doc/inputs"

# A site using a (very old) Unix TeX, that doesn't use kpathsea at
# all, is "on its own".  A site that has installed the kpathsea patch,
# or that uses teTeX (e.g. from the Live TeX CD) may well need to set
# a variable TEXINPUTS.latex (or something of the sort) instead of
# plain TEXINPUTS

export TEXINPUTS = $(ROOT)/doc/inputs:
export BIBINPUTS = $(ROOT)/doc/inputs:

# This is needed for bibtex on linux, I know not why.
export BSTINPUTS = $(ROOT)/doc/inputs:


# Local network environment configuration

# put c0:ff:fe on the end of bootp requires
DEFINES_mod/net/flowman += -DIDENTIFY_THYSELF

# ugly data stuff

DEFINES_app/timidity += -DTIMDIR=\"/usr/groups/pegasus/misc/demos/timidity-data\"

