ARCH=PC

INCLUDE =\
	-I../../isc-lens \
	-I../../ats-common \
	-I../../socket_interface \
	-I../../command_line_parser \
	-I../../state-machine \
	-I../../FastTrac/UtilityLib \
	-I../../FastTrac/FASTLib \
	-I../../FastTrac \
	-I../../db-monitor \
	-I../../include \
	-I../../../3rd-party/rapidjson/include

LD_FLAGS=\
	-L../../db-monitor/bin/$(ARCH) \
	-L../../command_line_parser/bin/$(ARCH) \
	-L../../socket_interface/bin/$(ARCH) \
	-L../../ats-common/bin/$(ARCH) \
	-L../../FastTrac/FASTLib/bin/$(ARCH) \
	-L../../state-machine/bin/$(ARCH) \
	-L../../FastTrac/UtilityLib/bin/$(ARCH) \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lstate-machine \
	-lFASTLib \
	-lUtilityLib \
	-lrt \
	-lpthread

EXEC=LENSUploadMode

CFLAGS=-g -O0

SRC = \
	../../isc-lens/lens.cpp \
	../../isc-lens/LensRegisters.cpp \
	main.cpp

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) ${SRC)  -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -Wall $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
