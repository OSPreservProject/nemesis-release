# Launcher makefile

# build item name launcher

ROOT = ../..
CFILES = launcher.c buttons_gui.c
PERSONALITY_LIBS = $(ROOT)/lib/static/posix/libposix.a
PROGRAM = launcher

include $(ROOT)/mk/autoconf.mk
include $(ROOT)/mk/rules.mk
ifdef INSTDIR
  install::
	cp -f buttons.ppm $(INSTDIR)

  installmost:: $(INSTDIR)/buttons.ppm

  $(INSTDIR)/buttons.ppm: buttons.ppm
	cp -f buttons.ppm $(INSTDIR)
endif

buttons_gui.c buttons_gui.h: launcher.gui.py $(ROOT)/glue/guicompiler.py
	PYTHONPATH=$(ROOT)/glue python launcher.gui.py

launcher.c: buttons_gui.h

clean::
	$(RM) buttons_gui.*
