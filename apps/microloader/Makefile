ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../libb64-1.2/include \
	-I../state-machine \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../FastTrac/ \
	-I../buzzer-monitor \
	-I${KERNEL_SRC_DIR}/include \
	-I../db-monitor

ifneq ($(REDSTONE_BUILD_DIR),)
INCLUDE +=\
	-I$(REDSTONE_BUILD_DIR)/include/i2c-tools-3.0.2
else
INCLUDE +=\
	-I../../3rd-party/i2c-tools-3.0.2/include
endif

LD_FLAGS=\
	-s \
	-L../db-monitor/bin/$(ARCH) \
	-ldb-monitor \
	-L../command_line_parser/bin/$(ARCH)/ \
	-lcommand_line_parser \
	-L../socket_interface/bin/$(ARCH)/ \
	-lsocket_interface \
	-L../state-machine/bin/$(ARCH) \
	-lstate-machine \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lats-common \
	-lpthread \
	-lrt

EXEC=microloader

CFLAGS=-Wall -Os \
	$(REDSTONE_CFLAGS)

SRC = \
	targetwrite.cpp  \
	heximage.cpp  \
	main.cpp

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC) $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
