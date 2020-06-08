
ARCH=PC

ifeq ($(REDSTONE_BUILD_DIR),)
LIB_OUTPUT = bin/PC/libats-common.a
LIB_OUTPUT_ARM = bin/arm/libats-common.a
LDFLAGS =
else
SHARED=1
LIB_OUTPUT = bin/PC/libats-common.so
LIB_OUTPUT_ARM = bin/arm/libats-common.so
LDFLAGS = -shared
CFLAGS = -fPIC
endif

SRC = \
	ats-common.cpp \
	ats-string.cpp \
	atslogger.cpp \
	ats-serial.cpp \
	ats-security.cpp \
	base64.cpp

LIBS =	-lpthread

LIBPATH =

INCLUDE =

CFLAGS += -Wall -Os $(INCLUDE) ${REDSTONE_GCC_MACHINE}

OBJS = $(SRC:%.cpp=bin/PC/%.o)

OBJS_ARM = $(SRC:%.cpp=bin/arm/%.o)

all: $(ARCH)

PC: $(LIB_OUTPUT)
arm: $(LIB_OUTPUT_ARM)

$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	ar -r $@ $(OBJS)
	g++ $(CFLAGS) -o bin/$(ARCH)/ats-common-test ats-common-test.cpp -Lbin/$(ARCH) -lats-common $(LDFLAGS) $(LIBS)

ifeq ($(SHARED),1)
$(LIB_OUTPUT_ARM): bin/$(ARCH) $(OBJS_ARM) Makefile
	$(CROSS_COMPILE)g++ $(LDFLAGS) -o $@ $(OBJS_ARM) $(LIBPATH) $(LIBS)
	$(CROSS_COMPILE)g++ $(CFLAGS) -o bin/$(ARCH)/ats-common-test ats-common-test.cpp -Lbin/$(ARCH) -lats-common $(LDFLAGS) $(LIBS)
else
$(LIB_OUTPUT_ARM): bin/$(ARCH) $(OBJS_ARM) Makefile
	$(CROSS_COMPILE)ar -r $@ $(OBJS_ARM)
	$(CROSS_COMPILE)g++ $(CFLAGS) -o bin/$(ARCH)/ats-common-test ats-common-test.cpp -Lbin/$(ARCH) -lats-common $(LDFLAGS) $(LIBS) -lpthread
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
