#-------------------------------------------------
#
# Project created by QtCreator 2011-05-19T15:24:02
#
#-------------------------------------------------

QT       -= gui
QT       += network

TARGET = test
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DESTDIR = bin
MOC_DIR = moc
OBJECTS_DIR = obj

SOURCES += main.cpp

QMAKE_CXXFLAGS = -I../
QMAKE_LIBS =\
	-L../bin\
	-lBSP_WiFi\
	-Xlinker -rpath -Xlinker ../bin

