
ARCH=PC
UTILITY_LIB_DIR='../UtilityLib'
FASTRAC_INCLUDE_DIR='../../include'

LIB_OUTPUT=libFASTLib.a

SRC=\
	AFFData.cpp\
	AFFDevice.cpp\
	DeviceBase.cpp\
	FASTTrackData.cpp\
	NMEA_Client.cpp\
	SERDevice.cpp

LIBS = \
LIBPATH =-L../../ats-common/bin/$(ARCH)


INCLUDE = \
	-I'..' \
	-I${FASTRAC_INCLUDE_DIR} \
	-I${UTILITY_LIB_DIR} \
	-I../../ats-common
	
CFLAGS = -Wall -Wextra  $(INCLUDE) -Os

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
