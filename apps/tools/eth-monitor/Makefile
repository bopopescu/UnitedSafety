
ARCH:=PC

EXEC=eth-monitor

ifeq ($(REDSTONE_BUILD_DIR), )
D=../..
else
D=..
endif

CFLAGS=\
	-I$D/ats-common \
	-I$D/db-monitor

LDFLAGS=\
	-L$D/ats-common/bin/$(ARCH) \
	-L$D/db-monitor/bin/$(ARCH) \
	-L$D/socket_interface/bin/$(ARCH) \
	-ldb-monitor \
	-lsocket_interface \
	-lats-common \
	-lpthread

all:
	${CROSS_COMPILE}g++ -Wall -o $(EXEC) main.cpp -Os -Xlinker -s $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(EXEC)
