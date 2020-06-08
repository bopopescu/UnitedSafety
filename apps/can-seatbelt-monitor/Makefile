ARCH=PC

ifneq ($(REDSTONE_BUILD_DIR),)
KERNEL_SRC_DIR='../mx28/linux-2.6.35.3'
else
KERNEL_SRC_DIR='../../mx28/linux-2.6.35.3'
endif

INCLUDE =\
	-I../ats-common \
	-I../redstone-socketcan \
        -I${KERNEL_SRC_DIR}/include \
	-I../socket_interface \
	-I../FastTrac \
	-I../FastTrac/FASTLib \
	-I../FastTrac/UtilityLib \
	-I../command_line_parser

LD_FLAGS=\
	-L../redstone-socketcan/bin/$(ARCH)/static \
	-L../ats-common/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-lsocket_interface \
	-lcommand_line_parser \
	-lats-common \
	-lredstone-socketcan \
	-lpthread \
	-lrt \
	-s

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/can-seatbelt-monitor main.cpp -Wall -Os $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/can-seatbelt-monitor main.cpp -Wall -Os $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
