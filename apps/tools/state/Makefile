
ARCH:=PC

EXEC=bin/$(ARCH)/state

ifeq ($(REDSTONE_BUILD_DIR), )
D=../..
else
D=..
endif

CFLAGS=\
	-I$D/db-monitor \
	-I$D/state-machine \
	-I$D/socket_interface \
	-I$D/command_line_parser \
	-I$D/FastTrac/UtilityLib \
	-I$D/FastTrac/FASTLib \
	-I$D/FastTrac \
	-I$D/ats-common \
	-I$D/feature-monitor

LDFLAGS=\
	-L$D/db-monitor/bin/$(ARCH) \
	-L$D/state-machine/bin/$(ARCH) \
	-L$D/socket_interface/bin/$(ARCH) \
	-L$D/command_line_parser/bin/$(ARCH) \
	-L$D/ats-common/bin/$(ARCH) \
	-L$D/FastTrac/UtilityLib/bin/$(ARCH) \
	-ldb-monitor \
	-lstate-machine \
	-lsocket_interface \
	-lcommand_line_parser \
	-lats-common \
	-lrt \
	-lpthread

all: | bin/$(ARCH)
	${CROSS_COMPILE}g++ -Wall -o $(EXEC) main.cpp essid.cpp -Os -Xlinker -s $(CFLAGS) $(LDFLAGS)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

clean:
	rm -rf bin
