ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../FastTrac \
	-I../db-monitor \
	-I../include \
	-I../../3rd-party/rapidjson/include \
	-I./libmodbus-3.0.6/src \
	-I./libmodbus-3.0.6

LD_FLAGS=\
	-L../db-monitor/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../lib \
	-L. \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lFASTLib \
	-lUtilityLib \
	-lrt \
	-L./libmodbus-3.0.6/src/.libs -l modbus \
	-lpthread

EXEC=isc-modbus

CFLAGS=-g -O0  -DSTATIC_HOOKS

SRC = \
	SysParameters.cpp \
	main.cpp

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC)  -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	${CROSS_COMPILE}strip bin/$(ARCH)/$(EXEC)

clean:
	rm -rf bin
