ARCH=$$(ARCH)
isEmpty (ARCH) {
	ARCH=PC
}

LIB_EXT=.so
REDSTONE_BUILD_DIR=$$(REDSTONE_BUILD_DIR)
isEmpty(REDSTONE_BUILD_DIR) {
	LIB_EXT=.a
}

TARGET = packetizer-iridium

TEMPLATE = app
CONFIG += console
CONFIG -= qt

SOURCES += main.cpp \
	packetizer_state_machine.cpp \
	packetizerSender.cpp \
	packetizerDB.cpp \
	packetizerCopy.cpp \
	Iridium.cpp \
	IridiumMonitor.cpp \
  packetizerIridiumSender.cpp \
  packetizerIridiumMessage.cpp \
  packetizerIridiumDB.cpp \
    midDB.cpp

OTHER_FILES +=

HEADERS += \
	packetizer_state_machine.h \
	packetizerSender.h \
	packetizerDB.h \
	packetizerCopy.h \
	packetizer.h \
	Iridium.h \
	IridiumMonitor.h \
  packetizerIridiumSender.h \
  packetizerIridiumMessage.h \
  packetizerIridiumDB.h

LIBS +=	-L../socket_interface/bin/$$ARCH/ -lsocket_interface \
		-L../state-machine/bin/$$ARCH/ -lstate-machine \
		-L../db-monitor/bin/$$ARCH/ -ldb-monitor \
		-L../ats-common/bin/$$ARCH/ -lats-common \
		-L../FastTrac/FASTLib/bin/$$ARCH/ -lFASTLib \
		-L../FastTrac/UtilityLib/bin/$$ARCH/ -lUtilityLib \
		-lrt -s

INCLUDEPATH += ../ats-common \
		../db-monitor \
		../socket_interface \
		../state-machine \
		../FastTrac \
		../FastTrac/FASTLib \
		../FastTrac/UtilityLib \
		../message-assembler \
		../packetizer-lib \
		../packetizer-calamps

DEPENDPATH += ../ats-common \
		../db-monitor \
		../socket_interface \
		../state-machine \
		../message-assembler \
		../packetizer-lib \
		../packetizer-calamps

PRE_TARGETDEPS += ../ats-common/bin/$$ARCH/libats-common$$LIB_EXT \
		../db-monitor/bin/$$ARCH/libdb-monitor$$LIB_EXT \
		../socket_interface/bin/$$ARCH/libsocket_interface$$LIB_EXT \
		../state-machine/bin/$$ARCH/libstate-machine$$LIB_EXT
