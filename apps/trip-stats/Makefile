ARCH=PC
KERNEL_SRC_DIR='../mx28/linux-2.6.35.3'

INCLUDE =\
	-I../ats-common \
        -I${KERNEL_SRC_DIR}/include \
	-I../socket_interface \
	-I../command_line_parser \
	-I../FastTrac/UtilityLib

LD_FLAGS=\
	-L../ats-common/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-lats-common \
	-lsocket_interface \
	-lcommand_line_parser \
	-lrt \
	-lpthread \
	-s

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/trip-stats main.cpp -Wall -Os $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/trip-stats main.cpp -Wall -Os $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
