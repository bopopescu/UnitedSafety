
CFLAGS=-Wall -Werror -Os -Xlinker -s

INCLUDE=\
	-I../ats-common \
	-I../command_line_parser \
	-I../socket_interface \
	-I../FastTrac \
	-I../FastTrac/FASTLib \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/NMEA \
	-I../db-monitor

LIBS=\
	-L../db-monitor/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L. \
	-lroadinfo \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lFASTLib \
	-lUtilityLib \
	-lats-common \
	-lrt \
	-lpthread

all:
	mkdir -p bin
	${CROSS_COMPILE}g++ $(CFLAGS) $(INCLUDE) -o bin/SkyBase main.cpp $(LIBS) 

clean:
	rm -rf bin
