#
# Use make -i -k etags
#
DERIVED_DEF     := $(INTERFACES:.if=.def.c)

DERIVED_MC      := $(MARSHAL_INTFS:%=IDC_M_%.c)
CUSTOM_MC       := $(CUSTOM_MARSHAL_INTFS:%=IDC_CM_%.c) 

DERIVED_SC      := $(STUBS_INTFS:%=IDC_S_%.c)
CUSTOM_SC       := $(CUSTOM_STUBS_INTFS:%=IDC_CS_%.c) 

ALLCFILES       := $(DERIVED_DEF) \
		     $(DERIVED_MC) $(CUSTOM_MC) \
		     $(DERIVED_SC) $(CUSTOM_SC) \
		     $(CFILES) $(CFILES_$(ARCH)) $(CFILES_$(MC))

ALLASFILES      := $(ASFILES) $(ASFILES_$(ARCH)) $(ASFILES_$(MC))

ALLSUBDIRS := $(SUBDIRS) $(SUBDIRS_$(ARCH)) $(SUBDIRS_$(MC))

etags hetags ahetags::

etags:: FRC
	etags -a $(ALLCFILES) $(ALLASFILES) $(wildcard *.h) $(wildcard *.ih) -o $(ROOT)/TAGS

cetags:: FRC
	etags -a $(ALLCFILES) $(ALLASFILES) -o $(ROOT)/CTAGS

hetags:: FRC
	etags -a $(wildcard *.h) $(wildcard *.ih) -o $(ROOT)/HTAGS

ifneq ($(strip $(ALLSUBDIRS)),)
  etags hetags cetags:: FRC
	@for d in $(ALLSUBDIRS); do \
	  ( echo ; echo '####' $(CURRENT_DIR)/$$d $@ '####'; echo; \
	   cd $$d; $(MAKE) $@; exit $$? ) || exit $$?; \
	done
endif #ALLSUBDIRS

