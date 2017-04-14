ROOT	= ..

CFILES	= fixup.c

INSTALL_BINS := multiboot-fixup

include $(ROOT)/mk/rules.mk

clean::
	$(RM) $(OBJS)

multiboot-fixup: $(OBJS)
	$(CC) $(CFLAGS) -o multiboot-fixup $(OBJS)
