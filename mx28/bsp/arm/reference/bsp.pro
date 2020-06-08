#-------------------------------------------------
#
# Project created by QtCreator 2011-05-17T14:18:42
#
#-------------------------------------------------

QT       += network

QT       -= gui

TARGET = bsp
TEMPLATE = lib

DESTDIR = bin
MOC_DIR = moc
OBJECTS_DIR = obj

DEFINES += BSP_LIBRARY

SOURCES +=\
	bsp.cpp\
	qbsp.cpp

HEADERS +=\
	bsp.h\
        bsp_global.h\
	qbsp.h

