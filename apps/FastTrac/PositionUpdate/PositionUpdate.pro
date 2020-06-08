# -------------------------------------------------
# Project created by QtCreator 2012-06-29T09:36:43
# -------------------------------------------------
ARCH=$$(ARCH)
isEmpty (ARCH) {
  ARCH=arm
}
message("Creating Makefile for $$ARCH")

QMAKE_CFLAGS_RELEASE -= -O2
QMAKE_CFLAGS += -Os

QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS += -Os

QMAKE_LFLAGS += -s

QT += xml
QT -= gui
TARGET = PositionUpdate
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
SOURCES += main.cpp \
    Proc_PosnUpdate.cpp \

HEADERS += PU_Parms.h \
    Proc_PosnUpdate.h \
    ../../ats-common/ats-common.h

LIBS += -L../FASTLib/bin/$$ARCH/ -lFASTLib \
  -L../UtilityLib/bin/$$ARCH/ -lUtilityLib \
  -L../../socket_interface/bin/$$ARCH -lsocket_interface \
  -L../../ats-common/bin/$$ARCH -lats-common \
  -L../../db-monitor/bin/$$ARCH -ldb-monitor

INCLUDEPATH += ../FASTLib \
	../UtilityLib \
  ../../socket_interface \
  ../../ats-common \
  ../../db-monitor \
  ..

DEPENDPATH += ../FASTLib \
	../UtilityLib \
	..

PRE_TARGETDEPS += ../FASTLib/bin/$$ARCH/libFASTLib.a \
	../UtilityLib/bin/$$ARCH/libUtilityLib.a
