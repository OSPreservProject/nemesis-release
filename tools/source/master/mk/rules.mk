#
# This file contains platform independent make rules for a
# Nemesis TOOLS tree.  Every Makefile defines ROOT relative to itself,
# then includes this file as $(ROOT)/mk/rules.mk AFTER defining its
# local values.
#

#
# "all" is the default target, unless there is an earlier target in
# the including Makefile.
#

all::

#
# Include make definitions configured per platform.
#
include $(ROOT)/mk/platform.mk		# defines CC etc.


#
# Directories
#
INSTDIR	= $(ROOT)/../install/$(ARCH)


#
# Dependency generators (deduced from compilers unless explicitly specified)
#
ifndef CDEPEND
  CDEPEND := $(CC) -M
endif
ifndef SDEPEND
  SDEPEND := $(CC) -M
endif


#
# Rules:
#
%.i: %.c
	$(CPP) $(DEF) $(INC) $< > $@

%.i: %.s
	$(CPP) $(DEF) $(INC) $< > $@

%.o: %.c
	$(CC) $(CFLAGS) $(DEF) $(INC) -c $<

%.s: %.c
	$(CC) $(CFLAGS) $(DEF) $(INC) -S $<

%.o: %.s
	$(AS) $(DEF) $(INC) $< -o $@


#
# Per-directory values:
#

OBJS	:= $(CFILES:.c=.o)  $(SFILES:.s=.o) $(LOCAL_OBJS) \
	   $(addprefix $(EXTRA_OBJDIR)/,$(EXTRA_OBJS))

INSTALLS:= $(INSTALL_BINS) $(INSTALL_PYS)

#
# Generate and include dependencies, if any.
# (see "Generating Dependencies Automatically" in the gnumake manual)
#
%.c.d: %.c
	$(CDEPEND) $(INC) $(DEF) $< | $(SED) 's/$*.o/& $*.c.d/g' > $@

%.s.d: %.s
	$(SDEPEND) $(INC) $(DEF) $< | $(SED) 's/$*.o/& $*.s.d/g' > $@

DEPS := $(addsuffix .d, $(wildcard $(CFILES) $(SFILES) $(LOCAL_DEPS)))

ifneq ($(strip $(DEPS)),)
  include $(DEPS)
endif


#
# Standard targets in the current directory:
#

all:: $(INSTALLS)

ifneq ($(strip $(INSTALL_BINS)),)

  install:: $(INSTALL_BINS)
	-mkdir -p $(INSTDIR)/bin
	-mkdir -p $(INSTDIR)/python
	@for i in $(INSTALL_BINS); do echo $$i; done | \
	while { read i || exit 0 ; } \
	&& echo $(INSTALL) -c $$i $(INSTDIR)/bin \
	&& rm -f $(INSTDIR)/bin/$$i \
	&& $(INSTALL) -c $$i $(INSTDIR)/bin ; \
	do true; done

endif # INSTALL_BINS

ifneq ($(strip $(INSTALL_PYS)),)

  install:: $(INSTALL_PYS)
	@for i in $(INSTALL_PYS); do echo $$i; done | \
	while { read i || exit 0 ; } \
	&& echo $(INSTALL) -c $$i $(INSTDIR)/python \
	&& rm -f $(INSTDIR)/python/$$i \
	&& $(INSTALL) -c $$i $(INSTDIR)/python ; \
	do true; done

endif # INSTALL_PYS

clean::
	$(RM) *.i core *~ *.o *.d *.bak *.sym *.dis

#
# Standard recursive targets in the subdirs, if any:
#
ALLSUBDIRS := $(SUBDIRS) $(SUBDIRS_$(ARCH)) $(SUBDIRS_$(MC))

ifneq ($(strip $(ALLSUBDIRS)),)

all install clean::
	@for i in $(ALLSUBDIRS); do echo $$i; done | \
	while { read i || exit 0 ; } \
	&& echo "making $@ in $(CURRENT)/$$i..." \
	&& (cd ./$$i && $(MAKE) $@) \
	|| exit 1; \
	do :; done

endif	# ALLSUBDIRS
