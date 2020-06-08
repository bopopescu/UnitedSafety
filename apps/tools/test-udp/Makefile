ARCH=PC

INCLUDE =\
	-I../../ats-common \
	-I../../socket_interface \
	-I../../command_line_parser

LD_FLAGS=\
	-L../../command_line_parser/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../ats-common/bin/$(ARCH) \
	-lsocket_interface \
	-lats-common \
	-lpthread

EXEC=test-udp

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(INCLUDE) -s $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(INCLUDE) -s $(LD_FLAGS)

clean:
	rm -rf bin
