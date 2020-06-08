ARCH=PC

ifneq ($(REDSTONE_BUILD_DIR),)
KERNEL_SRC_DIR='../mx28/linux-2.6.35.3'
else
KERNEL_SRC_DIR='../../mx28/linux-2.6.35.3'
endif

ifeq ($(REDSTONE_CAN_J1939_MONITOR_INCLUDE_DIR),)
REDSTONE_CAN_J1939_MONITOR_INCLUDE_DIR=../../3rd-party/expat-2.0.1/lib
endif

ifeq ($(REDSTONE_CAN_J1939_MONITOR_LIB_DIR),)
REDSTONE_CAN_J1939_MONITOR_LIB_DIR=../../3rd-party/expat-2.0.1/.libs/
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
	-I../buzzer-monitor \
	-I$(REDSTONE_CAN_J1939_MONITOR_INCLUDE_DIR) \
	-I../redstone-socketcan-j1939 \
	-I${KERNEL_SRC_DIR}/include \
	-I../feature-monitor \
	-I../db-monitor

LD_FLAGS=\
	-L../redstone-socketcan-j1939/bin/$(ARCH)/static \
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L$(REDSTONE_CAN_J1939_MONITOR_LIB_DIR) \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lstate-machine \
	-lexpat \
	-lredstone-socketcan-j1939 \
	-lats-common \
	-lpthread \
	-lrt \
	-s

EXEC=can-j1939-monitor

CFLAGS=-Wall -Os

SRC = \
	main.cpp \
	signal-monitor.cpp \
	can-j1939-monitor-state-machine.cpp

SRC_SENDMESSAGE = \
	j1939messagesend.cpp

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	g++ -o bin/$(ARCH)/j1939messagesend $(SRC_SENDMESSAGE) -s $(CFLAGS) $(INCLUDE) -Lbin/$(ARCH) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/j1939messagesend $(SRC_SENDMESSAGE) -s $(CFLAGS) $(INCLUDE) -Lbin/$(ARCH) $(LD_FLAGS)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
