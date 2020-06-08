ARCH=PC

INCLUDE =\
	-I../../ats-common \
	-I../../FastTrac/UtilityLib \
	-I../../FastTrac/FASTLib \
	-I../../FastTrac/ \
	-I../../command_line_parser \
	-I../../db-monitor

LD_FLAGS=\
	-s \
	-L../../db-monitor/bin/$(ARCH) \
	-ldb-monitor \
	-L../../command_line_parser/bin/$(ARCH)/ \
	-lcommand_line_parser \
	-L../../ats-common/bin/$(ARCH) \
	-lUtilityLib \
	-L../../FastTrac/UtilityLib/bin/$(ARCH) \
	-lats-common \
	-lpthread \
	-lrt

EXEC=gps-sim

CFLAGS=-Wall -Os

SRC = \
	main.cpp

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SRC) -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SRC) $(CFLAGS) $(INCLUDE) $(LD_FLAGS)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

.PHONY:
clean:
	rm -rf bin/$(ARCH)/*
