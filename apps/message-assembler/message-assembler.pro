#-------------------------------------------------
#
# Project created by QtCreator 2012-07-17T13:21:07
#
#-------------------------------------------------

ARCH=$$(ARCH)
isEmpty (ARCH) {
	ARCH=PC
}
message("Creating Makefile for $$ARCH")

FASTTRAC_DIR="FastTrac"
message("FastTrac source directory is \"$$FASTTRAC_DIR\"")

LIB_EXT=.so
REDSTONE_BUILD_DIR=$$(REDSTONE_BUILD_DIR)
isEmpty (REDSTONE_BUILD_DIR) {
	LIB_EXT=.a
}

QT		+=	core

QT		-=	gui

TARGET	= message-assembler
CONFIG	+= console
CONFIG	-= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
	datacollector.cpp \
	ipcserver.cpp \
	messageassembler.cpp \
	messagedatabase.cpp

HEADERS += \
	datacollector.h \
	ipcserver.h \
	messageassembler.h \
	messagedatabase.h \
	messagetypes.h

OTHER_FILES +=

LIBS +=		-L../db-monitor/bin/$$ARCH/ -ldb-monitor \
	-L../ats-common/bin/$$ARCH/ -lats-common \
	-L../socket_interface/bin/$$ARCH/ -lsocket_interface \
	-L../command_line_parser/bin/$$ARCH/ -lcommand_line_parser \
	-L../$$FASTTRAC_DIR/FASTLib/bin/$$ARCH/ -lFASTLib \
	-L../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/ -lUtilityLib \

INCLUDEPATH += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR \
	../db-monitor

DEPENDPATH += += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR \
	../db-monitor

PRE_TARGETDEPS += ../ats-common/bin/$$ARCH/libats-common$$LIB_EXT \
	../socket_interface/bin/$$ARCH/libsocket_interface$$LIB_EXT \
	../command_line_parser/bin/$$ARCH/libcommand_line_parser$$LIB_EXT \
	../$$FASTTRAC_DIR/FASTLib/bin/$$ARCH/libFASTLib.a \
	../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/libUtilityLib.a \
	../db-monitor/bin/$$ARCH/libdb-monitor$$LIB_EXT
