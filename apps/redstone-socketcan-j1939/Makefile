
ARCH=PC

LIB_OUTPUT = bin/$(ARCH)/libredstone-socketcan-j1939.so
LIB_OUTPUT_STATIC = bin/$(ARCH)/static/libredstone-socketcan-j1939.a

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
