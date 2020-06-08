
ARCH=PC

EXEC=port-test

INCLUDE=\
	-I../../gcc \
	-I../../ats-common \
	-I../../socket_interface \
	-I../../command_line_parser

LIBS=\
	-L../../ats-common/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../command_line_parser/bin/$(ARCH) \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lpthread

all:
	mkdir -p bin
	${CROSS_COMPILE}g++ -o bin/$(EXEC) $(INCLUDE) -Wall -Werror port-test.cpp $(LIBS)

clean:
	rm -rf bin
