
ARCH=PC

ifneq ($(REDSTONE_BUILD_DIR),)
KERNEL_SRC_DIR='../mx28/linux-2.6.35.3'
else
KERNEL_SRC_DIR='../../mx28/linux-2.6.35.3'
endif

LIB_OUTPUT = bin/$(ARCH)/libredstone-socketcan.so
LIB_OUTPUT_STATIC = bin/$(ARCH)/static/libredstone-socketcan.a

SRC = \
	main.cpp

LIBS = \
	-lpthread

LIBPATH =

INCLUDE = \
	-I${KERNEL_SRC_DIR}/include

CFLAGS = -fPIC $(INCLUDE)

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

all: $(LIB_OUTPUT) $(LIB_OUTPUT_STATIC)

$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	$(CROSS_COMPILE)g++ -shared -o $@ $(OBJS) $(LIBPATH) $(LIBS)

$(LIB_OUTPUT_STATIC): bin/$(ARCH)/static $(OBJS) Makefile
	$(CROSS_COMPILE)ar -r $@ $(OBJS)

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $<

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

bin/$(ARCH)/static:
	mkdir -p bin/$(ARCH)/static

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
