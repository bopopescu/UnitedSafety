ARCH=PC
KERNEL_SRC_DIR='../mx28/linux-2.6.35.3'

INCLUDE =\
        -I${KERNEL_SRC_DIR}/include

ifneq ($(REDSTONE_BUILD_DIR),)
INCLUDE +=\
	-I$(REDSTONE_BUILD_DIR)/include/i2c-tools-3.0.2
else
INCLUDE +=\
	-I../../../3rd-party/i2c-tools-3.0.2/include
endif

LD_FLAGS=\
	-lpthread \
	-s

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	gcc -o bin/$(ARCH)/supports-latching main.c -Wall -Os $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}gcc -o bin/$(ARCH)/supports-latching main.c -Wall -Os $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
