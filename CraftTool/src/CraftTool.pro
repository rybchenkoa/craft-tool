#-------------------------------------------------
#
# Project created by QtCreator 2014-10-29T22:50:30
#
#-------------------------------------------------

QT       += core
QT       -= gui

LIBS += -lws2_32

TARGET = CraftTool
TEMPLATE = app

CONFIG   += console
CONFIG   -= app_bundle

QMAKE_CXXFLAGS += -std=c++11

SOURCES += ComPortConnect.cpp\
           GCodeInterpreter.cpp\
           CRemoteDevice.cpp \
           main.cpp



HEADERS  += GCodeInterpreter.h\
            IRemoteDevice.h\
            ComPortConnect.h\
            IPortToDevice.h
