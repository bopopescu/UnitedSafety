ARCH=PC

INCLUDE =\
	-I../../ats-common \
	-I../../socket_interface \
	-I../../command_line_parser \
	-I../../buzzer-monitor \
	-I../../FastTrac/UtilityLib \
	-I../../FastTrac/FASTLib \
	-I../../FastTrac \

LD_FLAGS=\
	-L../../command_line_parser/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../ats-common/bin/$(ARCH) \
	-L../../FastTrac/FASTLib/bin/$(ARCH) \
	-L../../FastTrac/UtilityLib/bin/$(ARCH) \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lFASTLib \
	-lUtilityLib \
	-lrt \
	-lpthread \
	-s

EXEC=iridium-emulator

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
