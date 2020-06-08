ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../db-monitor \
	-I../


LD_FLAGS=\
	-L../socket_interface/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-lsocket_interface \
	-lcommand_line_parser \
	-lats-common \
	-ldb-monitor \
	-lpthread \
	-lrt \
	-s

EXEC=wifi-monitor

SRC = \
	main.cpp \
	AdminCommandContext.cpp

CFLAGS=-Os -Wall -Xlinker -s

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)
	${CROSS_COMPILE}strip bin/$(ARCH)/$(EXEC)

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $< $(CFLAGS) $(INCLUDE)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

clean:
	rm -rf bin
	
