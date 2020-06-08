ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../db-monitor \
	-I../socket_interface

LD_FLAGS=\
	-L../db-monitor/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-ldb-monitor \
	-lats-common \
	-lsocket_interface \
	-lpthread \
	-s

EXEC=port-forward

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
