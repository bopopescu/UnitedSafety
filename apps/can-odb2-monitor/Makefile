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
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../FastTrac/ \
	-I../command_line_parser \
	-I../db-monitor

LD_FLAGS=\
	-L../redstone-socketcan/bin/$(ARCH)/static \
	-L../db-monitor/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-ldb-monitor \
	-lredstone-socketcan \
	-lats-common \
	-lsocket_interface \
	-lcommand_line_parser \
	-lpthread \
	-lrt \
	-s

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/can-odb2-monitor main.cpp -Wall -Os $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/can-odb2-monitor main.cpp -Wall -Os $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
