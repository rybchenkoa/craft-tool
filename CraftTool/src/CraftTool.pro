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
           LogConsole.cpp\
           StatusBar.cpp\
           ComPortConnect.cpp\
           GCodeInterpreter.cpp\
           CRemoteDevice.cpp\
           log.cpp\



HEADERS  += main.h\
            mainwindow.h\
            scene3d.h\
            LogConsole.h\
            StatusBar.h\
            GCodeInterpreter.h\
            IRemoteDevice.h\
            ComPortConnect.h\
            IPortToDevice.h\
            log.h \
            AutoLockCS.h

FORMS    += mainwindow.ui
