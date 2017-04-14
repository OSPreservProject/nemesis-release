ROOT	= ..

CFILES	= $(wildcard *.c)

INSTALL_BINS := undefarm

include $(ROOT)/mk/rules.mk

undefarm: $(OBJS)
	$(CC) $(CFLAGS) -o undefarm $(OBJS)

