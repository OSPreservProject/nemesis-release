
######################################################################
#                                                                    #
# Makefile for init.rc preprocessing                                 #
#                                                                    #
######################################################################
#
# Copyright 1998 Dickon Reed

ROOT = ../..
include $(ROOT)/mk/autoconf.mk
include $(ROOT)/mk/rules.mk


# the init.rc rule comes from metadata.mk
all:: init.rc standard.rc

clean::
	$(RM) init.rc standard.rc


init.rc: init.rc.py
	PYTHONPATH=.:$(ROOT)/glue python $< > $@ || rm $@

standard.rc: standard.rc.py
	PYTHONPATH=.:$(ROOT)/glue python $< > $@ || rm $@

-include init.rc.d
-include standard.rc.d


