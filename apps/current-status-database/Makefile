
ARCH=PC
KERNEL_SRC_DIR='../mx28/linux-2.6.35.3'
SQLITE3_SRC_DIR='../sqlite3-3.6.22/src'
FASTTRAC_SRC_DIR='../FastTrac/'

LIB_OUTPUT = bin/$(ARCH)/libcurrent-status-database.so
LIB_OUTPUT_STATIC = bin/$(ARCH)/static/libcurrent-status-database.a

SRC = \
	current-status-database.cpp

LIBS = \
	-lpthread \
	-lFASTLib \
	-lUtilityLib

LIBPATH = \
	-L$(FASTTRAC_SRC_DIR)/FASTLib/bin/$(ARCH)/ \
	-L$(FASTTRAC_SRC_DIR)/UtilityLib/bin/$(ARCH)/

INCLUDE = \
	-I${KERNEL_SRC_DIR}/include \
	-I${FASTTRAC_SRC_DIR}/FASTLib/ \
	-I${FASTTRAC_SRC_DIR}/UtilityLib/ 

CFLAGS = -fPIC $(INCLUDE) $(LIBPATH) $(LIBS)

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
