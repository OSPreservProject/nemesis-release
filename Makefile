#
# this is a noddy makefile for people who don't read docs
#
# override or hack the makefile to build on a different platform or for a
# different target
#

# there must be a tools/master/mk/$(PLATFORM).mk
PLATFORM = ix86_linux_rh5.1
# there must be a nemesis/master/mk/$(PLATFORM).mk
PLATFORM_FILE = ix86_linux
# This Nemesis release supports intel only; if you are using a non-intel
# package change this to something else
TARGET = intel

all: image

image: .tools-phony
	@echo @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@echo Welcome to this Nemesis quickstart release
	@echo I will now build a Nemesis image for you. 
	@echo If you want to rebuild parts of it, you can save time by putting:
	@echo     `pwd`/tools/install/$(PLATFORM)/bin on to your path
	@echo on to your path and typing make in nemesis/build_$(TARGET)
	@echo @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@echo
	@echo
	@echo
	PATH=`pwd`/tools/install/$(PLATFORM)/bin:$(PATH) python quickbuild.py $(TARGET) $(PLATFORM_FILE)

.bootloader-phony: .tools-phony
	@echo @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@echo Welcome to this Nemesis quickstart release
	@echo I will now build the Nemesis bootloader for you. A bootloader
	@echo tree should appear:
	@echo     `pwd`/nemesis/build_$(TARGET)_bootloader
	@echo @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	PATH=`pwd`/tools/install/$(PLATFORM)/bin:$(PATH) python quickbuild.py bootloader-$(TARGET) $(PLATFORM_FILE)
	touch .bootloader-phony

bootloader: .bootloader-phony

bootfloppy: .bootloader-phony
	python quickbuild.py bootfloppy

.tools-phony:
	@echo @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@echo Welcome to this Nemesis quickstart release
	@echo I will now build the Nemesis tools for you. They should appear in
	@echo     `pwd`/tools/install/$(PLATFORM)/bin on to your path
	@echo @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	python quickbuild.py tools $(PLATFORM)
	touch .tools-phony

clean: distclean

distclean:
	rm -rf nemesis/build_*
	rm -rf tools/install
	rm -rf tools/build
	-rm .tools-phony


#
# a hack to build a quickstart tarball
# XXX assumes we are in a subdirectory called release
#

dist:	distclean
	(cd .. && gtar cvfz quickstart.tar.gz release/)

#
# a hack to copy release notes in to the root of the tree
#

README.html: docs/release-notes/release-notes.tex
	python quickbuild.py dochtml-release-notes
	cp docs/release-notes/release-notes/index.html README.html

docs:
	python quickbuild.py docs
