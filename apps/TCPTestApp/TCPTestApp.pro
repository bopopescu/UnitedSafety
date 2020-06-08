#-------------------------------------------------
#
# Project created by QtCreator 2012-07-17T13:21:07
#
#-------------------------------------------------

QT       += core \
            network

QT       -= gui

TARGET = TCPTestApp
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    tcptestapp.cpp \
    trakmessage.cpp \
    trakmessagehandler.cpp \
    datacollector.cpp \
    trakconfig.cpp

HEADERS += \
    tcptestapp.h \
    trakmessage.h \
    trakmessagehandler.h \
    datacollector.h \
    trakconfig.h

OTHER_FILES += \
    ct_config.data
