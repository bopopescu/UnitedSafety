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

LD_FLAGS=\
	-s \
	-L../db-monitor/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH)/ \
	-L../socket_interface/bin/$(ARCH)/ \
	-ldb-monitor \
	-lUtilityLib \
	-lstate-machine \
	-lats-common \
	-lcommand_line_parser \
	-lsocket_interface \
	-lpthread \
	-lrt

EXEC=zigbee-monitor

CFLAGS=-Wall -Os

SRC = \
	messages.cpp \
	fobSm.cpp \
	calampdecode.cpp \
	iridiumdecode.cpp \
	main.cpp

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC) $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/zigbee-mfg-test mfgtest.cpp $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	${CROSS_COMPILE}strip bin/$(ARCH)/$(EXEC)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
