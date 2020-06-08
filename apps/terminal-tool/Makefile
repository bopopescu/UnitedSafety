ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../


LD_FLAGS=\
	-L../socket_interface/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lsocket_interface \
	-lcommand_line_parser \
	-ldb-monitor \
	-lats-common \
	-lpthread \
	-lrt \
	-s

EXEC=terminal-tool

SRC = \
	main.cpp

CFLAGS=-Os

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $< $(CFLAGS) $(INCLUDE)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

clean:
	rm -rf bin
	
