##########################################################################
#                                                                       #
#  Copyright 1994, University of Cambridge Computer Laboratory  	#
#                                                                       #
#  All Rights Reserved.						        #
#                                                                       #
#########################################################################
#
# FILE:
#
#	rules.mk
# 
# DESCRIPTION:
#
#	This file contains the make rules for the Nemesis tree.  Every
#	Makefile defines ROOT relative to itself, then includes this file
#	as $(ROOT)/mk/rules.mk AFTER defining its local values.
# 
# ID : $Id: rules.mk 1.5 Wed, 02 Jun 1999 13:55:50 +0100 dr10009 $
# 

#
# "all" is the default target, unless there is an earlier target in
# the including Makefile.
#

all::

#
# the install targets must appear in the subdirs line, so we cant
# simply elimitate all references to them.  Thus we here for existance
# of the INSTDIR variable.
#

ifdef INSTDIR

install installmost::

else

install installmost::
	@echo "ERROR: variable INSTDIR not defined." ; exit 2

endif

include $(ROOT)/mk/config.mk		# tree-specific defines
-include $(ROOT)/mk/autoconf.mk.d

ifeq (0,${MAKELEVEL})

ifndef NEMESIS_EXPERT
#
# We must check whether people have the Nemesis Unix tools on their PATH
#
got_tools_cmd := \
  case "`which middlc`" in \
    no*)                    echo no  ;; \
    "which: no "*)          echo no ;; \
    "which: Cannot find "*) echo no ;; \
    "")                     echo no ;; \
    *)                      echo yes ;; \
  esac

ifndef got_tools
got_tools := $(strip $(shell $(got_tools_cmd)))
export got_tools
endif

ifneq ($(got_tools),yes)

  %::	# catch-all rule
	@echo "ERROR: can't find Nemesis tools (please check PATH)." ; exit 2

endif

endif # NEMESIS_EXPERT

# This double list of many many targets is to ensure that we always
# check that the configuration is up to date even on e.g. from-dev and
# not just on a all

all clean depend cleanse install installmost::	$(ROOT)/mk/autoconf.mk

grow-% from-%:: $(ROOT)/mk/autoconf.mk

$(ROOT)/mk/cfg/config.dat:
	cd $(ROOT)/mk/cfg && $(MAKE) config.dat


$(ROOT)/choices:
	touch $(ROOT)/choices

endif # MAKELEVEL=0

#
# Directories
#
H	:= $(ROOT)/h
IF	:= $(ROOT)/repo

#
# Rules: these come here so they can be overridden by the platform file
#	 if necessary.
#

% : %,v		# Remove some of the more stupid default rules
% : RCS/%,v
% : s.%
% : SCCS/s.%
%.c : %.w %.ch

.SUFFIXES:	# the defaults lose


.PHONY::	all grow FRC clean depend cleanse install installmost dir srcdir rmlinks thing ldlump ldlumpnou precompiled veneer


# These go here to define CC etc, and also after the default rules
# to be able to override them.


-include $(ROOT)/mk/source.mk		# defines absolute SRCROOT
include $(ROOT)/mk/platform.mk		# defines CC etc.
include $(ROOT)/mk/target.mk		# defines ARCH TGT_x MC MC_x etc.

# Makefiles that EVERYTHING should depend on
MAKEFILE_DEPS += Makefile \
  $(ROOT)/mk/rules.mk \
  $(ROOT)/mk/platform.mk \
  $(ROOT)/mk/target.mk 

#
# Now the default rules.
#

%.i: %.c $(MAKEFILE_DEPS)
	$(CC) $(CFLAGS) $(DEF) $(INC) -E $< > $@ || { rm -f $@ ; exit 1 ; }

%.i.c : %.i
	grep -v '^#' $< > $@

%.i: %.S $(MAKEFILE_DEPS)
	$(CPP) $(DEF) $(INC) $< > $@ || { rm -f $@ ; exit 1 ; }

# 
# rules for antistatic
#

ifdef ANTISTATIC_DESCRIPTION

ifdef MODULE
THING_NAME = $(MODULE)
endif

ifdef PROGRAM
THING_NAME = $(PROGRAM)
endif


ifdef ANTISTATIC_INIT
CFILES += $(THING_NAME)_init.c
clean::
	$(RM) $(THING_NAME)_init.c
endif


ANTISTATIC_INIT_O := $(ANTISTATIC_DESCRIPTION:.desc=_init.o)


%_init.c: %.desc
	genasinit.py < $< > $(<:.desc=_init.c)

%_init.o: %_init.c
	$(CC) $(CFLAGS) $(DEF) $(INC) -MD -c $(<:.desc=_init.c)


%.as.i: %.i $(ANTISTATIC_DESCRIPTION)
	cmutate.py -elim_state -get_rec -get_nem_pers -d$(ANTISTATIC_DESCRIPTION) $(ANTISTATIC_LIBS) -o$@ $<

clean::
	$(RM) *.as.i

%.o: %.as.i $(MAKEFILE_DEPS)
	$(CC) $(CFLAGS) $(DEF) $(INC) -MD -c $< -o $(<:.as.i=.o)

.PRECIOUS:: %.as.i %.i

else

#
# rules for non-antistaticed code
#


%.c.d %.o: %.c $(MAKEFILE_DEPS)
	$(CC) $(CFLAGS) $(DEF) $(INC) -MD -c $< && $(MV) $*.d $*.c.d
endif
%.s: %.c $(MAKEFILE_DEPS)
	$(CC) $(CFLAGS) $(DEF) $(INC) -S $<

%.o: %.S $(MAKEFILE_DEPS)
	$(AS) $(ASFLAGS) $(DEF) $(INC) $< -o $@

%.sym: %.o 
	$(NM) $< | $(AWK) $(NMFILT) > $@

ifdef NEMESIS

%.nbf::
	PYTHONPATH=$(ROOT)/glue python $(ROOT)/glue/gennbf.py nemesis.nbf

# suck up depedencies if they are there
-include $(NEMESIS).nem.d
-include $(NEMESIS).nbf.d

# Some rules for the boot/images/<arch> directories:
.PRECIOUS:: %.nbf

%.nem %.sym %.nem.d: nemesis.nbf
	$(NEMBUILD) $(NEMBUILDFLAGS) -g $*.sym -o $*.nem -M \
	  -i "`whoami`@`hostname` `date \"+%H:%M %a %d/%m/%y\"`" nemesis.nbf
	grep -v "IntfTypes" $*.sym > $*.sym.tmp
	$(MV) $*.sym.tmp $*.sym

%.nem %.sym %.nem.d:: %.nbf
	$(NEMBUILD) $(NEMBUILDFLAGS) -g $*.sym -o $*.nem -M \
	  -i "`whoami`@`hostname` `date \"+%H:%M %a %d/%m/%y\"`" $<
	grep -v "IntfTypes" $*.sym > $*.sym.tmp
	$(MV) $*.sym.tmp $*.sym

%.src: $(NEMESIS).nbf %.nem
	$(GENSRC) $(NEMESIS).nbf > $@ || { rm -f $@ ; exit 1 ; }

clean::
	$(RM) .symbols* *.run 
	$(RM) $(NEMESIS).sym $(NEMESIS).nbf $(NEMESIS).nem $(NEMESIS).src

endif

cxref.function: $(CFILES) 
	cxref -xref-func -D__NO_METHOD_MACROS__ $(DEF) $(INC) $(CFILES)

#
# Include make definitions configured per build tree.
#
# We put these here so they can over-ride the default ones if necessary.
# (contrary to the gnumake docs, the last rule or variable definition is
#  the one used.)
#

include $(ROOT)/mk/source.mk           # defines absolute SRCROOT
include $(ROOT)/mk/platform.mk         # defines CC etc.
include $(ROOT)/mk/target.mk           # defines ARCH TGT_x MC MC_x etc.
include $(ROOT)/mk/etags.mk            # magic for building tags database

# interface headers and type system definitions

# Only build interfaces if they are local to this dir
ifneq ($(strip $(INTERFACES)),)

%.ast: %.if
	$(CL) $(CLINC) -A $(notdir $<)

%.ast.d: %.if
	$(CL) $(CLINC) -M $(notdir $<)

ifneq ($(INDIRECTORY),if)
%.ih: %.ast
	$(ASTGEN) $(ASTGENINC) ih ifd $(notdir $<)

%.def.c: %.ast
	$(ASTGEN) $(ASTGENINC) def ifd $(notdir $<)
else
%.ih: %.ast
	$(ASTGEN) $(ASTGENINC) ih def ifd $<

%.def.c: %.ast
	$(ASTGEN) $(ASTGENINC) ih def ifd $<
endif

endif

# stubs and marshalling procs

vpath %.ast $(IF)

IDC_M_%.h: IDC_CM_%.h
	ln -s $< $@
IDC_M_%.h: %.ast
	$(ASTGEN) $(ASTGENINC) idc[marshalling] ifd $(notdir $<)

IDC_M_%.c: IDC_CM_%.c
	ln -s $< $@
IDC_M_%.c : %.ast
	$(ASTGEN) $(ASTGENINC) idc[marshalling] ifd $(notdir $<)

IDC_S_%.c: %.ast
	$(ASTGEN) $(ASTGENINC) idc[stubs] ifd $(notdir $<)

idcstuff: $(MARSHAL_INTFS:%=%.ast) $(STUBS_INTFS:%=%.ast)
	$(ASTGEN) $(ASTGENINC) idc[marshalling] ifd $(MARSHAL_INTFS:%=%.ast)
	$(ASTGEN) $(ASTGENINC) idc[stubs] ifd $(STUBS_INTFS:%=%.ast)
	touch idcstuff


# TeX versions of interfaces

%.eps: %.obj
	$(TGIF) -print -eps -gray $*

%.tex: $(ROOT)/repo/%.if
	$(MIDDL2TEX) $< $* > $@ || { rm -f $@; exit 1; }

%.dvi: %.tex
	$(LATEX) $<

%.ps: %.dvi
	$(DVIPS) -o $@ $< 


# Work out where we are
# XXX: this is bad, because it breaks outside of the tree

ifeq ($(ROOT),.)
  CURRENT_DIR := .
  # This value is used when growing a build tree from its root.
endif

ifndef CURRENT_DIR
  # deduce it from ROOT - not sure this is a good idea
  CURRENT_DIR := $(shell \
    tmp=$(ROOT) ; \
    wd=`pwd` ; \
    crd=. ; \
    while [ $$tmp != '.' ] ; do \
      crd=`basename $$wd`/$$crd ; \
      wd=`dirname $$wd` ; \
      tmp=`dirname $$tmp` ; \
    done ; \
    echo `dirname $$crd` ; \
  )
endif


#
# Dependency generators (deduced from compilers unless explicitly specified)
#
ifneq ({CONFIG_INTERFACES}, y)
ifndef IFDEPEND
  IFDEPEND := $(MIDDLC) -M
endif
endif
ifndef CDEPEND
  CDEPEND := $(CC) -M
endif
ifndef SDEPEND
  SDEPEND := $(CC) -M -x assembler-with-cpp
endif

#
# Compiler options:
#
MIDDLINC:= -I$(IF) $(LOCAL_MIDDLINC)

ASTGENINC:= +$(IF) $(LOCAL_ASTINC)

CLINC:= -I$(IF) $(LOCAL_CLINC)

CFLAGS	:= -Wall -Wno-format -O $(TGT_CFLAGS) $(MC_CFLAGS) \
	   $(GLOBAL_CFLAGS) $(LOCAL_CFLAGS) \
	   -nostdinc -fno-builtin 

ASFLAGS	:= $(TGT_ASFLAGS) $(MC_ASFLAGS) \
	   $(GLOBAL_ASFLAGS) $(LOCAL_ASFLAGS)  \
	   -DASSEMBLER=1

DEF	:= -DNEMESIS $(TGT_DEF) $(MC_DEF) $(LOCAL_DEF) $(GLOBAL_DEFINES)

INC	:= -I$(H)   -I$(H)/$(ARCH)  -I$(H)/$(MC)  \
           -I$(IF) \
           $(TGT_INC) $(MC_INC) -I$(H)/dev -I. $(LOCAL_INC)

LDFLAGS	:= $(TGT_LDFLAGS) $(MC_LDFLAGS) $(LOCAL_LDFLAGS)


#
# Per-directory values:
#

# Set this to an empty string to specify no libc to be linked in
ifeq "$(origin DEFAULT_VENLIBS)" "undefined"
DEFAULT_VENLIBS := c/libc.a
endif

ALLVENLIBS	:= $(EXTRA_VENLIBS) $(DEFAULT_VENLIBS)

ifeq "$(origin DEFAULT_LIBS)" "undefined"
DEFAULT_LIBS := $(ROOT)/lib/static/system/libsystem.a $(LIBGCCA) 
endif

ALLLIBS 	:= $(EXTRA_LIBS) $(PERSONALITY_LIBS) $(addprefix $(ROOT)/lib/veneer/, $(ALLVENLIBS)) \
		$(DEFAULT_LIBS) 
ifeq (${CONFIG_INTERFACES}, y)
DERIVED_P	:= $(INTERFACES:.if=.p)
DERIVED_AST     := $(INTERFACES:.if=.ast)

DERIVED_IH	:= $(INTERFACES:.if=.ih)
DERIVED_DEF	:= $(INTERFACES:.if=.def.c)

DERIVED_H	:= $(DERIVED_IH) $(LOCAL_DERIVED_H)
DERIVED_C	:= $(DERIVED_DEF) 

INTF_DERIVEDS	:= $(DERIVED_H) $(DERIVED_C) $(DERIVED_P) $(DERIVED_AST)

ast: 	$(DERIVED_AST)
else
DERIVED_IH	:= $(INTERFACES:.if=.ih)
DERIVED_DEF	:= $(INTERFACES:.if=.def.c)

DERIVED_MH	:= $(MARSHAL_INTFS:%=IDC_M_%.h) $(CUSTOM_MARSHAL_INTFS:%=IDC_M_%.h)
CUSTOM_MH	:= $(CUSTOM_MARSHAL_INTFS:%=IDC_M_%.h) 

DERIVED_MC	:= $(MARSHAL_INTFS:%=IDC_M_%.c)
CUSTOM_MC	:= $(CUSTOM_MARSHAL_INTFS:%=IDC_CM_%.c) 

DERIVED_SC	:= $(STUBS_INTFS:%=IDC_S_%.c)
CUSTOM_SC	:= $(CUSTOM_STUBS_INTFS:%=IDC_CS_%.c) 

DERIVED_H	:= $(DERIVED_IH) $(DERIVED_MH) $(LOCAL_DERIVED_H)
DERIVED_C	:= $(DERIVED_DEF) $(DERIVED_MC) $(DERIVED_SC) 

INTF_DERIVEDS	:= $(DERIVED_H) $(DERIVED_C)
endif

# Keep intermediate files
.PRECIOUS::	$(INTF_DERIVEDS)

ALLCFILES	:= $(DERIVED_DEF) \
		     $(DERIVED_MC) $(CUSTOM_MC) \
		     $(DERIVED_SC) $(CUSTOM_SC) \
		     $(CFILES) $(CFILES_$(ARCH)) $(CFILES_$(MC))

ALLASFILES	:= $(ASFILES) $(ASFILES_$(ARCH)) $(ASFILES_$(MC))

OBJS		:= $(ALLASFILES:.S=.o) $(ALLCFILES:.c=.o) $(LOCAL_OBJS) \
		   $(addprefix $(EXTRA_OBJDIR)/,$(EXTRA_OBJS)) \
		   $(addprefix $(EXTRA_OBJDIR)/,$(EXTRA_OBJS_$(ARCH))) \
		   $(addprefix $(EXTRA_OBJDIR)/,$(EXTRA_OBJS_$(MC))) 


DERIVED_S	:= $(ALLCFILES:.c=.s)

#
# Configuration based extras
#
DEF += $(DEFINES_$(CURRENT_DIR))
CFLAGS += $(CFLAGS_$(CURRENT_DIR)) 
ASFLAGS += $(ASFLAGS_$(CURRENT_DIR)) 

#
# Generate and include dependencies, if any.
# (see "Generating Dependencies Automatically" in the gnumake manual)
#
# .s files need to bracket assembler with "ifndef __LANGUAGE_C" so they
# can be run through the C compiler in the guise of .s.c files.  Don't
# ask me why "gcc -M foo.s" doesn't work, but it doesn't.
#
# Another subtlety: if there are bugs in .h files, "gcc -M > foo.x.d" may
# produce an empty but up-to-date file "foo.x.d", which will not be remade
# once the bugs are fixed.  This will prevent "foo.o" from being remade
# when its real dependencies change.  To prevent this lossage, we abandon
# all hope when an empty ".d" is made.
#
# We must build derived header files before doing dependency processing for C,
# and only include .d files for the derived .c we've built so far.
#

all install installmost:: $(DERIVED_H)

DEPS := $(addsuffix .d, $(wildcard $(INTERFACES:.if=.ast))) \
	$(wildcard $(addsuffix .d, $(ALLCFILES))) \
	$(addsuffix .d, $(ALLASFILES) $(LOCAL_DEPS)) 

ifeq ($(strip $(wildcard $(DERIVED_H))),$(strip $(DERIVED_H)))
  # the derived headers are there
  ifneq ($(strip $(DEPS)),)
    include $(DEPS)
  endif
endif

%.if.d: %.if
	$(IFDEPEND) $(MIDDLINC) $< > $@ || rm -f $@
	@if [ ! -s $@ ] ; then \
	   $(RM) $@ ; \
	   echo "ERROR: can't make depends for $<; please fix its includes.";\
	   exit 1 ; \
	else :; \
	fi

%.c.d: %.c
	-$(CDEPEND) -nostdinc $(INC) $(DEF) $< > $@ || rm -f $@
	@if [ ! -s $@ ] ; then \
	   $(RM) $@ ; \
	   echo "ERROR: can't make depends for $<; please fix its includes.";\
	   exit 1 ; \
	else :; \
	fi

%.S.d: %.S
	-$(SDEPEND) -nostdinc $(INC) $(DEF) $< >$@ || rm -f $@
	@if [ ! -s $@ ] ; then \
	   $(RM) $@ ; \
	   echo "ERROR: can't make depends for $<; please fix its includes.";\
	   exit 1 ; \
	else :; \
	fi

#
# To Force ReCompile, add FRC as a dependency
#
FRC:


#
# Standard recursive targets in the subdirs, if any.  These come before
# the standard targets in the current directory so that things are built
# depth first.


# If we have an INSTDIR, then add the trivial 'install' target, which doesn't 
# depend on anything, and has no associated actions.  Later add in actions
# if this is a MODULE or a PROGRAM
ifdef INSTDIR
  install::

  installmost::
endif

# Rather than duplicating the rule to copy the image in every 
# boot/images/$(MC) directory, do it once here

ifeq ($(CURRENT_DIR), boot/images/$(MC))
$(INSTDIR)/image: 	image
	cp -f image $(INSTDIR)/image

installmost:: $(INSTDIR)/image
install::
	cp -f image $(INSTDIR)/image
endif


# Filter out duplicate subdirs
SUBDIRS_$(CURRENT_DIR) := $(sort $(SUBDIRS_$(CURRENT_DIR)))

ALLSUBDIRS := $(filter-out $(EXCLUDE_$(CURRENT_DIR)), $(SUBDIRS_PRE_$(ARCH)) $(SUBDIRS) $(SUBDIRS_$(ARCH)) $(SUBDIRS_$(MC)) $(SUBDIRS_$(CURRENT_DIR)))

grow:: FRC
	PYTHONPATH=$(ROOT)/glue python $(ROOT)/glue/growtree.py $(CURRENT_DIR)

grow-%:: FRC
	PYTHONPATH=$(ROOT)/glue python $(ROOT)/glue/growtree.py $(CURRENT_DIR)/$(subst grow-,,$@)



ifneq ($(strip $(ALLSUBDIRS)),)

#
# This little bit of MAKEOVERRIDES magic allows us to say e.g.:
#
# sh$ gnumake all SUBDIRS=foo
#
# without the make child that is run in subdirectory foo from inheriting
# the SUBDIRS=foo override which could potentially confuse its own
# action on subdirectories. We permit other variables to be overridden (such
# as CFLAGS). It is further necessary to ensure that they dont propogate via
# the environment.

MAKEOVERRIDES := $(filter-out ALLSUBDIRS%,$(filter-out SUBDIRS%,$(MAKEOVERRIDES)))

unexport ALLSUBDIRS SUBDIRS_PRE_$(ARCH) SUBDIRS SUBDIRS_$(ARCH) SUBDIRS_$(MC)



  from-%:: FRC
	@for i in $(ALLSUBDIRS); do echo $$i; done | \
	{ go='' ; while { read d || exit 0 ; } \
	&& if [ "$$d" = "$*" ] ; then go='1'; else :; fi \
	&& if [ "$$go" ] ; \
	    then \
		echo && echo '####' $(CURRENT_DIR)/$$d all '####' && echo && \
		(cd ./$$d && $(MAKE) all) ; \
	    else \
		echo "Skipping subdir $$d..." ; \
	    fi \
	|| exit 1 ; \
	do :; done ; }

#	@ go='' ; \
#	  for d in $(ALLSUBDIRS); do \
#	    if [ $$d = $* ] ; then go='1' ; fi ; \
#	    if [ "$$go" ] ; then \
#	      echo && echo '####' $(CURRENT_DIR)/$$d all '####' && echo && \
#	      (cd ./$$d && $(MAKE) all) \
#	    else \
#              echo "Skipping subdir $$d..." ; \
#	    fi; \
#	  done

# This is the naive way:
#
#  all clean depend:: FRC
#	@for d in $(ALLSUBDIRS); do \
#	  (echo && echo '####' $(CURRENT_DIR)/$$d $@ '####' && echo && \
#	   cd ./$$d && $(MAKE) $@) || exit 1; \
#	 done
#
# This is the problem with it:
# $ for a in 3 2 1 0
# do 
# (exit $a)
# done; echo $?
# 0
#
# This happens even if set -e is in operation. The result of a for is the
# result of the last list irrespective of what the others did. There is
# no way to terminate early.

# One efficient way of doing it would be:
#
#   all clean depend:: FRC
#	@$(foreach d,$(ALLSUBDIRS),(echo && echo '####' $(CURRENT_DIR)/$d '####' && echo && cd ./$d && $(MAKE) $@) &&) true
#
# but this can fail on account of being too long.
#
# This solution (following) works with ordinary make as well as gnumake.

   all clean depend cleanse install installmost:: FRC $(ALLSUBDIRS)
	@for i in $(ALLSUBDIRS); do echo $$i; done | \
	while { read i || exit 0 ; } \
	&& echo "#### making $@ in $(CURRENT_DIR)/$$i... ####" \
	&& (cd ./$$i && $(MAKE) $@) \
	|| exit 1; \
	do :; done

ifdef SRCROOT
# This bit of magic causes new subdirs to be automatically grown
$(ALLSUBDIRS):
	${MAKE} DEPS= grow-$@ 
endif
endif #ALLSUBDIRS

#
# Standard targets in the current directory:
#

dir:: FRC
	@echo $(CURRENT_DIR)

srcdir:: FRC
	@echo $(SRCROOT)/$(CURRENT_DIR)


clean:: FRC
	$(RM) *.i core *~ *.o *.d *.bak *.sym *.dis
	$(RM) $(INTF_DERIVEDS) $(DERIVED_S) $(LOCAL_DERIVEDS)

cleanse:: FRC

depend:: FRC
	$(RM) *.d
	$(MAKE) DEPS='' $(addsuffix .d, $(wildcard $(INTERFACES) $(ALLCFILES) \
			  $(ALLASFILES) $(LOCAL_DEPS)))

rmlinks:: FRC
	-@for i in `find . -type l -print` ; do \
	    case $$i in \
	      *Makefile*) ;; \
	      *) $(RM) $$i ; $(RM) $$i.d ; \
	         echo "$(RM) $$i[.d]" ;; \
	    esac ; \
	  done; exit 0;

# Rather than duplicating the rule to copy the image in every 
# boot/images/$(MC) directory, do it once here

ifeq ($(CURRENT_DIR), boot/images/$(MC))
$(INSTDIR)/image: 	image
	cp -f image $(INSTDIR)/image

install installmost:: $(INSTDIR)/image

endif


#
# Generate AR libraries
#

ifdef ARLIB
  lib$(ARLIB).a: $(OBJS)
	$(RM) -f $@
	$(AR) rc $@ $(OBJS)
	$(RANLIB) $@

  install installmost all:: lib$(ARLIB).a

  clean cleanse::
	$(RM) lib$(ARLIB).a

endif


#
# Now targets which may or may not be triggered by some particular Makefile
#

ifdef MODULE
  THING=$(MODULE)
  CRT=
  CHECKER=grep "^\(0x\)\{0,1\}[0-9a-fA-F][0-9a-fA-F]* [dDbBCc] "
endif

ifdef STATEFULMODULE
  THING=$(STATEFULMODULE)
  CRT=
  CHECKER=false
endif

ifdef PROGRAM
  THING=$(PROGRAM)
  CRT=$(ROOT)/lib/static/system/crt0.o
  CHECKER=false
ifndef INSTALLNAME
  INSTALLNAME=$(PROGRAM)
endif
endif

ifdef PROGMOD
  THING=$(PROGMOD)
  CRT=$(ROOT)/lib/static/system/crt0.o
  CHECKER=grep "^\(0x\)\{0,1\}[0-9a-fA-F][0-9a-fA-F]* [dDbBCc] "
ifndef INSTALLNAME
  INSTALLNAME=$(PROGMOD)
endif
endif



# ---------------------
# Now rules for all THINGs normally THING is null so this wont get triggered
# 


ifdef THING

  # This evil hack has to be here otherwise this silly system thinks that
  # crt0.o depends on the Makefile of every directory in which that Makefile
  # has a 'program' line (even out of the tree). Duh!
  $(ROOT)/lib/static/system/crt0.o:
	@true

  clean cleanse::
	$(RM) $(THING)

  install installmost all:: thing

  thing:: $(THING)

ifdef CONFIG_MODULE_OFFSET_DATA
  .modulename.c: Makefile
	echo 'const char NemesisModuleName[] = "'$(notdir $(THING))'";' > .modulename.c

  INTF_DERIVEDS += .modulename.c
  OBJS += .modulename.o
endif

  $(THING): $(OBJS) $(CRT) $(ALLLIBS)
	$(RM) $(THING)
	$(LD) $(LDFLAGS) -o $(THING) $(CRT) $(OBJS) $(ALLLIBS)
	@$(NM) $(THING) | $(SYMFILT) > nm.out
	@if grep "^ * U " nm.out; then \
		echo "error: $(THING) had undefined symbols"; \
		$(MV) $(THING) $(THING)~; \
		$(RM) nm.out; \
		exit 1; \
	else :; \
	fi
	@if $(CHECKER) nm.out;\
	then \
		echo "error: $(THING) has state"; \
		$(MV) $(THING) $(THING)~; \
		$(RM) nm.out; \
		exit 1; \
	else :; \
	fi
	@chmod a+x $(THING)
	@$(RM) nm.out

ifdef INSTDIR
  install:: $(THING)
	cp $(THING) $(INSTDIR)

ifdef INSTALLNAME
ifneq ($(INSTALLNAME),donotinstall)
  installmost:: $(INSTDIR)/$(INSTALLNAME)

  $(INSTDIR)/$(INSTALLNAME): $(THING)
	cp $(THING) $(INSTDIR)/$(INSTALLNAME)
endif
endif # INSTALLNAME
endif # INSTDIR

endif # THING


# An LDLUMP is a boring lump of code ld'ed together. Some makefiles
# were using MODULE to mean this earlier, but MODULE has now changed
# to link in some std libraries.
ifdef LDLUMP

  clean cleanse::
	$(RM) $(LDLUMP)

  install installmost all:: ldlump

  ldlump:: $(LDLUMP)

  $(LDLUMP): $(OBJS) 
	$(RM) $(LDLUMP)
	$(LD) $(LDFLAGS) -o $(LDLUMP) $(OBJS)

endif # LDLUMP

# An LDLUMPNOU is a boring lump of code ld'ed together. It is checked against undefined symbold.
ifdef LDLUMPNOU

  clean cleanse::
	$(RM) $(LDLUMPNOU)

  install installmost all:: ldlumpnou

  ldlumpnou:: $(LDLUMPNOU)

  $(LDLUMPNOU): $(OBJS) 
	$(RM) $(LDLUMPNOU)
	$(LD) $(LDFLAGS) -o $(LDLUMPNOU) $(OBJS)
	@$(NM) $(LDLUMPNOU) | $(SYMFILT) > nm.out
	@if grep "^ * U " nm.out; then \
		echo "error: $(LDLUMPNOU) had undefined symbols"; \
		$(MV) $(LDLUMPNOU) $(LDLUMPNOU)~; \
		$(RM) nm.out; \
		exit 1; \
	else :; \
	fi
	@$(RM) nm.out

endif # LDLUMPNOU


ifdef PRECOMPILED

  precompiled:: FRC
	$(INSTALL) -c -m 444 Precompiled.mk \
	      $(SRCROOT)/../precompiled/$(CURRENT_DIR)/Makefile
	for f in $(PRECOMPILED); do \
	  $(INSTALL) -c -m 444 $$f $(SRCROOT)/../precompiled/$(CURRENT_DIR) ; \
	done

endif # PRECOMPILED


ifdef VENEER

  clean::
	$(RM) $(VENEER).a

  install installmost all:: veneer

  veneer:: $(VENEER).a

  VENEERMEMBERS := $(wildcard veneer*.S)

  veneersub: $(VENEERMEMBERS:.S=.o)
	$(AR) r $(VENEER).a $?

  $(VENEER).a: $(VENEER).ven $(H)/$(ARCH)/veneer.h $(VENEER_ARCH_DEPEND_HACK)
	$(RM) $@
	$(CPP) $(DEF) - < $(VENEER).ven | sed -e '/^#/d' -e '/^[ \t]*$$/d' | \
	( t=0 && { read p || exit 1 ; } \
	    && while { read f || exit 0 ; } \
		&& echo "#include <veneer.h>" > veneer$$t.S \
		&& echo "VENEER_NO_CL($$f,$$t,$$p)" >> veneer$$t.S \
		&& t=`eval expr $$t + 1` \
	       || { rm -f veneer* $@; exit 1; }; \
	       do :; done \
	)
	$(MAKE) veneersub
	rm -f veneer*
	$(RANLIB) $@

endif # VENEER


ifdef ANTISTATIC_GENERATION

ALLINITFILES = $(ALLCFILES:.c=.init)


%.init %.protos: %.i
	cmutate.py -inits $< > $@
	cmutate.py -protos $< > $(<:.i=.protos)


allinits: $(ALLINITFILES)
	cat $^ > $@

.PRECIOUS:: %.init

$(ANTISTATIC_GENERATION): allinits
	genasdesc.py $(THING)~ allinits > $@

endif


# End of rules.mk
