ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../db-monitor

LD_FLAGS =\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lcommand_line_parser \
	-lsocket_interface \
	-ldb-monitor \
	-lats-common \
	-lpthread

CFLAGS =\
	-Wall \
	-ggdb3

EXEC=db-test

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	$(CROSS_COMPILE)g++ -o bin/$(ARCH)/$(EXEC) main.cpp $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
