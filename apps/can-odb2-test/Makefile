ARCH=PC

INCLUDE =\
	-I../redstone-socketcan \
        -I../mx28/linux-2.6.35.3/include

LD_FLAGS=\
	-lredstone-socketcan \
	-L../redstone-socketcan/bin/$(ARCH) \
	-lrt
all:
	mkdir -p bin
	${CROSS_COMPILE}g++ -o bin/can-odb2-test main.cpp -Wall $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
