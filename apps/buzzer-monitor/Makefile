ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser

CFLAGS = -O3

LD_FLAGS=\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lpthread \
	-s

EXEC=buzzer-monitor

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp $(CFLAGS) -Wall $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(CFLAGS) main.cpp -Wall $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
