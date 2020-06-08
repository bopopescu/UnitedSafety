ARCH=PC

INCLUDE =\
	-I../redstone-socketcan

LD_FLAGS=\
	-lredstone-socketcan \
	-L../redstone-socketcan/bin/$(ARCH)

all:
	mkdir -p bin
	${CROSS_COMPILE}g++ -o bin/can-test main.cpp -Wall $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
