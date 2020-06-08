ARCH=PC


ifeq ($(REDSTONE_BUILD_DIR), )
D=../..
else
D=..
endif

INCLUDE =\
	-I$D/ats-common \
	-I$D/socket_interface \
	-I$D/command_line_parser \
	-I$D/db-monitor \
	-I$D/FastTrac/FASTLib \
	-I$D/FastTrac/UtilityLib \
	-I$D/FastTrac

LD_FLAGS=\
	-L$D/command_line_parser/bin/$(ARCH) \
	-L$D/socket_interface/bin/$(ARCH) \
	-L$D/ats-common/bin/$(ARCH) \
	-L$D/db-monitor/bin/$(ARCH) \
	-L$D/FastTrac/FASTLib/bin/$(ARCH) \
	-L$D/FastTrac/UtilityLib/bin/$(ARCH) \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lFASTLib \
	-lUtilityLib \
	-lrt \
	-lpthread \
	-s

EXEC=connect-ppp

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
