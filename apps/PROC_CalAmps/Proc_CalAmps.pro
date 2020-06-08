#-------------------------------------------------
#
# Project created by QtCreator 2012-09-24T18:28:28
#
#-------------------------------------------------

ARCH=$$(ARCH)
isEmpty (ARCH) {
	ARCH=PC
}
message("Creating Makefile for $$ARCH")

FASTTRAC_DIR="fastrac"
BUILD_DIR=$$(BUILD_DIR)
isEmpty (BUILD_DIR) {
	FASTTRAC_DIR="FastTrac"
}
message("FastTrac source directory is \"$$FASTTRAC_DIR\"")

LIB_EXT=.so
REDSTONE_BUILD_DIR=$$(REDSTONE_BUILD_DIR)
isEmpty (REDSTONE_BUILD_DIR) {
	LIB_EXT=.a
}

QT			+=	core \
			network \
			xml

QT			-=	gui

TARGET		=	Proc_CalAmps
CONFIG		+=	console
CONFIG		-=	app_bundle

TEMPLATE	= app


SOURCES		+=	main.cpp \
	calampsusermessage.cpp \
	calampsmessagehandler.cpp \
	calampsmessage.cpp \
	calampseventmessage.cpp \
	datacollector.cpp \
    ipcserver.cpp \
    proccalamps.cpp

OTHER_FILES += \
	calamp_settings.xml

HEADERS += \
	calampsusermessage.h \
	calampsmessagehandler.h \
	calampsmessage.h \
	calampseventmessage.h \
	datacollector.h \
    ipcserver.h \
    proccalamps.h

LIBS += -L../ats-common/bin/$$ARCH/ -lats-common \
	-L../socket_interface/bin/$$ARCH/ -lsocket_interface \
	-L../command_line_parser/bin/$$ARCH/ -lcommand_line_parser \
	-L../$$FASTTRAC_DIR/FASTLib/bin/$$ARCH/ -lFASTLib \
	-L../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/ -lUtilityLib

INCLUDEPATH += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR

DEPENDPATH += += ../ats-common \
	../socket_interface \
	../command_line_parser \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR

PRE_TARGETDEPS += ../ats-common/bin/$$ARCH/libats-common$$LIB_EXT \
	../socket_interface/bin/$$ARCH/libsocket_interface$$LIB_EXT \
	../command_line_parser/bin/$$ARCH/libcommand_line_parser$$LIB_EXT \
	../$$FASTTRAC_DIR/FASTLib/bin/$$ARCH/libFASTLib.a \
	../$$FASTTRAC_DIR/UtilityLib/bin/$$ARCH/libUtilityLib.a
