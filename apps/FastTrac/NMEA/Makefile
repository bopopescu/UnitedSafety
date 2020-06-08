ARCH=PC

EXEC=NMEA

SRC = \
	main.cpp\
	NMEA.cpp\
	MyData.cpp

LIBS = \
	-ldb-monitor \
	-lstate-machine \
	-lFASTLib \
	-lUtilityLib \
	-lats-common \
	-lcommand_line_parser \
	-lsocket_interface \
	-lrt

LIBPATH =\
	-L../UtilityLib/bin/$(ARCH) \
	-L../FASTLib/bin/$(ARCH) \
	-L../../ats-common/bin/$(ARCH) \
	-L../../db-monitor/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../state-machine/bin/$(ARCH) \
	-L../../command_line_parser/bin/$(ARCH)
	
INCLUDE = \
	-I../ \
	-I../UtilityLib \
	-I../../ats-common \
	-I../FASTLib \
	-I../../db-monitor \
	-I../../socket_interface \
	-I../../command_line_parser\
	-I../../state-machine

CFLAGS = $(INCLUDE)

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
