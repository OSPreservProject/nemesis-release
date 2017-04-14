ROOT	= ..

SUBDIRS	= front python

CFILES  = fp_reduce.c

INSTALL_BINS := middlc fp_reduce

include $(ROOT)/mk/rules.mk

fp_reduce: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
