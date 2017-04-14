#
# CAUTION. This is very gross.
#

# Here we are building an ADDITIONAL binary for a standard platform
# thus we wish to define ARCH to be that for the install point for the
# normal tools of that architecure.

# It is included by rules.mk
#

include $(ROOT)/mk/ix86_linux_rh4.0.mk

# over-ride the subdirs in top level make file
ifeq ($(ROOT),.)
  SUBDIRS		:=
  SUBDIRS_$(ARCH) := nembuild
endif

BINARYFORMAT	= arm-unknown-coff-

CC		= gcc -Wall -g -I/usr/local/gnuarm/include -Dlinux_for_arm -L/usr/local/gnuarm/lib
