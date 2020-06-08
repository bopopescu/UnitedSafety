
ARCH=PC

ifeq ($(REDSTONE_BUILD_DIR),)
LIB_OUTPUT = bin/PC/libTLP.a
LIB_OUTPUT_ARM = bin/arm/libTLP.a
LDFLAGS =
else
SHARED=1
LIB_OUTPUT = bin/PC/libTLP.so
LIB_OUTPUT_ARM = bin/arm/libTLP.so
LDFLAGS = -shared
CFLAGS = -fPIC
endif

SRC = \
	TLPClass.cpp \
	tlp.cpp \
	TLPEvent.cpp \
	TLPMessage.cpp \
	TLPFile.cpp	 

LIBS = \
	-lpthread

LIBPATH =

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

CFLAGS += -Wall -Os $(INCLUDE) ${REDSTONE_GCC_MACHINE}

OBJS = $(SRC:%.cpp=bin/PC/%.o)

OBJS_ARM = $(SRC:%.cpp=bin/arm/%.o)

all: $(ARCH)

PC: $(LIB_OUTPUT)
arm: $(LIB_OUTPUT_ARM)

$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	ar -r $@ $(OBJS)
	g++ $(CFLAGS) -o bin/$(ARCH)/TLP-test TLP-test.cpp -Lbin/$(ARCH) -lTLP $(LDFLAGS) $(LIBS)

ifeq ($(SHARED),1)
$(LIB_OUTPUT_ARM): bin/$(ARCH) $(OBJS_ARM) Makefile
	$(CROSS_COMPILE)g++ $(LDFLAGS) -o $@ $(OBJS_ARM) $(LIBPATH) $(LIBS)
else
$(LIB_OUTPUT_ARM): bin/$(ARCH) $(OBJS_ARM) Makefile
	$(CROSS_COMPILE)ar -r $@ $(OBJS_ARM)
endif

$(OBJS): bin/PC/%.o: %.cpp Makefile | bin/$(ARCH)
	g++ $(CFLAGS) -c -o $@ $<

$(OBJS_ARM): bin/arm/%.o: %.cpp Makefile | bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $<

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*

