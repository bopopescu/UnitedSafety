ARCH=PC

INCLUDE =\
	-I../db-monitor \
	-I../socket_interface \
	-I../command_line_parser \
	-I../ats-common

ifneq ($(REDSTONE_BUILD_DIR),)
INCLUDE +=\
	-I$(REDSTONE_BUILD_DIR)/include/i2c-tools-3.0.2
else
INCLUDE +=\
	-I../../3rd-party/i2c-tools-3.0.2/include
endif

ifeq ($(REDSTONE_SQLITE3_INCLUDE_DIR),)
REDSTONE_SQLITE3_INCLUDE_DIR=../../3rd-party/sqlite3-3.6.22
endif

ifeq ($(REDSTONE_SQLITE3_LIB_DIR),)
REDSTONE_SQLITE3_LIB_DIR=../../3rd-party/sqlite3-3.6.22/.libs
endif

LD_FLAGS=\
	-L../db-monitor/bin/$(ARCH) \
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-ldb-monitor \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lsqlite3 \
	-lpthread \
	-lrt \
	-s

EXEC=rtc-monitor

SOURCE=\
	MyData.cpp \
	OnTime.cpp \
	TimerControlBlock.cpp \
	TimerEngine.cpp \
	main.cpp \
	RegisteredLogger.cpp \
	RegisteredLogger_sql.cpp

all: $(ARCH)

PC:
	mkdir -p bin/$(ARCH)
	g++ -o bin/$(ARCH)/$(EXEC) $(SOURCE) -Wall -Os $(INCLUDE) $(LD_FLAGS)

arm:
	mkdir -p bin/$(ARCH)
	${CROSS_COMPILE}g++ -o bin/$(ARCH)/$(EXEC) $(SOURCE) -Wall -Os $(INCLUDE) $(LD_FLAGS)

clean:
	rm -rf bin
