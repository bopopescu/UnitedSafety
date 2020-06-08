ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../command_line_parser

LD_FLAGS=\
	-L../command_line_parser/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lcommand_line_parser \
	-lats-common \
	-lpthread

CFLAGS=-Wall -Os

SRC = \
	state_machine.cpp \
	state-machine-data.cpp \
	event_listener.cpp \
	timer-event.cpp

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

ifeq ($(REDSTONE_BUILD_DIR),)
LIB_OUTPUT=bin/$(ARCH)/libstate-machine.a
else
SHARED=1
LIB_OUTPUT=bin/$(ARCH)/libstate-machine.so
LDFLAGS += -shared
CFLAGS += -fPIC
endif

all: $(ARCH)

ifeq ($(SHARED),1)
$(ARCH): bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(LDFLAGS) -o $(LIB_OUTPUT) $(OBJS)
else
$(ARCH): bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	$(CROSS_COMPILE)ar -r $(LIB_OUTPUT) $(OBJS)
endif

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $< $(CFLAGS) $(INCLUDE)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
