ARCH=PC

INCLUDE =\
	-I../redstone-socketcan \
        -I../linux-2.6.35.3/include

LD_FLAGS=\
	-lredstone-socketcan \
	-L../redstone-socketcan/bin/$(ARCH)

all:
	mkdir -p bin
	${CROSS_COMPILE}g++ -o bin/can-test main.cpp -Wall $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
