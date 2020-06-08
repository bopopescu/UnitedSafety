ARCH=PC

ifeq ($(REDSTONE_EXPAT_INCLUDE_DIR),)
REDSTONE_EXPAT_INCLUDE_DIR=../../3rd-party/expat-2.0.1/lib
endif

ifeq ($(REDSTONE_LIBMODBUS_INCLUDE_DIR),)
REDSTONE_LIBMODBUS_INCLUDE_DIR=../../3rd-party/libmodbus-3.0.4/src/
endif

ifeq ($(REDSTONE_EXPAT_LIB_DIR),)
REDSTONE_EXPAT_LIB_DIR=../../3rd-party/expat-2.0.1/.libs/
endif

ifeq ($(REDSTONE_LIBMODBUS_LIB_DIR),)
REDSTONE_LIBMODBUS_LIB_DIR=../../3rd-party/libmodbus-3.0.4/src/.libs/
endif


INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../libb64-1.2/include \
	-I../state-machine \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../FastTrac/ \
	-I$(REDSTONE_EXPAT_INCLUDE_DIR) \
	-I$(REDSTONE_LIBMODBUS_INCLUDE_DIR) \
	-I../feature-monitor \
	-I../db-monitor

LD_FLAGS=\
	-L../db-monitor/bin/$(ARCH) \
	-ldb-monitor \
	-L../command_line_parser/bin/$(ARCH) \
	-lcommand_line_parser \
	-L../socket_interface/bin/$(ARCH) \
	-lsocket_interface \
	-L../state-machine/bin/$(ARCH) \
	-lstate-machine \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lexpat \
	-L$(REDSTONE_EXPAT_LIB_DIR) \
	-lmodbus \
	-L$(REDSTONE_LIBMODBUS_LIB_DIR) \
	-lats-common \
	-lpthread \
	-lrt \
	-s

EXEC=modbus-monitor

CFLAGS=-Wall -Os \
	$(REDSTONE_CFLAGS)

SRC = \
	main.cpp \
	signal-monitor.cpp

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
