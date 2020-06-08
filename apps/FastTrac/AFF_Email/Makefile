
ARCH=PC

EXEC=AFF_Email

SRC = \
	main.cpp\
	AFF_Email.cpp

LIBS = \
	-lrt \
	-lFASTLib \
	-lUtilityLib \
	-ldb-monitor \
	-lsocket_interface \
	-lats-common

LIBPATH =\
	-L../FASTLib/bin/$(ARCH) \
	-L../UtilityLib/bin/$(ARCH) \
	-L../../ats-common/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../db-monitor/bin/$(ARCH)

INCLUDE = \
	-I../ \
	-I../FASTLib \
	-I../UtilityLib \
	-I../../db-monitor \
	-I../../ats-common

CFLAGS = -Wall -Os $(INCLUDE)

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
