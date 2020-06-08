
ARCH=PC

ifeq ($(REDSTONE_BUILD_DIR),)
LIB_OUTPUT = bin/$(ARCH)/libcommand_line_parser.a
LDFLAGS =
CFLAGS =
else
SHARED=1
LIB_OUTPUT = bin/$(ARCH)/libcommand_line_parser.so
LDFLAGS = -shared
CFLAGS = -fPIC
endif

SRC = \
	command_line_parser.c

LIBS = \
	-lpthread

LIBPATH =

INCLUDE =

CFLAGS += -Wall $(INCLUDE) $(REDSTONE_GCC_MACHINE) -Os

OBJS = $(SRC:%.c=bin/$(ARCH)/%.o)

all: $(LIB_OUTPUT)

ifeq ($(SHARED),1)
$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	$(CROSS_COMPILE)gcc $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBPATH) $(LIBS)
else
$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	$(CROSS_COMPILE)ar -r $@ $(OBJS)
endif

$(OBJS): bin/$(ARCH)/%.o: %.c Makefile bin/$(ARCH)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $@ $<

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
