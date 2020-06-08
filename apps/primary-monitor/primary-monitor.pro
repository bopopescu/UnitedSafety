ARCH=$$(ARCH)
isEmpty (ARCH) {
	ARCH=PC
}

LIB_EXT=.so
REDSTONE_BUILD_DIR=$$(REDSTONE_BUILD_DIR)
isEmpty(REDSTONE_BUILD_DIR) {
	LIB_EXT=.a
}
QT		+=	core

QT		-=	gui
TARGET = primary-monitor


TEMPLATE = app
CONFIG += console

SOURCES += main.cpp \
		../message-assembler/messagedatabase.cpp 


LIBS +=	-L../packetizer-lib/bin/$$ARCH/ -lpacketizer \
		-L../socket_interface/bin/$$ARCH/ -lsocket_interface \
		-L../state-machine/bin/$$ARCH/ -lstate-machine \
		-L../db-monitor/bin/$$ARCH/ -ldb-monitor \
		-L../ats-common/bin/$$ARCH/ -lats-common \
		-L../FastTrac/FASTLib/bin/$$ARCH/ -lFASTLib \
		-L../FastTrac/UtilityLib/bin/$$ARCH/ -lUtilityLib \
		-L../FastTrac/boost/stage/lib/ -lboost_system \
		-lrt -s

INCLUDEPATH += ../ats-common \
		../db-monitor \
		../socket_interface \
		../state-machine \
		../FastTrac \
		../FastTrac/FASTLib \
		../FastTrac/UtilityLib \
		../message-assembler \
		../packetizer-lib

DEPENDPATH += ../ats-common \
		../db-monitor \
		../socket_interface \
		../state-machine \
		../message-assembler \
		../packetizer-lib

PRE_TARGETDEPS += ../ats-common/bin/$$ARCH/libats-common$$LIB_EXT \
		../db-monitor/bin/$$ARCH/libdb-monitor$$LIB_EXT \
		../socket_interface/bin/$$ARCH/libsocket_interface$$LIB_EXT \
		../state-machine/bin/$$ARCH/libstate-machine$$LIB_EXT \
		../packetizer-lib/bin/$$ARCH/libpacketizer$$LIB_EXT


