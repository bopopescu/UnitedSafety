#-------------------------------------------------
#
# Project created by QtCreator 2011-05-17T14:18:42
#
#-------------------------------------------------

QT       += network

QT       -= gui

TARGET = BSP_WiFi
TEMPLATE = lib

DESTDIR = bin
MOC_DIR = moc
OBJECTS_DIR = obj

DEFINES += BSP_LIBRARY

SOURCES +=\
	BSP_WiFi.cpp\
	QBSP_WiFi.cpp

HEADERS +=\
	BSP_WiFi.h\
        bsp_global.h\
	QBSP_WiFi.h

