ARCH=PC

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../db-monitor \
	-I../FastTrac/FASTLib \
	-I../FastTrac/UtilityLib \
	-I../FastTrac

LD_FLAGS=\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-lrt \
	-ldb-monitor \
	-lsocket_interface \
	-lats-common \
	-lFASTLib \
	-lUtilityLib \
	-lpthread

EXEC=modem-monitor

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) main.cpp gemalto.cpp -Wall $(INCLUDE) -s $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) main.cpp -Wall $(INCLUDE) -s $(LD_FLAGS)

clean:
	rm -rf bin
