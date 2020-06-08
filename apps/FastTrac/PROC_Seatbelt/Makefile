
ARCH=PC

EXEC=PROC_Seatbelt

SRC = \
	main.cpp\
	Proc_SeatbeltStateMachine.cpp

LIBS = \
	-lrt \
	-lFASTLib \
	-lUtilityLib \
	-lats-common \
	-lsocket_interface \
	-lcommand_line_parser \
	-lstate-machine\
	-ldb-monitor

LIBPATH =\
	-L../UtilityLib/bin/$(ARCH) \
	-L../FASTLib/bin/$(ARCH) \
	-L../../ats-common/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../command_line_parser/bin/$(ARCH) \
	-L../../state-machine/bin/$(ARCH) \
	-L../../db-monitor/bin/$(ARCH)

INCLUDE = \
	-I../ \
	-I../UtilityLib \
	-I../FASTLib \
	-I../../ats-common \
	-I../../socket_interface \
	-I../../command_line_parser \
	-I../../state-machine \
	-I../../buzzer-monitor \
	-I../../db-monitor

CFLAGS = $(INCLUDE) -Os -Xlinker -s

OBJS = $(SRC:%.cpp=bin/PC/%.o)
OBJS_ARM = $(SRC:%.cpp=bin/arm/%.o)

all: $(ARCH)/$(EXEC)

PC/$(EXEC): bin/$(ARCH) $(OBJS) Makefile
	g++ -o bin/$@ $(OBJS) $(LIBPATH) $(LIBS)

arm/$(EXEC): bin/$(ARCH) $(OBJS_ARM) Makefile
	$(CROSS_COMPILE)g++ -o bin/$@ $(OBJS_ARM) $(LIBPATH) $(LIBS)

$(OBJS): bin/PC/%.o: %.cpp Makefile bin/$(ARCH)
	g++ $(CFLAGS) -c -o $@ $<

$(OBJS_ARM): bin/arm/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $<

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
