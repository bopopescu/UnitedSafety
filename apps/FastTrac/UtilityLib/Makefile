
ARCH=PC

LIB_OUTPUT=libUtilityLib.a

SRC=\
	angle.cpp\
	BufferedFile.cpp\
	CSelect.cpp\
	checksum.cpp \
	dbls_eq.cpp\
	ellipse.cpp\
	fragment.cpp\
	J2K.cpp\
	keyboard.cpp\
	Lat.cpp\
	Lon.cpp\
	tinystr.cpp\
	tinyxml.cpp\
	tinyxmlerror.cpp\
	tinyxmlparser.cpp\
	utility.cpp\
	GetInterfaceAddr.cpp\
	WriteDebug.cpp

LIBS = \

LIBPATH =-L../../ats-common/bin/$(ARCH)\

INCLUDE = -I../../ats-common\

CFLAGS = $(INCLUDE) -Os

OBJS = $(SRC:%.cpp=bin/PC/%.o)
OBJS_ARM = $(SRC:%.cpp=bin/arm/%.o)

all: $(ARCH)/$(LIB_OUTPUT)

PC/$(LIB_OUTPUT): bin/$(ARCH) $(OBJS) Makefile
	ar -r bin/$@ $(OBJS)

arm/$(LIB_OUTPUT): bin/$(ARCH) $(OBJS_ARM) Makefile
	$(CROSS_COMPILE)ar -r bin/$@ $(OBJS_ARM)

$(OBJS): bin/PC/%.o: %.cpp Makefile bin/$(ARCH)
	g++ $(CFLAGS) -c -o $@ $<

$(OBJS_ARM): bin/arm/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $<

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
