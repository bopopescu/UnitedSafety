ARCH=$$(ARCH)
isEmpty (ARCH) {
	ARCH=PC
}
message("Creating Makefile for $$ARCH")

LIB_EXT=.so
REDSTONE_BUILD_DIR=$$(REDSTONE_BUILD_DIR)
isEmpty (REDSTONE_BUILD_DIR) {
	LIB_EXT=.a
}

FASTTRAC_DIR="FastTrac"

QT       += core

QT       -= gui

TARGET = plc-serial-monitor
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
	serialportmanager.cpp \
    plcserialmonitorstatemachine.cpp

HEADERS += \
	serialportmanager.h \
    plcserialmonitorstatemachine.h

LIBS += -L../socket_interface/bin/$$ARCH/ -lsocket_interface \
	-L../command_line_parser/bin/$$ARCH/ -lcommand_line_parser \
	-L../$$FASTTRAC_DIR/FASTLib/bin/$$ARCH/ -lFASTLib \
	-L../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/ -lUtilityLib \
	-L../state-machine/bin/$$ARCH/ -lstate-machine \
	-L../db-monitor/bin/$$ARCH/ -ldb-monitor \
	-L../ats-common/bin/$$ARCH/ -lats-common

INCLUDEPATH += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR \
	../state-machine \
	../db-monitor \
	../message-assembler

DEPENDPATH += += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR \
	../state-machine \
	../db-monitor \
	../message-assembler

PRE_TARGETDEPS += ../ats-common/bin/$$ARCH/libats-common$$LIB_EXT \
	../socket_interface/bin/$$ARCH/libsocket_interface$$LIB_EXT \
	../command_line_parser/bin/$$ARCH/libcommand_line_parser$$LIB_EXT \
	../$$FASTTRAC_DIR/FASTLib/bin/$$ARCH/libFASTLib.a \
	../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/libUtilityLib.a \
	../state-machine/bin/$$ARCH/libstate-machine$$LIB_EXT \
	../db-monitor/bin/$$ARCH/libdb-monitor$$LIB_EXT
