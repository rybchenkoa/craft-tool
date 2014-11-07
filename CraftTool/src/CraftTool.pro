#-------------------------------------------------
#
# Project created by QtCreator 2014-10-29T22:50:30
#
#-------------------------------------------------

QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

LIBS += -lws2_32

TARGET = CraftTool
TEMPLATE = app

CONFIG   += console
CONFIG   -= app_bundle

QMAKE_CXXFLAGS += -std=c++11

PRECOMPILED_HEADER = StdAfx.h

SOURCES += main.cpp\
           mainwindow.cpp\
           scene3d.cpp\
           ComPortConnect.cpp\
           GCodeInterpreter.cpp\
           CRemoteDevice.cpp\
           LogConsole.cpp\
           log.cpp



HEADERS  += main.h\
            mainwindow.h\
            scene3d.h\
            GCodeInterpreter.h\
            IRemoteDevice.h\
            ComPortConnect.h\
            IPortToDevice.h\
            LogConsole.h\
            log.h

FORMS    += mainwindow.ui
