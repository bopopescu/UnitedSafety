ARCH=PC

ifeq ($(REDSTONE_BUILD_DIR), )
D=../..
PCAP_DIR=../../../3rd-party/libpcap-1.3.0
else
D=..
PCAP_DIR=../libpcap-1.3.0
endif

INCLUDE =\
	-I$D/ats-common \
	-I$D/socket_interface \
	-I$D/command_line_parser \
	-I$D/buzzer-monitor \
	-I$D/FastTrac/UtilityLib \
	-I$D/FastTrac/FASTLib \
	-I$D/FastTrac \
        -I$D/db-monitor \
	-I$D/state-machine \
	-I$(PCAP_DIR)

LD_FLAGS=\
	-L$D/command_line_parser/bin/$(ARCH) \
	-L$D/socket_interface/bin/$(ARCH) \
	-L$D/ats-common/bin/$(ARCH) \
	-L$D/FastTrac/FASTLib/bin/$(ARCH) \
	-L$D/FastTrac/UtilityLib/bin/$(ARCH) \
        -L$D/db-monitor/bin/$(ARCH) \
	-L$D/state-machine/bin/$(ARCH) \
	-L$(PCAP_DIR) \
	-lstate-machine \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lFASTLib \
	-lUtilityLib \
	-lpcap \
	-lrt \
	-lpthread \
	-s

EXEC=wifi-client-monitor

CFLAGS=-Os

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
