#pragma once
#include <QApplication>
#include "mainwindow.h"
#include "GCodeInterpreter.h"
#include "IRemoteDevice.h"

extern QApplication *g_application;
extern MainWindow *g_mainWindow;
extern Interpreter::GCodeInterpreter *g_inter;
extern CRemoteDevice *g_device;
