ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../state-machine \
	-I../FastTrac \
	-I../FastTrac/FASTLib \
	-I../FastTrac/UtilityLib \
	-I../db-monitor 

LD_FLAGS=\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-ldb-monitor \
	-lFASTLib \
	-lUtilityLib \
	-lstate-machine \
	-lats-common \
	-lcommand_line_parser \
	-lsocket_interface \
	-lpthread \
	-lrt \
	-s

EXEC=heartbeat

CFLAGS=-Wall -Os

SRC = \
	heartbeat-state-machine.cpp \
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
