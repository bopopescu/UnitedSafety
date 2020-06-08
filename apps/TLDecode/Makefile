ARCH=PC

EXEC=TLDecode

SRC = \
	main.cpp

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
	-L../Libs/bin/$(ARCH) 
	
INCLUDE = \
	-I../ \
	-I../FastTrac/UtilityLib \
	-I../FastTrac \
	-I../FastTrac/FASTLib \
	-I../ats-common \
	-I../db-monitor \
	-I../socket_interface \
	-I../command_line_parser\
	-I../state-machine

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
