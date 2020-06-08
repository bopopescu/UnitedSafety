
ARCH=PC

LIB_OUTPUT = bin/$(ARCH)/liblogger.a

SRC = \
	LogServer.cpp

LIBS =

LIBPATH =

INCLUDE = \
	-I../ats-common \
	-I../command_line_parser \
	-I../socket_interface

CFLAGS = -Wall $(INCLUDE) $(REDSTONE_GCC_MACHINE)

HEADERS =\
	LogServer.h

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

all: \
	$(LIB_OUTPUT)

$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	$(CROSS_COMPILE)ar -r $@ $(OBJS)

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH) ${HEADERS}
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $<

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)
