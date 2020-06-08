ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../state-machine \
	-I../FastTrac \
	-I../FastTrac/FASTLib \
	-I../FastTrac/UtilityLib \
	-I../db-monitor \
	-I../message-assembler

LD_FLAGS=\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-ldb-monitor \
	-lpthread

CFLAGS=-Wall -Os

SRC = \
	packetizerSender.cpp \
	packetizerDB.cpp \
	midDB.cpp 
 

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

ifeq ($(REDSTONE_BUILD_DIR),)
LIB_OUTPUT=bin/$(ARCH)/libpacketizer.a
else
SHARED=1
LIB_OUTPUT=bin/$(ARCH)/libpacketizer.so
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
	$(CROSS_COMPILE)g++ -c -o $@ $< $(CFLAGS) $(INCLUDE)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
