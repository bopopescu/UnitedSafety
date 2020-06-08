ARCH=PC

#========
#= CONFIG_DB_FNAME
#= Description: When building a PC version, a test file is used instead of the
#=              normal NVRAM directory location.
#========
ifeq ($(ARCH), PC)
CONFIG_DB_FNAME="\"/tmp/redstone-db-monitor-test-config.db\""
else
CONFIG_DB_FNAME="\"/mnt/nvram/config/config.db\""
REDSTONE_BUILD=-DREDSTONE_BUILD
endif

ifeq ($(REDSTONE_SQLITE3_INCLUDE_DIR),)
REDSTONE_SQLITE3_INCLUDE_DIR=../../3rd-party/sqlite3-3.6.22
endif

ifeq ($(REDSTONE_SQLITE3_LIB_DIR),)
REDSTONE_SQLITE3_LIB_DIR=../../3rd-party/sqlite3-3.6.22/.libs
endif
REDSTONE_SQLITE3_LIB_DIR=../include/sqlite3-3.6.22/.libs
REDSTONE_SQLITE3_INCLUDE_DIR="../include/sqlite3-3.6.22"

INCLUDE =\
	-I../ats-common \
	-I../socket_interface \
	-I../command_line_parser \
	-I../FastTrac \
	-I$(REDSTONE_SQLITE3_INCLUDE_DIR)

LD_FLAGS =\
	-L../command_line_parser/bin/$(ARCH) \
	-L../socket_interface/bin/$(ARCH) \
	-L../ats-common/bin/$(ARCH) \
	-L$(REDSTONE_SQLITE3_LIB_DIR) \
	-lcommand_line_parser \
	-lsocket_interface \
	-lats-common \
	-lpthread \
	-lsqlite3

CFLAGS =\
	-Wall \
	-Os \
	-DCONFIG_DB_FNAME=$(CONFIG_DB_FNAME) \
	$(REDSTONE_BUILD)

EXEC=db-monitor

LIB_OUTPUT=libdb-monitor.a

all: $(ARCH)

$(ARCH):
	mkdir -p bin/$(ARCH)
	$(CROSS_COMPILE)g++ -o bin/$(ARCH)/$(EXEC) main.cpp MyData.cpp DBMapping.cpp DBLock.cpp -s $(CFLAGS) $(INCLUDE) $(LD_FLAGS)
	$(CROSS_COMPILE)g++ -o bin/$(ARCH)/db-monitor.o -c db-monitor.cpp $(CFLAGS) $(INCLUDE)
	$(CROSS_COMPILE)g++ -o bin/$(ARCH)/ConfigDB.o -c ConfigDB.cpp $(CFLAGS) $(INCLUDE)
	$(CROSS_COMPILE)ar -r bin/$(ARCH)/$(LIB_OUTPUT) bin/$(ARCH)/db-monitor.o 
	$(CROSS_COMPILE)ar -r bin/$(ARCH)/$(LIB_OUTPUT) bin/$(ARCH)/ConfigDB.o
	$(CROSS_COMPILE)g++ -o bin/$(ARCH)/db-config db-config.cpp ConfigDB.cpp -s $(CFLAGS) $(INCLUDE) -Lbin/$(ARCH) -ldb-monitor $(LD_FLAGS)
	$(CROSS_COMPILE)g++ -o bin/$(ARCH)/libdb-monitor.so bin/$(ARCH)/db-monitor.o bin/$(ARCH)/ConfigDB.o $(LDFLAGS) -shared
clean:
	rm -rf bin
