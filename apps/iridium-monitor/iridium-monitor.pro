ARCH=$$(ARCH)
isEmpty (ARCH) {
	ARCH=PC
}
message("Creating Makefile for $$ARCH")

ZLIB_PATH=../zlib
LIB_EXT=.so
REDSTONE_BUILD_DIR=$$(REDSTONE_BUILD_DIR)
isEmpty (REDSTONE_BUILD_DIR) {
	LIB_EXT=.a
	ZLIB_PATH=../../3rd-party/zlib-1.2.5
}

FASTTRAC_DIR="FastTrac"

QT	+=	core \
		network
QT	-=	gui

TARGET = iridium-monitor

TEMPLATE = app
CONFIG += console

QMAKE_CFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CFLAGS_RELEASE += -Os
QMAKE_CXXFLAGS_RELEASE += -Os

SOURCES += main.cpp \
    Iridium.cpp \
	IridiumSender.cpp

HEADERS += \
    Iridium.h \
    IridiumSender.h

LIBS += -L../socket_interface/bin/$$ARCH/ -lsocket_interface \
	-L../command_line_parser/bin/$$ARCH/ -lcommand_line_parser \
	-L../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/ -lUtilityLib \
	-L../db-monitor/bin/$$ARCH/ -ldb-monitor \
	-L../ats-common/bin/$$ARCH/ -lats-common \
	-L../FastTrac/boost/stage/lib/ -lboost_system


INCLUDEPATH += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR \
	../db-monitor 

DEPENDPATH += += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR \
	../db-monitor

PRE_TARGETDEPS += ../ats-common/bin/$$ARCH/libats-common$$LIB_EXT \
	../socket_interface/bin/$$ARCH/libsocket_interface$$LIB_EXT \
	../command_line_parser/bin/$$ARCH/libcommand_line_parser$$LIB_EXT \
	../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/libUtilityLib.a \
	../db-monitor/bin/$$ARCH/libdb-monitor$$LIB_EXT
