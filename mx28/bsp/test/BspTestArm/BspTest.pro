#-------------------------------------------------
#
# Project created by QtCreator 2011-05-17T14:45:09
#
#-------------------------------------------------

TARGET = BspTestArm
TEMPLATE = app

DESTDIR = bin
MOC_DIR = moc
OBJECTS_DIR = obj
UI_DIR = uic

SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui

LIBS += -L/home/data/ppni/git/bsp/arm/bin -lbsp

QMAKE_CXXFLAGS += -I/home/data/ppni/git/bsp/arm -I/usr/include/qt4/QtNetwork

QMAKE_LFLAGS += -Xlinker -rpath -Xlinker /home/data/ppni/git/bsp/arm/bin
