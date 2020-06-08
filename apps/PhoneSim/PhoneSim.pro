ARCH=PC
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

TARGET = PhoneSim

TEMPLATE = app
CONFIG += console

QMAKE_CFLAGS_RELEASE -= -O2 --static
QMAKE_CXXFLAGS_RELEASE -= -O2 --static
QMAKE_CFLAGS_RELEASE += -Os --static
QMAKE_CXXFLAGS_RELEASE += -Os --static

SOURCES += main.cpp \
	calampsusermessage.cpp \
	calampsmessage.cpp \
	calampseventmessage.cpp \
	calampsackmessage.cpp \
		../ats-common/ats-common.cpp \
		../ats-common/ats-string.cpp \
		SafetyLinkMessage.cpp

HEADERS += \
	calampsusermessage.h \
	calampsmessage.h \
	calampseventmessage.h \
	calampsackmessage.h \
		../ats-common/ats-common.h \
		../ats-common/ats-string.h \
		SafetyLinkMessage.h

INCLUDEPATH += ../ats-common \
	../socket_interface \
	../$$FASTTRAC_DIR/FASTLib \
	../$$FASTTRAC_DIR/UtilityLib \
	../$$FASTTRAC_DIR



win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../FastTrac/boost/stage/lib/release/ -lboost_system
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../FastTrac/boost/stage/lib/debug/ -lboost_system
else:symbian: LIBS += -lboost_system
else:unix: LIBS += -L$$PWD/../FastTrac/boost/stage/lib/ -lboost_system

INCLUDEPATH += $$PWD/../FastTrac
DEPENDPATH += $$PWD/../FastTrac

win32:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../FastTrac/boost/stage/lib/release/boost_system.lib
else:win32:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../FastTrac/boost/stage/lib/debug/boost_system.lib
else:unix:!symbian: PRE_TARGETDEPS += $$PWD/../FastTrac/boost/stage/lib/libboost_system.a
