ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../db-monitor \
	-I../message-assembler \
	-I../state-machine \
	-I../FastTrac/ \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../packetizer-lib

LD_FLAGS=\
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-lsocket_interface \
	-lats-common \
	-ldb-monitor \
	-lstate-machine \
	-lpthread \
	-lrt \
	-s

EXEC=packetizer

CFLAGS=-Wall -Os

SRC = \
     packetizerDB.cpp \
     packetizerSender.cpp \
     packetizerMessage.cpp \
     packetizer_state_machine.cpp \
     packetizerCopy.cpp \
     midDB.cpp \
	 main.cpp

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

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
