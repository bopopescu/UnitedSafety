ARCH=PC

INCLUDE =\
	-I../db-monitor \
	-I../state-machine \
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../FastTrac/UtilityLib \
	-I../FastTrac/FASTLib \
	-I../FastTrac \
	-I../


LD_FLAGS=\
	-L../db-monitor/bin/$(ARCH) \
	-L../state-machine/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../db-monitor/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L../FastTrac/FASTLib/bin/$(ARCH) \
	-L../FastTrac/UtilityLib/bin/$(ARCH) \
	-ldb-monitor \
	-lstate-machine \
	-lsocket_interface \
	-lcommand_line_parser \
	-ldb-monitor \
	-lats-common \
	-lFASTLib \
	-lUtilityLib \
	-lpthread \
	-lrt \
	-s

EXEC=telit-monitor

SRC = \
	main.cpp\
	ATCCID_AMH.cpp\
	ATCGDCONT_AMH.cpp\
	ATCIMI_AMH.cpp\
	ATCOPS_AMH.cpp\
	ATCREG_AMH.cpp\
	ATCSQ_AMH.cpp\
	PACSP1_AMH.cpp\
	ATRFSTS_AMH.cpp\
	ATPSNT_AMH.cpp\
	ATCNUM_AMH.cpp\
	PBREADY.cpp
	
CFLAGS=-Wall -Os

OBJS = $(SRC:%.cpp=bin/$(ARCH)/%.o)

all: $(ARCH)

PC: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

arm: bin/$(ARCH) $(OBJS) Makefile
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(OBJS) $(LD_FLAGS)

$(OBJS): bin/$(ARCH)/%.o: %.cpp Makefile bin/$(ARCH)
	$(CROSS_COMPILE)g++ $(CFLAGS) -c -o $@ $< $(CFLAGS) $(INCLUDE)

bin/$(ARCH):
	mkdir -p bin/$(ARCH)

clean:
	rm -rf bin
	
